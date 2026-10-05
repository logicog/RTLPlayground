# Firmware Updates

The web UI uploads a complete image (`output/<machine>/rtlplayground<b>_<machine>.bin`,
512 KiB) and the switch applies it on the next reset. How the uploaded image is
held until then depends on the flash size:

| flash | uploaded image | cost |
| ----- | -------------- | ---- |
| 1 MiB or more | written above the running image, as before | the whole image, twice the flash |
| 512 KiB | staged into the flash the running image leaves free | only the sectors of the image that hold something |

Everything else is the same in both cases: the browser checks size, LJMP magic
and CRC16 before uploading, the switch accumulates the same CRC16 while it
receives and answers `OK: checksum verified, rebooting`, then resets, lets the
boot code verify the staged image again and copies it into place.

## The staging pool of a 512 KiB device

The pool is flash that the running image does not occupy:

- `0x28000-0x3ffff`, between the code banks and the web UI: no build can use it,
  the banks end there and the UI starts at `0x40000`.
- From the end of the web UI up to `0x6efff`, the space the UI grows into
  between releases. The pool starts above whatever the running image serves, so
  a staged sector never lands on data the switch is serving.

Sectors are placed from the top of each area downwards. For a build of today's
size that is 57 sectors (228 KiB) of pool for 47 sectors (188 KiB) of content,
about 40 KiB of headroom; the pool shrinks as the web UI grows.

## Refusals

The upload is refused when a staged sector would be overwritten by the copy back
or when the free flash runs out. Nothing is written to the running image in that
case; the web UI shows the reason and the same sentence goes to the console. A
device at that limit is updated over the serial programmer, as in the README.

The switch checks the image twice: while it receives, the CRC16 of the complete
upload - the same `0xb001` the browser checks - and at boot the CRC of the part
the copy writes, which is what the staged flash has to still match.

## Interrupted updates

The copy ends with the write of the first flash sector (prefetch header and
reset vector), so a reset in the middle leaves a bootable switch that resumes
where it stopped instead of a half-written image. Staged image, progress and the
length of the pool are kept in one flash sector (`0x7e000`) that the copy does
not overwrite, and cleared when the image is complete.

A device must already run firmware that knows about the pool, so 512 KiB boards
need the serial programmer once, as with any first installation.

The copy leaves the live configuration, the two trailer bytes the build puts on
the end of the image and everything above them as they are, so reading the flash
back and comparing it with the `.bin` has to mask those, as for any update.
