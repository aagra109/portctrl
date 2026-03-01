#include "list_command.h"
#include "command_exec.h"
#include "exit_codes.h"
#include "table_output.h"
#include "usage.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct ListenerEntry {
  std::string pid;
  std::string command;
  std::string user;
  std::string endpoint;
  int port = -1;
};

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

static int parseEndpointPort(const std::string &endpoint) {
  std::size_t colonPos = endpoint.rfind(':');
  if (colonPos == std::string::npos || colonPos + 1 >= endpoint.size()) {
    return -1;
  }

  int value = 0;
  for (std::size_t i = colonPos + 1; i < endpoint.size(); ++i) {
    char ch = endpoint[i];
    if (ch < '0' || ch > '9') {
      return -1;
    }
    value = (value * 10) + (ch - '0');
    if (value > 65535) {
      return -1;
    }
  }

  if (value < 1) {
    return -1;
  }

  return value;
}

static std::vector<ListenerEntry> parseListeners(const std::string &raw) {
  std::vector<ListenerEntry> listeners;
  auto lines = splitLines(raw);

  std::string currentPid;
  std::string currentCommand;
  std::string currentUser;

  for (const auto &line : lines) {
    if (line.size() < 2)
      continue;

    char tag = line[0];
    std::string value = line.substr(1);

    if (tag == 'p') {
      currentPid = value;
      currentCommand.clear();
      currentUser.clear();
    } else if (tag == 'c') {
      currentCommand = value;
    } else if (tag == 'u') {
      currentUser = value;
    } else if (tag == 'n') {
      if (currentPid.empty()) {
        continue;
      }

      ListenerEntry entry;
      entry.pid = currentPid;
      entry.command = currentCommand.empty() ? "unknown" : currentCommand;
      entry.user = currentUser.empty() ? "unknown" : currentUser;
      entry.endpoint = value;
      entry.port = parseEndpointPort(value);
      listeners.push_back(entry);
    }
  }

  return listeners;
}

static bool sortByPortThenPid(const ListenerEntry &a, const ListenerEntry &b) {
  if (a.port != b.port) {
    if (a.port < 0)
      return false;
    if (b.port < 0)
      return true;
    return a.port < b.port;
  }

  if (a.pid != b.pid)
    return a.pid < b.pid;
  if (a.user != b.user)
    return a.user < b.user;
  if (a.command != b.command)
    return a.command < b.command;

  return a.endpoint < b.endpoint;
}

static bool sameListener(const ListenerEntry &a, const ListenerEntry &b) {
  return a.port == b.port && a.pid == b.pid && a.user == b.user && a.command == b.command &&
         a.endpoint == b.endpoint;
}

int runListCommand(int argc, char *argv[]) {
  for (int i = 2; i < argc; ++i) {
    std::cerr << "Unknown option for list: " << argv[i] << "\n";
    usage();
    return toExitCode(ExitCode::kUsage);
  }

  CommandResult result = runCommand({"lsof", "-nP", "-iTCP", "-sTCP:LISTEN", "-Fpcun"});

  if (result.exitCode == 1 && result.output.empty()) {
    std::cout << "No listening TCP ports found.\n";
    return toExitCode(ExitCode::kOk);
  }

  if (result.exitCode != 0 && result.exitCode != 1) {
    std::cerr << "Failed to list listening ports.\n";
    if (!result.output.empty()) {
      std::cerr << result.output;
    }
    return toExitCode(ExitCode::kInspectFailure);
  }

  if (result.exitCode == 1 && !result.output.empty()) {
    std::cerr << "Failed to list listening ports.\n" << result.output;
    return toExitCode(ExitCode::kInspectFailure);
  }

  if (result.output.empty()) {
    std::cout << "No listening TCP ports found.\n";
    return toExitCode(ExitCode::kOk);
  }

  std::vector<ListenerEntry> listeners = parseListeners(result.output);
  if (listeners.empty()) {
    std::cerr << "Failed to parse listener data.\nRaw lsof fields:\n" << result.output;
    return toExitCode(ExitCode::kInspectFailure);
  }

  std::sort(listeners.begin(), listeners.end(), sortByPortThenPid);

  listeners.erase(std::unique(listeners.begin(), listeners.end(), sameListener), listeners.end());

  std::cout << "Listening TCP endpoints: " << listeners.size() << "\n";
  std::vector<std::vector<std::string>> rows;
  rows.reserve(listeners.size());
  for (const auto &listener : listeners) {
    std::string portText = listener.port > 0 ? std::to_string(listener.port) : "unknown";
    rows.push_back({portText, listener.pid, listener.user, listener.command, listener.endpoint});
  }
  std::cout << renderTable({"PORT", "PID", "USER", "PROCESS", "ENDPOINT"}, rows) << "\n";

  return toExitCode(ExitCode::kOk);
}
