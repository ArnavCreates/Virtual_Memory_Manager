CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude

ifeq ($(OS),Windows_NT)
    SAN_FLAGS =
    BIN_EXT = .exe
    RM = del /Q /F 2>nul || rm -f
    MKDIR = if not exist build mkdir build
else
    SAN_FLAGS = -fsanitize=address,undefined
    BIN_EXT =
    RM = rm -rf
    MKDIR = mkdir -p build
endif

SRC_FILES = src/MemoryManager.cpp src/CommandParser.cpp src/JsonWriter.cpp src/Simulator.cpp
CLI_SRC   = $(SRC_FILES) src/main.cpp
TEST_SRC  = $(SRC_FILES) tests/test_main.cpp

CLI_BIN  = build/vmm$(BIN_EXT)
TEST_BIN = build/test_runner$(BIN_EXT)

.PHONY: all cli test clean

all: cli test

cli: $(CLI_BIN)

test: $(TEST_BIN)
	$(TEST_BIN)

$(CLI_BIN): $(CLI_SRC)
	@$(MKDIR)
	$(CXX) $(CXXFLAGS) $(CLI_SRC) -o $(CLI_BIN)

$(TEST_BIN): $(TEST_SRC)
	@$(MKDIR)
	$(CXX) $(CXXFLAGS) $(SAN_FLAGS) $(TEST_SRC) -o $(TEST_BIN)

clean:
	$(RM) $(CLI_BIN) $(TEST_BIN) build/*.o
