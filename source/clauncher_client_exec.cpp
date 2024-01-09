#include <iostream>
#include <string>

#include "clauncher-client.hpp"

void PrintUsage() {
  std::cout << "USAGE:\n"
               "\tCommands:\n"
               "\t\t- load  >     args    > launch on boot > rerun on term > time to stop > should wait <\n"
               "\t\t- stop  > should wait <\n"
               "\t\t- rerun > should wait <\n"
               "\t\t- check <\n"
               "\t\t- pid   <\n";
}

int main(int argc, char** argv) {
  if (argc < 4) {
    PrintUsage();
    return -2;
  }
  try {
    LNCR::LauncherClient client(std::stoi(argv[1]));

    std::string command = argv[2];
    std::string bin_path = argv[3];

    if (command == "load") {
      if (argc < 8) {
        PrintUsage();
        return -2;
      }

      LNCR::ProcessConfig config;
      int arg_min = 4, arg_max = argc - 4;

      for (int i = arg_min; i < arg_max; ++i) {
        config.args.push_back(argv[i]);
      }

      config.launch_on_boot = std::stoi(argv[arg_max]);
      config.term_rerun = std::stoi(argv[arg_max + 1]);

      int time_to_stop = std::stoi(argv[arg_max + 2]);
      if (time_to_stop != 0) {
        config.time_to_stop.emplace(time_to_stop);
      }

      return client.LoadProcess(bin_path, config, std::stoi(argv[arg_max + 3]));
    }
    if (command == "stop") {
      if (argc != 5) {
        PrintUsage();
        return -2;
      }
      return client.StopProcess(bin_path, std::stoi(argv[4]));
    }
    if (command == "rerun") {
      if (argc != 5) {
        PrintUsage();
        return -2;
      }
      return client.ReRunProcess(bin_path, std::stoi(argv[4]));
    }
    if (command == "check") {
      if (argc != 4) {
        PrintUsage();
        return -2;
      }
      return client.IsProcessRunning(bin_path);
    }
    if (command == "pid") {
      if (argc != 4) {
        PrintUsage();
        return -2;
      }
      auto pid = client.GetProcessPid(bin_path);
      return (pid.has_value() ? pid.value() : 0);
    }
  } catch (std::exception& error) {
    perror(error.what());
    return -1;
  }

  PrintUsage();
  return -3;  // no such command
}