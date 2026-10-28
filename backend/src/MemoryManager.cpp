#include "MemoryManager.h"
#include <cctype>
#include <iomanip>
#include <sstream>

// Purpose: Implementation of MemoryManager allocation, coalescing, compaction, and queueing logic.

namespace vmm {

MemoryManager::MemoryManager(int totalMemory, Strategy strategy)
    : totalMemory_(totalMemory), strategy_(strategy) {
    reset();
}

void MemoryManager::reset() {
    // [DS: DoublyLinkedList] Clear all blocks and initialize with single free block
    blocks_.clear();
    blocks_.pushBack(Block{0, totalMemory_, true, ""});

    // [DS: HashTable] Clear process lookup table
    processTable_.clear();

    // [DS: Queue] Clear waiting queue
    waitingQueue_.clear();
}

std::string MemoryManager::normalizePid(const std::string& pid) {
    std::string result = pid;
    for (char& c : result) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

bool MemoryManager::isValidPid(const std::string& pid) {
    if (pid.empty() || pid.size() > 16) {
        return false;
    }
    if (!std::isalpha(static_cast<unsigned char>(pid[0]))) {
        return false;
    }
    for (char c : pid) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            return false;
        }
    }
    return true;
}

ListNode<Block>* MemoryManager::findFirstFit(int size) const {
    // [DS: DoublyLinkedList] Scan from head to find first free block with size >= request
    ListNode<Block>* current = blocks_.head();
    while (current != nullptr) {
        if (current->data.isFree && current->data.size >= size) {
            return current;
        }
        current = current->next;
    }
    return nullptr;
}

ListNode<Block>* MemoryManager::findBestFit(int size) const {
    // [DS: DoublyLinkedList] Scan list to find smallest free block with size >= request
    ListNode<Block>* best = nullptr;
    ListNode<Block>* current = blocks_.head();
    while (current != nullptr) {
        if (current->data.isFree && current->data.size >= size) {
            if (best == nullptr || current->data.size < best->data.size) {
                best = current; // Strictly smaller breaks ties in favor of lowest address
            }
        }
        current = current->next;
    }
    return best;
}

ListNode<Block>* MemoryManager::findBlock(int size) const {
    if (strategy_ == Strategy::FIRST_FIT) {
        return findFirstFit(size);
    }
    return findBestFit(size);
}

void MemoryManager::placeInBlock(ListNode<Block>* node, const std::string& pid, int size) {
    int start = node->data.start;
    int oldSize = node->data.size;

    if (oldSize == size) {
        // Exact fit: no split needed
        node->data.isFree = false;
        node->data.pid = pid;
        // [DS: HashTable] Map pid to allocated block node pointer
        processTable_.insert(pid, node);
    } else {
        // Split block: shrink node to requested size
        node->data.size = size;
        node->data.isFree = false;
        node->data.pid = pid;

        // [DS: DoublyLinkedList] Insert remainder free block immediately after allocated node
        Block freeRemainder{start + size, oldSize - size, true, ""};
        blocks_.insertAfter(node, freeRemainder);

        // [DS: HashTable] Map pid to allocated block node pointer (original node retained)
        processTable_.insert(pid, node);
    }
}

void MemoryManager::coalesce(ListNode<Block>* node) {
    // [DS: DoublyLinkedList] Check if next neighbor exists and is free
    if (node->next != nullptr && node->next->data.isFree) {
        ListNode<Block>* nextNode = node->next;
        node->data.size += nextNode->data.size;
        // [DS: DoublyLinkedList] Erase next free block after absorbing its size
        blocks_.erase(nextNode);
    }

    // [DS: DoublyLinkedList] Check if previous neighbor exists and is free
    if (node->prev != nullptr && node->prev->data.isFree) {
        ListNode<Block>* prevNode = node->prev;
        prevNode->data.size += node->data.size;
        // [DS: DoublyLinkedList] Erase current node after absorbing its size into prevNode
        blocks_.erase(node);
    }
}

