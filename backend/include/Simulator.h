#pragma once
#include <string>

// Purpose: Stateless simulation runner validating inputs, replaying commands, and producing JSON output.

namespace vmm {

/**
 * Runs a deterministic simulation from scratch.
 * Validates configuration, constructs a fresh MemoryManager,
 * replays each command line, and returns JSON formatted string.
 */
std::string runSimulation(int totalMemory, const std::string& strategy, const std::string& script);

} // namespace vmm
