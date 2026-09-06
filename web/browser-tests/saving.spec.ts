import { expect, test, type Page } from "@playwright/test";

test.beforeEach(async ({ page }) => {
  await page.route("**/*", (route) => {
    if (new URL(route.request().url()).origin !== "http://127.0.0.1:5175") return route.abort();
    return route.continue();
  });
  await page.goto("/#system");
  await expect(
    page.getByRole("checkbox", { name: "Save automatically", exact: true }),
  ).toBeVisible();
});

async function recordWrites(page: Page, failSave = false) {
  await page.evaluate(async (fail) => {
    const apiPath = "/src/api.ts";
    const demoPath = "/src/demo.ts";
    const { api } = await import(apiPath);
    const { demoFetch } = await import(demoPath);
    const state = window as typeof window & { savedBodies: string[] };
    state.savedBodies = [];
    api.useTransport(async (input: string, init?: RequestInit) => {
      if (input === "/config" && init?.method === "POST") {
        const file = (init.body as FormData).get("configuration") as Blob;
        state.savedBodies.push(await file.text());
        if (fail) return new Response("Save failed", { status: 500 });
      }
      return demoFetch(input, init);
    });
  }, failSave);
}

async function savedBodies(page: Page): Promise<string[]> {
  return page.evaluate(() => (window as typeof window & { savedBodies: string[] }).savedBodies);
}

async function editPort(page: Page) {
  await page.evaluate(() => {
    location.hash = "ports";
  });
  await page.locator('.front-port[data-port="6"]').click();
  await page.locator('.port-inspector input[name="name"]').fill("New-workstation");
  await expect(page.locator("header .draft-status")).toHaveText("Unapplied changes");
  await page.getByRole("button", { name: "Review changes", exact: true }).click();
}

test("header Save writes without downloads; Export remains explicit", async ({ page }) => {
  await recordWrites(page);
  const downloads: string[] = [];
  page.on("download", (download) => downloads.push(download.suggestedFilename()));
  await editPort(page);
  await page.getByRole("button", { name: "Apply changes", exact: true }).click();
  await expect(page.locator("header .startup-status")).toHaveText("Unsaved · 1");
  await expect(page.locator("header .draft-status")).toHaveCount(0);
  expect(await savedBodies(page)).toEqual([]);
  await page.locator("header").getByRole("button", { name: "Save", exact: true }).click();
  await expect(page.locator("header .startup-status")).toHaveText("Saved");
  const bodies = await savedBodies(page);
  expect(bodies).toHaveLength(1);
  expect(bodies[0]).toContain("vlan 4 smart_local 1t 8t\n");
  expect(bodies[0]).toContain("port 6 name New-workstation\n");
  expect(downloads).toEqual([]);
  await page.evaluate(() => {
    location.hash = "system";
  });
  const download = page.waitForEvent("download");
  await page.getByRole("button", { name: "Export configuration", exact: true }).click();
  expect((await download).suggestedFilename()).toBe("rtlplayground-config.txt");
});

test("auto-save runs only after Apply and persists as a browser preference", async ({ page }) => {
  await recordWrites(page);
  await page.getByRole("checkbox", { name: "Save automatically", exact: true }).check();
  expect(await savedBodies(page)).toEqual([]);
  await editPort(page);
  expect(await savedBodies(page)).toEqual([]);
  await page.getByRole("button", { name: "Apply changes", exact: true }).click();
  await expect(page.locator("header .startup-status")).toHaveText("Saved");
  await expect.poll(async () => (await savedBodies(page)).length).toBe(1);
  await expect(page.locator("header .draft-status")).toHaveCount(0);
  await expect(
    page.locator(".page-heading").getByRole("button", { name: "Refresh", exact: true }),
  ).toBeEnabled();
  await page.evaluate(() => {
    location.hash = "system";
  });
  await expect(
    page.getByRole("checkbox", { name: "Save automatically", exact: true }),
  ).toBeChecked();
  await page.reload();
  await expect(
    page.getByRole("checkbox", { name: "Save automatically", exact: true }),
  ).toBeChecked();
});

test("failed auto-save remains visible and is not retried by navigation or polling", async ({
  page,
}) => {
  await recordWrites(page, true);
  await page.getByRole("checkbox", { name: "Save automatically", exact: true }).check();
  await editPort(page);
  await page.getByRole("button", { name: "Apply changes", exact: true }).click();
  await expect(page.locator("header .startup-status")).toHaveText("Save failed");
  await expect(page.locator("header .draft-status")).toHaveCount(0);
  await expect(
    page.locator(".page-heading").getByRole("button", { name: "Refresh", exact: true }),
  ).toBeEnabled();
  await page.evaluate(() => {
    location.hash = "system";
  });
  await expect(page.locator("header .startup-status")).toHaveText("Save failed");
  await page.locator("header").getByRole("button", { name: "Verify save", exact: true }).click();
  await expect(page.locator("header .startup-status")).toHaveText("Save failed");
  expect(await savedBodies(page)).toHaveLength(1);
  await page.setViewportSize({ width: 390, height: 844 });
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await page.screenshot({ path: "/tmp/rtl-save-status-mobile.png", fullPage: true });
});