bool MemoryManager::isWaiting(const std::string& pid) const {
    bool found = false;
    // [DS: Queue] Inspect waiting requests via read-only traversal
    waitingQueue_.forEach([&](const WaitingRequest& req) {
        if (req.pid == pid) {
            found = true;
        }
    });
    return found;
}

void MemoryManager::retryWaitingQueue(std::vector<std::string>& details) {
    // [DS: Queue] Query queue size for single-pass retry
    std::size_t count = waitingQueue_.size();
    for (std::size_t i = 0; i < count; ++i) {
        WaitingRequest req;
        // [DS: Queue] Dequeue request from head
        waitingQueue_.dequeue(req);

        ListNode<Block>* block = findBlock(req.size);
        if (block != nullptr) {
            int start = block->data.start;
            int end = start + req.size - 1;
            placeInBlock(block, req.pid, req.size);
            details.push_back("Placed queued request " + req.pid + " (" +
                              std::to_string(req.size) + " units at " +
                              std::to_string(start) + "-" + std::to_string(end) + ")");
        } else {
            // [DS: Queue] Re-enqueue unplaced request to tail
            waitingQueue_.enqueue(req);
        }
    }
}

bool MemoryManager::cancelWaitingRequest(const std::string& pid) {
    // [DS: Queue] Single-pass queue rotation to cancel target request
    std::size_t count = waitingQueue_.size();
    bool cancelled = false;
    for (std::size_t i = 0; i < count; ++i) {
        WaitingRequest req;
        // [DS: Queue] Dequeue request
        waitingQueue_.dequeue(req);
        if (!cancelled && req.pid == pid) {
            cancelled = true; // Drop target request
        } else {
            // [DS: Queue] Re-enqueue other requests to preserve FIFO order
            waitingQueue_.enqueue(req);
        }
    }
    return cancelled;
}

bool MemoryManager::isAlreadyCompact() const {
    int freeCount = 0;
    // [DS: DoublyLinkedList] Inspect free block distribution
    ListNode<Block>* current = blocks_.head();
    while (current != nullptr) {
        if (current->data.isFree) {
            ++freeCount;
        }
        current = current->next;
    }
    if (freeCount == 0) return true;
    if (freeCount == 1 && blocks_.tail() != nullptr && blocks_.tail()->data.isFree) {
        return true;
    }
    return false;
}

CommandResult MemoryManager::allocate(const std::string& pidRaw, int size) {
    std::string pid = normalizePid(pidRaw);

    if (!isValidPid(pid)) {
        return {CommandStatus::ERROR,
                "Invalid process ID '" + pidRaw + "': must start with a letter and contain 1-16 alphanumeric characters or underscores",
                {}};
    }
    if (size < 1) {
        return {CommandStatus::ERROR,
                "Invalid allocation size " + std::to_string(size) + ": size must be at least 1 unit",
                {}};
    }
    if (size > totalMemory_) {
        return {CommandStatus::ERROR,
                "Invalid allocation size " + std::to_string(size) + ": exceeds total memory capacity (" + std::to_string(totalMemory_) + " units)",
                {}};
    }
    // [DS: HashTable] Check if process ID is already allocated
    if (processTable_.contains(pid)) {
        return {CommandStatus::ERROR, "Process " + pid + " is already allocated", {}};
    }
    // [DS: Queue] Check if process ID is already waiting
    if (isWaiting(pid)) {
        return {CommandStatus::ERROR, "Process " + pid + " is already in the waiting queue", {}};
    }

    ListNode<Block>* block = findBlock(size);
    if (block != nullptr) {
        int start = block->data.start;
        int end = start + size - 1;
        placeInBlock(block, pid, size);
        std::string stratName = (strategy_ == Strategy::FIRST_FIT) ? "First Fit" : "Best Fit";
        return {CommandStatus::OK,
                pid + " allocated " + std::to_string(size) + " units at " +
                std::to_string(start) + "-" + std::to_string(end) + " (" + stratName + ")",
                {}};
    }

    Stats stats = getStats();
    // [DS: Queue] Enqueue request to waiting queue
    waitingQueue_.enqueue({pid, size});

    if (stats.freeMemory >= size) {
        return {CommandStatus::QUEUED,
                "Request queued: enough total free memory (" + std::to_string(stats.freeMemory) +
                " units) but the largest contiguous block is only " + std::to_string(stats.largestFreeBlock) +
                " units (external fragmentation). Try COMPACT.",
                {}};
    }
    return {CommandStatus::QUEUED,
            "Request queued: not enough free memory (" + std::to_string(stats.freeMemory) + " units free)",
            {}};
}

