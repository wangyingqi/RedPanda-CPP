/*
 * Copyright (C) 2020-2026 Roy Qu (royqh1979@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "runconsolewidget.h"

#include "../systemconsts.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollBar>
#include <QSocketNotifier>
#include <QTextBlock>
#include <QToolButton>
#include <QVBoxLayout>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>          // forkpty() on macOS/BSD

/* ------------------------------------------------------------------ */
/* ConsoleView: turn keystrokes into terminal bytes                    */
/* ------------------------------------------------------------------ */

ConsoleView::ConsoleView(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setUndoRedoEnabled(false);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    // Not read-only, so a text caret is shown; edits are prevented by the
    // keyPressEvent override (typing is forwarded to the PTY, not inserted).
}

void ConsoleView::keyPressEvent(QKeyEvent* e)
{
    if (!mRunning) {
        // allow navigation/copy but no editing when idle
        if (e->matches(QKeySequence::Copy) || e->matches(QKeySequence::SelectAll))
            QPlainTextEdit::keyPressEvent(e);
        return;
    }
    Qt::KeyboardModifiers m = e->modifiers();
#ifdef Q_OS_MACOS
    // On macOS Qt maps Cmd->ControlModifier and physical Ctrl->MetaModifier.
    const bool cmd = m.testFlag(Qt::ControlModifier);
    const bool ctrl = m.testFlag(Qt::MetaModifier);
#else
    const bool cmd = false;
    const bool ctrl = m.testFlag(Qt::ControlModifier);
#endif
    if (cmd) {
        if (e->key() == Qt::Key_V) { emit pasteRequested(); return; }
        if (e->key() == Qt::Key_C) { copy(); return; }
        if (e->key() == Qt::Key_A) { selectAll(); return; }
        return; // swallow other Cmd shortcuts
    }

    QByteArray out;
    const int key = e->key();
    if (ctrl && key >= Qt::Key_A && key <= Qt::Key_Z) {
        out.append(char(key - Qt::Key_A + 1)); // Ctrl-A..Z -> 0x01..0x1a
    } else {
        switch (key) {
        case Qt::Key_Return:
        case Qt::Key_Enter:     out.append('\r'); break;
        case Qt::Key_Backspace: out.append(char(0x7f)); break;
        case Qt::Key_Tab:       out.append('\t'); break;
        case Qt::Key_Escape:    out.append(char(0x1b)); break;
        case Qt::Key_Up:        out.append("\x1b[A", 3); break;
        case Qt::Key_Down:      out.append("\x1b[B", 3); break;
        case Qt::Key_Right:     out.append("\x1b[C", 3); break;
        case Qt::Key_Left:      out.append("\x1b[D", 3); break;
        case Qt::Key_Home:      out.append("\x1b[H", 3); break;
        case Qt::Key_End:       out.append("\x1b[F", 3); break;
        case Qt::Key_Delete:    out.append("\x1b[3~", 4); break;
        default:
            if (!e->text().isEmpty())
                out = e->text().toUtf8();
            break;
        }
    }
    if (!out.isEmpty())
        emit keyInput(out);
}

/* ------------------------------------------------------------------ */
/* ANSI 16-colour palette                                              */
/* ------------------------------------------------------------------ */

static QColor ansiColor(int index)
{
    static const QColor palette[16] = {
        QColor(0,0,0),       QColor(205,49,49),   QColor(13,188,121),  QColor(229,229,16),
        QColor(36,114,200),  QColor(188,63,188),  QColor(17,168,205),  QColor(229,229,229),
        QColor(102,102,102), QColor(241,76,76),   QColor(35,209,139),  QColor(245,245,67),
        QColor(59,142,234),  QColor(214,112,214), QColor(41,184,219),  QColor(255,255,255),
    };
    if (index < 0 || index > 15)
        return QColor();
    return palette[index];
}

/* ------------------------------------------------------------------ */
/* RunConsoleWidget                                                     */
/* ------------------------------------------------------------------ */

