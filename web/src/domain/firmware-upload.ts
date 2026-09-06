import { api } from "../api";

/** XHR is used only here because fetch has no widely supported upload progress event. */
export function uploadFirmware(file: File, onProgress: (percent: number) => void): Promise<void> {
  if (import.meta.env.DEV) {
    return Promise.reject(new Error("Firmware upload is disabled in the development preview."));
  }

  return api.exclusive(
    () =>
      new Promise<void>((resolve, reject) => {
        const request = new XMLHttpRequest();
        request.open("POST", "/upload");
        request.timeout = 180_000;

        request.upload.onprogress = (event) => {
          if (event.lengthComputable)
            onProgress(Math.min(99, Math.round((event.loaded / event.total) * 100)));
        };
        request.onload = () => {
          if (request.status >= 200 && request.status < 300) resolve();
          else
            reject(
              new Error(`Upload rejected (${request.status}). Check device state before retrying.`),
            );
        };
        request.onerror = request.ontimeout = () => {
          reject(
            new Error(
              "Connection interrupted during upload. The result is unknown. Do not retry or power-cycle; check the device first.",
            ),
          );
        };

        const form = new FormData();
        form.append("uploadedfile", file);
        request.send(form);
      }),
  );
}
