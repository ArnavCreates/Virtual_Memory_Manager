#pragma once
#include "MemoryManager.h"
#include "Block.h"
#include <string>

// Purpose: Parser and dispatcher for simulator commands (ALLOC, FREE, COMPACT, STRATEGY, FIND, DISPLAY, FRAG).

namespace vmm {

class CommandParser {
public:
    /**
     * Parses a single command line, executes it against the MemoryManager,
     * and populates the LogEntry.
     * Returns true if a command was executed (producing a log entry),
     * or false if the line was blank or a comment and should be ignored.
     */
    static bool parseAndExecute(const std::string& rawLine, int lineNumber,
                                MemoryManager& manager, LogEntry& entry);
};

} // namespace vmm
