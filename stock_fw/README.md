# Vendor firmware (local only)

Drop zone for firmware that belongs to a chip vendor and that the build needs
but the repository must not carry. Everything here except this README is
ignored by git (see `.gitignore`); do not add files with `git add -f`.

## TPS23880 PoE controller

The TI TPS23880 runs its port state machines from SRAM, which the switch
downloads after every power-on. The image comes from TI's
"TPS23880 PoE MSP430 Reference Code for Basic Applications", SLVC709:

1. Download `slvc709c.zip` from <https://www.ti.com/tool/download/SLVC709>.
2. Put it here unchanged: `stock_fw/slvc709c.zip`.
3. Build a machine with `MACHINE_POE_CHIP POE_TPS23880` as usual. The build runs
   `tools/poe/extract_tps23880.py`, which writes `poe/firmware/tps23880.bin`
   (also ignored) and places it in the image at `POE_IMAGE_LOCATION`.

Without the zip the build still succeeds, with a warning, and the switch
reports at boot that there is no PoE image. See `doc/poe-tps23880.md`.

These files are the vendor's property and are neither part of RTLPlayground
nor covered by its license.
