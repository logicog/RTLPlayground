/** Local check of production assets extracted from an image. No hardware access. */
import assert from "node:assert/strict";
import { readFile, readdir } from "node:fs/promises";
import { createServer } from "node:http";
import { resolve } from "node:path";
import { chromium } from "../web/node_modules/playwright/index.mjs";

const [imagePath, snapshotPath] = process.argv.slice(2);
assert(
  imagePath && snapshotPath,
  "Usage: node tools/verify_ui_image.mjs IMAGE SNAPSHOT_DIRECTORY",
);
const binary = await readFile(imagePath);
const header = await readFile("html_data.h", "utf8");
const definitions = new Map(
  [
    ...header.matchAll(/#define (FDATA_(?:START|SIZE)_\w+) (0x[0-9a-f]+|\d+)/g),
  ].map(([, key, value]) => [key, Number(value)]),
);
const assets = new Map();
const mime = {
  html: "text/html",
  js: "text/javascript",
  css: "text/css",
  svg: "image/svg+xml",
};

for (const name of await readdir("html")) {
  const symbol = name.replaceAll(".", "_");
  const start = definitions.get(`FDATA_START_${symbol}`);
  const length = definitions.get(`FDATA_SIZE_${symbol}`);
  assert(
    Number.isInteger(start) && Number.isInteger(length),
    `Missing asset index: ${name}`,
  );
  assert(
    start >= 0x40000 && start + length < 0x6f000,
    `Asset out of bounds: ${name}`,
  );
  const bytes = binary.subarray(start, start + length);
  assert(
    bytes.equals(await readFile(resolve("html", name))),
    `Image/source mismatch: ${name}`,
  );
  assets.set(`/${name}`, { bytes, type: mime[name.split(".").at(-1)] });
}

const snapshots = new Map();
for (const name of [
  "information.json",
  "status.json",
  "vlanlist",
  "mtu.json",
  "stp.json",
  "lag.json",
  "eee.json",
  "bandwidth.json",
  "mirror.json",
]) {
  snapshots.set(`/${name}`, await readFile(resolve(snapshotPath, name)));
}
const networks = JSON.parse(snapshots.get("/vlanlist"));
for (const vlan of networks.vlan) {
  snapshots.set(
    `/vlan.json?vid=${vlan.id}`,
    await readFile(resolve(snapshotPath, `vlan-${vlan.id}.json`)),
  );
}
snapshots.set("/config", await readFile(resolve(snapshotPath, "config.txt")));
const publicAssets = new Set([
  "/login.html",
  "/style.css",
  "/port.svg",
  "/sfp.svg",
  "/i18n.js",
]);
const csp =
  "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; form-action 'self'";
const requests = [];
const overlaps = [];
let activeRequests = 0;
function serve(request, response) {
  const path = request.url === "/" ? "/index.html" : request.url;
  requests.push(`${request.method} ${path}`);
  response.setHeader("Connection", "close");
  if (request.method === "POST" && path === "/login") {
    request.resume();
    // Match the firmware parser: a charset suffix currently produces HTTP 400.
    if (
      request.headers["content-type"] !== "application/x-www-form-urlencoded"
    ) {
      response.writeHead(400);
      response.end("Unsupported login content type.");
      return;
    }
    response.writeHead(302, {
      Location: "/index.html",
      "Set-Cookie": "session=local-image-test; Path=/",
    });
    response.end();
    return;
  }
  if (request.method !== "GET") {
    response.writeHead(405);
    response.end("Writes are disabled in the image test.");
    return;
  }
  if (
    !publicAssets.has(path) &&
    request.headers.cookie !== "session=local-image-test"
  ) {
    response.writeHead(302, { Location: "/login.html" });
    response.end();
    return;
  }
  if (snapshots.has(path)) {
    response.setHeader(
      "Content-Type",
      path === "/config" ? "text/plain" : "application/json",
    );
    response.end(snapshots.get(path));
    return;
  }
  const asset = assets.get(path);
  if (!asset) {
    response.writeHead(404);
    response.end();
    return;
  }
  response.setHeader("Content-Type", `${asset.type}; charset=UTF-8`);
  response.setHeader("Content-Security-Policy", csp);
  response.end(asset.bytes);
}
const server = createServer((request, response) => {
  activeRequests++;
  response.once("finish", () => { activeRequests--; });
  if (activeRequests > 1) {
    overlaps.push(request.url);
    response.writeHead(503, { Connection: "close" });
    response.end("The firmware only handles one connection at a time.");
    return;
  }
  // Deliberately slow responses expose parallel stylesheet/module/API requests.
  setTimeout(() => serve(request, response), 80);
});

await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
const origin = `http://127.0.0.1:${server.address().port}`;
let browser;
try {
  browser = await chromium.launch({
    executablePath: process.env.PLAYWRIGHT_CHROMIUM_PATH || undefined,
  });
  const page = await browser.newPage();
  const errors = [];
  page.on("pageerror", (error) => errors.push(error.message));
  await page.route("**/*", (route) => {
    if (new URL(route.request().url()).origin !== origin) return route.abort();
    return route.continue();
  });
  await page.goto(origin);
  await page.locator('input[name="pwd"]').fill("local-test-only");
  await page.getByRole("button", { name: /Sign in/ }).click();
  await page.waitForURL(`${origin}/index.html`);
  await page.locator(".front-port").first().waitFor();
  await page
    .getByRole("button", { name: "Configure port", exact: true })
    .click();
  const draftName = page.locator('.port-inspector input[name="name"]');
  await draftName.fill("Session-draft");
  await page.evaluate(() => window.dispatchEvent(new Event("session-expired")));
  const sessionDialog = page.getByRole("dialog");
  await sessionDialog
    .getByLabel("Password", { exact: true })
    .fill("local-test-only");
  await sessionDialog
    .getByRole("button", { name: "Sign in", exact: true })
    .click();
  await sessionDialog.waitFor({ state: "detached" });
  assert.equal(
    await draftName.inputValue(),
    "Session-draft",
    "Reauthentication lost the port draft.",
  );
  await page
    .getByRole("button", { name: "Discard draft", exact: true })
    .click();
  assert.equal(
    await page.locator(".preview-label").count(),
    0,
    "Development transport leaked into production",
  );
  const routes = [
    "overview",
    "ports",
    "vlans",
    "stp",
    "lag",
    "eee",
    "bandwidth",
    "statistics",
    "mirror",
    "system",
    "firmware",
  ];
  for (const route of routes) {
    await page.evaluate((route) => {
      location.hash = route;
    }, route);
    await page.waitForFunction(
      (route) =>
        document.querySelector("rtl-app")?.route_ === route &&
        document.querySelector("rtl-app")?.ready_,
      route,
    );
    await page.waitForFunction(
      () =>
        !document
          .querySelector("main")
          ?.textContent?.includes("Reading switch state…"),
    );
    assert.equal(
      await page.locator(".notice.error").count(),
      0,
      `Page load failed: ${route}`,
    );
  }
  await page.evaluate(() => { location.hash = "vlans"; });
  const membership = page.locator("rtl-vlans .membership-toggle").first();
  await membership.waitFor();
  const beforeMembership = await membership.textContent();
  await membership.click();
  assert.notEqual(await membership.textContent(), beforeMembership, "Membership badge did not update its draft.");
  const confirmation = page.waitForEvent("dialog");
  await page.evaluate(() => { location.hash = "overview"; });
  const dialog = await confirmation;
  assert.match(dialog.message(), /Discard unapplied changes/);
  await dialog.dismiss();
  await page.waitForURL(`${origin}/index.html#vlans`);
  assert.notEqual(await membership.textContent(), beforeMembership, "Cancelled navigation lost the draft.");
  await page.getByRole("button", { name: "Discard draft", exact: true }).click();
  assert.equal(await membership.textContent(), beforeMembership, "Discard did not restore membership.");
  console.log("PASS: production membership interaction and cancelled navigation preserve drafts without writes.");
  assert.deepEqual(errors, [], "Production JavaScript errors");
  assert.deepEqual(overlaps, [], "Parallel requests exceed the firmware's one-connection limit");
  assert(
    !requests.some(
      (request) => request.startsWith("POST") && request !== "POST /login",
    ),
    "Unexpected mutation request",
  );
  console.log(
    `PASS: ${assets.size} assets match image bytes; both login forms and ${routes.length} routes work with firmware CSP, saved API responses and one HTTP request at a time.`,
  );
  console.log(
    "This validates browser behavior locally, not the 8051 HTTP stack or physical network.",
  );
} finally {
  await browser?.close();
  await new Promise((resolve) => server.close(resolve));
}
