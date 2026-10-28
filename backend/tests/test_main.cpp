#include "DoublyLinkedList.h"
#include "Queue.h"
#include "HashTable.h"
#include "Block.h"
#include "MemoryManager.h"
#include "CommandParser.h"
#include "Simulator.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// Purpose: Comprehensive unit and scenario test suite verifying data structures and memory allocation invariants.

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  FAIL: " << msg << " (line " << __LINE__ << ")\n"; \
        ++g_testsFailed; \
    } else { \
        ++g_testsPassed; \
    } \
} while(0)

static bool approxEqual(double a, double b, double eps = 0.05) {
    return std::fabs(a - b) < eps;
}

// -------------------------------------------------------------
// Data Structure Unit Tests
// -------------------------------------------------------------

void testDoublyLinkedList() {
    std::cout << "[TEST] DoublyLinkedList: push, insert, erase, bidirectionality\n";
    vmm::DoublyLinkedList<int> list;
    CHECK(list.empty(), "New list is empty");
    CHECK(list.size() == 0, "New list size is 0");

    auto* n1 = list.pushBack(10);
    CHECK(list.size() == 1, "List size after pushBack");
    CHECK(list.head() == n1 && list.tail() == n1, "Single node is head and tail");
    CHECK(n1->prev == nullptr && n1->next == nullptr, "Single node prev/next are null");

    auto* n2 = list.pushBack(20);
    CHECK(list.size() == 2, "Size is 2");
    CHECK(list.head() == n1 && list.tail() == n2, "Head is n1, tail is n2");
    CHECK(n1->next == n2 && n2->prev == n1, "Links between n1 and n2 correct");

    auto* n3 = list.insertAfter(n1, 15);
    CHECK(list.size() == 3, "Size is 3 after insertAfter");
    CHECK(n1->next == n3 && n3->prev == n1, "n1 <-> n3 linked");
    CHECK(n3->next == n2 && n2->prev == n3, "n3 <-> n2 linked");

    auto* n0 = list.insertBefore(n1, 5);
    CHECK(list.size() == 4, "Size is 4 after insertBefore head");
    CHECK(list.head() == n0, "n0 is now head");
    CHECK(n0->next == n1 && n1->prev == n0, "n0 <-> n1 linked");

    // Erase middle
    list.erase(n3);
    CHECK(list.size() == 3, "Size is 3 after erasing n3");
    CHECK(n1->next == n2 && n2->prev == n1, "n1 <-> n2 linked after middle erase");

    // Erase head
    list.erase(n0);
    CHECK(list.size() == 2, "Size is 2 after erasing head");
    CHECK(list.head() == n1, "Head is now n1");
    CHECK(n1->prev == nullptr, "New head prev is null");

    // Erase tail
    list.erase(n2);
    CHECK(list.size() == 1, "Size is 1 after erasing tail");
    CHECK(list.tail() == n1, "Tail is now n1");
    CHECK(n1->next == nullptr, "New tail next is null");

    // Erase only node
    list.erase(n1);
    CHECK(list.empty(), "List is empty after erasing only node");
    CHECK(list.head() == nullptr && list.tail() == nullptr, "Head and tail null");
}

void testQueue() {
    std::cout << "[TEST] Queue: FIFO order, empty state, forEach traversal\n";
    vmm::Queue<std::string> q;
    CHECK(q.empty(), "New queue is empty");
    CHECK(q.size() == 0, "New queue size is 0");

    std::string out;
    CHECK(!q.dequeue(out), "Dequeue on empty returns false");

    q.enqueue("REQ1");
    q.enqueue("REQ2");
    q.enqueue("REQ3");
    CHECK(q.size() == 3, "Queue size 3 after 3 enqueues");

    CHECK(q.front(out) && out == "REQ1", "Front is REQ1");
    CHECK(q.dequeue(out) && out == "REQ1", "Dequeued REQ1");
    CHECK(q.dequeue(out) && out == "REQ2", "Dequeued REQ2");
    CHECK(q.size() == 1, "Size is 1");

    std::vector<std::string> remaining;
    q.forEach([&](const std::string& item) {
        remaining.push_back(item);
    });
    CHECK(remaining.size() == 1 && remaining[0] == "REQ3", "forEach order preserved");

    CHECK(q.dequeue(out) && out == "REQ3", "Dequeued REQ3");
    CHECK(q.empty(), "Queue is empty after dequeuing all");
}

