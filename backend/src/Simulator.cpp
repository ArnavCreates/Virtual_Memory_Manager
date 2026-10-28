#include "Simulator.h"
#include "MemoryManager.h"
#include "CommandParser.h"
#include "JsonWriter.h"
#include <cctype>
#include <sstream>
#include <vector>

// Purpose: Implementation of simulation replay runner.

namespace vmm {

namespace {

std::string toUpper(const std::string& str) {
    std::string res = str;
    for (char& c : res) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return res;
}

} // namespace

std::string runSimulation(int totalMemory, const std::string& strategyStr, const std::string& script) {
    // Validate totalMemory in [64, 100000]
    if (totalMemory < 64 || totalMemory > 100000) {
        return JsonWriter::writeError("Invalid totalMemory " + std::to_string(totalMemory) +
                                      ": must be between 64 and 100000 units");
    }

    // Validate strategy
    std::string normStrat = toUpper(strategyStr);
    Strategy strategy;
    if (normStrat == "FIRST_FIT" || normStrat == "FIRST") {
        strategy = Strategy::FIRST_FIT;
    } else if (normStrat == "BEST_FIT" || normStrat == "BEST") {
        strategy = Strategy::BEST_FIT;
    } else {
        return JsonWriter::writeError("Invalid strategy '" + strategyStr +
                                      "': must be FIRST_FIT or BEST_FIT");
    }

    // Split script into lines and validate limits
    std::vector<std::string> lines;
    std::istringstream iss(script);
    std::string line;
    while (std::getline(iss, line)) {
        // Strip trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.size() > 100) {
            return JsonWriter::writeError("Command line exceeds maximum length of 100 characters: '" +
                                          line.substr(0, 30) + "...'");
        }
        lines.push_back(line);
        if (lines.size() > 1000) {
            return JsonWriter::writeError("Script exceeds maximum limit of 1000 command lines");
        }
    }

    // Initialize MemoryManager and replay commands
    MemoryManager manager(totalMemory, strategy);
    std::vector<LogEntry> log;
    int executedCommandNumber = 0;

    for (const std::string& cmdLine : lines) {
        LogEntry entry;
        if (CommandParser::parseAndExecute(cmdLine, executedCommandNumber + 1, manager, entry)) {
            ++executedCommandNumber;
            log.push_back(entry);
        }
    }

    return JsonWriter::writeSuccess(manager, log);
}

} // namespace vmm
