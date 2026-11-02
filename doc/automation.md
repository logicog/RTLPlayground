# Automation

## Upload

You can automate upload of the firmware via WEB with curl:

1. Authorize with /login endpoint and save cookie:

   ```bash
   curl -c cookies.txt  http://${SWITCH_IP}/login -d pwd=${PASSWORD} -i
   ```

   This will save session cookie in cookies.txt
2. Send the firmware via form:

    ```bash
    curl -b cookies.txt http://${SWITCH_IP}/upload -F "uploadedfile=@${FIRMWARE_FILE_PATH}" -i
    ```

    You can expect that server will close connection, without responding to request.
    Wait for SWITCH_IP to be responding again.

### CLI

`tools/fwupdate.py` performs the same exchange and needs only Python 3 (no
third-party packages). It checks the image locally (512 KiB, header, CRC16)
before uploading, logs in, prints the switch's verdict and waits for the
reboot to finish:

```bash
./tools/fwupdate.py -p "${PASSWORD}" output/rtlplayground_*.bin "${SWITCH_IP}"
```

Keep the password out of the process list by putting it in `RTLP_PASSWORD`
instead of `-p`; when neither is given the tool prompts. `--no-wait` returns
right after the switch accepts the image, and the exit status distinguishes a
bad local image (1), a failed login (2), a rejected upload (3) and a transfer,
reboot or health read failure (4). Add `--health` to print a `/status.json`
summary after the switch has rebooted: per-port link speed, good/error packet
counts and learned MACs, plus totals. It requires waiting, so it cannot be
combined with `--no-wait`; if the health read itself fails, the update is still
reported as successful.

`--check` is read-only: it logs in, prints the running version and the same
`/status.json` health view, and exits without touching flash. It takes the host
itself (`--check 172.23.127.250`), needs no image, and exits 4 if the health
read fails.

## Port status

In similar way to upload, you can fetch the json status of the ports.

1. Get the session cookie as for upload.
2. Hit the `/status.json` with cookie:

    ```bash
    curl -b cookies.txt http://${SWITCH_IP}/status.json
    ```
