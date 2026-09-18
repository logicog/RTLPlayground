/* Stand-in for the generated html_data.h: the table generators under test
 * never touch the embedded file list, and the real header exists only after
 * a firmware build, which the host harness must not depend on. */
#ifndef FDATA_DEFS_H
#define FDATA_DEFS_H
#include <stdint.h>
typedef enum mime_type_e { mime_HTML = 0, mime_SVG, mime_ICO, mime_PNG, mime_JS, mime_CSS, mime_TXT } mime_type_t;
struct f_data { __code char *file; uint32_t start; uint16_t len; mime_type_t mime; uint8_t gzip; };
#define FDATA_START_login_html		0x00040000UL
#define FDATA_START_favicon_ico		0x00041000UL
#endif
