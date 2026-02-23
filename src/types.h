#pragma once

#include <string>

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
