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

  const ListenerInfo &info = inspect.listeners.front();
  std::cout << "Port " << *port << " is in use.\n";
  std::cout << "Process: " << info.command << "\n";
  std::cout << "PID: " << info.pid << "\n";
  std::cout << "User: " << info.user << "\n";
  std::cout << "Endpoint: " << info.endpoint << "\n";
  return 0;
}
