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
#ifndef RUNCONSOLEWIDGET_H
#define RUNCONSOLEWIDGET_H

#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QStringDecoder>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QWidget>

class QLabel;
class QToolButton;
class QSocketNotifier;

/**
 * A QPlainTextEdit that behaves like a terminal display: it never edits its own
 * text in response to keystrokes; instead each keypress is turned into terminal
 * bytes and emitted via keyInput() to be written to the PTY. What appears on
 * screen is only what the program (echoed by the tty) writes back.
 */
class ConsoleView : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit ConsoleView(QWidget* parent = nullptr);
    void setRunning(bool running) { mRunning = running; }

signals:
    void keyInput(const QByteArray& bytes);
    void pasteRequested();

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    bool mRunning = false;
};

/**
 * An in-IDE terminal for running console programs without spawning an external
 * terminal window. The child is attached to a pseudo-terminal (PTY) and the user
 * types directly in the view, so interactive input (cin/scanf), getch, line
 * editing, backspace and Ctrl+C behave as in a real terminal. A pragmatic subset
 * of ANSI escapes is rendered (SGR colours, CR overwrite, BS, TAB, erase
 * line/screen, simple cursor moves); it is not a full-screen VT100/ncurses host.
 */
class RunConsoleWidget : public QWidget {
    Q_OBJECT
public:
    explicit RunConsoleWidget(QWidget* parent = nullptr);
    ~RunConsoleWidget() override;

    /** Launch program with args; workDir is the process CWD, binDirs prepend PATH. */
    void runProgram(const QString& program,
                    const QStringList& arguments,
                    const QString& workDir,
                    const QStringList& binDirs);
    void stopProgram();
    bool isRunning() const;

    void retranslate();

signals:
    void runStateChanged(bool running);

private slots:
    void onMasterReadable();
    void writeToPty(const QByteArray& bytes);
    void pasteToPty();

private:
    // terminal rendering
    void feed(const QString& text);
    void putChar(QChar c);
    void handleCsi(const QString& seq, QChar final);
    void applySgr(const QString& params);
    void appendMeta(const QString& text);
    void syncCaret();

    void setRunningUi(bool running);
    void finishRun(const QString& summary);
    void closeMaster();

private:
    ConsoleView* mView;
    QLabel* mStatusLabel;
    QLabel* mHintLabel;
    QToolButton* mStopButton;
    QToolButton* mClearButton;

    int mMasterFd;
    long long mChildPid;              // pid_t, kept wide to avoid header leak
    QSocketNotifier* mReadNotifier;
    QStringDecoder mDecoder;
    QElapsedTimer mTimer;

    // terminal state
    QTextCursor mCursor;
    QTextCharFormat mFormat;
    QTextCharFormat mDefaultFormat;
    QString mEscPending;              // incomplete escape sequence across reads
};

#endif // RUNCONSOLEWIDGET_H
