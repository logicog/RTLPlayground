#!/usr/bin/env python3
"""Local, single-file RTLPlayground translation editor (Python standard library).

Usage:
    python3 rtl_translation_editor.py /path/to/RTLPlayground
    python3 rtl_translation_editor.py /path/to/RTLPlayground/html/app.js
    python3 rtl_translation_editor.py --port 8765 --open

Reads translations from html/app.js and html/login.html, and language display
names from html/index.html. Edit one row at a time in a browser; changes are
autosaved in the respective source file. The first edit backs up each file.

New languages must use two-letter lower-case *language* codes, because
RTLPlayground detects a browser language with navigator.language.slice(0, 2).
Run against a checkout that contains all three files in its html/ directory.
"""

import argparse
from dataclasses import dataclass
import html as html_module
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import stat
import tempfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Lock
import webbrowser


IDENT = re.compile(r"[a-zA-Z_$][\w$]*")
LANG_CODE = re.compile(r"[a-z]{2}\Z")
LOCK = Lock()
TOKEN = secrets.token_urlsafe(32)


class ParseError(ValueError):
    pass


def skip_trivia(src, p, stop=None):
    stop = len(src) if stop is None else stop
    while p < stop:
        if src[p].isspace():
            p += 1
        elif src.startswith('//', p):
            j = src.find('\n', p + 2, stop)
            p = stop if j < 0 else j + 1
        elif src.startswith('/*', p):
            j = src.find('*/', p + 2, stop)
            if j < 0:
                raise ParseError('Unterminated JavaScript comment')
            p = j + 2
        else:
            break
    return p


def parse_string(src, pos):
    """Decode a JavaScript quoted string and return (value, next_offset)."""
    if pos >= len(src) or src[pos] not in "\"'":
        raise ParseError('Expected JavaScript string')
    quote = src[pos]
    i = pos + 1
    value = []
    common = {'n': '\n', 'r': '\r', 't': '\t', 'b': '\b', 'f': '\f', 'v': '\v', '0': '\0'}
    while i < len(src):
        c = src[i]
        if c == quote:
            s = ''.join(value)
            # Unescape UTF-16 surrogate pairs, if the input used \\u escapes.
            try:
                s = s.encode('utf-16', 'surrogatepass').decode('utf-16')
            except UnicodeError:
                pass
            return s, i + 1
        if c in '\r\n':
            raise ParseError('Unexpected newline in JavaScript string')
        if c != '\\':
            value.append(c)
            i += 1
            continue
        i += 1
        if i >= len(src):
            raise ParseError('Unterminated JavaScript escape')
        c = src[i]
        if c == '\r':
            i += 1
            if i < len(src) and src[i] == '\n':
                i += 1
            continue
        if c == '\n':
            i += 1
            continue
        if c == 'x' and re.fullmatch(r'[0-9a-fA-F]{2}', src[i+1:i+3]):
            value.append(chr(int(src[i+1:i+3], 16)))
            i += 3
        elif c == 'u' and re.fullmatch(r'[0-9a-fA-F]{4}', src[i+1:i+5]):
            value.append(chr(int(src[i+1:i+5], 16)))
            i += 5
        elif c == 'u' and src[i+1:i+2] == '{':
            close = src.find('}', i + 2, i + 10)
            if close < 0 or not re.fullmatch(r'[0-9a-fA-F]{1,6}', src[i+2:close]):
                raise ParseError('Invalid Unicode escape')
            value.append(chr(int(src[i+2:close], 16)))
            i = close + 1
        else:
            value.append(common.get(c, c))
            i += 1
    raise ParseError('Unterminated JavaScript string')


def find_closing_brace(src, opening):
    if src[opening] != '{':
        raise ParseError('Expected opening brace')
    depth = 0
    i = opening
    while i < len(src):
        if src.startswith('//', i):
            j = src.find('\n', i+2)
            i = len(src) if j < 0 else j+1
            continue
        if src.startswith('/*', i):
            j = src.find('*/', i+2)
            if j < 0:
                raise ParseError('Unterminated comment')
            i = j+2
            continue
        if src[i] in "\"'":
            _, i = parse_string(src, i)
            continue
        if src[i] == '{':
            depth += 1
        if src[i] == '}':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ParseError('Unclosed JavaScript object')


@dataclass
class Property:
    key: str
    start: int
    value_start: int
    value_end: int
    comma_end: int | None
    is_object: bool
    value: str | None


def properties(src, opening, closing):
    """Read properties of a simple JS object with string/object values."""
    out = []
    i = opening + 1
    while True:
        i = skip_trivia(src, i, closing)
        if i == closing:
            return out
        start = i
        if src[i] in "\"'":
            key, i = parse_string(src, i)
        else:
            m = IDENT.match(src, i)
            if not m:
                raise ParseError(f'Expected a property name at offset {i}')
            key, i = m.group(), m.end()
        i = skip_trivia(src, i, closing)
        if i >= closing or src[i] != ':':
            raise ParseError(f'Expected colon after {key}')
        i = skip_trivia(src, i + 1, closing)
        value_start = i
        if i < closing and src[i] in "\"'":
            val, i = parse_string(src, i)
            object_value = False
        elif i < closing and src[i] == '{':
            i = find_closing_brace(src, i) + 1
            val = None
            object_value = True
        else:
            raise ParseError(f'Unsupported value for {key} (only strings and objects are supported)')
        value_end = i
        i = skip_trivia(src, i, closing)
        comma_end = None
        if i < closing and src[i] == ',':
            i += 1
            comma_end = i
        elif i != closing:
            raise ParseError(f'Expected comma after {key}')
        out.append(Property(key, start, value_start, value_end, comma_end, object_value, val))


