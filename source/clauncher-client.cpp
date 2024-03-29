#include "clauncher-client.hpp"

#include <list>

#include "clauncher-client-impl.hpp"

namespace LNCR {

std::string Unite(const std::list<std::string>& list, char delimiter) {
  std::string result;
  for (const auto& member : list) {
    result += member + delimiter;
  }
  if (!result.empty()) {
    result.pop_back();
  }
  return result;
}

LauncherClient::LauncherClient(int port, LNCR::logging_foo logging_f) {
  LClient l_client(LClient::Constructor, logging_f);
  Logger& logger = l_client;
  logger.Log("Creating client", Info);

  logger.Log("Trying to create tcp-client", Debug);
  implementation_ = std::unique_ptr<Implementation>(new Implementation{
      .port_ = port,
      .logger_ = logging_f,
      .tcp_client_ = new TCP::TcpClient("127.0.0.1", port, logging_f)});
  logger.Log("Tcp-client created", Debug);

  logger.Log("Trying to send configuration to server", Debug);
  implementation_->tcp_client_->Send(static_cast<int>(SenderStatus::Client));
  logger.Log("Configuration successfully sent. Client created", Debug);

  logger.Log("Client created", Info);
}

LauncherClient::~LauncherClient() {}

bool LauncherClient::LoadProcess(const std::string& bin_name,
                                 const LNCR::ProcessConfig& process_config,
                                 bool wait_for_run) {
  LClient l_client(LClient::LoadProcess, implementation_->logger_);
  Logger& logger = l_client;

  logger.Log("Trying to load process: " + bin_name, Info);
  logger.Log("Checking tcp-connection", Debug);
  implementation_->CheckTcpClient();

  try {
    logger.Log("Trying to send command to server", Debug);
    implementation_->tcp_client_->Send(static_cast<int>(Command::Load));
    implementation_->tcp_client_->Send(
        bin_name, process_config.args.size(), process_config.launch_on_boot,
        process_config.term_rerun,
        process_config.time_to_stop.has_value()
            ? process_config.time_to_stop.value().count()
            : 0,
        wait_for_run);
    for (const auto& arg : process_config.args) {
      implementation_->tcp_client_->Send(arg);
    }
    logger.Log("Command send to server", Debug);

    logger.Log("Trying to receive answer from server", Debug);
    bool result;
    while (!implementation_->tcp_client_->Receive(
        implementation_->tcp_client_->GetMsPingThreshold(), result)) {
    }
    logger.Log("Answer from server received: " + std::to_string(result), Info);
    return result;
  } catch (TCP::TcpException& exception) {
    logger.Log(std::string("Caught exception: ") + exception.what(), Warning);
    if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
      delete implementation_->tcp_client_;
      implementation_->tcp_client_ = nullptr;
    }
    throw exception;
  }
}

TermStatus LauncherClient::StopProcess(const std::string& bin_name,
                                       bool wait_for_stop) {
  LClient l_client(LClient::StopProcess, implementation_->logger_);
  Logger& logger = l_client;
  logger.Log("Trying to stop process: " + bin_name, Info);

  logger.Log("Checking tcp-connection", Debug);
  implementation_->CheckTcpClient();

  try {
    logger.Log("Trying to send command to server", Debug);
    implementation_->tcp_client_->Send(static_cast<int>(Command::Stop));
    implementation_->tcp_client_->Send(bin_name, wait_for_stop);
    logger.Log("Command sent to server", Debug);

    logger.Log("Trying to receive answer from server", Debug);
    int result;
    while (!implementation_->tcp_client_->Receive(
        implementation_->tcp_client_->GetMsPingThreshold(), result)) {
    }
    logger.Log("Answer from server received: " + std::to_string(result), Info);
    return static_cast<TermStatus>(result);
  } catch (TCP::TcpException& exception) {
    logger.Log(std::string("Caught exception: ") + exception.what(), Warning);
    if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
      delete implementation_->tcp_client_;
      implementation_->tcp_client_ = nullptr;
    }
    throw exception;
  }
}

