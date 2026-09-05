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
#include "dapdebugger.h"
#include "../mainwindow.h"
#include "../editormanager.h"
#include "../utils.h"
#include "../utils/parsearg.h"
#include "../systemconsts.h"
#include "../settings.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutexLocker>

static const int STACK_TRACE_LEVELS = 64;
static const int DISASSEMBLY_BEFORE = 24;
static const int DISASSEMBLY_COUNT = 64;

DAPDebuggerClient::DAPDebuggerClient(Debugger *debugger, QObject *parent):
    DebuggerClient{debugger, parent},
    mSeq{0},
    mStop{false},
    mInitialized{false},
    mRunRequested{false},
    mHasBreakpoints{false},
    mConfigured{false},
    mTerminated{false},
    mThreadId{0},
    mCurrentFrameId{-1},
    mCurrentLine{0},
    mCurrentAddress{0},
    mTempBreakLine{-1},
    mWatchSeq{0},
    mPendingWatchRefreshes{0},
    mPendingRegisterGroups{0}
{
}

/* ------------------------------------------------------------------ */
/* protocol plumbing                                                   */
/* ------------------------------------------------------------------ */

void DAPDebuggerClient::sendRequest(const QString &command, const QJsonObject &arguments, ResponseHandler handler)
{
    QMutexLocker locker(&mMutex);
    qint64 seq = ++mSeq;
    QJsonObject message{
        {"seq", seq},
        {"type", "request"},
        {"command", command},
    };
    if (!arguments.isEmpty())
        message["arguments"] = arguments;
    if (handler)
        mPending.insert(seq, handler);
    mOutgoing.enqueue(DAPTransport::encode(message));
    if (pSettings->debugger().showDetailLog())
        mFullOutput.append("> " + QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void DAPDebuggerClient::sendResponse(qint64 requestSeq, const QString &command, bool success, const QJsonObject &body, const QString &message)
{
    QMutexLocker locker(&mMutex);
    QJsonObject response{
        {"seq", ++mSeq},
        {"type", "response"},
        {"request_seq", requestSeq},
        {"success", success},
        {"command", command},
    };
    if (!body.isEmpty())
        response["body"] = body;
    if (!message.isEmpty())
        response["message"] = message;
    mOutgoing.enqueue(DAPTransport::encode(response));
}

void DAPDebuggerClient::appendConsoleLines(const QString &text)
{
    if (text.isEmpty())
        return;
    QStringList lines = text.split('\n');
    if (lines.last().isEmpty())
        lines.removeLast();
    mConsoleOutput.append(lines);
}

void DAPDebuggerClient::flushBatch()
{
    emit parseFinished();
    mConsoleOutput.clear();
    mFullOutput.clear();
    mSignalReceived = false;
    mUpdateCPUInfo = false;
}

void DAPDebuggerClient::dispatch(const QJsonObject &message)
{
    if (pSettings->debugger().showDetailLog())
        mFullOutput.append("< " + QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
    QString type = message["type"].toString();
    if (type == "response") {
        qint64 requestSeq = static_cast<qint64>(message["request_seq"].toDouble());
        ResponseHandler handler;
        {
            QMutexLocker locker(&mMutex);
            handler = mPending.take(requestSeq);
        }
        bool success = message["success"].toBool();
        QJsonObject body = message["body"].toObject();
        QString errorMessage = message["message"].toString();
        if (!success && errorMessage.isEmpty()) {
            errorMessage = body["error"].toObject()["format"].toString();
        }
        if (handler)
            handler(success, body, errorMessage);
    } else if (type == "event") {
        onEvent(message["event"].toString(), message["body"].toObject());
    } else if (type == "request") {
        onReverseRequest(static_cast<qint64>(message["seq"].toDouble()),
                         message["command"].toString(),
                         message["arguments"].toObject());
    }
}

void DAPDebuggerClient::run()
{
    mStop = false;
    mInferiorRunning = false;
    mProcessExited = false;
    mTerminated = false;
    bool errorOccured = false;

    QString cmd = debuggerPath();
    QString workingDir = QFileInfo(cmd).path();

    mProcess = std::make_shared<QProcess>();
    auto action = finally([&]{
        mProcess.reset();
    });
    mProcess->setProgram(cmd);
    mProcess->setArguments(QStringList{});
    mProcess->setProcessChannelMode(QProcess::SeparateChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value("PATH");
    QStringList pathAdded = binDirs();
    if (!path.isEmpty()) {
        path = pathAdded.join(PATH_SEPARATOR) + PATH_SEPARATOR + path;
    } else {
        path = pathAdded.join(PATH_SEPARATOR);
    }
    QString cmdDir = extractFileDir(cmd);
    if (!cmdDir.isEmpty()) {
        path = cmdDir + PATH_SEPARATOR + path;
    }
    env.insert("PATH", path);
    mProcess->setProcessEnvironment(env);
    mProcess->setWorkingDirectory(workingDir);

    connect(mProcess.get(), &QProcess::errorOccurred,
            [&](){
                errorOccured = true;
            });

    mProcess->start();
    mProcess->waitForStarted(5000);
    mStartSemaphore.release(1);

    bool disconnectSent = false;
    qint64 disconnectSentAt = 0;
    while (true) {
        if (mProcess->state() != QProcess::Running)
            break;
        if (errorOccured)
            break;
        if (mStop && !disconnectSent) {
            disconnectSent = true;
            disconnectSentAt = QDateTime::currentMSecsSinceEpoch();
            sendRequest("disconnect", QJsonObject{{"terminateDebuggee", true}});
        }
        if (disconnectSent && QDateTime::currentMSecsSinceEpoch() - disconnectSentAt > 1500) {
            mProcess->terminate();
            mProcess->waitForFinished(300);
            mProcess->kill();
            break;
        }
        // write pending requests
        {
            QQueue<QByteArray> outgoing;
            {
                QMutexLocker locker(&mMutex);
                outgoing.swap(mOutgoing);
            }
            while (!outgoing.isEmpty()) {
                mProcess->write(outgoing.dequeue());
            }
            if (mProcess->bytesToWrite() > 0)
                mProcess->waitForBytesWritten(100);
        }
        // read
        bool gotData = mProcess->waitForReadyRead(5);
        QByteArray stderrData = mProcess->readAllStandardError();
        if (!stderrData.isEmpty())
            appendConsoleLines(QString::fromUtf8(stderrData));
        if (gotData || mProcess->bytesAvailable() > 0) {
            mTransport.feed(mProcess->readAllStandardOutput());
        }
        QJsonObject message;
        bool handledAny = false;
        while (mTransport.next(message)) {
            handledAny = true;
            dispatch(message);
        }
        if (handledAny || !mConsoleOutput.isEmpty() || mProcessExited) {
            flushBatch();
        }
        if (mProcessExited && !mStop) {
            // debuggee finished: ask Debugger to stop us (happens through parseFinished)
            msleep(10);
        }
    }
    if (errorOccured) {
        emit processFailed(mProcess->error());
    }
}

void DAPDebuggerClient::stopDebug()
{
    mStop = true;
}

DebuggerType DAPDebuggerClient::clientType()
{
    return DebuggerType::DAP;
}

bool DAPDebuggerClient::commandRunning() const
{
    QMutexLocker locker(&mMutex);
    return mInferiorRunning;
}

/* ------------------------------------------------------------------ */
/* launch sequence                                                     */
/* ------------------------------------------------------------------ */

void DAPDebuggerClient::initialize(const QString &inferior, bool /*hasSymbols*/)
{
    mInferior = inferior;
    mWorkingDir = extractFileDir(inferior);
    QJsonObject args{
        {"clientID", "redpanda-cpp"},
        {"clientName", "Red Panda C++"},
        {"adapterID", "lldb-dap"},
        {"locale", "en-US"},
        {"linesStartAt1", true},
        {"columnsStartAt1", true},
        {"pathFormat", "path"},
        {"supportsRunInTerminalRequest", true},
        {"supportsVariableType", true},
        {"supportsMemoryReferences", true},
    };
    sendRequest("initialize", args, [this](bool success, const QJsonObject&, const QString& message) {
        if (!success) {
            appendConsoleLines(tr("Debug adapter initialization failed: %1").arg(message));
            mProcessExited = true;
            return;
        }
        sendLaunch();
    });
}

void DAPDebuggerClient::sendLaunch()
{
    QJsonArray programArgs;
    if (pSettings->executor().useParams()) {
        for (const QString& arg : parseArgumentsWithoutVariables(pSettings->executor().params()))
            programArgs.append(arg);
    }
    QJsonObject args{
        {"program", mInferior},
        {"cwd", mWorkingDir},
        {"args", programArgs},
        {"stopOnEntry", false},
        {"console", "integratedTerminal"},
        {"disableASLR", false},
    };
    sendRequest("launch", args, [this](bool success, const QJsonObject&, const QString& message) {
        if (!success) {
            appendConsoleLines(tr("Failed to launch program: %1").arg(message));
            mProcessExited = true;
        }
    });
}

void DAPDebuggerClient::runInferior(bool hasBreakpoints)
{
    mHasBreakpoints = hasBreakpoints;
    mRunRequested = true;
    if (mInitialized)
        sendConfigurationDone();
}

void DAPDebuggerClient::sendConfigurationDone()
{
    if (mConfigured)
        return;
    mConfigured = true;
    if (!mHasBreakpoints) {
        // Dev-C++ behaviour: with no breakpoints, stop at main()
        sendRequest("setFunctionBreakpoints",
                    QJsonObject{{"breakpoints", QJsonArray{QJsonObject{{"name", "main"}}}}});
    }
    beginRunning();
    sendRequest("configurationDone", QJsonObject());
}

void DAPDebuggerClient::onReverseRequest(qint64 seq, const QString &command, const QJsonObject &arguments)
{
    if (command == "runInTerminal") {
        handleRunInTerminal(seq, arguments);
    } else {
        sendResponse(seq, command, false, QJsonObject(), tr("Unsupported request: %1").arg(command));
    }
}

void DAPDebuggerClient::handleRunInTerminal(qint64 seq, const QJsonObject &arguments)
{
    QStringList execArgs;
    for (const QJsonValue& v : arguments["args"].toArray())
        execArgs.append(v.toString());
    QString cwd = arguments["cwd"].toString();
    if (cwd.isEmpty())
        cwd = mWorkingDir;
    if (execArgs.isEmpty()) {
        sendResponse(seq, "runInTerminal", false, QJsonObject(), tr("Empty command"));
        return;
    }
    QString cmd;
    QStringList args;
    PNonExclusiveTemporaryFileOwner fileOwner;
    std::tie(cmd, args, fileOwner) = wrapCommandForTerminalEmulator(
        pSettings->environment().terminalPath(),
        pSettings->environment().terminalArgumentsPattern(),
        execArgs,
        &pSettings->dirs());
    if (fileOwner)
        mTempFiles.push_back(std::move(fileOwner));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value("PATH");
    QStringList pathAdded = binDirs();
    if (!path.isEmpty())
        path = pathAdded.join(PATH_SEPARATOR) + PATH_SEPARATOR + path;
    else
        path = pathAdded.join(PATH_SEPARATOR);
    QProcess process;
    process.setProgram(cmd);
    process.setArguments(args);
    process.setWorkingDirectory(cwd);
    env.insert("PATH", path);
    QJsonObject envObj = arguments["env"].toObject();
    for (auto it = envObj.begin(); it != envObj.end(); ++it)
        env.insert(it.key(), it.value().toString());
    process.setProcessEnvironment(env);
    qint64 pid = 0;
    if (!process.startDetached(&pid)) {
        sendResponse(seq, "runInTerminal", false, QJsonObject(),
                     tr("Can't start terminal \"%1\"").arg(cmd));
        return;
    }
    sendResponse(seq, "runInTerminal", true, QJsonObject{{"shellProcessId", pid}});
}

/* ------------------------------------------------------------------ */
/* events                                                              */
/* ------------------------------------------------------------------ */

void DAPDebuggerClient::onEvent(const QString &event, const QJsonObject &body)
{
    if (event == "initialized") {
        mInitialized = true;
        sendAllBreakpoints();
        sendDataBreakpoints();
        if (mRunRequested)
            sendConfigurationDone();
    } else if (event == "output") {
        QString category = body["category"].toString();
        if (category == "telemetry")
            return;
        appendConsoleLines(body["output"].toString());
    } else if (event == "stopped") {
        {
            QMutexLocker locker(&mMutex);
            mInferiorRunning = false;
        }
        if (body.contains("threadId"))
            mThreadId = static_cast<qint64>(body["threadId"].toDouble());
        mUpdateCPUInfo = true;
        QString reason = body["reason"].toString();
        QString description = body["description"].toString();
        if (reason == "exception" || reason == "signal") {
            mSignalReceived = true;
            mSignalName = description.isEmpty() ? reason : description;
            mSignalMeaning = body["text"].toString();
            if (mSignalName.startsWith("signal ")) // lldb-dap: "signal SIGSEGV"
                mSignalName = mSignalName.mid(7);
        } else if (reason.contains("watchpoint") || reason.contains("data breakpoint")) {
            emit watchpointHitted(description, QString(), QString());
        }
        requestStackTrace(true);
    } else if (event == "continued") {
        beginRunning();
    } else if (event == "exited") {
        int exitCode = body["exitCode"].toInt();
        appendConsoleLines(tr("Process exited with code %1").arg(exitCode));
    } else if (event == "terminated") {
        mTerminated = true;
        {
            QMutexLocker locker(&mMutex);
            mInferiorRunning = false;
        }
        mProcessExited = true;
    }
    // "thread", "process", "breakpoint", "module", "capabilities", "progress*" are ignored
}

/* ------------------------------------------------------------------ */
/* execution control                                                   */
/* ------------------------------------------------------------------ */

void DAPDebuggerClient::beginRunning()
{
    bool wasRunning;
    {
        QMutexLocker locker(&mMutex);
        wasRunning = mInferiorRunning;
        mInferiorRunning = true;
    }
    mCurrentAddress = 0;
    mCurrentFile.clear();
    mCurrentLine = 0;
    mCurrentFunc.clear();
    if (!wasRunning) {
        emit cmdStarted();
        emit inferiorContinued();
    }
}

void DAPDebuggerClient::stepOver()
{
    beginRunning();
    sendRequest("next", QJsonObject{{"threadId", mThreadId}});
}

void DAPDebuggerClient::stepInto()
{
    beginRunning();
    sendRequest("stepIn", QJsonObject{{"threadId", mThreadId}});
}

void DAPDebuggerClient::stepOut()
{
    beginRunning();
    sendRequest("stepOut", QJsonObject{{"threadId", mThreadId}});
}

void DAPDebuggerClient::stepOverInstruction()
{
    beginRunning();
    sendRequest("next", QJsonObject{{"threadId", mThreadId}, {"granularity", "instruction"}});
}

void DAPDebuggerClient::stepIntoInstruction()
{
    beginRunning();
    sendRequest("stepIn", QJsonObject{{"threadId", mThreadId}, {"granularity", "instruction"}});
}

void DAPDebuggerClient::resume()
{
    beginRunning();
    sendRequest("continue", QJsonObject{{"threadId", mThreadId}});
}

void DAPDebuggerClient::interrupt()
{
    sendRequest("pause", QJsonObject{{"threadId", mThreadId}});
}

void DAPDebuggerClient::runTo(const QString &filename, int line)
{
    mTempBreakFile = normalizePath(filename);
    mTempBreakLine = line;
    sendBreakpointsForFile(mTempBreakFile);
    resume();
}

void DAPDebuggerClient::clearTemporaryBreakpoint()
{
    if (mTempBreakLine < 0)
        return;
    QString file = mTempBreakFile;
    mTempBreakFile.clear();
    mTempBreakLine = -1;
    sendBreakpointsForFile(file);
}

QJsonObject DAPDebuggerClient::currentFrameObject() const
{
    for (const QJsonObject& frame : mFrames) {
        if (frame["id"].toInt() == mCurrentFrameId)
            return frame;
    }
    return QJsonObject();
}

void DAPDebuggerClient::updateCurrentFrame(const QJsonObject &frame)
{
    mCurrentFrameId = frame["id"].toInt();
    mCurrentFunc = frame["name"].toString();
    mCurrentFile = normalizePath(frame["source"].toObject()["path"].toString());
    mCurrentLine = frame["line"].toInt();
    bool ok = false;
    mCurrentAddress = frame["instructionPointerReference"].toString().toULongLong(&ok, 16);
    if (!ok)
        mCurrentAddress = 0;
}

void DAPDebuggerClient::requestStackTrace(bool emitStopped)
{
    QJsonObject args{
        {"threadId", mThreadId},
        {"startFrame", 0},
        {"levels", STACK_TRACE_LEVELS},
    };
    sendRequest("stackTrace", args, [this, emitStopped](bool success, const QJsonObject& body, const QString&) {
        if (!success)
            return;
        mFrames.clear();
        QList<PTrace> traces;
        QJsonArray frames = body["stackFrames"].toArray();
        int level = 0;
        for (const QJsonValue& v : frames) {
            QJsonObject frame = v.toObject();
            mFrames.append(frame);
            PTrace trace = std::make_shared<Trace>();
            trace->funcname = frame["name"].toString();
            trace->filename = normalizePath(frame["source"].toObject()["path"].toString());
            trace->line = frame["line"].toInt() - 1;
            trace->level = level++;
            trace->address = frame["instructionPointerReference"].toString();
            traces.append(trace);
        }
        debugger()->backtraceModel()->setTraces(traces);
        if (!mFrames.isEmpty())
            updateCurrentFrame(mFrames.first());
        if (emitStopped) {
            if (mTempBreakLine >= 0 && mCurrentFile == mTempBreakFile && mCurrentLine - 1 == mTempBreakLine)
                clearTemporaryBreakpoint();
            emit cmdFinished();
            emit inferiorStopped(mCurrentFile, mCurrentLine - 1);
        }
    });
}

void DAPDebuggerClient::selectFrame(PTrace trace)
{
    if (!trace)
        return;
    if (trace->level < 0 || trace->level >= mFrames.count())
        return;
    updateCurrentFrame(mFrames[trace->level]);
    emit inferiorStopped(mCurrentFile, mCurrentLine - 1);
}

void DAPDebuggerClient::refreshFrame()
{
    requestStackTrace(false);
}

void DAPDebuggerClient::refreshStackVariables()
{
    if (mCurrentFrameId < 0)
        return;
    sendRequest("scopes", QJsonObject{{"frameId", mCurrentFrameId}}, [this](bool success, const QJsonObject& body, const QString&) {
        if (!success)
            return;
        int localsRef = 0;
        for (const QJsonValue& v : body["scopes"].toArray()) {
            QJsonObject scope = v.toObject();
            if (scope["name"].toString() == "Locals" || scope["presentationHint"].toString() == "locals") {
                localsRef = scope["variablesReference"].toInt();
                break;
            }
        }
        if (localsRef == 0) {
            emit localsUpdated(QStringList());
            return;
        }
        sendRequest("variables", QJsonObject{{"variablesReference", localsRef}}, [this](bool success, const QJsonObject& body, const QString&) {
            if (!success)
                return;
            QStringList locals;
            for (const QJsonValue& v : body["variables"].toArray()) {
                QJsonObject var = v.toObject();
                locals.append(QString("%1 = %2").arg(var["name"].toString(), var["value"].toString()));
            }
            emit localsUpdated(locals);
        });
    });
}

/* ------------------------------------------------------------------ */
/* breakpoints                                                         */
/* ------------------------------------------------------------------ */

QString DAPDebuggerClient::normalizePath(const QString &path) const
{
    if (path.isEmpty())
        return path;
    QString result = path;
    result.replace('\\', '/');
    return QDir::cleanPath(result);
}

void DAPDebuggerClient::addBreakpoint(PBreakpoint breakpoint)
{
    if (!breakpoint)
        return;
    QString file = normalizePath(breakpoint->filename);
    QList<PBreakpoint>& list = mBreakpoints[file];
    for (const PBreakpoint& bp : list) {
        if (bp == breakpoint)
            return;
    }
    list.append(breakpoint);
    if (mInitialized)
        sendBreakpointsForFile(file);
}

void DAPDebuggerClient::removeBreakpoint(PBreakpoint breakpoint)
{
    if (!breakpoint)
        return;
    QString file = normalizePath(breakpoint->filename);
    if (!mBreakpoints.contains(file))
        return;
    QList<PBreakpoint>& list = mBreakpoints[file];
    for (int i = list.count() - 1; i >= 0; i--) {
        if (list[i] == breakpoint || (list[i]->line == breakpoint->line))
            list.removeAt(i);
    }
    if (mInitialized)
        sendBreakpointsForFile(file);
}

void DAPDebuggerClient::setBreakpointCondition(PBreakpoint breakpoint)
{
    if (!breakpoint)
        return;
    if (mInitialized)
        sendBreakpointsForFile(normalizePath(breakpoint->filename));
}

void DAPDebuggerClient::sendBreakpointsForFile(const QString &filename)
{
    QJsonArray breakpoints;
    QList<int> lines; // 0-based, parallel to breakpoints
    for (const PBreakpoint& bp : mBreakpoints.value(filename)) {
        QJsonObject obj{{"line", bp->line + 1}};
        if (!bp->condition.isEmpty())
            obj["condition"] = bp->condition;
        breakpoints.append(obj);
        lines.append(bp->line);
    }
    if (mTempBreakLine >= 0 && mTempBreakFile == filename) {
        breakpoints.append(QJsonObject{{"line", mTempBreakLine + 1}});
        lines.append(mTempBreakLine);
    }
    QJsonObject args{
        {"source", QJsonObject{{"path", filename}}},
        {"breakpoints", breakpoints},
    };
    sendRequest("setBreakpoints", args, [this, filename, lines](bool success, const QJsonObject& body, const QString& message) {
        if (!success) {
            appendConsoleLines(tr("Failed to set breakpoints in %1: %2").arg(filename, message));
            return;
        }
        QJsonArray result = body["breakpoints"].toArray();
        for (int i = 0; i < result.count() && i < lines.count(); i++) {
            QJsonObject bp = result[i].toObject();
            int id = bp["id"].toInt(-1);
            emit breakpointInfoGetted(filename, lines[i], id);
        }
    });
}

void DAPDebuggerClient::sendAllBreakpoints()
{
    for (const QString& file : mBreakpoints.keys())
        sendBreakpointsForFile(file);
}

void DAPDebuggerClient::addWatchpoint(const QString &watchExp)
{
    if (watchExp.isEmpty())
        return;
    QJsonObject args{{"name", watchExp}};
    if (mCurrentFrameId >= 0)
        args["frameId"] = mCurrentFrameId;
    sendRequest("dataBreakpointInfo", args, [this, watchExp](bool success, const QJsonObject& body, const QString& message) {
        QString dataId = body["dataId"].toString();
        if (!success || dataId.isEmpty()) {
            QString reason = message.isEmpty() ? body["description"].toString() : message;
            appendConsoleLines(tr("Can't set watchpoint on \"%1\": %2").arg(watchExp, reason));
            return;
        }
        mDataBreakpoints.append(QJsonObject{{"dataId", dataId}, {"accessType", "write"}});
        sendDataBreakpoints();
    });
}

void DAPDebuggerClient::sendDataBreakpoints()
{
    if (mDataBreakpoints.isEmpty())
        return;
    sendRequest("setDataBreakpoints", QJsonObject{{"breakpoints", mDataBreakpoints}});
}

/* ------------------------------------------------------------------ */
/* watches                                                             */
/* ------------------------------------------------------------------ */

QString DAPDebuggerClient::newWatchName()
{
    return QString("dap_%1").arg(++mWatchSeq);
}

int DAPDebuggerClient::childCountFromVariable(const QJsonObject &variable) const
{
    int ref = variable["variablesReference"].toInt();
    if (ref <= 0)
        return 0;
    int indexed = variable["indexedVariables"].toInt();
    int named = variable["namedVariables"].toInt();
    int count = indexed + named;
    return count > 0 ? count : 1;
}

void DAPDebuggerClient::addWatch(const QString &expression)
{
    QString name = newWatchName();
    WatchInfo info;
    info.expression = expression;
    mWatches.insert(name, info);
    evaluateWatch(name, true);
}

void DAPDebuggerClient::evaluateWatch(const QString &name, bool isNew)
{
    if (!mWatches.contains(name))
        return;
    QString expression = mWatches[name].expression;
    QJsonObject args{{"expression", expression}, {"context", "watch"}};
    if (mCurrentFrameId >= 0)
        args["frameId"] = mCurrentFrameId;
    mPendingWatchRefreshes++;
    sendRequest("evaluate", args, [this, name, expression, isNew](bool success, const QJsonObject& body, const QString& message) {
        auto done = finally([this]{ finishWatchRefresh(); });
        if (!mWatches.contains(name))
            return;
        WatchInfo& info = mWatches[name];
        QString oldType = info.type;
        if (success) {
            info.variablesReference = body["variablesReference"].toInt();
            info.type = body["type"].toString();
            int numChild = childCountFromVariable(body);
            QString value = body["result"].toString();
            if (isNew) {
                emit varCreated(expression, name, numChild, value, info.type, false);
            } else {
                emit varValueUpdated(name, value, "true", oldType != info.type, info.type, numChild, false);
                if (info.childrenFetched && numChild > 0)
                    fetchChildren(name);
            }
        } else {
            info.variablesReference = 0;
            QString value = message.isEmpty() ? tr("Not Valid") : message.trimmed();
            if (isNew) {
                emit varCreated(expression, name, 0, value, QString(), false);
            } else {
                emit varValueUpdated(name, value, "true", false, QString(), 0, false);
            }
        }
    });
}

void DAPDebuggerClient::fetchChildren(const QString &parentName)
{
    if (!mWatches.contains(parentName))
        return;
    int ref = mWatches[parentName].variablesReference;
    if (ref <= 0)
        return;
    mWatches[parentName].childrenFetched = true;
    mPendingWatchRefreshes++;
    sendRequest("variables", QJsonObject{{"variablesReference", ref}}, [this, parentName](bool success, const QJsonObject& body, const QString&) {
        auto done = finally([this]{ finishWatchRefresh(); });
        if (!success || !mWatches.contains(parentName))
            return;
        // remember which children had been expanded, so they can be re-fetched
        QHash<QString, bool> expandedChildren; // keyed by child expression
        QString prefix = parentName + ".";
        for (auto it = mWatches.begin(); it != mWatches.end(); ) {
            if (it.key().startsWith(prefix)) {
                if (it.value().childrenFetched)
                    expandedChildren.insert(it.value().expression, true);
                it = mWatches.erase(it);
            } else {
                ++it;
            }
        }
        QJsonArray children = body["variables"].toArray();
        emit prepareVarChildren(parentName, children.count(), false);
        int index = 0;
        QStringList toExpand;
        for (const QJsonValue& v : children) {
            QJsonObject child = v.toObject();
            QString childName = QString("%1.%2").arg(parentName).arg(index++);
            WatchInfo info;
            info.expression = child["name"].toString();
            info.variablesReference = child["variablesReference"].toInt();
            info.type = child["type"].toString();
            mWatches.insert(childName, info);
            emit addVarChild(parentName, childName, info.expression,
                             childCountFromVariable(child), child["value"].toString(),
                             info.type, false);
            if (expandedChildren.contains(info.expression))
                toExpand.append(childName);
        }
        for (const QString& childName : toExpand)
            fetchChildren(childName);
    });
}

void DAPDebuggerClient::fetchWatchVarChildren(const QString &varName)
{
    fetchChildren(varName);
}

void DAPDebuggerClient::removeWatchSubtree(const QString &name)
{
    QString prefix = name + ".";
    for (auto it = mWatches.begin(); it != mWatches.end(); ) {
        if (it.key() == name || it.key().startsWith(prefix))
            it = mWatches.erase(it);
        else
            ++it;
    }
}

void DAPDebuggerClient::removeWatch(PWatchVar watchVar)
{
    if (!watchVar || watchVar->name.isEmpty())
        return;
    removeWatchSubtree(watchVar->name);
}

void DAPDebuggerClient::refreshWatch(PWatchVar var)
{
    if (!var || var->name.isEmpty())
        return;
    evaluateWatch(var->name, false);
}

void DAPDebuggerClient::refreshWatch()
{
    QStringList names;
    for (auto it = mWatches.constBegin(); it != mWatches.constEnd(); ++it) {
        if (!it.key().contains('.'))
            names.append(it.key());
    }
    for (const QString& name : names)
        evaluateWatch(name, false);
}

void DAPDebuggerClient::finishWatchRefresh()
{
    mPendingWatchRefreshes--;
    if (mPendingWatchRefreshes <= 0) {
        mPendingWatchRefreshes = 0;
        emit varsValueUpdated();
    }
}

void DAPDebuggerClient::writeWatchVar(const QString &varName, const QString &value)
{
    if (!mWatches.contains(varName))
        return;
    // Build the full expression for nested children: parent expression + member/index
    QString expression = mWatches[varName].expression;
    QString name = varName;
    while (name.contains('.')) {
        name = name.left(name.lastIndexOf('.'));
        if (!mWatches.contains(name))
            break;
        QString parentExpr = mWatches[name].expression;
        if (expression.startsWith('['))
            expression = parentExpr + expression;
        else
            expression = parentExpr + "." + expression;
    }
    QJsonObject args{{"expression", expression}, {"value", value}};
    if (mCurrentFrameId >= 0)
        args["frameId"] = mCurrentFrameId;
    sendRequest("setExpression", args, [this](bool success, const QJsonObject&, const QString& message) {
        if (!success)
            appendConsoleLines(tr("Failed to change value: %1").arg(message));
        refreshWatch();
    });
}

/* ------------------------------------------------------------------ */
/* evaluate / memory / registers / disassembly / console               */
/* ------------------------------------------------------------------ */

void DAPDebuggerClient::evalExpression(const QString &expression)
{
    QJsonObject args{{"expression", expression}, {"context", "hover"}};
    if (mCurrentFrameId >= 0)
        args["frameId"] = mCurrentFrameId;
    sendRequest("evaluate", args, [this](bool success, const QJsonObject& body, const QString& message) {
        if (success)
            emit evalUpdated(body["result"].toString());
        else
            emit evalUpdated(message.trimmed());
    });
}

void DAPDebuggerClient::readMemory(const QString &startAddress, int rows, int cols)
{
    auto doRead = [this, rows, cols](const QString& reference) {
        QJsonObject args{{"memoryReference", reference}, {"count", rows * cols}};
        sendRequest("readMemory", args, [this, cols](bool success, const QJsonObject& body, const QString& message) {
            if (!success) {
                appendConsoleLines(tr("Failed to read memory: %1").arg(message));
                emit memoryUpdated(QStringList());
                return;
            }
            bool ok = false;
            qulonglong address = body["address"].toString().toULongLong(&ok, 16);
            if (!ok)
                address = body["address"].toString().toULongLong(&ok, 10);
            QByteArray data = QByteArray::fromBase64(body["data"].toString().toUtf8());
            QStringList lines;
            for (int offset = 0; offset < data.length(); offset += cols) {
                QStringList bytes;
                for (int j = 0; j < cols && offset + j < data.length(); j++)
                    bytes.append(QString("%1").arg(static_cast<unsigned char>(data[offset + j]), 2, 16, QChar('0')));
                lines.append(QString("0x%1 %2").arg(address + offset, 0, 16).arg(bytes.join(' ')));
            }
            emit memoryUpdated(lines);
        });
    };
    QString trimmed = startAddress.trimmed();
    bool ok = false;
    qulonglong address = trimmed.toULongLong(&ok, 0);
    if (ok) {
        doRead(QString("0x%1").arg(address, 0, 16));
        return;
    }
    QJsonObject args{{"expression", trimmed}, {"context", "hover"}};
    if (mCurrentFrameId >= 0)
        args["frameId"] = mCurrentFrameId;
    sendRequest("evaluate", args, [this, doRead, trimmed](bool success, const QJsonObject& body, const QString& message) {
        if (!success) {
            appendConsoleLines(tr("Can't evaluate \"%1\": %2").arg(trimmed, message));
            return;
        }
        QString reference = body["memoryReference"].toString();
        if (reference.isEmpty()) {
            bool ok = false;
            QString result = body["result"].toString().trimmed();
            qulonglong value = result.section(' ', 0, 0).toULongLong(&ok, 0);
            if (!ok) {
                appendConsoleLines(tr("\"%1\" is not an address").arg(trimmed));
                return;
            }
            reference = QString("0x%1").arg(value, 0, 16);
        }
        doRead(reference);
    });
}

void DAPDebuggerClient::writeMemory(qulonglong address, unsigned char data)
{
    QString command = QString("memory write 0x%1 0x%2").arg(address, 0, 16).arg(data, 2, 16, QChar('0'));
    sendRequest("evaluate", QJsonObject{{"expression", command}, {"context", "repl"}},
                [this](bool success, const QJsonObject&, const QString& message) {
        if (!success)
            appendConsoleLines(tr("Failed to write memory: %1").arg(message));
    });
}

void DAPDebuggerClient::refreshRegisters()
{
    if (mCurrentFrameId < 0)
        return;
    sendRequest("scopes", QJsonObject{{"frameId", mCurrentFrameId}}, [this](bool success, const QJsonObject& body, const QString&) {
        if (!success)
            return;
        int registersRef = 0;
        for (const QJsonValue& v : body["scopes"].toArray()) {
            QJsonObject scope = v.toObject();
            if (scope["name"].toString() == "Registers" || scope["presentationHint"].toString() == "registers") {
                registersRef = scope["variablesReference"].toInt();
                break;
            }
        }
        if (registersRef == 0)
            return;
        sendRequest("variables", QJsonObject{{"variablesReference", registersRef}}, [this](bool success, const QJsonObject& body, const QString&) {
            if (!success)
                return;
            mRegisterNames.clear();
            mRegisterValues.clear();
            collectRegisterGroups(body["variables"].toArray());
        });
    });
}

void DAPDebuggerClient::collectRegisterGroups(const QJsonArray &groups)
{
    // lldb-dap returns register groups ("General Purpose Registers", ...) that each expand to registers.
    QList<int> groupRefs;
    for (const QJsonValue& v : groups) {
        QJsonObject var = v.toObject();
        int ref = var["variablesReference"].toInt();
        if (ref > 0) {
            groupRefs.append(ref);
        } else {
            mRegisterValues.insert(mRegisterNames.count(), var["value"].toString());
            mRegisterNames.append(var["name"].toString());
        }
    }
    if (groupRefs.isEmpty()) {
        emit registerNamesUpdated(mRegisterNames);
        emit registerValuesUpdated(mRegisterValues);
        return;
    }
    // fetch groups sequentially to keep a stable order
    std::shared_ptr<std::function<void(int)>> fetchGroup = std::make_shared<std::function<void(int)>>();
    *fetchGroup = [this, groupRefs, fetchGroup](int index) {
        if (index >= groupRefs.count()) {
            emit registerNamesUpdated(mRegisterNames);
            emit registerValuesUpdated(mRegisterValues);
            return;
        }
        sendRequest("variables", QJsonObject{{"variablesReference", groupRefs[index]}},
                    [this, fetchGroup, index](bool success, const QJsonObject& body, const QString&) {
            if (success) {
                for (const QJsonValue& v : body["variables"].toArray()) {
                    QJsonObject var = v.toObject();
                    mRegisterValues.insert(mRegisterNames.count(), var["value"].toString());
                    mRegisterNames.append(var["name"].toString());
                }
            }
            (*fetchGroup)(index + 1);
        });
    };
    (*fetchGroup)(0);
}

QString DAPDebuggerClient::sourceLine(const QString &filename, int line)
{
    if (filename.isEmpty() || line <= 0)
        return QString();
    QStringList contents;
    if (mFileCache.contains(filename)) {
        contents = mFileCache.value(filename);
    } else {
        if (!pMainWindow->editorManager()->getContentFromOpenedEditor(filename, contents))
            contents = readFileToLines(filename);
        mFileCache[filename] = contents;
    }
    if (line - 1 < contents.count())
        return contents[line - 1];
    return QString();
}

void DAPDebuggerClient::disassembleCurrentFrame(bool blendMode)
{
    if (mCurrentAddress == 0)
        return;
    QJsonObject args{
        {"memoryReference", QString("0x%1").arg(mCurrentAddress, 0, 16)},
        {"instructionOffset", -DISASSEMBLY_BEFORE},
        {"instructionCount", DISASSEMBLY_COUNT},
        {"resolveSymbols", true},
    };
    sendRequest("disassemble", args, [this, blendMode](bool success, const QJsonObject& body, const QString& message) {
        if (!success) {
            appendConsoleLines(tr("Failed to disassemble: %1").arg(message));
            return;
        }
        QStringList lines;
        QString lastFile;
        int lastLine = -1;
        for (const QJsonValue& v : body["instructions"].toArray()) {
            QJsonObject inst = v.toObject();
            QString address = inst["address"].toString();
            bool ok = false;
            qulonglong addrValue = address.toULongLong(&ok, 16);
            if (blendMode) {
                QString file = inst["location"].toObject()["path"].toString();
                int line = inst["line"].toInt(-1);
                if (line > 0 && (file != lastFile || line != lastLine)) {
                    QString text = sourceLine(file, line);
                    lines.append(QString("%1\t%2").arg(line).arg(text));
                    lastFile = file;
                    lastLine = line;
                }
            }
            QString text = (ok && addrValue == mCurrentAddress) ? "=> " : "   ";
            text += address + " " + inst["instruction"].toString();
            lines.append(text);
        }
        emit disassemblyUpdate(mCurrentFile, mCurrentFunc, lines);
    });
}

void DAPDebuggerClient::setDisassemblyLanguage(bool isIntel)
{
    QString command = QString("settings set target.x86-disassembly-flavor %1").arg(isIntel ? "intel" : "att");
    sendRequest("evaluate", QJsonObject{{"expression", command}, {"context", "repl"}});
}

void DAPDebuggerClient::runConsoleCommand(const QString &command)
{
    QJsonObject args{{"expression", command}, {"context", "repl"}};
    if (mCurrentFrameId >= 0)
        args["frameId"] = mCurrentFrameId;
    sendRequest("evaluate", args, [this](bool success, const QJsonObject& body, const QString& message) {
        if (success)
            appendConsoleLines(body["result"].toString());
        else
            appendConsoleLines(message);
    });
}

void DAPDebuggerClient::skipDirectoriesInSymbolSearch(const QStringList &)
{
}

void DAPDebuggerClient::addSymbolSearchDirectories(const QStringList &)
{
}

void DAPDebuggerClient::skipStandardLibraryFunctions()
{
}
