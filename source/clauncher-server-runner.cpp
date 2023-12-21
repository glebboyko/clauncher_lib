#include <signal.h>
#include <unistd.h>

#include "clauncher-server.hpp"

namespace LNCR {

LauncherServer* server;
logging_foo global_logger;

void TermHandler(int signal) {
  LRunner l_runner(LRunner::SigHandler, global_logger);
  Logger& logger = l_runner;

  logger.Log("Got signal " + std::to_string(signal), Info);
  delete server;
  logger.Log("Launcher server deleted. Terminating", Info);
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
                    logging_foo logging_f) noexcept {
  global_logger = logging_f;
  LRunner l_runner(LRunner::Main, global_logger);
  Logger& logger = l_runner;

  logger.Log("Trying to set signal handling", Debug);
  SigTermSetup(SIGTERM, TermHandler);
  logger.Log("Signal handler set", Info);

  try {
    logger.Log("Trying to create server", Info);
    server = new LauncherServer(port, config_file, agent_binary, global_logger);
    logger.Log("Server created", Info);
  } catch (std::exception& exception) {
    logger.Log(std::string("Server creation failed: ") + exception.what(),
               Error);
    return;
  }

  while (true) {
    sleep(100);
  }
}

}  // namespace LNCR