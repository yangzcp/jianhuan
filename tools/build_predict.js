const fs = require("fs");
const path = require("path");
const root = path.resolve(__dirname, "..");

function readEntries(file, minWeight, multiOnly) {
  const text = fs.readFileSync(path.join(root, "data", file), "utf8");
  const body = text.split("...")[1] || "";
  const out = [];
  for (const line of body.split(/\r?\n/)) {
    const parts = line.split("\t");
    if (parts.length >= 3 && parts[0] && /^[a-z]+( [a-z]+)*$/.test(parts[1])) {
      const weight = Number(parts[2]);
      const syllables = parts[1].split(" ");
      if (multiOnly && syllables.length < 2) continue;
      if (weight < minWeight) continue;
      out.push([parts[0], syllables]);
    }
  }
  return out;
}

// Mixed-code variants: first k syllables spelled fully, the rest abbreviated to
// their initial letter (k = 0..S). Covers full pinyin, pure abbrev and mixed
// input such as "zenms" for 怎么说 (zen me shuo).
function variants(syllables) {
  const out = [];
  for (let k = 0; k <= syllables.length; k++) {
    const full = syllables.slice(0, k).join("");
    const abbr = syllables.slice(k).map(s => s[0]).join("");
    out.push(full + abbr);
  }
  return out;
}

const entries = [
  ...readEntries("jianhuan_colloquial.dict.yaml", 0, false),
  ...readEntries("jianhuan_poetry.dict.yaml", 0, false),
  ...readEntries("pinyin_simp.dict.yaml", 1000, true),
  ...readEntries("jianhuan_english.dict.yaml", 0, false),
];

const seen = new Set();
const lines = [];
for (const [text, syllables] of entries) {
  for (const code of variants(syllables)) {
    if (code.length < 5) continue;
    const key = text + "\t" + code;
    if (seen.has(key)) continue;
    seen.add(key);
    lines.push(key);
  }
}
fs.writeFileSync(path.join(root, "data", "jianhuan_predict.txt"), lines.join("\n") + "\n");
console.log("predict entries", entries.length, "variant lines", lines.length);