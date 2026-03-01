#include "cli_constants.h"
#include "usage.h"

#include <iostream>

void usage() {
  std::cout << "portctrl usage:\n"
            << "  portctrl " << kCommandWho << " <port>\n"
            << "  portctrl " << kCommandFree
            << " <port> [--apply] [--force] [--yes] [--signal INT|TERM]\n"
            << "  portctrl " << kCommandList << "\n";
}
