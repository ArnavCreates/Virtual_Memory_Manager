#pragma once
#include "MemoryManager.h"
#include "Block.h"
#include <string>
#include <vector>

// Purpose: Hand-written JSON serializer for simulation state, configuration, data structures, and logs.

namespace vmm {

class JsonWriter {
public:
    /**
     * Serializes a successful simulation run to JSON.
     */
    static std::string writeSuccess(const MemoryManager& manager,
                                    const std::vector<LogEntry>& log);

    /**
     * Serializes an error response to JSON.
     */
    static std::string writeError(const std::string& errorMessage);

    /**
     * Escapes special characters for valid JSON strings.
     */
    static std::string escapeJson(const std::string& str);
};

} // namespace vmm
