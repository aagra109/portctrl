#include <cctype>
#include <cstdio>
#include <iostream>
#include <string>

static void usage() {
  std::cout << "portdoctor usage:\n"
            << "  portdoctor who <port>\n"
            << "  portdoctor free <port> [--force] [--project <name>]\n"
            << "  portdoctor history [--limit <n>]\n";
}

static bool isValidPort(const std::string &text) {
  if (text.empty())
    return false;

  for (char ch : text) {
    if (!std::isdigit(static_cast<unsigned char>(ch)))
      return false;
  }

  int port = std::stoi(text);
  return port >= 1 && port <= 65535;
}

static std::string runCommand(const std::string &command) {
  FILE *pipe = popen(command.c_str(), "r");
  if (pipe == nullptr)
    return "";

  char buffer[256];
  std::string output;

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }

  pclose(pipe);
  return output;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    usage();
    return 1;
  }

  std::string cmd = argv[1];
  if (cmd == "who") {
    if (argc < 3) {
      std::cerr << "Missing port.\n";
      usage();
      return 1;
    }
    std::string port = argv[2];
    if (!isValidPort(port)) {
      std::cerr << "Invalid port: " << port << "\n";
      return 1;
    }

    std::string lsofCommand = "lsof -nP -iTCP:" + port + " -sTCP:LISTEN";
    std::string output = runCommand(lsofCommand);

    if (output.empty()) {
      std::cout << "Port " << port << " appears free (no LISTEN process found).\n";
      return 0;
    }

    std::cout << "Raw lsof output for port " << port << ":\n";
    std::cout << output;
    return 0;
  }

  if (cmd == "free") {
    if (argc < 3) {
      std::cerr << "Missing port.\n";
      usage();
      return 1;
    }
    std::cout << "[stub] cmd=free port=" << argv[2] << "\n";
    return 0;
  }
  if (cmd == "history") {
    std::cout << "[stub] history\n";
    return 0;
  }

  usage();
  return 1;
}
