#!/usr/bin/env bash
set -euo pipefail

# Build WebAssembly module from C++ source files using Emscripten
echo "Building wasm/vmm.js with Emscripten..."
mkdir -p wasm

emcc backend/src/Simulator.cpp \
     backend/src/MemoryManager.cpp \
     backend/src/CommandParser.cpp \
     backend/src/JsonWriter.cpp \
     backend/src/wasm_api.cpp \
     -I backend/include -std=c++17 -O2 \
     "-sMODULARIZE=1" "-sEXPORT_NAME=createVMM" "-sENVIRONMENT=node" "-sSINGLE_FILE=1" \
     "-sALLOW_MEMORY_GROWTH=1" \
     "-sEXPORTED_FUNCTIONS=_vmm_run,_malloc,_free" \
     "-sEXPORTED_RUNTIME_METHODS=UTF8ToString,stringToUTF8,lengthBytesUTF8" \
     -o wasm/vmm.js

echo "Successfully built wasm/vmm.js"