RunConsoleWidget::RunConsoleWidget(QWidget* parent)
    : QWidget(parent),
      mMasterFd(-1),
      mChildPid(-1),
      mReadNotifier(nullptr),
      mDecoder(QStringDecoder::Utf8)
{
    mView = new ConsoleView(this);
    mDefaultFormat = mView->currentCharFormat();
    mFormat = mDefaultFormat;

    mStatusLabel = new QLabel(this);
    mStopButton = new QToolButton(this);
    mStopButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    mStopButton->setEnabled(false);
    connect(mStopButton, &QToolButton::clicked, this, &RunConsoleWidget::stopProgram);
    mClearButton = new QToolButton(this);
    mClearButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(mClearButton, &QToolButton::clicked, mView, &QPlainTextEdit::clear);

    auto* topBar = new QHBoxLayout;
    topBar->setContentsMargins(4, 2, 4, 2);
    topBar->addWidget(mStatusLabel);
    topBar->addStretch(1);
    topBar->addWidget(mClearButton);
    topBar->addWidget(mStopButton);

    mHintLabel = new QLabel(this);
    mHintLabel->setContentsMargins(4, 0, 4, 2);
    QFont hf = mHintLabel->font();
    hf.setPointSizeF(hf.pointSizeF() * 0.9);
    mHintLabel->setFont(hf);
    mHintLabel->setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addLayout(topBar);
    layout->addWidget(mView, 1);
    layout->addWidget(mHintLabel);

    connect(mView, &ConsoleView::keyInput, this, &RunConsoleWidget::writeToPty);
    connect(mView, &ConsoleView::pasteRequested, this, &RunConsoleWidget::pasteToPty);

    mCursor = QTextCursor(mView->document());
    retranslate();
}

RunConsoleWidget::~RunConsoleWidget()
{
    if (isRunning())
        stopProgram();
    closeMaster();
}

void RunConsoleWidget::retranslate()
{
    mStopButton->setText(tr("Stop"));
    mClearButton->setText(tr("Clear"));
    mHintLabel->setText(tr("Type directly in the console; input goes to the program (Ctrl+C interrupts)."));
    if (!isRunning())
        mStatusLabel->setText(tr("Ready"));
}

bool RunConsoleWidget::isRunning() const
{
    return mChildPid > 0;
}

void RunConsoleWidget::closeMaster()
{
    if (mReadNotifier) {
        mReadNotifier->setEnabled(false);
        mReadNotifier->deleteLater();
        mReadNotifier = nullptr;
    }
    if (mMasterFd >= 0) {
        ::close(mMasterFd);
        mMasterFd = -1;
    }
}

void RunConsoleWidget::runProgram(const QString& program,
                                  const QStringList& arguments,
                                  const QString& workDir,
                                  const QStringList& binDirs)
{
    if (isRunning())
        stopProgram();
    closeMaster();

    mView->clear();
    mDecoder = QStringDecoder(QStringDecoder::Utf8);
    mFormat = mDefaultFormat;
    mEscPending.clear();
    mCursor = QTextCursor(mView->document());
    mCursor.movePosition(QTextCursor::End);

    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_row = 24;
    ws.ws_col = 80;

    int master = -1;
    pid_t pid = forkpty(&master, nullptr, nullptr, &ws);
    if (pid < 0) {
        appendMeta(tr("Failed to start the program (cannot allocate a terminal)."));
        setRunningUi(false);
        return;
    }

    if (pid == 0) {
        // --- child ---
        if (!workDir.isEmpty())
            (void)chdir(workDir.toLocal8Bit().constData());
        if (!binDirs.isEmpty()) {
            QByteArray path = qgetenv("PATH");
            QByteArray prepend = binDirs.join(PATH_SEPARATOR).toLocal8Bit();
            QByteArray full = path.isEmpty() ? prepend : prepend + PATH_SEPARATOR + path;
            setenv("PATH", full.constData(), 1);
        }
        setenv("TERM", "xterm-256color", 1);

        QList<QByteArray> argvStore;
        argvStore.append(program.toLocal8Bit());
        for (const QString& a : arguments)
            argvStore.append(a.toLocal8Bit());
        std::vector<char*> argv;
        for (QByteArray& a : argvStore)
            argv.push_back(a.data());
        argv.push_back(nullptr);

        execv(program.toLocal8Bit().constData(), argv.data());
        const char* msg = "Failed to start the program.\n";
        [[maybe_unused]] auto r = write(STDERR_FILENO, msg, strlen(msg));
        _exit(127);
    }

    // --- parent ---
    mChildPid = pid;
    mMasterFd = master;
    int flags = fcntl(mMasterFd, F_GETFL, 0);
    fcntl(mMasterFd, F_SETFL, flags | O_NONBLOCK);

    mReadNotifier = new QSocketNotifier(mMasterFd, QSocketNotifier::Read, this);
    connect(mReadNotifier, &QSocketNotifier::activated, this, &RunConsoleWidget::onMasterReadable);

    mTimer.start();
    setRunningUi(true);
    mView->setFocus();
}

