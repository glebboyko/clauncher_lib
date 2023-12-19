#include "clauncher-server.hpp"

#include <signal.h>
#include <unistd.h>

#include <fstream>

namespace LNCR {

/*-------------------------------- constants ---------------------------------*/
const int kMaxQueue = 1024;
const std::chrono::milliseconds kWaitToRerun = std::chrono::milliseconds(100);
const std::chrono::milliseconds kLoopWait = std::chrono::milliseconds(100);

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

void LauncherServer::PrCtrlToRun() noexcept {
  pr_to_run_erasing_.lock();
  for (auto iter = processes_to_run_.begin();
       iter != processes_to_run_.end();) {
    auto& [bin_name, runner] = *iter;
    if (runner.info.pid.has_value()) {    // process has already sent config
      if (runner.last_run.has_value()) {  // process has not been moved yet
        processes_.insert({bin_name, runner.info});
        runner.last_run = {};  // moved flag
      }
      if (!runner.run_status.has_value()) {  // runner is not waiting for result
        iter = processes_to_run_.erase(iter);
        continue;
      }
    } else if (runner.last_run
                   .has_value()) {  // process run but has not sent config
      if (std::chrono::system_clock::now() - runner.last_run.value() >=
          kWaitToRerun) {      // is timeout
        runner.last_run = {};  // setting rerun flag
      }
    }
    if (!runner.info.pid.has_value() &&
        !runner.last_run.has_value()) {  // run flag set
      if (SendRun(bin_name, runner.info.config)) {
        runner.last_run = std::chrono::system_clock::now();
      }
    }
    ++iter;
  }
  pr_to_run_erasing_.unlock();
}
void LauncherServer::PrCtrlToTerm() noexcept {
  for (auto iter = processes_to_terminate_.begin();
       iter != processes_to_terminate_.end();) {
    auto& [bin_name, deleter] = *iter;
    if (processes_.contains(bin_name)) {  // requires to terminate
      auto main_iter = processes_.find(bin_name);
      if (!deleter.term_sent.has_value()) {  // ordinary termination
        kill(main_iter->second.pid.value(), SIGTERM);
        if (!main_iter->second.config.time_to_stop
                 .has_value()) {  // no after checking required
          processes_.erase(main_iter);
          if (deleter.term_status.has_value()) {
            deleter.term_status.value().release();
          }
        } else {  // after checking required
          deleter.term_sent = std::chrono::system_clock::now();
        }
      }

      if (deleter.term_sent.has_value()) {  // termination checker
        if (!IsPidAvailable(main_iter->second.pid.value())) {  // is terminated
          processes_.erase(main_iter);
          if (deleter.term_status.has_value()) {
            deleter.term_status.value().release();
          }
        } else {  // is not terminated
          if (std::chrono::system_clock::now() - deleter.term_sent.value() >
              main_iter->second.config.time_to_stop) {  // is timeout
            kill(main_iter->second.pid.value(), SIGKILL);
            processes_.erase(main_iter);
            if (deleter.term_status.has_value()) {
              deleter.term_status.value().release();
            }
          }  // else skip
        }
      }
    }
    if (!processes_.contains(bin_name) && !deleter.term_sent.has_value()) {
      iter = processes_to_terminate_.erase(iter);
    } else {
      ++iter;
    }
  }
}
void LauncherServer::PrCtrlMain() noexcept {
  for (auto iter = processes_.begin(); iter != processes_.end();) {
    if (!processes_to_terminate_.contains(iter->first)) {
      if (!IsPidAvailable(iter->second.pid.value())) {
        if (iter->second.config.term_rerun) {
          auto bin_name = iter->first;
          auto config = std::move(iter->second.config);
          RunProcess(std::move(bin_name), std::move(config));
        }

        iter = processes_.erase(iter);
        continue;
      }
    }
    ++iter;
  }
}

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

  for (auto&& [bin_name, process] : GetConfig()) {
    RunProcess(std::move(bin_name), std::move(process));
  }
}

