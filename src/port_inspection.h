#pragma once

#include "types.h"
#include <optional>
#include <string>

std::optional<int> parsePort(const std::string &text);
InspectResult inspectPort(int port);