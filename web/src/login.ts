import { loginRequest, verifyLoginResponse } from "./domain/session";

const form = document.querySelector<HTMLFormElement>("#login")!;
const errorMessage = document.querySelector<HTMLElement>("#login-error")!;
const button = form.querySelector<HTMLButtonElement>("button")!;

async function signIn(event: SubmitEvent): Promise<void> {
  event.preventDefault();
  if (button.disabled) return;
  button.disabled = true;
  button.textContent = "Signing in…";
  errorMessage.hidden = true;

  try {
    const password = String(new FormData(form).get("pwd"));
    const response = await fetch("/login", loginRequest(password));
    await verifyLoginResponse(response);
    location.replace("/index.html");
  } catch (error) {
    errorMessage.textContent =
      error instanceof Error ? error.message : "Could not connect to the switch.";
    errorMessage.hidden = false;
  } finally {
    button.disabled = false;
    button.textContent = "Sign in ↗";
  }
}

form.addEventListener("submit", signIn);

export {};
