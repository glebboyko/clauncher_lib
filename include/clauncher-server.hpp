#pragma once

#include <chrono>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

#include "clauncher-supply.hpp"
#include "tcp-server.hpp"

namespace LNCR {

class LauncherServer {
 public:
  // structs //
  struct ProcessInfo {
    ProcessConfig config;
    int pid = 0;
  };
  struct Runner {
    ProcessInfo info;
    std::optional<std::chrono::time_point<std::chrono::system_clock>> last_run =
        {};
    std::binary_semaphore* run_status = nullptr;
  };
  struct Stopper {
    std::optional<std::chrono::time_point<std::chrono::system_clock>>
        term_sent = {};

    bool is_ordinary = false;
    std::binary_semaphore* term_status = nullptr;
  };

  struct Client {
    std::optional<TCP::TcpServer::ClientConnection> connection = {};
    std::optional<std::thread> curr_communication = {};
    bool is_running = false;
  };

  // constructor / destructor //
  LauncherServer(int port, const std::string& config_file,
                 const std::string& agent_binary, logging_foo = LoggerCap);
  ~LauncherServer();

  // boot configuration //
  void GetConfig() noexcept;
  void SaveConfig() const noexcept;

  // thread functions //
  void Accepter() noexcept;
  void Receiver() noexcept;
  void ProcessCtrl() noexcept;

  void ClientCommunication(std::list<Client>::iterator* client) noexcept;

  bool RunProcess(std::string&& bin_name, ProcessConfig&& process,
                  bool wait_for_run = false) noexcept;
  bool StopProcess(const std::string& bin_name,
                   bool wait_for_term = false) noexcept;

  // atomic operations //
  void ALoad(TCP::TcpServer::ClientConnection client);
  void AStop(TCP::TcpServer::ClientConnection client);
  void ARerun(TCP::TcpServer::ClientConnection client);
  void AIsRunning(TCP::TcpServer::ClientConnection client);
  void AGetPid(TCP::TcpServer::ClientConnection client);
  void AGetConfig(TCP::TcpServer::ClientConnection client);
  void ASetConfig(TCP::TcpServer::ClientConnection client);

  // secondary functions //
  bool SendRun(const std::string& name, const ProcessConfig& config) noexcept;
  void PrCtrlToRun() noexcept;
  void PrCtrlToTerm() noexcept;
  void PrCtrlMain() noexcept;
  bool IsPidAvailable(int pid) const noexcept;

  static const int kNumAMethods = 7;
  typedef void (LauncherServer::*MethodPtr)(TCP::TcpServer::ClientConnection);
  MethodPtr method_ptr[kNumAMethods] = {
      &LauncherServer::ALoad,     &LauncherServer::AStop,
      &LauncherServer::ARerun,    &LauncherServer::AIsRunning,
      &LauncherServer::AGetPid,   &LauncherServer::AGetConfig,
      &LauncherServer::ASetConfig};

  // variables //
  std::map<std::string, ProcessConfig> load_config_;
  std::map<std::string, ProcessInfo> processes_;

  std::map<std::string, Runner> processes_to_run_;
  std::mutex pr_to_run_erasing_;

  std::map<std::string, Stopper> processes_to_terminate_;

  TCP::TcpServer tcp_server_;
  std::list<Client> clients_;

  std::string agent_binary_;
  std::string config_file_;
  int port_;

  std::thread accepter_;
  std::thread receiver_;
  std::thread process_ctrl_;

  bool is_active_ = true;

  logging_foo logger_;
};

}  // namespace LNCR