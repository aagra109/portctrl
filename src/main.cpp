#include <cctype>
#include <cstdio>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/wait.h>

struct CommandResult {
  int exitCode;
  std::string output;
};

static void usage() {
  std::cout << "portdoctor usage:\n"
            << "  portdoctor who <port>\n"
            << "  portdoctor free <port> [--force] [--project <name>]\n"
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

  FILE *pipe = popen(command.c_str(), "r");
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

    std::string lsofCommand = "lsof -nP -iTCP:" + std::to_string(*port) + " -sTCP:LISTEN";
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

    std::cout << "Raw lsof output for port " << *port << ":\n";
    std::cout << result.output;
    return 0;
  }

  if (cmd == "free") {
    if (argc < 3) {
      std::cerr << "Missing port.\n";
      usage();
      return 1;
    }
    std::cout << "[stub] cmd=free port=" << argv[2] << "\n";
    return 0;
  }
  if (cmd == "history") {
    std::cout << "[stub] history\n";
    return 0;
  }

  usage();
  return 1;
}
