#include "who_command.h"
#include "port_inspection.h"
#include "table_output.h"
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
  std::vector<std::vector<std::string>> rows;
  rows.reserve(inspect.listeners.size());
  for (const auto &listener : inspect.listeners) {
    rows.push_back({listener.pid, listener.user, listener.command, listener.endpoint});
  }
  std::cout << renderTable({"PID", "USER", "PROCESS", "ENDPOINT"}, rows) << "\n";
  return 0;
}
