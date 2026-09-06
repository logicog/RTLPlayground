export const idleTimeoutOptions: [number, string][] = [
  [5, "After 5 minutes"],
  [15, "After 15 minutes"],
  [30, "After 30 minutes"],
  [60, "After 1 hour"],
  [0, "Do not sign out automatically"],
];
export const idleTimeoutKey = "rtl-idle-minutes";
export const sessionPreferenceEvent = "session-preference-changed";

export function readIdleTimeout(): number {
  try {
    const stored = localStorage.getItem(idleTimeoutKey);
    const minutes = stored === null ? 15 : Number(stored);
    if (idleTimeoutOptions.some(([value]) => value === minutes)) return minutes;
  } catch {
    // Use the default if browser storage is unavailable.
  }
  return 15;
}

export function saveIdleTimeout(minutes: number): void {
  if (!idleTimeoutOptions.some(([value]) => value === minutes)) {
    throw new Error("Choose a supported sign-out interval.");
  }
  localStorage.setItem(idleTimeoutKey, String(minutes));
  window.dispatchEvent(new Event(sessionPreferenceEvent));
}

export function idleTimeoutReached(lastActivity: number, minutes: number, now: number): boolean {
  return minutes > 0 && now - lastActivity >= minutes * 60_000;
}

/** Both sign-in forms must send precisely the media type accepted by the firmware. */
export function loginRequest(password: string): RequestInit {
  return {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: new URLSearchParams({ pwd: password }),
    credentials: "same-origin",
    signal: AbortSignal.timeout(10_000),
  };
}

export async function verifyLoginResponse(response: Response): Promise<void> {
  await response.text();
  if (!response.ok)
    throw new Error(`The switch could not complete sign-in (HTTP ${response.status}).`);
  if (new URL(response.url).pathname !== "/index.html") {
    throw new Error("Incorrect password. Try again.");
  }
}
