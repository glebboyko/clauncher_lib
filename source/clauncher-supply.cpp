#include "clauncher-supply.hpp"

namespace LNCR {

void LoggerCap(const std::string& l_module, const std::string& l_action,
               const std::string& l_event, int priority) {}

void Logger::Log(const std::string& event, int priority) {
  auto id = GetID();
  if (!id.empty()) {
    id = "( " + id + " ) ";
  }
  logger_(GetModule(), id + GetAction(), event, priority);
}
std::string Logger::GetID() const { return ""; }

int64_t LServer::calls[23] = {0};
LServer::LServer(LNCR::LServer::LAction action, logging_foo logger)
    : action_(action) {
  logger_ = logger;
  calls[action_] += 1;
}
std::string LServer::GetModule() const { return "LAUNCHER SERVER"; }
std::string LServer::GetAction() const {
  switch (action_) {
    case Constructor:
      return "CONSTRUCTOR";
    case Destructor:
      return "DESTRUCTOR";
    case GetConfig:
      return "GET CONFIG";
    case SetConfig:
      return "SET CONFIG";
    case Accepter:
      return "ACCEPTER THREAD";
    case Receiver:
      return "RECEIVER THREAD";
    case ProcessCtrl:
      return "PROCESS CTRL THREAD";
    case ClientComm:
      return "COMMUNICATION WITH CLIENT";
    case RunProcess:
      return "PROCESS RUNNER";
    case StopProcess:
      return "PROCESS TERMINATOR";
    case ALoad:
      return "(CLIENT) PROCESS LOADER";
    case AStop:
      return "(CLIENT) PROCESS TERMINATOR";
    case ARerun:
      return "(CLIENT) PROCESS RERUNNER";
    case AIsRunning:
      return "(CLIENT) PROCESS RUNNING CHECKER";
    case AGetPid:
      return "(CLIENT) PROCESS PID GETTER";
    case AGetConfig:
      return "(CLIENT) PROCESS CONFIG GETTER";
    case ASetConfig:
      return "(CLIENT) PROCESS CONFIG SETTER";
    case SentRun:
      return "AGENT RUNNER";
    case PrCtrlToRun:
      return "PROCESSES TO RUN HANDLER";
    case PrCtrlToTerm:
      return "PROCESSES TO TERM HANDLER";
    case PrCtrlMain:
      return "RUNNING PROCESSES HANDLER";
    case IsPidAvail:
      return "PROCESS RUNNING CHECKER";
    case GetPid:
      return "PROCESS PID GETTER";
    case IsRunning:
      return "PROCESS RUNNING CHECKER";
    default:
      return "CANNOT RECOGNIZE ACTION";
  }
}
std::string LServer::GetID() const { return std::to_string(calls[action_]); }

LClient::LClient(LNCR::LClient::LAction action, LNCR::logging_foo logger)
    : action_(action) {
  logger_ = logger;
}
std::string LClient::GetModule() const { return "LAUNCHER CLIENT"; }
std::string LClient::GetAction() const {
  switch (action_) {
    case Constructor:
      return "CONSTRUCTOR";
    case Destructor:
      return "DESTRUCTOR";
    case LoadProcess:
      return "PROCESS LOADER";
    case StopProcess:
      return "PROCESS TERMINATOR";
    case ReRunProcess:
      return "PROCESS RERUNNER";
    case IsProcessRunning:
      return "PROCESS RUNNING CHECKER";
    case GetProcessPid:
      return "PROCESS PID GETTER";
    case CheckTcpClient:
      return "TCP CLIENT AVAILABILITY CHECKER";
    default:
      return "CANNOT RECOGNIZE ACTION";
  }
}

LRunner::LRunner(LNCR::LRunner::LAction action, LNCR::logging_foo logger)
    : action_(action) {
  logger_ = logger;
}
std::string LRunner::GetModule() const { return "LAUNCHER RUNNER"; }
std::string LRunner::GetAction() const {
  switch (action_) {
    case Main:
      return "MAIN";
    case SigHandler:
      return "SIGNAL HANDLER";
    default:
      return "CANNOT RECOGNIZE ACTION";
  }
}

}  // namespace LNCR