// scripts/smoke-test.js
// Loads lib/vmmBridge.js to exercise the WebAssembly module path and verifies scenarios T1, T2b, and T12.

const { runSimulation } = require('../lib/vmmBridge');

let failed = 0;
let passed = 0;

function assert(condition, message) {
  if (!condition) {
    console.error(`  FAIL: ${message}`);
    failed++;
  } else {
    passed++;
  }
}

function approx(a, b, eps = 0.05) {
  return Math.abs(a - b) < eps;
}

async function runSmokeTests() {
  console.log("============================================================");
  console.log("           Virtual Memory Manager Smoke Test Suite          ");
  console.log("============================================================\n");

  // --- Scenario T1 ---
  console.log("[SMOKE] Running Scenario T1 (First Fit)...");
  const rawT1 = await runSimulation(1000, "FIRST_FIT", [
    "ALLOC P1 200",
    "ALLOC P2 100",
    "ALLOC P3 300",
    "FREE P2",
    "ALLOC P4 80"
  ].join("\n"));

  const t1 = JSON.parse(rawT1);
  assert(t1.ok === true, "T1 ok is true");
  assert(t1.blocks.length === 5, "T1 blocks count is 5");
  assert(t1.blocks[0].start === 0 && t1.blocks[0].size === 200 && t1.blocks[0].pid === "P1", "T1 block 0 is P1 (200)");
  assert(t1.blocks[1].start === 200 && t1.blocks[1].size === 80 && t1.blocks[1].pid === "P4", "T1 block 1 is P4 (80)");
  assert(t1.blocks[2].start === 280 && t1.blocks[2].size === 20 && t1.blocks[2].status === "FREE", "T1 block 2 is FREE (20)");
  assert(t1.blocks[3].start === 300 && t1.blocks[3].size === 300 && t1.blocks[3].pid === "P3", "T1 block 3 is P3 (300)");
  assert(t1.blocks[4].start === 600 && t1.blocks[4].size === 400 && t1.blocks[4].status === "FREE", "T1 block 4 is FREE (400)");
  assert(t1.stats.freeMemory === 420, "T1 freeMemory is 420");
  assert(t1.stats.usedMemory === 580, "T1 usedMemory is 580");
  assert(t1.stats.largestFreeBlock === 400, "T1 largestFreeBlock is 400");
  assert(t1.stats.freeBlockCount === 2, "T1 freeBlockCount is 2");
  assert(t1.stats.allocatedBlockCount === 3, "T1 allocatedBlockCount is 3");
  assert(approx(t1.stats.utilizationPercent, 58.0), "T1 utilizationPercent is 58.00%");
  assert(approx(t1.stats.externalFragmentationPercent, 4.76), "T1 externalFragmentationPercent is 4.76%");

  // --- Scenario T2b ---
  console.log("[SMOKE] Running Scenario T2b (Best Fit)...");
  const rawT2b = await runSimulation(1000, "BEST_FIT", [
    "ALLOC P1 100",
    "ALLOC P2 300",
    "ALLOC P3 100",
    "ALLOC P4 150",
    "ALLOC P5 100",
    "FREE P2",
    "FREE P4",
    "ALLOC P6 140"
  ].join("\n"));

  const t2b = JSON.parse(rawT2b);
  assert(t2b.ok === true, "T2b ok is true");
  assert(t2b.config.strategy === "BEST_FIT", "T2b active strategy is BEST_FIT");
  assert(t2b.blocks.length === 7, "T2b blocks count is 7");
  assert(t2b.blocks[3].start === 500 && t2b.blocks[3].size === 140 && t2b.blocks[3].pid === "P6", "T2b P6 placed at 500 in 150-hole");
  assert(t2b.stats.freeMemory === 560, "T2b freeMemory is 560");
  assert(t2b.stats.largestFreeBlock === 300, "T2b largestFreeBlock is 300");
  assert(approx(t2b.stats.externalFragmentationPercent, 46.43), "T2b externalFragmentationPercent is 46.43%");

  // --- Scenario T12 ---
  console.log("[SMOKE] Running Scenario T12 (FIND & Display)...");
  const rawT12 = await runSimulation(1000, "FIRST_FIT", [
    "ALLOC P1 200",
    "ALLOC P2 100",
    "FREE P1",
    "FIND P2",
    "FIND P1",
    "DISPLAY",
    "DISPLAY FREE",
    "DISPLAY ALLOCATED",
    "FRAG"
  ].join("\n"));

  const t12 = JSON.parse(rawT12);
  assert(t12.ok === true, "T12 ok is true");
  assert(t12.blocks.length === 3, "T12 blocks count is 3");

  const logFindP2 = t12.log.find(l => l.command === "FIND P2");
  assert(logFindP2 && logFindP2.status === "INFO", "FIND P2 status is INFO");
  assert(logFindP2 && logFindP2.message.includes("hash bucket") && logFindP2.message.includes("200-299"), "FIND P2 message includes bucket and 200-299");

  const logFindP1 = t12.log.find(l => l.command === "FIND P1");
  assert(logFindP1 && logFindP1.status === "ERROR", "FIND P1 status is ERROR");

  const logDisplay = t12.log.find(l => l.command === "DISPLAY");
  assert(logDisplay && logDisplay.status === "INFO" && logDisplay.message.length > 0, "DISPLAY status is INFO");

  const logFrag = t12.log.find(l => l.command === "FRAG");
  assert(logFrag && logFrag.status === "INFO" && logFrag.message.includes("Fragmentation report"), "FRAG status is INFO");

  console.log("\n============================================================");
  console.log(`Smoke Test Summary: ${passed} passed, ${failed} failed.`);
  console.log("============================================================\n");

  if (failed > 0) {
    process.exit(1);
  }
}

runSmokeTests().catch(err => {
  console.error("Unhandled smoke test exception:", err);
  process.exit(1);
});