void testHashTable() {
    std::cout << "[TEST] HashTable: insertion, duplicate rejection, lookup, collisions, erase\n";
    vmm::HashTable<int> table(17);
    CHECK(table.size() == 0, "Initial table size 0");
    CHECK(table.bucketCount() == 17, "Bucket count is 17");

    CHECK(table.insert("P1", 100), "Inserted P1");
    CHECK(!table.insert("P1", 200), "Duplicate P1 rejected");
    CHECK(table.size() == 1, "Size is 1");

    int* val = table.find("P1");
    CHECK(val != nullptr && *val == 100, "Found P1 with value 100");
    CHECK(table.contains("P1"), "contains(P1) is true");
    CHECK(!table.contains("P99"), "contains(P99) is false");

    // Insert 200 items to force collisions in 17 buckets
    for (int i = 0; i < 200; ++i) {
        std::string key = "KEY_" + std::to_string(i);
        CHECK(table.insert(key, i), "Inserted " + key);
    }
    CHECK(table.size() == 201, "Size is 201");

    // Check all 200 items can be found
    bool allFound = true;
    for (int i = 0; i < 200; ++i) {
        std::string key = "KEY_" + std::to_string(i);
        int* foundVal = table.find(key);
        if (foundVal == nullptr || *foundVal != i) {
            allFound = false;
            break;
        }
    }
    CHECK(allFound, "All 200 keys found despite collisions");

    // Erase all 200 keys
    bool allErased = true;
    for (int i = 0; i < 200; ++i) {
        std::string key = "KEY_" + std::to_string(i);
        if (!table.erase(key)) {
            allErased = false;
            break;
        }
    }
    CHECK(allErased, "All 200 keys erased correctly");
    CHECK(table.size() == 1, "Only P1 remains");
    CHECK(table.erase("P1"), "P1 erased");
    CHECK(table.size() == 0, "Table empty after erasing P1");
}

// -------------------------------------------------------------
// Scenario Tests (T1 to T12)
// -------------------------------------------------------------

static void executeAndValidate(vmm::MemoryManager& mgr, const std::string& cmd, int lineNum, vmm::LogEntry& entry) {
    bool ok = vmm::CommandParser::parseAndExecute(cmd, lineNum, mgr, entry);
    CHECK(ok, "Command parser accepted: " + cmd);
    std::string reason;
    CHECK(mgr.validate(&reason), "Invariant check after '" + cmd + "': " + reason);
}

void testT1() {
    std::cout << "[TEST T1] Basic allocation and fragmentation: 1000 units, First Fit\n";
    vmm::MemoryManager mgr(1000, vmm::Strategy::FIRST_FIT);
    vmm::LogEntry entry;

    executeAndValidate(mgr, "ALLOC P1 200", 1, entry);
    executeAndValidate(mgr, "ALLOC P2 100", 2, entry);
    executeAndValidate(mgr, "ALLOC P3 300", 3, entry);
    executeAndValidate(mgr, "FREE P2", 4, entry);
    executeAndValidate(mgr, "ALLOC P4 80", 5, entry);

    // Expected blocks: [0,200,P1] [200,80,P4] [280,20,FREE] [300,300,P3] [600,400,FREE]
    CHECK(mgr.blocks().size() == 5, "Total blocks is 5");
    auto* n = mgr.blocks().head();
    CHECK(n->data.start == 0 && n->data.size == 200 && n->data.pid == "P1", "Block 0 is P1");
    n = n->next;
    CHECK(n->data.start == 200 && n->data.size == 80 && n->data.pid == "P4", "Block 1 is P4");
    n = n->next;
    CHECK(n->data.start == 280 && n->data.size == 20 && n->data.isFree, "Block 2 is FREE (20)");
    n = n->next;
    CHECK(n->data.start == 300 && n->data.size == 300 && n->data.pid == "P3", "Block 3 is P3");
    n = n->next;
    CHECK(n->data.start == 600 && n->data.size == 400 && n->data.isFree, "Block 4 is FREE (400)");

    vmm::Stats s = mgr.getStats();
    CHECK(s.freeMemory == 420, "Free memory is 420");
    CHECK(s.usedMemory == 580, "Used memory is 580");
    CHECK(s.largestFreeBlock == 400, "Largest free block is 400");
    CHECK(s.freeBlockCount == 2, "Free block count is 2");
    CHECK(s.allocatedBlockCount == 3, "Allocated block count is 3");
    CHECK(approxEqual(s.utilizationPercent, 58.0), "Utilization is 58.00%");
    CHECK(approxEqual(s.externalFragmentationPercent, 4.76), "Fragmentation is 4.76%");
}

