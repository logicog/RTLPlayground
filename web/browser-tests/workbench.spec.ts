import { expect, test } from "@playwright/test";

test.beforeEach(async ({ page }) => {
  await page.route("**/*", (route) => {
    if (new URL(route.request().url()).origin !== "http://127.0.0.1:5175") return route.abort();
    return route.continue();
  });
  await page.goto("/");
  await expect(page.locator(".front-port")).toHaveCount(9);
});

test("every port feature keeps the faceplate and selected connection", async ({ page }) => {
  await page.locator('.front-port[data-port="6"]').click();
  for (const mode of [
    "ports",
    "lag",
    "eee",
    "bandwidth",
    "stp",
    "mirror",
    "statistics",
    "l2",
    "vlans",
    "overview",
  ]) {
    await page.evaluate((route) => {
      location.hash = route;
    }, mode);
    await expect(page.locator(".device-front")).toHaveCount(1);
    await expect(page.locator(".front-port")).toHaveCount(9);
    if (mode !== "lag")
      await expect(
        page.locator(
          '.front-port[data-port="6"][aria-pressed], .front-port[data-port="6"] .front-port-select',
        ),
      ).toHaveAttribute("aria-pressed", "true");
    await expect(page.locator(".port-inspector")).toContainText(
      mode === "lag" ? "Group 1" : "Workstation",
    );
    await expect(page.getByRole("dialog")).toHaveCount(0);
    await expect(page.locator("table")).toHaveCount(0);
    await page.screenshot({ path: `/tmp/rtl-workbench-${mode}.png`, fullPage: true });
    await page.setViewportSize({ width: 390, height: 844 });
    expect(
      await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth),
      mode,
    ).toBe(true);
    if (mode === "stp")
      await page.screenshot({ path: "/tmp/rtl-workbench-mobile.png", fullPage: true });
    await page.setViewportSize({ width: 1440, height: 1000 });
  }
});

test("spanning-tree drafts follow the selected port and preserve other ports", async ({ page }) => {
  await page.evaluate(() => {
    location.hash = "stp";
  });
  await page.locator('.front-port[data-port="2"]').click();
  await page
    .getByRole("spinbutton", { name: "Path cost (0 = automatic)", exact: true })
    .fill("500");
  await page.locator('.front-port[data-port="6"]').click();
  await expect(
    page.getByRole("spinbutton", { name: "Path cost (0 = automatic)", exact: true }),
  ).toHaveValue("0");
  await page.locator('.front-port[data-port="2"]').click();
  await expect(
    page.getByRole("spinbutton", { name: "Path cost (0 = automatic)", exact: true }),
  ).toHaveValue("500");
  await page.getByRole("button", { name: "Review changes", exact: true }).click();
  const dialog = page.getByRole("dialog");
  await dialog.locator("summary").click();
  await expect(dialog.locator("pre")).toHaveText("stp port 2 cost 500");
});

test("active links and VLAN membership stay legible in dark mode", async ({ page }) => {
  await page.getByRole("button", { name: "Toggle color theme" }).click();
  await page.locator('.network-choice[data-vlan="4"]').click();
  await expect(page.locator('.front-port[data-port="1"] .membership-badge')).toHaveText(
    "T · Tagged",
  );
  await expect(page.locator('.front-port[data-port="2"] .membership-badge')).toHaveText("Excluded");
  await page.screenshot({ path: "/tmp/rtl-workbench-dark.png", fullPage: true });
});

test("aggregation edits membership directly on ports without applying it", async ({ page }) => {
  await page.evaluate(() => {
    location.hash = "lag";
  });
  await page.locator('.front-port[data-port="2"]').click();
  await expect(page.locator('.front-port[data-port="2"]')).toContainText("Group 1");
  await page.locator('.front-port[data-port="6"]').click();
  await page.locator('.front-port[data-port="2"]').click();
  await expect(page.locator('.front-port[data-port="2"]')).toHaveAttribute("aria-pressed", "false");
  await page.locator('.front-port[data-port="2"]').click();
  await expect(page.locator('.front-port[data-port="2"]')).toHaveAttribute("aria-pressed", "true");
  await expect(page.getByRole("checkbox")).toHaveCount(0);
  await expect(page.getByRole("dialog")).toHaveCount(0);
  await page.getByRole("button", { name: "Review changes", exact: true }).click();
  const dialog = page.getByRole("dialog");
  await dialog.locator("summary").click();
  await expect(dialog.locator("pre")).toHaveText("lag 1 2 6");
});

