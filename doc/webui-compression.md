# Compressing the Web UI Assets

The Web UI (all files under `html/`) is embedded into the flash image and
served over HTTP.  Two build-time steps keep it small: a safe minifier and
gzip compression of the embedded files.

## How it works

```
html/  --(minify.py)-->  output/html_min/  --(fileadder -z)-->  flash image
```

- `tools/minify.py` removes comments (`<!-- -->`, `//`, `/* */`) and
  line-leading whitespace without touching string literals, so the output
  is functionally identical to the input.  Binary files (`.ico`) are
  copied through unchanged.  The raw sources in `html/` stay untouched
  for development; the minified copy in `output/` is a pure build
  artifact.  Comments and indentation are pure overhead for the served
  bytes, so minification alone already reduces the transfer size from
  111,280 to 99,715 bytes, independent of gzip.
- The per-page scripts are consolidated into a single `html/main.js`
  bundle (i18n, shared helpers, navigation and the page scripts).  Every
  page loads only this one script, so the browser fetches it once and
  gzip can compress across all of it.  The page-specific initialisation
  runs only when the matching page elements are present.
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
| raw sources | 111,280 |
| minified (comments and indentation removed) | 99,715 |
| gzip (flash + wire) | 28,200 (25.3 %) |

The minified assets are also what the browser receives when gzip support
is missing or disabled, so the transfer size drops in both stages:
111,280 → 99,715 bytes from minification, then → 28,200 bytes from gzip.

The embedded data block ends at `0x5b977` without compression and at
`0x46e28` with it, freeing ~83 KB of the 512 KB image.

Merging the JS files into one bundle is worth more than the minifier and
gzip combined: served per file, the 17 scripts gzip to 23,302 bytes; as
one `main.js` bundle they gzip to 16,948 bytes, because the gzip
dictionary spans all scripts and every page fetches the bundle only
once.

## Notes

- The files stay well below the `uint16_t` size limit of the file table
  (largest gzip output: 16.9 KB for the merged `main.js`).
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