void testT2a() {
    std::cout << "[TEST T2a] First Fit vs Best Fit comparison scenario (First Fit)\n";
    vmm::MemoryManager mgr(1000, vmm::Strategy::FIRST_FIT);
    vmm::LogEntry entry;

    executeAndValidate(mgr, "ALLOC P1 100", 1, entry);
    executeAndValidate(mgr, "ALLOC P2 300", 2, entry);
    executeAndValidate(mgr, "ALLOC P3 100", 3, entry);
    executeAndValidate(mgr, "ALLOC P4 150", 4, entry);
    executeAndValidate(mgr, "ALLOC P5 100", 5, entry);
    executeAndValidate(mgr, "FREE P2", 6, entry);
    executeAndValidate(mgr, "FREE P4", 7, entry);
    executeAndValidate(mgr, "ALLOC P6 140", 8, entry);

    // Expected: [0,100,P1] [100,140,P6] [240,160,FREE] [400,100,P3] [500,150,FREE] [650,100,P5] [750,250,FREE]
    CHECK(mgr.blocks().size() == 7, "Total blocks is 7");
    auto* n = mgr.blocks().head();
    CHECK(n->data.start == 0 && n->data.size == 100 && n->data.pid == "P1", "P1 at 0");
    n = n->next;
    CHECK(n->data.start == 100 && n->data.size == 140 && n->data.pid == "P6", "P6 placed in first hole (size 300)");
    n = n->next;
    CHECK(n->data.start == 240 && n->data.size == 160 && n->data.isFree, "Free remainder 160 at 240");
    n = n->next;
    CHECK(n->data.start == 400 && n->data.size == 100 && n->data.pid == "P3", "P3 at 400");
    n = n->next;
    CHECK(n->data.start == 500 && n->data.size == 150 && n->data.isFree, "Free block 150 at 500");
    n = n->next;
    CHECK(n->data.start == 650 && n->data.size == 100 && n->data.pid == "P5", "P5 at 650");
    n = n->next;
    CHECK(n->data.start == 750 && n->data.size == 250 && n->data.isFree, "Free block 250 at 750");

    vmm::Stats s = mgr.getStats();
    CHECK(s.freeMemory == 560, "Free memory is 560");
    CHECK(s.largestFreeBlock == 250, "Largest free block is 250");
    CHECK(approxEqual(s.externalFragmentationPercent, 55.36), "Fragmentation is 55.36%");
}

