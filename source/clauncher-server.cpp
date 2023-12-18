#include "clauncher-server.hpp"

#include <signal.h>
#include <unistd.h>

#include <fstream>

namespace CLGR {

/*--------------------------- secondary functions ----------------------------*/
std::list<std::string> Split(const std::string& string, const char& delimiter) {
  std::list<std::string> split;
  size_t start_from = 0;
  while (true) {
    size_t pos = string.find(delimiter, start_from);
    if (pos == std::variant_npos) {
      split.push_back(string.substr(start_from, string.size() - start_from));
      break;
    }
    split.push_back(string.substr(start_from, pos - start_from));
    start_from = pos + 1;
  }
  return split;
}

template <typename T>
auto SaveListErase(std::list<T>& list, decltype(list.begin()) iter) {
  auto deleting_iter = iter;
  iter++;
  list.erase(deleting_iter);
  return iter;
}

/*-------------------------------- constants ---------------------------------*/
const int kMaxQueue = 1024;
const std::chrono::milliseconds kWaitToRerun = std::chrono::milliseconds(100);
const std::chrono::milliseconds kLoopWait = std::chrono::milliseconds(100);

/*------------------------- constructor / destructor -------------------------*/
LauncherServer::LauncherServer(int port, const std::string& config_file,
                               const std::string& agent_binary,
                               logging_foo logger)
    : port_(port),
      tcp_server_(0, port, logger, kMaxQueue),
      config_file_(config_file),
      agent_binary_(agent_binary),
      logger_(logger) {
  accepter_ = std::thread(&LauncherServer::Accepter, this);
  receiver_ = std::thread(&LauncherServer::Receiver, this);
  process_ctrl_ = std::thread(&LauncherServer::ProcessCtrl, this);

  GetConfig();

  for (int i = 0; i < processes_.size(); ++i) {
    RunProcess(i);
  }
}

LauncherServer::~LauncherServer() {
  is_active_ = false;
  receiver_.join();
  process_ctrl_.join();

  tcp_server_.CloseListener();
  accepter_.join();

  for (auto& process : processes_) {
    if (process.pid.has_value()) {
      kill(process.pid.value(), SIGTERM);
    }
  }

  for (auto& [connection, curr_communication, is_running] : clients_) {
    if (connection.has_value()) {
      tcp_server_.CloseConnection(connection.value());
      if (curr_communication.has_value()) {
        curr_communication->join();
      }
    }
  }

  SaveConfig();
}

/*---------------------------- boot configuration ----------------------------*/
void LauncherServer::GetConfig() noexcept {
  processes_.clear();

  std::ifstream config(config_file_);
  if (!config.is_open()) {
    return;
  }
  if (config.eof()) {
    config.close();
    return;
  }

  do {
    ProcessInfo info;
    config >> info.bin_name;

    int arg_num;
    config >> arg_num;
    for (int i = 0; i < arg_num; ++i) {
      std::string arg;
      config >> arg;
      info.args.push_back(std::move(arg));
    }

    info.config.launch_on_boot = true;
    config >> info.config.term_rerun;

    processes_.push_back(std::move(info));
  } while (!config.eof());

  config.close();
}
void LauncherServer::SaveConfig() noexcept {
  std::ofstream config(config_file_);
  if (!config.is_open()) {
    return;
  }

  for (const auto& process : processes_) {
    if (!process.config.launch_on_boot) {
      continue;
    }

    config << process.bin_name << "\t";

    config << process.args.size() << "\t";
    for (const auto& arg : process.args) {
      config << arg << "\t";
    }

    config << process.config.term_rerun << "\n";
  }

  config.close();
}

/*----------------------------- thread functions -----------------------------*/
void LauncherServer::Accepter() noexcept {
  while (is_active_) {
    std::optional<decltype(tcp_server_.AcceptConnection())> connection;
    try {
      connection = tcp_server_.AcceptConnection();
      int send_from;
      tcp_server_.Receive(connection.value(), send_from);
      if (send_from == SenderStatus::Client) {
        clients_.push_back({.connection = connection.value()});
        continue;
      }
      if (send_from == SenderStatus::Agent) {
        int process_num, nd_arg;
        tcp_server_.Receive(connection.value(), process_num, nd_arg);
        if (processes_[process_num].pid.has_value()) {
          processes_[process_num].pid = {};
          processes_[process_num].last_run = std::chrono::system_clock::now();
        } else {
          processes_[process_num].last_run = {};
          processes_[process_num].pid = nd_arg;
        }
      }
      tcp_server_.CloseConnection(connection.value());
    } catch (TCP::TcpException& exception) {
      if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
        return;
      }

      if (connection.has_value()) {
        tcp_server_.CloseConnection(connection.value());
      }
      continue;
    }
  }
}
void LauncherServer::Receiver() noexcept {
  while (is_active_) {
    for (auto iter = clients_.begin(); is_active_ && iter != clients_.end();) {
      // terminating communication
      if (iter->is_running) {
        continue;
      }
      if (iter->curr_communication.has_value()) {
        iter->curr_communication->join();
        iter->curr_communication = {};
      }
      if (!iter->connection.has_value()) {
        iter = SaveListErase(clients_, iter);
        continue;
      }

      try {
        if (tcp_server_.IsAvailable(iter->connection.value())) {
          iter->curr_communication =
              std::thread(&LauncherServer::ClientCommunication, this,
                          new decltype(iter)(iter));
          iter->is_running = true;
        }
      } catch (TCP::TcpException& tcp_exception) {
        if (tcp_exception.GetType() == TCP::TcpException::ConnectionBreak) {
          iter = SaveListErase(clients_, iter);
          continue;
        }
      } catch (std::system_error& thread_error) {
      }

      ++iter;
    }
    std::this_thread::sleep_for(kLoopWait);
  }
}

void LauncherServer::ClientCommunication(
    std::list<Client>::iterator* client) noexcept {
  try {
    int command;
    tcp_server_.Receive((*client)->connection.value(), command);
    (this->*method_ptr[command])((*client)->connection.value());
  } catch (TCP::TcpException& tcp_exception) {
    if (tcp_exception.GetType() == TCP::TcpException::ConnectionBreak) {
      (*client)->connection = {};
    }
  }
  (*client)->is_running = false;
  delete client;
}

/*---------------------------- checking functions ----------------------------*/
bool LauncherServer::ShouldReRun(
    const ProcessInfo& process_info) const noexcept {
  if (process_info.pid.has_value() || !process_info.last_run.has_value()) {
    return false;
  }
  return std::chrono::system_clock::now() - process_info.last_run.value() >=
         kWaitToRerun;
}

}  // namespace CLGR