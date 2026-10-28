#include "CommandParser.h"
#include <cctype>
#include <sstream>
#include <vector>

// Purpose: Implementation of command parsing and dispatch for simulator commands.

namespace vmm {

namespace {

std::string trim(const std::string& str) {
    std::size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    if (start == str.size()) {
        return "";
    }
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

std::vector<std::string> tokenize(const std::string& str) {
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

bool isDigits(const std::string& str) {
    if (str.empty()) return false;
    for (char c : str) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

} // namespace

bool CommandParser::parseAndExecute(const std::string& rawLine, int lineNumber,
                                    MemoryManager& manager, LogEntry& entry) {
    std::string line = trim(rawLine);
    if (line.empty() || line[0] == '#') {
        return false;
    }

    std::vector<std::string> tokens = tokenize(line);
    if (tokens.empty()) {
        return false;
    }

    entry.line = lineNumber;
    entry.command = line;
    entry.status = CommandStatus::ERROR;
    entry.message = "";
    entry.details.clear();

    std::string cmd = toUpper(tokens[0]);

    if (cmd == "ALLOC") {
        if (tokens.size() != 3) {
            entry.status = CommandStatus::ERROR;
            entry.message = "ALLOC requires 2 arguments: ALLOC <pid> <size>";
            return true;
        }
        std::string pid = tokens[1];
        if (!isDigits(tokens[2])) {
            entry.status = CommandStatus::ERROR;
            entry.message = "Invalid allocation size '" + tokens[2] + "': must be a positive integer";
            return true;
        }
        int size = std::atoi(tokens[2].c_str());
        CommandResult res = manager.allocate(pid, size);
        entry.status = res.status;
        entry.message = res.message;
        entry.details = res.details;
        return true;
    }

    if (cmd == "FREE") {
        if (tokens.size() != 2) {
            entry.status = CommandStatus::ERROR;
            entry.message = "FREE requires 1 argument: FREE <pid>";
            return true;
        }
        CommandResult res = manager.release(tokens[1]);
        entry.status = res.status;
        entry.message = res.message;
        entry.details = res.details;
        return true;
    }

    if (cmd == "COMPACT") {
        if (tokens.size() != 1) {
            entry.status = CommandStatus::ERROR;
            entry.message = "COMPACT takes no arguments";
            return true;
        }
        CommandResult res = manager.compact();
        entry.status = res.status;
        entry.message = res.message;
        entry.details = res.details;
        return true;
    }

    if (cmd == "STRATEGY") {
        if (tokens.size() != 2) {
            entry.status = CommandStatus::ERROR;
            entry.message = "STRATEGY requires strategy name: STRATEGY <FIRST_FIT|BEST_FIT>";
            return true;
        }
        std::string strat = toUpper(tokens[1]);
        if (strat == "FIRST_FIT" || strat == "FIRST") {
            CommandResult res = manager.setStrategy(Strategy::FIRST_FIT);
            entry.status = res.status;
            entry.message = res.message;
            return true;
        } else if (strat == "BEST_FIT" || strat == "BEST") {
            CommandResult res = manager.setStrategy(Strategy::BEST_FIT);
            entry.status = res.status;
            entry.message = res.message;
            return true;
        } else {
            entry.status = CommandStatus::ERROR;
            entry.message = "Unknown strategy '" + tokens[1] + "': use FIRST_FIT or BEST_FIT";
            return true;
        }
    }

    if (cmd == "FIND") {
        if (tokens.size() != 2) {
            entry.status = CommandStatus::ERROR;
            entry.message = "FIND requires 1 argument: FIND <pid>";
            return true;
        }
        CommandResult res = manager.find(tokens[1]);
        entry.status = res.status;
        entry.message = res.message;
        return true;
    }

    if (cmd == "DISPLAY") {
        if (tokens.size() == 1) {
            CommandResult res = manager.display();
            entry.status = res.status;
            entry.message = res.message;
            return true;
        } else if (tokens.size() == 2) {
            std::string sub = toUpper(tokens[1]);
            if (sub == "FREE") {
                CommandResult res = manager.displayFree();
                entry.status = res.status;
                entry.message = res.message;
                return true;
            } else if (sub == "ALLOCATED") {
                CommandResult res = manager.displayAllocated();
                entry.status = res.status;
                entry.message = res.message;
                return true;
            } else {
                entry.status = CommandStatus::ERROR;
                entry.message = "Unknown DISPLAY option '" + tokens[1] + "': use FREE or ALLOCATED";
                return true;
            }
        } else {
            entry.status = CommandStatus::ERROR;
            entry.message = "Too many arguments for DISPLAY";
            return true;
        }
    }

    if (cmd == "FRAG") {
        if (tokens.size() != 1) {
            entry.status = CommandStatus::ERROR;
            entry.message = "FRAG takes no arguments";
            return true;
        }
        CommandResult res = manager.reportFragmentation();
        entry.status = res.status;
        entry.message = res.message;
        return true;
    }

    entry.status = CommandStatus::ERROR;
    entry.message = "Unknown command '" + tokens[0] + "'";
    return true;
}

} // namespace vmm
