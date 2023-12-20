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
enum LModule {

};
enum LAction {

};
void Logger(LModule l_module, LAction l_action, const std::string& event,
            int priority, logging_foo logger, void* module_address);

struct ProcessConfig {
  std::list<std::string> args;

  bool launch_on_boot;
  bool term_rerun;

  std::optional<std::chrono::milliseconds> time_to_stop;
};

enum SenderStatus { Agent, Client };
enum Command { Load, Stop, Rerun, IsRunning, GetPid, GetConfig, SetConfig };

}  // namespace LNCR