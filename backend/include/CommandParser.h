#pragma once
#include "MemoryManager.h"
#include "Block.h"
#include <string>

namespace vmm {

class CommandParser {
public:
    static bool parseAndExecute(const std::string& rawLine, int lineNumber,
                                MemoryManager& manager, LogEntry& entry);
};

} // namespace vmm
