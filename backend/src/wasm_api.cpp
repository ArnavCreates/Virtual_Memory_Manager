#include "Simulator.h"
#include <string>

// Purpose: WebAssembly exported C interface for executing memory manager simulations.

extern "C" {

/**
 * Entry point called from WebAssembly/JavaScript bridge.
 * Executes the simulation and returns a pointer to a null-terminated JSON string.
 */
const char* vmm_run(int totalMemory, const char* strategy, const char* commands) {
    static std::string result; // The single allowed static buffer for returning string to JS
    const char* strat = strategy ? strategy : "FIRST_FIT";
    const char* cmds = commands ? commands : "";
    result = vmm::runSimulation(totalMemory, strat, cmds);
    return result.c_str();
}

}
