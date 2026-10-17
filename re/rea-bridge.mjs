// Keeps one REA MCP server (stdio) alive and exposes it on localhost so a
// long Ghidra analysis is paid once per binary instead of once per query.
//
//   source X:/tools/rea-env.sh && node re/rea-bridge.mjs            (port 47815)
//   curl -s localhost:47815/tools
//   curl -s localhost:47815/call -d '{"name":"open_binary","arguments":{...}}'
//
// Every call's request and result is also written to re/out/NNN-<tool>.json.
import { spawn } from 'node:child_process';
import { mkdirSync, writeFileSync } from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const PORT = Number(process.env.NWN5E_BRIDGE_PORT ?? 47815);
const REA = process.env.NWN5E_REA_ENTRY ?? 'X:/tools/rea/node_modules/rea-agents/scripts/rea.mjs';
const OUT = path.join(path.dirname(fileURLToPath(import.meta.url)), 'out');
mkdirSync(OUT, { recursive: true });

const child = spawn(process.execPath, [REA, 'mcp'], { env: process.env, stdio: ['pipe', 'pipe', 'pipe'] });
child.on('exit', (code) => {
  console.error(`[bridge] rea mcp exited with ${code}`);
  process.exit(code ?? 1);
});
child.stderr.on('data', (d) => process.stderr.write(`[rea] ${d}`));

const pending = new Map();
let nextId = 0;
let callSeq = 0;
let buffer = '';

function send(message) {
  child.stdin.write(`${JSON.stringify(message)}\n`);
}

function rpc(method, params) {
  return new Promise((resolve) => {
    const id = ++nextId;
    pending.set(id, resolve);
    send({ jsonrpc: '2.0', id, method, params });
  });
}

function onServerRequest(message) {
  // The bridge advertises no client capabilities; answer the few generic requests.
  if (message.method === 'ping') return send({ jsonrpc: '2.0', id: message.id, result: {} });
  if (message.method === 'roots/list') return send({ jsonrpc: '2.0', id: message.id, result: { roots: [] } });
  send({ jsonrpc: '2.0', id: message.id, error: { code: -32601, message: `unsupported: ${message.method}` } });
}

child.stdout.on('data', (chunk) => {
  buffer += chunk;
  let newline;
  while ((newline = buffer.indexOf('\n')) >= 0) {
    const line = buffer.slice(0, newline).trim();
    buffer = buffer.slice(newline + 1);
    if (!line) continue;
    let message;
    try {
      message = JSON.parse(line);
    } catch {
      console.error(`[bridge] unparsable line: ${line.slice(0, 200)}`);
      continue;
    }
    if (message.method && message.id !== undefined) onServerRequest(message);
    else if (message.method) console.error(`[notify] ${message.method} ${JSON.stringify(message.params ?? {}).slice(0, 300)}`);
    else if (pending.has(message.id)) {
      pending.get(message.id)(message);
      pending.delete(message.id);
    }
  }
});

const init = await rpc('initialize', {
  protocolVersion: '2025-06-18',
  capabilities: {},
  clientInfo: { name: 'nwn5e-bridge', version: '0.1.0' },
});
if (init.error) {
  console.error(`[bridge] initialize failed: ${JSON.stringify(init.error)}`);
  process.exit(1);
}
send({ jsonrpc: '2.0', method: 'notifications/initialized' });
console.error(`[bridge] connected to ${init.result?.serverInfo?.name} ${init.result?.serverInfo?.version}`);

function readBody(req) {
  return new Promise((resolve, reject) => {
    let body = '';
    req.on('data', (d) => (body += d));
    req.on('end', () => resolve(body));
    req.on('error', reject);
  });
}

http
  .createServer(async (req, res) => {
    try {
      let reply;
      if (req.method === 'GET' && req.url === '/tools') {
        reply = await rpc('tools/list', {});
      } else if (req.method === 'POST' && req.url === '/call') {
        const { name, arguments: args = {} } = JSON.parse(await readBody(req));
        const seq = String(++callSeq).padStart(3, '0');
        const started = Date.now();
        reply = await rpc('tools/call', { name, arguments: args });
        writeFileSync(
          path.join(OUT, `${seq}-${name}.json`),
          JSON.stringify({ request: { name, arguments: args }, elapsed_ms: Date.now() - started, reply }, null, 2),
        );
      } else {
        res.writeHead(404).end();
        return;
      }
      res.writeHead(200, { 'content-type': 'application/json' }).end(JSON.stringify(reply));
    } catch (error) {
      res.writeHead(500, { 'content-type': 'text/plain' }).end(String(error?.stack ?? error));
    }
  })
  .listen(PORT, '127.0.0.1', () => console.error(`[bridge] listening on 127.0.0.1:${PORT}`));