CommandResult MemoryManager::release(const std::string& pidRaw) {
    std::string pid = normalizePid(pidRaw);

    // [DS: HashTable] Look up process node by process ID
    ListNode<Block>** nodePtr = processTable_.find(pid);
    if (nodePtr != nullptr && *nodePtr != nullptr) {
        ListNode<Block>* node = *nodePtr;
        int start = node->data.start;
        int size = node->data.size;
        int end = start + size - 1;

        // [DS: HashTable] Erase hash entry first before modifying node or coalescing
        processTable_.erase(pid);

        node->data.isFree = true;
        node->data.pid = "";

        // [DS: DoublyLinkedList] Coalesce with adjacent free neighbors
        coalesce(node);

        std::vector<std::string> details;
        // [DS: Queue] Retry waiting queue with freshly freed memory
        retryWaitingQueue(details);

        return {CommandStatus::OK,
                "Process " + pid + " released " + std::to_string(size) + " units at " +
                std::to_string(start) + "-" + std::to_string(end),
                details};
    }

    // [DS: Queue] Check if process is in waiting queue and cancel
    if (isWaiting(pid)) {
        cancelWaitingRequest(pid);
        return {CommandStatus::OK, "Waiting request for " + pid + " cancelled", {}};
    }

    return {CommandStatus::ERROR, "Process " + pid + " not found", {}};
}

CommandResult MemoryManager::compact() {
    if (isAlreadyCompact()) {
        return {CommandStatus::INFO, "Memory is already compact", {}};
    }

    int address = 0;
    int totalFree = 0;
    int movedCount = 0;

    // [DS: DoublyLinkedList] Traverse all blocks to shift allocated blocks forward
    ListNode<Block>* current = blocks_.head();
    while (current != nullptr) {
        ListNode<Block>* nextNode = current->next; // Save next pointer before erasing or modifying
        if (!current->data.isFree) {
            if (current->data.start != address) {
                current->data.start = address;
                ++movedCount;
            }
            address += current->data.size;
        } else {
            totalFree += current->data.size;
            // [DS: DoublyLinkedList] Erase free block to combine into single trailing block
            blocks_.erase(current);
        }
        current = nextNode;
    }

    if (totalFree > 0) {
        // [DS: DoublyLinkedList] Append coalesced free block at the end of memory
        blocks_.pushBack(Block{address, totalFree, true, ""});
    }

    std::vector<std::string> details;
    // [DS: Queue] Retry waiting requests now that free space has been unified
    retryWaitingQueue(details);

    std::string msg = "Compacted memory: " + std::to_string(movedCount) + " allocated " +
                      (movedCount == 1 ? "block" : "blocks") + " moved, " +
                      std::to_string(totalFree) + " free units coalesced at " +
                      std::to_string(address) + "-" + std::to_string(totalMemory_ - 1);

    return {CommandStatus::OK, msg, details};
}

CommandResult MemoryManager::setStrategy(Strategy strategy) {
    strategy_ = strategy;
    std::string name = (strategy_ == Strategy::FIRST_FIT) ? "FIRST_FIT" : "BEST_FIT";
    return {CommandStatus::OK, "Allocation strategy set to " + name, {}};
}

