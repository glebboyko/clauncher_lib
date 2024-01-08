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
      logger.Log("Process has already sent config. Moving to main table", Info);
      processes_.insert({bin_name, runner.info});

      ProcessChangeSend(true, runner.run_semaphore, runner.run_status, logger);

      logger.Log("Process successfully run. Erasing from Run table", Info);
      iter = processes_to_run_.erase(iter);
      continue;
    }

    if (runner.last_run.has_value()) {  // process run but has not sent config
      logger.Log("Process is running but has not sent config", Info);
      if (std::chrono::system_clock::now() - runner.last_run.value() >=
          kWaitToRerun) {      // is timeout
        runner.last_run = {};  // setting rerun flag
        logger.Log("Launching timeout. Rerunning", Info);
      } else {
        logger.Log("Launching not timeout", Debug);
      }
    }

    if (is_active_ && runner.info.pid == 0 &&
        !runner.last_run.has_value()) {  // run flag set
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
  pr_to_run_m_.unlock();
  pr_to_term_m_.lock();
  logger.Log("Mutex locked. Entering loop", Debug);

  for (auto iter = processes_to_terminate_.begin();
       iter != processes_to_terminate_.end();) {
    auto& [bin_name, deleter] = *iter;
    logger.Log("Processing " + bin_name, Info);

    if (processes_.contains(bin_name)) {  // Main table contains process
      logger.Log("Main table contains process", Info);
      auto main_iter = processes_.find(bin_name);
      if (!IsPidAvailable(
              processes_[bin_name].pid)) {  // Process is not running
        logger.Log("Process has already terminated. Erasing from main talbe",
                   Info);
        processes_.erase(main_iter);
        ProcessChangeSend(SigTerm, deleter.term_semaphore, deleter.term_status,
                          logger);
        logger.Log("Erasing from Term table", Debug);
        iter = processes_to_terminate_.erase(iter);
        continue;
      }
      // Process is running
      logger.Log("Process is running", Info);

      if (!deleter.term_sent.has_value()) {  // SigTerm has not been sent yet
        logger.Log("SigTerm signal has not been sent yet. Sending SIGTERM",
                   Info);
        kill(main_iter->second.pid, SIGTERM);
        if (!main_iter->second.config.time_to_stop
                 .has_value()) {  // no checking required
          logger.Log(
              "Checking termination is not required. Erasing from Main talbe",
              Info);
          processes_.erase(main_iter);
          ProcessChangeSend(NoCheck, deleter.term_semaphore,
                            deleter.term_status, logger);
          logger.Log("Erasing from Term table", Debug);
          iter = processes_to_terminate_.erase(iter);
          continue;
        }
        logger.Log("Termination checker is required. Setting timer", Info);
        deleter.term_sent = std::chrono::system_clock::now();
      } else if (std::chrono::system_clock::now() - deleter.term_sent.value() >
                 main_iter->second.config.time_to_stop.value()) {  // timeout
        logger.Log(
            "SigTerm signal has already been sent. Timer timeout. Sending "
            "SIGKILL. Erasing from Main table",
            Info);
        kill(main_iter->second.pid, SIGKILL);
        processes_.erase(main_iter);
        ProcessChangeSend(SigKill, deleter.term_semaphore, deleter.term_status,
                          logger);
        logger.Log("Erasing from Term table", Debug);
        iter = processes_to_terminate_.erase(iter);
        continue;
      }
      // not timeout
      logger.Log("Timer is not timeout. Moving to next process", Info);
    } else if (processes_to_run_.contains(
                   bin_name)) {  // Run table contains process
      logger.Log("Run table contains process", Info);
      auto run_iter = processes_to_run_.find(bin_name);
      if (run_iter->second.info.pid == 0) {  // Process has no PID
        logger.Log("Process has no PID. Erasing from Run table", Info);
        processes_to_run_.erase(run_iter);
        ProcessChangeSend(NotRun, deleter.term_semaphore, deleter.term_status,
                          logger);
        logger.Log("Erasing process from Term table", Debug);
        iter = processes_to_terminate_.erase(iter);
        continue;
      }
      logger.Log("Process has got PID. Moving to next process", Info);
    } else {  // No table contains process
      logger.Log("Not table contains process", Info);
      ProcessChangeSend(NotRunning, deleter.term_semaphore, deleter.term_status,
                        logger);
      logger.Log("Erasing process from Term table", Debug);
      iter = processes_to_terminate_.erase(iter);
      continue;
    }
    ++iter;
  }

  logger.Log("Unlocking mutex", Debug);
  pr_to_term_m_.unlock();
  pr_to_run_m_.unlock();
  pr_main_m_.unlock();
  logger.Log("Function finish", Info);
}

