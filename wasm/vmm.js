/**
 * wasm/vmm.js
 * WebAssembly runtime module interface for Virtual Memory Manager.
 * Export: createVMM() returns Promise resolving to Emscripten-compatible Module.
 *
 * Implements the core simulation engine using hand-crafted Doubly Linked List,
 * FIFO Queue, and Separate-Chaining Hash Table (17 buckets, djb2 hash).
 * When built via Emscripten (backend/build_wasm.sh or backend/build_wasm.bat),
 * this file is replaced by the compiled Emscripten WebAssembly single-file artifact.
 */

class ListNode {
  constructor(data) {
    this.data = data;
    this.prev = null;
    this.next = null;
  }
}

class DoublyLinkedList {
  constructor() {
    this.head = null;
    this.tail = null;
    this._size = 0;
  }
  size() { return this._size; }
  empty() { return this._size === 0; }
  pushBack(data) {
    const node = new ListNode(data);
    if (!this.tail) {
      this.head = node;
      this.tail = node;
    } else {
      this.tail.next = node;
      node.prev = this.tail;
      this.tail = node;
    }
    this._size++;
    return node;
  }
  insertAfter(pos, data) {
    if (!pos || pos === this.tail) return this.pushBack(data);
    const node = new ListNode(data);
    node.prev = pos;
    node.next = pos.next;
    pos.next.prev = node;
    pos.next = node;
    this._size++;
    return node;
  }
  erase(node) {
    if (!node) return;
    if (node.prev) node.prev.next = node.next;
    else this.head = node.next;
    if (node.next) node.next.prev = node.prev;
    else this.tail = node.prev;
    this._size--;
  }
  clear() {
    this.head = null;
    this.tail = null;
    this._size = 0;
  }
}

class Queue {
  constructor() {
    this.items = [];
  }
  size() { return this.items.length; }
  empty() { return this.items.length === 0; }
  enqueue(val) { this.items.push(val); }
  dequeue() { return this.items.length > 0 ? this.items.shift() : null; }
  front() { return this.items.length > 0 ? this.items[0] : null; }
  forEach(fn) { for (const it of this.items) fn(it); }
  clear() { this.items = []; }
}

class HashTable {
  constructor(bucketCount = 17) {
    this.bucketCount = bucketCount;
    this.buckets = Array.from({ length: bucketCount }, () => []);
    this._size = 0;
  }
  bucketIndex(key) {
    let hash = 5381;
    for (let i = 0; i < key.length; i++) {
      hash = ((hash << 5) + hash) + key.charCodeAt(i);
      hash = hash & 0xffffffff;
    }
    return Math.abs(hash) % this.bucketCount;
  }
  insert(key, value) {
    const idx = this.bucketIndex(key);
    const b = this.buckets[idx];
    for (const e of b) {
      if (e.key === key) return false;
    }
    b.push({ key, value });
    this._size++;
    return true;
  }
  find(key) {
    const idx = this.bucketIndex(key);
    for (const e of this.buckets[idx]) {
      if (e.key === key) return e.value;
    }
    return null;
  }
  erase(key) {
    const idx = this.bucketIndex(key);
    const b = this.buckets[idx];
    for (let i = 0; i < b.length; i++) {
      if (b[i].key === key) {
        b.splice(i, 1);
        this._size--;
        return true;
      }
    }
    return false;
  }
  contains(key) { return this.find(key) !== null; }
  size() { return this._size; }
  keysInBucket(idx) {
    return (idx >= 0 && idx < this.bucketCount) ? this.buckets[idx].map(e => e.key) : [];
  }
  clear() {
    for (let i = 0; i < this.bucketCount; i++) this.buckets[i] = [];
    this._size = 0;
  }
}

class MemoryManager {
  constructor(totalMemory = 1000, strategy = "FIRST_FIT") {
    this.totalMemory = totalMemory;
    this.strategy = strategy;
    this.blocks = new DoublyLinkedList();
    this.processTable = new HashTable(17);
    this.waitingQueue = new Queue();
    this.reset();
  }

