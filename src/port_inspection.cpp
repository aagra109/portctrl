#include "port_inspection.h"
#include "command_exec.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
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

static bool sortByPidThenEndpoint(const ListenerInfo &a, const ListenerInfo &b) {
  if (a.pid != b.pid) {
    return a.pid < b.pid;
  }
  if (a.user != b.user) {
    return a.user < b.user;
  }
  if (a.command != b.command) {
    return a.command < b.command;
  }
  return a.endpoint < b.endpoint;
}

static bool sameListener(const ListenerInfo &a, const ListenerInfo &b) {
  return a.pid == b.pid && a.user == b.user && a.command == b.command && a.endpoint == b.endpoint;
}

static std::vector<ListenerInfo> parseAllListeners(const std::string &raw) {
  std::vector<ListenerInfo> listeners;
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
      if (!currentPid.empty()) {
        ListenerInfo listener;
        listener.pid = currentPid;
        listener.command = currentCommand.empty() ? "unknown" : currentCommand;
        listener.user = currentUser.empty() ? "unknown" : currentUser;
        listener.endpoint = value;
        listeners.push_back(listener);
      }
    }
  }

  std::sort(listeners.begin(), listeners.end(), sortByPidThenEndpoint);
  listeners.erase(std::unique(listeners.begin(), listeners.end(), sameListener), listeners.end());
  return listeners;
}

static std::vector<std::string> listenerInspectArgs(int port) {
  return {"lsof", "-nP", "-iTCP:" + std::to_string(port), "-sTCP:LISTEN", "-Fpcun"};
}

static CommandResult runListenerInspectCommand(int port) {
  return runCommand(listenerInspectArgs(port));
}

InspectResult inspectPort(int port) {
  InspectResult inspect;
  CommandResult result = runListenerInspectCommand(port);

  if (result.exitCode == 1 && result.output.empty()) {
    inspect.status = InspectStatus::kFree;
    return inspect;
  }

  if (result.exitCode != 0 && result.exitCode != 1) {
    inspect.status = InspectStatus::kError;
    inspect.error = "Failed to inspect port " + std::to_string(port) + ".";
    if (!result.output.empty()) {
      inspect.error += "\n" + result.output;
    }
    return inspect;
  }

  if (result.exitCode == 1 && !result.output.empty()) {
    inspect.status = InspectStatus::kError;
    inspect.error = "Failed to inspect port " + std::to_string(port) + ".\n" + result.output;
    return inspect;
  }

  if (result.output.empty()) {
    inspect.status = InspectStatus::kFree;
    return inspect;
  }

  inspect.listeners = parseAllListeners(result.output);
  if (inspect.listeners.empty()) {
    inspect.status = InspectStatus::kError;
    inspect.error = "Port " + std::to_string(port) +
                    " appears occupied, but listener parsing failed.\nRaw lsof fields:\n" +
                    result.output;
    return inspect;
  }

  inspect.status = InspectStatus::kOccupied;
  return inspect;
}