LauncherServer::~LauncherServer() {
  is_active_ = false;
  receiver_.join();

  tcp_server_.CloseListener();
  accepter_.join();

  for (auto& [connection, curr_communication, is_running] : clients_) {
    if (connection.has_value()) {
      tcp_server_.CloseConnection(connection.value());
      if (curr_communication.has_value()) {
        curr_communication->join();
      }
    }
  }

  SaveConfig();

  for (const auto& [bin_name, process] : processes_) {
    StopProcess(bin_name, false);
  }
  process_ctrl_.join();
}

/*---------------------------- boot configuration ----------------------------*/
std::list<std::pair<std::string, ProcessConfig>> LauncherServer::GetConfig()
    const noexcept {
  std::list<std::pair<std::string, ProcessConfig>> processes;

  std::ifstream config(config_file_);
  if (!config.is_open()) {
    return processes;
  }
  if (config.eof()) {
    config.close();
    return processes;
  }

  do {
    std::string bin_name;
    ProcessConfig info;
    config >> bin_name;

    int arg_num;
    config >> arg_num;
    for (int i = 0; i < arg_num; ++i) {
      std::string arg;
      config >> arg;
      info.args.push_back(std::move(arg));
    }

    info.launch_on_boot = true;
    config >> info.term_rerun;

    int delay;
    config >> delay;
    if (delay != 0) {
      info.time_to_stop = std::chrono::milliseconds(delay);
    }

    processes.push_back({std::move(bin_name), std::move(info)});
  } while (!config.eof());

  config.close();
  return processes;
}
void LauncherServer::SaveConfig() const noexcept {
  std::ofstream config(config_file_);
  if (!config.is_open()) {
    return;
  }

  for (const auto& [bin_name, process] : processes_) {
    if (!process.config.launch_on_boot) {
      continue;
    }

    config << bin_name << "\t";

    config << process.config.args.size() << "\t";
    for (const auto& arg : process.config.args) {
      config << arg << "\t";
    }

    config << process.config.term_rerun << "\t";
    config << (process.config.time_to_stop.has_value()
                   ? process.config.time_to_stop.value().count()
                   : 0)
           << "\n";
  }

  config.close();
}

/*----------------------------- thread functions -----------------------------*/
void LauncherServer::Accepter() noexcept {
  while (is_active_) {
    TCP::TcpServer::ClientConnection* connection;
    try {
      connection =
          new TCP::TcpServer::ClientConnection(tcp_server_.AcceptConnection());
    } catch (TCP::TcpException& exception) {
      if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
        return;
      }
      continue;
    }

    auto init_receiver = [this, connection]() {
      int send_from;
      try {
        tcp_server_.Receive(*connection, send_from);
        if (send_from == SenderStatus::Client) {
          clients_.push_back({.connection = *connection});
          delete connection;
          return;
        }
        if (send_from == SenderStatus::Agent) {
          // get NAME, PID and ERROR
          std::string process_name;
          int pid;
          int error;
          tcp_server_.Receive(*connection, process_name, pid, error);
          tcp_server_.CloseConnection(*connection);

          // block tables to use
          pr_to_run_erasing_.lock();

          // check where is it contained
          bool to_run = processes_to_run_.contains(process_name);

          if (!to_run && error == 0) {  // if nowhere
            kill(pid, SIGKILL);
          } else if (error == 0) {  // if it is init mode
            auto& process = processes_to_run_[process_name];
            process.info.pid = pid;  // set pid : "successful run" flag
            if (process.run_status
                    .has_value()) {  // check if run process wait for result
              process.run_status.value().release();
            }
          } else {  // error mode
            // do nothing because it will be processed by ctrl
          }

          // unlock mutexes
          pr_to_run_erasing_.unlock();
        }
      } catch (TCP::TcpException& tcp_exception) {
        if (tcp_exception.GetType() != TCP::TcpException::ConnectionBreak) {
          tcp_server_.CloseConnection(*connection);
        }
      }
      delete connection;
    };

    try {
      std::thread(init_receiver).detach();
    } catch (std::system_error& error) {
      tcp_server_.CloseConnection(*connection);
      delete connection;
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
void LauncherServer::ProcessCtrl() noexcept {
  while (is_active_ || !processes_.empty()) {
    PrCtrlToRun();
    PrCtrlToTerm();
    PrCtrlMain();
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

}  // namespace LNCR