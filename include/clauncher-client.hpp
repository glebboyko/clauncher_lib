#pragma once

#include <optional>

#include "clauncher-supply.hpp"
#include "tcp-client.hpp"

namespace LNCR {

class LauncherClient {
 public:
  LauncherClient(int port, logging_foo = LoggerCap);
  ~LauncherClient();

  bool LoadProcess(const std::string& bin_name,
                   const ProcessConfig& process_config,
                   bool wait_for_run) noexcept;
  bool StopProcess(const std::string& bin_name, bool wait_for_stop) noexcept;
  bool ReRunProcess(const std::string& bin_name, bool wait_for_rerun) noexcept;
  bool IsProcessRunning(const std::string& bin_name) noexcept;
  std::optional<int> GetProcessPid(const std::string& bin_name) noexcept;

 private:
  int port_;
  logging_foo logger_;
  std::optional<TCP::TcpClient> tcp_client_;
};

}  // namespace LNCR