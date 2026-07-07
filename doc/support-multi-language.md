# Supporting Multiple Languages in the Web UI

The firmware uses a client-side i18n approach
all translations are stored in a single JavaScript dictionary embedded in the firmware.
No server-side changes are needed.

## Architecture

All translation logic lives in `html/i18n.js`. The file contains:

- A `LANG` object with one sub-object per language (`en`, `ja`, ...)
- Language auto-detection (browser language → `localStorage` override)
- `t(key)` — look up a translated string
- `setLang(lang)` — switch language and update the page
- `applyTranslation(el)` — apply translation to one DOM element

Translation keys are **flat strings** (no nesting). The English keys in `LANG.en` also serve as the fallback when a key is missing in another language.

## How to Add a New Language

### 1. Add a dictionary entry in `html/i18n.js`

Append a new sub-object to the `LANG` object. Every key from `LANG.en` must be present:

```js
var LANG = {
  en: {
    nav_overview: 'Overview',
    nav_port_config: 'Port Configuration',
    // ... all keys for English
  },
  ja: {
    nav_overview: '概要',
    nav_port_config: 'ポート設定',
    // ... all keys for Japanese
  },
  LANGCODE: {            // ← add your language here
    nav_overview: '...',
    nav_port_config: '...',
    // ... translate every key
  },
};
```

### 2. Add the language to the navigation sidebar

In `html/navigation.js`, add an `<option>` to the language selector:

```js
+ "<option value='en'>English</option><option value='ja'>日本語</option>"
```

Replace with:

```js
+ "<option value='en'>English</option><option value='ja'>日本語</option><option value='LANGCODE'>Native Name</option>"
```

### 3. Verify auto-detection

The language detection code in `i18n.js` reads `navigator.language` and normalises it to the first two characters:

```js
var browser = (navigator.language || navigator.userLanguage || 'en').substring(0, 2);
return LANG[browser] ? browser : 'en';
```

If the two-letter code matches a key in `LANG`, it will be auto-selected. No changes needed here.

## Two Translation Mechanisms

### (A) `data-i18n` attribute (declarative — for HTML)

Add `data-i18n="key_name"` to any HTML element. The English text goes in the element content as a fallback:

```html
<h1 data-i18n="port_heading">Port Configuration</h1>
<input type="button" data-i18n="port_apply" value="Apply">
<option data-i18n="port_auto">Auto</option>
<title data-i18n="port_title">Port Configuration</title>
```

On page load, `applyTranslation()` sets:

- `el.value` for `<input type="submit|button">`
- `el.textContent` for `<option>`, `<title>`
- `el.innerHTML` for everything else

### (B) `t('key')` call (imperative — for JavaScript strings)

When generating HTML or text in JavaScript, wrap translatable strings with `t()`:

```js
td.appendChild(document.createTextNode(t('common_port') + i));
td.innerHTML = t('port_auto');
iHTML += "<tr><td>" + t('port_vendor') + "</td></tr>";
```

### Special Case: Link Speed Display

The `linkS` array in `html/main.js` maps numeric link states to display strings. The first two entries (`speed_disabled`, `speed_down`) use `t()` for translation; the remaining entries are static literals (they are the same in all languages):

```js
const linkS = [
  function(){return t('speed_disabled')},
  function(){return t('speed_down')},
  "10M", "100M", "1000M", "500M", "10G", "2.5G", "5G"
];
function linkText(idx) { var v = linkS[idx]; return typeof v === 'function' ? v() : v; }
```

Always use `linkText(idx)` (not `linkS[idx]`) to read these values.

## Size Considerations

- `html/i18n.js` is embedded in the firmware filesystem (~14 KB for two languages)
- Each new language adds roughly the same number of bytes as the English dictionary (~3–4 KB)
- The firmware binary is padded to 512 KiB, so a few extra KB do not change the flash footprint
- Values that are identical in all languages should be inlined as literals rather than added to the dictionary (e.g., `"10M"`, `"2.5G"`, `"MAC"`, `"VLAN"`, `"CPU"`)

## Build

No special flags are needed. The `html/` directory is embedded by `fileadder` during the build.

## Script Load Order

`i18n.js` must be loaded after `main.js` (which defines `t()`'s dependencies like `LANG`) but before any page-specific JS that calls `t()`:

```html
<script src="/main.js"></script>
<script src="/i18n.js"></script>
<script src="/eee.js"></script>    <!-- uses t() -->
```

The `navigation.js` script is loaded last (bottom of `<body>`).