void RunConsoleWidget::onMasterReadable()
{
    if (mMasterFd < 0)
        return;
    char buf[4096];
    bool eof = false;
    while (true) {
        ssize_t n = ::read(mMasterFd, buf, sizeof(buf));
        if (n > 0) {
            feed(mDecoder.decode(QByteArrayView(buf, n)));
        } else if (n == 0) {
            eof = true;
            break;
        } else {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            eof = true;    // EIO on macOS: slave closed
            break;
        }
    }

    if (eof) {
        double secs = mTimer.elapsed() / 1000.0;
        int status = 0;
        QString summary;
        if (mChildPid > 0) {
            pid_t r = waitpid((pid_t)mChildPid, &status, 0);
            if (r > 0) {
                if (WIFEXITED(status))
                    summary = tr("Process exited with code %1 (elapsed %2 s).")
                                  .arg(WEXITSTATUS(status)).arg(secs, 0, 'f', 3);
                else if (WIFSIGNALED(status))
                    summary = tr("Process terminated by signal %1 (elapsed %2 s).")
                                  .arg(WTERMSIG(status)).arg(secs, 0, 'f', 3);
            }
        }
        if (summary.isEmpty())
            summary = tr("Process finished (elapsed %1 s).").arg(secs, 0, 'f', 3);
        finishRun(summary);
    }
}

void RunConsoleWidget::writeToPty(const QByteArray& bytes)
{
    if (mMasterFd < 0)
        return;
    qint64 off = 0;
    while (off < bytes.size()) {
        ssize_t w = ::write(mMasterFd, bytes.constData() + off, bytes.size() - off);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        off += w;
    }
}

void RunConsoleWidget::pasteToPty()
{
    const QString text = QApplication::clipboard()->text();
    if (!text.isEmpty())
        writeToPty(text.toUtf8());
}

/* --------------------------- terminal rendering ------------------------- */

void RunConsoleWidget::putChar(QChar c)
{
    if (!mCursor.atBlockEnd()) {
        // overwrite the character under the cursor (terminal semantics)
        mCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        mCursor.insertText(QString(c), mFormat);
    } else {
        mCursor.insertText(QString(c), mFormat);
    }
}

void RunConsoleWidget::feed(const QString& textIn)
{
    QString text = mEscPending + textIn;
    mEscPending.clear();

    QScrollBar* sb = mView->verticalScrollBar();
    bool atBottom = sb->value() >= sb->maximum() - 4;

    int i = 0;
    const int n = text.length();
    while (i < n) {
        QChar c = text.at(i);
        ushort u = c.unicode();
        if (u == 0x1b) { // ESC
            // Need at least ESC + one more char to know the kind
            if (i + 1 >= n) { mEscPending = text.mid(i); break; }
            QChar next = text.at(i + 1);
            if (next == '[') {
                // CSI: ESC [ params... final(0x40-0x7e)
                int j = i + 2;
                while (j < n) {
                    ushort fj = text.at(j).unicode();
                    if (fj >= 0x40 && fj <= 0x7e)
                        break;
                    j++;
                }
                if (j >= n) { mEscPending = text.mid(i); break; } // incomplete
                QString seq = text.mid(i + 2, j - (i + 2));
                handleCsi(seq, text.at(j));
                i = j + 1;
                continue;
            } else if (next == ']') {
                // OSC: ESC ] ... BEL or ST(ESC \). Skip it (e.g. window title).
                int j = i + 2;
                bool done = false;
                while (j < n) {
                    if (text.at(j).unicode() == 0x07) { j++; done = true; break; }
                    if (text.at(j).unicode() == 0x1b && j + 1 < n && text.at(j+1) == '\\') {
                        j += 2; done = true; break;
                    }
                    j++;
                }
                if (!done) { mEscPending = text.mid(i); break; }
                i = j;
                continue;
            } else {
                // other 2-char escape (e.g. ESC(B) - ignore
                i += 2;
                continue;
            }
        } else if (u == '\n') {
            if (mCursor.blockNumber() >= mView->document()->blockCount() - 1) {
                mCursor.movePosition(QTextCursor::EndOfBlock);
                mCursor.insertBlock();
            } else {
                mCursor.movePosition(QTextCursor::NextBlock);
                mCursor.movePosition(QTextCursor::StartOfBlock);
            }
            i++;
        } else if (u == '\r') {
            mCursor.movePosition(QTextCursor::StartOfBlock);
            i++;
        } else if (u == '\b') {
            if (!mCursor.atBlockStart())
                mCursor.movePosition(QTextCursor::PreviousCharacter);
            i++;
        } else if (u == '\t') {
            int col = mCursor.positionInBlock();
            int spaces = 8 - (col % 8);
            for (int s = 0; s < spaces; s++)
                putChar(QChar(' '));
            i++;
        } else if (u == 0x07) { // BEL
            i++;
        } else if (u < 0x20) {
            i++; // ignore other control chars
        } else {
            putChar(c);
            i++;
        }
    }

    syncCaret();
    if (atBottom)
        sb->setValue(sb->maximum());
}