CommandResult MemoryManager::find(const std::string& pidRaw) const {
    std::string pid = normalizePid(pidRaw);

    // [DS: HashTable] Look up process node pointer in O(1) average time
    const ListNode<Block>* const* nodePtr = processTable_.find(pid);
    if (nodePtr != nullptr && *nodePtr != nullptr) {
        const ListNode<Block>* node = *nodePtr;
        std::size_t bucket = processTable_.bucketIndex(pid);
        int start = node->data.start;
        int end = start + node->data.size - 1;
        std::string msg = pid + " found in hash bucket " + std::to_string(bucket) +
                          ": allocated " + std::to_string(node->data.size) + " units at " +
                          std::to_string(start) + "-" + std::to_string(end);
        return {CommandStatus::INFO, msg, {}};
    }
    return {CommandStatus::ERROR, "Process " + pid + " not found (not currently allocated)", {}};
}

CommandResult MemoryManager::display() const {
    std::ostringstream oss;
    int blockCount = static_cast<int>(blocks_.size());
    oss << "Memory layout (" << blockCount << " blocks, " << totalMemory_ << " total):";

    // [DS: DoublyLinkedList] Format full memory block layout
    const ListNode<Block>* current = blocks_.head();
    while (current != nullptr) {
        int end = current->data.start + current->data.size - 1;
        oss << " [" << current->data.start << ".." << end << ": ";
        if (current->data.isFree) {
            oss << "FREE (" << current->data.size << ")]";
        } else {
            oss << current->data.pid << " (" << current->data.size << ")]";
        }
        current = current->next;
    }
    return {CommandStatus::INFO, oss.str(), {}};
}

CommandResult MemoryManager::displayFree() const {
    std::ostringstream oss;
    int count = 0;
    int totalFree = 0;

    // [DS: DoublyLinkedList] Count and summarize all free blocks
    const ListNode<Block>* current = blocks_.head();
    while (current != nullptr) {
        if (current->data.isFree) {
            ++count;
            totalFree += current->data.size;
        }
        current = current->next;
    }

    oss << "Free memory: " << count << " blocks, " << totalFree << " units total.";
    if (count > 0) {
        oss << " Blocks: ";
        bool first = true;
        current = blocks_.head();
        while (current != nullptr) {
            if (current->data.isFree) {
                if (!first) oss << ", ";
                int end = current->data.start + current->data.size - 1;
                oss << "[" << current->data.start << ".." << end << " (" << current->data.size << " units)]";
                first = false;
            }
            current = current->next;
        }
    }
    return {CommandStatus::INFO, oss.str(), {}};
}

CommandResult MemoryManager::displayAllocated() const {
    std::ostringstream oss;
    int count = 0;
    int totalUsed = 0;

    // [DS: DoublyLinkedList] Count and summarize all allocated blocks
    const ListNode<Block>* current = blocks_.head();
    while (current != nullptr) {
        if (!current->data.isFree) {
            ++count;
            totalUsed += current->data.size;
        }
        current = current->next;
    }

    oss << "Allocated memory: " << count << " blocks, " << totalUsed << " units total.";
    if (count > 0) {
        oss << " Blocks: ";
        bool first = true;
        current = blocks_.head();
        while (current != nullptr) {
            if (!current->data.isFree) {
                if (!first) oss << ", ";
                int end = current->data.start + current->data.size - 1;
                oss << "[" << current->data.pid << ": " << current->data.start << ".." << end
                    << " (" << current->data.size << " units)]";
                first = false;
            }
            current = current->next;
        }
    }
    return {CommandStatus::INFO, oss.str(), {}};
}

CommandResult MemoryManager::reportFragmentation() const {
    Stats s = getStats();
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "Fragmentation report: " << s.freeMemory << " free units in " << s.freeBlockCount
        << " blocks (largest: " << s.largestFreeBlock << "). External fragmentation: "
        << s.externalFragmentationPercent << "%.";
    return {CommandStatus::INFO, oss.str(), {}};
}

