#include "process_actions.h"

#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <exception>
#include <limits>
#include <optional>
#include <string>

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

std::optional<pid_t> parseSignalablePid(const std::string &text) {
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

std::string gracefulSignalName(GracefulSignal signal) {
  switch (signal) {
  case GracefulSignal::kInt:
    return "INT";
  case GracefulSignal::kTerm:
  default:
    return "TERM";
  }
}

int gracefulSignalValue(GracefulSignal signal) {
  switch (signal) {
  case GracefulSignal::kInt:
    return SIGINT;
  case GracefulSignal::kTerm:
  default:
    return SIGTERM;
  }
}

bool sendSignalToPid(pid_t pid, int signalValue, std::string &error, int *errorNumber) {
  if (::kill(pid, signalValue) == 0) {
    return true;
  }

  const int savedErrno = errno;
  if (errorNumber != nullptr) {
    *errorNumber = savedErrno;
  }
  error = std::string("kill(") + std::to_string(pid) + ", " + std::to_string(signalValue) +
          ") failed: " + std::strerror(savedErrno);
  return false;
}
