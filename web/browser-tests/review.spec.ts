import { expect, test } from "@playwright/test";

test.beforeEach(async ({ page }) => {
  await page.route("**/*", (route) =>
    new URL(route.request().url()).origin === "http://127.0.0.1:5175"
      ? route.continue()
      : route.abort(),
  );
  await page.goto("/");
  await expect(page.locator(".front-port")).toHaveCount(9);
});

test("moving an assigned port updates both groups in one review and one discard", async ({
  page,
}) => {
  await page.evaluate(async () => {
    const apiPath = "/src/api.ts";
    const demoPath = "/src/demo.ts";
    const { api } = await import(apiPath);
    const { demoFetch } = await import(demoPath);
    api.useTransport((input: string, init: RequestInit) => {
      if (init?.method === "POST") throw new Error("Draft editing must not write to the switch");
      if (input === "/lag.json")
        return Promise.resolve(
          new Response(
            JSON.stringify([
              { members: "10", hash: "0x03" },
              { members: "100000", hash: "0x03" },
              { members: "10000000", hash: "0x03" },
              { members: "0", hash: "0x03" },
            ]),
          ),
        );
      return demoFetch(input, init);
    });
    location.hash = "lag";
  });
  await page.locator('[data-group="2"]').click();
  await page.locator('.front-port[data-port="2"]').click();
  await expect(page.locator('.front-port[data-port="2"]')).toContainText("Group 2");
  await expect(page.locator('[data-group="1"]')).toContainText("0 ports");
  await expect(page.locator('[data-group="2"]')).toContainText("2 ports");
  await expect(page.locator('.front-port[data-port="8"]')).toContainText("Group 3");
  await expect(page.getByRole("alert")).toHaveCount(0);
  await expect(page.getByRole("dialog")).toHaveCount(0);
  const review = page.getByRole("button", { name: "Review changes", exact: true });
  await expect(review).toHaveCount(1);
  await review.click();
  const dialog = page.getByRole("dialog");
  await dialog.locator("summary").click();
  await expect(dialog.locator("pre")).toHaveText("lag 1\nlag 2 2 6");
  await dialog.getByRole("button", { name: "Cancel", exact: true }).click();
  await page.getByRole("button", { name: "Discard draft", exact: true }).click();
  await expect(page.locator('.front-port[data-port="2"]')).toContainText("Group 1");
  await expect(page.locator('[data-group="2"]')).toContainText("1 ports");
  await expect(review).toBeDisabled();
  await expect(page.locator(".port-inspector .inspector-heading")).toHaveCount(0);
});

test("one STP review includes bridge settings and edits on multiple ports", async ({ page }) => {
  await page.evaluate(() => {
    location.hash = "stp";
  });
  await page.locator('.front-port[data-port="2"]').click();
  await page
    .getByRole("spinbutton", { name: "Path cost (0 = automatic)", exact: true })
    .fill("500");
  await page.getByRole("button", { name: "Disable participation", exact: true }).click();
  await page.locator('.front-port[data-port="6"]').click();
  await page.locator(".feature-context summary").click();
  await page.getByRole("spinbutton", { name: "Hello time (s)", exact: true }).fill("3");
  const review = page.getByRole("button", { name: /^Review/ });
  await expect(review).toHaveCount(1);
  await review.click();
  const dialog = page.getByRole("dialog");
  await dialog.locator("summary").click();
  await expect(dialog.locator("pre")).toHaveText(
    "stp hello 3\nstp port 2 cost 500\nstp port 6 off",
  );
});

test("system has one review and does not submit untouched network or password fields", async ({
  page,
}) => {
  await page.evaluate(() => {
    location.hash = "system";
  });
  await page.getByRole("textbox", { name: "Hostname", exact: true }).fill("office-switch");
  const review = page.getByRole("button", { name: /^Review/ });
  await expect(review).toHaveCount(1);
  await review.click();
  const dialog = page.getByRole("dialog");
  await dialog.locator("summary").click();
  await expect(dialog.locator("pre")).toHaveText("hostname office-switch");
});

test("one port review includes settings and state drafts from different ports", async ({
  page,
}) => {
  await page.evaluate(() => {
    location.hash = "ports";
  });
  await page.locator('.front-port[data-port="2"]').click();
  await page.getByRole("textbox", { name: "Port name", exact: true }).fill("camera");
  await page.getByRole("button", { name: "Disable port", exact: true }).click();
  await page.locator('.front-port[data-port="6"]').click();
  const review = page.getByRole("button", { name: /^Review/ });
  await expect(review).toHaveCount(1);
  await review.click();
  const dialog = page.getByRole("dialog");
  await dialog.locator("summary").click();
  await expect(dialog.locator("pre")).toHaveText("port 2 name camera\nport 6 off");
});

test("groups and VLANs keep the same ID colors across routes and themes", async ({ page }) => {
  await page.evaluate(() => {
    location.hash = "lag";
  });
  await expect(page.locator("[data-group]")).toHaveCount(4);
  const hues = await page
    .locator("[data-group]")
    .evaluateAll((elements) =>
      elements.map((el) => (el as HTMLElement).style.getPropertyValue("--entity-hue")),
    );
  expect(new Set(hues).size).toBe(4);
  await page.locator('[data-group="2"]').click();
  await page.locator('.front-port[data-port="3"]').click();
  await expect(page.locator('.front-port[data-port="3"]')).toHaveAttribute(
    "style",
    `--entity-hue: ${hues[1]}`,
  );
  await page.getByRole("button", { name: "Toggle color theme" }).click();
  await expect(page.locator('[data-group="2"]')).toHaveAttribute(
    "style",
    `--entity-hue: ${hues[1]}`,
  );
  await page.screenshot({
    path: "/tmp/rtl-aggregation-compact-dark.png",
    fullPage: true,
    animations: "disabled",
  });
  page.once("dialog", (dialog) => dialog.accept());
  await page.evaluate(() => {
    location.hash = "vlans";
  });
  await expect(page.locator('.network-choice[data-vlan="1"]')).toHaveAttribute(
    "style",
    `--entity-hue: ${hues[0]}`,
  );
  await expect(page.locator('.network-choice[data-vlan="4"]')).toHaveAttribute(
    "style",
    `--entity-hue: ${hues[3]}`,
  );
  await page.screenshot({ path: "/tmp/rtl-vlan-colors-dark.png", fullPage: true });
});
