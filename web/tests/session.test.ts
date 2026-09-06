import { describe, expect, it } from "vitest";
import { idleTimeoutReached, loginRequest, verifyLoginResponse } from "../src/domain/session";

describe("firmware login compatibility", () => {
  it("sends the exact media type and safely encodes password characters", async () => {
    const request = new Request("http://switch/login", loginRequest("a+b &=c"));
    expect(request.headers.get("content-type")).toBe("application/x-www-form-urlencoded");
    expect(new URLSearchParams(await request.text()).get("pwd")).toBe("a+b &=c");
  });

  it("rejects a successful HTTP response redirected back to the password form", async () => {
    const response = new Response("login form");
    Object.defineProperty(response, "url", { value: "http://switch/login.html" });
    await expect(verifyLoginResponse(response)).rejects.toThrow("Incorrect password");
  });

  it("accepts the authenticated destination and consumes its body", async () => {
    const response = new Response("authenticated page");
    Object.defineProperty(response, "url", { value: "http://switch/index.html" });
    await expect(verifyLoginResponse(response)).resolves.toBeUndefined();
    expect(response.bodyUsed).toBe(true);
  });
});

describe("automatic sign-out", () => {
  it("expires at the inactivity boundary and respects a new activity timestamp", () => {
    expect(idleTimeoutReached(1000, 5, 300999)).toBe(false);
    expect(idleTimeoutReached(1000, 5, 301000)).toBe(true);
    expect(idleTimeoutReached(300000, 5, 301000)).toBe(false);
  });

  it("does not automatically expire when disabled", () => {
    expect(idleTimeoutReached(0, 0, 86_400_000)).toBe(false);
  });
});
