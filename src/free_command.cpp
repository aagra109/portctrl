#include "free_command.h"
#include "free_options.h"
#include "port_inspection.h"
#include "types.h"
#include "usage.h"

#include <cctype>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <unistd.h>

static std::optional<long> parsePositiveNumber(const std::string &text) {
  if (text.empty()) {
    return std::nullopt;
  }
  for (char ch : text) {
    if (!std::isdigit(static_cast<unsigned char>(ch))) {
      return std::nullopt;
    }
  }
  try {
    long value = std::stol(text);
    if (value <= 0) {
      return std::nullopt;
    }
    return value;
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

static std::optional<pid_t> parseSignalablePid(const std::string &text) {
  auto parsed = parsePositiveNumber(text);
  if (!parsed.has_value()) {
    return std::nullopt;
  }
  if (*parsed > std::numeric_limits<pid_t>::max()) {
    return std::nullopt;
  }
  pid_t pid = static_cast<pid_t>(*parsed);
  if (pid <= 1) {
    return std::nullopt;
  }
  return pid;
}

static std::string gracefulSignalName(GracefulSignal signal) {
  switch (signal) {
  case GracefulSignal::kInt:
    return "INT";
  case GracefulSignal::kTerm:
  default:
    return "TERM";
  }
}

static int gracefulSignalValue(GracefulSignal signal) {
  switch (signal) {
  case GracefulSignal::kInt:
    return SIGINT;
  case GracefulSignal::kTerm:
  default:
    return SIGTERM;
  }
}

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

static bool sendSignalToPid(pid_t pid, int signalValue, std::string &error) {
  if (::kill(pid, signalValue) == 0) {
    return true;
  }

  error = std::string("kill(") + std::to_string(pid) + ", " + std::to_string(signalValue) +
          ") failed: " + std::strerror(errno);
  return false;
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

  if (inspect.status == InspectStatus::kFree || !inspect.listener.has_value()) {
    std::cout << "Port " << *port << " is already free.\n";
    return 0;
  }

  const ListenerInfo initialListener = *inspect.listener;
  auto initialPid = parseSignalablePid(initialListener.pid);

  if (!initialPid.has_value()) {
    std::cerr << "Refusing to signal invalid PID '" << initialListener.pid << "'.\n";
    return 1;
  }

  const std::string gracefulName = gracefulSignalName(options.gracefulSignal);
  std::cout << "Port " << *port << " is owned by " << initialListener.command << " (PID "
            << initialListener.pid << ").\n";
  std::cout << "Planned graceful signal: " << gracefulName << "\n";

  if (!options.apply) {
    std::cout << "Dry-run mode: no signal sent.\n";
    std::cout << "Run: ./bin/portdoctor free " << *port << " --apply";
    if (options.gracefulSignal != GracefulSignal::kTerm) {
      std::cout << " --signal " << gracefulName;
    }
    std::cout << "\n";
    return 0;
  }

  if (!confirmAction("Send SIG" + gracefulName + " to PID " + std::to_string(*initialPid) + "?",
                     options.yes)) {
    return 1;
  }

  std::string signalError;
  if (!sendSignalToPid(*initialPid, gracefulSignalValue(options.gracefulSignal), signalError)) {
    std::cerr << "Failed to send SIG" << gracefulName << " to PID " << *initialPid << ".\n";
    std::cerr << signalError << "\n";
    return 1;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(900));

  InspectResult afterGraceful = inspectPort(*port);

  if (afterGraceful.status == InspectStatus::kError) {
    std::cerr << "Sent SIG" << gracefulName << ", but failed to re-check port " << *port << ".\n";
    std::cerr << afterGraceful.error;
    return 1;
  }

  if (afterGraceful.status == InspectStatus::kFree || !afterGraceful.listener.has_value()) {
    std::cout << "Success: port " << *port << " is now free.\n";
    return 0;
  }

  std::cout << "Port is still busy after SIG" << gracefulName << ".\n";
  if (!options.force) {
    std::cout << "Re-run with --apply --force to escalate to SIGKILL.\n";
    return 1;
  }

  const ListenerInfo currentListener = *afterGraceful.listener;
  auto currentPid = parseSignalablePid(currentListener.pid);

  if (!currentPid.has_value()) {
    std::cerr << "Refusing to signal invalid PID '" << currentListener.pid << "'.\n";
    return 1;
  }

  if (*currentPid != *initialPid) {
    std::cout << "Listener PID changed from " << *initialPid << " to " << *currentPid
              << " before force escalation.\n";
  }

  if (!confirmAction("Send SIGKILL to PID " + std::to_string(*currentPid) + "?", options.yes)) {
    return 1;
  }

  if (!sendSignalToPid(*currentPid, SIGKILL, signalError)) {
    std::cerr << "Failed to send SIGKILL to PID " << *currentPid << ".\n";
    std::cerr << signalError << "\n";
    return 1;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  InspectResult afterForce = inspectPort(*port);
  if (afterForce.status == InspectStatus::kError) {
    std::cerr << "Sent SIGKILL, but failed to re-check port " << *port << ".\n";
    std::cerr << afterForce.error;
    return 1;
  }

  if (afterForce.status == InspectStatus::kFree || !afterForce.listener.has_value()) {
    std::cout << "Success: port " << *port << " is now free.\n";
    return 0;
  }

  std::cerr << "Port " << *port << " is still in use after SIGKILL by process "
            << afterForce.listener->command << " (PID " << afterForce.listener->pid << ").\n";

  return 1;
}
