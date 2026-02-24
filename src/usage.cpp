#include "usage.h"

#include <iostream>

void usage() {
  std::cout << "portctrl usage:\n"
            << "  portctrl who <port>\n"
            << "  portctrl free <port> [--apply] [--force] [--yes] [--signal INT|TERM]\n"
            << "  portctrl list\n";
}
