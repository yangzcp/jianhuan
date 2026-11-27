const fs = require("fs");
const path = require("path");
const root = path.resolve(__dirname, "..");

function readTexts(file, minWeight) {
  const full = path.join(root, "data", file);
  if (!fs.existsSync(full)) return [];
  const body = fs.readFileSync(full, "utf8").split("...")[1] || "";
  const out = [];
  for (const line of body.split(/\r?\n/)) {
    const p = line.split("\t");
    if (p.length >= 2 && p[0]) {
      if (minWeight && Number(p[2]) < minWeight) continue;
      out.push(p[0]);
    }
  }
  return out;
}

// Permissive corpus: own colloquial/poetry annotations, Apache-2.0 pinyin_simp
// word list, and self-generated combinatorial sentences.
const sentences = [
  ...readTexts("jianhuan_colloquial.dict.yaml", 0),
  ...readTexts("jianhuan_poetry.dict.yaml", 0),
  ...readTexts("pinyin_simp.dict.yaml", 1000),
];
const subjects = ["我", "你", "他", "她", "我们", "你们", "他们", "大家", "今天", "明天", "现在"];
const verbs = ["感觉", "觉得", "认为", "喜欢", "希望", "知道", "想要", "打算", "准备", "开始", "继续", "需要", "应该", "可以", "想吃", "想看", "想去", "担心", "害怕", "开心", "难过", "觉得", "发现", "记得", "忘记"];
const objects = ["好", "冷", "热", "累", "困", "饿", "渴", "疼", "你", "他", "它", "吃饭", "睡觉", "工作", "学习", "生活", "看书", "写字", "唱歌", "跳舞", "旅游", "购物", "很好", "不好", "一点", "很多", "什么", "怎么", "为什么"];
for (const s of subjects) {
  for (const v of verbs) {
    sentences.push(s + v);
    for (const o of objects) sentences.push(s + v + o);
  }
}

const unigram = new Map();
const bigram = new Map();
const BOS = "\x02";
for (const sentence of sentences) {
  const chars = [BOS, ...Array.from(sentence)];
  for (let i = 0; i < chars.length; i++) {
    const c = chars[i];
    unigram.set(c, (unigram.get(c) || 0) + 1);
    if (i > 0) {
      const key = chars[i - 1] + c;
      bigram.set(key, (bigram.get(key) || 0) + 1);
    }
  }
}
const lines = [];
for (const [c, n] of unigram) lines.push(`U\t${c}\t${n}`);
for (const [k, n] of bigram) lines.push(`B\t${k}\t${n}`);
fs.writeFileSync(path.join(root, "data", "jianhuan_lm.txt"), lines.join("\n") + "\n");
console.log("lm unigrams", unigram.size, "bigrams", bigram.size, "sentences", sentences.length);