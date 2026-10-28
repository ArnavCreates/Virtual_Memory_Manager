#include "MemoryManager.h"
#include "CommandParser.h"
#include "Block.h"
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Purpose: Native interactive command-line simulator with human-readable ASCII output.

namespace {

std::string trim(const std::string& str) {
    std::size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    if (start == str.size()) return "";
    std::size_t end = str.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(str[end]))) {
        --end;
    }
    return str.substr(start, end - start + 1);
}

std::string toUpper(const std::string& str) {
    std::string res = str;
    for (char& c : res) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return res;
}

void printHelp() {
    std::cout << "\n================ Virtual Memory Manager CLI Commands ================\n";
    std::cout << "  ALLOC <pid> <size>     - Allocate memory block for process\n";
    std::cout << "  FREE <pid>             - Release memory or cancel waiting request\n";
    std::cout << "  COMPACT                - Coalesce all free blocks together\n";
    std::cout << "  STRATEGY <name>        - Change strategy (FIRST_FIT or BEST_FIT)\n";
    std::cout << "  FIND <pid>             - O(1) hash table lookup for process block\n";
    std::cout << "  DISPLAY                - Display full memory block table & stats\n";
    std::cout << "  DISPLAY ALLOCATED      - Display allocated blocks only\n";
    std::cout << "  DISPLAY FREE           - Display free blocks only\n";
    std::cout << "  FRAG                   - Display external fragmentation report\n";
    std::cout << "  RESET                  - Reset memory to initial empty state\n";
    std::cout << "  HELP                   - Show this help message\n";
    std::cout << "  EXIT / QUIT            - Exit program\n";
    std::cout << "======================================================================\n\n";
}

void printBlockTable(const vmm::MemoryManager& mgr, int filterMode /* 0: all, 1: allocated only, 2: free only */) {
    std::cout << "+-------+-----------------+---------+-------------+---------+\n";
    std::cout << "| Index | Start .. End    | Size    | Status      | Process |\n";
    std::cout << "+-------+-----------------+---------+-------------+---------+\n";

    const auto* node = mgr.blocks().head();
    int idx = 0;
    int matchedCount = 0;

    while (node != nullptr) {
        bool show = true;
        if (filterMode == 1 && node->data.isFree) show = false;
        if (filterMode == 2 && !node->data.isFree) show = false;

        if (show) {
            ++matchedCount;
            int start = node->data.start;
            int end = start + node->data.size - 1;
            std::string range = std::to_string(start) + " .. " + std::to_string(end);
            std::string status = node->data.isFree ? "FREE" : "ALLOCATED";
            std::string owner = node->data.isFree ? "-" : node->data.pid;

            std::cout << "| " << std::setw(5) << idx << " | "
                      << std::setw(15) << range << " | "
                      << std::setw(7) << node->data.size << " | "
                      << std::setw(11) << status << " | "
                      << std::setw(7) << owner << " |\n";
        }
        node = node->next;
        ++idx;
    }

    std::cout << "+-------+-----------------+---------+-------------+---------+\n";
    if (matchedCount == 0) {
        std::cout << "  (No blocks matched filter)\n";
    }

    // Print summary stats and data structures
    vmm::Stats s = mgr.getStats();
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Memory: Used " << s.usedMemory << "/" << s.totalMemory
              << " (" << s.utilizationPercent << "%), Free " << s.freeMemory
              << " (Largest " << s.largestFreeBlock << ", Frag " << s.externalFragmentationPercent << "%)\n";

    // Waiting queue summary
    std::cout << "Waiting Queue (" << s.waitingCount << "): ";
    if (s.waitingCount == 0) {
        std::cout << "[empty]\n";
    } else {
        bool first = true;
        mgr.waitingQueue().forEach([&](const vmm::WaitingRequest& req) {
            if (!first) std::cout << ", ";
            std::cout << req.pid << " (" << req.size << " units)";
            first = false;
        });
        std::cout << "\n";
    }

    // Hash table summary
    std::cout << "Hash Table (" << mgr.processTable().size() << " allocated processes indexed in 17 buckets)\n";
    std::cout << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    int totalMemory = 1000;
    vmm::Strategy strategy = vmm::Strategy::FIRST_FIT;

    if (argc >= 2) {
        int parsed = std::atoi(argv[1]);
        if (parsed >= 64 && parsed <= 100000) {
            totalMemory = parsed;
        } else {
            std::cerr << "Warning: Invalid totalMemory '" << argv[1]
                      << "'. Using default 1000.\n";
        }
    }
    if (argc >= 3) {
        std::string s = toUpper(argv[2]);
        if (s == "BEST_FIT" || s == "BEST") {
            strategy = vmm::Strategy::BEST_FIT;
        } else if (s == "FIRST_FIT" || s == "FIRST") {
            strategy = vmm::Strategy::FIRST_FIT;
        } else {
            std::cerr << "Warning: Unknown strategy '" << argv[2]
                      << "'. Using default FIRST_FIT.\n";
        }
    }

    vmm::MemoryManager manager(totalMemory, strategy);

    std::cout << "============================================================\n";
    std::cout << "    Virtual Memory Manager Simulator (C++17 Native CLI)     \n";
    std::cout << "============================================================\n";
    std::cout << "Total Memory: " << totalMemory << " units | Strategy: "
              << vmm::strategyToString(strategy) << "\n";
    std::cout << "Type 'HELP' for available commands, 'EXIT' to quit.\n\n";

    std::string line;
    int commandLineNumber = 0;

    while (std::getline(std::cin, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::string upperCmd = toUpper(trimmed);
        if (upperCmd == "EXIT" || upperCmd == "QUIT") {
            std::cout << "Exiting Virtual Memory Manager. Goodbye!\n";
            return 0;
        }
        if (upperCmd == "HELP") {
            printHelp();
            continue;
        }
        if (upperCmd == "RESET") {
            manager.reset();
            std::cout << "[OK] Memory manager reset to initial state (1000 free units).\n\n";
            printBlockTable(manager, 0);
            continue;
        }

        ++commandLineNumber;
        vmm::LogEntry entry;
        bool executed = vmm::CommandParser::parseAndExecute(trimmed, commandLineNumber, manager, entry);
        if (!executed) {
            continue;
        }

        // Print status header
        std::cout << "[" << vmm::statusToString(entry.status) << "] " << entry.message << "\n";
        for (const std::string& detail : entry.details) {
            std::cout << "       -> " << detail << "\n";
        }

        // Print updated memory views
        if (upperCmd.rfind("DISPLAY FREE", 0) == 0) {
            printBlockTable(manager, 2);
        } else if (upperCmd.rfind("DISPLAY ALLOCATED", 0) == 0) {
            printBlockTable(manager, 1);
        } else if (upperCmd.rfind("DISPLAY", 0) == 0 ||
                   upperCmd.rfind("ALLOC", 0) == 0 ||
                   upperCmd.rfind("FREE", 0) == 0 ||
                   upperCmd.rfind("COMPACT", 0) == 0) {
            printBlockTable(manager, 0);
        } else {
            std::cout << "\n";
        }
    }

    return 0;
}
