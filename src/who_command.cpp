#include "who_command.h"
#include "port_inspection.h"
#include "types.h"
#include "usage.h"

#include <iostream>
#include <optional>
#include <string>

int runWhoCommand(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Missing port.\n";
    usage();
    return 1;
  }
  std::string portText = argv[2];
  auto port = parsePort(portText);
  if (!port.has_value()) {
    std::cerr << "Invalid port: " << portText << "\n";
    return 1;
  }

  CommandResult result = runListenerInspectCommand(*port);

  if (result.exitCode == 1 && result.output.empty()) {
    std::cout << "Port " << *port << " appears free (no LISTEN process found).\n";
    return 0;
  }

  if (result.exitCode == 1 && !result.output.empty()) {
    std::cerr << "Failed to inspect port " << *port << ".\n";
    std::cerr << result.output;
    return 1;
  }

  if (result.exitCode != 0 && result.exitCode != 1) {
    std::cerr << "Failed to inspect port " << *port << ".\n";
    if (!result.output.empty())
      std::cerr << result.output;
    return 1;
  }

  if (result.output.empty()) {
    std::cout << "Port " << *port << " appears free (no LISTEN process found).\n";
    return 0;
  }

  ListenerInfo info;
  if (!parseFirstListener(result.output, info)) {
    std::cout << "Port " << *port << " is in use, but failed to parse listener details.\n";
    std::cout << "Raw lsof fields:\n" << result.output;
    return 0;
  }

  std::cout << "Port " << *port << " is in use.\n";
  std::cout << "Process: " << info.command << "\n";
  std::cout << "PID: " << info.pid << "\n";
  std::cout << "User: " << info.user << "\n";
  std::cout << "Endpoint: " << info.endpoint << "\n";
  return 0;
}