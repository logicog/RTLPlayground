/*
 * Startup-config shadow store, giving the CLI a reliable `save` command.
 *
 * The web UI's save button merges the startup config with the command
 * log in browser JavaScript; nothing on the device itself could persist
 * the running changes. This module keeps an authoritative in-RAM copy
 * of the startup config instead: seeded from flash at boot and after a
 * web config upload, and updated line-by-line as config commands are
 * executed, using the same per-command overwrite/delete semantics as
 * the web UI (CONF_CMDS/CONF_TOGGLE/CONF_OVERWRITE in app.js). `save`
 * then writes the shadow to the config sector and verifies it.
 *
 * Query commands never enter the shadow. Commands the web UI does not
 * persist (e.g. static `mac` entries) are skipped here too, so both
 * save paths agree on what a startup config can contain.
 */
#include "machine.h"
#include "cfgstore.h"
#include "cmd_parser.h"
#include "rtl837x_common.h"
#include "rtl837x_flash.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __xdata uint8_t cmd_words_len;
extern __xdata uint8_t cmd_words_b[];
extern __xdata struct flash_region_t flash_region;
extern __xdata uint8_t flash_buf[FLASH_BUF_SIZE];

static __xdata char shadow[CFG_SHADOW_SIZE];
static __xdata uint16_t shadow_len;

/* Command normalized to single-space-separated words */
static __xdata char norm[CMD_BUF_SIZE];
static __xdata uint8_t norm_wb[16];	/* word start offsets */
static __xdata uint8_t norm_words;

/* Delete-rule kinds for lines already in the shadow */
#define DK_NONE		0	/* delete nothing */
#define DK_PREFIX	1	/* first dk_n words match norm */
#define DK_TOGGLE	2	/* first dk_n words match, next word is on|off */
#define DK_VLAN_SET	3	/* words 0-1 match, line word 2 is not "mgmt" */
#define DK_VLAN_MGMT	4	/* line word 0 == "vlan" and word 2 == "mgmt" */
#define DK_PORT_SET	5	/* words 0-1 match, line word 2 is not "name" */
#define DK_LAG_D	6	/* (lag|laghash) with word 1 matching norm word 1 */
#define DK_EEE_PORT	7	/* word 0 == "eee", line word 2 == norm word 2 */
#define DK_BW_RATE	8	/* words 0-2 match, line word 3 not drop|fc */
#define DK_BW_MODE	9	/* words 0-2 match, line word 3 is drop|fc */

static __xdata uint8_t dk_kind;
static __xdata uint8_t dk_n;
static __xdata uint8_t do_store;


/* --- small string helpers on xdata --- */

static uint8_t is_digit_x(char c) __reentrant
{
	return c >= '0' && c <= '9';
}


/* Length of word starting at p (to space/NL/NUL) */
static uint8_t wlen(__xdata const char *p) __reentrant
{
	uint8_t n = 0;

	while (p[n] && p[n] != ' ' && p[n] != '\n' && p[n] != '\r')
		n++;
	return n;
}


/* Start of word n in a line; 0 if the line has no word n */
static __xdata const char *line_word(__xdata const char *line, uint8_t n) __reentrant
{
	while (n--) {
		line += wlen(line);
		while (*line == ' ')
			line++;
		if (!*line || *line == '\n' || *line == '\r')
			return 0;
	}
	return line;
}


/* Compare word at p against a code-space literal */
static uint8_t wordc(__xdata const char *p, __code const char *s) __reentrant
{
	uint8_t l = wlen(p);

	while (l && *s) {
		if (*p != *s)
			return 0;
		p++; s++; l--;
	}
	return !l && !*s;
}


/* Compare word at p against word at q (both xdata) */
static uint8_t wordx(__xdata const char *p, __xdata const char *q) __reentrant
{
	uint8_t lp = wlen(p);
	uint8_t lq = wlen(q);

	if (lp != lq)
		return 0;
	while (lp--) {
		if (*p++ != *q++)
			return 0;
	}
	return 1;
}