  reset() {
    this.blocks.clear();
    this.blocks.pushBack({ start: 0, size: this.totalMemory, isFree: true, pid: "" });
    this.processTable.clear();
    this.waitingQueue.clear();
  }

  static isValidPid(pid) {
    if (!pid || pid.length > 16) return false;
    if (!/^[A-Za-z][A-Za-z0-9_]{0,15}$/.test(pid)) return false;
    return true;
  }

  findBlock(size) {
    if (this.strategy === "FIRST_FIT") {
      let curr = this.blocks.head;
      while (curr) {
        if (curr.data.isFree && curr.data.size >= size) return curr;
        curr = curr.next;
      }
      return null;
    } else {
      let best = null;
      let curr = this.blocks.head;
      while (curr) {
        if (curr.data.isFree && curr.data.size >= size) {
          if (!best || curr.data.size < best.data.size) {
            best = curr;
          }
        }
        curr = curr.next;
      }
      return best;
    }
  }

  placeInBlock(node, pid, size) {
    const start = node.data.start;
    const oldSize = node.data.size;
    if (oldSize === size) {
      node.data.isFree = false;
      node.data.pid = pid;
      this.processTable.insert(pid, node);
    } else {
      node.data.size = size;
      node.data.isFree = false;
      node.data.pid = pid;
      const rem = { start: start + size, size: oldSize - size, isFree: true, pid: "" };
      this.blocks.insertAfter(node, rem);
      this.processTable.insert(pid, node);
    }
  }

  coalesce(node) {
    if (node.next && node.next.data.isFree) {
      const nextNode = node.next;
      node.data.size += nextNode.data.size;
      this.blocks.erase(nextNode);
    }
    if (node.prev && node.prev.data.isFree) {
      const prevNode = node.prev;
      prevNode.data.size += node.data.size;
      this.blocks.erase(node);
    }
  }

  isWaiting(pid) {
    let found = false;
    this.waitingQueue.forEach(req => {
      if (req.pid === pid) found = true;
    });
    return found;
  }

  retryWaitingQueue(details) {
    const n = this.waitingQueue.size();
    for (let i = 0; i < n; i++) {
      const req = this.waitingQueue.dequeue();
      const block = this.findBlock(req.size);
      if (block) {
        const start = block.data.start;
        const end = start + req.size - 1;
        this.placeInBlock(block, req.pid, req.size);
        details.push(`Placed queued request ${req.pid} (${req.size} units at ${start}-${end})`);
      } else {
        this.waitingQueue.enqueue(req);
      }
    }
  }

  cancelWaitingRequest(pid) {
    const n = this.waitingQueue.size();
    let cancelled = false;
    for (let i = 0; i < n; i++) {
      const req = this.waitingQueue.dequeue();
      if (!cancelled && req.pid === pid) {
        cancelled = true;
      } else {
        this.waitingQueue.enqueue(req);
      }
    }
    return cancelled;
  }

  isAlreadyCompact() {
    let freeCount = 0;
    let curr = this.blocks.head;
    while (curr) {
      if (curr.data.isFree) freeCount++;
      curr = curr.next;
    }
    if (freeCount === 0) return true;
    if (freeCount === 1 && this.blocks.tail && this.blocks.tail.data.isFree) return true;
    return false;
  }

  getStats() {
    let usedMemory = 0;
    let freeMemory = 0;
    let largestFreeBlock = 0;
    let freeBlockCount = 0;
    let allocatedBlockCount = 0;

    let curr = this.blocks.head;
    while (curr) {
      if (curr.data.isFree) {
        freeBlockCount++;
        freeMemory += curr.data.size;
        if (curr.data.size > largestFreeBlock) largestFreeBlock = curr.data.size;
      } else {
        allocatedBlockCount++;
        usedMemory += curr.data.size;
      }
      curr = curr.next;
    }

    const utilizationPercent = this.totalMemory > 0
      ? Number(((usedMemory / this.totalMemory) * 100).toFixed(2))
      : 0.0;

    const externalFragmentationPercent = freeMemory === 0
      ? 0.0
      : Number(((1.0 - (largestFreeBlock / freeMemory)) * 100).toFixed(2));

    return {
      totalMemory: this.totalMemory,
      usedMemory,
      freeMemory,
      utilizationPercent,
      largestFreeBlock,
      freeBlockCount,
      allocatedBlockCount,
      externalFragmentationPercent,
      waitingCount: this.waitingQueue.size()
    };
  }

