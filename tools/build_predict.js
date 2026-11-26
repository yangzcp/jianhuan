const fs = require("fs");
const path = require("path");
const root = path.resolve(__dirname, "..");

function readEntries(file) {
  const text = fs.readFileSync(path.join(root, "data", file), "utf8");
  const body = text.split("...")[1] || "";
  const out = [];
  for (const line of body.split(/\r?\n/)) {
    const parts = line.split("\t");
    if (parts.length >= 2 && parts[0] && /^[a-z ]+$/.test(parts[1])) {
      out.push([parts[0], parts[1].replace(/ /g, "")]);
    }
  }
  return out;
}

const entries = [
  ...readEntries("jianhuan_colloquial.dict.yaml"),
  ...readEntries("jianhuan_poetry.dict.yaml"),
];
const seen = new Set();
const lines = [];
for (const [text, code] of entries) {
  if (code.length < 5) continue;
  const key = text + "\t" + code;
  if (seen.has(key)) continue;
  seen.add(key);
  lines.push(key);
}
fs.writeFileSync(path.join(root, "data", "jianhuan_predict.txt"), lines.join("\n") + "\n");
console.log("predict entries", lines.length);