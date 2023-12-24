#include "clauncher-server.hpp"

#include <signal.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

#include "clauncher-server-impl.hpp"

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

void LauncherServer::Implementation::PrCtrlToRun() noexcept {
  LServer l_server(LServer::PrCtrlToRun, logger_);
  Logger& logger = l_server;
  logger.Log("Starting function", Debug);

  logger.Log("Locking mutex", Debug);
  pr_to_run_m_.lock();
  logger.Log("Mutex locked. Entering loop", Debug);
  for (auto iter = processes_to_run_.begin();
       iter != processes_to_run_.end();) {
    auto& [bin_name, runner] = *iter;
    logger.Log("Processing " + bin_name, Info);
    if (runner.info.pid != 0) {  // process has already sent config
      logger.Log("Process has already sent config", Debug);
      if (runner.last_run.has_value()) {  // process has not been moved yet
        processes_.insert({bin_name, runner.info});
        runner.last_run = {};  // moved flag
        logger.Log("Moving process to main table", Info);
      } else {
        logger.Log("Process has already moved to main table", Info);
      }
      if (runner.run_status == nullptr) {  // runner is not waiting for result
        iter = processes_to_run_.erase(iter);
        logger.Log(
            "Runner is not waiting for result. Erasing process in run table",
            Info);
        continue;
      }
      logger.Log("Runner is waiting for result", Debug);
    } else if (runner.last_run
                   .has_value()) {  // process run but has not sent config
      logger.Log("Process is running but has not sent config", Info);
      if (std::chrono::system_clock::now() - runner.last_run.value() >=
          kWaitToRerun) {      // is timeout
        runner.last_run = {};  // setting rerun flag
        logger.Log("Launching timeout. Rerunning", Info);
      } else {
        logger.Log("Launching not timeout", Debug);
      }
    }
    if (runner.info.pid == 0 && !runner.last_run.has_value()) {  // run flag set
      runner.last_run = std::chrono::system_clock::now();
      SendRun(bin_name, runner.info.config);
      logger.Log("Set run flag. Agent has been run", Info);
    }
    ++iter;
  }
  pr_to_run_m_.unlock();
  logger.Log("Mutex unlocked", Debug);
}
void LauncherServer::Implementation::PrCtrlToTerm() noexcept {
  LServer l_server(LServer::PrCtrlToTerm, logger_);
  Logger& logger = l_server;
  logger.Log("Starting function", Debug);

  logger.Log("Locking mutex", Debug);
  pr_main_m_.lock();
  pr_to_term_m_.lock();
  logger.Log("Mutex locked. Entering loop", Debug);

  for (auto iter = processes_to_terminate_.begin();
       iter != processes_to_terminate_.end();) {
    auto& [bin_name, deleter] = *iter;
    logger.Log("Processing " + bin_name, Info);

    if (processes_.contains(bin_name)) {  // requires to terminate
      logger.Log("Process is required to be terminated", Info);
      auto main_iter = processes_.find(bin_name);
      if (!deleter.term_sent.has_value()) {  // ordinary termination
        logger.Log("Ordinary termination. Sending SIGTERM signal", Info);
        kill(main_iter->second.pid, SIGTERM);
        if (!main_iter->second.config.time_to_stop
                 .has_value()) {  // no after checking required
          logger.Log(
              "Process does not require termination checking. Erasing from "
              "main table",
              Info);
          processes_.erase(main_iter);
          if (deleter.term_status != nullptr) {
            logger.Log("Runner is waiting for result", Info);
            deleter.is_ordinary = true;
            deleter.term_status->release();
          } else {
            logger.Log("Runner is not waiting for result", Debug);
          }
        } else {  // after checking required
          deleter.term_sent = std::chrono::system_clock::now();
          logger.Log("Process requires termination checking. Setting timer",
                     Info);
        }
      } else {
        logger.Log("Termination timer has already been set", Debug);
      }

      if (deleter.term_sent.has_value()) {  // termination checker
        logger.Log("Termination timer is set", Info);
        if (!IsPidAvailable(main_iter->second.pid)) {  // is terminated
          logger.Log("Process has already terminated. Erasing from main table",
                     Info);
          processes_.erase(main_iter);
          if (deleter.term_status != nullptr) {
            logger.Log("Runner is waiting for result", Info);
            deleter.is_ordinary = true;
            deleter.term_status->release();
          } else {
            logger.Log("Runner is not waiting for result", Debug);
          }
        } else {  // is not terminated
          logger.Log("Process has not terminated", Info);
          if (std::chrono::system_clock::now() - deleter.term_sent.value() >
              main_iter->second.config.time_to_stop) {  // is timeout
            logger.Log(
                "Termination timeout. Sending SIGKILL signal. Erasing from "
                "main table",
                Info);
            kill(main_iter->second.pid, SIGKILL);
            processes_.erase(main_iter);
            if (deleter.term_status != nullptr) {
              logger.Log("Runner is waiting for result", Info);
              deleter.is_ordinary = false;
              deleter.term_status->release();
            } else {
              logger.Log("Runner is not waiting for result", Debug);
            }
          }  // else skip
        }
      } else {
        logger.Log("Termination timer is not set", Debug);
      }
    } else {
      logger.Log("Process has already been terminated", Debug);
    }
    if (!processes_.contains(bin_name) && deleter.term_status == nullptr) {
      logger.Log(
          "Process has been terminated, runner is not waiting for result. "
          "Erasing from termination table",
          Info);
      iter = processes_to_terminate_.erase(iter);
    } else {
      logger.Log("Process cannot be erased from termination table", Debug);
      ++iter;
    }
  }

  logger.Log("Unlocking mutex", Debug);
  pr_to_term_m_.unlock();
  pr_main_m_.unlock();
  logger.Log("Function finish", Info);
}
void LauncherServer::Implementation::PrCtrlMain() noexcept {
  LServer l_server(LServer::PrCtrlMain, logger_);
  Logger& logger = l_server;
  logger.Log("Starting function", Debug);

  logger.Log("Locking mutex", Debug);
  pr_main_m_.lock();
  logger.Log("Mutex locked. Entering loop", Debug);
  for (auto iter = processes_.begin(); iter != processes_.end();) {
    logger.Log("Processing process " + iter->first, Info);
    if (!processes_to_terminate_.contains(iter->first)) {
      logger.Log("Process is not to terminate", Info);
      if (!IsPidAvailable(iter->second.pid)) {
        logger.Log("Process is not running", Info);
        if (iter->second.config.term_rerun) {
          logger.Log("Prosess's rerun flag is set to true. Rerunning", Info);
          auto bin_name = iter->first;
          auto config = std::move(iter->second.config);

          pr_main_m_.unlock();
          logger.Log("Mutex unlocked", Debug);

          RunProcess(std::move(bin_name), std::move(config));

          logger.Log("Locking mutex", Debug);
          pr_main_m_.lock();
          logger.Log("Mutex locked", Debug);

        } else {
          logger.Log("Process's rerun flag is set to false", Debug);
        }

        logger.Log("Process erasing from main table", Info);
        iter = processes_.erase(iter);
        continue;
      } else {
        logger.Log("Process is running", Debug);
      }
    } else {
      logger.Log("Process is set to terminate", Debug);
    }
    ++iter;
  }
  pr_main_m_.unlock();
  logger.Log("Mutex unlocked", Debug);
}

