CXX ?= clang++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=
SRC := $(wildcard src/*.cpp)
TARGET := bin/portctrl
APP_SRC_NO_MAIN := $(filter-out src/main.cpp,$(SRC))
UNIT_TEST_SRC := tests/unit_tests.cpp
UNIT_TEST_BIN := bin/portctrl_unit_tests

.PHONY: all clean install uninstall test-unit

all: $(TARGET)

$(TARGET): $(SRC)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -Isrc $(SRC) -o $(TARGET)

clean:
	rm -rf bin

install: $(TARGET)
	mkdir -p "$(DESTDIR)$(BINDIR)"
	install -m 0755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/portctrl"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/portctrl"

$(UNIT_TEST_BIN): $(APP_SRC_NO_MAIN) $(UNIT_TEST_SRC)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -Isrc $(APP_SRC_NO_MAIN) $(UNIT_TEST_SRC) -o $(UNIT_TEST_BIN)

test-unit: $(UNIT_TEST_BIN)
	./$(UNIT_TEST_BIN)
