#include <unistd.h>
#include <string>

#include "clauncher-supply.hpp"
#include "tcp-client.hpp"

int main(const int argc, char** argv) {
  if (argc < 3) {
    return 1;
  }

  int port;
  sscanf(argv[1], "%d", &port);

  try {
    TCP::TcpClient tcp_client("127.0.0.1", port);
    tcp_client.Send(static_cast<int>(LNCR::Agent));
    tcp_client.Send(std::string(argv[2]), getpid(), 0);

    bool should_run;
    if (!tcp_client.Receive(tcp_client.GetMsPingThreshold(), should_run) ||
        !should_run) {
      return 0;
    }
  } catch (...) {
    return 2;
  }

  char* args[argc - 1];

  args[0] = argv[2];
  for (int i = 1; i < argc - 2; ++i) {
    args[i] = argv[i + 2];
  }
  args[argc - 2] = NULL;

  execv(args[0], args);

  try {
    TCP::TcpClient tcp_client("127.0.0.1", 0);
    tcp_client.Send(static_cast<int>(LNCR::Agent));
    tcp_client.Send(std::string(argv[2]), getpid(), errno);
  } catch (...) {
    return 3;
  }
}