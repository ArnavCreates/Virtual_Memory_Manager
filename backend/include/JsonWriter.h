#pragma once
#include "MemoryManager.h"
#include "Block.h"
#include <string>
#include <vector>

namespace vmm {

class JsonWriter {
public:
    static std::string writeSuccess(const MemoryManager& manager,
                                    const std::vector<LogEntry>& log);
    static std::string writeError(const std::string& errorMessage);
    static std::string escapeJson(const std::string& str);
};

} // namespace vmm
