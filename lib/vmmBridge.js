// lib/vmmBridge.js
// Shared bridge that loads wasm/vmm.js and executes simulations via vmm_run

const createVMM = require('../wasm/vmm.js');
let modulePromise = null; // Reuse the wasm instance across warm invocations
const getModule = () => (modulePromise ??= createVMM());

function toHeap(M, str) {
  const size = M.lengthBytesUTF8(str) + 1;
  const ptr = M._malloc(size); // Allocate on the wasm heap (not the small stack)
  M.stringToUTF8(str, ptr, size);
  return ptr;
}

async function runSimulation(totalMemory, strategy, commandsText) {
  const M = await getModule();
  const sPtr = toHeap(M, strategy);
  const cPtr = toHeap(M, commandsText);
  try {
    return M.UTF8ToString(M._vmm_run(totalMemory, sPtr, cPtr));
  } finally {
    M._free(sPtr);
    M._free(cPtr);
  }
}

module.exports = { runSimulation };
