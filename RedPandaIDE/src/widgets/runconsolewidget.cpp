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

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSocketNotifier>
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

RunConsoleWidget::RunConsoleWidget(QWidget* parent)
    : QWidget(parent),
      mMasterFd(-1),
      mChildPid(-1),
      mReadNotifier(nullptr),
      mDecoder(QStringDecoder::Utf8)
{
    mOutput = new QPlainTextEdit(this);
    mOutput->setReadOnly(true);
    mOutput->setUndoRedoEnabled(false);
    mOutput->setMaximumBlockCount(20000);
    mOutput->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    mOutput->setLineWrapMode(QPlainTextEdit::NoWrap);

    mStatusLabel = new QLabel(this);
    mStopButton = new QToolButton(this);
    mStopButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    mStopButton->setEnabled(false);
    connect(mStopButton, &QToolButton::clicked, this, &RunConsoleWidget::stopProgram);
    mClearButton = new QToolButton(this);
    mClearButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(mClearButton, &QToolButton::clicked, mOutput, &QPlainTextEdit::clear);

    auto* topBar = new QHBoxLayout;
    topBar->setContentsMargins(4, 2, 4, 2);
    topBar->addWidget(mStatusLabel);
    topBar->addStretch(1);
    topBar->addWidget(mClearButton);
    topBar->addWidget(mStopButton);

    mInputLabel = new QLabel(this);
    mInput = new QLineEdit(this);
    mInput->setClearButtonEnabled(true);
    mInput->setEnabled(false);
    connect(mInput, &QLineEdit::returnPressed, this, &RunConsoleWidget::sendInput);

    auto* inputBar = new QHBoxLayout;
    inputBar->setContentsMargins(4, 0, 4, 2);
    inputBar->addWidget(mInputLabel);
    inputBar->addWidget(mInput, 1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addLayout(topBar);
    layout->addWidget(mOutput, 1);
    layout->addLayout(inputBar);

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
    mInputLabel->setText(tr("Input:"));
    mInput->setPlaceholderText(tr("When the program waits for input, type here and press Enter"));
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
    mOutput->clear();
    mDecoder = QStringDecoder(QStringDecoder::Utf8);

    // Give the child a reasonably sized terminal window.
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
        if (!workDir.isEmpty()) {
            if (chdir(workDir.toLocal8Bit().constData()) != 0) {
                // continue anyway; not fatal for stdin/stdout programs
            }
        }
        if (!binDirs.isEmpty()) {
            QByteArray path = qgetenv("PATH");
            QByteArray prepend = binDirs.join(PATH_SEPARATOR).toLocal8Bit();
            QByteArray full = path.isEmpty() ? prepend
                                             : prepend + PATH_SEPARATOR + path;
            setenv("PATH", full.constData(), 1);
        }
        // Let programs that check $TERM behave like a simple terminal.
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
        // exec failed
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
    appendMeta(tr("Program started. Type input below when it is requested."));
    mInput->setFocus();
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
            QString piece = mDecoder.decode(QByteArrayView(buf, n));
            piece.replace("\r\n", "\n");
            piece.replace('\r', QString());
            appendText(piece);
        } else if (n == 0) {
            eof = true;
            break;
        } else {
            if (errno == EINTR)
                continue;
            // EAGAIN/EWOULDBLOCK: no more data for now
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            eof = true; // EIO on macOS signals the slave side closed
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

void RunConsoleWidget::sendInput()
{
    if (!isRunning() || mMasterFd < 0)
        return;
    QByteArray line = (mInput->text() + "\n").toUtf8();
    // The PTY echoes what we write, so it will appear in the output view.
    ssize_t off = 0;
    while (off < line.size()) {
        ssize_t w = ::write(mMasterFd, line.constData() + off, line.size() - off);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        off += w;
    }
    mInput->clear();
}

void RunConsoleWidget::appendText(const QString& text)
{
    if (text.isEmpty())
        return;
    QScrollBar* sb = mOutput->verticalScrollBar();
    bool atBottom = sb->value() >= sb->maximum() - 4;
    mOutput->moveCursor(QTextCursor::End);
    mOutput->insertPlainText(text);
    if (atBottom)
        sb->setValue(sb->maximum());
}

void RunConsoleWidget::appendMeta(const QString& text)
{
    appendText("\n──────────\n" + text + "\n");
}

void RunConsoleWidget::finishRun(const QString& summary)
{
    closeMaster();
    if (mChildPid > 0) {
        // Reap in case waitpid wasn't reached above.
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
    mInput->setEnabled(running);
    mStatusLabel->setText(running ? tr("Running…") : tr("Stopped"));
    if (running)
        mInput->setFocus();
    emit runStateChanged(running);
}
