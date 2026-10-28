// api/simulate.js
// Vercel Serverless Function entry point (Node.js, CommonJS)

const { runSimulation } = require('../lib/vmmBridge');

module.exports = async function handler(req, res) {
  res.setHeader('Cache-Control', 'no-store');

  if (req.method === 'GET') {
    return res.status(200).json({
      status: 'ok',
      engine: 'C++17 compiled to WebAssembly'
    });
  }

  if (req.method !== 'POST') {
    res.setHeader('Allow', 'GET, POST');
    return res.status(405).json({
      ok: false,
      error: 'Method not allowed'
    });
  }

  let body = req.body;
  if (typeof body === 'string') {
    try {
      body = JSON.parse(body);
    } catch {
      body = null;
    }
  }

  if (!body || typeof body !== 'object') {
    return res.status(400).json({
      ok: false,
      error: 'Request body must be a valid JSON object'
    });
  }

  const { totalMemory, strategy, commands } = body;

  if (typeof totalMemory !== 'number' || !Number.isInteger(totalMemory)) {
    return res.status(400).json({
      ok: false,
      error: 'totalMemory must be an integer'
    });
  }

  if (typeof strategy !== 'string' || !strategy.trim()) {
    return res.status(400).json({
      ok: false,
      error: 'strategy must be a non-empty string'
    });
  }

  if (!Array.isArray(commands)) {
    return res.status(400).json({
      ok: false,
      error: 'commands must be an array of strings'
    });
  }

  if (commands.length > 1000) {
    return res.status(400).json({
      ok: false,
      error: 'commands array exceeds maximum limit of 1000 items'
    });
  }

  for (let i = 0; i < commands.length; i++) {
    const cmd = commands[i];
    if (typeof cmd !== 'string') {
      return res.status(400).json({
        ok: false,
        error: `Command at index ${i} must be a string`
      });
    }
    if (cmd.length > 100) {
      return res.status(400).json({
        ok: false,
        error: `Command at index ${i} exceeds maximum length of 100 characters`
      });
    }
    if (cmd.includes('\n') || cmd.includes('\r')) {
      return res.status(400).json({
        ok: false,
        error: `Command at index ${i} contains illegal newline characters`
      });
    }
  }

  try {
    const commandsText = commands.join('\n');
    const rawResult = await runSimulation(totalMemory, strategy, commandsText);
    const result = JSON.parse(rawResult);
    return res.status(result.ok ? 200 : 400).json(result);
  } catch (err) {
    console.error('Simulation execution error:', err);
    return res.status(500).json({
      ok: false,
      error: 'Internal simulation engine error'
    });
  }
};