  allocate(pidRaw, size) {
    const pid = (pidRaw || "").toUpperCase();
    if (!MemoryManager.isValidPid(pid)) {
      return { status: "ERROR", message: `Invalid process ID '${pidRaw}': must start with a letter and contain 1-16 alphanumeric characters or underscores`, details: [] };
    }
    if (size < 1) {
      return { status: "ERROR", message: `Invalid allocation size ${size}: size must be at least 1 unit`, details: [] };
    }
    if (size > this.totalMemory) {
      return { status: "ERROR", message: `Invalid allocation size ${size}: exceeds total memory capacity (${this.totalMemory} units)`, details: [] };
    }
    if (this.processTable.contains(pid)) {
      return { status: "ERROR", message: `Process ${pid} is already allocated`, details: [] };
    }
    if (this.isWaiting(pid)) {
      return { status: "ERROR", message: `Process ${pid} is already in the waiting queue`, details: [] };
    }

    const block = this.findBlock(size);
    if (block) {
      const start = block.data.start;
      const end = start + size - 1;
      this.placeInBlock(block, pid, size);
      const stratName = (this.strategy === "FIRST_FIT") ? "First Fit" : "Best Fit";
      return {
        status: "OK",
        message: `${pid} allocated ${size} units at ${start}-${end} (${stratName})`,
        details: []
      };
    }

    const stats = this.getStats();
    this.waitingQueue.enqueue({ pid, size });
    if (stats.freeMemory >= size) {
      return {
        status: "QUEUED",
        message: `Request queued: enough total free memory (${stats.freeMemory} units) but the largest contiguous block is only ${stats.largestFreeBlock} units (external fragmentation). Try COMPACT.`,
        details: []
      };
    }
    return {
      status: "QUEUED",
      message: `Request queued: not enough free memory (${stats.freeMemory} units free)`,
      details: []
    };
  }

  release(pidRaw) {
    const pid = (pidRaw || "").toUpperCase();
    const node = this.processTable.find(pid);
    if (node) {
      const start = node.data.start;
      const size = node.data.size;
      const end = start + size - 1;

      this.processTable.erase(pid);
      node.data.isFree = true;
      node.data.pid = "";
      this.coalesce(node);

      const details = [];
      this.retryWaitingQueue(details);

      return {
        status: "OK",
        message: `Process ${pid} released ${size} units at ${start}-${end}`,
        details
      };
    }

    if (this.isWaiting(pid)) {
      this.cancelWaitingRequest(pid);
      return { status: "OK", message: `Waiting request for ${pid} cancelled`, details: [] };
    }

    return { status: "ERROR", message: `Process ${pid} not found`, details: [] };
  }

  compact() {
    if (this.isAlreadyCompact()) {
      return { status: "INFO", message: "Memory is already compact", details: [] };
    }

    let address = 0;
    let totalFree = 0;
    let movedCount = 0;

    let curr = this.blocks.head;
    while (curr) {
      const nextNode = curr.next;
      if (!curr.data.isFree) {
        if (curr.data.start !== address) {
          curr.data.start = address;
          movedCount++;
        }
        address += curr.data.size;
      } else {
        totalFree += curr.data.size;
        this.blocks.erase(curr);
      }
      curr = nextNode;
    }

    if (totalFree > 0) {
      this.blocks.pushBack({ start: address, size: totalFree, isFree: true, pid: "" });
    }

    const details = [];
    this.retryWaitingQueue(details);

    return {
      status: "OK",
      message: `Compacted memory: ${movedCount} allocated ${movedCount === 1 ? "block" : "blocks"} moved, ${totalFree} free units coalesced at ${address}-${this.totalMemory - 1}`,
      details
    };
  }

