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
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

RunConsoleWidget::RunConsoleWidget(QWidget* parent)
    : QWidget(parent),
      mProcess(nullptr)
{
    mOutput = new QPlainTextEdit(this);
    mOutput->setReadOnly(true);
    mOutput->setUndoRedoEnabled(false);
    mOutput->setMaximumBlockCount(20000); // cap runaway output
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

    mInput = new QLineEdit(this);
    mInput->setEnabled(false);
    connect(mInput, &QLineEdit::returnPressed, this, &RunConsoleWidget::sendInput);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addLayout(topBar);
    layout->addWidget(mOutput, 1);
    layout->addWidget(mInput);

    retranslate();
}

RunConsoleWidget::~RunConsoleWidget()
{
    if (mProcess) {
        mProcess->disconnect(this);
        if (mProcess->state() != QProcess::NotRunning) {
            mProcess->kill();
            mProcess->waitForFinished(1000);
        }
        delete mProcess;
        mProcess = nullptr;
    }
}

void RunConsoleWidget::retranslate()
{
    mStopButton->setText(tr("Stop"));
    mClearButton->setText(tr("Clear"));
    mInput->setPlaceholderText(tr("Type here and press Enter to send input to the program"));
    if (!isRunning())
        mStatusLabel->setText(tr("Ready"));
}

bool RunConsoleWidget::isRunning() const
{
    return mProcess && mProcess->state() != QProcess::NotRunning;
}

void RunConsoleWidget::runProgram(const QString& program,
                                  const QStringList& arguments,
                                  const QString& workDir,
                                  const QStringList& binDirs)
{
    if (isRunning())
        stopProgram();
    if (mProcess) {
        mProcess->disconnect(this);
        mProcess->deleteLater();
        mProcess = nullptr;
    }

    mOutput->clear();

    mProcess = new QProcess(this);
    mProcess->setProgram(program);
    mProcess->setArguments(arguments);
    mProcess->setWorkingDirectory(workDir);
    mProcess->setProcessChannelMode(QProcess::MergedChannels); // interleave stdout+stderr in order

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!binDirs.isEmpty()) {
        QString path = env.value("PATH");
        QString prepended = binDirs.join(PATH_SEPARATOR);
        path = path.isEmpty() ? prepended : prepended + PATH_SEPARATOR + path;
        env.insert("PATH", path);
    }
    mProcess->setProcessEnvironment(env);

    connect(mProcess, &QProcess::readyReadStandardOutput, this, &RunConsoleWidget::onReadyRead);
    connect(mProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RunConsoleWidget::onFinished);
    connect(mProcess, &QProcess::errorOccurred, this, &RunConsoleWidget::onErrorOccurred);

    mTimer.start();
    mProcess->start();
    setRunningUi(true);
    mInput->setFocus();
}

void RunConsoleWidget::stopProgram()
{
    if (!isRunning())
        return;
    mProcess->terminate();
    if (!mProcess->waitForFinished(1000)) {
        mProcess->kill();
        mProcess->waitForFinished(1000);
    }
}

void RunConsoleWidget::sendInput()
{
    if (!isRunning())
        return;
    QString line = mInput->text();
    appendText(line + "\n"); // local echo
    mProcess->write((line + "\n").toUtf8());
    mInput->clear();
}

void RunConsoleWidget::onReadyRead()
{
    if (!mProcess)
        return;
    QByteArray data = mProcess->readAllStandardOutput();
    if (!data.isEmpty())
        appendText(QString::fromUtf8(data));
}

void RunConsoleWidget::onFinished(int exitCode, QProcess::ExitStatus status)
{
    onReadyRead(); // drain remaining buffered output
    double secs = mTimer.elapsed() / 1000.0;
    if (status == QProcess::CrashExit)
        appendMeta(tr("Process crashed (elapsed %1 s).").arg(secs, 0, 'f', 3));
    else
        appendMeta(tr("Process exited with code %1 (elapsed %2 s).")
                       .arg(exitCode).arg(secs, 0, 'f', 3));
    setRunningUi(false);
}

void RunConsoleWidget::onErrorOccurred(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        appendMeta(tr("Failed to start the program."));
        setRunningUi(false);
    }
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
    appendText("\n────────────────────\n" + text + "\n");
}

void RunConsoleWidget::setRunningUi(bool running)
{
    mStopButton->setEnabled(running);
    mInput->setEnabled(running);
    mStatusLabel->setText(running ? tr("Running…") : tr("Stopped"));
    emit runStateChanged(running);
}