def locate(src):
    """Return {code: (object_open, object_close)} for LANG and LANG.xx objects."""
    m = re.search(r'\b(?:var|let|const)\s+LANG\s*=\s*\{', src)
    if not m:
        raise ParseError('Could not find `var LANG={...}` in app.js')
    root_open = src.rfind('{', m.start(), m.end())
    root_close = find_closing_brace(src, root_open)
    root = properties(src, root_open, root_close)
    langs = {}
    for p in root:
        if not p.is_object:
            raise ParseError('Unexpected non-object value in LANG')
        if p.key in langs:
            raise ParseError(f'Duplicate language {p.key}')
        langs[p.key] = (p.value_start, p.value_end - 1)
    # Include separately added translations (e.g. `LANG.pt = {...}`), too.
    for mm in re.finditer(r'\bLANG\.([a-zA-Z_$][\w$]*)\s*=\s*\{', src[root_close+1:]):
        code = mm.group(1)
        op = root_close + 1 + mm.end() - 1
        cl = find_closing_brace(src, op)
        if code in langs:
            raise ParseError(f'Duplicate language definition: {code}')
        langs[code] = (op, cl)
    if 'en' not in langs:
        raise ParseError('English reference dictionary (LANG.en) not found')
    return root_open, root_close, root, langs


def dictionary(src, bounds):
    op, cl = bounds
    entries = properties(src, op, cl)
    if any(p.is_object for p in entries):
        raise ParseError('Expected translation strings, found a nested object')
    # JS resolves duplicate keys by using the last one. Reject instead to avoid ambiguity.
    keys = [p.key for p in entries]
    if len(set(keys)) != len(keys):
        raise ParseError('Duplicate translation keys detected')
    return entries


def snapshot(src):
    _, _, _, langs = locate(src)
    english = dictionary(src, langs['en'])
    return {
        'english': [{'key': p.key, 'value': p.value} for p in english],
        'languages': [code for code in langs if code != 'en'],
        'translations': {
            code: {p.key: p.value for p in dictionary(src, bounds)}
            for code, bounds in langs.items() if code != 'en'
        },
    }


def read_js(path):
    # Preserve CRLF and an optional UTF-8 BOM; Path.read_text normalizes newlines.
    return path.read_bytes().decode('utf-8')


def js_string(value):
    return json.dumps(value, ensure_ascii=False).replace('\u2028', r'\u2028').replace('\u2029', r'\u2029')


def line_endings(src, text):
    return text.replace('\n', '\r\n') if '\r\n' in src else text


@dataclass
class StringSpan:
    value: str
    start: int
    end: int


def login_array(src, opening):
    """Parse a JS array of quoted strings and retain exact editable spans."""
    if src[opening] != '[':
        raise ParseError('Expected a login translation array')
    strings = []
    pos = opening + 1
    while pos < len(src):
        pos = skip_trivia(src, pos)
        if src[pos:pos+1] == ']':
            return strings, pos + 1
        start = pos
        value, pos = parse_string(src, pos)
        strings.append(StringSpan(value, start, pos))
        pos = skip_trivia(src, pos)
        if src[pos:pos+1] == ',':
            pos += 1
        elif src[pos:pos+1] == ']':
            return strings, pos + 1
        else:
            raise ParseError(f'Expected a comma or ] in login dictionary at {pos}')
    raise ParseError('Unterminated login translation array')


def login_dictionaries(src):
    """Read var L={en:[...],...} from login.html, not HTML text defaults."""
    match = re.search(r'\bvar\s+L\s*=\s*\{', src)
    if not match:
        raise ParseError('Could not find `var L={...}` in login.html')
    opening = match.end() - 1
    closing = find_closing_brace(src, opening)
    entries = {}
    pos = opening + 1
    while pos < closing:
        pos = skip_trivia(src, pos, closing)
        if pos == closing:
            break
        if src[pos] in "\"'":
            code, pos = parse_string(src, pos)
        else:
            m = IDENT.match(src, pos)
            if not m:
                raise ParseError('Unexpected entry in login translation dictionary')
            code, pos = m.group(), m.end()
        pos = skip_trivia(src, pos, closing)
        if src[pos:pos+1] != ':':
            raise ParseError(f'Expected colon after login language {code}')
        pos = skip_trivia(src, pos + 1, closing)
        strings, end = login_array(src, pos)
        if code in entries:
            raise ParseError(f'Duplicate login language {code}')
        entries[code] = (pos, end - 1, strings)
        pos = skip_trivia(src, end, closing)
        if pos < closing:
            if src[pos] != ',':
                raise ParseError(f'Expected comma after login language {code}')
            pos += 1
    if 'en' not in entries:
        raise ParseError('English login translation array (L.en) not found')
    count = len(entries['en'][2])
    if any(len(strings) != count for _, _, strings in entries.values()):
        raise ParseError('All login language arrays must match L.en in length')
    return closing, entries


