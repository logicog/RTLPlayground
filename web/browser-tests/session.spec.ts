import { expect, test } from "@playwright/test";

test.beforeEach(async ({ page }) => {
  await page.route("**/*", (route) => {
    if (new URL(route.request().url()).origin !== "http://127.0.0.1:5175") return route.abort();
    return route.continue();
  });
  await page.goto("/");
  await expect(page.locator(".network-choice")).toHaveCount(3);
});

test("renewing an expired session keeps drafts and accepts the firmware-compatible login", async ({
  page,
}) => {
  await page.getByRole("button", { name: "Configure port", exact: true }).click();
  const name = page.locator('.port-inspector input[name="name"]');
  await name.fill("Keep-this-draft");
  await page.evaluate(() => window.dispatchEvent(new Event("session-expired")));
  const dialog = page.getByRole("dialog");
  await dialog.getByLabel("Password", { exact: true }).fill("wrong");
  await dialog.getByRole("button", { name: "Sign in", exact: true }).click();
  await expect(dialog.getByRole("alert")).toContainText("Incorrect password");
  await dialog.getByLabel("Password", { exact: true }).fill("preview");
  await dialog.getByRole("button", { name: "Sign in", exact: true }).click();
  await expect(dialog).toHaveCount(0);
  await expect(name).toHaveValue("Keep-this-draft");
});

test("the session preference is saved locally and enforces idle sign-out", async ({
  page,
  context,
}) => {
  await page.evaluate(() => {
    location.hash = "system";
  });
  await page.getByRole("combobox", { name: "Automatic sign-out", exact: true }).selectOption("5");
  await page.getByRole("button", { name: "Save session preference", exact: true }).click();
  await expect(
    page.getByText("Automatic sign-out preference saved in this browser."),
  ).toBeVisible();
  expect(await page.evaluate(() => localStorage.getItem("rtl-idle-minutes"))).toBe("5");
  await expect(page.getByRole("dialog")).toHaveCount(0);
  await context.addCookies([
    { name: "session", value: "preview-session", url: "http://127.0.0.1:5175" },
  ]);
  await page.clock.install();
  await page.clock.fastForward(301000);
  await expect(page.getByRole("dialog")).toContainText("Signed out after inactivity.");
  expect((await context.cookies()).some((cookie) => cookie.name === "session")).toBe(false);
});

test("disabling idle sign-out keeps a paused panel authenticated", async ({ page }) => {
  await page.evaluate(() => {
    location.hash = "system";
  });
  await page.getByRole("combobox", { name: "Automatic sign-out", exact: true }).selectOption("0");
  await page.getByRole("button", { name: "Save session preference", exact: true }).click();
  await page.getByRole("button", { name: "Live · 5s", exact: true }).click();
  await page.clock.install();
  await page.clock.fastForward(3_600_000);
  await expect(page.getByRole("dialog")).toHaveCount(0);
  await expect(page.getByRole("button", { name: "Live paused", exact: true })).toBeVisible();
});
