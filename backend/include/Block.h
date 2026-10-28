#pragma once
#include <string>
#include <vector>

// Purpose: Definitions for Block, Strategy, CommandStatus, WaitingRequest, CommandResult, Stats, and LogEntry.

namespace vmm {

/**
 * Contiguous virtual memory block.
 */
struct Block {
    int start = 0;
    int size = 0;
    bool isFree = true;
    std::string pid = "";
};

/**
 * Memory allocation search strategies.
 */
enum class Strategy {
    FIRST_FIT,
    BEST_FIT
};

/**
 * Execution status for executed commands.
 */
enum class CommandStatus {
    OK,
    QUEUED,
    INFO,
    ERROR
};

/**
 * Request held in the waiting queue when immediate allocation is not possible.
 */
struct WaitingRequest {
    std::string pid;
    int size;
};

/**
 * Result returned by MemoryManager after executing a single command.
 */
struct CommandResult {
    CommandStatus status;
    std::string message;
    std::vector<std::string> details;
};

/**
 * Fragmentation and memory utilization statistics.
 */
struct Stats {
    int totalMemory = 0;
    int usedMemory = 0;
    int freeMemory = 0;
    double utilizationPercent = 0.0;
    int largestFreeBlock = 0;
    int freeBlockCount = 0;
    int allocatedBlockCount = 0;
    double externalFragmentationPercent = 0.0;
    int waitingCount = 0;
};

/**
 * Entry recorded in the command execution log.
 */
struct LogEntry {
    int line = 0;
    std::string command;
    CommandStatus status = CommandStatus::OK;
    std::string message;
    std::vector<std::string> details;
};

inline std::string strategyToString(Strategy strategy) {
    switch (strategy) {
        case Strategy::FIRST_FIT: return "FIRST_FIT";
        case Strategy::BEST_FIT:  return "BEST_FIT";
    }
    return "FIRST_FIT";
}

inline std::string statusToString(CommandStatus status) {
    switch (status) {
        case CommandStatus::OK:     return "OK";
        case CommandStatus::QUEUED: return "QUEUED";
        case CommandStatus::INFO:   return "INFO";
        case CommandStatus::ERROR:  return "ERROR";
    }
    return "OK";
}

} // namespace vmm