void RunConsoleWidget::handleCsi(const QString& seq, QChar final)
{
    auto firstParam = [&](int def) {
        bool ok = false;
        int v = seq.section(';', 0, 0).toInt(&ok);
        return ok ? v : def;
    };
    switch (final.unicode()) {
    case 'm':
        applySgr(seq);
        break;
    case 'K': { // erase in line
        int mode = firstParam(0);
        QTextCursor c = mCursor;
        if (mode == 0) {
            c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        } else if (mode == 1) {
            int pos = mCursor.positionInBlock();
            c.movePosition(QTextCursor::StartOfBlock);
            c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, pos);
        } else {
            c.movePosition(QTextCursor::StartOfBlock);
            c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        }
        c.removeSelectedText();
        break;
    }
    case 'J': { // erase in display
        int mode = firstParam(0);
        if (mode == 2 || mode == 3) {
            mView->clear();
            mCursor = QTextCursor(mView->document());
        } else if (mode == 0) {
            QTextCursor c = mCursor;
            c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            c.removeSelectedText();
        }
        break;
    }
    case 'A': { int nn = firstParam(1); for (int k=0;k<nn;k++) mCursor.movePosition(QTextCursor::PreviousBlock); break; }
    case 'B': { int nn = firstParam(1); for (int k=0;k<nn;k++) mCursor.movePosition(QTextCursor::NextBlock); break; }
    case 'C': { int nn = firstParam(1); mCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, nn); break; }
    case 'D': { int nn = firstParam(1); mCursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor, nn); break; }
    default:
        break; // ignore cursor addressing (H/f) and the rest
    }
}

void RunConsoleWidget::applySgr(const QString& params)
{
    const QStringList parts = params.isEmpty() ? QStringList{"0"} : params.split(';');
    for (int idx = 0; idx < parts.size(); idx++) {
        bool ok = false;
        int p = parts.at(idx).toInt(&ok);
        if (!ok) continue;
        if (p == 0) {
            mFormat = mDefaultFormat;
        } else if (p == 1) {
            mFormat.setFontWeight(QFont::Bold);
        } else if (p == 22) {
            mFormat.setFontWeight(QFont::Normal);
        } else if (p == 4) {
            mFormat.setFontUnderline(true);
        } else if (p == 24) {
            mFormat.setFontUnderline(false);
        } else if (p >= 30 && p <= 37) {
            mFormat.setForeground(ansiColor(p - 30));
        } else if (p >= 90 && p <= 97) {
            mFormat.setForeground(ansiColor(p - 90 + 8));
        } else if (p == 39) {
            mFormat.setForeground(mDefaultFormat.foreground());
        } else if (p >= 40 && p <= 47) {
            mFormat.setBackground(ansiColor(p - 40));
        } else if (p >= 100 && p <= 107) {
            mFormat.setBackground(ansiColor(p - 100 + 8));
        } else if (p == 49) {
            mFormat.setBackground(mDefaultFormat.background());
        } else if ((p == 38 || p == 48) && idx + 2 < parts.size() && parts.at(idx+1).toInt() == 5) {
            int n256 = parts.at(idx + 2).toInt();
            QColor col = (n256 < 16) ? ansiColor(n256) : QColor();
            if (col.isValid()) {
                if (p == 38) mFormat.setForeground(col); else mFormat.setBackground(col);
            }
            idx += 2;
        }
    }
}

void RunConsoleWidget::syncCaret()
{
    mView->setTextCursor(mCursor);
    mView->ensureCursorVisible();
}

void RunConsoleWidget::appendMeta(const QString& text)
{
    mCursor.movePosition(QTextCursor::End);
    QTextCharFormat meta = mDefaultFormat;
    meta.setForeground(ansiColor(8)); // dim grey
    mCursor.insertText("\n──────────\n" + text + "\n", meta);
    syncCaret();
}

/* --------------------------- lifecycle -------------------------------- */

void RunConsoleWidget::finishRun(const QString& summary)
{
    closeMaster();
    if (mChildPid > 0) {
        int st;
        waitpid((pid_t)mChildPid, &st, WNOHANG);
        mChildPid = -1;
    }
    appendMeta(summary);
    setRunningUi(false);
}

void RunConsoleWidget::stopProgram()
{
    if (!isRunning())
        return;
    pid_t pid = (pid_t)mChildPid;
    ::kill(pid, SIGTERM);
    for (int i = 0; i < 10; i++) {
        int st;
        pid_t r = waitpid(pid, &st, WNOHANG);
        if (r == pid || r < 0)
            break;
        usleep(20000);
        if (i == 4)
            ::kill(pid, SIGKILL);
    }
    double secs = mTimer.elapsed() / 1000.0;
    mChildPid = -1;
    finishRun(tr("Stopped (elapsed %1 s).").arg(secs, 0, 'f', 3));
}

void RunConsoleWidget::setRunningUi(bool running)
{
    mStopButton->setEnabled(running);
    mView->setRunning(running);
    mStatusLabel->setText(running ? tr("Running…") : tr("Stopped"));
    if (running)
        mView->setFocus();
    emit runStateChanged(running);
}
