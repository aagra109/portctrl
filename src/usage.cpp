#include "usage.h"

#include <iostream>

void usage() {
  std::cout << "portdoctor usage:\n"
            << "  portdoctor who <port>\n"
            << "  portdoctor free <port> [--apply] [--force] [--yes] [--signal INT|TERM]\n"
            << "  portdoctor history [--limit <n>]\n";
}