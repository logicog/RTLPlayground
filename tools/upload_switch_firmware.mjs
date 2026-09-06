/** Explicit firmware upload; never writes configuration and never retries. */
import assert from "node:assert/strict";
import { createHash, randomBytes } from "node:crypto";
import { execFileSync } from "node:child_process";
import { readFile, mkdir, writeFile } from "node:fs/promises";
import { request } from "node:http";
import { basename, resolve } from "node:path";
import { createInterface } from "node:readline/promises";

const [imagePath, baselinePath, expectedHash, authorization] =
  process.argv.slice(2);
assert(
  authorization === "--upload",
  "Requires IMAGE BASELINE SHA256 --upload and explicit user authorization.",
);
assert(
  basename(imagePath).endsWith("-SWTGW218AS.bin"),
  "Incorrect target filename.",
);
const image = await readFile(imagePath);
assert(
  image.length === 524288 && image[0] === 0 && image[1] === 0x40,
  "Invalid image format.",
);
const hash = (bytes) => createHash("sha256").update(bytes).digest("hex");
assert(hash(image) === expectedHash, "Image differs from the reviewed build.");
let crc = 0;
for (const byte of image) {
  crc ^= byte;
  for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ (crc & 1 ? 0xa001 : 0);
}
assert(crc === 0xb001, "Invalid image CRC.");

let cookie;
function exchange(method, path, body, contentType, timeout = 10000) {
  return new Promise((resolveResponse, reject) => {
    const headers = { Connection: "close" };
    if (cookie) headers.Cookie = cookie;
    if (body) {
      headers["Content-Type"] = contentType;
      headers["Content-Length"] = Buffer.byteLength(body);
    }
    const connection = request(
      { host: "192.168.5.9", port: 80, method, path, headers, agent: false },
      (response) => {
        const chunks = [];
        let length = 0;
        response.on("data", (chunk) => {
          length += chunk.length;
          if (length > 65536)
            connection.destroy(new Error("Response exceeds limit."));
          else chunks.push(chunk);
        });
        response.on("error", reject);
        response.on("end", () =>
          resolveResponse({
            status: response.statusCode,
            headers: response.headers,
            body: Buffer.concat(chunks),
          }),
        );
      },
    );
    connection.setTimeout(timeout, () =>
      connection.destroy(new Error("Request timed out. No retry attempted.")),
    );
    connection.on("error", reject);
    connection.end(body);
  });
}

let password = process.env.SWITCH_PASSWORD;
if (!password) {
  assert(process.stdin.isTTY, "Use a terminal or SWITCH_PASSWORD.");
  const input = createInterface({
    input: process.stdin,
    output: process.stdout,
    terminal: false,
  });
  execFileSync("stty", ["-echo"], { stdio: "inherit" });
  try {
    password = await input.question("Switch password: ");
  } finally {
    execFileSync("stty", ["echo"], { stdio: "inherit" });
    input.close();
    process.stdout.write("\n");
  }
}
const login = await exchange(
  "POST",
  "/login",
  new URLSearchParams({ pwd: password }).toString(),
  "application/x-www-form-urlencoded",
);
password = undefined;
assert(login.status === 302, "Login rejected.");
cookie = login.headers["set-cookie"]
  ?.find((value) => value.startsWith("session="))
  ?.split(";", 1)[0];
assert(cookie, "No authenticated session.");

async function get(path) {
  const response = await exchange("GET", path);
  assert(
    response.status === 200,
    `Read failed: ${path}, HTTP ${response.status}`,
  );
  return response.body;
}
const info = JSON.parse(await get("/information.json"));
assert(
  info.hw_ver === "SWTGW218AS 8+1 Managed Switch" && info.flash_size === "2 MB",
  "Hardware mismatch.",
);
assert(info.ip_address === "192.168.5.9", "Management address mismatch.");
const config = await get("/config");
assert(
  config.equals(await readFile(resolve(baselinePath, "config.txt"))),
  "Saved configuration changed since the verified snapshot.",
);
const networks = await get("/vlanlist");
assert.deepEqual(
  JSON.parse(networks),
  JSON.parse(await readFile(resolve(baselinePath, "vlanlist"))),
  "Active VLAN list changed.",
);
const backup = resolve(
  ".switch-backups",
  `${new Date().toISOString().replaceAll(":", "-")}-before-firmware-upload`,
);
await mkdir(backup, { mode: 0o700 });
const records = new Map([
  ["config.txt", config],
  ["vlanlist", networks],
]);
for (const { id } of JSON.parse(networks).vlan) {
  const bytes = await get(`/vlan.json?vid=${id}`);
  const name = `vlan-${id}.json`;
  assert.deepEqual(
    JSON.parse(bytes),
    JSON.parse(await readFile(resolve(baselinePath, name))),
    `VLAN ${id} changed.`,
  );
  records.set(name, bytes);
}
assert(
  config.equals(await get("/config")),
  "Saved configuration changed during verification.",
);
const manifest = {};
for (const [name, bytes] of records) {
  await writeFile(resolve(backup, name), bytes, { mode: 0o600 });
  manifest[name] = hash(bytes);
}
await writeFile(
  resolve(backup, "sha256.json"),
  JSON.stringify(manifest, null, 2),
  { mode: 0o600 },
);
console.log(
  `Configuration and VLANs match the verified snapshot. Backup: ${backup}`,
);

const boundary = `rtl-${randomBytes(12).toString("hex")}`;
const body = Buffer.concat([
  Buffer.from(
    `--${boundary}\r\nContent-Disposition: form-data; name="uploadedfile"; filename="${basename(imagePath)}"\r\nContent-Type: application/octet-stream\r\n\r\n`,
  ),
  image,
  Buffer.from(`\r\n--${boundary}--\r\n`),
]);
console.log(
  `Uploading verified image ${expectedHash}. One attempt; no automatic retry.`,
);
const uploaded = await exchange(
  "POST",
  "/upload",
  body,
  `multipart/form-data; boundary=${boundary}`,
  180000,
);
assert(
  uploaded.status === 200 &&
    uploaded.body.toString().startsWith("OK: checksum verified, rebooting"),
  `Upload not confirmed: HTTP ${uploaded.status}. Do not retry without checking device state.`,
);
console.log(
  "Upload accepted: device verified checksum and is rebooting to apply firmware.",
);
