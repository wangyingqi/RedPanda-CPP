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
#ifndef DAP_DEBUGGER_H
#define DAP_DEBUGGER_H

#include "debugger.h"
#include "daptransport.h"
#include "../utils/terminal.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QProcess>
#include <QQueue>
#include <functional>
#include <vector>
#include <memory>

/**
 * Debugger client speaking the Debug Adapter Protocol (DAP).
 *
 * Tested against lldb-dap (shipped with Xcode / LLVM). All DebuggerClient
 * interface methods are called from the UI thread and only enqueue requests;
 * the protocol loop in run() sends them, reads responses/events and emits the
 * same signals as GDBMIDebuggerClient, so the UI layer is unchanged.
 */
class DAPDebuggerClient : public DebuggerClient {
    Q_OBJECT
public:
    using ResponseHandler = std::function<void(bool success, const QJsonObject& body, const QString& message)>;

    explicit DAPDebuggerClient(Debugger* debugger, QObject *parent = nullptr);
    DAPDebuggerClient(const DAPDebuggerClient&) = delete;
    DAPDebuggerClient& operator=(const DAPDebuggerClient&) = delete;

    // DebuggerClient interface
    void stopDebug() override;
    DebuggerType clientType() override;
    bool commandRunning() const override;

    void initialize(const QString& inferior, bool hasSymbols) override;
    void runInferior(bool hasBreakpoints) override;

    void stepOver() override;
    void stepInto() override;
    void stepOut() override;
    void runTo(const QString& filename, int line) override;
    void resume() override;
    void stepOverInstruction() override;
    void stepIntoInstruction() override;
    void interrupt() override;

    void refreshStackVariables() override;

    void readMemory(const QString& startAddress, int rows, int cols) override;
    void writeMemory(qulonglong address, unsigned char data) override;

    void addBreakpoint(PBreakpoint breakpoint) override;
    void removeBreakpoint(PBreakpoint breakpoint) override;
    void addWatchpoint(const QString& watchExp) override;
    void setBreakpointCondition(PBreakpoint breakpoint) override;

    void addWatch(const QString& expression) override;
    void removeWatch(PWatchVar watchVar) override;
    void writeWatchVar(const QString& varName, const QString& value) override;
    void refreshWatch(PWatchVar var) override;
    void refreshWatch() override;
    void fetchWatchVarChildren(const QString& varName) override;

    void evalExpression(const QString& expression) override;

    void selectFrame(PTrace trace) override;
    void refreshFrame() override;
    void refreshRegisters() override;
    void disassembleCurrentFrame(bool blendMode) override;
    void setDisassemblyLanguage(bool isIntel) override;

    void skipDirectoriesInSymbolSearch(const QStringList& lst) override;
    void addSymbolSearchDirectories(const QStringList& lst) override;
    void skipStandardLibraryFunctions() override;

    /** Run a raw lldb command typed into the debug console (DAP "repl" evaluate). */
    void runConsoleCommand(const QString& command);

protected:
    void run() override;

private:
    struct WatchInfo {
        QString expression;
        int variablesReference = 0;
        QString type;
        bool childrenFetched = false;
    };

    // --- protocol plumbing (thread safe) ---
    void sendRequest(const QString& command, const QJsonObject& arguments, ResponseHandler handler = ResponseHandler());
    void sendResponse(qint64 requestSeq, const QString& command, bool success, const QJsonObject& body, const QString& message = QString());
    void dispatch(const QJsonObject& message);
    void onEvent(const QString& event, const QJsonObject& body);
    void onReverseRequest(qint64 seq, const QString& command, const QJsonObject& arguments);
    void appendConsoleLines(const QString& text);
    void flushBatch();

    // --- launch sequence ---
    void sendLaunch();
    void sendConfigurationDone();
    void handleRunInTerminal(qint64 seq, const QJsonObject& arguments);

    // --- execution / stop ---
    void beginRunning();
    void requestStackTrace(bool emitStopped);
    void updateCurrentFrame(const QJsonObject& frame);
    void clearTemporaryBreakpoint();

    // --- breakpoints ---
    QString normalizePath(const QString& path) const;
    void sendBreakpointsForFile(const QString& filename);
    void sendAllBreakpoints();
    void sendDataBreakpoints();

    // --- watches ---
    QString newWatchName();
    int childCountFromVariable(const QJsonObject& variable) const;
    void evaluateWatch(const QString& name, bool isNew);
    void fetchChildren(const QString& parentName);
    void removeWatchSubtree(const QString& name);
    void finishWatchRefresh();

    // --- registers / disassembly helpers ---
    void collectRegisterGroups(const QJsonArray& groups);
    QString sourceLine(const QString& filename, int line);

    // --- lookup helpers ---
    QJsonObject currentFrameObject() const;

private:
    mutable QMutex mMutex;
    QQueue<QByteArray> mOutgoing;
    QHash<qint64, ResponseHandler> mPending;
    qint64 mSeq;
    DAPTransport mTransport;
    std::shared_ptr<QProcess> mProcess;

    bool mStop;
    bool mInitialized;
    bool mRunRequested;
    bool mHasBreakpoints;
    bool mConfigured;
    bool mTerminated;

    QString mInferior;
    QString mWorkingDir;

    qint64 mThreadId;
    int mCurrentFrameId;
    QList<QJsonObject> mFrames;
    QString mCurrentFile;
    int mCurrentLine;
    qulonglong mCurrentAddress;
    QString mCurrentFunc;

    QMap<QString, QList<PBreakpoint>> mBreakpoints;
    QString mTempBreakFile;
    int mTempBreakLine;
    QJsonArray mDataBreakpoints;

    QHash<QString, WatchInfo> mWatches;
    int mWatchSeq;
    int mPendingWatchRefreshes;

    QStringList mRegisterNames;
    QHash<int, QString> mRegisterValues;
    int mPendingRegisterGroups;

    QMap<QString, QStringList> mFileCache;
    std::vector<PNonExclusiveTemporaryFileOwner> mTempFiles;
};

#endif // DAP_DEBUGGER_H
