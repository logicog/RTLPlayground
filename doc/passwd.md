# Password

## Plaintext passwords

Set the login password in **System → Admin password**, or enter `passwd <password>` in the web or serial console. Sign back in and choose **Save to flash** to keep the change after reboot. The startup configuration stores the password in plaintext.

## Hashed passwords

To store a password hash, supply a pre-generated record in this format. Use a decimal count from 1 to 64 without leading zeros, an eight-digit hexadecimal salt and a 32-digit hexadecimal digest. The switch treats malformed records as plaintext passwords.

```text
passwd blake3 <iterations> <salt> <digest>
```

Run `tools/passwd [blake3] [iterations [salt]]` with `b3sum` installed. It prompts without echo or reads a password from stdin. Use 1–20 password bytes without NUL or ASCII whitespace. The defaults are BLAKE3, 42 iterations and a random salt. A fixed per-device salt makes repeated builds reproducible.

- **Change the running switch.** Paste the record into **System → Console**, enter it at the serial console, or send it as the body of an authenticated `POST /cmd`. Sign in with the new password and choose **Save to flash** while the command is still in history.
- **Provision a startup configuration.** Put the record on its own line and remove any other `passwd` lines. Write the full configuration in the web config editor or upload it using authenticated multipart `POST /config`, then reboot. The last successful `passwd` command wins.
- **Build a firmware image.** Put the record in `config.txt` or the file passed to `make CONFIG=...`. Firmware upgrades preserve the switch's saved startup configuration.

Anyone with the hash can test password guesses offline. HTTP sends the login password in cleartext, and debug builds log raw HTTP input.

Before downgrading to firmware without hash support, set a plaintext password in **System → Admin password**, sign back in and choose **Save to flash**. Confirm the saved configuration contains the plaintext password you intend to use.