LOGIN_KEYS = ('login_heading', 'login_subtitle', 'password_label',
              'sign_in_button', 'wrong_password_message')
LOGIN_CONTEXT = ('#hn · Login heading', '#sub · Management interface subtitle',
                 '#lp · Password field label', '#lb · Submit button',
                 '#err · Invalid password message')


def login_key(index):
    return LOGIN_KEYS[index] if index < len(LOGIN_KEYS) else f'login_string_{index+1}'


def login_snapshot(src):
    _, dictionaries = login_dictionaries(src)
    english = dictionaries['en'][2]
    return {
        'loginEnglish': [
            {'key': login_key(i), 'value': string.value,
             'context': LOGIN_CONTEXT[i] if i < len(LOGIN_CONTEXT) else f'Array entry {i+1}'}
            for i, string in enumerate(english)
        ],
        'loginTranslations': {
            code: {login_key(i): span.value for i, span in enumerate(strings)}
            for code, (_, _, strings) in dictionaries.items() if code != 'en'
        },
    }


def ensure_login_fallback(src):
    """An unfinished array must display English, not an empty login label."""
    original = re.compile(r'\bvar\s+s\s*=\s*L\[l\]\s*\|\|\s*L\.en\s*;')
    replacement = 'var s=(L[l]||L.en).map(function(v,i){return v||L.en[i];});'
    if replacement in src:
        return src
    updated, count = original.subn(lambda _: replacement, src, count=1)
    if count != 1:
        raise ParseError('Cannot locate login language selection for empty-string fallback')
    return updated


def add_login_language(src, code):
    closing, entries = login_dictionaries(src)
    if code in entries:
        # Keep existing work, but make clearing any field safe as well.
        return ensure_login_fallback(src)
    english = entries['en'][2]
    item = code + ':[' + ','.join('""' for _ in english) + ']'
    if entries:
        # Last array ends before its optional comma.
        last = list(entries.values())[-1]
        after = skip_trivia(src, last[1] + 1, closing)
        if src[after:after+1] == ',':
            insert_at, insertion = after + 1, '\n    ' + item + ','
        else:
            insert_at, insertion = last[1] + 1, ',\n    ' + item
    else:
        insert_at, insertion = closing, '\n    ' + item + '\n'
    return ensure_login_fallback(src[:insert_at] + line_endings(src, insertion) + src[insert_at:])


def set_login_translation(src, code, key, value):
    if not isinstance(value, str) or len(value) > 20000:
        raise ValueError('Translation must be a string of at most 20,000 characters')
    if not LANG_CODE.fullmatch(code or '') or code == 'en':
        raise ValueError('Select a target language')
    src = add_login_language(src, code)
    _, entries = login_dictionaries(src)
    keys = [login_key(i) for i in range(len(entries['en'][2]))]
    if key not in keys:
        raise ValueError(f'Unknown login translation key: {key}')
    span = entries[code][2][keys.index(key)]
    if span.value == value:
        return src
    return src[:span.start] + js_string(value) + src[span.end:]


# Restrict edits to the language dropdown, never the other index.html selects.
SELECT_LANG = re.compile(r'<select\b[^>]*\bid\s*=\s*["\']langSel["\'][^>]*>', re.I)
OPTION_LANG = re.compile(
    r'(?P<open><option\b[^>]*\bvalue\s*=\s*["\'](?P<code>[a-z]{2})["\'][^>]*>)'
    r'(?P<label>.*?)</option\s*>', re.I | re.S)


def index_language_options(src):
    match = SELECT_LANG.search(src)
    if not match:
        raise ParseError('Could not find language selector (#langSel) in index.html')
    closing = re.search(r'</select\s*>', src[match.end():], re.I)
    if not closing:
        raise ParseError('Unclosed language selector in index.html')
    end = match.end() + closing.start()
    options = {}
    for opt in OPTION_LANG.finditer(src, match.end(), end):
        code = opt.group('code').lower()
        if code in options:
            raise ParseError(f'Duplicate index.html language option: {code}')
        options[code] = opt
    return match.end(), end, options


def set_language_name(src, code, name):
    if not isinstance(code, str) or not LANG_CODE.fullmatch(code):
        raise ValueError('Language code must consist of two lowercase letters')
    if not isinstance(name, str) or not name.strip() or len(name) > 80 or '\n' in name or '\r' in name:
        raise ValueError('Language display name must be 1–80 characters on one line')
    name = name.strip()
    start, end, options = index_language_options(src)
    label = html_module.escape(name, quote=False)
    if code in options:
        span = options[code].span('label')
        return src[:span[0]] + label + src[span[1]:]
    option = f'<option value="{code}">{label}</option>'
    # Alphabetical by language code, retaining all existing selector markup.
    following = next((opt for c, opt in options.items() if c > code), None)
    if following:
        insert_at = src.rfind('\n', start, following.start()) + 1
        indentation = src[insert_at:following.start()]
        insertion = indentation + option + ('\r\n' if '\r\n' in src else '\n')
    elif options:
        last = list(options.values())[-1]
        insert_at = last.end()
        indent_match = re.search(r'(?m)^([ \t]*)<option\b', src[start:end])
        indentation = indent_match.group(1) if indent_match else '  '
        insertion = ('\r\n' if '\r\n' in src else '\n') + indentation + option
    else:
        insert_at = end
        insertion = ('\r\n' if '\r\n' in src else '\n') + '  ' + option
    return src[:insert_at] + insertion + src[insert_at:]


