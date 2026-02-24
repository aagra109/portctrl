#include "free_options.h"

#include <cctype>
#include <optional>
#include <string>

static std::optional<GracefulSignal> parseGracefulSignal(const std::string &text) {
  std::string normalized = text;
  for (char &ch : normalized) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }

  if (normalized == "INT" || normalized == "SIGINT") {
    return GracefulSignal::kInt;
  }
  if (normalized == "TERM" || normalized == "SIGTERM") {
    return GracefulSignal::kTerm;
  }
  return std::nullopt;
}

bool parseFreeOptions(int argc, char *argv[], FreeOptions &options, std::string &error) {
  for (int i = 3; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--apply") {
      options.apply = true;
      continue;
    }
    if (arg == "--force") {
      options.force = true;
      continue;
    }
    if (arg == "--yes") {
      options.yes = true;
      continue;
    }
    if (arg == "--signal") {
      if (i + 1 >= argc) {
        error = "Missing value for --signal. Expected INT or TERM.";
        return false;
      }
      std::string value = argv[++i];
      auto parsedSignal = parseGracefulSignal(value);
      if (!parsedSignal.has_value()) {
        error = "Invalid --signal value: " + value + " (expected INT or TERM).";
        return false;
      }
      options.gracefulSignal = *parsedSignal;
      continue;
    }
    error = "Unknown option for free: " + arg;
    return false;
  }

  if (options.force && !options.apply) {
    error = "--force requires --apply.";
    return false;
  }

  return true;
}
