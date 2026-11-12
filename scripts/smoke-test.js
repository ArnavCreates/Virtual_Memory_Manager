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

  // --- Scenario 1: First Fit ---
  console.log("[SMOKE] Running Scenario 1 (First Fit)...");
  const raw1 = await runSimulation(1000, "FIRST_FIT", [
    "ALLOC P1 200",
    "ALLOC P2 100",
    "ALLOC P3 300",
    "FREE P2",
    "ALLOC P4 80"
  ].join("\n"));

  const res1 = JSON.parse(raw1);
  assert(res1.ok === true, "Scenario 1 ok is true");
  assert(res1.blocks.length === 5, "Scenario 1 blocks count is 5");
  assert(res1.blocks[0].start === 0 && res1.blocks[0].size === 200 && res1.blocks[0].pid === "P1", "Block 0 is P1 (200)");
  assert(res1.blocks[1].start === 200 && res1.blocks[1].size === 80 && res1.blocks[1].pid === "P4", "Block 1 is P4 (80)");
  assert(res1.blocks[2].start === 280 && res1.blocks[2].size === 20 && res1.blocks[2].status === "FREE", "Block 2 is FREE (20)");
  assert(res1.blocks[3].start === 300 && res1.blocks[3].size === 300 && res1.blocks[3].pid === "P3", "Block 3 is P3 (300)");
  assert(res1.blocks[4].start === 600 && res1.blocks[4].size === 400 && res1.blocks[4].status === "FREE", "Block 4 is FREE (400)");
  assert(res1.stats.freeMemory === 420, "Free memory is 420");
  assert(res1.stats.usedMemory === 580, "Used memory is 580");
  assert(res1.stats.largestFreeBlock === 400, "Largest free block is 400");
  assert(res1.stats.freeBlockCount === 2, "Free block count is 2");
  assert(res1.stats.allocatedBlockCount === 3, "Allocated block count is 3");
  assert(approx(res1.stats.utilizationPercent, 58.0), "Utilization is 58.00%");
  assert(approx(res1.stats.externalFragmentationPercent, 4.76), "External fragmentation is 4.76%");

  // --- Scenario 2: Best Fit ---
  console.log("[SMOKE] Running Scenario 2 (Best Fit)...");
  const raw2 = await runSimulation(1000, "BEST_FIT", [
    "ALLOC P1 100",
    "ALLOC P2 300",
    "ALLOC P3 100",
    "ALLOC P4 150",
    "ALLOC P5 100",
    "FREE P2",
    "FREE P4",
    "ALLOC P6 140"
  ].join("\n"));

  const res2 = JSON.parse(raw2);
  assert(res2.ok === true, "Scenario 2 ok is true");
  assert(res2.config.strategy === "BEST_FIT", "Active strategy is BEST_FIT");
  assert(res2.blocks.length === 7, "Blocks count is 7");
  assert(res2.blocks[3].start === 500 && res2.blocks[3].size === 140 && res2.blocks[3].pid === "P6", "P6 placed at 500 in 150-hole");
  assert(res2.stats.freeMemory === 560, "Free memory is 560");
  assert(res2.stats.largestFreeBlock === 300, "Largest free block is 300");
  assert(approx(res2.stats.externalFragmentationPercent, 46.43), "External fragmentation is 46.43%");

  // --- Scenario 3: Process Lookup & Display ---
  console.log("[SMOKE] Running Scenario 3 (FIND & Display)...");
  const raw3 = await runSimulation(1000, "FIRST_FIT", [
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

  const res3 = JSON.parse(raw3);
  assert(res3.ok === true, "Scenario 3 ok is true");
  assert(res3.blocks.length === 3, "Blocks count is 3");

  const logFindP2 = res3.log.find(l => l.command === "FIND P2");
  assert(logFindP2 && logFindP2.status === "INFO", "FIND P2 status is INFO");
  assert(logFindP2 && logFindP2.message.includes("hash bucket") && logFindP2.message.includes("200-299"), "FIND P2 message includes bucket and 200-299");

  const logFindP1 = res3.log.find(l => l.command === "FIND P1");
  assert(logFindP1 && logFindP1.status === "ERROR", "FIND P1 status is ERROR");

  const logDisplay = res3.log.find(l => l.command === "DISPLAY");
  assert(logDisplay && logDisplay.status === "INFO" && logDisplay.message.length > 0, "DISPLAY status is INFO");

  const logFrag = res3.log.find(l => l.command === "FRAG");
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
