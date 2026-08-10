# Compressing the Web UI Assets

The Web UI (all files under `html/`) is embedded into the flash image and
served over HTTP.  Two build-time steps keep it small: a safe minifier and
gzip compression of the embedded files.

## How it works

```
html/  --(minify.py)-->  output/html_min/  --(fileadder -z)-->  flash image
```

- `tools/minify.py` removes comments (`<!-- -->`, `//`, `/* */`) and
  line-leading whitespace without touching string or regex literals, so
  the output is functionally identical to the input.  Binary files
  (`.ico`) are copied through unchanged.  The raw sources in `html/` stay
  untouched for development; the minified copy in `output/` is a pure
  build artifact.  Comments and indentation are pure overhead for the
  served bytes, so minification alone already reduces the transfer size
  from 109,814 to 97,730 bytes, independent of gzip.
- The Web UI is a single-page application: `index.html` holds all pages
  as sections (`#/ports`, `#/vlan`, ...) that the sidebar switches
  between via the URL hash.  `login.html` stays separate because it is
  the authentication gate.  Only these two pages plus `main.js`,
  `style.css`, the three SVG port images and the favicon are embedded.
- All JavaScript (i18n, shared helpers, navigation and the per-section
  scripts) lives in one `html/main.js` bundle that the pages load as
  their only script, so the browser fetches it once and gzip can
  compress across all of it.  A section's initialisation runs when the
  section is shown, its polling intervals run only while it is visible,
  and re-entering a section refetches the data.
- `fileadder -z` gzip-compresses every file (zlib, gzip format,
  `Z_BEST_COMPRESSION`) when it generates the file table
  (`html_data.c`/`html_data.h`) and when it embeds the files into the
  final image.  Both invocations run on `output/html_min/`, so the table
  and the image always agree.
- The httpd serves each file with `Content-Encoding: gzip` according to
  the `gzip` flag in its `f_data` entry.  The 8051 only copies compressed
  bytes from flash to the socket; decompression happens in the browser.
- Only the static files are compressed.  The JSON API responses
  (`/information.json`, `/vlanlist`, ...) are generated on the fly and
  stay uncompressed.

## Measured effect

Build on any machine (the Web UI is machine-independent):

| | bytes |
|---|---:|
| raw sources | 109,814 |
| minified (comments and indentation removed) | 97,730 |
| gzip (flash + wire) | 25,302 (23.0 %) |

The minified assets are also what the browser receives when gzip support
is missing or disabled, so the transfer size drops in both stages:
109,814 → 97,730 bytes from minification, then → 25,302 bytes from gzip.

The embedded data block ends at `0x5b977` without compression and at
`0x462d6` with it, freeing ~86 KB of the 512 KB image.

The consolidation helps at every step.  The 17 scripts gzip to 23,302
bytes as separate files but to 17,231 bytes as one `main.js` bundle,
because the gzip dictionary spans all of them.  The single-page layout
then removes the per-page HTML and, more importantly, the repeated
downloads: with the multi-page UI the browser fetched `main.js` on every
navigation; the SPA fetches everything once and section switches are
pure in-page JavaScript (measured on a real device: cold load ~450 ms,
section switch ~60-160 ms, the only network traffic afterwards is the
JSON polling of the visible section).

## Notes

- The files stay well below the `uint16_t` size limit of the file table
  (largest gzip output: 17.2 KB for the merged `main.js`).
- `fileadder` terminates the embedded files with a NUL directly after the
  content (previously at `data_read + 1`), so the `strlen()`-based size
  computation no longer depends on uninitialised buffer content.
- `version.h` is no longer `.PHONY`; every build previously rewrote it
  while a parallel build (`make -j`) could read a half-written copy.

## Verification

Decompress what the firmware embeds and compare it with the minified
sources:

```python
import gzip, re
h = open('html_data.h').read(); c = open('html_data.c').read()
starts = dict((k, int(v, 16)) for k, v in
              re.findall(r'#define FDATA_START_(\w+) 0x([0-9a-f]+)', h))
sizes = dict((k, int(v)) for k, v in
             re.findall(r'#define FDATA_SIZE_(\w+) (\d+)', h))
entries = [(p, sizes[z], starts[s]) for p, s, z in
           re.findall(r'\{"/([^"]+)", FDATA_START_(\w+), '
                      r'FDATA_SIZE_(\w+), \w+, 1\}', c)]
d = open('output/rtlplayground.bin', 'rb').read()
for name, size, start in entries:
    assert gzip.decompress(d[start:start+size]) == \
        open('output/html_min/' + name, 'rb').read()
```

`tools/httpd_sim` serves the raw files from `html/` and does not exercise
the gzip path; a browser session against a flashed switch (or `curl
--compressed`) is the final check.