void testT2b() {
    std::cout << "[TEST T2b] First Fit vs Best Fit comparison scenario (Best Fit)\n";
    vmm::MemoryManager mgr(1000, vmm::Strategy::BEST_FIT);
    vmm::LogEntry entry;

    executeAndValidate(mgr, "ALLOC P1 100", 1, entry);
    executeAndValidate(mgr, "ALLOC P2 300", 2, entry);
    executeAndValidate(mgr, "ALLOC P3 100", 3, entry);
    executeAndValidate(mgr, "ALLOC P4 150", 4, entry);
    executeAndValidate(mgr, "ALLOC P5 100", 5, entry);
    executeAndValidate(mgr, "FREE P2", 6, entry);
    executeAndValidate(mgr, "FREE P4", 7, entry);
    executeAndValidate(mgr, "ALLOC P6 140", 8, entry);

    // Expected: [0,100,P1] [100,300,FREE] [400,100,P3] [500,140,P6] [640,10,FREE] [650,100,P5] [750,250,FREE]
    CHECK(mgr.blocks().size() == 7, "Total blocks is 7");
    auto* n = mgr.blocks().head();
    CHECK(n->data.start == 0 && n->data.size == 100 && n->data.pid == "P1", "P1 at 0");
    n = n->next;
    CHECK(n->data.start == 100 && n->data.size == 300 && n->data.isFree, "Free block 300 preserved at 100");
    n = n->next;
    CHECK(n->data.start == 400 && n->data.size == 100 && n->data.pid == "P3", "P3 at 400");
    n = n->next;
    CHECK(n->data.start == 500 && n->data.size == 140 && n->data.pid == "P6", "P6 placed in best-fit hole (150)");
    n = n->next;
    CHECK(n->data.start == 640 && n->data.size == 10 && n->data.isFree, "Free remainder 10 at 640");
    n = n->next;
    CHECK(n->data.start == 650 && n->data.size == 100 && n->data.pid == "P5", "P5 at 650");
    n = n->next;
    CHECK(n->data.start == 750 && n->data.size == 250 && n->data.isFree, "Free block 250 at 750");

    vmm::Stats s = mgr.getStats();
    CHECK(s.freeMemory == 560, "Free memory is 560");
    CHECK(s.largestFreeBlock == 300, "Largest free block is 300");
    CHECK(approxEqual(s.externalFragmentationPercent, 46.43), "Fragmentation is 46.43%");
}

void testT3() {
    std::cout << "[TEST T3] Coalescing of adjacent free blocks\n";
    vmm::MemoryManager mgr(1000, vmm::Strategy::FIRST_FIT);
    vmm::LogEntry entry;

    executeAndValidate(mgr, "ALLOC A 100", 1, entry);
    executeAndValidate(mgr, "ALLOC B 100", 2, entry);
    executeAndValidate(mgr, "ALLOC C 100", 3, entry);
    executeAndValidate(mgr, "FREE A", 4, entry);
    executeAndValidate(mgr, "FREE C", 5, entry);

    // After FREE C: [0,100,FREE] [100,100,B] [200,800,FREE]
    CHECK(mgr.blocks().size() == 3, "3 blocks after freeing A and C");
    auto* last = mgr.blocks().tail();
    CHECK(last->data.start == 200 && last->data.size == 800 && last->data.isFree, "C coalesced with trailing free space");

    executeAndValidate(mgr, "FREE B", 6, entry);
    // After FREE B: exactly 1 free block [0, 1000, FREE]
    CHECK(mgr.blocks().size() == 1, "Exactly 1 block after all freed");
    auto* head = mgr.blocks().head();
    CHECK(head->data.start == 0 && head->data.size == 1000 && head->data.isFree, "Single coalesced block [0, 1000, FREE]");
    CHECK(mgr.processTable().size() == 0, "Hash table is completely empty");
}