bool LauncherServer::Implementation::RunProcess(std::string&& bin_name,
                                                LNCR::ProcessConfig&& process,
                                                bool wait_for_run) noexcept {
  LServer l_server(LServer::RunProcess, logger_);
  Logger& logger = l_server;
  logger.Log("Process: " + bin_name, Info);

  logger.Log("Locking main and run mutexes", Debug);
  pr_main_m_.lock();
  pr_to_run_m_.lock();
  logger.Log("Mutexes main and run locked", Debug);

  bool is_running =
      processes_to_run_.contains(bin_name) || processes_.contains(bin_name);

  pr_main_m_.unlock();
  logger.Log("Mutex main unlocked", Debug);

  if (is_running) {
    logger.Log("Process is already running", Info);

    pr_to_run_m_.unlock();
    logger.Log("Mutex run unlocked", Debug);
    return false;
  }

  logger.Log("Locking load mutex", Debug);
  load_conf_m_.lock();
  logger.Log("Mutex load locked", Debug);

  if (process.launch_on_boot) {
    logger.Log("Process will be launched on boot", Info);

    if (!load_config_.contains(bin_name)) {
      logger.Log("Inserting process into loading table", Debug);
      load_config_.insert({bin_name, process});
    } else {
      load_config_[bin_name] = process;
      logger.Log("Load table already contains process", Debug);
    }
  } else {
    logger.Log(
        "Process will not be launched on boot. Trying to erase out of date "
        "content",
        Info);
    load_config_.erase(bin_name);
  }

  load_conf_m_.unlock();
  logger.Log("Mutex load unlocked", Debug);

  std::binary_semaphore* semaphore =
      wait_for_run ? new std::binary_semaphore(0) : nullptr;
  Runner runner = {.info = {.config = std::move(process)},
                   .run_status = semaphore};
  logger.Log("Inserting process into run table", Debug);
  auto inserted =
      processes_to_run_.insert({std::move(bin_name), std::move(runner)});

  pr_to_run_m_.unlock();
  logger.Log("Mutex run unlocked", Debug);

  if (wait_for_run) {
    logger.Log("Runner is waiting for running", Info);
    semaphore->acquire();
    inserted.first->second.run_status = nullptr;
    delete semaphore;
    logger.Log("Process is running", Info);
  } else {
    logger.Log("Runner is not waiting for running", Info);
  }
  return true;
}
bool LauncherServer::Implementation::StopProcess(const std::string& bin_name,
                                                 bool wait_for_term) noexcept {
  LServer l_server(LServer::StopProcess, logger_);
  Logger& logger = l_server;
  logger.Log("Process: " + bin_name, Info);

  logger.Log("Locking load mutex", Debug);
  load_conf_m_.lock();
  logger.Log("Load mutex locked", Debug);

  logger.Log("Trying to erase process from load table", Debug);
  if (load_config_.erase(bin_name) == 1) {
    logger.Log("Process erased from load table", Info);
  } else {
    logger.Log("Table was not contain this process", Debug);
  }

  load_conf_m_.unlock();
  logger.Log("Load mutex unlocked", Debug);

  std::binary_semaphore* semaphore =
      wait_for_term ? new std::binary_semaphore(0) : nullptr;
  Stopper stopper = {.term_status = semaphore};

  logger.Log("Locking mutex", Debug);
  pr_to_term_m_.lock();
  logger.Log("Mutex locked", Debug);

  auto iter =
      processes_to_terminate_.insert({std::move(bin_name), std::move(stopper)});

  pr_to_term_m_.unlock();
  logger.Log("Mutex unlocked", Debug);

  if (!iter.second) {
    if (semaphore != nullptr) {
      delete semaphore;
    }
    logger.Log("Term table has already contained process. Term exec", Info);
    return true;
  } else {
    logger.Log("Process inserted to term table", Debug);
  }

  bool result = true;

  if (wait_for_term) {
    logger.Log("Terminator is waiting for terminating", Info);
    semaphore->acquire();
    result = iter.first->second.is_ordinary;
    iter.first->second.term_status = nullptr;
    delete semaphore;
    logger.Log("Process is terminated with " + std::to_string(result), Info);
  } else {
    logger.Log("Terminator is not waiting for terminating", Info);
  }
  return result;
}