void LauncherServer::Implementation::ProcessChangeSend(
    int status, std::binary_semaphore*& semaphore, int*& status_ptr,
    LNCR::Logger& logger) noexcept {
  if (semaphore == nullptr) {
    logger.Log("Nothing is waiting for result", Debug);
    return;
  }

  logger.Log("Something is waiting for result, sending result", Info);
  *status_ptr = status;
  semaphore->release();

  status_ptr = nullptr;
  semaphore = nullptr;

  logger.Log("Result sent", Debug);
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
          logger.Log("Process erasing from main table", Info);
          iter = processes_.erase(iter);

          pr_main_m_.unlock();
          logger.Log("Mutex unlocked", Debug);

          RunProcess(std::move(bin_name), std::move(config));

          logger.Log("Locking mutex", Debug);
          pr_main_m_.lock();
          logger.Log("Mutex locked", Debug);

        } else {
          logger.Log("Process's rerun flag is set to false", Debug);
          logger.Log("Process erasing from main table", Info);
          iter = processes_.erase(iter);
        }
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

  std::binary_semaphore* semaphore = nullptr;
  int* run_status = nullptr;
  if (wait_for_run) {
    semaphore = new std::binary_semaphore(0);
    run_status = new int;
  }
  Runner runner = {.info = {.config = std::move(process)},
                   .run_status = run_status,
                   .run_semaphore = semaphore};
  logger.Log("Inserting process into run table", Debug);
  auto inserted =
      processes_to_run_.insert({std::move(bin_name), std::move(runner)});

  pr_to_run_m_.unlock();
  logger.Log("Mutex run unlocked", Debug);

  if (!inserted.second) {
    if (wait_for_run) {
      delete semaphore;
      delete run_status;
    }
    logger.Log("Run table has already contained process. Term exec", Info);
    return false;
  } else {
    logger.Log("Process inserted to run table", Debug);
  }

  bool result = true;
  if (wait_for_run) {
    logger.Log("Runner is waiting for running", Info);
    semaphore->acquire();
    result = *run_status;
    delete semaphore;
    delete run_status;
    logger.Log("Process is running", Info);
  } else {
    logger.Log("Runner is not waiting for running", Info);
  }
  return result;
}
TermStatus LauncherServer::Implementation::StopProcess(
    const std::string& bin_name, bool wait_for_term) noexcept {
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

  std::binary_semaphore* semaphore = nullptr;
  int* term_status = nullptr;
  if (wait_for_term) {
    semaphore = new std::binary_semaphore(0);
    term_status = new int;
  }
  Stopper stopper = {.term_status = term_status, .term_semaphore = semaphore};

  logger.Log("Locking mutex", Debug);
  pr_to_term_m_.lock();
  logger.Log("Mutex locked", Debug);

  auto iter =
      processes_to_terminate_.insert({std::move(bin_name), std::move(stopper)});

  pr_to_term_m_.unlock();
  logger.Log("Mutex unlocked", Debug);

  if (!iter.second) {
    if (wait_for_term) {
      delete semaphore;
      delete term_status;
    }
    logger.Log("Term table has already contained process. Term exec", Info);
    return AlreadyTerminating;
  } else {
    logger.Log("Process inserted to term table", Debug);
  }

  TermStatus result = NoCheck;

  if (wait_for_term) {
    logger.Log("Terminator is waiting for terminating", Info);
    semaphore->acquire();
    result = static_cast<TermStatus>(*term_status);

    delete semaphore;
    delete term_status;
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
    pr_main_m_.unlock();
    logger.Log("Main table does not contain process, unlocking mutex", Debug);
    return {};
  }

  int pid = processes_[bin_name].pid;
  pr_main_m_.unlock();
  logger.Log("Main table contains process. PID: " + std::to_string(pid) +
                 ". Unlocked mutex",
             Debug);

  return pid;
}
bool LauncherServer::Implementation::IsRunning(
    const std::string& bin_name) noexcept {
  LServer l_server(LServer::IsRunning, logger_);
  Logger& logger = l_server;
  logger.Log("Process: " + bin_name, Debug);

  logger.Log("Locking main and run mutexes", Debug);
  pr_main_m_.lock();
  pr_to_run_m_.lock();
  logger.Log("Locked main and run mutexes. Getting result", Debug);
  bool is_running =
      processes_.contains(bin_name) || processes_to_run_.contains(bin_name);

  logger.Log("Result got: " + std::to_string(is_running), Debug);
  pr_to_run_m_.unlock();
  pr_main_m_.unlock();
  logger.Log("Unlocked run and main mutexes", Debug);

  return is_running;
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
  try {
    implementation_->accepter_ =
        std::thread(&Implementation::Accepter, implementation_.get());
  } catch (std::system_error& error) {
    logger.Log("Cannot create accepter thread", Error);
    throw error;
  }

  try {
    implementation_->receiver_ =
        std::thread(&Implementation::Receiver, implementation_.get());
  } catch (std::system_error& error) {
    logger.Log("Cannot create receiver thread", Error);
    throw error;
  }
  try {
    implementation_->process_ctrl_ =
        std::thread(&Implementation::ProcessCtrl, implementation_.get());
  } catch (std::system_error& error) {
    logger.Log("Cannot create process control thread", Error);
    throw error;
  }

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

  logger.Log("Deleting existing semaphores", Debug);
  for (auto& [bin_name, runner] : implementation_->processes_to_run_) {
    implementation_->ProcessChangeSend(false, runner.run_semaphore,
                                       runner.run_status, logger);
  }
  for (auto& [bin_name, stopper] : implementation_->processes_to_terminate_) {
    implementation_->ProcessChangeSend(TermError, stopper.term_semaphore,
                                       stopper.term_status, logger);
  }
  logger.Log("All semaphores deleted", Debug);

  logger.Log("Terminating clients", Debug);
  for (auto& [connection, curr_communication, is_running] :
       implementation_->clients_) {
    if (connection.has_value()) {
      logger.Log("Client is running, closing connection", Debug);
      try {
        connection.value().CloseConnection();
      } catch (std::exception& exception) {
        logger.Log("Multithreading tcp connection error", Warning);
      }
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

  int table_size;
  config >> table_size;
  logger.Log("Table size got: " + std::to_string(table_size), Debug);

  logger.Log("Getting configs from file", Debug);
  for (int i = 0; i < table_size; ++i) {
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
  }
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

  logger.Log("Saving table size: " + std::to_string(load_config_.size()),
             Debug);
  config << load_config_.size() << "\n";

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
    TCP::TcpClient* connection;
    try {
      logger.Log("Trying to accept connection", Info);
      connection = new TCP::TcpClient(tcp_server_.AcceptConnection());
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
        connection->Receive(send_from);
        logger.Log("Status received: " +
                       std::string(send_from == SenderStatus::Client ? "Client"
                                                                     : "Agent"),
                   Info);
        if (send_from == SenderStatus::Client) {
          logger.Log("Locking client mutex", Debug);
          clients_m_.lock();
          logger.Log("Client mutex locked", Debug);
          clients_.push_back({.connection = std::move(*connection)});
          clients_m_.unlock();
          logger.Log("Client mutex unlocked", Debug);

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
          connection->Receive(process_name, pid, error);
          logger.Log("Config received", Debug);

          // block tables to use
          logger.Log("Locking Run mutex", Debug);
          pr_to_run_m_.lock();
          logger.Log("Mutex Run locked", Debug);

          // check where is it contained
          bool to_run = processes_to_run_.contains(process_name);

          if (!to_run && error == 0) {  // if nowhere
            logger.Log(
                "Run table does not contain process, process is in init "
                "mode. "
                "Sending kill signal",
                Warning);
            connection->Send(false);
          } else if (error == 0) {  // if it is init mode
            logger.Log("Run table contains process. Process is in init mode",
                       Info);
            connection->Send(true);

            auto& process = processes_to_run_[process_name];
            process.info.pid = pid;  // set pid : "successful run" flag
            ProcessChangeSend(true, process.run_semaphore, process.run_status,
                              logger);
          } else {  // error mode
            // do nothing because it will be processed by ctrl
            logger.Log("Process is in error mode: " + std::to_string(error),
                       Warning);
          }

          // unlock mutexes
          pr_to_run_m_.unlock();
          logger.Log("Unlocked run mutex", Debug);
        }
      } catch (TCP::TcpException& tcp_exception) {
        if (tcp_exception.GetType() != TCP::TcpException::ConnectionBreak) {
          connection->CloseConnection();
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
      connection->CloseConnection();
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

    logger.Log("Locking client mutex", Debug);
    clients_m_.lock();
    logger.Log("Client mutex locked", Debug);

    for (auto iter = clients_.begin(); is_active_ && iter != clients_.end();) {
      logger.Log("Processing client", Debug);
      // terminating communication
      if (iter->is_running) {
        logger.Log("Client thread is running, skip", Debug);
        ++iter;
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
        iter = clients_.erase(iter);
        continue;
      }
      logger.Log("Client is connected", Debug);

      try {
        logger.Log("Checking client message availability", Debug);
        if (iter->connection->IsAvailable()) {
          logger.Log("Client message is available. Running thread", Info);
          iter->is_running = true;
          iter->curr_communication =
              std::thread(&LauncherServer::Implementation::ClientCommunication,
                          this, new decltype(iter)(iter));
          logger.Log("Thread is running", Debug);
        }
      } catch (TCP::TcpException& tcp_exception) {
        if (tcp_exception.GetType() == TCP::TcpException::ConnectionBreak) {
          logger.Log("Client connection broke, erasing", Warning);
          iter = clients_.erase(iter);
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
    clients_m_.unlock();
    logger.Log("Processed all connections. Unlocked client mutex. Sleeping",
               Info);
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
    logger.Log("Terminating table processing", Info);
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
    (*client)->connection->Receive(command);
    logger.Log("Command received: " + std::to_string(command), Info);
    (this->*method_ptr[command])((*client)->connection.value());
  } catch (TCP::TcpException& tcp_exception) {
    if (tcp_exception.GetType() == TCP::TcpException::ConnectionBreak) {
      (*client)->connection.reset();
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
void LauncherServer::Implementation::ALoad(TCP::TcpClient& client) {
  LServer l_server(LServer::ALoad, logger_);
  Logger& logger = l_server;
  logger.Log("Entering loading foo", Info);

  logger.Log("Trying to receive config", Debug);
  std::string bin_name;
  ProcessConfig config;
  int num_of_args;
  int tmp_time_to_stop;
  bool should_wait;
  client.Receive(bin_name, num_of_args, config.launch_on_boot,
                 config.term_rerun, tmp_time_to_stop, should_wait);
  for (int i = 0; i < num_of_args; ++i) {
    std::string arg;
    client.Receive(arg);
    config.args.push_back(arg);
  }
  logger.Log("Config received", Debug);

  if (tmp_time_to_stop != 0) {
    config.time_to_stop = std::chrono::milliseconds(tmp_time_to_stop);
  }

  logger.Log("Running process", Debug);
  bool result = RunProcess(std::move(bin_name), std::move(config), should_wait);
  logger.Log("Process has been run, sending result to client", Debug);
  client.Send(result);
  logger.Log("Result sent to client: " + std::to_string(result), Info);
}

void LauncherServer::Implementation::AStop(TCP::TcpClient& client) {
  LServer l_server(LServer::AStop, logger_);
  Logger& logger = l_server;
  logger.Log("Entering stop foo", Info);

  logger.Log("Receiving process config", Debug);
  std::string bin_name;
  bool should_wait;
  client.Receive(bin_name, should_wait);

  logger.Log("Config received. Terminating process", Debug);
  int result = StopProcess(bin_name, should_wait);

  logger.Log("Process has been terminated, sending result to client", Debug);
  client.Send(result);
  logger.Log("Result sent to client: " + std::to_string(result), Info);
}

void LauncherServer::Implementation::ARerun(TCP::TcpClient& client) {
  LServer l_server(LServer::ARerun, logger_);
  Logger& logger = l_server;
  logger.Log("Entering Rerun foo", Info);

  logger.Log("Receiving process config", Debug);
  std::string bin_name;
  bool should_wait;
  client.Receive(bin_name, should_wait);

  logger.Log("Config received. Terminating process. Locking mutex", Debug);
  pr_main_m_.lock();
  logger.Log("Mutex locked", Debug);
  if (!processes_.contains(bin_name)) {
    pr_main_m_.unlock();
    logger.Log(
        "Main table does not contain process. Unlocked mutex. Sending result "
        "to client",
        Debug);

    client.Send(false);
    logger.Log("Result sent to client: false. Exit", Info);
    return;
  }
  ProcessConfig config = processes_[bin_name].config;
  pr_main_m_.unlock();
  logger.Log(
      "Main table contains process. Got config. Unlocked mutex. Terminating",
      Debug);

  StopProcess(bin_name, true);
  logger.Log("Process terminated. Running process", Debug);
  bool result = RunProcess(std::move(bin_name), std::move(config), should_wait);
  logger.Log("Process has been run. Sending result to client", Debug);
  client.Send(result);
  logger.Log("Result sent to client: " + std::to_string(result), Info);
}

void LauncherServer::Implementation::AIsRunning(TCP::TcpClient& client) {
  LServer l_server(LServer::AIsRunning, logger_);
  Logger& logger = l_server;
  logger.Log("Entering IsRunning foo", Info);

  logger.Log("Receiving process name", Debug);
  std::string bin_name;
  client.Receive(bin_name);
  logger.Log("Process name received. Getting PID", Debug);

  bool result = IsRunning(bin_name);
  logger.Log("Status is got. Sending to client", Debug);
  client.Send(result);
  logger.Log("Result sent to client, success", Info);
}

void LauncherServer::Implementation::AGetPid(TCP::TcpClient& client) {
  LServer l_server(LServer::AGetPid, logger_);
  Logger& logger = l_server;
  logger.Log("Entering GetPid foo", Info);

  logger.Log("Receiving process name", Debug);
  std::string bin_name;
  client.Receive(bin_name);
  logger.Log("Process name received. Getting PID", Debug);

  auto result = GetPid(bin_name);
  logger.Log("PID is got. Sending to client", Debug);
  client.Send(result.has_value() ? result.value() : 0);
  logger.Log("Result sent to client, success", Info);
}

}  // namespace LNCR