/* Word n of norm; norm always has the words the classifier asks for */
#define NW(n) ((__xdata const char *)&norm[norm_wb[n]])


/* Do the first n words of line match the first n words of norm? */
static uint8_t line_match_n(__xdata const char *line, uint8_t n) __reentrant
{
	__xdata const char *lw;
	uint8_t i;

	for (i = 0; i < n; i++) {
		lw = line_word(line, i);
		if (!lw || !wordx(lw, NW(i)))
			return 0;
	}
	return 1;
}


static uint8_t word_on_off(__xdata const char *w) __reentrant
{
	return w && (wordc(w, "on") || wordc(w, "off"));
}


/* Does this shadow line die under the current delete rule? */
static uint8_t line_dies(__xdata const char *line) __reentrant
{
	__xdata const char *w;

	switch (dk_kind) {
	case DK_PREFIX:
		return line_match_n(line, dk_n);
	case DK_TOGGLE:
		if (!line_match_n(line, dk_n))
			return 0;
		return word_on_off(line_word(line, dk_n));
	case DK_VLAN_SET:
		if (!line_match_n(line, 2))
			return 0;
		w = line_word(line, 2);
		return !(w && wordc(w, "mgmt"));
	case DK_VLAN_MGMT:
		w = line_word(line, 0);
		if (!w || !wordc(w, "vlan"))
			return 0;
		w = line_word(line, 2);
		return w && wordc(w, "mgmt");
	case DK_PORT_SET:
		if (!line_match_n(line, 2))
			return 0;
		w = line_word(line, 2);
		return !(w && wordc(w, "name"));
	case DK_LAG_D:
		w = line_word(line, 0);
		if (!w || !(wordc(w, "lag") || wordc(w, "laghash")))
			return 0;
		w = line_word(line, 1);
		return w && wordx(w, NW(1));
	case DK_EEE_PORT:
		w = line_word(line, 0);
		if (!w || !wordc(w, "eee"))
			return 0;
		w = line_word(line, 2);
		return w && wordx(w, NW(2));
	case DK_BW_RATE:
	case DK_BW_MODE:
		if (!line_match_n(line, 3))
			return 0;
		w = line_word(line, 3);
		if (!w)
			return 0;
		if (wordc(w, "drop") || wordc(w, "fc"))
			return dk_kind == DK_BW_MODE;
		return dk_kind == DK_BW_RATE;
	}
	return 0;
}


/* Remove all shadow lines matching the current delete rule */
static __xdata uint16_t sd_r, sd_w, sd_ls;

static void shadow_delete(void) __reentrant
{
	uint8_t dies;

	if (dk_kind == DK_NONE)
		return;
	sd_r = 0; sd_w = 0;
	while (shadow[sd_r]) {
		sd_ls = sd_r;
		while (shadow[sd_r] && shadow[sd_r] != '\n')
			sd_r++;
		if (shadow[sd_r] == '\n')
			sd_r++;
		dies = line_dies(&shadow[sd_ls]);
		if (!dies) {
			while (sd_ls < sd_r)
				shadow[sd_w++] = shadow[sd_ls++];
		}
	}
	shadow[sd_w] = 0;
	shadow_len = sd_w;
}


static __xdata uint16_t sa_l;

static void shadow_append_norm(void) __reentrant
{
	sa_l = 0;
	while (norm[sa_l])
		sa_l++;
	if (shadow_len + sa_l + 2 >= CFG_SHADOW_SIZE) {
		print_string("Startup config full; line NOT recorded for save: ");
		print_string_x(norm);
		write_char('\n');
		return;
	}
	sa_l = 0;
	while (norm[sa_l])
		shadow[shadow_len++] = norm[sa_l++];
	shadow[shadow_len++] = '\n';
	shadow[shadow_len] = 0;
}


/*
 * Classify the normalized command: set the delete rule for superseded
 * shadow lines and whether the command itself is stored. Returns 0 for
 * commands that never belong in a startup config.
 */