test("bandwidth drafts stay attached to their physical port", async ({ page }) => {
  await page.evaluate(() => {
    location.hash = "bandwidth";
  });
  await page.locator('.front-port[data-port="2"]').click();
  await page.getByRole("spinbutton", { name: "Ingress (Kbit/s)", exact: true }).fill("1600");
  await page.locator('.front-port[data-port="6"]').click();
  await expect(page.getByRole("spinbutton", { name: "Ingress (Kbit/s)", exact: true })).toHaveValue(
    "0",
  );
  await page.locator('.front-port[data-port="2"]').click();
  await expect(page.getByRole("spinbutton", { name: "Ingress (Kbit/s)", exact: true })).toHaveValue(
    "1600",
  );
  await page.getByRole("button", { name: "Review changes", exact: true }).click();
  const dialog = page.getByRole("dialog");
  await dialog.locator("summary").click();
  await expect(dialog.locator("pre")).toHaveText("bw in 2 00000640\nbw in 2 fc");
});

test("mirroring preserves source directions while changing the inspected port", async ({
  page,
}) => {
  await page.evaluate(() => {
    location.hash = "mirror";
  });
  await page.getByRole("button", { name: "Copy RX", exact: true }).click();
  await page.locator('.front-port[data-port="2"]').click();
  await expect(page.locator('.front-port[data-port="2"]')).toContainText("RX source");
  await page.getByRole("button", { name: "Copy TX", exact: true }).click();
  await page.locator('.front-port[data-port="6"]').click();
  await page.getByRole("button", { name: "Inspect", exact: true }).click();
  await page.locator('.front-port[data-port="2"]').click();
  await expect(page.locator('.front-port[data-port="2"]')).toContainText("RX source");
  await expect(page.getByRole("dialog")).toHaveCount(0);
  await page.getByRole("button", { name: "Review changes", exact: true }).click();
  const dialog = page.getByRole("dialog");
  await dialog.locator("summary").click();
  await expect(dialog.locator("pre")).toHaveText("mirror 9 2r 6t");
});

test("EEE port clicks toggle drafts and leave SFP untouched", async ({ page }) => {
  await page.evaluate(() => {
    location.hash = "eee";
  });
  await page.locator('.front-port[data-port="2"]').click();
  await page.locator('.front-port[data-port="6"]').click();
  await page.locator('.front-port[data-port="2"]').click();
  await page.locator('.front-port[data-port="9"]').click();
  await expect(page.locator('.front-port[data-port="9"]')).toContainText("Not supported");
  await expect(page.getByRole("dialog")).toHaveCount(0);
  await page.getByRole("button", { name: "Review changes", exact: true }).click();
  const dialog = page.getByRole("dialog");
  await dialog.locator("summary").click();
  await expect(dialog.locator("pre")).toHaveText("eee 6 off");
});

test("port state and STP tools change only clicked ports", async ({ page }) => {
  for (const [route, action, review, command] of [
    ["ports", "Disable port", "Review changes", "port 6 off"],
    ["stp", "Disable participation", "Review changes", "stp port 6 off"],
  ]) {
    if (route === "stp") page.once("dialog", (dialog) => dialog.accept());
    await page.evaluate((value) => {
      location.hash = value;
    }, route);
    await page.getByRole("button", { name: action, exact: true }).click();
    await page.locator('.front-port[data-port="6"]').click();
    await expect(page.getByRole("dialog")).toHaveCount(0);
    await page.getByRole("button", { name: review, exact: true }).click();
    const dialog = page.getByRole("dialog");
    await dialog.locator("summary").click();
    await expect(dialog.locator("pre")).toHaveText(command);
    await dialog.getByRole("button", { name: "Cancel", exact: true }).click();
  }
});