  setStrategy(strat) {
    this.strategy = strat;
    return { status: "OK", message: `Allocation strategy set to ${strat}`, details: [] };
  }

  find(pidRaw) {
    const pid = (pidRaw || "").toUpperCase();
    const node = this.processTable.find(pid);
    if (node) {
      const bucket = this.processTable.bucketIndex(pid);
      const start = node.data.start;
      const end = start + node.data.size - 1;
      return {
        status: "INFO",
        message: `${pid} found in hash bucket ${bucket}: allocated ${node.data.size} units at ${start}-${end}`,
        details: []
      };
    }
    return { status: "ERROR", message: `Process ${pid} not found (not currently allocated)`, details: [] };
  }

  display() {
    let count = 0;
    let list = [];
    let curr = this.blocks.head;
    while (curr) {
      count++;
      const end = curr.data.start + curr.data.size - 1;
      const owner = curr.data.isFree ? "FREE" : curr.data.pid;
      list.push(`[${curr.data.start}..${end}: ${owner} (${curr.data.size})]`);
      curr = curr.next;
    }
    return {
      status: "INFO",
      message: `Memory layout (${count} blocks, ${this.totalMemory} total): ${list.join(" ")}`,
      details: []
    };
  }

  displayFree() {
    let count = 0;
    let totalFree = 0;
    let list = [];
    let curr = this.blocks.head;
    while (curr) {
      if (curr.data.isFree) {
        count++;
        totalFree += curr.data.size;
        const end = curr.data.start + curr.data.size - 1;
        list.push(`[${curr.data.start}..${end} (${curr.data.size} units)]`);
      }
      curr = curr.next;
    }
    let msg = `Free memory: ${count} blocks, ${totalFree} units total.`;
    if (count > 0) msg += ` Blocks: ${list.join(", ")}`;
    return { status: "INFO", message: msg, details: [] };
  }

  displayAllocated() {
    let count = 0;
    let totalUsed = 0;
    let list = [];
    let curr = this.blocks.head;
    while (curr) {
      if (!curr.data.isFree) {
        count++;
        totalUsed += curr.data.size;
        const end = curr.data.start + curr.data.size - 1;
        list.push(`[${curr.data.pid}: ${curr.data.start}..${end} (${curr.data.size} units)]`);
      }
      curr = curr.next;
    }
    let msg = `Allocated memory: ${count} blocks, ${totalUsed} units total.`;
    if (count > 0) msg += ` Blocks: ${list.join(", ")}`;
    return { status: "INFO", message: msg, details: [] };
  }

  reportFragmentation() {
    const s = this.getStats();
    return {
      status: "INFO",
      message: `Fragmentation report: ${s.freeMemory} free units in ${s.freeBlockCount} blocks (largest: ${s.largestFreeBlock}). External fragmentation: ${s.externalFragmentationPercent.toFixed(2)}%.`,
      details: []
    };
  }

  toJSON(log) {
    const blocksArr = [];
    let curr = this.blocks.head;
    let idx = 0;
    while (curr) {
      blocksArr.push({
        index: idx++,
        start: curr.data.start,
        end: curr.data.start + curr.data.size - 1,
        size: curr.data.size,
        status: curr.data.isFree ? "FREE" : "ALLOCATED",
        pid: curr.data.isFree ? null : curr.data.pid,
        prevStart: curr.prev ? curr.prev.data.start : null,
        nextStart: curr.next ? curr.next.data.start : null
      });
      curr = curr.next;
    }

    const waitingArr = [];
    let pos = 1;
    this.waitingQueue.forEach(req => {
      waitingArr.push({ position: pos++, pid: req.pid, size: req.size });
    });

    const bucketsArr = [];
    for (let b = 0; b < this.processTable.bucketCount; b++) {
      bucketsArr.push({
        index: b,
        keys: this.processTable.keysInBucket(b)
      });
    }

    return {
      ok: true,
      config: {
        totalMemory: this.totalMemory,
        strategy: this.strategy
      },
      blocks: blocksArr,
      waitingQueue: waitingArr,
      hashTable: {
        bucketCount: this.processTable.bucketCount,
        entryCount: this.processTable.size(),
        buckets: bucketsArr
      },
      stats: this.getStats(),
      log
    };
  }
}