void testT4() {
    std::cout << "[TEST T4] Waiting Queue and automatic retry on FREE\n";
    vmm::MemoryManager mgr(500, vmm::Strategy::FIRST_FIT);
    vmm::LogEntry entry;

    executeAndValidate(mgr, "ALLOC P1 300", 1, entry);
    executeAndValidate(mgr, "ALLOC P2 200", 2, entry);
    executeAndValidate(mgr, "ALLOC P3 250", 3, entry);
    CHECK(entry.status == vmm::CommandStatus::QUEUED, "P3 was QUEUED");
    executeAndValidate(mgr, "ALLOC P4 100", 4, entry);
    CHECK(entry.status == vmm::CommandStatus::QUEUED, "P4 was QUEUED");
    CHECK(mgr.waitingQueue().size() == 2, "Queue has 2 requests [P3, P4]");

    // T4b: FREE P1 -> P3 is placed
    executeAndValidate(mgr, "FREE P1", 5, entry);
    CHECK(mgr.waitingQueue().size() == 1, "Queue has 1 request [P4]");
    bool p3Mentioned = false;
    for (const auto& d : entry.details) {
        if (d.find("P3") != std::string::npos) p3Mentioned = true;
    }
    CHECK(p3Mentioned, "Log details mentions P3 placed");
    // State: [0,250,P3] [250,50,FREE] [300,200,P2]
    CHECK(mgr.blocks().size() == 3, "Blocks count is 3");
    auto* n = mgr.blocks().head();
    CHECK(n->data.start == 0 && n->data.size == 250 && n->data.pid == "P3", "P3 at 0..249");
    n = n->next;
    CHECK(n->data.start == 250 && n->data.size == 50 && n->data.isFree, "Free 50 at 250..299");
    n = n->next;
    CHECK(n->data.start == 300 && n->data.size == 200 && n->data.pid == "P2", "P2 at 300..499");

    // T4c: FREE P2 -> P4 is placed
    executeAndValidate(mgr, "FREE P2", 6, entry);
    CHECK(mgr.waitingQueue().empty(), "Queue is empty after P4 placed");
    // State: [0,250,P3] [250,100,P4] [350,150,FREE]
    CHECK(mgr.blocks().size() == 3, "Blocks count is 3");
    n = mgr.blocks().head();
    CHECK(n->data.start == 0 && n->data.size == 250 && n->data.pid == "P3", "P3 at 0");
    n = n->next;
    CHECK(n->data.start == 250 && n->data.size == 100 && n->data.pid == "P4", "P4 placed at 250");
    n = n->next;
    CHECK(n->data.start == 350 && n->data.size == 150 && n->data.isFree, "Free block 150 at 350");
}

void testT5() {
    std::cout << "[TEST T5] Compaction and queued request placement\n";
    vmm::MemoryManager mgr(1000, vmm::Strategy::FIRST_FIT);
    vmm::LogEntry entry;

    executeAndValidate(mgr, "ALLOC P1 100", 1, entry);
    executeAndValidate(mgr, "ALLOC P2 200", 2, entry);
    executeAndValidate(mgr, "ALLOC P3 100", 3, entry);
    executeAndValidate(mgr, "ALLOC P4 300", 4, entry);
    executeAndValidate(mgr, "FREE P2", 5, entry);
    executeAndValidate(mgr, "FREE P4", 6, entry);

    // Before last: [0,100,P1] [100,200,FREE] [300,100,P3] [400,600,FREE]
    vmm::Stats s = mgr.getStats();
    CHECK(s.freeMemory == 800 && s.largestFreeBlock == 600, "Free 800, largest 600");

    executeAndValidate(mgr, "ALLOC P5 700", 7, entry);
    CHECK(entry.status == vmm::CommandStatus::QUEUED, "P5 700 is QUEUED");
    CHECK(entry.message.find("Try COMPACT") != std::string::npos, "Message mentions fragmentation and COMPACT");

    // T5b: COMPACT
    executeAndValidate(mgr, "COMPACT", 8, entry);
    CHECK(entry.status == vmm::CommandStatus::OK, "COMPACT status OK");
    CHECK(mgr.waitingQueue().empty(), "Queue is empty after compaction");

    // State: [0,100,P1] [100,100,P3] [200,700,P5] [900,100,FREE]
    CHECK(mgr.blocks().size() == 4, "Total blocks is 4");
    auto* n = mgr.blocks().head();
    CHECK(n->data.start == 0 && n->data.size == 100 && n->data.pid == "P1", "P1 at 0");
    n = n->next;
    CHECK(n->data.start == 100 && n->data.size == 100 && n->data.pid == "P3", "P3 moved to 100");
    n = n->next;
    CHECK(n->data.start == 200 && n->data.size == 700 && n->data.pid == "P5", "P5 placed at 200");
    n = n->next;
    CHECK(n->data.start == 900 && n->data.size == 100 && n->data.isFree, "Free 100 at 900");

    // Verify hash entries for P1 and P3 still resolve to correct blocks
    auto* p1Node = *mgr.processTable().find("P1");
    auto* p3Node = *mgr.processTable().find("P3");
    auto* p5Node = *mgr.processTable().find("P5");
    CHECK(p1Node->data.start == 0 && p1Node->data.pid == "P1", "P1 hash entry valid");
    CHECK(p3Node->data.start == 100 && p3Node->data.pid == "P3", "P3 hash entry valid");
    CHECK(p5Node->data.start == 200 && p5Node->data.pid == "P5", "P5 hash entry valid");
}

