const fs = require("fs");
const path = require("path");
const root = path.resolve(__dirname, "..");

function readTexts(file) {
  const text = fs.readFileSync(path.join(root, "data", file), "utf8");
  const body = text.split("...")[1] || "";
  const out = [];
  for (const line of body.split(/\r?\n/)) {
    const parts = line.split("\t");
    if (parts.length >= 2 && parts[0]) out.push(parts[0]);
  }
  return out;
}

const sentences = [
  ...readTexts("jianhuan_colloquial.dict.yaml"),
  ...readTexts("jianhuan_poetry.dict.yaml"),
];
const subjects = ["我", "你", "他", "她", "我们", "你们", "他们", "大家"];
const verbs = ["感觉", "觉得", "认为", "喜欢", "希望", "知道", "想要", "打算", "准备", "开始", "继续", "停止", "需要", "应该", "可以", "想吃", "想看", "想去", "喜欢", "担心", "害怕", "开心", "难过"];
const objects = ["好", "冷", "热", "累", "困", "饿", "渴", "疼", "你", "他", "它", "吃饭", "睡觉", "工作", "学习", "生活", "看书", "写字", "唱歌", "跳舞", "旅游", "购物", "很好", "不好", "一点", "很多"];
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