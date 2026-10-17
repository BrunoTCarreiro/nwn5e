// Prints the first N bytes at each nwmain.exe address, as C initialisers for the DLL's prologue checks.
//   node re/prologue.mjs <nwmain.exe> 0x69C290 0x69C400 [--n 8]
import { readFileSync } from 'node:fs';
const args = process.argv.slice(2);
const n = args.includes('--n') ? Number(args[args.indexOf('--n') + 1]) : 8;
const file = args[0];
const addrs = args.slice(1).filter((a, i, all) => a !== '--n' && all[i - 1] !== '--n');
const b = readFileSync(file);
const pe = b.readUInt32LE(0x3c), opt = pe + 24, base = b.readUInt32LE(opt + 28);
let h = opt + b.readUInt16LE(pe + 20); const secs = [];
for (let i = 0; i < b.readUInt16LE(pe + 6); i++, h += 40) secs.push({ va: b.readUInt32LE(h + 12), vs: b.readUInt32LE(h + 8), raw: b.readUInt32LE(h + 20) });
for (const a of addrs) {
  const rva = parseInt(a, 16) - base; const s = secs.find((x) => rva >= x.va && rva < x.va + x.vs);
  const off = rva - s.va + s.raw; const bytes = [...b.subarray(off, off + n)];
  console.log(`${a}: {${bytes.map((x) => '0x' + x.toString(16).padStart(2, '0')).join(', ')}}`);
}