void testT6() {
    std::cout << "[TEST T6] Cancellation of queued requests via FREE\n";
    vmm::MemoryManager mgr(100, vmm::Strategy::FIRST_FIT);
    vmm::LogEntry entry;

    executeAndValidate(mgr, "ALLOC P1 100", 1, entry);
    executeAndValidate(mgr, "ALLOC P2 50", 2, entry);
    CHECK(entry.status == vmm::CommandStatus::QUEUED, "P2 is QUEUED");
    CHECK(mgr.waitingQueue().size() == 1, "Queue size is 1");

    executeAndValidate(mgr, "FREE P2", 3, entry);
    CHECK(entry.status == vmm::CommandStatus::OK, "FREE P2 cancelled request with OK");
    CHECK(mgr.waitingQueue().empty(), "Queue is empty after cancellation");
    CHECK(mgr.blocks().size() == 1 && mgr.blocks().head()->data.pid == "P1", "Memory unchanged");
}

void testT7() {
    std::cout << "[TEST T7] Exact fit allocation (no split)\n";
    vmm::MemoryManager mgr(300, vmm::Strategy::FIRST_FIT);
    vmm::LogEntry entry;

    executeAndValidate(mgr, "ALLOC P1 100", 1, entry);
    executeAndValidate(mgr, "ALLOC P2 100", 2, entry);
    executeAndValidate(mgr, "ALLOC P3 100", 3, entry);
    executeAndValidate(mgr, "FREE P2", 4, entry);
    executeAndValidate(mgr, "ALLOC P4 100", 5, entry);

    // Exact fit: [0,100,P1] [100,100,P4] [200,100,P3], exactly 3 blocks, no split
    CHECK(mgr.blocks().size() == 3, "Exactly 3 blocks");
    auto* n = mgr.blocks().head();
    CHECK(n->data.start == 0 && n->data.size == 100 && n->data.pid == "P1", "P1 at 0");
    n = n->next;
    CHECK(n->data.start == 100 && n->data.size == 100 && n->data.pid == "P4", "P4 at 100 (exact fit)");
    n = n->next;
    CHECK(n->data.start == 200 && n->data.size == 100 && n->data.pid == "P3", "P3 at 200");
}

void testT8() {
    std::cout << "[TEST T8] Error handling and syntax validation\n";
    vmm::MemoryManager mgr(1000, vmm::Strategy::FIRST_FIT);
    vmm::LogEntry entry;

    executeAndValidate(mgr, "ALLOC P1 0", 1, entry);
    CHECK(entry.status == vmm::CommandStatus::ERROR, "ALLOC P1 0 is ERROR");

    executeAndValidate(mgr, "ALLOC P1 5000", 2, entry);
    CHECK(entry.status == vmm::CommandStatus::ERROR, "ALLOC 5000 is ERROR (not queued)");
    CHECK(mgr.waitingQueue().empty(), "5000 was NOT queued");

    executeAndValidate(mgr, "FREE P9", 3, entry);
    CHECK(entry.status == vmm::CommandStatus::ERROR, "FREE non-existent P9 is ERROR");

    executeAndValidate(mgr, "ALLOC P1 100", 4, entry);
    CHECK(entry.status == vmm::CommandStatus::OK, "ALLOC P1 100 is OK");

    executeAndValidate(mgr, "ALLOC P1 50", 5, entry);
    CHECK(entry.status == vmm::CommandStatus::ERROR, "Duplicate ALLOC P1 is ERROR");

    executeAndValidate(mgr, "HELLO", 6, entry);
    CHECK(entry.status == vmm::CommandStatus::ERROR, "Unknown command HELLO is ERROR");

    executeAndValidate(mgr, "ALLOC P2", 7, entry);
    CHECK(entry.status == vmm::CommandStatus::ERROR, "Missing size is ERROR");

    executeAndValidate(mgr, "ALLOC P-1 100", 8, entry);
    CHECK(entry.status == vmm::CommandStatus::ERROR, "Invalid PID P-1 is ERROR");

    executeAndValidate(mgr, "ALLOC P3 12abc", 9, entry);
    CHECK(entry.status == vmm::CommandStatus::ERROR, "Non-digit size 12abc is ERROR");

    // Final state: just [0,100,P1] [100,900,FREE]
    CHECK(mgr.blocks().size() == 2, "Final block count is 2");
    auto* n = mgr.blocks().head();
    CHECK(n->data.start == 0 && n->data.size == 100 && n->data.pid == "P1", "P1 at 0");
    n = n->next;
    CHECK(n->data.start == 100 && n->data.size == 900 && n->data.isFree, "Free 900 at 100");
}

