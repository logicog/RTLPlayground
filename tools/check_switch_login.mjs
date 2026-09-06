/** Read-only browser check. Only login POSTs are permitted, with a fixed attempt limit. */
import { execFileSync } from "node:child_process";
import assert from "node:assert/strict";
import { readFile, readdir } from "node:fs/promises";
import { request as httpRequest } from "node:http";
import { createInterface } from "node:readline/promises";
import { chromium } from "../web/node_modules/playwright/index.mjs";

const origin = "http://192.168.5.9";
const readPaths = new Set([
  "/login.html",
  "/index.html",
  "/style.css",
  "/style2.css",
  "/app.js",
  "/core.js",
  "/vlans.js",
  "/system.js",
  "/lit.js",
  "/shared.js",
  "/device_front.js",
  "/port_workbench.js",
  "/port_actions.js",
  "/ports.js",
  "/port.svg",
  "/sfp.svg",
  "/i18n.js",
  "/information.json",
  "/status.json",
  "/vlanlist",
  "/vlan.json",
  "/mtu.json",
  "/config",
]);

let password = process.env.SWITCH_PASSWORD;
if (!password) {
  if (!process.stdin.isTTY)
    throw new Error("Use a terminal or SWITCH_PASSWORD.");
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

const browser = await chromium.launch({
  executablePath: process.env.PLAYWRIGHT_CHROMIUM_PATH,
  headless: true,
  // Local document overrides lack a real IP address in Chromium. The isolated
  // override test bypasses that address-space check; normal verification does not.
  args: process.argv.includes("--local-assets") ? ["--disable-web-security"] : [],
});
try {
  const context = await browser.newContext();
  const localAssets = process.argv.includes("--local-assets") ? new Set((await readdir("html")).map(name => `/${name}`)) : new Set();
  let loginAttempts = 0;
  const verifySession = process.argv.includes("--verify-session");
  const loginLimit = verifySession ? 2 : 1;
  await context.route("**/*", async (route) => {
    const request = route.request();
    const url = new URL(request.url());
    if (url.origin !== origin) return route.abort();
    if (request.method() === "GET" && localAssets.has(url.pathname)) {
      const extension = url.pathname.split(".").at(-1);
      const types = { html: "text/html", css: "text/css", js: "text/javascript", svg: "image/svg+xml" };
      return route.fulfill({ status: 200, contentType: types[extension], body: await readFile(`html${url.pathname}`) });
    }
    if (
      request.method() === "POST" &&
      url.pathname === "/login" &&
      loginAttempts < loginLimit
    ) {
      loginAttempts++;
      // Apply the frontend fix in this browser without modifying device flash.
      if (!process.argv.includes("--apply-header-fix")) return route.continue();
      return route.continue({
        headers: {
          ...request.headers(),
          "content-type": "application/x-www-form-urlencoded",
        },
      });
    }
    if (request.method() === "GET" && readPaths.has(url.pathname))
      return route.continue();
    return route.abort();
  });
  const page = await context.newPage();
  page.on("console", message => { if (message.type() === "error") console.log(`Browser console: ${message.text()}`); });
  const requestTimes = new Map();
  page.on("request", request => requestTimes.set(request, Date.now()));
  page.on("requestfinished", request => {
    const path = new URL(request.url()).pathname;
    if (path.endsWith(".json")) console.log(`GET ${path}: ${Date.now() - requestTimes.get(request)} ms`);
  });
  page.on("requestfailed", request => console.log(`Request failed: ${new URL(request.url()).pathname}: ${request.failure()?.errorText}`));
  page.on("pageerror", (error) =>
    console.log(`Browser error: ${error.message}`),
  );
  page.on("response", (response) => {
    const path = new URL(response.url()).pathname;
    if (path === "/login" || response.status() >= 400) {
      console.log(
        `${response.request().method()} ${path}: HTTP ${response.status()}`,
      );
    }
  });
  const loginPage = await page.goto(`${origin}/login.html`, {
    waitUntil: "networkidle",
    timeout: 60000,
  });
  if (!process.argv.includes("--apply-header-fix")) {
    assert(
      (await loginPage.body()).equals(await readFile("html/login.html")),
      "Device login page differs from the corrected build.",
    );
    console.log(localAssets.size ? "Testing local UI assets against the physical switch API; no flash writes." : "PASS: device serves the corrected login asset byte for byte.");
  }
  await page.locator('input[name="pwd"]').fill(password);
  if (!verifySession) password = undefined;
  await page.locator('button[type="submit"]').click();
  try {
    await page.waitForURL(`${origin}/index.html`, { timeout: 30000, waitUntil: "domcontentloaded" });
  } catch (error) {
    console.log(`Login check stopped at ${page.url()}; message: ${await page.locator("#login-error").textContent().catch(() => "unavailable")}`);
    throw error;
  }
  console.log(
    "PASS: corrected login opened /index.html on the physical switch.",
  );
  await page.locator(".front-ports button").first().waitFor({ timeout: 60000 });
  console.log(
    "PASS: physical-switch port view rendered. No configuration or firmware writes.",
  );
  await page.evaluate(() => { location.hash = "vlans"; });
  await page.locator("rtl-vlans .front-ports button").first().click();
  await page.locator(".vlan-port-editor .membership-options").waitFor();
  assert.equal(await page.locator("[popover]").count(), 0, "Unexpected popup menu.");
  console.log("PASS: VLAN port settings render inline, without a popup.");
  if (verifySession) {
    await page.evaluate(() => window.dispatchEvent(new Event("session-expired")));
    const dialog = page.getByRole("dialog");
    await dialog.getByLabel("Password", { exact: true }).fill(password);
    password = undefined;
    await dialog.getByRole("button", { name: "Sign in", exact: true }).click();
    await dialog.waitFor({ state: "detached", timeout: 30000 });
    console.log("PASS: session renewal logged in on the physical switch without a header override.");
  }
  if (process.argv.includes("--verify-settings")) {
    await page.evaluate(() => { location.hash = "system"; });
    const autoSave = page.getByRole("checkbox", { name: "Save automatically", exact: true });
    await autoSave.waitFor();
    assert.equal(await autoSave.isChecked(), false, "Auto-save must default to off.");
    assert.equal(await page.locator("header .configuration-status").count(), 1);
    assert.equal(await page.getByRole("button", { name: "Export configuration", exact: true }).count(), 1);
    console.log("PASS: header save status, opt-in auto-save and explicit export are available on the device.");
  }
  // Stop UI polling before sequential verification reads. Avoid a pooled browser
  // HTTP client: this firmware supports only one connection and closes responses.
  await page.close();
  const sessionCookie = (await context.cookies(origin)).find(cookie => cookie.name === "session");
  assert(sessionCookie, "No session cookie after browser verification.");
  async function getDevice(path) {
    return new Promise((resolveResponse, reject) => {
      const request = httpRequest(`${origin}${path}`, {
        agent: false,
        headers: { Connection: "close", Cookie: `session=${sessionCookie.value}` },
      }, response => {
        const chunks = [];
        response.on("data", chunk => chunks.push(chunk));
        response.on("error", reject);
        response.on("end", () => {
          if (response.statusCode !== 200) return reject(new Error(`Read failed: ${path}, HTTP ${response.statusCode}`));
          resolveResponse(Buffer.concat(chunks));
        });
      });
      request.setTimeout(10000, () => request.destroy(new Error(`Read timed out: ${path}. No retry.`)));
      request.on("error", reject);
      request.end();
    });
  }
  for (const name of localAssets.size ? [] : ["app.js", "switching.js", "vlans.js", "system.js", "core.js", "style.css"]) {
    const bytes = await getDevice(`/${name}`);
    assert(bytes.equals(await readFile(`html/${name}`)), `Device asset differs from build: ${name}`);
  }
  if (!localAssets.size) console.log("PASS: application, aggregation, core and stylesheet match the new build byte for byte.");
} finally {
  await browser.close();
}
