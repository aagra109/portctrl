#include "who_command.h"
#include "exit_codes.h"
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
    return toExitCode(ExitCode::kUsage);
  }
  if (argc > 3) {
    for (int i = 3; i < argc; ++i) {
      std::cerr << "Unknown option for who: " << argv[i] << "\n";
    }
    usage();
    return toExitCode(ExitCode::kUsage);
  }
  std::string portText = argv[2];
  auto port = parsePort(portText);
  if (!port.has_value()) {
    std::cerr << "Invalid port: " << portText << "\n";
    return toExitCode(ExitCode::kUsage);
  }

  InspectResult inspect = inspectPort(*port);

  if (inspect.status == InspectStatus::kError) {
    std::cerr << inspect.error << "\n";
    return toExitCode(ExitCode::kInspectFailure);
  }

  if (inspect.status == InspectStatus::kFree || inspect.listeners.empty()) {
    std::cout << "Port " << *port << " appears free (no LISTEN process found).\n";
    return toExitCode(ExitCode::kOk);
  }

  std::cout << "Port " << *port << " is in use.\n";
  std::cout << "Listening endpoints: " << inspect.listeners.size() << "\n";
  std::vector<std::vector<std::string>> rows;
  rows.reserve(inspect.listeners.size());
  for (const auto &listener : inspect.listeners) {
    rows.push_back({listener.pid, listener.user, listener.command, listener.endpoint});
  }
  std::cout << renderTable({"PID", "USER", "PROCESS", "ENDPOINT"}, rows) << "\n";
  return toExitCode(ExitCode::kOk);
}