void testT9() {
    std::cout << "[TEST T9] Mid-script strategy change and Simulator JSON output\n";
    std::string script =
        "ALLOC P1 100\n"
        "ALLOC P2 300\n"
        "ALLOC P3 100\n"
        "ALLOC P4 150\n"
        "ALLOC P5 100\n"
        "FREE P2\n"
        "FREE P4\n"
        "strategy best_fit\n" // lowercase strategy command
        "ALLOC P6 140\n";

    std::string json = vmm::runSimulation(1000, "FIRST_FIT", script);
    CHECK(json.find("\"ok\": true") != std::string::npos, "Simulation JSON ok is true");
    CHECK(json.find("\"strategy\": \"BEST_FIT\"") != std::string::npos, "JSON config strategy reports BEST_FIT");
    // Verify P6 placed at 500..639 (Best Fit) rather than 100..239 (First Fit)
    CHECK(json.find("\"start\": 500") != std::string::npos, "P6 placed at start 500");
    CHECK(json.find("\"pid\": \"P6\"") != std::string::npos, "P6 present in blocks");
}

void testT10() {
    std::cout << "[TEST T10] Compaction no-op on already compact memory\n";
    vmm::MemoryManager mgr(1000, vmm::Strategy::FIRST_FIT);
    vmm::LogEntry entry;

    executeAndValidate(mgr, "ALLOC P1 100", 1, entry);
    executeAndValidate(mgr, "COMPACT", 2, entry);
    CHECK(entry.status == vmm::CommandStatus::INFO, "COMPACT status is INFO");
    CHECK(entry.message == "Memory is already compact", "Message states already compact");
    CHECK(mgr.blocks().size() == 2, "State unchanged: 2 blocks");
}

void testT11() {
    std::cout << "[TEST T11] Stress test: 10,000 pseudo-random commands under both strategies\n";
    for (int stratIdx = 0; stratIdx < 2; ++stratIdx) {
        vmm::Strategy strat = (stratIdx == 0) ? vmm::Strategy::FIRST_FIT : vmm::Strategy::BEST_FIT;
        vmm::MemoryManager mgr(1000, strat);

        // Deterministic LCG random generator
        uint64_t state = 123456789ULL;
        auto nextRand = [&]() -> uint32_t {
            state = state * 6364136223846793005ULL + 1ULL;
            return static_cast<uint32_t>(state >> 32);
        };

        for (int step = 0; step < 10000; ++step) {
            uint32_t op = nextRand() % 10;
            vmm::LogEntry entry;
            if (op < 5) {
                // ALLOC P0..P29 with size 1..300
                int pNum = nextRand() % 30;
                int size = 1 + (nextRand() % 300);
                std::string cmd = "ALLOC P" + std::to_string(pNum) + " " + std::to_string(size);
                vmm::CommandParser::parseAndExecute(cmd, step + 1, mgr, entry);
            } else if (op < 9) {
                // FREE P0..P29
                int pNum = nextRand() % 30;
                std::string cmd = "FREE P" + std::to_string(pNum);
                vmm::CommandParser::parseAndExecute(cmd, step + 1, mgr, entry);
            } else {
                // COMPACT
                vmm::CommandParser::parseAndExecute("COMPACT", step + 1, mgr, entry);
            }

            // Invariant validation after every step
            std::string reason;
            bool valid = mgr.validate(&reason);
            if (!valid) {
                std::cerr << "Invariant failure at step " << step << ": " << reason << "\n";
                CHECK(false, "Invariant failed in stress test");
                return;
            }

            vmm::Stats s = mgr.getStats();
            if (s.usedMemory + s.freeMemory != s.totalMemory) {
                CHECK(false, "used + free != total in stress test");
                return;
            }
        }
    }
    CHECK(true, "10,000 commands completed with zero invariant violations under both strategies");
}

