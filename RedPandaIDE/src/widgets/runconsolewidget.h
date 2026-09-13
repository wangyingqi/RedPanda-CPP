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
#include <QProcess>
#include <QWidget>

class QPlainTextEdit;
class QLineEdit;
class QLabel;
class QToolButton;

/**
 * An in-IDE console for running console programs without spawning an external
 * terminal window. stdout/stderr stream into a text view; a single-line input
 * feeds stdin. Interactive line-buffered programs (cin/cout, scanf/printf) work;
 * programs that need a real TTY (getch/conio, ANSI cursor/color control) do not.
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
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError error);
    void sendInput();

private:
    void appendText(const QString& text);
    void appendMeta(const QString& text);       // status/summary lines (dimmed)
    void setRunningUi(bool running);

private:
    QPlainTextEdit* mOutput;
    QLineEdit* mInput;
    QLabel* mStatusLabel;
    QToolButton* mStopButton;
    QToolButton* mClearButton;
    QProcess* mProcess;
    QElapsedTimer mTimer;
};

#endif // RUNCONSOLEWIDGET_H
