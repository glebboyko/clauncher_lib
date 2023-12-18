#include <unistd.h>

#include "tcp-client.hpp"
#include "clauncher-supply.hpp"

int main(int argc, char** argv) {
  if (argc <= 4) {
    exit(1);
  }

  int port;
  sscanf(argv[1], "%d", &port);

  try {
    TCP::TcpClient tcp_client(0, port, "127.0.0.1");
    tcp_client.Send(CLGR::Agent);
    tcp_client.Send(argv[2], getpid());
  } catch (...) {
    exit(2);
  }

  execv(argv[3], &(argv[4]));

  try {
    TCP::TcpClient tcp_client(0, port, "127.0.0.1");
    tcp_client.Send(CLGR::Agent);
    tcp_client.Send(argv[2], errno);
  } catch (...) {
    exit(3);
  }
}