#include <iostream>
#include <string>

static void usage() {
  std::cout << "portdoctor usage:\n"
            << "  portdoctor who <port>\n"
            << "  portdoctor free <port> [--force] [--project <name>]\n"
            << "  portdoctor history [--limit <n>]\n";
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    usage();
    return 1;
  }

  std::string cmd = argv[1];
  if (cmd == "who" || cmd == "free") {
    if (argc < 3) {
      std::cerr << "Missing port.\n";
      usage();
      return 1;
    }
    std::cout << "[stub] cmd=" << cmd << " port=" << argv[2] << "\n";
    return 0;
  }

  if (cmd == "history") {
    std::cout << "[stub] history\n";
    return 0;
  }

  usage();
  return 1;
}
