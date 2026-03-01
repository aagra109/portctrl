#include "cli_constants.h"
#include "exit_codes.h"
#include "free_command.h"
#include "list_command.h"
#include "usage.h"
#include "who_command.h"

#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    usage();
    return toExitCode(ExitCode::kUsage);
  }

  std::string cmd = argv[1];
  if (cmd == kCommandWho) {
    return runWhoCommand(argc, argv);
  }

  if (cmd == kCommandFree) {
    return runFreeCommand(argc, argv);
  }

  if (cmd == kCommandList) {
    return runListCommand(argc, argv);
  }

  usage();
  return toExitCode(ExitCode::kUsage);
}
