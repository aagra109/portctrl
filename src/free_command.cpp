#include "free_command.h"
#include "cli_constants.h"
#include "exit_codes.h"
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

enum class ConfirmResult {
  kConfirmed,
  kDeclined,
  kRequiresYes,
};

static constexpr std::chrono::milliseconds kInspectPollInterval(100);
static constexpr std::chrono::milliseconds kGracefulWaitTimeout(3000);
static constexpr std::chrono::milliseconds kForceWaitTimeout(1500);

static ConfirmResult confirmAction(const std::string &prompt, bool autoYes) {
  if (autoYes) {
    return ConfirmResult::kConfirmed;
  }

  if (!isatty(STDIN_FILENO)) {
    std::cerr << "Refusing to proceed in non-interactive mode without --yes.\n";
    return ConfirmResult::kRequiresYes;
  }

  std::cout << prompt << " [y/N]: ";
  std::string answer;
  if (!std::getline(std::cin, answer)) {
    std::cout << "Cancelled.\n";
    return ConfirmResult::kDeclined;
  }

  for (char &ch : answer) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  if (answer == "Y" || answer == "YES") {
    return ConfirmResult::kConfirmed;
  }

  std::cout << "Cancelled.\n";
  return ConfirmResult::kDeclined;
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

static ExitCode sendSignalToAll(const std::vector<pid_t> &pids, int signalValue,
                                const std::string &signalName) {
  std::string signalError;
  for (pid_t pid : pids) {
    int signalErrno = 0;
    if (!sendSignalToPid(pid, signalValue, signalError, &signalErrno)) {
      std::cerr << "Failed to send SIG" << signalName << " to PID " << pid << ".\n";
      std::cerr << signalError << "\n";
      return classifySignalErrno(signalErrno);
    }
  }
  return ExitCode::kOk;
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

static InspectResult waitForPortState(int port, std::chrono::milliseconds timeout) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  InspectResult latest;

  while (true) {
    latest = inspectPort(port);
    if (latest.status == InspectStatus::kError || latest.status == InspectStatus::kFree ||
        latest.listeners.empty()) {
      return latest;
    }

    if (std::chrono::steady_clock::now() >= deadline) {
      return latest;
    }

    std::this_thread::sleep_for(kInspectPollInterval);
  }
}

int runFreeCommand(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Missing port.\n";
    usage();
    return toExitCode(ExitCode::kUsage);
  }

  std::string portText = argv[2];
  auto port = parsePort(portText);
  if (!port.has_value()) {
    std::cerr << "Invalid port: " << portText << "\n";
    return toExitCode(ExitCode::kUsage);
  }

  FreeOptions options;
  std::string optionError;
  if (!parseFreeOptions(argc, argv, options, optionError)) {
    std::cerr << optionError << "\n";
    return toExitCode(ExitCode::kUsage);
  }

  InspectResult inspect = inspectPort(*port);
  if (inspect.status == InspectStatus::kError) {
    std::cerr << inspect.error << "\n";
    return toExitCode(ExitCode::kInspectFailure);
  }

  if (inspect.status == InspectStatus::kFree || inspect.listeners.empty()) {
    std::cout << "Port " << *port << " is already free.\n";
    return toExitCode(ExitCode::kOk);
  }

  std::cout << "Current listeners:\n";
  std::cout << renderTable({"PID", "USER", "PROCESS", "ENDPOINT"},
                           listenerRowsForTable(inspect.listeners))
            << "\n";

  std::vector<pid_t> initialPids;
  std::string invalidPid;
  if (!collectSignalablePids(inspect.listeners, initialPids, invalidPid)) {
    std::cerr << "Refusing to signal invalid PID '" << invalidPid << "'.\n";
    return toExitCode(ExitCode::kInspectFailure);
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
    return toExitCode(ExitCode::kOk);
  }

  ConfirmResult gracefulConfirm = confirmAction(
      "Send SIG" + gracefulName + " to PIDs " + joinPids(initialPids) + "?", options.yes);
  if (gracefulConfirm == ConfirmResult::kRequiresYes) {
    return toExitCode(ExitCode::kUsage);
  }
  if (gracefulConfirm == ConfirmResult::kDeclined) {
    return toExitCode(ExitCode::kUnresolved);
  }

  ExitCode gracefulSignalResult =
      sendSignalToAll(initialPids, gracefulSignalValue(options.gracefulSignal), gracefulName);
  if (gracefulSignalResult != ExitCode::kOk) {
    return toExitCode(gracefulSignalResult);
  }

  InspectResult afterGraceful = waitForPortState(*port, kGracefulWaitTimeout);

  if (afterGraceful.status == InspectStatus::kError) {
    std::cerr << "Sent SIG" << gracefulName << ", but failed to re-check port " << *port << ".\n";
    std::cerr << afterGraceful.error;
    return toExitCode(ExitCode::kInspectFailure);
  }

  if (afterGraceful.status == InspectStatus::kFree || afterGraceful.listeners.empty()) {
    std::cout << "Success: port " << *port << " is now free.\n";
    return toExitCode(ExitCode::kOk);
  }

  std::cout << "Port is still busy after SIG" << gracefulName << ".\n";
  std::cout << "Listeners after graceful attempt:\n";
  std::cout << renderTable({"PID", "USER", "PROCESS", "ENDPOINT"},
                           listenerRowsForTable(afterGraceful.listeners))
            << "\n";
  if (!options.force) {
    std::cout << "Re-run with --apply --force to escalate to SIGKILL.\n";
    return toExitCode(ExitCode::kUnresolved);
  }

  std::vector<pid_t> currentPids;
  if (!collectSignalablePids(afterGraceful.listeners, currentPids, invalidPid)) {
    std::cerr << "Refusing to signal invalid PID '" << invalidPid << "'.\n";
    return toExitCode(ExitCode::kInspectFailure);
  }

  if (currentPids != initialPids) {
    std::cout << "Listener PIDs changed from [" << joinPids(initialPids) << "] to ["
              << joinPids(currentPids) << "] before force escalation.\n";
  }

  ConfirmResult forceConfirm =
      confirmAction("Send SIGKILL to PIDs " + joinPids(currentPids) + "?", options.yes);
  if (forceConfirm == ConfirmResult::kRequiresYes) {
    return toExitCode(ExitCode::kUsage);
  }
  if (forceConfirm == ConfirmResult::kDeclined) {
    return toExitCode(ExitCode::kUnresolved);
  }

  ExitCode forceSignalResult = sendSignalToAll(currentPids, SIGKILL, kSignalNameKill);
  if (forceSignalResult != ExitCode::kOk) {
    return toExitCode(forceSignalResult);
  }

  InspectResult afterForce = waitForPortState(*port, kForceWaitTimeout);
  if (afterForce.status == InspectStatus::kError) {
    std::cerr << "Sent SIGKILL, but failed to re-check port " << *port << ".\n";
    std::cerr << afterForce.error;
    return toExitCode(ExitCode::kInspectFailure);
  }

  if (afterForce.status == InspectStatus::kFree || afterForce.listeners.empty()) {
    std::cout << "Success: port " << *port << " is now free.\n";
    return toExitCode(ExitCode::kOk);
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

  return toExitCode(ExitCode::kUnresolved);
}
