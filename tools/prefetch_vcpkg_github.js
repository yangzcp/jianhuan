const fs = require("fs");
const path = require("path");
const crypto = require("crypto");

const root = path.resolve(__dirname, "..");
const portsDir = path.join(root, ".tools", "vcpkg", "ports");
const downloadsDir = path.join(root, ".tools", "vcpkg", "downloads");

const seeds = process.argv.slice(2);
const visited = new Set();

function readJson(port) {
  const file = path.join(portsDir, port, "vcpkg.json");
  if (!fs.existsSync(file)) return null;
  return JSON.parse(fs.readFileSync(file, "utf8"));
}

function collect(port) {
  if (visited.has(port)) return;
  visited.add(port);
  const json = readJson(port);
  if (!json) return;
  for (const dep of json.dependencies || []) {
    const name = typeof dep === "string" ? dep : dep.name;
    if (name && name.startsWith("boost-")) collect(name);
  }
}

async function prefetch(port) {
  const json = readJson(port);
  const portfile = path.join(portsDir, port, "portfile.cmake");
  if (!json || !fs.existsSync(portfile)) return;
  const text = fs.readFileSync(portfile, "utf8");
  const repoMatch = text.match(/REPO\s+([^\s)]+)/);
  const refMatch = text.match(/REF\s+([^\s)]+)/);
  const shaMatch = text.match(/SHA512\s+([a-f0-9]+)/);
  if (!repoMatch || !refMatch || !shaMatch) return;
  const repo = repoMatch[1];
  const ref = refMatch[1].replace(/\$\{VERSION\}/g, json.version);
  const expected = shaMatch[1];
  const [owner, name] = repo.split("/");
  const filename = `${owner}-${name}-${ref}.tar.gz`;
  const target = path.join(downloadsDir, filename);
  if (fs.existsSync(target)) {
    const existing = crypto.createHash("sha512").update(fs.readFileSync(target)).digest("hex");
    if (existing === expected) return;
  }
  const urls = [
    `https://codeload.github.com/${repo}/tar.gz/refs/tags/${ref}`,
    `https://codeload.github.com/${repo}/tar.gz/${ref}`,
  ];
  for (const url of urls) {
    const response = await fetch(url);
    if (!response.ok) continue;
    const buffer = Buffer.from(await response.arrayBuffer());
    const sha512 = crypto.createHash("sha512").update(buffer).digest("hex");
    if (sha512 !== expected) continue;
    fs.writeFileSync(target, buffer);
    console.log(`cached ${filename}`);
    return;
  }
  console.log(`FAILED ${port} ${repo} ${ref}`);
}

(async () => {
  for (const seed of seeds) collect(seed);
  for (const port of visited) {
    if (port === "boost-cmake" || port === "boost-headers" || port === "boost-uninstall") continue;
    await prefetch(port);
  }
})().catch(error => { console.error(error); process.exit(1); });