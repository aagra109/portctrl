#include "who_command.h"
#include "port_inspection.h"
#include "types.h"
#include "usage.h"

#include <iostream>
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

  InspectResult inspect = inspectPort(*port);

  if (inspect.status == InspectStatus::kError) {
    std::cerr << inspect.error << "\n";
    return 1;
  }

  if (inspect.status == InspectStatus::kFree || inspect.listeners.empty()) {
    std::cout << "Port " << *port << " appears free (no LISTEN process found).\n";
    return 0;
  }

  std::cout << "Port " << *port << " is in use.\n";
  std::cout << "Listening endpoints: " << inspect.listeners.size() << "\n";
  for (const auto &listener : inspect.listeners) {
    std::cout << "PID: " << listener.pid << " | User: " << listener.user
              << " | Process: " << listener.command << " | Endpoint: " << listener.endpoint
              << "\n";
  }
  return 0;
}
