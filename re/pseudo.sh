#!/usr/bin/env bash
# Usage: re/pseudo.sh <address-or-name>  — prints only the decompiled C from REA.
re/call.sh procedure_pseudo_code "{\"procedure\":\"$1\"}" | node -e '
let s = ""; process.stdin.on("data", (d) => (s += d)).on("end", () => {
  let j; try { j = JSON.parse(s); } catch { console.log(s); return; }
  const v = j.raw_result?.value ?? j.normalized_result ?? j;
  const text = typeof v === "string" ? v : (v.pseudo_code ?? v.pseudocode ?? v.text ?? JSON.stringify(v, null, 1));
  console.log(text);
});'
