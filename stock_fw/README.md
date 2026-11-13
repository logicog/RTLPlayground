# Stock firmware (local only)

This directory is a drop zone for the **original (vendor) firmware** of a switch. Files
placed here are treated as read-only input for extracting tasks — for example
extracting the POE controller firmware that the playground's `poe/` support is built
against.

Nothing in this directory is committed to the repository. The bundled `.gitignore`
ignores everything except this README and itself, so images dropped here stay local.
Do not commit firmware images with `git add -f`.

## What to put here

Anything that represents the firmware as shipped by the vendor:

- the update image downloaded from the vendor web-interface (`.bin`, `.img`, ...)
- a full flash dump taken with a SOIC clamp / programmer
- bootloader or partition dumps, if extracted separately

Keep the file unmodified, and name it after the device and version so the origin stays
traceable, e.g. `GS110TP_v1.0.0.8.bin` or `RTL8373_ref_20240115_flash.bin`.

## Typical workflow

1. Copy or download the stock firmware into this directory.
2. Analyse it (strings, entropy, binwalk, Ghidra, ...) to locate the parts of interest.
3. Put the results intended for the build somewhere else: generated sources belong in
   `poe/`, build products in `output/` (also ignored). Only this folder is for the
   untouched vendor input.

Note that these images are vendor property; they are neither part of RTLPlayground nor
covered by its license, and they must not be redistributed through this repository.
