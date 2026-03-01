#include "command_exec.h"

#include <cerrno>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

CommandResult runCommand(const std::vector<std::string> &args) {
  CommandResult result{1, ""};

  if (args.empty()) {
    result.output = "Failed to execute command.";
    return result;
  }

  std::vector<std::vector<char>> argBuffers;
  argBuffers.reserve(args.size());
  std::vector<char *> argv;
  argv.reserve(args.size() + 1);
  for (const auto &arg : args) {
    argBuffers.emplace_back(arg.begin(), arg.end());
    argBuffers.back().push_back('\0');
    argv.push_back(argBuffers.back().data());
  }
  argv.push_back(nullptr);

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

    ::execvp(argv[0], argv.data());
    static constexpr char kExecError[] = "Failed to execute command via execvp.\n";
    (void)::write(STDERR_FILENO, kExecError, sizeof(kExecError) - 1);
    _exit(127);
  }

  ::close(pipefd[1]);
  char buffer[256];
  bool readFailed = false;
  int readErrno = 0;
  while (true) {
    ssize_t bytesRead = ::read(pipefd[0], buffer, sizeof(buffer));
    if (bytesRead > 0) {
      result.output.append(buffer, static_cast<std::size_t>(bytesRead));
      continue;
    }
    if (bytesRead == 0) {
      break;
    }

    if (errno == EINTR) {
      continue;
    }

    readFailed = true;
    readErrno = errno;
    break;
  }
  ::close(pipefd[0]);

  int status = 0;
  pid_t waitResult = -1;
  do {
    waitResult = ::waitpid(pid, &status, 0);
  } while (waitResult < 0 && errno == EINTR);

  if (waitResult < 0) {
    result.exitCode = 1;
    if (result.output.empty()) {
      result.output = std::string("waitpid failed: ") + std::strerror(errno);
    }
    return result;
  }

  if (readFailed) {
    result.exitCode = 1;
    if (!result.output.empty()) {
      result.output += "\n";
    }
    result.output += std::string("Failed to read command output: ") + std::strerror(readErrno);
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
