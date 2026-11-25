const fs = require("fs");
const path = require("path");

const root = path.resolve(__dirname, "..");
const input = path.join(root, ".deps", "phrase-pinyin-data-master", "large_pinyin.txt");
const charDict = path.join(root, "data", "pinyin_simp.dict.yaml");
const output = path.join(root, "data", "jianhuan_phrases.dict.yaml");
const limit = Number(process.env.JIANHUAN_PHRASE_LIMIT || 120000);

function stripTones(pinyin) {
  return pinyin
    .replace(/[üǖǘǚǜ]/g, "v")
    .normalize("NFD")
    .replace(/[\u0300-\u036f]/g, "")
    .toLowerCase();
}

const charWeights = new Map();
const charText = fs.readFileSync(charDict, "utf8");
const charBody = charText.split("...")[1] || "";
for (const line of charBody.split(/\r?\n/)) {
  const parts = line.split("\t");
  if (parts.length < 3) continue;
  const weight = Number(parts[2]);
  if (!Number.isFinite(weight)) continue;
  const previous = charWeights.get(parts[0]) || 0;
  if (weight > previous) charWeights.set(parts[0], weight);
}

const text = fs.readFileSync(input, "utf8");
const candidates = [];
for (const line of text.split(/\r?\n/)) {
  if (!line || line.startsWith("#")) continue;
  const separator = line.indexOf(":");
  if (separator < 0) continue;
  const phrase = line.slice(0, separator).trim();
  const pinyin = line.slice(separator + 1).trim();
  if (!phrase || !pinyin) continue;
  const chars = Array.from(phrase);
  if (chars.length < 2 || chars.length > 8) continue;
  const syllables = pinyin.split(/\s+/).map(stripTones).filter(Boolean);
  if (syllables.length === 0) continue;
  if (!syllables.every(s => /^[a-z]+$/.test(s))) continue;
  let score = Infinity;
  for (const ch of chars) {
    const weight = charWeights.get(ch) || 0;
    if (weight < score) score = weight;
  }
  if (!Number.isFinite(score) || score <= 0) continue;
  candidates.push({ phrase, syllables, score });
}

candidates.sort((a, b) => b.score - a.score || a.phrase.length - b.phrase.length);
const selected = candidates.slice(0, limit);
const entries = selected.map(item => {
  const weight = Math.max(1, Math.round(item.score / 1000));
  return `${item.phrase}\t${item.syllables.join(" ")}\t${weight}`;
});

const header = `# Rime dictionary
# encoding: utf-8
#
# Phrase data derived from mozillazg/phrase-pinyin-data (MIT).
# Tones removed; ü represented as v.
# Filtered by single-character frequency from rime-pinyin-simp (Apache-2.0).
#
---
name: jianhuan_phrases
version: "0.2"
sort: by_weight
...
`;

fs.writeFileSync(output, header + entries.join("\n") + "\n");
console.log(`generated ${output} entries=${entries.length} limit=${limit}`);