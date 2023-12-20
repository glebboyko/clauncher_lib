#include "clauncher-client.hpp"

#include <list>

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

LauncherClient::LauncherClient(int port, LNCR::logging_foo logger)
    : port_(port),
      logger_(logger),
      tcp_client_(TCP::TcpClient(0, port, "127.0.0.0", logger_)) {
  tcp_client_.value().Send(SenderStatus::Client);
}

bool LauncherClient::LoadProcess(const std::string& bin_name,
                                 const LNCR::ProcessConfig& process_config,
                                 bool wait_for_run) {
  CheckTcpClient();

  try {
    tcp_client_.value().Send(Command::Load);
    tcp_client_.value().Send(bin_name, Unite(process_config.args, ';'),
                             process_config.launch_on_boot,
                             process_config.term_rerun,
                             process_config.time_to_stop.has_value()
                                 ? process_config.time_to_stop.value().count()
                                 : 0,
                             wait_for_run);
    bool result;
    tcp_client_->Receive(result);
    return result;
  } catch (TCP::TcpException& exception) {
    if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
      tcp_client_ = {};
    }
    throw exception;
  }
}

bool LauncherClient::StopProcess(const std::string& bin_name,
                                 bool wait_for_stop) {
  CheckTcpClient();

  try {
    tcp_client_->Send(Command::Stop);
    tcp_client_->Send(bin_name, wait_for_stop);

    bool result;
    tcp_client_->Receive(result);
    return result;
  } catch (TCP::TcpException& exception) {
    if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
      tcp_client_ = {};
    }
    throw exception;
  }
}

bool LauncherClient::ReRunProcess(const std::string& bin_name,
                                  bool wait_for_rerun) {
  CheckTcpClient();

  try {
    tcp_client_->Send(Command::Rerun);
    tcp_client_->Send(bin_name, wait_for_rerun);

    bool result;
    tcp_client_->Receive(result);
    return result;
  } catch (TCP::TcpException& exception) {
    if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
      tcp_client_ = {};
    }
    throw exception;
  }
}

bool LauncherClient::IsProcessRunning(const std::string& bin_name) {
  CheckTcpClient();

  try {
    tcp_client_->Send(Command::IsRunning);
    tcp_client_->Send(bin_name);

    bool result;
    tcp_client_->Receive(result);
    return result;
  } catch (TCP::TcpException& exception) {
    if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
      tcp_client_ = {};
    }
    throw exception;
  }
}

std::optional<int> LauncherClient::GetProcessPid(const std::string& bin_name) {
  CheckTcpClient();

  try {
    tcp_client_->Send(Command::GetPid);
    tcp_client_->Send(bin_name);

    int result;
    tcp_client_->Receive(result);
    if (result == 0) {
      return {};
    }
    return result;
  } catch (TCP::TcpException& exception) {
    if (exception.GetType() == TCP::TcpException::ConnectionBreak) {
      tcp_client_ = {};
    }
    throw exception;
  }
}
void LauncherClient::CheckTcpClient() {
  if (!tcp_client_.has_value()) {
    try {
      tcp_client_ = TCP::TcpClient(0, port_, "127.0.0.1", logger_);
    } catch (TCP::TcpException& tcp_exception) {
      throw tcp_exception;
    }
  }
}

}  // namespace LNCR