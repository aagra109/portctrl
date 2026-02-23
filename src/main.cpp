#include <cctype>
#include <cstdio>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
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
  if (lines.size() < 2)
    return false; // header + at least one data row

  std::stringstream ss(lines[1]);
  std::vector<std::string> cols;
  std::string token;
  while (ss >> token)
    cols.push_back(token);

  if (cols.size() < 9)
    return false;

  out.command = cols[0];
  out.pid = cols[1];
  out.user = cols[2];

  out.endpoint.clear();
  for (size_t i = 8; i < cols.size(); ++i) {
    if (i > 8)
      out.endpoint += " ";
    out.endpoint += cols[i];
  }

  return true;
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

    ListenerInfo info;
    if (!parseFirstListener(result.output, info)) {
      std::cout << "Port " << *port << " is in use, but failed to parse listener details.\n";
      std::cout << "Raw output:\n" << result.output;
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
