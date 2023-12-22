#pragma once

#include <functional>
#include <list>
#include <string>

namespace LNCR {

enum MessagePriority { Error = 0, Warning = 1, Info = 2, Debug = 3 };
using logging_foo = std::function<void(const std::string&, const std::string&,
                                       const std::string&, int)>;
void LoggerCap(const std::string& l_module, const std::string& l_action,
               const std::string& l_event, int priority);

class Logger {
 public:
  void Log(const std::string& event, int priority);

  virtual std::string GetModule() const = 0;
  virtual std::string GetAction() const = 0;

 protected:
  logging_foo logger_;
  virtual std::string GetID() const;
};

class LServer : public Logger {
 public:
  enum LAction {
    Constructor,
    Destructor,
    GetConfig,
    SetConfig,
    Accepter,
    Receiver,
    ProcessCtrl,
    ClientComm,
    RunProcess,
    StopProcess,
    ALoad,
    AStop,
    ARerun,
    AIsRunning,
    AGetPid,
    AGetConfig,
    ASetConfig,
    SentRun,
    PrCtrlToRun,
    PrCtrlToTerm,
    PrCtrlMain,
    IsPidAvail,
    GetPid
  };
  LServer(LAction action, logging_foo logger);

  std::string GetModule() const override;
  std::string GetAction() const override;

 private:
  LAction action_;
  std::string GetID() const override;
  static int64_t calls[23];
};
int64_t LServer::calls[23] = {0};

class LClient : public Logger {
 public:
  enum LAction {
    Constructor,
    Destructor,
    LoadProcess,
    StopProcess,
    ReRunProcess,
    IsProcessRunning,
    GetProcessPid,
    CheckTcpClient
  };

  LClient(LAction action, logging_foo logger);
  std::string GetModule() const override;
  std::string GetAction() const override;

 private:
  LAction action_;
};
class LRunner : public Logger {
 public:
  enum LAction { Main, SigHandler };

  LRunner(LAction action, logging_foo logger);
  std::string GetModule() const override;
  std::string GetAction() const override;

 private:
  LAction action_;
};

struct ProcessConfig {
  std::list<std::string> args;

  bool launch_on_boot;
  bool term_rerun;

  std::optional<std::chrono::milliseconds> time_to_stop;
};

enum SenderStatus { Agent, Client };
enum Command { Load, Stop, Rerun, IsRunning, GetPid, GetConfig, SetConfig };

}  // namespace LNCR