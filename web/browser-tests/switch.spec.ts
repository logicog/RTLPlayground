import { expect, test } from "@playwright/test";

test.beforeEach(async ({ page }) => {
  await page.route("**/*", (route) => {
    const url = new URL(route.request().url());
    if (url.origin !== "http://127.0.0.1:5175") return route.abort();
    return route.continue();
  });
  await page.goto("/");
  await expect(page.getByText("Development preview · simulated switch data")).toBeVisible();
  await expect(page.locator(".network-choice")).toHaveCount(3);
});

test("VLAN overlay and port inspector stay on the same page", async ({ page }) => {
  await page.locator('.network-choice[data-vlan="4"]').click();
  await expect(page.locator(".front-port.in-network")).toHaveCount(2);
  await expect(page.locator('.front-port[data-port="1"]')).toContainText("T · Tagged");
  await expect(page.locator('.front-port[data-port="6"]')).toHaveClass(/outside-network/);
  await page.locator('.front-port[data-port="8"]').click();
  await expect(page.locator("#port-details-title")).toHaveText("Home server");
  await expect(page.locator(".port-network-list")).toContainText("smart_local");
  await expect(page.locator(".port-network-list .pvid-label")).toHaveCount(1);
  await expect(page).toHaveURL("http://127.0.0.1:5175/");
  await expect(page.locator("table")).toHaveCount(0);
});

test("port drafts survive refresh, tab changes and selection of another port", async ({ page }) => {
  await page.getByRole("button", { name: "Configure port", exact: true }).click();
  const name = page.locator('.port-inspector input[name="name"]');
  await name.fill("Router-draft");
  await page.locator('.port-inspector select[name="speed"]').selectOption("1g");
  await page.getByRole("button", { name: "Refresh", exact: true }).click();
  await expect(page.locator(".network-choice")).toHaveCount(3);
  await expect(name).toHaveValue("Router-draft");
  await page.getByRole("button", { name: "Status & networks", exact: true }).click();
  await page.getByRole("button", { name: "Configure port", exact: true }).click();
  await expect(name).toHaveValue("Router-draft");
  await page.locator('.front-port[data-port="8"]').click();
  await expect(name).toHaveValue("Home server");
  await page.locator('.front-port[data-port="1"]').click();
  await expect(name).toHaveValue("Router-draft");
  await expect(page.locator('.port-inspector select[name="speed"]')).toHaveValue("1g");
  await page.getByRole("button", { name: "Review changes", exact: true }).click();
  const dialog = page.getByRole("dialog");
  await expect(dialog).toContainText("Update Port 1");
  await dialog.locator("summary").click();
  await expect(dialog.locator("pre")).toHaveText("port 1 name Router-draft\nport 1 1g");
  await dialog.getByRole("button", { name: "Cancel", exact: true }).click();
  await page.getByRole("button", { name: "Discard draft", exact: true }).click();
  await expect(name).toHaveValue("Uplink");
});

test("failed VLAN reads show unknown membership instead of exclusion", async ({ page }) => {
  await page.locator('.network-choice[data-vlan="4"]').click();
  await page.evaluate(async () => {
    const apiPath = "/src/api.ts";
    const demoPath = "/src/demo.ts";
    const { api } = await import(apiPath);
    const { demoFetch } = await import(demoPath);
    api.useTransport((input: string, init: RequestInit) => {
      if (input.startsWith("/vlan.json"))
        return Promise.resolve(new Response("Unavailable", { status: 503 }));
      return demoFetch(input, init);
    });
  });
  await page.getByRole("button", { name: "Refresh", exact: true }).click();
  await expect(page.getByRole("button", { name: "Retry VLAN read" })).toBeVisible();
  await expect(page.locator(".front-port.in-network")).toHaveCount(0);
  await expect(page.locator(".front-port.outside-network")).toHaveCount(0);
  await expect(page.locator(".front-port").first()).toContainText("Unknown");
});

test("keyboard, narrow screens and dark theme", async ({ page }) => {
  const port = page.locator('.front-port[data-port="6"]');
  await port.focus();
  await page.keyboard.press("Enter");
  await expect(page.locator("#port-details-title")).toHaveText("Workstation");
  await page.getByRole("button", { name: "Toggle color theme" }).click();
  await expect(page.locator("html")).toHaveAttribute("data-theme", "dark");
  await page.setViewportSize({ width: 390, height: 844 });
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await expect(page.locator(".device-navigation nav a")).toHaveCount(4);
});
