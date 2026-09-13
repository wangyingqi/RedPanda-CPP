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
#include <QStringDecoder>
#include <QWidget>

class QPlainTextEdit;
class QLineEdit;
class QLabel;
class QToolButton;
class QSocketNotifier;

/**
 * An in-IDE console for running console programs without spawning an external
 * terminal window. The child is attached to a pseudo-terminal (PTY), so it sees
 * a real TTY: prompts flush immediately and interactive input (cin/scanf) works
 * as it does in a normal terminal. Output is shown as text (basic control
 * characters handled; it is not a full VT100 emulator, so ANSI colour/cursor
 * control sequences are not rendered).
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
    void sendInput();

private:
    void appendText(const QString& text);
    void appendMeta(const QString& text);       // status/summary lines
    void setRunningUi(bool running);
    void finishRun(const QString& summary);
    void closeMaster();

private:
    QPlainTextEdit* mOutput;
    QLineEdit* mInput;
    QLabel* mInputLabel;
    QLabel* mStatusLabel;
    QToolButton* mStopButton;
    QToolButton* mClearButton;

    int mMasterFd;
    long long mChildPid;              // pid_t, kept as long long to avoid header leak
    QSocketNotifier* mReadNotifier;
    QStringDecoder mDecoder;
    QElapsedTimer mTimer;
};

#endif // RUNCONSOLEWIDGET_H
