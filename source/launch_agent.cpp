#include <unistd.h>

#include "tcp-client.hpp"
#include "clauncher-supply.hpp"

int main(int argc, char** argv) {
  if (argc <= 3) {
    exit(1);
  }

  int port;
  sscanf(argv[1], "%d", &port);

  try {
    TCP::TcpClient tcp_client(0, port, "127.0.0.1");
    tcp_client.Send(LNCR::Agent);
    tcp_client.Send(argv[2], getpid(), 0);
  } catch (...) {
    exit(2);
  }

  execv(argv[2], &(argv[3]));

  try {
    TCP::TcpClient tcp_client(0, port, "127.0.0.1");
    tcp_client.Send(LNCR::Agent);
    tcp_client.Send(argv[2], getpid(), errno);
  } catch (...) {
    exit(3);
  }
}