static uint8_t classify(void) __reentrant
{
	__xdata const char *w0 = NW(0);
	__xdata const char *w1 = norm_words > 1 ? NW(1) : 0;

	dk_kind = DK_NONE;
	do_store = 1;

	if (wordc(w0, "ip") || wordc(w0, "gw") || wordc(w0, "netmask")
	    || wordc(w0, "hostname") || wordc(w0, "passwd") || wordc(w0, "ntp")) {
		if (norm_words < 2)
			return 0;	/* query form */
		/* `ntp off` overwrites like any server setting */
		dk_kind = DK_PREFIX; dk_n = 1;
		return 1;
	}
	if (wordc(w0, "mtu") || wordc(w0, "pvid") || wordc(w0, "isolate")
	    || wordc(w0, "laghash")) {
		if (norm_words < 3)
			return 0;
		dk_kind = DK_PREFIX; dk_n = 2;
		return 1;
	}
	if (wordc(w0, "ingress")) {
		if (norm_words < 2)
			return 0;
		dk_kind = DK_PREFIX; dk_n = 1;
		return 1;
	}
	if (wordc(w0, "syslog") || wordc(w0, "telnet") || wordc(w0, "totp")) {
		if (!w1)
			return 0;
		if (word_on_off(w1)) {
			dk_kind = DK_TOGGLE; dk_n = 1;
			return 1;
		}
		if (wordc(w1, "ip") || wordc(w1, "port") || wordc(w1, "bind")
		    || wordc(w1, "secret") || wordc(w1, "timeout")) {
			if (norm_words < 3)
				return 0;
			dk_kind = DK_PREFIX; dk_n = 2;
			return 1;
		}
		return 0;
	}
	if (wordc(w0, "igmp")) {
		if (!word_on_off(w1))
			return 0;	/* igmp show */
		dk_kind = DK_TOGGLE; dk_n = 1;
		return 1;
	}
	if (wordc(w0, "stp")) {
		if (!w1)
			return 0;
		if (word_on_off(w1)) {
			dk_kind = DK_TOGGLE; dk_n = 1;
			return 1;
		}
		if (wordc(w1, "prio") || wordc(w1, "hello") || wordc(w1, "maxage")
		    || wordc(w1, "fwd") || wordc(w1, "txhold") || wordc(w1, "version")) {
			if (norm_words < 3)
				return 0;
			dk_kind = DK_PREFIX; dk_n = 2;
			return 1;
		}
		if ((wordc(w1, "port") || wordc(w1, "lag")) && norm_words >= 4) {
			if (word_on_off(NW(3)) && norm_words == 4) {
				dk_kind = DK_TOGGLE; dk_n = 3;
				return 1;
			}
			if (norm_words >= 5) {
				dk_kind = DK_PREFIX; dk_n = 4;
				return 1;
			}
		}
		return 0;
	}
	if (wordc(w0, "vlan")) {
		if (!w1 || !is_digit_x(*w1))
			return 0;	/* vlan show */
		if (norm_words == 3 && wordc(NW(2), "d")) {
			dk_kind = DK_PREFIX; dk_n = 2;	/* takes mgmt line too */
			do_store = 0;
			return 1;
		}
		if (norm_words == 3 && wordc(NW(2), "mgmt")) {
			dk_kind = DK_VLAN_MGMT;
			return 1;
		}
		if (norm_words < 3)
			return 0;
		dk_kind = DK_VLAN_SET;
		return 1;
	}
	if (wordc(w0, "port")) {
		if (norm_words < 3)
			return 0;
		if (norm_words >= 4 && wordc(NW(2), "name")) {
			dk_kind = DK_PREFIX; dk_n = 3;
			return 1;
		}
		dk_kind = DK_PORT_SET;
		return 1;
	}
	if (wordc(w0, "mirror")) {
		if (!w1)
			return 0;
		dk_kind = DK_PREFIX; dk_n = 1;
		if (wordc(w1, "off"))
			do_store = 0;
		return 1;
	}
	if (wordc(w0, "lag")) {
		if (norm_words < 3)
			return 0;
		if (wordc(NW(2), "d")) {
			dk_kind = DK_LAG_D;
			do_store = 0;
			return 1;
		}
		dk_kind = DK_PREFIX; dk_n = 2;
		return 1;
	}
	if (wordc(w0, "eee")) {
		if (!word_on_off(w1))
			return 0;
		if (norm_words >= 3 && is_digit_x(*NW(2))) {
			dk_kind = DK_EEE_PORT;
			return 1;
		}
		dk_kind = DK_PREFIX; dk_n = 1;
		return 1;
	}
	if (wordc(w0, "bw")) {
		if (norm_words < 4)
			return 0;
		if (wordc(NW(3), "drop") || wordc(NW(3), "fc"))
			dk_kind = DK_BW_MODE;
		else
			dk_kind = DK_BW_RATE;
		dk_n = 3;
		return 1;
	}
	return 0;
}