def project_snapshot(paths):
    state = snapshot(read_js(paths['app']))
    state.update(login_snapshot(read_js(paths['login'])))
    _, _, options = index_language_options(read_js(paths['index']))
    state['languageNames'] = {
        code: html_module.unescape(re.sub(r'<[^>]*>', '', opt.group('label')).strip())
        for code, opt in options.items()
    }
    return state


def add_language(src, code):
    if not LANG_CODE.fullmatch(code) or code == 'en':
        raise ValueError('Use a two-letter lowercase language code other than en, e.g. pt')
    _, root_close, roots, langs = locate(src)
    if code in langs:
        raise ValueError(f'Language {code} already exists')
    new = f'{code}:{{}}'
    if roots:
        last = roots[-1]
        if last.comma_end is None:
            insert_at = last.value_end
            insert = ',\n' + new
        else:
            insert_at = last.comma_end
            insert = '\n' + new + ','
    else:
        insert_at = root_close
        insert = '\n' + new + '\n'
    return src[:insert_at] + line_endings(src, insert) + src[insert_at:]


def set_translation(src, code, key, value):
    if not isinstance(value, str) or len(value) > 20000:
        raise ValueError('Translation must be a string of at most 20,000 characters')
    _, _, _, langs = locate(src)
    if code == 'en' or code not in langs:
        raise ValueError('Select an existing target language')
    enkeys = {p.key for p in dictionary(src, langs['en'])}
    if key not in enkeys:
        raise ValueError(f'Unknown English key: {key}')
    op, cl = langs[code]
    entries = dictionary(src, (op, cl))
    previous = next((p for p in entries if p.key == key), None)
    if previous:
        if previous.value == value:
            return src
        return src[:previous.value_start] + js_string(value) + src[previous.value_end:]
    if not value:  # Missing target entries automatically fall back to English in app.js.
        return src
    item = f'{key}:{js_string(value)}'
    if entries:
        last = entries[-1]
        if last.comma_end is None:
            insert_at, insert = last.value_end, ',\n' + item
        else:
            insert_at, insert = last.comma_end, '\n' + item + ','
    else:
        insert_at, insert = op + 1, '\n' + item + '\n'
    return src[:insert_at] + line_endings(src, insert) + src[insert_at:]


def atomic_save(path, original, updated):
    if original == updated:
        return False
    backup = path.with_name(path.name + '.translation-editor.bak')
    if not backup.exists():
        # Keep the very first original intact even after many autosaves.
        shutil.copy2(path, backup)
    old_mode = stat.S_IMODE(path.stat().st_mode)
    fd, temp = tempfile.mkstemp(prefix='.translation-editor-', dir=path.parent)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8', newline='') as file:
            file.write(updated)
            file.flush()
            os.fsync(file.fileno())
        os.chmod(temp, old_mode)
        os.replace(temp, path)
    finally:
        if os.path.exists(temp):
            os.unlink(temp)
    return True