function parseAndExecuteLine(lineRaw, lineNum, manager) {
  const line = lineRaw.trim();
  if (!line || line.startsWith("#")) return null;

  const tokens = line.split(/\s+/).filter(Boolean);
  if (tokens.length === 0) return null;

  const cmd = tokens[0].toUpperCase();

  if (cmd === "ALLOC") {
    if (tokens.length !== 3) {
      return { line: lineNum, command: line, status: "ERROR", message: "ALLOC requires 2 arguments: ALLOC <pid> <size>", details: [] };
    }
    if (!/^\d+$/.test(tokens[2])) {
      return { line: lineNum, command: line, status: "ERROR", message: `Invalid allocation size '${tokens[2]}': must be a positive integer`, details: [] };
    }
    const res = manager.allocate(tokens[1], parseInt(tokens[2], 10));
    return { line: lineNum, command: line, status: res.status, message: res.message, details: res.details };
  }

  if (cmd === "FREE") {
    if (tokens.length !== 2) {
      return { line: lineNum, command: line, status: "ERROR", message: "FREE requires 1 argument: FREE <pid>", details: [] };
    }
    const res = manager.release(tokens[1]);
    return { line: lineNum, command: line, status: res.status, message: res.message, details: res.details };
  }

  if (cmd === "COMPACT") {
    if (tokens.length !== 1) {
      return { line: lineNum, command: line, status: "ERROR", message: "COMPACT takes no arguments", details: [] };
    }
    const res = manager.compact();
    return { line: lineNum, command: line, status: res.status, message: res.message, details: res.details };
  }

  if (cmd === "STRATEGY") {
    if (tokens.length !== 2) {
      return { line: lineNum, command: line, status: "ERROR", message: "STRATEGY requires strategy name: STRATEGY <FIRST_FIT|BEST_FIT>", details: [] };
    }
    const s = tokens[1].toUpperCase();
    if (s === "FIRST_FIT" || s === "FIRST") {
      const res = manager.setStrategy("FIRST_FIT");
      return { line: lineNum, command: line, status: res.status, message: res.message, details: [] };
    } else if (s === "BEST_FIT" || s === "BEST") {
      const res = manager.setStrategy("BEST_FIT");
      return { line: lineNum, command: line, status: res.status, message: res.message, details: [] };
    } else {
      return { line: lineNum, command: line, status: "ERROR", message: `Unknown strategy '${tokens[1]}': use FIRST_FIT or BEST_FIT`, details: [] };
    }
  }

  if (cmd === "FIND") {
    if (tokens.length !== 2) {
      return { line: lineNum, command: line, status: "ERROR", message: "FIND requires 1 argument: FIND <pid>", details: [] };
    }
    const res = manager.find(tokens[1]);
    return { line: lineNum, command: line, status: res.status, message: res.message, details: [] };
  }

  if (cmd === "DISPLAY") {
    if (tokens.length === 1) {
      const res = manager.display();
      return { line: lineNum, command: line, status: res.status, message: res.message, details: [] };
    } else if (tokens.length === 2) {
      const sub = tokens[1].toUpperCase();
      if (sub === "FREE") {
        const res = manager.displayFree();
        return { line: lineNum, command: line, status: res.status, message: res.message, details: [] };
      } else if (sub === "ALLOCATED") {
        const res = manager.displayAllocated();
        return { line: lineNum, command: line, status: res.status, message: res.message, details: [] };
      }
      return { line: lineNum, command: line, status: "ERROR", message: `Unknown DISPLAY option '${tokens[1]}': use FREE or ALLOCATED`, details: [] };
    }
    return { line: lineNum, command: line, status: "ERROR", message: "Too many arguments for DISPLAY", details: [] };
  }

  if (cmd === "FRAG") {
    if (tokens.length !== 1) {
      return { line: lineNum, command: line, status: "ERROR", message: "FRAG takes no arguments", details: [] };
    }
    const res = manager.reportFragmentation();
    return { line: lineNum, command: line, status: res.status, message: res.message, details: [] };
  }

  return { line: lineNum, command: line, status: "ERROR", message: `Unknown command '${tokens[0]}'`, details: [] };
}

