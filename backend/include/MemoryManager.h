#pragma once
#include "DoublyLinkedList.h"
#include "Queue.h"
#include "HashTable.h"
#include "Block.h"
#include <string>
#include <vector>

// Purpose: Core memory manager class simulating dynamic allocation, coalescing, compaction, and queueing.

namespace vmm {

class MemoryManager {
public:
    explicit MemoryManager(int totalMemory = 1000, Strategy strategy = Strategy::FIRST_FIT);
    ~MemoryManager() = default;

    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    CommandResult allocate(const std::string& pid, int size);
    CommandResult release(const std::string& pid);
    CommandResult compact();
    CommandResult setStrategy(Strategy strategy);
    CommandResult find(const std::string& pid) const;
    CommandResult display() const;
    CommandResult displayFree() const;
    CommandResult displayAllocated() const;
    CommandResult reportFragmentation() const;

    void reset();
    bool validate(std::string* reason = nullptr) const;

    int totalMemory() const { return totalMemory_; }
    Strategy strategy() const { return strategy_; }
    const DoublyLinkedList<Block>& blocks() const { return blocks_; }
    const HashTable<ListNode<Block>*>& processTable() const { return processTable_; }
    const Queue<WaitingRequest>& waitingQueue() const { return waitingQueue_; }
    Stats getStats() const;

    static bool isValidPid(const std::string& pid);
    static std::string normalizePid(const std::string& pid);

private:
    int totalMemory_;
    Strategy strategy_;

    // [DS: DoublyLinkedList] Maintains contiguous memory blocks in address order
    DoublyLinkedList<Block> blocks_;

    // [DS: HashTable] Fast O(1) lookup from process ID to its allocated block node
    HashTable<ListNode<Block>*> processTable_;

    // [DS: Queue] FIFO queue for processes waiting for memory to become available
    Queue<WaitingRequest> waitingQueue_;

    ListNode<Block>* findFirstFit(int size) const;
    ListNode<Block>* findBestFit(int size) const;
    ListNode<Block>* findBlock(int size) const;

    void placeInBlock(ListNode<Block>* node, const std::string& pid, int size);
    void coalesce(ListNode<Block>* node);
    void retryWaitingQueue(std::vector<std::string>& details);
    bool cancelWaitingRequest(const std::string& pid);
    bool isWaiting(const std::string& pid) const;
    bool isAlreadyCompact() const;
};

} // namespace vmm
