const fs = require("fs");
const path = require("path");
const root = path.resolve(__dirname, "..");
const sizes = [16, 32, 48, 256];
const images = sizes.map(s => fs.readFileSync(path.join(root, "build", `icon_${s}.png`)));
const header = Buffer.alloc(6);
header.writeUInt16LE(0, 0);
header.writeUInt16LE(1, 2);
header.writeUInt16LE(sizes.length, 4);
const entries = [];
let offset = 6 + 16 * sizes.length;
sizes.forEach((s, i) => {
  const e = Buffer.alloc(16);
  e.writeUInt8(s >= 256 ? 0 : s, 0);
  e.writeUInt8(s >= 256 ? 0 : s, 1);
  e.writeUInt8(0, 2);
  e.writeUInt8(0, 3);
  e.writeUInt16LE(1, 4);
  e.writeUInt16LE(32, 6);
  e.writeUInt32LE(images[i].length, 8);
  e.writeUInt32LE(offset, 12);
  offset += images[i].length;
  entries.push(e);
});
fs.writeFileSync(path.join(root, "src", "platform", "windows", "jianhuan.ico"),
                 Buffer.concat([header, ...entries, ...images]));
console.log("multi-size ico written");