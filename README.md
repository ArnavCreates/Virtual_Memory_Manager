# Virtual Memory Manager

A dynamic memory allocation simulator implementing contiguous memory management algorithms, written in C++17 and compiled to WebAssembly with an interactive web visualizer and a standalone command-line interface.

[![CI](https://github.com/ArnavCreates/Virtual_Memory_Manager/actions/workflows/ci.yml/badge.svg)](https://github.com/ArnavCreates/Virtual_Memory_Manager/actions/workflows/ci.yml)
[![Backend: C++17](https://img.shields.io/badge/backend-C%2B%2B17-blue.svg)](backend/)
[![WebAssembly](https://img.shields.io/badge/runtime-WebAssembly-purple.svg)](wasm/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**Live Demo:** [https://virtualmemorymanager.vercel.app/](https://virtualmemorymanager.vercel.app/)

---

## Overview

This project simulates contiguous dynamic memory allocation similar to OS kernel heap managers. It models how memory is allocated, split, coalesced, and compacted under different allocation strategies (**First Fit** and **Best Fit**), while managing pending requests through a waiting queue.

> **Note:** This project focuses on contiguous variable-partition dynamic allocation (heap allocation, block splitting, free list coalescing, and compaction) rather than virtual memory paging or TLB management.

---

## Features

- **Custom Data Structures**: All core data structures (Doubly Linked List, FIFO Queue, Separate-Chaining Hash Table) are built from scratch with explicit pointer manipulation without relying on STL container classes (`std::list`, `std::queue`, `std::unordered_map`).
- **Contiguous Allocation Strategies**:
  - **First Fit**: Scans memory from address 0 and places the process in the first free block that is large enough.
  - **Best Fit**: Evaluates all free holes and selects the smallest hole that fits the request, minimizing leftover space. Breaks ties by lowest starting address.
  - **Block Splitting**: Exact fits allocate the block as-is; larger blocks are cleanly split into an allocated segment and a remaining free block.
- **Immediate Free Coalescing**: When a block is freed, any adjacent free blocks (left, right, or both) are merged in $O(1)$ time to prevent memory fragmentation.
- **Memory Compaction**: Shifts allocated blocks toward address 0, updates bounds while preserving node addresses, merges free fragments into a single contiguous block at the end, and retries queued requests.
- **FIFO Waiting Queue**: Requests that cannot be satisfied due to lack of contiguous memory are held in a waiting queue and automatically retried when memory is released or compacted.
- **O(1) Process Lookup**: Uses a separate-chaining hash table with the djb2 algorithm to map process IDs to block pointers for instant lookup (`FIND <pid>`).
- **Interactive Visualizer & Native CLI**:
  - Interactive web interface displaying real-time memory blocks, data structure node states, fragmentation stats, and a side-by-side strategy comparison view.
  - Standalone native C++ CLI with formatted ASCII table output.

---

## System Architecture

The web interface uses a stateless replay design. Since serverless deployments on platforms like Vercel do not hold persistent process memory across requests, each action replays the command sequence to produce the resulting state deterministically:

```
 Browser (public/: HTML + CSS + JS)
        |   POST /api/simulate  { totalMemory, strategy, commands[] }
        v
 Vercel Serverless Function (api/simulate.js, Node.js bridge)
        |   invokes exported C function vmm_run(...)
        v
 WebAssembly Module (wasm/vmm.js, compiled from C++17 via Emscripten)
        |   Simulator -> CommandParser -> MemoryManager
        |        uses: DoublyLinkedList (blocks), HashTable (pid -> block), Queue (waiting)
        v
 JSON Response (blocks, waitingQueue, hashTable, stats, log) -> UI Renders
```

---

## Custom Data Structures

| Data Structure | Source | Role | Details |
|---|---|---|---|
| **Doubly Linked List** | [`DoublyLinkedList.h`](backend/include/DoublyLinkedList.h) | Tracks memory blocks in address order | Enables $O(1)$ block splitting, insertion, and neighbor coalescing without full list reallocations. |
| **FIFO Queue** | [`Queue.h`](backend/include/Queue.h) | Holds pending allocation requests | Singly linked queue with head and tail pointers ensuring $O(1)$ enqueue and dequeue operations. |
| **Hash Table** | [`HashTable.h`](backend/include/HashTable.h) | Maps process ID (`std::string`) to block node pointer | Uses 17 prime buckets and the djb2 hash algorithm with separate chaining for collision resolution. |

---

## Memory Management Logic

### 1. Allocation & Splitting
- **First Fit**: Iterates through the block list and picks the first free block where `size >= requestedSize`.
- **Best Fit**: Evaluates all free blocks and selects the one with the smallest difference `(blockSize - requestedSize)`.
- **Splitting**: When a selected free block is larger than the requested size, its size is adjusted to the requested amount and a new free block representing the remainder is inserted immediately after it.

### 2. Deallocation & Coalescing
- When a process is released, its node pointer is resolved via the hash table.
- The block is marked as free.
- If the succeeding block is free, it is merged into the current block.
- If the preceding block is free, the current block is merged into the predecessor.
- After coalescing, waiting requests are retried in arrival order.

### 3. Queue Handling & Retry
- When an allocation fails due to insufficient contiguous memory, the request enters the FIFO queue.
- Whenever memory is released (`FREE`) or compacted (`COMPACT`), queued requests are evaluated in a single pass. Requests that fit are allocated, while those that still do not fit remain queued.

### 4. Compaction
- Compaction shifts all allocated blocks toward the beginning of memory (starting at address 0), consolidating all fragmented free blocks into a single block at the end.
- If memory is already contiguous and free space is already at the end, compaction recognizes this and avoids unnecessary block movements.

---

## Command Reference

Command keywords are case-insensitive. Process IDs can be up to 16 alphanumeric characters.

| Command | Syntax | Description |
|---|---|---|
| `ALLOC` | `ALLOC <pid> <size>` | Allocates `size` units for process `<pid>` or queues if unavailable |
| `FREE` | `FREE <pid>` | Releases process memory (with immediate coalescing) or cancels waiting request |
| `COMPACT` | `COMPACT` | Consolidates all allocated blocks toward address 0 and merges all free space |
| `STRATEGY` | `STRATEGY <FIRST_FIT \| BEST_FIT>` | Changes the active allocation algorithm mid-simulation |
| `FIND` | `FIND <pid>` | Looks up process location and hash table bucket |
| `DISPLAY` | `DISPLAY` | Prints all memory blocks |
| `DISPLAY FREE` | `DISPLAY FREE` | Summarizes free blocks only |
| `DISPLAY ALLOCATED` | `DISPLAY ALLOCATED` | Summarizes allocated blocks only |
| `FRAG` | `FRAG` | Displays current external fragmentation percentage |
| `RESET` | `RESET` | *(CLI)* Resets memory to initial state |
| `HELP` | `HELP` | *(CLI)* Lists available commands |
| `EXIT` | `EXIT` / `QUIT` | *(CLI)* Exits the simulator |

---

## Building and Running

### Native C++ CLI & Test Suite

The C++ backend has zero external dependencies and compiles with any C++17 compiler (GCC, Clang, MSVC).

```bash
# Build and run the test suite
make -C backend test
# (On Windows with MinGW)
mingw32-make -C backend test

# Build the interactive CLI
make -C backend cli
# (On Windows with MinGW)
mingw32-make -C backend cli

# Run interactive CLI
./backend/build/vmm 1000 FIRST_FIT
```

Sample CLI output:
```
============================================================
    Virtual Memory Manager Simulator (C++17 Native CLI)     
============================================================
Total Memory: 1000 units | Strategy: FIRST_FIT
Type 'HELP' for available commands, 'EXIT' to quit.

ALLOC P1 200
[OK] P1 allocated 200 units at 0-199 (First Fit)
+-------+-----------------+---------+-------------+---------+
| Index | Start .. End    | Size    | Status      | Process |
+-------+-----------------+---------+-------------+---------+
|     0 |        0 .. 199 |     200 |   ALLOCATED |      P1 |
|     1 |      200 .. 999 |     800 |        FREE |       - |
+-------+-----------------+---------+-------------+---------+
Memory: Used 200/1000 (20.00%), Free 800 (Largest 800, Frag 0.00%)
Waiting Queue (0): [empty]
Hash Table (1 allocated processes indexed in 17 buckets)
```

Commands can also be piped in batch:
```bash
./backend/build/vmm < script.txt
```

---

### WebAssembly Build (Optional)

The WebAssembly binary is pre-compiled in `wasm/vmm.js`. To recompile using Emscripten:

```bash
# Using npm script:
npm run build:wasm

# Or using the build script directly:
bash backend/build_wasm.sh        # Linux / macOS
backend\build_wasm.bat            # Windows
```

---

### Local Web Development

```bash
# Run smoke tests
npm run smoke

# Start local dev server (using Vercel CLI)
npx vercel dev
```
Open `http://localhost:3000` in your browser.

---

## Project Structure

```
Virtual_Memory_Manager/
├── backend/
│   ├── include/
│   │   ├── Block.h             # Memory block, strategy, and stats structures
│   │   ├── CommandParser.h     # Command parsing logic
│   │   ├── DoublyLinkedList.h  # Custom doubly linked list template
│   │   ├── HashTable.h         # Custom separate-chaining hash table template
│   │   ├── JsonWriter.h        # JSON serialization for simulation results
│   │   ├── MemoryManager.h     # Core memory manager implementation
│   │   ├── Queue.h             # Custom FIFO queue template
│   │   └── Simulator.h         # Simulation replay coordinator
│   ├── src/
│   │   ├── CommandParser.cpp
│   │   ├── JsonWriter.cpp
│   │   ├── main.cpp            # Native CLI entry point
│   │   ├── MemoryManager.cpp
│   │   ├── Simulator.cpp
│   │   └── wasm_api.cpp        # WebAssembly export interface
│   ├── tests/
│   │   └── test_main.cpp       # Unit & integration test suite
│   ├── Makefile
│   ├── build_wasm.bat
│   └── build_wasm.sh
├── api/
│   └── simulate.js             # Vercel serverless function entry point
├── lib/
│   └── vmmBridge.js            # Node.js to WebAssembly bridge
├── public/
│   ├── app.js                  # Frontend visualizer logic
│   ├── index.html              # Web interface markup
│   └── style.css               # Styling (dark & light theme support)
├── scripts/
│   └── smoke-test.js           # Node.js test script for Wasm output
├── wasm/
│   └── vmm.js                  # Compiled WebAssembly bundle
├── package.json
└── README.md
```

---

## Course Details

- **Project:** Virtual Memory Manager Simulator
- **Course:** Data Structures & Algorithms Mini Project
- **Author:** Arnav ([@ArnavCreates](https://github.com/ArnavCreates))
- **License:** MIT
