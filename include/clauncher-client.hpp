#pragma once

#include <optional>
#include <string>

#include "clauncher-supply.hpp"
#include "tcp-client.hpp"

namespace LNCR {

class LauncherClient {
 public:
  LauncherClient(int port, logging_foo = LoggerCap);
  ~LauncherClient() = default;

  bool LoadProcess(const std::string& bin_name,
                   const ProcessConfig& process_config, bool wait_for_run);
  bool StopProcess(const std::string& bin_name, bool wait_for_stop);
  bool ReRunProcess(const std::string& bin_name, bool wait_for_rerun);
  bool IsProcessRunning(const std::string& bin_name);
  std::optional<int> GetProcessPid(const std::string& bin_name);

 private:
  void CheckTcpClient();
  int port_;
  logging_foo logger_;
  std::optional<TCP::TcpClient> tcp_client_;
};

}  // namespace LNCR