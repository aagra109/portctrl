#include "port_inspection.h"
#include <cctype>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <sys/wait.h>
#include <vector>

std::optional<int> parsePort(const std::string &text) {
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

CommandResult runCommand(const std::string &command) {
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

bool parseFirstListener(const std::string &raw, ListenerInfo &out) {
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

std::string listenerInspectCommand(int port) {
  return "lsof -nP -iTCP:" + std::to_string(port) + " -sTCP:LISTEN -Fpcun";
}

bool inspectPort(int port, std::optional<ListenerInfo> &listener, std::string &error) {
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