void LauncherServer::Implementation::SendRun(
    const std::string& name, const LNCR::ProcessConfig& config) noexcept {
  LServer l_server(LServer::SentRun, logger_);
  Logger& logger = l_server;
  logger.Log("Process: " + name, Debug);

  std::string launch_conf =
      agent_binary_ + " " + std::to_string(port_) + " " + name + " ";
  for (const auto& arg : config.args) {
    launch_conf += arg + " ";
  }
  launch_conf += "&";

  system(launch_conf.c_str());
  logger.Log("Agent launched", Debug);
}
bool LauncherServer::Implementation::IsPidAvailable(int pid) const noexcept {
  return kill(pid, 0) == 0;
}

std::optional<int> LauncherServer::Implementation::GetPid(
    const std::string& bin_name) noexcept {
  LServer l_server(LServer::GetPid, logger_);
  Logger& logger = l_server;
  logger.Log("Process: " + bin_name, Debug);

  logger.Log("Locking mutex", Debug);
  pr_main_m_.lock();
  logger.Log("Mutex locked", Debug);

  if (!processes_.contains(bin_name)) {
    logger.Log("Main table does not contain process, unlocking mutex", Debug);
    pr_main_m_.unlock();
    return {};
  }

  int pid = processes_[bin_name].pid;
  logger.Log("Main table contains process. PID: " + std::to_string(pid) +
                 ". Unlocking mutex",
             Debug);

  pr_main_m_.unlock();

  return pid;
}

