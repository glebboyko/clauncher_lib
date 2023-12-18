#pragma once

#include <chrono>
#include <list>
#include <optional>
#include <semaphore>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "clauncher-supply.hpp"
#include "tcp-server.hpp"

namespace CLGR {

class LauncherServer {
 public:
  // structs //
  struct ProcessInfo {
    std::list<std::string> args;

    ProcessConfig config;
    std::optional<int> pid;

    std::optional<std::chrono::time_point<std::chrono::system_clock>> last_run;
  };
  struct Stopper {
    std::optional<std::chrono::milliseconds> time_to_stop;
    std::optional<std::chrono::time_point<std::chrono::system_clock>> term_sent;
    std::optional<std::binary_semaphore> term_status;
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
  std::list<std::pair<std::string, ProcessInfo>> GetConfig() const noexcept;
  void SaveConfig() const noexcept;

  // thread functions //
  void Accepter() noexcept;
  void Receiver() noexcept;
  void ProcessCtrl() noexcept;

  void ClientCommunication(std::list<Client>::iterator* client) noexcept;

  void RunProcess(std::string&& bin_name, ProcessInfo&& process) noexcept;
  bool StopProcess(
      std::string&& bin_name, bool wait_for_term = false,
      std::optional<std::chrono::milliseconds> time_to_stop = {}) noexcept;

  bool ShouldReRun(const ProcessInfo& process_info) const noexcept;
  const ProcessInfo& GetProcess(const std::string& bin_name) noexcept;

  // atomic operations //
  void ALoad(TCP::TcpServer::ClientConnection client);
  void AStop(TCP::TcpServer::ClientConnection client);
  void ARerun(TCP::TcpServer::ClientConnection client);
  void AIsRunning(TCP::TcpServer::ClientConnection client);
  void AGetPid(TCP::TcpServer::ClientConnection client);
  void AGetConfig(TCP::TcpServer::ClientConnection client);
  void ASetConfig(TCP::TcpServer::ClientConnection client);

  static const int kNumAMethods = 7;
  typedef void (LauncherServer::*MethodPtr)(TCP::TcpServer::ClientConnection);
  MethodPtr method_ptr[kNumAMethods] = {
      &LauncherServer::ALoad,     &LauncherServer::AStop,
      &LauncherServer::ARerun,    &LauncherServer::AIsRunning,
      &LauncherServer::AGetPid,   &LauncherServer::AGetConfig,
      &LauncherServer::ASetConfig};

  // variables //
  std::unordered_map<std::string, ProcessInfo> processes_;
  std::unordered_map<std::string, Stopper> processes_to_terminate_;

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

}  // namespace CLGR