HTML = r'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RTLPlayground translation editor</title>
<style>
  :root { color-scheme: light dark; --bg:#f6f7f9; --panel:#fff; --ink:#182536; --muted:#5a6778; --line:#d8dfe7; --accent:#175ac2; --wash:#eef5ff; --error:#ad342e; --ok:#19724b; }
  @media(prefers-color-scheme:dark) { :root {--bg:#111820;--panel:#1b2530;--ink:#edf3fa;--muted:#aab7c8;--line:#374453;--accent:#8db7ff;--wash:#26374c;--error:#ff9991;--ok:#74d9af;} }
  * {box-sizing:border-box}
  body {font:14px/1.5 system-ui,-apple-system,Segoe UI,sans-serif; background:var(--bg);color:var(--ink);margin:0}
  header {background:var(--panel);border-bottom:1px solid var(--line);padding:20px max(24px,calc((100vw - 1500px)/2));}
  h1 {font-size:22px;font-weight:700;margin:0 0 5px}
  .sub {color:var(--muted);margin:0}
  main {max-width:1500px;margin:0 auto;padding:20px 24px 50px}
  .toolbar {background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:15px;display:flex;gap:12px;align-items:end;flex-wrap:wrap;margin-bottom:15px}
  .control {display:flex;flex-direction:column;gap:4px}
  label {font-size:12px;font-weight:700;color:var(--muted)}
  input,select,textarea,button {font:inherit}
  input,select,textarea {background:var(--panel);color:var(--ink);border:1px solid var(--line);border-radius:6px;padding:8px 10px;outline-offset:2px}
  input:focus,textarea:focus,select:focus {outline:2px solid var(--accent);border-color:transparent}
  button {background:var(--accent);color:var(--panel);border:0;border-radius:6px;padding:9px 13px;cursor:pointer;font-weight:600}
  @media(prefers-color-scheme:dark) {button {color:#132335}}
  button.secondary {background:var(--wash);color:var(--ink);border:1px solid var(--line)}
  button:disabled {opacity:.5;cursor:default}
  #search {min-width:230px;width:min(310px,75vw)}
  #newCode {width:85px}
  #newName {width:210px}
  .name-toolbar {align-items:center}
  .name-toolbar .sub {flex:1;min-width:230px}
  h2 {font-size:17px;margin:22px 0 8px}
  h2 code {font-size:13px}
  .grow {flex:1}
  .line {border-left:1px solid var(--line);padding-left:15px;display:flex;gap:8px;align-items:end}
  .stats {display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;margin:0 0 10px}
  #progress {font-weight:650}
  #message {color:var(--muted);font-size:13px}
  #message.error {color:var(--error)}
  .table-wrap {overflow:auto;border:1px solid var(--line);border-radius:10px;background:var(--panel)}
  table {border-collapse:collapse;width:100%;table-layout:fixed}
  thead th {position:sticky;top:0;z-index:2;background:var(--wash);border-bottom:1px solid var(--line);font-size:12px;text-transform:uppercase;letter-spacing:.04em;color:var(--muted);text-align:left;padding:13px}
  th.col-key {width:22%} th.col-en {width:38%} th.col-translation {width:40%}
  tbody td {padding:12px 13px;border-bottom:1px solid var(--line);vertical-align:top;overflow-wrap:anywhere}
  tbody tr:last-child td {border-bottom:0}
  tbody tr:hover {background:var(--wash)}
  code {font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:12px;white-space:normal;color:var(--accent)}
  td.source {white-space:pre-wrap}
  textarea {resize:vertical;width:100%;min-height:54px;line-height:1.4;display:block}
  .meta {font-size:11px;min-height:17px;color:var(--muted);display:flex;justify-content:space-between;gap:10px;margin-top:4px}
  .warn {color:var(--error);font-weight:600}
  .saved {color:var(--ok)}
  tr.missing textarea {border-left:3px solid var(--line)}
  .empty-note {padding:24px;text-align:center;color:var(--muted);display:none}
  .help {font-size:12px;color:var(--muted);margin:12px 0}
  @media(max-width:680px){ th.col-key{width:25%}th.col-en{width:32%}th.col-translation{width:43%}td,thead th{padding:9px} .line{border-left:0;padding-left:0} }
</style>
</head>
<body>
<header>
  <h1>RTLPlayground · Translation editor</h1>
  <p class="sub">Edit both interface and login translations. Changes save automatically into <code>app.js</code>, <code>login.html</code>, and <code>index.html</code>.</p>
</header>
<main>
  <section class="toolbar" aria-label="Translation controls">
    <div class="control"><label for="language">Language to translate</label><select id="language"></select></div>
    <div class="control"><label for="search">Find key or English text</label><input id="search" type="search" placeholder="Filter the table…"></div>
    <div class="control"><label for="filter">Show</label><select id="filter"><option value="all">All strings</option><option value="missing">Missing only</option><option value="done">Translated only</option></select></div>
    <button id="next" class="secondary" type="button">Next missing</button>
    <div class="grow"></div>
    <form id="addForm" class="line">
      <div class="control"><label for="newCode">New language code</label><input id="newCode" autocomplete="off" placeholder="zz" maxlength="2" pattern="[a-z]{2}" title="Two lowercase letters" required></div>
      <div class="control"><label for="newName">Display name</label><input id="newName" autocomplete="off" placeholder="Language name" maxlength="80" required></div>
      <button type="submit">Add language</button>
    </form>
  </section>
  <div class="toolbar name-toolbar">
    <div class="control"><label for="displayName">Selected language's name in index.html</label><input id="displayName" placeholder="Name shown in language selector" maxlength="80" style="min-width:260px"><span class="help" id="nameSave" role="status"></span></div>
    <p class="sub">Uses a two-letter language code (e.g. <code>zz</code>, not a country code such as <code>ZZ</code>). Names are shown as written in the switch language selector.</p>
  </div>
  <div class="stats"><span id="progress">Loading…</span><span id="message" role="status" aria-live="polite"></span></div>
  <h2>Login page <code>login.html</code></h2>
  <p class="help">Translations appear on the sign-in form and its password error message. Empty fields fall back to English.</p>
  <section class="table-wrap" aria-label="Login page translations">
    <table><thead><tr><th class="col-key" scope="col">Login element</th><th class="col-en" scope="col">English reference</th><th class="col-translation" id="loginHead" scope="col">Translation</th></tr></thead><tbody id="loginRows"></tbody></table>
    <div class="empty-note" id="loginEmpty">No login strings match the current filters.</div>
  </section>
  <h2>Switch interface <code>app.js</code></h2>
  <section class="table-wrap" aria-label="Main interface translations">
    <table><thead><tr><th class="col-key" scope="col">Dictionary key</th><th class="col-en" scope="col">English reference</th><th class="col-translation" id="langHead" scope="col">Translation</th></tr></thead><tbody id="rows"></tbody></table>
    <div class="empty-note" id="empty">No interface strings match the current filters.</div>
  </section>
  <p class="help">Empty translations fall back to English. Keep placeholders such as <code>{n}</code>, <code>{ip}</code> and <code>{c}</code>. Each edited file gets its own <code>.translation-editor.bak</code> backup on the first change.</p>
</main>
<script>
'use strict';
'use strict';
const TOKEN = __TOKEN__;
const $ = id => document.getElementById(id);
const pending = new Map();
const inFlight = new Map();
let data = {english:[],languages:[],translations:{},loginEnglish:[],loginTranslations:{},languageNames:{}};
let current = '';
const status = (message, error=false) => {
  $('message').textContent=message;
  $('message').className=error?'error':'';
};

async function api(path, payload) {
  const r = await fetch(path, {method:payload?'POST':'GET',cache:'no-store',
    headers:payload?{'Content-Type':'application/json','X-Editor-Token':TOKEN}:{},
    body:payload?JSON.stringify(payload):undefined});
  let result;
  try { result=await r.json(); } catch (_) { throw Error(`HTTP ${r.status}`); }
  if(!r.ok) throw Error(result.error||`HTTP ${r.status}`);
  return result;
}
function tokens(text) { return [...new Set((text.match(/\{[a-zA-Z0-9_]+\}/g)||[]))].sort(); }
function placeholderWarning(source, target) {
  if(!target.trim()) return '';
  const a=tokens(source), b=tokens(target);
  return JSON.stringify(a)===JSON.stringify(b)?'':`Check placeholders: expected ${a.join(', ')||'none'}`;
}
function sourceRows(scope) { return scope==='login'?data.loginEnglish:data.english; }
function translationMap(scope) { return scope==='login'?data.loginTranslations:data.translations; }
function translated(scope,language,key) {
  return (translationMap(scope)[language]||{})[key]||'';
}
function stats() {
  const appDone=data.english.filter(({key})=>translated('app',current,key).trim()).length;
  const loginDone=data.loginEnglish.filter(({key})=>translated('login',current,key).trim()).length;
  $('progress').textContent=`app.js ${appDone}/${data.english.length} · login.html ${loginDone}/${data.loginEnglish.length} · ${$('rows').rows.length + $('loginRows').rows.length} shown`;
}
function selectedDisplayName() {
  $('displayName').value=data.languageNames[current]||'';
  $('displayName').disabled=!current;
  $('nameSave').textContent='';
}
function setupLanguages() {
  const select=$('language'); select.replaceChildren();
  for(const code of data.languages) {
    const option=document.createElement('option');
    option.value=code;
    option.textContent=(data.languageNames[code]||code.toUpperCase())+` (${code})`;
    select.append(option);
  }
  select.disabled=!data.languages.length;
  current=data.languages.includes(current)?current:(data.languages[0]||'');
  select.value=current;
  selectedDisplayName();
}
function setRowState(tr,field,source) {
  const value=field.value;
  tr.classList.toggle('missing',!value.trim());
  const warning=placeholderWarning(source,value);
  const info=tr.querySelector('.placeholder');
  info.textContent=warning;
  info.className=warning?'placeholder warn':'placeholder';
}
function renderTable(scope,tableId,emptyId) {
  const tbody=$(tableId); tbody.replaceChildren();
  const term=$('search').value.toLocaleLowerCase();
  const filter=$('filter').value;
  for(const source of sourceRows(scope)) {
    const value=translated(scope,current,source.key);
    if(term && ![source.key,source.value,source.context||''].some(s=>s.toLocaleLowerCase().includes(term))) continue;
    if(filter==='missing' && value.trim()) continue;
    if(filter==='done' && !value.trim()) continue;
    const tr=document.createElement('tr');
    const keyTd=document.createElement('td'), code=document.createElement('code');
    code.textContent=source.key;keyTd.append(code);
    if(source.context) {
      const context=document.createElement('div');context.className='help';context.textContent=source.context;
      keyTd.append(context);
    }
    const enTd=document.createElement('td'); enTd.className='source';enTd.textContent=source.value;
    const target=document.createElement('td');
    const field=document.createElement('textarea');field.value=value;field.rows=2;
    field.disabled=!current;
    field.setAttribute('aria-label',`${scope==='login'?'Login':'Interface'} translation for ${source.key}`);
    field.placeholder=current?'Translate here…':'Add a language to begin';
    const meta=document.createElement('div');meta.className='meta';
    const warn=document.createElement('span');warn.className='placeholder';
    const saveInfo=document.createElement('span');saveInfo.textContent=value?'Saved':'Missing';
    meta.append(warn,saveInfo);target.append(field,meta);tr.append(keyTd,enTd,target);tbody.append(tr);
    const lang=current, rowKey=source.key;
    field.addEventListener('input',()=>{
      saveInfo.className='';saveInfo.textContent='Unsaved…';
      if(!translationMap(scope)[lang]) translationMap(scope)[lang]={};
      translationMap(scope)[lang][rowKey]=field.value;
      setRowState(tr,field,source.value);stats();
      queueEdit(scope,lang,rowKey,field.value,saveInfo);
    });
    field.addEventListener('blur',()=>flushEdit(scope,lang,rowKey));
    setRowState(tr,field,source.value);
  }
  $(emptyId).style.display=tbody.rows.length?'none':'block';
}
function render() {
  $('loginHead').textContent=current?`Translation · ${current.toUpperCase()}`:'Translation';
  $('langHead').textContent=current?`Translation · ${current.toUpperCase()}`:'Translation';
  renderTable('login','loginRows','loginEmpty');
  renderTable('app','rows','empty');
  stats();
}
function editId(scope,lang,key) {return scope+'\0'+lang+'\0'+key;}
function queueEdit(scope,lang,key,value,label) {
  const id=editId(scope,lang,key), previous=pending.get(id);
  if(previous) clearTimeout(previous.timer);
  const edit={scope,lang,key,value,label,timer:null};
  edit.timer=setTimeout(()=>flushEdit(scope,lang,key),600);
  pending.set(id,edit);
}
function flushEdit(scope,lang,key) {
  const id=editId(scope,lang,key), edit=pending.get(id);
  if(!edit) return inFlight.get(id)||Promise.resolve();
  clearTimeout(edit.timer);pending.delete(id);
  if(edit.label) {edit.label.textContent='Saving…';edit.label.className='';}
  const prior=inFlight.get(id)||Promise.resolve();
  const payload=scope==='name'?{code:lang,name:edit.value}:
      {scope,language:lang,key,value:edit.value};
  const endpoint=scope==='name'?'/api/name':'/api/save';
  const job=prior.catch(()=>{}).then(()=>api(endpoint,payload))
    .then(()=>{
      if(!pending.has(id) && inFlight.get(id)===job) {
        if(edit.label) {edit.label.textContent='Saved';edit.label.className='saved';}
        if(scope==='name') {
          data.languageNames[lang]=edit.value.trim();
          const option=[...$('language').options].find(o=>o.value===lang);
          if(option) option.textContent=`${edit.value.trim()} (${lang})`;
        }
      }
      stats();status(`Saved to ${scope==='name'?'index.html':scope==='login'?'login.html':'app.js'}`);
    }).catch(err=>{
      if(edit.label) {edit.label.textContent='Save failed';edit.label.className='warn';}
      status(`${key}: ${err.message}`,true);
    });
  inFlight.set(id,job);
  job.finally(()=>{if(inFlight.get(id)===job) inFlight.delete(id);});
  return job;
}
async function flushAll() {
  await Promise.all([...pending.values()].map(e=>flushEdit(e.scope,e.lang,e.key)));
  await Promise.all([...inFlight.values()]);
}
$('language').addEventListener('change',async event=>{
  const code=event.target.value;
  await flushAll();current=code;selectedDisplayName();render();
});
$('displayName').addEventListener('input',()=>{
  const name=$('displayName').value;
  if(!current)return;
  if(!name.trim()) { $('nameSave').textContent='A name is required';$('nameSave').className='warn';return; }
  $('nameSave').textContent='Unsaved…';$('nameSave').className='';
  queueEdit('name',current,'display_name',name,$('nameSave'));
});
$('displayName').addEventListener('blur',()=>flushEdit('name',current,'display_name'));
$('search').addEventListener('input',()=>{
  for(const e of [...pending.values()]) if(e.scope!=='name') flushEdit(e.scope,e.lang,e.key);
  render();
});
$('filter').addEventListener('change',()=>{
  for(const e of [...pending.values()]) if(e.scope!=='name') flushEdit(e.scope,e.lang,e.key);
  render();
});
$('next').addEventListener('click',()=>{
  $('filter').value='missing';$('search').value='';
  for(const e of [...pending.values()]) if(e.scope!=='name') flushEdit(e.scope,e.lang,e.key);
  render();($('loginRows').querySelector('textarea')||$('rows').querySelector('textarea'))?.focus();
});
$('addForm').addEventListener('submit',async event=>{
  event.preventDefault();
  const code=$('newCode').value.trim().toLowerCase(),name=$('newName').value.trim();
  if(!/^[a-z]{2}$/.test(code)||code==='en'){status('Use two lowercase letters, other than en.',true);return;}
  if(!name){status('A language display name is required.',true);return;}
  try {
    await flushAll();
    await api('/api/languages',{code,name});
    current=code;data=await api('/api/state');
    setupLanguages();$('newCode').value='';$('newName').value='';
    $('filter').value='all';$('search').value='';render();
    status(`Added ${name} (${code}) to app.js, login.html, and index.html`);
    $('loginRows').querySelector('textarea')?.focus();
  }catch(err){status(err.message,true);}
});
window.addEventListener('beforeunload',event=>{
  if(pending.size||inFlight.size){event.preventDefault();event.returnValue='';}
});
(async()=>{
  try {
    data=await api('/api/state');setupLanguages();render();
    status(data.languages.length?'Ready. Changes save automatically.':'Add a language to start translating.');
  }catch(err){status(err.message,true);}
})();
</script>
</body>
</html>'''


def get_path(arg):
    candidate = Path(arg).expanduser().resolve()
    if candidate.is_dir():
        paths = [candidate / 'html' / 'app.js', candidate / 'app.js']
        candidate = next((p for p in paths if p.is_file()), paths[0])
    if not candidate.is_file():
        raise SystemExit(f'app.js not found at: {candidate}')
    return candidate


def make_handler(paths):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            print(f'[{self.log_date_time_string()}] {fmt % args}')

        def respond(self, status_code, body, content_type='application/json; charset=utf-8'):
            raw = (json.dumps(body, ensure_ascii=False) if isinstance(body, dict) else body).encode('utf-8')
            self.send_response(status_code)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(raw)))
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Content-Security-Policy', "default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; connect-src 'self'; base-uri 'none'; form-action 'self'")
            self.end_headers()
            self.wfile.write(raw)

        def do_GET(self):
            if self.path == '/':
                html = HTML.replace('__TOKEN__', json.dumps(TOKEN))
                self.respond(200, html, 'text/html; charset=utf-8')
            elif self.path == '/api/state':
                try:
                    with LOCK:
                        state = project_snapshot(paths)
                    self.respond(200, state)
                except (OSError, ParseError) as e:
                    self.respond(500, {'error': str(e)})
            else:
                self.respond(404, {'error': 'Not found'})

        def do_POST(self):
            if self.path not in ('/api/save', '/api/languages', '/api/name'):
                return self.respond(404, {'error':'Not found'})
            if self.headers.get('X-Editor-Token') != TOKEN or not self.headers.get('Content-Type','').startswith('application/json'):
                return self.respond(403, {'error': 'Invalid edit token or content type'})
            try:
                n = int(self.headers.get('Content-Length','0'))
                if not 0 < n <= 100000:
                    return self.respond(413, {'error': 'Invalid request length'})
                payload = json.loads(self.rfile.read(n))
                if not isinstance(payload, dict):
                    raise ValueError('Expected JSON object')
                with LOCK:
                    if self.path == '/api/languages':
                        code = payload.get('code')
                        name = payload.get('name')
                        app = read_js(paths['app'])
                        login = read_js(paths['login'])
                        index = read_js(paths['index'])
                        # Prepare and validate ALL edits before changing any file.
                        updated_app = add_language(app, code)
                        updated_login = add_login_language(login, code)
                        updated_index = set_language_name(index, code, name)
                        changed = False
                        for file_path, old, new in (
                            (paths['app'], app, updated_app),
                            (paths['login'], login, updated_login),
                            (paths['index'], index, updated_index),
                        ):
                            changed = atomic_save(file_path, old, new) or changed
                    elif self.path == '/api/name':
                        code, name = payload.get('code'), payload.get('name')
                        # A name must correspond to a language in app.js.
                        if code not in snapshot(read_js(paths['app']))['languages']:
                            raise ValueError('Unknown target language')
                        original = read_js(paths['index'])
                        changed = atomic_save(paths['index'], original,
                                              set_language_name(original, code, name))
                    else:
                        code = payload.get('language')
                        if code not in snapshot(read_js(paths['app']))['languages']:
                            raise ValueError('Unknown target language')
                        scope = payload.get('scope', 'app')
                        if scope not in ('app', 'login'):
                            raise ValueError('Unknown translation table')
                        file_path = paths[scope]
                        original = read_js(file_path)
                        if scope == 'app':
                            updated = set_translation(original, code, payload.get('key'), payload.get('value'))
                        else:
                            updated = set_login_translation(original, code, payload.get('key'), payload.get('value'))
                        changed = atomic_save(file_path, original, updated)
                self.respond(200, {'ok':True,'changed':changed})
            except (OSError, ParseError, ValueError, UnicodeError) as e:
                self.respond(400, {'error':str(e)})

    return Handler


def main():
    parser = argparse.ArgumentParser(description='RTLPlayground local translation editor')
    parser.add_argument('path', nargs='?', default='.', help='RTLPlayground root, html/ folder, or app.js')
    parser.add_argument('--port', type=int, default=8765, help='HTTP port (default: 8765, use 0 for an available port)')
    parser.add_argument('--host', default='127.0.0.1', help='Bind address (default: 127.0.0.1)')
    parser.add_argument('--open', action='store_true', help='Open a browser automatically')
    args = parser.parse_args()
    path = get_path(args.path)
    paths = {'app':path, 'login':path.with_name('login.html'),
             'index':path.with_name('index.html')}
    for label, file_path in paths.items():
        if not file_path.is_file():
            parser.error(f'Missing {label} source file: {file_path}')
    try:
        state = project_snapshot(paths)
    except (ParseError, OSError) as e:
        parser.error(f'Cannot read RTLPlayground translations: {e}')
    if args.host not in ('127.0.0.1','localhost','::1'):
        print('WARNING: Binding to a network interface exposes a WRITE API. Only use on a trusted network.')
    server = ThreadingHTTPServer((args.host, args.port), make_handler(paths))
    url = f'http://{args.host}:{server.server_port}/'
    print(f'RTLPlayground translation editor: {url}')
    print(f'Editing: {path.parent} (app.js, login.html, index.html)')
    print(f'{len(state["english"])} interface keys, {len(state["loginEnglish"])} login strings, '
          f'{len(state["languages"])} translated language(s)')
    print('Press Ctrl+C to stop the server.')
    if args.open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
