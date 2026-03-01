#pragma once

#include <cstddef>
#include <string>
#include <vector>

std::string renderTable(const std::vector<std::string> &headers,
                        const std::vector<std::vector<std::string>> &rows,
                        std::size_t maxColumnWidth = 48);
