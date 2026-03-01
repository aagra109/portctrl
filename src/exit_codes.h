#pragma once

#include <cerrno>

enum class ExitCode : int {
  kOk = 0,
  kUsage = 1,
  kInspectFailure = 2,
  kPermissionDenied = 3,
  kUnresolved = 4,
};

inline int toExitCode(ExitCode code) { return static_cast<int>(code); }

inline ExitCode classifySignalErrno(int errorNumber) {
  if (errorNumber == EPERM) {
    return ExitCode::kPermissionDenied;
  }
  return ExitCode::kInspectFailure;
}
