#pragma once

#include <memory>
#include <string>

#include "clauncher-supply.hpp"

namespace LNCR {

class LauncherServer {
 public:
  // constructor / destructor //
  LauncherServer(int port, const std::string& config_file,
                 const std::string& agent_binary, logging_foo = LoggerCap);
  ~LauncherServer();

 private:
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};

void LauncherRunner(int port, const std::string& config_file,
                    const std::string& agent_binary,
                    logging_foo = LoggerCap) noexcept;

}  // namespace LNCR