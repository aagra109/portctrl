#include "free_command.h"
#include "free_options.h"
#include "port_inspection.h"
#include "process_actions.h"
#include "table_output.h"
#include "types.h"
#include "usage.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

static bool confirmAction(const std::string &prompt, bool autoYes) {
  if (autoYes) {
    return true;
  }

  if (!isatty(STDIN_FILENO)) {
    std::cerr << "Refusing to proceed in non-interactive mode without --yes.\n";
    return false;
  }

  std::cout << prompt << " [y/N]: ";
  std::string answer;
  if (!std::getline(std::cin, answer)) {
    std::cout << "Cancelled.\n";
    return false;
  }

  for (char &ch : answer) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  if (answer == "Y" || answer == "YES") {
    return true;
  }

  std::cout << "Cancelled.\n";
  return false;
}

static bool collectSignalablePids(const std::vector<ListenerInfo> &listeners,
                                  std::vector<pid_t> &pids, std::string &invalidPid) {
  pids.clear();

  for (const auto &listener : listeners) {
    auto parsedPid = parseSignalablePid(listener.pid);
    if (!parsedPid.has_value()) {
      invalidPid = listener.pid;
      return false;
    }

    if (std::find(pids.begin(), pids.end(), *parsedPid) == pids.end()) {
      pids.push_back(*parsedPid);
    }
  }

  std::sort(pids.begin(), pids.end());
  return !pids.empty();
}

static std::string joinPids(const std::vector<pid_t> &pids) {
  std::string joined;
  for (std::size_t i = 0; i < pids.size(); ++i) {
    if (i > 0) {
      joined += ", ";
    }
    joined += std::to_string(pids[i]);
  }
  return joined;
}

static bool sendSignalToAll(const std::vector<pid_t> &pids, int signalValue,
                            const std::string &signalName) {
  std::string signalError;
  for (pid_t pid : pids) {
    if (!sendSignalToPid(pid, signalValue, signalError)) {
      std::cerr << "Failed to send SIG" << signalName << " to PID " << pid << ".\n";
      std::cerr << signalError << "\n";
      return false;
    }
  }
  return true;
}

static std::vector<std::vector<std::string>>
listenerRowsForTable(const std::vector<ListenerInfo> &listeners) {
  std::vector<std::vector<std::string>> rows;
  rows.reserve(listeners.size());
  for (const auto &listener : listeners) {
    rows.push_back({listener.pid, listener.user, listener.command, listener.endpoint});
  }
  return rows;
}

int runFreeCommand(int argc, char *argv[]) {
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

  FreeOptions options;
  std::string optionError;
  if (!parseFreeOptions(argc, argv, options, optionError)) {
    std::cerr << optionError << "\n";
    return 1;
  }

  InspectResult inspect = inspectPort(*port);
  if (inspect.status == InspectStatus::kError) {
    std::cerr << inspect.error << "\n";
    return 1;
  }

  if (inspect.status == InspectStatus::kFree || inspect.listeners.empty()) {
    std::cout << "Port " << *port << " is already free.\n";
    return 0;
  }

  std::cout << "Current listeners:\n";
  std::cout << renderTable({"PID", "USER", "PROCESS", "ENDPOINT"},
                           listenerRowsForTable(inspect.listeners))
            << "\n";

  std::vector<pid_t> initialPids;
  std::string invalidPid;
  if (!collectSignalablePids(inspect.listeners, initialPids, invalidPid)) {
    std::cerr << "Refusing to signal invalid PID '" << invalidPid << "'.\n";
    return 1;
  }

  const std::string gracefulName = gracefulSignalName(options.gracefulSignal);
  std::cout << "Port " << *port << " has " << inspect.listeners.size() << " listening endpoint(s)"
            << " across " << initialPids.size() << " process(es).\n";
  std::cout << "Target PIDs: " << joinPids(initialPids) << "\n";
  std::cout << "Planned graceful signal: " << gracefulName << "\n";

  if (!options.apply) {
    std::cout << "Dry-run mode: no signal sent.\n";
    std::cout << "Run: ./bin/portctrl free " << *port << " --apply";
    if (options.gracefulSignal != GracefulSignal::kTerm) {
      std::cout << " --signal " << gracefulName;
    }
    std::cout << "\n";
    return 0;
  }

  if (!confirmAction("Send SIG" + gracefulName + " to PIDs " + joinPids(initialPids) + "?",
                     options.yes)) {
    return 1;
  }

  if (!sendSignalToAll(initialPids, gracefulSignalValue(options.gracefulSignal), gracefulName)) {
    return 1;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(900));

  InspectResult afterGraceful = inspectPort(*port);

  if (afterGraceful.status == InspectStatus::kError) {
    std::cerr << "Sent SIG" << gracefulName << ", but failed to re-check port " << *port << ".\n";
    std::cerr << afterGraceful.error;
    return 1;
  }

  if (afterGraceful.status == InspectStatus::kFree || afterGraceful.listeners.empty()) {
    std::cout << "Success: port " << *port << " is now free.\n";
    return 0;
  }

  std::cout << "Port is still busy after SIG" << gracefulName << ".\n";
  std::cout << "Listeners after graceful attempt:\n";
  std::cout << renderTable({"PID", "USER", "PROCESS", "ENDPOINT"},
                           listenerRowsForTable(afterGraceful.listeners))
            << "\n";
  if (!options.force) {
    std::cout << "Re-run with --apply --force to escalate to SIGKILL.\n";
    return 1;
  }

  std::vector<pid_t> currentPids;
  if (!collectSignalablePids(afterGraceful.listeners, currentPids, invalidPid)) {
    std::cerr << "Refusing to signal invalid PID '" << invalidPid << "'.\n";
    return 1;
  }

  if (currentPids != initialPids) {
    std::cout << "Listener PIDs changed from [" << joinPids(initialPids) << "] to ["
              << joinPids(currentPids) << "] before force escalation.\n";
  }

  if (!confirmAction("Send SIGKILL to PIDs " + joinPids(currentPids) + "?", options.yes)) {
    return 1;
  }

  if (!sendSignalToAll(currentPids, SIGKILL, "KILL")) {
    return 1;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  InspectResult afterForce = inspectPort(*port);
  if (afterForce.status == InspectStatus::kError) {
    std::cerr << "Sent SIGKILL, but failed to re-check port " << *port << ".\n";
    std::cerr << afterForce.error;
    return 1;
  }

  if (afterForce.status == InspectStatus::kFree || afterForce.listeners.empty()) {
    std::cout << "Success: port " << *port << " is now free.\n";
    return 0;
  }

  std::vector<pid_t> remainingPids;
  std::string remainingPidError;
  if (collectSignalablePids(afterForce.listeners, remainingPids, remainingPidError)) {
    std::cerr << "Remaining listener PIDs: " << joinPids(remainingPids) << "\n";
  }
  std::cerr << "Remaining listeners:\n";
  std::cerr << renderTable({"PID", "USER", "PROCESS", "ENDPOINT"},
                           listenerRowsForTable(afterForce.listeners))
            << "\n";

  std::cerr << "Port " << *port << " is still in use after SIGKILL by process "
            << afterForce.listeners.front().command << " (PID " << afterForce.listeners.front().pid
            << ").\n";

  return 1;
}
