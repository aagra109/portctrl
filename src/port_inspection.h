#pragma once

#include "types.h"
#include <optional>
#include <string>

std::optional<int> parsePort(const std::string &text);
CommandResult runListenerInspectCommand(int port);
bool parseFirstListener(const std::string &raw, ListenerInfo &out);
std::string listenerInspectCommand(int port);
InspectResult inspectPort(int port);