/*------------------------- constructor / destructor -------------------------*/
LauncherServer::LauncherServer(int port, const std::string& config_file,
                               const std::string& agent_binary,
                               logging_foo logging_f) {
  LServer l_server(LServer::Constructor, logging_f);
  Logger& logger = l_server;
  logger.Log("Creating launcher server", Info);

  logger.Log("Init implementation var. Creating tcp-server", Debug);
  implementation_ = std::unique_ptr<Implementation>(new Implementation{
      .tcp_server_ = TCP::TcpServer(0, port, logging_f, kMaxQueue),
      .agent_binary_ = agent_binary,
      .config_file_ = config_file,
      .port_ = port,
      .logger_ = logging_f});
  logger.Log(
      "TCP-server created. Implementation var inited. Getting load config",
      Debug);
  implementation_->GetConfig();
  for (const auto& [bin_name, process] : implementation_->load_config_) {
    auto c_bin_name = bin_name;
    auto c_process = process;
    implementation_->RunProcess(std::move(c_bin_name), std::move(c_process));
  }
  logger.Log("Config got", Debug);

  logger.Log("Creating threads", Debug);
  implementation_->accepter_ =
      std::thread(&Implementation::Accepter, implementation_.get());
  implementation_->receiver_ =
      std::thread(&Implementation::Receiver, implementation_.get());
  implementation_->process_ctrl_ =
      std::thread(&Implementation::ProcessCtrl, implementation_.get());
  logger.Log("Threads created", Debug);
  logger.Log("Launcher server created", Info);
}

LauncherServer::~LauncherServer() {
  LServer l_server(LServer::Destructor, implementation_->logger_);
  Logger& logger = l_server;
  logger.Log("Deleting launcher server", Info);

  logger.Log("Setting terminating flag", Debug);
  implementation_->is_active_ = false;

  logger.Log("Joining receiver", Debug);
  implementation_->receiver_.join();
  logger.Log("Receiver joined", Debug);

  logger.Log("Closing listener", Debug);
  implementation_->tcp_server_.CloseListener();
  logger.Log("Joining accepter", Debug);
  implementation_->accepter_.join();
  logger.Log("Accepter joined", Debug);

  logger.Log("Terminating clients", Debug);
  for (auto& [connection, curr_communication, is_running] :
       implementation_->clients_) {
    if (connection.has_value()) {
      logger.Log("Client is running, closing connection", Debug);
      implementation_->tcp_server_.CloseConnection(connection.value());
      if (curr_communication.has_value()) {
        logger.Log("Joining client thread", Debug);
        curr_communication->join();
        logger.Log("Client thread joined", Debug);
      } else {
        logger.Log("Client is not running", Debug);
      }
    }
  }
  logger.Log("Clients terminated", Debug);

  logger.Log("Saving load config. Locking mutex", Debug);
  implementation_->load_conf_m_.lock();
  logger.Log("Mutex locked", Debug);
  implementation_->SaveConfig();
  implementation_->load_conf_m_.unlock();
  logger.Log("Mutex unlocked", Debug);

  for (const auto& [bin_name, process] : implementation_->processes_) {
    implementation_->StopProcess(bin_name, false);
  }
  logger.Log("Load config saved. Joining main table", Debug);
  implementation_->process_ctrl_.join();
  logger.Log("Main table joined", Debug);
  logger.Log("Server deleted", Info);
}

