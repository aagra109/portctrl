#pragma once

#include "types.h"

#include <optional>
#include <string>
#include <sys/types.h>

std::optional<pid_t> parseSignalablePid(const std::string &text);
std::string gracefulSignalName(GracefulSignal signal);
int gracefulSignalValue(GracefulSignal signal);
bool sendSignalToPid(pid_t pid, int signalValue, std::string &error, int *errorNumber = nullptr);
