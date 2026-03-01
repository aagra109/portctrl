#pragma once

#include <string>
#include <vector>

struct CommandResult {
  int exitCode = -1;
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

enum class InspectStatus {
  kFree,
  kOccupied,
  kError,
};

struct InspectResult {
  InspectStatus status = InspectStatus::kError;
  std::vector<ListenerInfo> listeners;
  std::string error;
};
