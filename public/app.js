// public/app.js
// Client-side controller for Virtual Memory Manager simulator

(function() {
  'use strict';

  // Application State
  const state = {
    totalMemory: 1000,
    initialStrategy: 'FIRST_FIT',
    commands: [],
    blockFilter: 'ALL', // 'ALL' | 'ALLOCATED' | 'FREE'
    highlightedPid: null,
    lastResponse: null,
    inFlight: false
  };

  // Preset Scenarios
  const PRESETS = {
    basic: {
      totalMemory: 1000,
      strategy: 'FIRST_FIT',
      commands: ['ALLOC P1 200', 'ALLOC P2 100', 'ALLOC P3 300', 'FREE P2', 'ALLOC P4 80']
    },
    comparison: {
      totalMemory: 1000,
      strategy: 'FIRST_FIT',
      commands: [
        'ALLOC P1 100', 'ALLOC P2 300', 'ALLOC P3 100',
        'ALLOC P4 150', 'ALLOC P5 100', 'FREE P2', 'FREE P4', 'ALLOC P6 140'
      ]
    },
    queue: {
      totalMemory: 500,
      strategy: 'FIRST_FIT',
      commands: ['ALLOC P1 300', 'ALLOC P2 200', 'ALLOC P3 250', 'ALLOC P4 100', 'FREE P1', 'FREE P2']
    },
    compaction: {
      totalMemory: 1000,
      strategy: 'FIRST_FIT',
      commands: [
        'ALLOC P1 100', 'ALLOC P2 200', 'ALLOC P3 100',
        'ALLOC P4 300', 'FREE P2', 'FREE P4', 'ALLOC P5 700', 'COMPACT'
      ]
    }
  };

  // DOM Elements
  const el = {
    totalMemoryInput: document.getElementById('totalMemoryInput'),
    applyMemoryBtn: document.getElementById('applyMemoryBtn'),
    strategySelect: document.getElementById('strategySelect'),
    presetSelect: document.getElementById('presetSelect'),
    loadingIndicator: document.getElementById('loadingIndicator'),
    errorBanner: document.getElementById('errorBanner'),
    statusToast: document.getElementById('statusToast'),
    toastIcon: document.getElementById('toastIcon'),
    toastMessage: document.getElementById('toastMessage'),
    toastCloseBtn: document.getElementById('toastCloseBtn'),
    highlightInfo: document.getElementById('highlightInfo'),

    // ALLOC
    allocPidInput: document.getElementById('allocPidInput'),
    allocSizeInput: document.getElementById('allocSizeInput'),
    allocBtn: document.getElementById('allocBtn'),

    // FREE
    freePidSelect: document.getElementById('freePidSelect'),
    freeBtn: document.getElementById('freeBtn'),

    // FIND
    findPidInput: document.getElementById('findPidInput'),
    findBtn: document.getElementById('findBtn'),

    // Actions
    compactBtn: document.getElementById('compactBtn'),
    fragBtn: document.getElementById('fragBtn'),
    displayAllBtn: document.getElementById('displayAllBtn'),
    displayAllocatedBtn: document.getElementById('displayAllocatedBtn'),
    displayFreeBtn: document.getElementById('displayFreeBtn'),
    undoBtn: document.getElementById('undoBtn'),
    resetBtn: document.getElementById('resetBtn'),
    compareBtn: document.getElementById('compareBtn'),

    // Script
    scriptInput: document.getElementById('scriptInput'),
    runScriptBtn: document.getElementById('runScriptBtn'),
    clearScriptBtn: document.getElementById('clearScriptBtn'),

    // Views
    memoryBar: document.getElementById('memoryBar'),
    linkedListView: document.getElementById('linkedListView'),
    hashTableView: document.getElementById('hashTableView'),
    queueView: document.getElementById('queueView'),
    blockTableBody: document.getElementById('blockTableBody'),
    filterAllBtn: document.getElementById('filterAllBtn'),
    filterAllocBtn: document.getElementById('filterAllocBtn'),
    filterFreeBtn: document.getElementById('filterFreeBtn'),
    logContainer: document.getElementById('logContainer'),
    logCount: document.getElementById('logCount'),

    // Stats
    statUsed: document.getElementById('statUsed'),
    statFree: document.getElementById('statFree'),
    statUtil: document.getElementById('statUtil'),
    statLargestFree: document.getElementById('statLargestFree'),
    statFreeBlocks: document.getElementById('statFreeBlocks'),
    statAllocBlocks: document.getElementById('statAllocBlocks'),
    statFrag: document.getElementById('statFrag'),
    statWaiting: document.getElementById('statWaiting'),
    statFragCard: document.getElementById('statFragCard'),
    blockTableCard: document.getElementById('blockTableCard'),

    // Compare
    compareSection: document.getElementById('compareSection'),
    closeCompareBtn: document.getElementById('closeCompareBtn'),
    compareBarFF: document.getElementById('compareBarFF'),
    compareStatsFF: document.getElementById('compareStatsFF'),
    compareBarBF: document.getElementById('compareBarBF'),
    compareStatsBF: document.getElementById('compareStatsBF')
  };

  // Helper: PID to Color
  function pidToColor(pid) {
    if (!pid) return '#334155';
    let hash = 0;
    for (let i = 0; i < pid.length; i++) {
      hash = pid.charCodeAt(i) + ((hash << 5) - hash);
    }
    const hue = Math.abs(hash) % 360;
    return `hsl(${hue}, 70%, 42%)`;
  }

  // Helper: UI Disable / Enable
  function setControlsDisabled(disabled) {
    state.inFlight = disabled;
    el.loadingIndicator.classList.toggle('hidden', !disabled);
    const buttons = document.querySelectorAll('button, select, input');
    buttons.forEach(b => {
      if (b !== el.closeCompareBtn) {
        b.disabled = disabled;
      }
    });
  }

  function showError(msg) {
    if (!msg) {
      el.errorBanner.classList.add('hidden');
      el.errorBanner.textContent = '';
    } else {
      el.errorBanner.classList.remove('hidden');
      el.errorBanner.textContent = msg;
    }
  }

  let toastTimer = null;
  function showToast(status, message, details) {
    if (!el.statusToast) return;
    if (!message) {
      el.statusToast.classList.add('hidden');
      return;
    }
    clearTimeout(toastTimer);
    el.statusToast.className = `status-toast status-${status}`;

    let icon = 'ℹ️';
    if (status === 'OK') icon = '✅';
    else if (status === 'QUEUED') icon = '⏳';
    else if (status === 'ERROR') icon = '⚠️';
    else if (status === 'INFO') {
      if (message.includes('Fragmentation')) icon = '📊';
      else if (message.includes('found in hash')) icon = '🔍';
      else if (message.includes('Allocated') || message.includes('Free') || message.includes('layout')) icon = '📋';
    }
    el.toastIcon.textContent = icon;

    let text = message;
    if (details && details.length > 0) {
      text += ' (' + details.join(', ') + ')';
    }
    el.toastMessage.textContent = text;
    el.statusToast.classList.remove('hidden');

    toastTimer = setTimeout(() => {
      el.statusToast.classList.add('hidden');
    }, 7000);
  }

  // Next PID suggestion
  function suggestNextPid(blocks, queue) {
    let maxNum = 0;
    const checkPid = (pid) => {
      if (pid && /^P(\d+)$/i.test(pid)) {
        const n = parseInt(pid.slice(1), 10);
        if (n > maxNum) maxNum = n;
      }
    };
    if (blocks) blocks.forEach(b => checkPid(b.pid));
    if (queue) queue.forEach(q => checkPid(q.pid));
    el.allocPidInput.value = `P${maxNum + 1}`;
  }

  // Update Free dropdown
  function updateFreeDropdown(blocks, queue) {
    const pids = new Set();
    if (blocks) {
      blocks.forEach(b => { if (b.status === 'ALLOCATED' && b.pid) pids.add(b.pid); });
    }
    if (queue) {
      queue.forEach(q => { if (q.pid) pids.add(q.pid); });
    }

    el.freePidSelect.innerHTML = '';
    if (pids.size === 0) {
      const opt = document.createElement('option');
      opt.textContent = 'No processes';
      opt.disabled = true;
      opt.selected = true;
      el.freePidSelect.appendChild(opt);
      el.freeBtn.disabled = true;
    } else {
      pids.forEach(pid => {
        const opt = document.createElement('option');
        opt.value = pid;
        opt.textContent = pid;
        el.freePidSelect.appendChild(opt);
      });
      el.freeBtn.disabled = state.inFlight;
    }
  }

  // Highlight selection handler
  function setHighlightedPid(pid) {
    if (state.highlightedPid === pid) {
      state.highlightedPid = null;
      el.highlightInfo.classList.add('hidden');
      el.highlightInfo.textContent = '';
    } else {
      state.highlightedPid = pid;
      if (pid) {
        el.highlightInfo.classList.remove('hidden');
        el.highlightInfo.textContent = `Highlighted Process: ${pid} (Click again to clear)`;
        el.findPidInput.value = pid;
      } else {
        el.highlightInfo.classList.add('hidden');
      }
    }
    if (state.lastResponse) {
      renderMemoryBar(state.lastResponse.blocks, state.lastResponse.config.totalMemory);
      renderLinkedList(state.lastResponse.blocks);
      renderHashTable(state.lastResponse.hashTable);
      renderBlockTable(state.lastResponse.blocks, state.blockFilter);
    }
  }

  // Rendering Functions
  function renderMemoryBar(blocks, totalMemory) {
    el.memoryBar.innerHTML = '';
    if (!blocks || blocks.length === 0) return;

    blocks.forEach(b => {
      const seg = document.createElement('div');
      seg.className = 'mem-segment';
      const pct = (b.size / totalMemory) * 100;
      seg.style.width = `${pct}%`;
      seg.style.minWidth = '14px';

      if (b.status === 'FREE') {
        seg.classList.add('is-free');
        seg.title = `FREE: ${b.start}-${b.end} (${b.size} units)`;
        if (pct > 6) seg.textContent = `${b.size}`;
      } else {
        seg.style.backgroundColor = pidToColor(b.pid);
        seg.title = `${b.pid}: ${b.start}-${b.end} (${b.size} units)`;
        if (pct > 5) seg.textContent = b.pid;
        if (state.highlightedPid === b.pid) {
          seg.classList.add('highlighted');
        }
        seg.addEventListener('click', () => setHighlightedPid(b.pid));
      }

      el.memoryBar.appendChild(seg);
    });
  }

  function renderStats(s) {
    if (!s) return;
    el.statUsed.textContent = s.usedMemory;
    el.statFree.textContent = s.freeMemory;
    el.statUtil.textContent = `${s.utilizationPercent.toFixed(2)}%`;
    el.statLargestFree.textContent = s.largestFreeBlock;
    el.statFreeBlocks.textContent = s.freeBlockCount;
    el.statAllocBlocks.textContent = s.allocatedBlockCount;
    el.statFrag.textContent = `${s.externalFragmentationPercent.toFixed(2)}%`;
    el.statWaiting.textContent = s.waitingCount;
  }

  function renderLinkedList(blocks) {
    el.linkedListView.innerHTML = '';
    if (!blocks || blocks.length === 0) return;

    blocks.forEach((b, idx) => {
      const node = document.createElement('div');
      node.className = 'll-node';
      if (b.status === 'FREE') {
        node.classList.add('is-free');
        node.innerHTML = `<strong>FREE</strong><span>${b.start}..${b.end}</span><span>(${b.size} u)</span>`;
      } else {
        node.style.borderColor = pidToColor(b.pid);
        node.innerHTML = `<strong style="color:${pidToColor(b.pid)}">${b.pid}</strong><span>${b.start}..${b.end}</span><span>(${b.size} u)</span>`;
        if (state.highlightedPid === b.pid) {
          node.classList.add('highlighted');
        }
        node.addEventListener('click', () => setHighlightedPid(b.pid));
      }
      el.linkedListView.appendChild(node);

      if (idx < blocks.length - 1) {
        const arrow = document.createElement('span');
        arrow.className = 'll-arrow';
        arrow.textContent = '⇄';
        el.linkedListView.appendChild(arrow);
      }
    });
  }

  function renderHashTable(ht) {
    el.hashTableView.innerHTML = '';
    if (!ht || !ht.buckets) return;

    ht.buckets.forEach(b => {
      const bucket = document.createElement('div');
      bucket.className = 'ht-bucket';

      const header = document.createElement('div');
      header.className = 'ht-bucket-header';
      header.innerHTML = `<span>Bucket ${b.index}</span><span>(${b.keys.length})</span>`;
      bucket.appendChild(header);

      const keysDiv = document.createElement('div');
      keysDiv.className = 'ht-keys';

      if (b.keys.length === 0) {
        const empty = document.createElement('span');
        empty.className = 'ht-empty';
        empty.textContent = 'empty';
        keysDiv.appendChild(empty);
      } else {
        b.keys.forEach(k => {
          const chip = document.createElement('span');
          chip.className = 'ht-chip';
          chip.textContent = k;
          if (state.highlightedPid === k) {
            chip.classList.add('highlighted');
          }
          chip.title = `Click to highlight process ${k}`;
          chip.addEventListener('click', (e) => {
            e.stopPropagation();
            setHighlightedPid(k);
          });
          keysDiv.appendChild(chip);
        });
      }

      bucket.appendChild(keysDiv);
      el.hashTableView.appendChild(bucket);
    });
  }

  function renderQueue(queue) {
    el.queueView.innerHTML = '';
    if (!queue || queue.length === 0) {
      const empty = document.createElement('span');
      empty.className = 'queue-empty';
      empty.textContent = '[Waiting queue is currently empty]';
      el.queueView.appendChild(empty);
      return;
    }

    const tagFront = document.createElement('span');
    tagFront.className = 'queue-tag';
    tagFront.textContent = 'FRONT ➔';
    el.queueView.appendChild(tagFront);

    queue.forEach((q, idx) => {
      const box = document.createElement('div');
      box.className = 'queue-box';
      box.textContent = `#${q.position}: ${q.pid} (${q.size} units)`;
      el.queueView.appendChild(box);

      if (idx < queue.length - 1) {
        const arrow = document.createElement('span');
        arrow.className = 'queue-arrow';
        arrow.textContent = '➔';
        el.queueView.appendChild(arrow);
      }
    });

    const tagBack = document.createElement('span');
    tagBack.className = 'queue-tag';
    tagBack.textContent = '➔ BACK';
    el.queueView.appendChild(tagBack);
  }

  function renderBlockTable(blocks, filter) {
    el.blockTableBody.innerHTML = '';
    if (!blocks || blocks.length === 0) return;

    blocks.forEach(b => {
      if (filter === 'ALLOCATED' && b.status !== 'ALLOCATED') return;
      if (filter === 'FREE' && b.status !== 'FREE') return;

      const tr = document.createElement('tr');
      if (b.status === 'ALLOCATED' && state.highlightedPid === b.pid) {
        tr.classList.add('highlighted');
      }

      tr.innerHTML = `
        <td>${b.index}</td>
        <td>${b.start}</td>
        <td>${b.end}</td>
        <td>${b.size}</td>
        <td><span class="status-pill ${b.status === 'FREE' ? 'free' : 'allocated'}">${b.status}</span></td>
        <td>${b.status === 'ALLOCATED' ? `<strong>${b.pid}</strong>` : '-'}</td>
        <td>
          ${b.status === 'ALLOCATED' ? `<button class="btn btn-outline" style="padding:0.2rem 0.5rem;font-size:0.75rem;" data-free-pid="${b.pid}">Free</button>` : ''}
        </td>
      `;

      if (b.status === 'ALLOCATED') {
        const freeBtn = tr.querySelector('[data-free-pid]');
        if (freeBtn) {
          freeBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            appendCommand(`FREE ${b.pid}`);
          });
        }
        tr.style.cursor = 'pointer';
        tr.addEventListener('click', () => setHighlightedPid(b.pid));
      }

      el.blockTableBody.appendChild(tr);
    });
  }

  function renderLog(log) {
    el.logContainer.innerHTML = '';
    if (!log || log.length === 0) {
      el.logContainer.innerHTML = '<div class="log-empty">No commands executed yet.</div>';
      el.logCount.textContent = '0 commands executed';
      return;
    }

    el.logCount.textContent = `${log.length} command${log.length === 1 ? '' : 's'} executed`;

    log.forEach(entry => {
      const item = document.createElement('div');
      item.className = `log-entry status-${entry.status}`;

      const header = document.createElement('div');
      header.className = 'log-header-line';

      const badge = document.createElement('span');
      badge.className = `log-badge status-${entry.status}`;
      badge.textContent = entry.status;

      const cmd = document.createElement('span');
      cmd.className = 'log-cmd';
      cmd.textContent = `#${entry.line} ${entry.command}`;

      const msg = document.createElement('span');
      msg.className = 'log-msg';
      msg.textContent = entry.message;

      header.appendChild(badge);
      header.appendChild(cmd);
      header.appendChild(msg);
      item.appendChild(header);

      if (entry.details && entry.details.length > 0) {
        const detList = document.createElement('div');
        detList.className = 'log-details';
        entry.details.forEach(d => {
          const p = document.createElement('div');
          p.textContent = `↳ ${d}`;
          detList.appendChild(p);
        });
        item.appendChild(detList);
      }

      el.logContainer.appendChild(item);
    });

    el.logContainer.scrollTop = el.logContainer.scrollHeight;
  }

  // Execute Simulation API
  async function runSimulationApi(totalMemory, strategy, commands) {
    const payload = { totalMemory, strategy, commands };
    const res = await fetch('/api/simulate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });

    const data = await res.json();
    if (!res.ok || !data.ok) {
      throw new Error(data.error || `HTTP ${res.status} Error`);
    }
    return data;
  }

  // Update simulation state and refresh UI
  async function updateSimulation(showToastForLast = false) {
    showError(null);
    setControlsDisabled(true);

    try {
      const data = await runSimulationApi(state.totalMemory, state.initialStrategy, state.commands);
      state.lastResponse = data;

      // Sync active strategy dropdown
      if (data.config && data.config.strategy) {
        el.strategySelect.value = data.config.strategy;
      }

      renderMemoryBar(data.blocks, data.config.totalMemory);
      renderStats(data.stats);
      renderLinkedList(data.blocks);
      renderHashTable(data.hashTable);
      renderQueue(data.waitingQueue);
      renderBlockTable(data.blocks, state.blockFilter);
      renderLog(data.log);

      updateFreeDropdown(data.blocks, data.waitingQueue);
      suggestNextPid(data.blocks, data.waitingQueue);

      if (showToastForLast && data.log && data.log.length > 0) {
        const lastEntry = data.log[data.log.length - 1];
        showToast(lastEntry.status, lastEntry.message, lastEntry.details);
      }
    } catch (err) {
      showError(err.message || 'Failed to communicate with simulation engine');
      showToast('ERROR', err.message || 'Simulation execution failed');
    } finally {
      setControlsDisabled(false);
    }
  }

  function appendCommand(cmd, showToastForLast = true) {
    if (!cmd || !cmd.trim()) return;
    state.commands.push(cmd.trim());
    updateSimulation(showToastForLast);
  }

  // Strategy Comparison Runner
  async function runComparison() {
    showError(null);
    setControlsDisabled(true);

    try {
      // Filter out any STRATEGY commands so both run identical base scripts
      const filtered = state.commands.filter(c => !c.trim().toUpperCase().startsWith('STRATEGY'));

      const [resFF, resBF] = await Promise.all([
        runSimulationApi(state.totalMemory, 'FIRST_FIT', filtered),
        runSimulationApi(state.totalMemory, 'BEST_FIT', filtered)
      ]);

      el.compareSection.classList.remove('hidden');

      // Render FF
      el.compareBarFF.innerHTML = '';
      resFF.blocks.forEach(b => {
        const seg = document.createElement('div');
        seg.className = 'mem-segment';
        seg.style.width = `${(b.size / resFF.config.totalMemory) * 100}%`;
        seg.style.minWidth = '10px';
        if (b.status === 'FREE') {
          seg.classList.add('is-free');
        } else {
          seg.style.backgroundColor = pidToColor(b.pid);
          seg.textContent = b.pid;
        }
        el.compareBarFF.appendChild(seg);
      });
      el.compareStatsFF.innerHTML = `
        <div><strong>Used:</strong> ${resFF.stats.usedMemory} units (${resFF.stats.utilizationPercent}%)</div>
        <div><strong>Free:</strong> ${resFF.stats.freeMemory} units in ${resFF.stats.freeBlockCount} blocks</div>
        <div><strong>Largest Free Block:</strong> ${resFF.stats.largestFreeBlock} units</div>
        <div><strong>External Fragmentation:</strong> ${resFF.stats.externalFragmentationPercent}%</div>
        <div><strong>Waiting in Queue:</strong> ${resFF.stats.waitingCount}</div>
      `;

      // Render BF
      el.compareBarBF.innerHTML = '';
      resBF.blocks.forEach(b => {
        const seg = document.createElement('div');
        seg.className = 'mem-segment';
        seg.style.width = `${(b.size / resBF.config.totalMemory) * 100}%`;
        seg.style.minWidth = '10px';
        if (b.status === 'FREE') {
          seg.classList.add('is-free');
        } else {
          seg.style.backgroundColor = pidToColor(b.pid);
          seg.textContent = b.pid;
        }
        el.compareBarBF.appendChild(seg);
      });
      el.compareStatsBF.innerHTML = `
        <div><strong>Used:</strong> ${resBF.stats.usedMemory} units (${resBF.stats.utilizationPercent}%)</div>
        <div><strong>Free:</strong> ${resBF.stats.freeMemory} units in ${resBF.stats.freeBlockCount} blocks</div>
        <div><strong>Largest Free Block:</strong> ${resBF.stats.largestFreeBlock} units</div>
        <div><strong>External Fragmentation:</strong> ${resBF.stats.externalFragmentationPercent}%</div>
        <div><strong>Waiting in Queue:</strong> ${resBF.stats.waitingCount}</div>
      `;

    } catch (err) {
      showError(`Comparison error: ${err.message}`);
    } finally {
      setControlsDisabled(false);
    }
  }

  // Event Listeners
  function attachEventListeners() {
    // Apply Total Memory
    el.applyMemoryBtn.addEventListener('click', () => {
      const val = parseInt(el.totalMemoryInput.value, 10);
      if (isNaN(val) || val < 64 || val > 100000) {
        showError('Total memory must be an integer between 64 and 100000 units');
        return;
      }
      if (state.commands.length > 0) {
        if (!confirm('Changing total memory will reset all current commands. Continue?')) {
          el.totalMemoryInput.value = state.totalMemory;
          return;
        }
      }
      state.totalMemory = val;
      state.commands = [];
      updateSimulation();
    });

    // Strategy Change
    el.strategySelect.addEventListener('change', () => {
      const newStrat = el.strategySelect.value;
      if (state.commands.length === 0) {
        state.initialStrategy = newStrat;
        updateSimulation();
      } else {
        appendCommand(`STRATEGY ${newStrat}`);
      }
    });

    // Preset Selection
    el.presetSelect.addEventListener('change', () => {
      const key = el.presetSelect.value;
      const preset = PRESETS[key];
      if (!preset) return;

      state.totalMemory = preset.totalMemory;
      state.initialStrategy = preset.strategy;
      state.commands = [...preset.commands];
      el.totalMemoryInput.value = preset.totalMemory;
      el.strategySelect.value = preset.strategy;

      updateSimulation();
    });

    if (el.toastCloseBtn) {
      el.toastCloseBtn.addEventListener('click', () => {
        el.statusToast.classList.add('hidden');
        clearTimeout(toastTimer);
      });
    }

    // ALLOC
    el.allocBtn.addEventListener('click', () => {
      const pid = (el.allocPidInput.value || '').trim();
      const size = parseInt(el.allocSizeInput.value, 10);
      if (!pid) {
        showToast('ERROR', 'Please enter a Process ID');
        return;
      }
      if (isNaN(size) || size < 1) {
        showToast('ERROR', 'Allocation size must be at least 1 unit');
        return;
      }
      appendCommand(`ALLOC ${pid} ${size}`, true);
    });

    // FREE
    el.freeBtn.addEventListener('click', () => {
      const pid = el.freePidSelect.value;
      if (!pid) return;
      appendCommand(`FREE ${pid}`, true);
    });

    // FIND
    el.findBtn.addEventListener('click', () => {
      const pid = (el.findPidInput.value || '').trim();
      if (!pid) {
        showToast('ERROR', 'Please enter a Process ID to find');
        return;
      }
      appendCommand(`FIND ${pid}`, true);
      setHighlightedPid(pid);
    });

    // Actions
    el.compactBtn.addEventListener('click', () => appendCommand('COMPACT', true));

    // FRAG
    el.fragBtn.addEventListener('click', () => {
      appendCommand('FRAG', true);
      if (el.statFragCard) {
        el.statFragCard.classList.remove('pulse-highlight');
        void el.statFragCard.offsetWidth; // Trigger reflow to restart animation
        el.statFragCard.classList.add('pulse-highlight');
      }
    });

    // DISPLAY ALL
    el.displayAllBtn.addEventListener('click', () => {
      setFilter('ALL');
      appendCommand('DISPLAY', true);
      setTimeout(() => {
        if (el.blockTableCard) {
          el.blockTableCard.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
        }
      }, 120);
    });

    // DISPLAY ALLOCATED
    el.displayAllocatedBtn.addEventListener('click', () => {
      setFilter('ALLOCATED');
      appendCommand('DISPLAY ALLOCATED', true);
      setTimeout(() => {
        if (el.blockTableCard) {
          el.blockTableCard.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
        }
      }, 120);
    });

    // DISPLAY FREE
    el.displayFreeBtn.addEventListener('click', () => {
      setFilter('FREE');
      appendCommand('DISPLAY FREE', true);
      setTimeout(() => {
        if (el.blockTableCard) {
          el.blockTableCard.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
        }
      }, 120);
    });

    el.undoBtn.addEventListener('click', () => {
      if (state.commands.length > 0) {
        const undone = state.commands.pop();
        updateSimulation(false);
        showToast('INFO', `Undid command: ${undone}`);
      } else {
        showToast('INFO', 'No commands to undo');
      }
    });

    el.resetBtn.addEventListener('click', () => {
      if (state.commands.length === 0 || confirm('Reset all memory and clear history?')) {
        state.commands = [];
        state.highlightedPid = null;
        updateSimulation(false);
        showToast('INFO', 'Memory reset to initial empty state (1000 free units)');
      }
    });

    // Compare
    el.compareBtn.addEventListener('click', runComparison);
    el.closeCompareBtn.addEventListener('click', () => {
      el.compareSection.classList.add('hidden');
    });

    // Script Runner
    el.runScriptBtn.addEventListener('click', () => {
      const text = el.scriptInput.value;
      if (!text || !text.trim()) return;
      const lines = text.split(/\r?\n/).map(l => l.trim()).filter(l => l && !l.startsWith('#'));
      if (lines.length === 0) return;
      state.commands.push(...lines);
      el.scriptInput.value = '';
      updateSimulation();
    });

    el.clearScriptBtn.addEventListener('click', () => {
      el.scriptInput.value = '';
    });

    // Filter Tabs
    function setFilter(filter) {
      state.blockFilter = filter;
      el.filterAllBtn.classList.toggle('active', filter === 'ALL');
      el.filterAllocBtn.classList.toggle('active', filter === 'ALLOCATED');
      el.filterFreeBtn.classList.toggle('active', filter === 'FREE');
      if (state.lastResponse) {
        renderBlockTable(state.lastResponse.blocks, state.blockFilter);
      }
    }

    el.filterAllBtn.addEventListener('click', () => setFilter('ALL'));
    el.filterAllocBtn.addEventListener('click', () => setFilter('ALLOCATED'));
    el.filterFreeBtn.addEventListener('click', () => setFilter('FREE'));
  }

  // Initialize
  function init() {
    attachEventListeners();
    updateSimulation();
  }

  document.addEventListener('DOMContentLoaded', init);
})();
