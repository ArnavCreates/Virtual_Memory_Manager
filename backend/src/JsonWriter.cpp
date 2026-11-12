#include "JsonWriter.h"
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace vmm {

std::string JsonWriter::escapeJson(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b";  break;
            case '\f': oss << "\\f";  break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    oss << buf;
                } else {
                    oss << c;
                }
                break;
        }
    }
    return oss.str();
}

std::string JsonWriter::writeError(const std::string& errorMessage) {
    std::ostringstream oss;
    oss << "{\"ok\":false,\"error\":\"" << escapeJson(errorMessage) << "\"}";
    return oss.str();
}

std::string JsonWriter::writeSuccess(const MemoryManager& manager,
                                    const std::vector<LogEntry>& log) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    oss << "{\n";
    oss << "  \"ok\": true,\n";

    // Configuration
    oss << "  \"config\": {\n";
    oss << "    \"totalMemory\": " << manager.totalMemory() << ",\n";
    oss << "    \"strategy\": \"" << strategyToString(manager.strategy()) << "\"\n";
    oss << "  },\n";

    // Memory blocks
    oss << "  \"blocks\": [\n";
    const ListNode<Block>* node = manager.blocks().head();
    int idx = 0;
    while (node != nullptr) {
        if (idx > 0) oss << ",\n";
        int start = node->data.start;
        int end = start + node->data.size - 1;
        oss << "    {\n";
        oss << "      \"index\": " << idx << ",\n";
        oss << "      \"start\": " << start << ",\n";
        oss << "      \"end\": " << end << ",\n";
        oss << "      \"size\": " << node->data.size << ",\n";
        oss << "      \"status\": \"" << (node->data.isFree ? "FREE" : "ALLOCATED") << "\",\n";
        if (node->data.isFree) {
            oss << "      \"pid\": null,\n";
        } else {
            oss << "      \"pid\": \"" << escapeJson(node->data.pid) << "\",\n";
        }
        if (node->prev != nullptr) {
            oss << "      \"prevStart\": " << node->prev->data.start << ",\n";
        } else {
            oss << "      \"prevStart\": null,\n";
        }
        if (node->next != nullptr) {
            oss << "      \"nextStart\": " << node->next->data.start << "\n";
        } else {
            oss << "      \"nextStart\": null\n";
        }
        oss << "    }";
        node = node->next;
        ++idx;
    }
    oss << "\n  ],\n";

    // Waiting queue
    oss << "  \"waitingQueue\": [\n";
    int queuePos = 1;
    bool firstQueue = true;
    manager.waitingQueue().forEach([&](const WaitingRequest& req) {
        if (!firstQueue) oss << ",\n";
        oss << "    {\n";
        oss << "      \"position\": " << queuePos << ",\n";
        oss << "      \"pid\": \"" << escapeJson(req.pid) << "\",\n";
        oss << "      \"size\": " << req.size << "\n";
        oss << "    }";
        firstQueue = false;
        ++queuePos;
    });
    oss << "\n  ],\n";

    // Process hash table
    const auto& table = manager.processTable();
    oss << "  \"hashTable\": {\n";
    oss << "    \"bucketCount\": " << table.bucketCount() << ",\n";
    oss << "    \"entryCount\": " << table.size() << ",\n";
    oss << "    \"buckets\": [\n";
    for (std::size_t b = 0; b < table.bucketCount(); ++b) {
        if (b > 0) oss << ",\n";
        oss << "      { \"index\": " << b << ", \"keys\": [";
        std::vector<std::string> keys = table.keysInBucket(b);
        for (std::size_t k = 0; k < keys.size(); ++k) {
            if (k > 0) oss << ", ";
            oss << "\"" << escapeJson(keys[k]) << "\"";
        }
        oss << "] }";
    }
    oss << "\n    ]\n";
    oss << "  },\n";

    // Statistics
    Stats s = manager.getStats();
    oss << "  \"stats\": {\n";
    oss << "    \"totalMemory\": " << s.totalMemory << ",\n";
    oss << "    \"usedMemory\": " << s.usedMemory << ",\n";
    oss << "    \"freeMemory\": " << s.freeMemory << ",\n";
    oss << "    \"utilizationPercent\": " << s.utilizationPercent << ",\n";
    oss << "    \"largestFreeBlock\": " << s.largestFreeBlock << ",\n";
    oss << "    \"freeBlockCount\": " << s.freeBlockCount << ",\n";
    oss << "    \"allocatedBlockCount\": " << s.allocatedBlockCount << ",\n";
    oss << "    \"externalFragmentationPercent\": " << s.externalFragmentationPercent << ",\n";
    oss << "    \"waitingCount\": " << s.waitingCount << "\n";
    oss << "  },\n";

    // Execution log
    oss << "  \"log\": [\n";
    for (std::size_t l = 0; l < log.size(); ++l) {
        if (l > 0) oss << ",\n";
        const LogEntry& entry = log[l];
        oss << "    {\n";
        oss << "      \"line\": " << entry.line << ",\n";
        oss << "      \"command\": \"" << escapeJson(entry.command) << "\",\n";
        oss << "      \"status\": \"" << statusToString(entry.status) << "\",\n";
        oss << "      \"message\": \"" << escapeJson(entry.message) << "\",\n";
        oss << "      \"details\": [";
        for (std::size_t d = 0; d < entry.details.size(); ++d) {
            if (d > 0) oss << ", ";
            oss << "\"" << escapeJson(entry.details[d]) << "\"";
        }
        oss << "]\n";
        oss << "    }";
    }
    oss << "\n  ]\n";

    oss << "}";
    return oss.str();
}

} // namespace vmm
