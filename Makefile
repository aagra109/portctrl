CXX ?= clang++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=
SRC := $(wildcard src/*.cpp)
TARGET := bin/portctrl

.PHONY: all clean install uninstall

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