/*---------------------------- boot configuration ----------------------------*/
void LauncherServer::Implementation::GetConfig() noexcept {
  LServer l_server(LServer::GetConfig, logger_);
  Logger& logger = l_server;
  logger.Log("Getting load config", Debug);

  logger.Log("Opening config file", Debug);
  std::ifstream config(config_file_);
  if (!config.is_open()) {
    logger.Log("Error while opening file", Warning);
    return;
  } else {
    logger.Log("File is opened", Debug);
  }
  if (config.peek() == std::ifstream::traits_type::eof()) {
    logger.Log("File is empty", Warning);
    config.close();
    return;
  }

  logger.Log("Getting configs from file", Debug);
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

    load_config_.insert({std::move(bin_name), std::move(info)});
  } while (!config.eof());
  logger.Log("Config got, closing file", Debug);

  config.close();
}
void LauncherServer::Implementation::SaveConfig() const noexcept {
  LServer l_server(LServer::SetConfig, logger_);
  Logger& logger = l_server;
  logger.Log("Saving load config", Debug);

  logger.Log("Trying to open file", Debug);
  std::ofstream config(config_file_);
  if (!config.is_open()) {
    logger.Log("Error while opening file", Warning);
    return;
  }

  logger.Log("Saving configs to file", Debug);
  for (const auto& [bin_name, process] : load_config_) {
    if (!process.launch_on_boot) {
      continue;
    }

    config << bin_name << "\t";

    config << process.args.size() << "\t";
    for (const auto& arg : process.args) {
      config << arg << "\t";
    }

    config << process.term_rerun << "\t";
    config << (process.time_to_stop.has_value()
                   ? process.time_to_stop.value().count()
                   : 0)
           << "\n";
  }
  logger.Log("Config saved. Closing file", Debug);

  config.close();
}

/*----------------------------- thread functions -----------------------------*/
void LauncherServer::Implementation::Accepter() noexcept {
  LServer l_server(LServer::Accepter, logger_);
  Logger& logger = l_server;
  logger.Log("Entering loop", Info);

  while (is_active_) {
    TCP::TcpServer::ClientConnection* connection;
    try {
      logger.Log("Trying to accept connection", Info);
      connection =
          new TCP::TcpServer::ClientConnection(tcp_server_.AcceptConnection());
      logger.Log("Connection accepted", Info);
    } catch (TCP::TcpException& exception) {
      if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
        logger.Log(
            "Listener was closed while trying to accept connection, "
            "terminating thread",
            Info);
        return;
      }
      logger.Log("Error occurred while trying to accept connection", Warning);
      continue;
    }

    auto init_receiver = [this, connection](LServer l_server) {
      Logger& logger = l_server;
      logger.Log("Entering init receiver foo", Info);

      int send_from;
      try {
        logger.Log("Trying to receive client status", Debug);
        tcp_server_.Receive(*connection, send_from);
        logger.Log("Status received: " +
                       std::string(send_from == SenderStatus::Client ? "Client"
                                                                     : "Agent"),
                   Info);
        if (send_from == SenderStatus::Client) {
          clients_.push_back({.connection = *connection});
          delete connection;
          logger.Log("Client inserted to table. Connection closed", Info);
          return;
        }
        if (send_from == SenderStatus::Agent) {
          // get NAME, PID and ERROR
          logger.Log("Receiving process config", Debug);
          std::string process_name;
          int pid;
          int error;
          tcp_server_.Receive(*connection, process_name, pid, error);
          tcp_server_.CloseConnection(*connection);
          logger.Log("Config received. Connection closed", Debug);

          // block tables to use
          logger.Log("Locking mutex", Debug);
          pr_to_run_m_.lock();
          logger.Log("Mutex locked", Debug);

          // check where is it contained
          bool to_run = processes_to_run_.contains(process_name);

          if (!to_run && error == 0) {  // if nowhere
            logger.Log(
                "Run table does not contain process, process is in init mode. "
                "Sending kill signal",
                Warning);
            kill(pid, SIGKILL);
          } else if (error == 0) {  // if it is init mode
            logger.Log("Run table contains process. Process is in init mode",
                       Info);
            auto& process = processes_to_run_[process_name];
            process.info.pid = pid;  // set pid : "successful run" flag
            if (process.run_status !=
                nullptr) {  // check if run process wait for result
              logger.Log("Runner is waiting. Realising", Info);
              process.run_status->release();
            } else {
              logger.Log("Runner is not waiting", Debug);
            }
          } else {  // error mode
            // do nothing because it will be processed by ctrl
            logger.Log("Process is in error mode: " + std::to_string(error),
                       Warning);
          }

          // unlock mutexes
          logger.Log("Unlocking mutex", Info);
          pr_to_run_m_.unlock();
        }
      } catch (TCP::TcpException& tcp_exception) {
        if (tcp_exception.GetType() != TCP::TcpException::ConnectionBreak) {
          tcp_server_.CloseConnection(*connection);
          logger.Log("Client broke connection. Connection deleted", Warning);
        } else {
          logger.Log("TCP error occurred: " + std::string(tcp_exception.what()),
                     Warning);
        }
      }
      delete connection;
    };

    try {
      logger.Log("Creating init receiver thread", Debug);
      std::thread(init_receiver, l_server).detach();
      logger.Log("Init receiver thread created", Debug);
    } catch (std::system_error& error) {
      logger.Log("Error occurred while creating thread", Warning);
      logger.Log("Closing connection", Debug);
      tcp_server_.CloseConnection(*connection);
      delete connection;
    }
  }
}
void LauncherServer::Implementation::Receiver() noexcept {
  LServer l_server(LServer::Receiver, logger_);
  Logger& logger = l_server;
  logger.Log("Entering loop", Info);

  while (is_active_) {
    logger.Log("Starting process clients", Info);
    for (auto iter = clients_.begin(); is_active_ && iter != clients_.end();) {
      logger.Log("Processing client", Debug);
      // terminating communication
      if (iter->is_running) {
        logger.Log("Client thread is running, skip", Debug);
        continue;
      }
      logger.Log("Client thread is not running", Debug);

      if (iter->curr_communication.has_value()) {
        logger.Log("Client thread has been terminated, joining", Debug);
        iter->curr_communication->join();
        iter->curr_communication = {};
      } else {
        logger.Log("Client thread has not been terminated", Debug);
      }

      if (!iter->connection.has_value()) {
        logger.Log("Client has been disconnected, erasing", Info);
        iter = SaveListErase(clients_, iter);
        continue;
      }
      logger.Log("Client is connected", Debug);

      try {
        logger.Log("Checking client message availability", Debug);
        if (tcp_server_.IsAvailable(iter->connection.value())) {
          logger.Log("Client message is available. Running thread", Info);
          iter->curr_communication =
              std::thread(&LauncherServer::Implementation::ClientCommunication,
                          this, new decltype(iter)(iter));
          iter->is_running = true;
          logger.Log("Thread is running", Debug);
        }
      } catch (TCP::TcpException& tcp_exception) {
        if (tcp_exception.GetType() == TCP::TcpException::ConnectionBreak) {
          logger.Log("Client connection broke, erasing", Warning);
          iter = SaveListErase(clients_, iter);
          continue;
        }
        logger.Log("Got exception while checking message availability:" +
                       std::string(tcp_exception.what()),
                   Warning);
      } catch (std::system_error& thread_error) {
        logger.Log("Error occurred while creating thread: " +
                       std::string(thread_error.what()),
                   Warning);
      }
      ++iter;
      logger.Log("Moving to next process", Debug);
    }
    logger.Log("Processed all connections. Sleeping", Info);
    std::this_thread::sleep_for(kLoopWait);
  }
}
void LauncherServer::Implementation::ProcessCtrl() noexcept {
  LServer l_server(LServer::ProcessCtrl, logger_);
  Logger& logger = l_server;
  logger.Log("Entering loop", Info);

  while (is_active_ || !processes_.empty()) {
    logger.Log("Running Run table processing", Info);
    PrCtrlToRun();
    logger.Log("Running Terminating table processing", Info);
    PrCtrlToTerm();
    logger.Log("Running Main table processing", Info);
    PrCtrlMain();
    std::this_thread::sleep_for(kLoopWait);
  }
}

