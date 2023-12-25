#pragma once

#include "clauncher-client.hpp"
#include "clauncher-supply.hpp"
#include "tcp-client.hpp"

namespace LNCR {

struct LauncherClient::Implementation {
  void CheckTcpClient();

  int port_;
  logging_foo logger_;
  TCP::TcpClient* tcp_client_ = nullptr;
};

}  // namespace LNCR