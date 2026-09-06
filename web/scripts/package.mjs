import { readdir, readFile, writeFile, mkdir, unlink } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { resolve } from "node:path";
import { build } from "esbuild";
import { minify } from "html-minifier-terser";

const webRoot = fileURLToPath(new URL("../", import.meta.url));
const dist = resolve(webRoot, "dist");
const destination = resolve(webRoot, "../html");
const htmlCapacity = 0x6f000 - 0x40000;
const reservedHeadroom = 8192;
const perFileLimit = 60 * 1024;

const aliases = {
  "ports.html": "ports",
  "stat.html": "statistics",
  "vlan.html": "vlans",
  "l2.html": "l2",
  "stp.html": "stp",
  "mirror.html": "mirror",
  "lag.html": "lag",
  "eee.html": "eee",
  "bandwidth.html": "bandwidth",
  "system.html": "system",
  "update.html": "firmware",
};

const files = new Map();
for (const entry of await readdir(dist, { withFileTypes: true })) {
  if (!entry.isFile() || !/^[A-Za-z0-9_]+\.(js|css|html)$/.test(entry.name)) {
    throw new Error(
      `Unsupported firmware asset: ${entry.name}. Assets must be flat and C-identifier compatible.`,
    );
  }
  files.set(entry.name, await readFile(resolve(dist, entry.name)));
}

// A parser-discovered stylesheet and module would compete for the device's only
// TCP connection. Start the module only after the stylesheet has finished.
const index = files.get("index.html").toString();
files.set(
  "index.html",
  Buffer.from(
    index
      .replace(/<script\b[^>]*src="\/app\.js"[^>]*><\/script>/, "")
      .replace(/<link\b[^>]*rel="stylesheet"[^>]*>/, "")
      .replace("</head>", '<link rel="icon" href="data:,"></head>')
      .replace(
        "</body>",
        `<script>
const style=document.createElement('link');
style.rel='stylesheet';style.href='/style.css';
const failed=()=>document.body.append('Could not load the panel. Refresh to try again.');
style.onload=()=>import('/app.js').catch(failed);
style.onerror=failed;
document.head.append(style);
</script></body>`,
      ),
  ),
);

const loginScript = await build({
  entryPoints: [resolve(webRoot, "src/login.ts")],
  bundle: true,
  write: false,
  format: "iife",
  target: "es2020",
  minify: true,
  legalComments: "none",
});
const login = await readFile(resolve(webRoot, "login.html"), "utf8");
files.set(
  "login.html",
  Buffer.from(
    login.replace("<!-- login-script -->", `<script>${loginScript.outputFiles[0].text}</script>`),
  ),
);

// These public asset symbols are part of the existing firmware's authentication code.
for (const name of ["port.svg", "sfp.svg"]) {
  files.set(name, await readFile(resolve(webRoot, "static", name)));
}
files.set("i18n.js", Buffer.from("/* Reserved public asset for firmware compatibility. */"));

for (const [name, route] of Object.entries(aliases)) {
  files.set(
    name,
    Buffer.from(
      `<!doctype html><html lang="en"><meta charset="utf-8"><title>RTLPlayground</title><script>location.replace('/index.html#${route}')</script><a href="/index.html#${route}">Open ${route}</a></html>`,
    ),
  );
}

let used = 0;
for (const [name, contents] of files) {
  let bytes = contents;
  if (name.endsWith(".html")) {
    bytes = Buffer.from(
      await minify(contents.toString(), { collapseWhitespace: true, removeComments: true }),
    );
    files.set(name, bytes);
  }
  if (bytes.length > perFileLimit)
    throw new Error(`${name}: ${bytes.length} bytes exceeds the ${perFileLimit}-byte file budget.`);
  if (bytes.includes(Buffer.from("#{")))
    throw new Error(`${name} contains the firmware's reserved template marker.`);
  used += bytes.length + 1;
}

if (used > htmlCapacity - reservedHeadroom) {
  throw new Error(
    `Frontend uses ${used} bytes; budget is ${htmlCapacity - reservedHeadroom}. No files were installed.`,
  );
}

// Validate the complete artifact before replacing the generated output directory.
await mkdir(destination, { recursive: true });
for (const [name, contents] of [...files].sort(([first], [second]) =>
  first.localeCompare(second),
)) {
  await writeFile(resolve(destination, name), contents);
}
for (const entry of await readdir(destination, { withFileTypes: true })) {
  if (entry.isFile() && /\.(js|css|html|svg|ico)$/.test(entry.name) && !files.has(entry.name)) {
    await unlink(resolve(destination, entry.name));
  }
}

console.log(
  `Firmware frontend: ${files.size} files, ${used.toLocaleString()} / ${htmlCapacity.toLocaleString()} bytes.`,
);
console.log(
  `Headroom before configuration: ${(htmlCapacity - used).toLocaleString()} bytes. Largest file: ${Math.max(...[...files.values()].map((file) => file.length)).toLocaleString()} bytes.`,
);
