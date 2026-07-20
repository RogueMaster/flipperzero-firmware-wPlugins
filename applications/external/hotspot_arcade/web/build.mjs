// Build the single self-contained index.html for the ESP to serve, then gzip
// it and emit the Flipper manifest. Zero runtime deps: Node stdlib only.
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { gzipSync, constants } from "node:zlib";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const ROOT = dirname(fileURLToPath(import.meta.url));
const DIST = join(ROOT, "dist");
const CEIL = 60 * 1024;   // hard ceiling: fail the build above this (gzipped)
const TARGET = 30 * 1024; // soft target: warn above this

const read = (p) => readFileSync(join(ROOT, p), "utf8");

// Conservative minifier. Line based so string literals (which are single line
// here, including "//" and "http://") are never touched. Removes /* */ blocks
// and whole-line // comments, strips leading indentation, drops blank lines.
function minify(src, css) {
  let s = src.replace(/\/\*[\s\S]*?\*\//g, "");   // block comments (none live inside strings here)
  const lines = s.split("\n").map((l) => l.trim());
  const kept = lines.filter((l) => l && !(!css && l.startsWith("//")));
  return kept.join(css ? "" : "\n");
}

const css = minify(read("core/style.css"), true);
const js = [
  read("core/app.js"),
  read("core/sound.js"),
  read("games/trivia.js"),
  read("games/duel.js"),
  read("games/draw.js"),
  read("games/pong.js"),
  read("games/wyr.js"),
  read("games/scramble.js"),
  read("games/react.js"),
].map((f) => minify(f, false)).join("\n");

let html = read("src/index.html")
  .replace("/*__CSS__*/", () => css)
  .replace("/*__JS__*/", () => js);

// Collapse HTML inter-tag whitespace and drop HTML comments (keep it simple).
html = html
  .replace(/<!--[\s\S]*?-->/g, "")
  .split("\n").map((l) => l.trim()).filter(Boolean).join("\n");

mkdirSync(DIST, { recursive: true });
writeFileSync(join(DIST, "index.html"), html);

const gz = gzipSync(Buffer.from(html), { level: constants.Z_BEST_COMPRESSION });
writeFileSync(join(DIST, "index.html.gz"), gz);

const manifest = [{ path: "/", file: "index.html.gz", mime: "text/html", gzip: true }];
const manifestJson = JSON.stringify(manifest, null, 2) + "\n";
writeFileSync(join(DIST, "manifest.json"), manifestJson);

const kb = (n) => (n / 1024).toFixed(2) + " KB";
const raw = Buffer.byteLength(html);
console.log("Hotspot Arcade web build");
console.log("  dist/index.html      " + kb(raw) + " raw");
console.log("  dist/index.html.gz   " + kb(gz.length) + " gzipped");
console.log("  dist/manifest.json   " + kb(Buffer.byteLength(manifestJson)));

if (gz.length > CEIL) {
  console.error("\nFAIL: gzipped bundle " + kb(gz.length) + " exceeds ceiling " + kb(CEIL));
  process.exit(1);
}
if (gz.length > TARGET) {
  console.warn("\nWARN: over the " + kb(TARGET) + " target (still under the " + kb(CEIL) + " ceiling)");
}
console.log("\nOK (gzipped " + kb(gz.length) + ", ceiling " + kb(CEIL) + ")");
