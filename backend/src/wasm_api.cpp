#include "Simulator.h"
#include <string>

extern "C" {

const char* vmm_run(int totalMemory, const char* strategy, const char* commands) {
    static std::string result;
    const char* strat = strategy ? strategy : "FIRST_FIT";
    const char* cmds = commands ? commands : "";
    result = vmm::runSimulation(totalMemory, strat, cmds);
    return result.c_str();
}

}
