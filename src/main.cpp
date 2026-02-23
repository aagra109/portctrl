#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

struct CommandResult {
  int exitCode;
  std::string output;
};

struct ListenerInfo {
  std::string command;
  std::string pid;
  std::string user;
  std::string endpoint;
};

enum class GracefulSignal {
  kTerm,
  kInt,
};

struct FreeOptions {
  bool apply = false;
  bool force = false;
  bool yes = false;
  GracefulSignal gracefulSignal = GracefulSignal::kTerm;
};

static void usage() {
  std::cout << "portdoctor usage:\n"
            << "  portdoctor who <port>\n"
            << "  portdoctor free <port> [--apply] [--force] [--yes] [--signal INT|TERM]\n"
            << "  portdoctor history [--limit <n>]\n";
}

static std::optional<int> parsePort(const std::string &text) {
  if (text.empty())
    return std::nullopt;

  for (char ch : text) {
    if (!std::isdigit(static_cast<unsigned char>(ch)))
      return std::nullopt;
  }

  try {
    long value = std::stol(text);
    if (value < 1 || value > 65535)
      return std::nullopt;
    return static_cast<int>(value);
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

static CommandResult runCommand(const std::string &command) {
  CommandResult result{1, ""};

  FILE *pipe = popen((command + " 2>&1").c_str(), "r");
  if (pipe == nullptr) {
    result.output = "Failed to execute command.";
    return result;
  }

  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result.output += buffer;
  }

  int status = pclose(pipe);
  if (status == -1) {
    result.exitCode = 1;
  } else if (WIFEXITED(status)) {
    result.exitCode = WEXITSTATUS(status);
  } else {
    result.exitCode = 1;
  }

  return result;
}

static std::vector<std::string> splitLines(const std::string &text) {
  std::vector<std::string> lines;
  std::stringstream ss(text);
  std::string line;
  while (std::getline(ss, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    lines.push_back(line);
  }
  return lines;
}

static bool parseFirstListener(const std::string &raw, ListenerInfo &out) {
  auto lines = splitLines(raw);
  std::string currentPid;
  std::string currentCommand;
  std::string currentUser;
  std::string currentEndpoint;

  for (const auto &line : lines) {
    if (line.size() < 2)
      continue;
    char tag = line[0];
    std::string value = line.substr(1);

    if (tag == 'p') {
      currentPid = value;
      currentCommand.clear();
      currentUser.clear();
      currentEndpoint.clear();
    } else if (tag == 'c') {
      currentCommand = value;
    } else if (tag == 'u') {
      currentUser = value;
    } else if (tag == 'n') {
      currentEndpoint = value;

      if (!currentPid.empty()) {
        out.pid = currentPid;
        out.command = currentCommand.empty() ? "unknown" : currentCommand;
        out.user = currentUser.empty() ? "unknown" : currentUser;
        out.endpoint = currentEndpoint;
        return true;
      }
    }
  }

  return false;
}

static std::string listenerInspectCommand(int port) {
  return "lsof -nP -iTCP:" + std::to_string(port) + " -sTCP:LISTEN -Fpcun";
}

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

static std::optional<GracefulSignal> parseGracefulSignal(const std::string &text) {
  std::string normalized = text;
  for (char &ch : normalized) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }

  if (normalized == "INT" || normalized == "SIGINT") {
    return GracefulSignal::kInt;
  }
  if (normalized == "TERM" || normalized == "SIGTERM") {
    return GracefulSignal::kTerm;
  }
  return std::nullopt;
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

static bool parseFreeOptions(int argc, char *argv[], FreeOptions &options, std::string &error) {
  for (int i = 3; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--apply") {
      options.apply = true;
      continue;
    }
    if (arg == "--force") {
      options.force = true;
      continue;
    }
    if (arg == "--yes") {
      options.yes = true;
      continue;
    }
    if (arg == "--signal") {
      if (i + 1 >= argc) {
        error = "Missing value for --signal. Expected INT or TERM.";
        return false;
      }
      std::string value = argv[++i];
      auto parsedSignal = parseGracefulSignal(value);
      if (!parsedSignal.has_value()) {
        error = "Invalid --signal value: " + value + " (expected INT or TERM).";
        return false;
      }
      options.gracefulSignal = *parsedSignal;
      continue;
    }
    error = "Unknown option for free: " + arg;
    return false;
  }

  if (options.force && !options.apply) {
    error = "--force requires --apply.";
    return false;
  }

  return true;
}

static bool inspectPort(int port, std::optional<ListenerInfo> &listener, std::string &error) {
  CommandResult result = runCommand(listenerInspectCommand(port));

  if (result.exitCode == 1 && result.output.empty()) {
    listener.reset();
    return true;
  }

  if (result.exitCode != 0 && result.exitCode != 1) {
    error = "Failed to inspect port " + std::to_string(port) + ".\n";
    if (!result.output.empty()) {
      error += result.output;
    }
    return false;
  }

  if (result.exitCode == 1 && !result.output.empty()) {
    error = "Failed to inspect port " + std::to_string(port) + ".\n" + result.output;
    return false;
  }

  if (result.output.empty()) {
    listener.reset();
    return true;
  }

  ListenerInfo parsedListener;
  if (!parseFirstListener(result.output, parsedListener)) {
    error = "Port " + std::to_string(port) +
            " appears occupied, but listener parsing failed.\nRaw lsof fields:\n" + result.output;
    return false;
  }

  listener = parsedListener;
  return true;
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

int main(int argc, char *argv[]) {
  if (argc < 2) {
    usage();
    return 1;
  }

  std::string cmd = argv[1];
  if (cmd == "who") {
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

    std::string lsofCommand = "lsof -nP -iTCP:" + std::to_string(*port) + " -sTCP:LISTEN -Fpcun";
    CommandResult result = runCommand(lsofCommand);

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

  if (cmd == "free") {
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

    std::optional<ListenerInfo> listener;
    std::string inspectError;
    if (!inspectPort(*port, listener, inspectError)) {
      std::cerr << inspectError;
      return 1;
    }

    if (!listener.has_value()) {
      std::cout << "Port " << *port << " is already free.\n";
      return 0;
    }

    const ListenerInfo initialListener = *listener;
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

    std::optional<ListenerInfo> afterGracefulListener;
    std::string afterGracefulError;
    if (!inspectPort(*port, afterGracefulListener, afterGracefulError)) {
      std::cerr << "Sent SIG" << gracefulName << ", but failed to re-check port " << *port << ".\n";
      std::cerr << afterGracefulError;
      return 1;
    }

    if (!afterGracefulListener.has_value()) {
      std::cout << "Success: port " << *port << " is now free.\n";
      return 0;
    }

    std::cout << "Port is still busy after SIG" << gracefulName << ".\n";
    if (!options.force) {
      std::cout << "Re-run with --apply --force to escalate to SIGKILL.\n";
      return 1;
    }

    const ListenerInfo currentListener = *afterGracefulListener;
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

    std::optional<ListenerInfo> afterForceListener;
    std::string afterForceError;
    if (!inspectPort(*port, afterForceListener, afterForceError)) {
      std::cerr << "Sent SIGKILL, but failed to re-check port " << *port << ".\n";
      std::cerr << afterForceError;
      return 1;
    }

    if (!afterForceListener.has_value()) {
      std::cout << "Success: port " << *port << " is now free.\n";
      return 0;
    }

    std::cerr << "Port " << *port << " is still in use after SIGKILL by process "
              << afterForceListener->command << " (PID " << afterForceListener->pid << ").\n";
    return 1;
  }
  if (cmd == "history") {
    std::cout << "[stub] history\n";
    return 0;
  }

  usage();
  return 1;
}
