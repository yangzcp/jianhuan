const fs = require("fs");
const path = require("path");
const { spawnSync } = require("child_process");
const root = path.resolve(__dirname, "..");

function readEntries(file, minWeight, multiOnly) {
  const text = fs.readFileSync(path.join(root, "data", file), "utf8");
  const body = text.split("...")[1] || "";
  const out = [];
  for (const line of body.split(/\r?\n/)) {
    const p = line.split("\t");
    if (p.length >= 3 && p[0] && /^[a-z]+( [a-z]+)*$/.test(p[1])) {
      const syl = p[1].split(" ");
      if (multiOnly && syl.length < 2) continue;
      if (Number(p[2]) < minWeight) continue;
      out.push([p[0], syl]);
    }
  }
  return out;
}
const entries = [
  ...readEntries("jianhuan_colloquial.dict.yaml", 0, false),
  ...readEntries("jianhuan_poetry.dict.yaml", 0, false),
  ...readEntries("pinyin_simp.dict.yaml", 1000, true),
];
const fullCodes = entries.map(([t, s]) => [t, s.join("")]);
const fullMap = new Map();
const variantMap = new Map();
const variantPrefixMap = new Map();
function variants(syl) {
  const out = [];
  for (let k = 0; k <= syl.length; k++) {
    out.push(syl.slice(0, k).join("") + syl.slice(k).map(s => s[0]).join(""));
  }
  return out;
}
for (const [t, s] of entries) {
  const full = s.join("");
  if (!fullMap.has(full)) fullMap.set(full, []);
  fullMap.get(full).push(t);
  for (const v of variants(s)) {
    if (!variantMap.has(v)) variantMap.set(v, []);
    if (!variantMap.get(v).includes(t)) variantMap.get(v).push(t);
    for (let len = 5; len <= v.length; len++) {
      const p = v.slice(0, len);
      if (!variantPrefixMap.has(p)) variantPrefixMap.set(p, []);
      if (!variantPrefixMap.get(p).includes(t)) variantPrefixMap.get(p).push(t);
    }
  }
}
function acceptableForPrefix(input) {
  const match = fullCodes.filter(([t, c]) => c.startsWith(input));
  if (!match.length) return null;
  const min = Math.min(...match.map(([t, c]) => c.length));
  return match.filter(([t, c]) => c.length === min).map(([t]) => t);
}
function mixed(syl) {
  return syl.slice(0, 1).join("") + syl.slice(1).map(s => s[0]).join("");
}
const singleSyllables = new Set();
for (const line of fs.readFileSync(path.join(root, "data", "pinyin_simp.dict.yaml"), "utf8").split("...")[1].split(/\r?\n/)) {
  const p = line.split("\t");
  if (p.length >= 2 && /^[a-z]+$/.test(p[1])) singleSyllables.add(p[1]);
}
const englishSet = new Set();
for (const line of fs.readFileSync(path.join(root, "data", "jianhuan_english.dict.yaml"), "utf8").split("...")[1].split(/\r?\n/)) {
  const p = line.split("\t");
  if (p.length >= 2 && p[0]) englishSet.add(p[0]);
}
function completions(input) {
  return fullCodes.filter(([t, c]) => c.startsWith(input)).map(([t]) => t);
}
function union(...lists) {
  const out = [];
  for (const l of lists) for (const t of l || []) if (!out.includes(t)) out.push(t);
  return out.length ? out : null;
}

const singles = ["a","ai","an","ang","ao","ba","bi","bu","da","de","di","du","e","en","er","fa","fu","ga","ge","gu","ha","he","hu","ji","ju","ka","ke","ku","la","le","li","lu","ma","me","mi","mu","na","ne","ni","nu","nv","ou","pa","pi","pu","qi","qu","re","ri","ru","sa","se","si","su","ta","te","ti","tu","wa","wo","wu","xi","xu","ya","ye","yi","yu","za","ze","zi","zu","zhi","chi","shi","zhong","shuang"];
const english = ["hello","world","computer","password","english","windows","internet","email","phone","music","video","game","time","love","happy","friend","family","school","work","money"];
const common = ["women","nihao","pengyou","tongxue","laoshi","xuesheng","gongzuo","xuexi","shenghuo","shijian","difang","dongxi","shiqing","wenti","banfa","jihui","guojia","zhongguo","beijing","jintian","mingtian","zuotian","zaoshang","wanshang","shuijiao","chifan","shangban","xiaban","huijia","shouji","diannao","wangluo","mima","zhanghao","shide","bushi","haode","wode","nide","dajia","keyi","yinggai","zhidao","juede","xihuan","xiwang","xiexie","zaijian","yinwei","suoyi","danshi","keshi","ruguo","suiran","erqie","huozhe","yiji","yijing","zenme","weishenme"];

const cases = [];
const seenInput = new Set();
function push(input, accept, kind) {
  if (seenInput.has(input)) return;
  seenInput.add(input);
  cases.push({ input, accept, kind });
}
for (const [text, syl] of entries) {
  const full = syl.join("");
  const strictFull = full.length >= 5 && !singleSyllables.has(full) && !englishSet.has(full);
  push(full, strictFull ? fullMap.get(full) : null, "full");
  const mx = mixed(syl);
  if (mx.length >= 5) push(mx, union(variantPrefixMap.get(mx), completions(mx)), "mixed");
  if (full.length - 1 >= 5) push(full.slice(0, -1), union(acceptableForPrefix(full.slice(0, -1)), completions(full.slice(0, -1)), fullMap.get(full.slice(0, -1))), "partial");
}
for (const w of english) push(w, [w], "english");
for (const w of common) push(w, acceptableForPrefix(w) || null, "common");
for (const w of singles) push(w, null, "single");

let stdin = "";
for (const c of cases) stdin += `/type ${c.input}\n/clear\n`;
stdin += "/quit\n";

const exe = path.join(root, "build", "jianhuan_console.exe");
const res = spawnSync(exe, [path.join(root, "data"), path.join(root, "build", "sweep_user"), "jianhuan_pinyin"], { input: stdin, encoding: "utf8" });
const lines = (res.stdout || "").split(/\r?\n/).filter(l => l.trim().startsWith("["));
if (lines.length !== cases.length) {
  console.log("ALIGN MISMATCH contexts=" + lines.length + " cases=" + cases.length);
  process.exit(1);
}
let bad = 0;
const byKind = {};
for (let i = 0; i < cases.length; i++) {
  const c = cases[i];
  const m = lines[i].match(/\]\s*(.*)$/);
  const body = m ? m[1] : "";
  const cands = [...body.matchAll(/\d+\.(.+?)(?=\s+\d+\.|$)/g)].map(x => x[1].trim());
  byKind[c.kind] = byKind[c.kind] || { total: 0, bad: 0 };
  byKind[c.kind].total++;
  if (c.accept && !c.accept.includes(cands[0])) {
    byKind[c.kind].bad++;
    bad++;
    if (bad <= 40) console.log(`BAD[${c.kind}] ${c.input} want=${c.accept.join("/")} got=${cands.slice(0, 3).join(",")}`);
  }
  if (!c.accept && cands.length === 0) {
    byKind[c.kind].bad++;
    bad++;
    if (bad <= 40) console.log(`EMPTY[${c.kind}] ${c.input}`);
  }
}
console.log("summary", JSON.stringify(byKind), "total_bad=" + bad, "cases=" + cases.length);
process.exit(bad ? 1 : 0);