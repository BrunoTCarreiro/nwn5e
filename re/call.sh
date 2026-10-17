#!/usr/bin/env bash
# Usage: re/call.sh <tool> '<json arguments>'   (forward slashes in paths are fine)
# Prints the MCP result text; the full reply is also kept under re/out/.
# Uses node:http (no client timeout): first Ghidra queries can take many minutes.
tool="$1"; args="${2:-{\}}"
node -e '
const http = require("node:http");
const [tool, args] = process.argv.slice(1);
const body = JSON.stringify({ name: tool, arguments: JSON.parse(args) });
const req = http.request({ host: "127.0.0.1", port: Number(process.env.NWN5E_BRIDGE_PORT || 47815), path: "/call", method: "POST" }, (res) => {
  let text = "";
  res.setEncoding("utf8");
  res.on("data", (d) => (text += d));
  res.on("end", () => {
    let j;
    try { j = JSON.parse(text); } catch { console.log(text); process.exit(4); }
    if (j.error) { console.log(JSON.stringify(j.error)); process.exit(2); }
    const r = j.result || {};
    for (const c of r.content || []) if (c.type === "text") console.log(c.text);
    if (!r.content && r.structuredContent) console.log(JSON.stringify(r.structuredContent));
    if (r.isError) process.exit(1);
  });
});
req.on("error", (e) => { console.error(String(e)); process.exit(3); });
req.end(body);
' "$tool" "$args"
