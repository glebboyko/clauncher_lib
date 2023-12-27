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

#include "clauncher-server.hpp"
#include "clauncher-supply.hpp"
#include "tcp-server.hpp"

namespace LNCR {

struct LauncherServer::Implementation {
  // structs //
  struct ProcessInfo {
    ProcessConfig config;
    int pid = 0;
  };
  struct Runner {
    ProcessInfo info;
    std::optional<std::chrono::time_point<std::chrono::system_clock>> last_run =
        {};

    int* run_status = nullptr;
    std::binary_semaphore* run_semaphore = nullptr;
  };
  struct Stopper {
    std::optional<std::chrono::time_point<std::chrono::system_clock>>
        term_sent = {};

    int* term_status = nullptr;
    std::binary_semaphore* term_semaphore = nullptr;
  };

  struct Client {
    std::optional<TCP::TcpServer::ClientConnection> connection = {};
    std::optional<std::thread> curr_communication = {};
    bool is_running = false;
  };

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
  Stopper::TermStatus StopProcess(const std::string& bin_name,
                                  bool wait_for_term = false) noexcept;

  // atomic operations //
  void ALoad(TCP::TcpServer::ClientConnection client);
  void AStop(TCP::TcpServer::ClientConnection client);
  void ARerun(TCP::TcpServer::ClientConnection client);
  void AIsRunning(TCP::TcpServer::ClientConnection client);
  void AGetPid(TCP::TcpServer::ClientConnection client);
  // void AGetConfig(TCP::TcpServer::ClientConnection client);
  // void ASetConfig(TCP::TcpServer::ClientConnection client);

  // secondary functions //
  void SendRun(const std::string& name, const ProcessConfig& config) noexcept;

  void PrCtrlToRun() noexcept;
  void PrCtrlToTerm() noexcept;
  void ProcessChangeSend(int status, std::binary_semaphore*&, int*&,
                         Logger&) noexcept;

  void PrCtrlMain() noexcept;
  bool IsPidAvailable(int pid) const noexcept;
  std::optional<int> GetPid(const std::string& bin_name) noexcept;
  bool IsRunning(const std::string& bin_name) noexcept;

  static const int kNumAMethods = 5;
  typedef void (Implementation::*MethodPtr)(TCP::TcpServer::ClientConnection);
  MethodPtr method_ptr[kNumAMethods] = {
      &Implementation::ALoad, &Implementation::AStop, &Implementation::ARerun,
      &Implementation::AIsRunning, &Implementation::AGetPid};

  // variables //
  std::map<std::string, ProcessConfig> load_config_;
  std::mutex load_conf_m_;

  std::map<std::string, ProcessInfo> processes_;
  std::mutex pr_main_m_;

  std::map<std::string, Runner> processes_to_run_;
  std::mutex pr_to_run_m_;

  std::map<std::string, Stopper> processes_to_terminate_;
  std::mutex pr_to_term_m_;

  TCP::TcpServer tcp_server_;
  std::list<Client> clients_;
  std::mutex clients_m_;

  std::string agent_binary_;
  std::string config_file_;
  int port_;

  std::thread accepter_;
  std::thread receiver_;
  std::thread process_ctrl_;

  bool is_active_ = true;

  logging_foo logger_;
};

};  // namespace LNCR