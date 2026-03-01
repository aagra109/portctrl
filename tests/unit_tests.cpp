#include "exit_codes.h"
#include "free_options.h"
#include "port_inspection.h"
#include "process_actions.h"
#include "table_output.h"

#include <cerrno>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void expectTrue(bool condition, const std::string &name) {
  if (!condition) {
    std::cerr << "FAIL: " << name << "\n";
    ++g_failures;
  }
}

template <typename T>
void expectEq(const T &actual, const T &expected, const std::string &name) {
  if (!(actual == expected)) {
    std::cerr << "FAIL: " << name << " (expected: " << expected << ", actual: " << actual << ")\n";
    ++g_failures;
  }
}

bool parseFreeOptionsFrom(const std::vector<std::string> &args, FreeOptions &options,
                          std::string &error) {
  std::vector<char *> argv;
  argv.reserve(args.size());
  for (const auto &arg : args) {
    argv.push_back(const_cast<char *>(arg.c_str()));
  }
  return parseFreeOptions(static_cast<int>(argv.size()), argv.data(), options, error);
}

void testParsePort() {
  expectTrue(parsePort("3000").has_value(), "parsePort valid");
  expectEq(parsePort("3000").value_or(-1), 3000, "parsePort valid value");
  expectTrue(!parsePort("0").has_value(), "parsePort rejects zero");
  expectTrue(!parsePort("65536").has_value(), "parsePort rejects >65535");
  expectTrue(!parsePort("abc").has_value(), "parsePort rejects non-numeric");
}

void testParseFreeOptions() {
  FreeOptions options;
  std::string error;
  bool ok = parseFreeOptionsFrom({"portctrl", "free", "3000", "--apply", "--signal", "INT", "--yes"},
                                 options, error);
  expectTrue(ok, "parseFreeOptions valid input");
  expectTrue(options.apply, "parseFreeOptions apply");
  expectTrue(options.yes, "parseFreeOptions yes");
  expectEq(static_cast<int>(options.gracefulSignal), static_cast<int>(GracefulSignal::kInt),
           "parseFreeOptions signal INT");

  options = FreeOptions{};
  error.clear();
  ok = parseFreeOptionsFrom({"portctrl", "free", "3000", "--force"}, options, error);
  expectTrue(!ok, "parseFreeOptions force requires apply");
  expectTrue(error.find("--force requires --apply.") != std::string::npos,
             "parseFreeOptions force error message");

  options = FreeOptions{};
  error.clear();
  ok = parseFreeOptionsFrom({"portctrl", "free", "3000", "--signal", "HUP"}, options, error);
  expectTrue(!ok, "parseFreeOptions invalid signal");
}

void testParseSignalablePid() {
  expectTrue(parseSignalablePid("123").has_value(), "parseSignalablePid valid");
  expectEq(static_cast<long>(parseSignalablePid("123").value_or(-1)), 123L,
           "parseSignalablePid valid value");
  expectTrue(!parseSignalablePid("1").has_value(), "parseSignalablePid rejects pid 1");
  expectTrue(!parseSignalablePid("0").has_value(), "parseSignalablePid rejects zero");
  expectTrue(!parseSignalablePid("abc").has_value(), "parseSignalablePid rejects non-numeric");
}

void testRenderTable() {
  std::string table = renderTable({"COL"}, {{"abcdef"}}, 4);
  expectTrue(table.find("COL") != std::string::npos, "renderTable includes header");
  expectTrue(table.find("a...") != std::string::npos, "renderTable truncates cells");
}

void testExitCodeClassification() {
  expectEq(toExitCode(ExitCode::kOk), 0, "toExitCode ok");
  expectEq(toExitCode(classifySignalErrno(EPERM)), toExitCode(ExitCode::kPermissionDenied),
           "classifySignalErrno EPERM");
  expectEq(toExitCode(classifySignalErrno(EINVAL)), toExitCode(ExitCode::kInspectFailure),
           "classifySignalErrno generic");
}

} // namespace

int main() {
  testParsePort();
  testParseFreeOptions();
  testParseSignalablePid();
  testRenderTable();
  testExitCodeClassification();

  if (g_failures == 0) {
    std::cout << "unit tests: ok\n";
    return 0;
  }

  std::cerr << "unit tests: " << g_failures << " failure(s)\n";
  return 1;
}
