const fs = require("fs");
const path = require("path");

const root = path.resolve(__dirname, "..");
const dist = path.join(root, "dist", "jianhuan");
const build = path.join(root, "build");
fs.mkdirSync(build, { recursive: true });

const files = [];
function walk(dir, rel) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    const relative = rel ? `${rel}/${entry.name}` : entry.name;
    if (entry.isDirectory()) walk(full, relative);
    else files.push({ full, relative });
  }
}
walk(dist, "");

const manifest = [];
const rc = [];
rc.push(`1 ICON "${path.join(root, "src", "platform", "windows", "jianhuan.ico").replace(/\\/g, "\\\\")}"`);
manifest.push("1\tsetup_manifest.txt");
rc.push(`1 RCDATA "${path.join(build, "setup_manifest.txt").replace(/\\/g, "\\\\")}"`);
files.forEach((file, index) => {
  const id = 1000 + index;
  manifest.push(`${id}\t${file.relative}`);
  rc.push(`${id} RCDATA "${file.full.replace(/\\/g, "\\\\")}"`);
});

fs.writeFileSync(path.join(build, "setup_manifest.txt"), manifest.join("\n") + "\n");
fs.writeFileSync(path.join(build, "setup_payload.rc"), rc.join("\n") + "\n");
console.log(`setup resources: ${files.length} files`);