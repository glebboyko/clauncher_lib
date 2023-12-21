#include <signal.h>
#include <unistd.h>

#include "clauncher-server.hpp"

namespace LNCR {

LauncherServer* server;
logging_foo global_logger;

void TermHandler(int signal) {
  global_logger("LAUNCHER SERVER RUNNER", "SIGNAL HANDLER",
                "Terminating with signal " + std::to_string(signal),
                MessagePriority::Info);
  delete server;
  exit(0);
}

void SigTermSetup(int signal, void (*handler)(int)) {
  struct sigaction struct_sigaction;
  struct_sigaction.sa_handler = handler;
  sigemptyset(&struct_sigaction.sa_mask);
  struct_sigaction.sa_flags = 0;

  sigaction(signal, &struct_sigaction, NULL);
}

void LauncherRunner(int port, const std::string& config_file,
                    const std::string& agent_binary,
                    logging_foo logger) noexcept {
  global_logger = logger;

  SigTermSetup(SIGTERM, TermHandler);

  try {
    server = new LauncherServer(port, config_file, agent_binary, global_logger);
  } catch (...) {
    return;
  }

  while (true) {
    sleep(100);
  }
}

}  // namespace LNCR