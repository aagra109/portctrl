CXX ?= clang++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
SRC := $(wildcard src/*.cpp)
TARGET := bin/portdoctor

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -Isrc $(SRC) -o $(TARGET)

clean:
	rm -rf bin