void testT12() {
    std::cout << "[TEST T12] Process lookup (FIND) and display filters\n";
    vmm::MemoryManager mgr(1000, vmm::Strategy::FIRST_FIT);
    vmm::LogEntry entry;

    executeAndValidate(mgr, "ALLOC P1 200", 1, entry);
    executeAndValidate(mgr, "ALLOC P2 100", 2, entry);
    executeAndValidate(mgr, "FREE P1", 3, entry);

    // FIND P2
    executeAndValidate(mgr, "FIND P2", 4, entry);
    CHECK(entry.status == vmm::CommandStatus::INFO, "FIND P2 status is INFO");
    CHECK(entry.message.find("hash bucket") != std::string::npos, "FIND mentions hash bucket");
    CHECK(entry.message.find("200-299") != std::string::npos, "FIND mentions 200-299 range");

    // FIND P1
    executeAndValidate(mgr, "FIND P1", 5, entry);
    CHECK(entry.status == vmm::CommandStatus::ERROR, "FIND P1 status is ERROR (freed)");

    // DISPLAY commands
    executeAndValidate(mgr, "DISPLAY", 6, entry);
    CHECK(entry.status == vmm::CommandStatus::INFO, "DISPLAY status is INFO");
    CHECK(!entry.message.empty(), "DISPLAY message is not empty");

    executeAndValidate(mgr, "DISPLAY FREE", 7, entry);
    CHECK(entry.status == vmm::CommandStatus::INFO, "DISPLAY FREE status is INFO");
    CHECK(entry.message.find("Free memory") != std::string::npos, "DISPLAY FREE mentions Free memory");

    executeAndValidate(mgr, "DISPLAY ALLOCATED", 8, entry);
    CHECK(entry.status == vmm::CommandStatus::INFO, "DISPLAY ALLOCATED status is INFO");
    CHECK(entry.message.find("Allocated memory") != std::string::npos, "DISPLAY ALLOCATED mentions Allocated memory");

    executeAndValidate(mgr, "FRAG", 9, entry);
    CHECK(entry.status == vmm::CommandStatus::INFO, "FRAG status is INFO");
    CHECK(entry.message.find("Fragmentation report") != std::string::npos, "FRAG message mentions Fragmentation report");

    // Verify memory state remains unchanged by informational commands
    CHECK(mgr.blocks().size() == 3, "Memory state unchanged by display and find commands");
}

int main() {
    std::cout << "============================================================\n";
    std::cout << "           Virtual Memory Manager Native Test Suite         \n";
    std::cout << "============================================================\n\n";

    testDoublyLinkedList();
    testQueue();
    testHashTable();

    testT1();
    testT2a();
    testT2b();
    testT3();
    testT4();
    testT5();
    testT6();
    testT7();
    testT8();
    testT9();
    testT10();
    testT11();
    testT12();

    std::cout << "\n============================================================\n";
    std::cout << "Tests Summary: " << g_testsPassed << " passed, " << g_testsFailed << " failed.\n";
    std::cout << "============================================================\n";

    return (g_testsFailed == 0) ? 0 : 1;
}