Stats MemoryManager::getStats() const {
    Stats s;
    s.totalMemory = totalMemory_;
    s.usedMemory = 0;
    s.freeMemory = 0;
    s.largestFreeBlock = 0;
    s.freeBlockCount = 0;
    s.allocatedBlockCount = 0;

    // [DS: DoublyLinkedList] Compute statistics across all blocks
    const ListNode<Block>* current = blocks_.head();
    while (current != nullptr) {
        if (current->data.isFree) {
            ++s.freeBlockCount;
            s.freeMemory += current->data.size;
            if (current->data.size > s.largestFreeBlock) {
                s.largestFreeBlock = current->data.size;
            }
        } else {
            ++s.allocatedBlockCount;
            s.usedMemory += current->data.size;
        }
        current = current->next;
    }

    if (totalMemory_ > 0) {
        s.utilizationPercent = (static_cast<double>(s.usedMemory) / totalMemory_) * 100.0;
    }

    if (s.freeMemory == 0) {
        s.externalFragmentationPercent = 0.0;
    } else {
        s.externalFragmentationPercent =
            (1.0 - (static_cast<double>(s.largestFreeBlock) / s.freeMemory)) * 100.0;
    }

    // [DS: Queue] Record current waiting queue length
    s.waitingCount = static_cast<int>(waitingQueue_.size());
    return s;
}

bool MemoryManager::validate(std::string* reason) const {
    auto setReason = [&](const std::string& msg) {
        if (reason != nullptr) {
            *reason = msg;
        }
    };

    // [DS: DoublyLinkedList] Validate invariants
    if (blocks_.empty()) {
        setReason("Invariant I1 failed: block list is empty");
        return false;
    }

    int expectedStart = 0;
    int allocatedCount = 0;
    const ListNode<Block>* current = blocks_.head();

    while (current != nullptr) {
        // Invariant I2: size >= 1
        if (current->data.size < 1) {
            setReason("Invariant I2 failed: block size < 1 at start " + std::to_string(current->data.start));
            return false;
        }

        // Invariant I1: contiguous addresses
        if (current->data.start != expectedStart) {
            setReason("Invariant I1 failed: expected block start " + std::to_string(expectedStart) +
                      " but got " + std::to_string(current->data.start));
            return false;
        }
        expectedStart += current->data.size;

        // Invariant I3: no two adjacent free blocks
        if (current->data.isFree && current->next != nullptr && current->next->data.isFree) {
            setReason("Invariant I3 failed: adjacent free blocks at addresses " +
                      std::to_string(current->data.start) + " and " +
                      std::to_string(current->next->data.start));
            return false;
        }

        if (!current->data.isFree) {
            ++allocatedCount;
            // Invariant I4: check hash table mapping
            // [DS: HashTable] Verify block node pointer in hash table matches node
            const ListNode<Block>* const* entry = processTable_.find(current->data.pid);
            if (entry == nullptr || *entry != current) {
                setReason("Invariant I4 failed: hash table entry for " + current->data.pid + " does not match node pointer");
                return false;
            }
        }

        current = current->next;
    }

    // Invariant I1: last block ends at totalMemory
    if (expectedStart != totalMemory_) {
        setReason("Invariant I1 failed: total blocks span " + std::to_string(expectedStart) +
                  " but totalMemory is " + std::to_string(totalMemory_));
        return false;
    }

    // Invariant I4: hash table entry count matches allocated block count
    if (processTable_.size() != static_cast<std::size_t>(allocatedCount)) {
        setReason("Invariant I4 failed: hash table size (" + std::to_string(processTable_.size()) +
                  ") does not match allocated block count (" + std::to_string(allocatedCount) + ")");
        return false;
    }

    // Invariants I5 and I6: check waiting queue
    bool queueValid = true;
    // [DS: Queue] Validate queued requests
    waitingQueue_.forEach([&](const WaitingRequest& req) {
        if (!queueValid) return;
        if (req.size < 1 || req.size > totalMemory_) {
            setReason("Invariant I6 failed: queued request size " + std::to_string(req.size) + " out of bounds");
            queueValid = false;
            return;
        }
        // [DS: HashTable] Check PID not in hash table
        if (processTable_.contains(req.pid)) {
            setReason("Invariant I5 failed: PID " + req.pid + " is both allocated and waiting");
            queueValid = false;
            return;
        }
    });

    return queueValid;
}

} // namespace vmm