void LauncherServer::Implementation::ClientCommunication(
    std::list<Client>::iterator* client) noexcept {
  LServer l_server(LServer::ClientComm, logger_);
  Logger& logger = l_server;
  logger.Log("Starting communication", Info);

  try {
    logger.Log("Trying to receive command", Debug);
    int command;
    tcp_server_.Receive((*client)->connection.value(), command);
    logger.Log("Command received: " + std::to_string(command), Info);
    (this->*method_ptr[command])((*client)->connection.value());
  } catch (TCP::TcpException& tcp_exception) {
    if (tcp_exception.GetType() == TCP::TcpException::ConnectionBreak) {
      (*client)->connection = {};
      logger.Log("Connection broke", Warning);
    } else {
      logger.Log("Error occurred while receiving command: " +
                     std::string(tcp_exception.what()),
                 Warning);
    }
  }
  logger.Log("Finishing client connection", Info);
  (*client)->is_running = false;
  delete client;
}

/*---------------------------- atomic operations -----------------------------*/
void LauncherServer::Implementation::ALoad(
    TCP::TcpServer::ClientConnection client) {
  LServer l_server(LServer::ALoad, logger_);
  Logger& logger = l_server;
  logger.Log("Entering loading foo", Info);

  logger.Log("Trying to receive config", Debug);
  std::string bin_name;
  ProcessConfig config;
  std::string tmp_args;
  int tmp_time_to_stop;
  bool should_wait;
  tcp_server_.Receive(client, bin_name, tmp_args, config.launch_on_boot,
                      config.term_rerun, config.term_rerun, tmp_time_to_stop,
                      should_wait);
  logger.Log("Config received", Debug);

  config.args = Split(tmp_args, ';');
  if (tmp_time_to_stop != 0) {
    config.time_to_stop = std::chrono::milliseconds(tmp_time_to_stop);
  }

  logger.Log("Running process", Debug);
  bool result = RunProcess(std::move(bin_name), std::move(config), should_wait);
  logger.Log("Process has been run, sending result to client", Debug);
  tcp_server_.Send(client, result);
  logger.Log("Result sent to client, success", Info);
}

