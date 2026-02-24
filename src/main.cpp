#include "free_command.h"
#include "list_command.h"
#include "usage.h"
#include "who_command.h"

#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    usage();
    return 1;
  }

  std::string cmd = argv[1];
  if (cmd == "who") {
    return runWhoCommand(argc, argv);
  }

  if (cmd == "free") {
    return runFreeCommand(argc, argv);
  }

  if (cmd == "list") {
    return runListCommand(argc, argv);
  }

  usage();
  return 1;
}
