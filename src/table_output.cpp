#include "table_output.h"

#include <algorithm>
#include <sstream>

static std::string truncateCell(const std::string &value, std::size_t maxWidth) {
  if (maxWidth == 0) {
    return "";
  }
  if (value.size() <= maxWidth) {
    return value;
  }
  if (maxWidth <= 3) {
    return value.substr(0, maxWidth);
  }
  return value.substr(0, maxWidth - 3) + "...";
}

static std::string makeDivider(const std::vector<std::size_t> &widths) {
  std::string line = "+";
  for (std::size_t width : widths) {
    line += std::string(width + 2, '-');
    line += "+";
  }
  return line;
}

static std::string padCell(const std::string &value, std::size_t width) {
  if (value.size() >= width) {
    return value;
  }
  return value + std::string(width - value.size(), ' ');
}

std::string renderTable(const std::vector<std::string> &headers,
                        const std::vector<std::vector<std::string>> &rows,
                        std::size_t maxColumnWidth) {
  if (headers.empty()) {
    return "";
  }

  std::vector<std::size_t> widths(headers.size(), 0);
  for (std::size_t i = 0; i < headers.size(); ++i) {
    widths[i] = truncateCell(headers[i], maxColumnWidth).size();
  }

  for (const auto &row : rows) {
    for (std::size_t i = 0; i < headers.size() && i < row.size(); ++i) {
      widths[i] = std::max(widths[i], truncateCell(row[i], maxColumnWidth).size());
    }
  }

  std::ostringstream out;
  const std::string divider = makeDivider(widths);
  out << divider << "\n";
  out << "|";
  for (std::size_t i = 0; i < headers.size(); ++i) {
    out << " " << padCell(truncateCell(headers[i], maxColumnWidth), widths[i]) << " |";
  }
  out << "\n" << divider;

  for (const auto &row : rows) {
    out << "\n|";
    for (std::size_t i = 0; i < headers.size(); ++i) {
      std::string value;
      if (i < row.size()) {
        value = row[i];
      }
      out << " " << padCell(truncateCell(value, maxColumnWidth), widths[i]) << " |";
    }
    out << "\n" << divider;
  }

  return out.str();
}
