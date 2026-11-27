const fs = require("fs");
const path = require("path");
const root = path.resolve(__dirname, "..");

const sources = [
  ["pinyin_simp.dict.yaml", 100],
  ["jianhuan_colloquial.dict.yaml", 0],
  ["jianhuan_poetry.dict.yaml", 0],
  ["jianhuan_english.dict.yaml", 0],
];
const best = new Map(); // code -> [text, weight]
const bestText = new Map(); // text -> [code, weight]
for (const [file, minW] of sources) {
  const full = path.join(root, "data", file);
  if (!fs.existsSync(full)) continue;
  const body = fs.readFileSync(full, "utf8").split("...")[1] || "";
  for (const line of body.split(/\r?\n/)) {
    const p = line.split("\t");
    if (p.length >= 3 && p[0] && /^[a-z]+( [a-z]+)*$/.test(p[1])) {
      const w = Number(p[2]);
      if (!Number.isFinite(w) || w < minW) continue;
      const code = p[1].replace(/ /g, "");
      const cur = best.get(code);
      if (!cur || w > cur[1]) best.set(code, [p[0], w]);
      const curT = bestText.get(p[0]);
      if (!curT || w > curT[1]) bestText.set(p[0], [code, w]);
    }
  }
}
const lines = [...best.entries()].map(([c, [t]]) => `${c}\t${t}`);
fs.writeFileSync(path.join(root, "data", "jianhuan_revcode.txt"), lines.join("\n") + "\n");
const tlines = [...bestText.entries()].map(([t, [c]]) => `${t}\t${c}`);
fs.writeFileSync(path.join(root, "data", "jianhuan_code.txt"), tlines.join("\n") + "\n");
console.log("revcode entries", lines.length, "code entries", tlines.length);