bool LauncherClient::ReRunProcess(const std::string& bin_name,
                                  bool wait_for_rerun) {
  LClient l_client(LClient::ReRunProcess, implementation_->logger_);
  Logger& logger = l_client;
  logger.Log("Trying to rerun process: " + bin_name, Info);

  logger.Log("Checking tcp-connection", Debug);
  implementation_->CheckTcpClient();

  try {
    logger.Log("Trying to send command to server", Debug);
    implementation_->tcp_client_->Send(static_cast<int>(Command::Rerun));
    implementation_->tcp_client_->Send(bin_name, wait_for_rerun);
    logger.Log("Command send to server", Debug);

    logger.Log("Trying to receive answer from server", Debug);
    bool result;
    while (!implementation_->tcp_client_->Receive(
        implementation_->tcp_client_->GetMsPingThreshold(), result)) {
    }
    logger.Log("Answer from server received: " + std::to_string(result), Info);
    return result;
  } catch (TCP::TcpException& exception) {
    logger.Log(std::string("Caught exception: ") + exception.what(), Warning);
    if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
      delete implementation_->tcp_client_;
      implementation_->tcp_client_ = nullptr;
    }
    throw exception;
  }
}

bool LauncherClient::IsProcessRunning(const std::string& bin_name) {
  LClient l_client(LClient::IsProcessRunning, implementation_->logger_);
  Logger& logger = l_client;
  logger.Log("Trying to check if process is running: " + bin_name, Info);

  logger.Log("Checking tcp-connection", Debug);
  implementation_->CheckTcpClient();

  try {
    logger.Log("Trying to send command to server", Debug);
    implementation_->tcp_client_->Send(static_cast<int>(Command::IsRunning));
    implementation_->tcp_client_->Send(bin_name);
    logger.Log("Command send to server", Debug);

    logger.Log("Trying to receive answer from server", Debug);
    bool result;
    while (!implementation_->tcp_client_->Receive(
        implementation_->tcp_client_->GetMsPingThreshold(), result)) {
    }
    logger.Log("Answer from server received: " + std::to_string(result), Info);
    return result;
  } catch (TCP::TcpException& exception) {
    logger.Log(std::string("Caught exception: ") + exception.what(), Warning);
    if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
      delete implementation_->tcp_client_;
      implementation_->tcp_client_ = nullptr;
    }
    throw exception;
  }
}

std::optional<int> LauncherClient::GetProcessPid(const std::string& bin_name) {
  LClient l_client(LClient::GetProcessPid, implementation_->logger_);
  Logger& logger = l_client;
  logger.Log("Trying to get process pid: " + bin_name, Info);

  logger.Log("Checking tcp-connection", Debug);
  implementation_->CheckTcpClient();

  try {
    logger.Log("Trying to send command to server", Debug);
    implementation_->tcp_client_->Send(static_cast<int>(Command::GetPid));
    implementation_->tcp_client_->Send(bin_name);
    logger.Log("Command send to server", Debug);

    logger.Log("Trying to receive answer from server", Debug);
    int result;
    while (!implementation_->tcp_client_->Receive(
        implementation_->tcp_client_->GetMsPingThreshold(), result)) {
    }
    logger.Log("Answer from server received: " + std::to_string(result), Info);
    if (result == 0) {
      return {};
    }
    return result;
  } catch (TCP::TcpException& exception) {
    logger.Log(std::string("Caught exception: ") + exception.what(), Warning);
    if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
      delete implementation_->tcp_client_;
      implementation_->tcp_client_ = nullptr;
    }
    throw exception;
  }
}
void LauncherClient::Implementation::CheckTcpClient() {
  LClient l_client(LClient::CheckTcpClient, logger_);
  Logger& logger = l_client;
  logger.Log("Trying to check tcp-connection", Debug);

  if (tcp_client_ == nullptr) {
    try {
      logger.Log("Tcp-connection is not active. Trying to connect", Debug);
      tcp_client_ = new TCP::TcpClient("127.0.0.1", port_, logger_);
    } catch (TCP::TcpException& tcp_exception) {
      logger.Log(std::string("Tcp-connection cannot be established: ") +
                     tcp_exception.what(),
                 Warning);
      throw tcp_exception;
    }
  }
  logger.Log("Tcp-connection is active", Debug);
}

}  // namespace LNCR