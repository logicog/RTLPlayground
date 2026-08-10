#!/usr/bin/env python3
"""Safe minifier for the WebUI sources (HTML/JS).

Removes comments and line-leading whitespace without touching string
literals, so the output is functionally identical to the input.  Used
by the firmware Makefile before the files are embedded in the flash;
the raw sources in html/ stay untouched for development.

Binary files (.ico) are copied through unchanged.

Usage: minify.py <src> <dst>
"""
import re
import shutil
import sys


def js_min(s):
    out = []
    i = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == '"' or c == "'":
            j = i + 1
            while j < n and s[j] != c:
                if s[j] == '\\':
                    j += 1
                j += 1
            out.append(s[i:j + 1])
            i = j + 1
        elif c == '/' and i + 1 < n and s[i + 1] == '/':
            j = s.find('\n', i)
            i = j if j >= 0 else n
        elif c == '/' and i + 1 < n and s[i + 1] == '*':
            j = s.find('*/', i + 2)
            i = j + 2 if j >= 0 else n
        else:
            out.append(c)
            i += 1
    lines = [l.strip() for l in ''.join(out).split('\n')]
    return '\n'.join(l for l in lines if l) + '\n'


def html_min(s):
    s = re.sub(r'<!--.*?-->', '', s, flags=re.S)
    lines = [l.strip() for l in s.split('\n')]
    return '\n'.join(l for l in lines if l) + '\n'


def main():
    if len(sys.argv) != 3:
        print('usage: minify.py <src> <dst>', file=sys.stderr)
        sys.exit(1)
    src, dst = sys.argv[1], sys.argv[2]
    if src.endswith('.ico'):
        shutil.copyfile(src, dst)
        print('%s: copied %d bytes' % (src.split('/')[-1], len(open(src, 'rb').read())))
        return
    with open(src, 'r', encoding='utf-8', errors='replace') as f:
        s = f.read()
    if src.endswith('.js'):
        out = js_min(s)
    else:
        out = html_min(s)
    with open(dst, 'w', encoding='utf-8') as f:
        f.write(out)
    print('%s: %d -> %d bytes' % (src.split('/')[-1], len(s), len(out)))


if __name__ == '__main__':
    main()
