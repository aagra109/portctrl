#include "command_exec.h"

#include <cstdio>
#include <sys/wait.h>

CommandResult runCommand(const std::string &command) {
  CommandResult result{1, ""};

  FILE *pipe = popen((command + " 2>&1").c_str(), "r");
  if (pipe == nullptr) {
    result.output = "Failed to execute command.";
    return result;
  }

  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result.output += buffer;
  }

  int status = pclose(pipe);
  if (status == -1) {
    result.exitCode = 1;
  } else if (WIFEXITED(status)) {
    result.exitCode = WEXITSTATUS(status);
  } else {
    result.exitCode = 1;
  }

  return result;
}
