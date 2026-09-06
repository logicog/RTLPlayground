import { expect, test } from "@playwright/test";

test.beforeEach(async ({ page }) => {
  await page.route("**/*", (route) => {
    if (new URL(route.request().url()).origin !== "http://127.0.0.1:5175") return route.abort();
    return route.continue();
  });
  await page.goto("/#vlans");
  await expect(page.locator(".vlan-navigation .network-choice")).toHaveCount(3);
});

test("uses the shared faceplate and retains VLAN drafts between networks", async ({ page }) => {
  await page.locator('.network-choice[data-vlan="4"]').click();
  await expect(page.locator(".front-port.in-network")).toHaveCount(2);
  await expect(page.locator("select")).toHaveCount(0);
  await page.locator('.front-port[data-port="6"]').click();
  await expect(page.locator("#vlan-port-title")).toHaveText("Workstation");
  await page.getByRole("radio", { name: "Tagged", exact: true }).check();
  await expect(page.locator(".front-port.in-network")).toHaveCount(3);
  await expect(page.locator(".vlan-change-summary")).toContainText("Port 6: none → tagged");
  await expect(page.getByRole("dialog")).toHaveCount(0);
  await page.locator('.network-choice[data-vlan="5"]').click();
  await page.locator('.network-choice[data-vlan="4"]').click();
  await page.locator('.front-port[data-port="6"]').click();
  await expect(page.getByRole("radio", { name: "Tagged", exact: true })).toBeChecked();
  await page.getByRole("button", { name: "Review changes", exact: true }).click();
  const dialog = page.getByRole("dialog");
  await expect(dialog).toBeVisible();
  await dialog.locator("summary").click();
  await expect(dialog.locator("pre")).toHaveText("vlan 4 1t 6t 8t");
  await dialog.getByRole("button", { name: "Cancel", exact: true }).click();
  await page.getByRole("button", { name: "Discard draft", exact: true }).click();
  await expect(page.locator(".front-port.in-network")).toHaveCount(2);
});

test("all-port actions update the draft without changing PVID or applying commands", async ({
  page,
}) => {
  await page.getByRole("button", { name: "Set all ports tagged", exact: true }).click();
  await expect(page.locator(".front-port-reading").filter({ hasText: "T · Tagged" })).toHaveCount(
    9,
  );
  await expect(page.locator(".front-port-reading .pvid-label")).toHaveCount(9);
  await expect(page.getByRole("button", { name: "Exclude all ports", exact: true })).toBeDisabled();
  await expect(page.getByRole("dialog")).toHaveCount(0);
  await page.getByRole("button", { name: "Set all ports untagged", exact: true }).click();
  await expect(page.locator(".front-port-reading").filter({ hasText: "U · Untagged" })).toHaveCount(
    9,
  );
  await expect(page.locator(".vlan-change-summary")).toContainText("No pending changes");
});

test("multi-select targets only checked ports and supports select all and clear", async ({
  page,
}) => {
  await page.locator('.network-choice[data-vlan="4"]').click();
  await page.getByRole("checkbox", { name: "Select multiple ports", exact: true }).check();
  await expect(
    page.getByRole("button", { name: "Set selected ports untagged", exact: true }),
  ).toBeDisabled();
  await page.locator('.front-port[data-port="2"]').click();
  await page.locator('.front-port[data-port="6"]').click();
  await page.getByRole("button", { name: "Set selected ports untagged", exact: true }).click();
  await expect(page.locator('.front-port[data-port="2"]')).toContainText("U · Untagged");
  await expect(page.locator('.front-port[data-port="6"]')).toContainText("U · Untagged");
  await expect(page.locator('.front-port[data-port="1"]')).toContainText("T · Tagged");
  await expect(page.locator('.front-port[data-port="8"]')).toContainText("T · Tagged");
  await page.getByRole("button", { name: "Select all", exact: true }).click();
  await expect(page.locator('.front-port[aria-pressed="true"]')).toHaveCount(9);
  await page.getByRole("button", { name: "Clear selection", exact: true }).click();
  await expect(page.locator('.front-port[aria-pressed="true"]')).toHaveCount(0);
  await page.setViewportSize({ width: 390, height: 844 });
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
});

test("PVID protection and narrow layout", async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await page.locator('.front-port[data-port="1"]').click();
  await expect(page.getByRole("checkbox", { name: "Use this VLAN as PVID" })).toBeChecked();
  await expect(page.getByRole("checkbox", { name: "Use this VLAN as PVID" })).toBeDisabled();
  await expect(page.getByRole("radio", { name: "Excluded", exact: true })).toBeDisabled();
  await expect(page.getByRole("button", { name: "Delete VLAN", exact: true })).toHaveCount(0);
  await page.setViewportSize({ width: 390, height: 844 });
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  const radio = await page.locator('input[type="radio"]').first().boundingBox();
  expect(radio?.width).toBeLessThanOrEqual(24);
  expect(radio?.height).toBeLessThanOrEqual(24);
  await page.locator(".skip").focus();
  await page.keyboard.press("Enter");
  await expect(page).toHaveURL("http://127.0.0.1:5175/#vlans");
  await expect(page.locator("h1")).toBeFocused();
});

test("refuses a stale VLAN draft before opening command review", async ({ page }) => {
  await page.locator('.network-choice[data-vlan="4"]').click();
  await page.locator('.front-port[data-port="6"]').click();
  await page.getByRole("radio", { name: "Tagged", exact: true }).check();
  await page.evaluate(async () => {
    const apiPath = "/src/api.ts";
    const demoPath = "/src/demo.ts";
    const { api } = await import(apiPath);
    const { demoFetch } = await import(demoPath);
    api.useTransport((input: string, init: RequestInit) => {
      if (input === "/vlan.json?vid=4")
        return Promise.resolve(new Response(JSON.stringify({ members: "0x2000001", pvid: "0x0" })));
      return demoFetch(input, init);
    });
  });
  await page.getByRole("button", { name: "Review changes", exact: true }).click();
  await expect(page.getByRole("alert")).toContainText("changed since you started editing");
  await expect(page.getByRole("dialog")).toHaveCount(0);
});

test("port settings stay inline without a popup or global action mode", async ({ page }) => {
  const port = page.locator('.front-port[data-port="6"]');
  await port.click();
  await expect(port).toContainText("U · Untagged");
  await expect(page.getByRole("button", { name: "Cycle tagging" })).toHaveCount(0);
  await expect(port.locator(".pvid-label")).toHaveText("PVID");
  await expect(page.locator(".vlan-change-summary")).toContainText("No pending changes");
  await page.getByRole("radio", { name: "Tagged", exact: true }).check();
  await expect(port).toContainText("T · Tagged");
  await expect(page.locator("[popover]")).toHaveCount(0);
  await expect(page.locator(".port-inspector .membership-options")).toBeVisible();
  await page.locator('.network-choice[data-vlan="4"]').click();
  await port.click();
  for (const [name, label] of [
    ["Tagged", "T · Tagged"],
    ["Untagged", "U · Untagged"],
    ["Excluded", "Excluded"],
  ]) {
    await page.getByRole("radio", { name, exact: true }).check();
    await expect(port).toContainText(label);
  }
  await page.screenshot({ path: "/tmp/rtl-vlan-inline-settings.png", fullPage: true });
  await expect(page.getByRole("dialog")).toHaveCount(0);
});