function runSimulationEngine(totalMemory, strategyStr, script) {
  if (typeof totalMemory !== "number" || totalMemory < 64 || totalMemory > 100000) {
    return JSON.stringify({ ok: false, error: `Invalid totalMemory ${totalMemory}: must be between 64 and 100000 units` });
  }

  const normStrat = (strategyStr || "").toUpperCase();
  let strategy = "FIRST_FIT";
  if (normStrat === "FIRST_FIT" || normStrat === "FIRST") {
    strategy = "FIRST_FIT";
  } else if (normStrat === "BEST_FIT" || normStrat === "BEST") {
    strategy = "BEST_FIT";
  } else {
    return JSON.stringify({ ok: false, error: `Invalid strategy '${strategyStr}': must be FIRST_FIT or BEST_FIT` });
  }

  const lines = (script || "").split(/\r?\n/);
  if (lines.length > 1000) {
    return JSON.stringify({ ok: false, error: "Script exceeds maximum limit of 1000 command lines" });
  }
  for (const l of lines) {
    if (l.length > 100) {
      return JSON.stringify({ ok: false, error: `Command line exceeds maximum length of 100 characters: '${l.slice(0, 30)}...'` });
    }
  }

  const manager = new MemoryManager(totalMemory, strategy);
  const log = [];
  let executedCount = 0;

  for (const l of lines) {
    const entry = parseAndExecuteLine(l, executedCount + 1, manager);
    if (entry) {
      executedCount++;
      log.push(entry);
    }
  }

  return JSON.stringify(manager.toJSON(log));
}

// Emscripten-compatible Module implementation
function createVMM() {
  const heap = new Uint8Array(16 * 1024 * 1024); // 16 MB heap
  let heapOffset = 1024; // Keep 0 reserved as null pointer

  const Module = {
    _malloc: function(size) {
      const ptr = heapOffset;
      heapOffset += (size + 7) & ~7; // 8-byte aligned
      if (heapOffset > heap.length) {
        throw new Error("Out of WebAssembly memory");
      }
      return ptr;
    },
    _free: function(_ptr) {
      // Linear bump allocator reset when free
    },
    lengthBytesUTF8: function(str) {
      return Buffer.byteLength(str, "utf8");
    },
    stringToUTF8: function(str, ptr, maxBytes) {
      const buf = Buffer.from(str, "utf8");
      const len = Math.min(buf.length, maxBytes - 1);
      buf.copy(Buffer.from(heap.buffer, heap.byteOffset, heap.byteLength), ptr, 0, len);
      heap[ptr + len] = 0;
    },
    UTF8ToString: function(ptr) {
      if (!ptr) return "";
      let end = ptr;
      while (heap[end] !== 0) end++;
      return Buffer.from(heap.buffer, heap.byteOffset + ptr, end - ptr).toString("utf8");
    },
    _vmm_run: function(totalMemory, sPtr, cPtr) {
      const strat = Module.UTF8ToString(sPtr);
      const cmds = Module.UTF8ToString(cPtr);
      const jsonRes = runSimulationEngine(totalMemory, strat, cmds);
      const resLen = Module.lengthBytesUTF8(jsonRes) + 1;
      const resPtr = Module._malloc(resLen);
      Module.stringToUTF8(jsonRes, resPtr, resLen);
      return resPtr;
    }
  };

  return Promise.resolve(Module);
}

module.exports = createVMM;
