# Firmware Updates

The web UI uploads a complete image (`output/<machine>/rtlplayground-*.bin`, 512
KiB) and the switch applies it on the next reset. How the uploaded image is
held until then depends on the flash size:

| flash | uploaded image | cost |
| ----- | -------------- | ---- |
| 1 MiB or more | written above the running image, as before | the whole image, twice the flash |
| 512 KiB | staged into the flash the running image leaves free | only the sectors of the image that hold something |

Everything else is the same in both cases: the browser checks size, LJMP magic
and CRC16 before uploading, the switch accumulates the same CRC16 while it
receives and answers `OK: checksum verified, rebooting`, then resets, lets the
boot code verify the staged image again and copies it into place.

Browsers do not agree on what to call the uploaded part: Chrome sends
`application/macbinary` for a `.bin` file on macOS, Firefox
`application/octet-stream`, and anything else is possible. The part's headers are
therefore located by position, their `Content-Type` is never read.

## The staging pool of a 512 KiB device

The pool is flash that the running image does not occupy:

- `0x28000-0x3ffff`, between the code banks and the web UI: no build can use it,
  the banks end there and the UI starts at `0x40000`.
- From the end of the web UI up to `0x6efff`, the space the UI grows into
  between releases. The pool starts above whatever the running image serves, so
  a staged sector never lands on data the switch is serving.
- `0x71000-0x7dfff`, between the live configuration and the update record, which
  is the padding a `512 KiB` image has at its end. It lies above every address
  the copy writes, so a slot there never covers a sector of the update itself.

Sectors are placed from the top of each area downwards. For a build of today's
size that is 70 sectors (280 KiB) of pool for 46 sectors (184 KiB) of content,
about 96 KiB of headroom; the pool shrinks as the web UI grows.

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

### Where a reset can land

The copy writes the image from the top down and erases each sector just before
writing it, so what a reset costs depends on where it interrupts. A 4 KiB sector
costs about 2.2 s here: `flash_write_bytes()` issues a write enable and a page
program for every 4 bytes, each about 2.2 ms, and an apply writes every sector of
the image region, around 50 of them with image content and the rest with zeros.

| position in the copy | what is being written | if a reset lands there |
| -------------------- | --------------------- | ---------------------- |
| first, about 100 s | the banked code (`0x04000-0x27fff`) and the web UI at `0x40000` | recovers: the prefetched area is still the running image, so its reset vector, `main()` and the update code run and resume the copy |
| last but one, about 7 s | sectors 1-3, the rest of the prefetched 16 KiB: the code all banks share, the ISRs, `main()` and the update code | may not recover: what runs after the reset is part old and part new, so the resume need not start. Serial programmer |
| last, about 0.3 s | sector 0, erased and then written: the prefetch size at `0x0000` and the reset vector at `0x0002` | does not recover: without them there is nothing to fetch. Serial programmer |

The switch cannot repair the last two itself, so the window that has to be
avoided is one sector erase plus one 512-byte block, about 0.3 s of the ~230 s
an apply takes on a 512 KiB board (0.1 %) and about a thousandth of the ~6
minutes an update takes including the upload.

In the first case the switch runs new banked code against old prefetched code,
which assumes the two images agree on their layout. That is certain for two
images of one build and not guaranteed between unrelated releases.

The pool sectors and the state sector `0x7e000` are safe to lose. The pool is
erased and zeroed by the copy's last pass, and a state record torn by a reset
fails its magic, which leaves the running image to boot as before and loses only
the staged update.

The copy leaves the live configuration, the two trailer bytes the build puts on
the end of the image and everything above them as they are, so reading the flash
back and comparing it with the `.bin` has to mask those, as for any update.
