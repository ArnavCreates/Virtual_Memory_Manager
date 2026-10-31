# Virtual Memory Manager: A Dynamic Memory Allocation Simulator Using Linked Lists, Queues and Hash Tables

[![CI](https://github.com/ArnavCreates/Virtual_Memory_Manager/actions/workflows/ci.yml/badge.svg)](https://github.com/ArnavCreates/Virtual_Memory_Manager/actions/workflows/ci.yml)
[![Backend: C++17](https://img.shields.io/badge/backend-C%2B%2B17-blue.svg)](backend/)
[![WebAssembly](https://img.shields.io/badge/runtime-WebAssembly-purple.svg)](wasm/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

> **Live Deployment:** [https://virtualmemorymanager.vercel.app/](https://virtualmemorymanager.vercel.app/) *(Deploy on Vercel)*  
> **UI Preview:**  
> *(Screenshot placeholder: Run the web UI locally or see live deployment)*

---

## 1. Scope Note

This simulator models **contiguous dynamic memory allocation** as used in operating system kernels and heap managers (First Fit, Best Fit, block splitting, adjacent free block coalescing, memory compaction, and waiting queue management). It **does not** implement virtual memory paging, page tables, TLBs, or page replacement algorithms.

---

## 2. Project Features

- **Custom C++17 Data Structures**: Hand-crafted Doubly Linked List, FIFO Queue, and Separate-Chaining Hash Table (no forbidden STL containers like `std::list`, `std::queue`, or `std::unordered_map`).
- **Dynamic Contiguous Allocation**:
  - **First Fit**: Scans from memory address 0 and selects the first free hole large enough.
  - **Best Fit**: Scans all free holes and selects the smallest hole that satisfies the request, minimizing wasted remainder. Breaks ties in favor of lower start addresses.
  - **Block Splitting**: Exact fits allocate the block intact; larger holes are cleanly split into an allocated block and a free remainder.
- **Immediate Coalescing**: On `FREE`, adjacent free neighbors (both left and right) are merged in O(1) time to prevent premature fragmentation.
- **Memory Compaction**: Shifts all allocated blocks towards address 0, updates block bounds without invalidating node pointers, combines all free space into one trailing block at the end, and retries queued requests.
- **FIFO Waiting Queue**: Requests that exceed available contiguous free memory are queued and automatically retried upon `FREE` or `COMPACT`.
- **Fast O(1) Process Lookup**: djb2 hash table with 17 buckets maps process IDs directly to their linked-list node pointers for instant lookup (`FIND <pid>`).
- **Interactive Multi-Platform Support**:
  - **Native Interactive CLI**: Standalone C++ executable with aligned ASCII tables and status reports.
  - **Web Visualizer**: Responsive HTML/CSS/JavaScript interface running via WebAssembly in a Vercel Serverless Function.
- **Side-by-Side Comparison Mode**: Runs the same command sequence simultaneously under First Fit and Best Fit to compare fragmentation and utilization.

---

## 3. Architecture

The simulator uses a **stateless replay architecture**. Serverless functions on Vercel retain no persistent memory between HTTP requests. To ensure determinism and allow instantaneous "Undo" and "Reset":

```
 Browser (public/: HTML + CSS + JS)
        |   POST /api/simulate  { totalMemory, strategy, commands[] }
        v
 Vercel Function (api/simulate.js, Node.js)   <- Thin bridge, validates input only
        |   calls exported C function vmm_run(...)
        v
 WebAssembly Module (wasm/vmm.js, compiled from C++17)
        |   Simulator -> CommandParser -> MemoryManager
        |        uses: DoublyLinkedList (blocks), HashTable (pid -> block), Queue (waiting)
        v
 JSON Result (blocks, waitingQueue, hashTable, stats, log)  ->  Browser renders live state
```

### Stateless Replay Design
- The client maintains `totalMemory`, `initialStrategy`, and the ordered list of executed `commands[]`.
- For each user action, the entire command history is transmitted to `/api/simulate`.
- The C++ backend constructs a clean `MemoryManager`, validates configuration, sequentially replays all commands, and outputs the complete resulting state and execution log as a single JSON response.
- Mid-session strategy changes append `STRATEGY <name>` to the history, ensuring prior allocations are not retroactively modified.

---

## 4. Custom Data Structures

Every core data structure is implemented by hand in `backend/include/` with explicit pointer management:

| Data Structure | Header File | Purpose in Simulator | Why Chosen & Key Complexity |
|---|---|---|---|
| **Doubly Linked List** | [`DoublyLinkedList.h`](backend/include/DoublyLinkedList.h) | Tracks contiguous memory blocks in address order | Exposes node pointers so `MemoryManager` can splice, split, and merge (`coalesce`) neighbor blocks in O(1) time without linear scanning. |
| **FIFO Queue** | [`Queue.h`](backend/include/Queue.h) | Tracks processes waiting for memory to become available | Singly linked nodes with head and tail pointers ensure strict O(1) FIFO `enqueue` at tail and `dequeue` at head. Preserves request arrival order. |
| **Hash Table** | [`HashTable.h`](backend/include/HashTable.h) | Maps process ID string (`std::string`) to `ListNode<Block>*` | Provides O(1) average-time lookup for `FIND <pid>` and release operations. Uses **17 fixed buckets** (prime number) and **djb2 hashing** with separate chaining to guarantee collision resolution. |

*(All STL containers for these three roles—such as `std::list`, `std::deque`, `std::map`, and `std::unordered_map`—are strictly avoided as mandated by requirement R1).*

---

## 5. Algorithms & Trade-offs

### 5.1 Allocation & Splitting
- **First Fit**: Traverses `blocks_` from head; returns the first node where `node->data.isFree && node->data.size >= request`.
- **Best Fit**: Traverses all blocks; locates the free node minimizing `node->data.size - request`. Ties are broken by lowest starting address.
- **Splitting**: If `node->data.size == request`, the block status is toggled to allocated. If `node->data.size > request`, `node->data.size` is shrunk to `request`, and a new free block of size `oldSize - request` is inserted immediately after it via `insertAfter`.

### 5.2 Release & Coalescing
- The process block pointer is retrieved from the Hash Table in O(1) average time, and its entry is removed.
- The block is marked free (`pid = ""`).
- **Coalescing**:
  - If `node->next` is free: `node->size += next->size`, and `next` is unlinked and deleted via `blocks_.erase(next)`.
  - If `node->prev` is free: `prev->size += node->size`, and `node` is unlinked and deleted via `blocks_.erase(node)`.

### 5.3 Waiting Queue Retry Policy
- When memory is freed or compacted, `retryWaitingQueue()` performs a **single-pass rotation**:
  ```
  n = waitingQueue_.size()
  repeat n times:
      req = dequeue()
      if findBlock(req.size) matches:
          placeInBlock(block, req.pid, req.size)
      else:
          enqueue(req)  // returns to back of queue
  ```
- **Trade-off Note**: This policy allows smaller, later requests to proceed if an earlier large request cannot yet fit. In extreme scenarios, a very large process can experience starvation if memory continuously frees only in small increments.

### 5.4 Compaction
- If all allocated blocks are already grouped at address 0 and free space is unified at the end, compaction returns `INFO` "Memory is already compact".
- Otherwise, `blocks_` is traversed with `address = 0`. Allocated blocks have `node->data.start` adjusted to `address` in place (preserving node addresses for hash table pointers). All existing free blocks are erased, and a single consolidated free block of size `totalFree` is pushed to the tail.

---

## 6. Command Reference

Keywords are case-insensitive. Process IDs are normalized to uppercase (`[A-Za-z][A-Za-z0-9_]{0,15}`).

| Command | Arguments | Description | Log Status |
|---|---|---|---|
| `ALLOC` | `<pid> <size>` | Request `size` units for process `<pid>`. Allocates immediately or enqueues. | `OK` or `QUEUED` |
| `FREE` | `<pid>` | Deallocates process memory (coalescing free neighbors) or cancels waiting request. | `OK` or `ERROR` |
| `COMPACT` | *(none)* | Relocates allocated blocks to address 0 and merges all free space. | `OK` or `INFO` |
| `STRATEGY` | `<FIRST_FIT \| BEST_FIT>` | Switches the active allocation algorithm mid-simulation. | `OK` |
| `FIND` | `<pid>` | Performs O(1) hash table lookup to display bucket index and block bounds. | `INFO` or `ERROR` |
| `DISPLAY` | *(none)* | Summarizes full memory layout (all blocks). | `INFO` |
| `DISPLAY FREE` | *(none)* | Summarizes only free blocks with counts and sizes. | `INFO` |
| `DISPLAY ALLOCATED`| *(none)* | Summarizes only allocated blocks with owner PIDs. | `INFO` |
| `FRAG` | *(none)* | Computes external fragmentation percentage: $\left(1 - \frac{\text{largestFree}}{\text{totalFree}}\right) \times 100$. | `INFO` |
| `HELP` | *(none)* | *(CLI only)* Prints command usage instructions. | - |
| `RESET` | *(none)* | *(CLI only)* Clears memory back to 1000 free units. | - |
| `EXIT` / `QUIT` | *(none)* | *(CLI only)* Exits CLI program with code 0. | - |

---

## 7. Running Natively (C++17 CLI & Tests)

The C++ backend requires no web tools, node, or Emscripten to build and run natively.

### 7.1 Build & Run Tests
```bash
# On Linux / macOS (builds with -fsanitize=address,undefined):
make -C backend test

# On Windows (MinGW / GCC):
mingw32-make -C backend test
```
All 12 required test scenarios (T1 through T12), including data structure checks, coalescing, queue retry, error handling, mid-script strategy switching, and 10,000-command stress testing, will run and report pass counts:
```
============================================================
Tests Summary: 497 passed, 0 failed.
============================================================
```

### 7.2 Build & Run Interactive CLI
```bash
# Compile CLI binary
make -C backend cli           # Linux / macOS -> backend/build/vmm
mingw32-make -C backend cli   # Windows -> backend/build/vmm.exe

# Run interactive session
./backend/build/vmm 1000 FIRST_FIT
```

### 7.3 Sample Interactive Session
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

Scripts can also be piped directly:
```bash
./backend/build/vmm < script.txt
```

---

## 8. WebAssembly Compilation (Emscripten)

The WebAssembly module is pre-compiled into `wasm/vmm.js` and committed to git. If you wish to recompile the C++ source files to WebAssembly:

1. **Install Emscripten SDK (emsdk)**:
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   # On Linux/macOS:
   source ./emsdk_env.sh
   # On Windows:
   emsdk_env.bat
   cd ..
   ```
2. **Compile to WebAssembly**:
   ```bash
   # On Linux / macOS:
   bash backend/build_wasm.sh

   # On Windows:
   backend\build_wasm.bat

   # Or via npm script:
   npm run build:wasm
   ```

---

## 9. Local Web Development

1. Run the smoke tests:
   ```bash
   npm run smoke
   ```
2. Start the local server using Vercel CLI:
   ```bash
   npx vercel dev
   ```
3. Open `http://localhost:3000` in your web browser.

---

## 10. Deployment to Vercel

1. Push your repository to GitHub:
   ```bash
   git add .
   git commit -m "Deploy Virtual Memory Manager"
   git push origin main
   ```
2. Log in to [Vercel](https://vercel.com) and click **Add New Project**.
3. Import your GitHub repository `Virtual_Memory_Manager`.
4. Configure Project Settings:
   - **Framework Preset**: `Other`
   - **Build Command**: *(Leave empty)*
   - **Output Directory**: `public`
5. Click **Deploy**.
6. Verify deployment:
   ```bash
   # Health check
   curl -s https://<your-deployment>.vercel.app/api/simulate
   # Expected: {"status":"ok","engine":"C++17 compiled to WebAssembly"}

   # Test simulation
   curl -X POST https://<your-deployment>.vercel.app/api/simulate \
     -H "Content-Type: application/json" \
     -d '{"totalMemory":1000,"strategy":"FIRST_FIT","commands":["ALLOC P1 200","ALLOC P2 100"]}'
   ```

---

## 11. Troubleshooting

- **Serverless Function cannot find WebAssembly module**: Ensure `wasm/vmm.js` is committed to git (not in `.gitignore`) and that `vercel.json` includes `"includeFiles": "wasm/**"`.
- **Emscripten compilation error on flag syntax**: Different versions of Emscripten treat flag quotes differently. In `build_wasm.bat`, ensure `-s...` arguments containing `=` or `,` are wrapped in double quotes (e.g. `"-sEXPORTED_FUNCTIONS=_vmm_run,_malloc,_free"`).
- **Cold start latency**: The initial request after an idle period may take 200-400ms while the WebAssembly instance initializes into Node.js heap memory; subsequent requests are fast.

---

## 12. Assumptions, Limitations & Future Work

### Assumptions
- Memory is addressed with integer coordinates `0 .. totalMemory - 1`.
- Block headers and allocation metadata are tracked outside the simulated memory space.
- Process IDs are case-insensitive and limited to 16 alphanumeric characters or underscores.

### Limitations
- Does not simulate non-contiguous allocation (paging, segmentation).
- Single-pass waiting queue retry can delay large requests if memory frees in small fragments.

### Future Work
- Implement Worst Fit and Next Fit allocation strategies.
- Add an animated memory compaction walkthrough in the frontend.
- Simulate buddy memory allocation and slab allocation subsystems.

---

## 13. Team & Academic Details

- **Course:** Data Structures Mini Project (Project 7 &ndash; Virtual Memory Manager)
- **Team Members:**
  - Arnav
- **Academic Mentor / Guide:** *(Faculty Name Placeholder)*
- **Repository:** [https://github.com/ArnavCreates/Virtual_Memory_Manager](https://github.com/ArnavCreates/Virtual_Memory_Manager)
