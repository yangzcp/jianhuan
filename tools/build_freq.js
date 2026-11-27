const fs = require("fs");
const path = require("path");
const root = path.resolve(__dirname, "..");

const sources = [
  "pinyin_simp.dict.yaml",
  "jianhuan_colloquial.dict.yaml",
  "jianhuan_poetry.dict.yaml",
  "jianhuan_phrases.dict.yaml",
  "jianhuan_english.dict.yaml",
];
const freq = new Map();
for (const file of sources) {
  const full = path.join(root, "data", file);
  if (!fs.existsSync(full)) continue;
  const body = fs.readFileSync(full, "utf8").split("...")[1] || "";
  for (const line of body.split(/\r?\n/)) {
    const p = line.split("\t");
    if (p.length >= 3 && p[0]) {
      const w = Number(p[2]);
      if (!Number.isFinite(w) || w < 100) continue;
      freq.set(p[0], Math.max(freq.get(p[0]) || 0, w));
    }
  }
}
const lines = [...freq.entries()].map(([t, w]) => `${t}\t${w}`);
fs.writeFileSync(path.join(root, "data", "jianhuan_freq.txt"), lines.join("\n") + "\n");
console.log("freq entries", lines.length);