void LauncherServer::Implementation::AStop(
    TCP::TcpServer::ClientConnection client) {
  LServer l_server(LServer::AStop, logger_);
  Logger& logger = l_server;
  logger.Log("Entering stop foo", Info);

  logger.Log("Receiving process config", Debug);
  std::string bin_name;
  bool should_wait;
  tcp_server_.Receive(client, bin_name, should_wait);

  logger.Log("Config received. Terminating process", Debug);
  bool result = StopProcess(bin_name, should_wait);

  logger.Log("Process has been terminated, sending result to client", Debug);
  tcp_server_.Send(client, result);
  logger.Log("Result sent to client, success", Info);
}

void LauncherServer::Implementation::ARerun(
    TCP::TcpServer::ClientConnection client) {
  LServer l_server(LServer::ARerun, logger_);
  Logger& logger = l_server;
  logger.Log("Entering Rerun foo", Info);

  logger.Log("Receiving process config", Debug);
  std::string bin_name;
  bool should_wait;
  tcp_server_.Receive(client, bin_name, should_wait);

  logger.Log("Config received. Terminating process. Locking mutex", Debug);
  pr_main_m_.lock();
  logger.Log("Mutex locked", Debug);
  if (!processes_.contains(bin_name)) {
    logger.Log(
        "Main table does not contain process. Unlocking mutex. Sending result "
        "to client",
        Debug);
    pr_main_m_.unlock();

    tcp_server_.Send(client, false);
    logger.Log("Result sent to client. Exit", Info);
    return;
  }
  logger.Log(
      "Main table contains process. Terminating process. Unlocking mutex",
      Debug);
  ProcessConfig config = processes_[bin_name].config;
  pr_main_m_.unlock();

  StopProcess(bin_name, true);
  logger.Log("Process terminated. Running process", Debug);
  bool result = RunProcess(std::move(bin_name), std::move(config), should_wait);
  logger.Log("Process has been run. Sending result to client", Debug);
  tcp_server_.Send(client, result);
  logger.Log("Result sent to client, success", Info);
}

void LauncherServer::Implementation::AIsRunning(
    TCP::TcpServer::ClientConnection client) {
  LServer l_server(LServer::AIsRunning, logger_);
  Logger& logger = l_server;
  logger.Log("Entering IsRunning foo", Info);

  logger.Log("Receiving process name", Debug);
  std::string bin_name;
  tcp_server_.Receive(client, bin_name);
  logger.Log("Process name received. Getting PID", Debug);

  bool result = GetPid(bin_name).has_value();
  logger.Log("Status (PID) is got. Sending to client", Debug);
  tcp_server_.Send(client, result);
  logger.Log("Result sent to client, success", Info);
}

void LauncherServer::Implementation::AGetPid(
    TCP::TcpServer::ClientConnection client) {
  LServer l_server(LServer::AGetPid, logger_);
  Logger& logger = l_server;
  logger.Log("Entering GetPid foo", Info);

  logger.Log("Receiving process name", Debug);
  std::string bin_name;
  tcp_server_.Receive(client, bin_name);
  logger.Log("Process name received. Getting PID", Debug);

  auto result = GetPid(bin_name);
  logger.Log("PID is got. Sending to client", Debug);
  tcp_server_.Send(client, result.has_value() ? result.value() : 0);
  logger.Log("Result sent to client, success", Info);
}

}  // namespace LNCR