import { expect, test, type Page } from "@playwright/test";

test.beforeEach(async ({ page }) => {
  await page.route("**/*", (route) => {
    if (new URL(route.request().url()).origin !== "http://127.0.0.1:5175") return route.abort();
    return route.continue();
  });
});

async function leave(page: Page, accept: boolean) {
  const dialogShown = page.waitForEvent("dialog");
  await page.evaluate(() => {
    location.hash = "statistics";
  });
  const dialog = await dialogShown;
  expect(dialog.message()).toContain("Discard unapplied changes");
  if (accept) await dialog.accept();
  else await dialog.dismiss();
}

test("membership badges cycle the draft and protect PVID", async ({ page }) => {
  const writes: string[] = [];
  page.on("request", (request) => {
    if (request.method() === "POST") writes.push(request.url());
  });
  await page.goto("/#vlans");
  await page.locator('.network-choice[data-vlan="4"]').click();
  const port = page.locator('.front-port[data-port="6"]');
  const badge = port.locator(".membership-toggle");
  await expect(badge).toHaveText("Excluded");
  for (const text of ["T · Tagged", "U · Untagged", "Excluded"]) {
    await badge.click();
    await expect(badge).toHaveText(text);
  }
  await expect(page.locator(".vlan-change-summary")).toContainText("No pending changes");
  await page.locator('.network-choice[data-vlan="1"]').click();
  await badge.focus();
  await page.keyboard.press("Enter");
  await expect(badge).toHaveText("T · Tagged");
  await page.keyboard.press("Space");
  await expect(badge).toHaveText("U · Untagged");
  await expect(port.locator(".pvid-label")).toHaveText("PVID");
  expect(writes).toEqual([]);
  await expect(page.locator("button button, [popover]")).toHaveCount(0);
});

for (const [route, action] of [
  ["ports", "Port 6: disable port"],
  ["eee", "Port 6: disable EEE"],
  ["stp", "Port 6: disable STP participation"],
  ["mirror", "Port 6: change mirror direction"],
]) {
  test(`${route}: badge edits directly and leaving requires confirmation`, async ({ page }) => {
    await page.goto(`/#${route}`);
    await page.getByRole("button", { name: action, exact: true }).click();
    await leave(page, false);
    await expect(page).toHaveURL(new RegExp(`#${route}$`));
    await page.getByRole("button", { name: "Review changes", exact: true }).click();
    await expect(page.getByRole("dialog")).toBeVisible();
    await page.getByRole("button", { name: "Cancel", exact: true }).click();
    await leave(page, true);
    await expect(page.locator("rtl-diagnostics")).toBeVisible();
  });
}

for (const route of ["vlans", "lag", "bandwidth", "system", "overview"]) {
  test(`${route}: drafts survive cancelled navigation and warn before closing`, async ({
    page,
  }) => {
    await page.goto(`/#${route}`);
    if (route === "vlans") {
      await page.locator('.front-port[data-port="6"] .membership-toggle').click();
    } else if (route === "lag") {
      await page.locator('.front-port[data-port="6"]').click();
    } else if (route === "bandwidth") {
      await page.getByRole("spinbutton", { name: "Ingress (Kbit/s)", exact: true }).fill("1600");
    } else if (route === "system") {
      await page.getByRole("textbox", { name: "Hostname", exact: true }).fill("New-switch");
    } else {
      await page.getByRole("button", { name: "Configure port", exact: true }).click();
      await page.locator('.port-inspector input[name="name"]').fill("New-port");
    }
    const warned = await page.evaluate(() => {
      const event = new Event("beforeunload", { cancelable: true });
      window.dispatchEvent(event);
      return event.defaultPrevented;
    });
    expect(warned).toBe(true);
    await leave(page, false);
    await expect(page).toHaveURL(new RegExp(`#${route}$`));
    await leave(page, true);
    await expect(page.locator("rtl-diagnostics")).toBeVisible();
  });
}

test("reverting edits does not leave a false dirty warning", async ({ page }) => {
  await page.goto("/#system");
  const hostname = page.getByRole("textbox", { name: "Hostname", exact: true });
  const original = await hostname.inputValue();
  await hostname.fill("Changed");
  await hostname.fill(original);
  const dialogs: string[] = [];
  page.on("dialog", (dialog) => {
    dialogs.push(dialog.message());
    void dialog.dismiss();
  });
  await page.evaluate(() => {
    location.hash = "vlans";
  });
  await expect(page.locator("rtl-vlans")).toBeVisible();
  const badge = page.locator('.front-port[data-port="6"] .membership-toggle');
  await badge.click();
  await badge.click();
  await page.evaluate(() => {
    location.hash = "ports";
  });
  await expect(page.locator("rtl-ports")).toBeVisible();
  expect(dialogs).toEqual([]);
});

test("refresh asks before discarding edited form fields", async ({ page }) => {
  await page.goto("/#bandwidth");
  const ingress = page.getByRole("spinbutton", { name: "Ingress (Kbit/s)", exact: true });
  await ingress.fill("1600");
  page.once("dialog", (dialog) => dialog.dismiss());
  await page
    .locator(".connection-browser")
    .getByRole("button", { name: "Refresh", exact: true })
    .click();
  await expect(ingress).toHaveValue("1600");
  page.once("dialog", (dialog) => dialog.accept());
  await page
    .locator(".connection-browser")
    .getByRole("button", { name: "Refresh", exact: true })
    .click();
  await expect(ingress).toHaveValue("0");
});

test("browser Back can be cancelled without losing the current draft", async ({ page }) => {
  await page.goto("/#overview");
  await expect(page.locator(".front-port")).toHaveCount(9);
  await page.evaluate(() => {
    location.hash = "vlans";
  });
  await page.locator('.front-port[data-port="6"] .membership-toggle').click();
  const dialogShown = page.waitForEvent("dialog");
  await page.evaluate(() => history.back());
  await (await dialogShown).dismiss();
  await expect(page).toHaveURL(/#vlans$/);
  await expect(page.locator('.front-port[data-port="6"] .membership-toggle')).toHaveText(
    "T · Tagged",
  );
});

test("bandwidth badge focuses the chosen port's numeric settings", async ({ page }) => {
  await page.goto("/#bandwidth");
  await page.getByRole("button", { name: "Port 6: edit bandwidth limits", exact: true }).click();
  await expect(page.locator('form[data-bandwidth="6"] input[name="ingress"]')).toBeFocused();
  await expect(page.locator("button button")).toHaveCount(0);
});

test("saving the browser session preference clears its navigation warning", async ({ page }) => {
  await page.goto("/#system");
  await page.getByRole("combobox", { name: "Automatic sign-out", exact: true }).selectOption("30");
  await leave(page, false);
  await page.getByRole("button", { name: /Save session preference/ }).click();
  const dialogs: string[] = [];
  page.on("dialog", (dialog) => {
    dialogs.push(dialog.message());
    void dialog.dismiss();
  });
  await page.evaluate(() => {
    location.hash = "overview";
  });
  await expect(page.locator("rtl-overview")).toBeVisible();
  expect(dialogs).toEqual([]);
});
