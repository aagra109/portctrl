#include "command_exec.h"

#include <cerrno>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>
#include <sys/wait.h>

static std::vector<std::string> splitArgs(const std::string &command) {
  std::vector<std::string> args;
  std::string current;

  for (char ch : command) {
    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
      if (!current.empty()) {
        args.push_back(current);
        current.clear();
      }
      continue;
    }
    current.push_back(ch);
  }

  if (!current.empty()) {
    args.push_back(current);
  }

  return args;
}

CommandResult runCommand(const std::string &command) {
  CommandResult result{1, ""};

  std::vector<std::string> args = splitArgs(command);
  if (args.empty()) {
    result.output = "Failed to execute command.";
    return result;
  }

  int pipefd[2];
  if (::pipe(pipefd) != 0) {
    result.output = std::string("Failed to create pipe: ") + std::strerror(errno);
    return result;
  }

  pid_t pid = ::fork();
  if (pid < 0) {
    int savedErrno = errno;
    ::close(pipefd[0]);
    ::close(pipefd[1]);
    result.output = std::string("Failed to fork process: ") + std::strerror(savedErrno);
    return result;
  }

  if (pid == 0) {
    ::close(pipefd[0]);
    ::dup2(pipefd[1], STDOUT_FILENO);
    ::dup2(pipefd[1], STDERR_FILENO);
    ::close(pipefd[1]);

    std::vector<char *> argv;
    argv.reserve(args.size() + 1);
    for (auto &arg : args) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);

    ::execvp(argv[0], argv.data());
    _exit(127);
  }

  ::close(pipefd[1]);
  char buffer[256];
  ssize_t bytesRead = 0;
  while ((bytesRead = ::read(pipefd[0], buffer, sizeof(buffer))) > 0) {
    result.output.append(buffer, static_cast<std::size_t>(bytesRead));
  }
  ::close(pipefd[0]);

  int status = 0;
  if (::waitpid(pid, &status, 0) < 0) {
    result.exitCode = 1;
    if (result.output.empty()) {
      result.output = std::string("waitpid failed: ") + std::strerror(errno);
    }
    return result;
  }

  if (status == -1) {
    result.exitCode = 1;
  } else if (WIFEXITED(status)) {
    result.exitCode = WEXITSTATUS(status);
  } else {
    result.exitCode = 1;
  }

  return result;
}