/* Rebuild the just-executed command from its tokens, single-spaced */
static __xdata uint8_t nb_d;

static uint8_t norm_build(void) __reentrant
{
	uint8_t w, s;

	norm_words = cmd_words_len;
	if (!norm_words || norm_words > 15)
		return 0;
	nb_d = 0;
	for (w = 0; w < norm_words; w++) {
		norm_wb[w] = nb_d;
		s = cmd_words_b[w];
		while (cmd_buffer[s] && cmd_buffer[s] != ' ') {
			if (nb_d >= CMD_BUF_SIZE - 2)
				return 0;
			norm[nb_d++] = cmd_buffer[s++];
		}
		norm[nb_d++] = (w == norm_words - 1) ? 0 : ' ';
	}
	return 1;
}


void cfgstore_note(void) __banked
{
	if (!norm_build())
		return;
	if (!classify())
		return;
	shadow_delete();
	if (do_store)
		shadow_append_norm();
}


void cfgstore_load(void) __banked
{
	__xdata uint32_t pos = CONFIG_START;
	__xdata uint16_t d = 0;
	uint8_t i, chunks;
	char c;

	/* 2048 shadow bytes = 8 chunks of 256 */
	for (chunks = 0; chunks < CFG_SHADOW_SIZE / 256; chunks++) {
		flash_region.addr = pos;
		flash_region.len = 256;
		flash_read_bulk(flash_buf);
		i = 0;
		do {
			c = flash_buf[i++];
			/* 0xff = erased sector, treat like end of config */
			if (!c || c == (char)0xff)
				goto done;
			if (d < CFG_SHADOW_SIZE - 1)
				shadow[d++] = c;
		} while (i);
		pos += 256;
	}
done:
	/* ensure the text ends with a newline before the terminator */
	if (d && shadow[d - 1] != '\n' && d < CFG_SHADOW_SIZE - 1)
		shadow[d++] = '\n';
	shadow[d] = 0;
	shadow_len = d;
}


void cfgstore_save(void) __banked
{
	__xdata uint32_t pos;
	__xdata uint16_t left, off;
	uint8_t i, n;

	flash_region.addr = CONFIG_START;
	flash_sector_erase();
	flash_region.addr = CONFIG_START;
	flash_region.len = shadow_len + 1;	/* include terminator */
	flash_write_bytes((__xdata uint8_t *)shadow);

	/* verify */
	pos = CONFIG_START;
	off = 0;
	left = shadow_len + 1;
	while (left) {
		n = left > 128 ? 128 : left;
		flash_region.addr = pos;
		flash_region.len = n;
		flash_read_bulk(flash_buf);
		for (i = 0; i < n; i++) {
			if (flash_buf[i] != (uint8_t)shadow[off + i]) {
				print_string("ERROR: verify failed, startup config may be corrupt!\n");
				err_status = ERR_INVALID_ARGUMENT;
				return;
			}
		}
		off += n;
		pos += n;
		left -= n;
	}
	print_string("Saved ");
	itoa_short(shadow_len);
	print_string(" bytes to startup config\n");
}


void cfgstore_show(void) __banked
{
	print_string_x(shadow);
	if (shadow_len && shadow[shadow_len - 1] != '\n')
		write_char('\n');
}
