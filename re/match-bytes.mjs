// Finds nwmain.exe's copy of named nwserver.exe functions by masked byte signatures.
// Both executables are the same MSVC 6 build of the same server sources, so a
// function's bytes match except for call/jump targets and absolute addresses.
//
//   node re/match-bytes.mjs <nwserver.exe> <nwmain.exe> <seeds.tsv> > re/maps/nwmain-1.69-bytematch.tsv
//
// Output columns: server_addr  name  nwmain_addr  status  signature_bytes
// status: unique | ambiguous(N) | none
import { readFileSync } from 'node:fs';

const [serverPath, mainPath, seedsPath] = process.argv.slice(2);
const LENGTHS = [24, 48, 96, 160, 256];

function loadPe(file) {
  const bytes = readFileSync(file);
  const pe = bytes.readUInt32LE(0x3c);
  const opt = pe + 24;
  const imageBase = bytes.readUInt32LE(opt + 28);
  const imageSize = bytes.readUInt32LE(opt + 56);
  const sectionCount = bytes.readUInt16LE(pe + 6);
  let header = opt + bytes.readUInt16LE(pe + 20);
  let text;
  for (let i = 0; i < sectionCount; i++, header += 40) {
    const name = bytes.subarray(header, header + 8).toString('latin1').replace(/\0.*$/, '');
    if (name === '.text') {
      text = { va: bytes.readUInt32LE(header + 12), raw: bytes.readUInt32LE(header + 20), size: bytes.readUInt32LE(header + 16) };
    }
  }
  const code = bytes.subarray(text.raw, text.raw + text.size);
  return {
    code,
    imageBase,
    imageEnd: imageBase + imageSize,
    toOffset: (va) => va - imageBase - text.va,
    toVa: (offset) => offset + imageBase + text.va,
  };
}

function signature(pe, va, length) {
  const offset = pe.toOffset(va);
  const bytes = pe.code.subarray(offset, offset + length);
  const keep = new Array(bytes.length).fill(true);
  for (let i = 0; i < bytes.length; i++) {
    // rel32 operands of call/jmp point at code that moved.
    if ((bytes[i] === 0xe8 || bytes[i] === 0xe9) && i + 5 <= bytes.length) for (let k = 1; k < 5; k++) keep[i + k] = false;
    // Any 32-bit value inside the server image is an absolute address (globals, vtables, jump tables).
    if (i + 4 <= bytes.length) {
      const value = bytes.readUInt32LE(i);
      if (value >= pe.imageBase && value < pe.imageEnd) for (let k = 0; k < 4; k++) keep[i + k] = false;
    }
  }
  return { bytes, keep };
}

function anchor(sig) {
  // Longest run of kept bytes, used with indexOf before a full masked compare.
  let best = { start: 0, length: 0 };
  for (let i = 0; i < sig.bytes.length; ) {
    if (!sig.keep[i]) { i++; continue; }
    let j = i;
    while (j < sig.bytes.length && sig.keep[j]) j++;
    if (j - i > best.length) best = { start: i, length: j - i };
    i = j;
  }
  return best;
}

function find(main, sig, limit = 8) {
  const a = anchor(sig);
  if (a.length < 4) return [];
  const needle = sig.bytes.subarray(a.start, a.start + a.length);
  const hits = [];
  for (let at = main.code.indexOf(needle); at >= 0; at = main.code.indexOf(needle, at + 1)) {
    const start = at - a.start;
    if (start < 0 || start + sig.bytes.length > main.code.length) continue;
    let ok = true;
    for (let i = 0; i < sig.bytes.length && ok; i++) if (sig.keep[i] && main.code[start + i] !== sig.bytes[i]) ok = false;
    if (ok) hits.push(main.toVa(start));
    if (hits.length >= limit) break;
  }
  return hits;
}

const server = loadPe(serverPath);
const main = loadPe(mainPath);
const seeds = readFileSync(seedsPath, 'utf8')
  .split(/\r?\n/)
  .filter((line) => line && !line.startsWith('#'))
  .map((line) => line.split('\t'))
  .map(([address, name]) => ({ va: parseInt(address, 16), name }));

const hex = (n) => `0x${n.toString(16).toUpperCase()}`;
const results = seeds.map(({ va, name }) => {
  let result = { va, name, status: 'none', hits: [], length: 0 };
  for (const length of LENGTHS) {
    const hits = find(main, signature(server, va, length));
    if (hits.length === 1) { result = { va, name, status: 'unique', hits, length }; break; }
    if (hits.length > 1) result = { va, name, status: `ambiguous(${hits.length})`, hits, length };
    if (hits.length === 0) break; // longer signatures only match less
  }
  return result;
});

// Pass 2: the linker keeps each object file's functions in order, so two server
// functions from the same object file sit the same distance apart in nwmain.
// A unique match whose distance to its nearest unique neighbour is preserved is
// "confirmed"; ambiguous/none entries are resolved from a confirmed neighbour's
// distance when the predicted address starts a function (preceded by padding).
const NEIGHBOUR_WINDOW = 0x8000;
const byServer = [...results].sort((a, b) => a.va - b.va);
const unique = byServer.filter((r) => r.status === 'unique');
for (const r of unique) {
  const i = unique.indexOf(r);
  const neighbours = [unique[i - 1], unique[i + 1]].filter((n) => n && Math.abs(n.va - r.va) <= NEIGHBOUR_WINDOW);
  r.confirmed = neighbours.some((n) => n.va - r.va === n.hits[0] - r.hits[0]);
}
const startsFunction = (va) => {
  const before = main.code[main.toOffset(va) - 1];
  return before === 0x90 || before === 0xcc || before === 0xc3;
};
for (const r of byServer.filter((x) => x.status !== 'unique')) {
  const anchors = unique
    .filter((n) => n.confirmed && Math.abs(n.va - r.va) <= NEIGHBOUR_WINDOW)
    .sort((a, b) => Math.abs(a.va - r.va) - Math.abs(b.va - r.va));
  const predicted = anchors.length ? anchors[0].hits[0] + (r.va - anchors[0].va) : undefined;
  if (predicted === undefined || !startsFunction(predicted)) continue;
  if (r.hits.includes(predicted) || r.status === 'none') {
    r.resolvedFrom = r.status;
    r.status = r.hits.includes(predicted) ? 'by-spacing' : 'predicted';
    r.hits = [predicted];
    r.anchor = anchors[0].name;
  }
}

console.log('# server_addr\tname\tnwmain_addr\tstatus\tsignature_bytes\tnote');
const counts = {};
for (const r of results) {
  const status = r.status === 'unique' && r.confirmed ? 'confirmed' : r.status;
  counts[status.replace(/\(.*/, '')] = (counts[status.replace(/\(.*/, '')] ?? 0) + 1;
  const note = r.anchor ? `from ${r.resolvedFrom} via ${r.anchor}` : '';
  console.log([hex(r.va), r.name, r.hits.map(hex).join(',') || '-', status, r.length, note].join('\t'));
}
console.error(`${JSON.stringify(counts)} of ${seeds.length}`);
