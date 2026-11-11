/*
 * update_shim.c - bind the firmware's flash interface and a few boot hooks to
 * the modelled flash device, so the real check_and_flash_update_image() runs
 * on the host. reset_chip() unwinds via longjmp so the test regains control.
 */
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include "rtl837x_common.h"
#include "rtl837x_flash.h"
#include "update_apply.h"
#include "flash_dev.h"
#include "update_shim.h"

flashdev_t g_flash;
int shim_verbose;

/* Storage the firmware expects to exist (defined in rtl837x_flash.c /
 * rtlplayground.c / crc16.asm on target). */
struct flash_region_t flash_region;
uint8_t  flash_buf[FLASH_BUF_SIZE];
uint32_t flash_size;

static jmp_buf reset_jmp;

/* --- flash interface, forwarded to the model --- */
void flash_read_jedecid(void) { flash_size = g_flash.size; }

const char *get_flash_size_str(void)
{
	switch (g_flash.size) {
	case 0x80000:  return "512K";
	case 0x100000: return "1M";
	case 0x200000: return "2M";
	case 0x400000: return "4M";
	default:       return "?";
	}
}

void flash_init(uint8_t enable_dio) { (void)enable_dio; }
void flash_read_bulk(uint8_t *dst)  { fd_read(&g_flash, flash_region.addr, dst, flash_region.len); }
/* A modelled power cut aborts execution where it happens - the CPU dies, it
 * does not skip one write and carry on - so unwind out of the routine. */
void flash_sector_erase(void)       { fd_erase_sector(&g_flash, flash_region.addr); if (g_flash.powercut) longjmp(reset_jmp, 2); }
void flash_write_bytes(uint8_t *ptr){ fd_write(&g_flash, flash_region.addr, ptr, flash_region.len); if (g_flash.powercut) longjmp(reset_jmp, 2); }

/* --- boot hooks the routine touches --- */
void set_sys_led_state(uint8_t state) { (void)state; }
void delay(uint16_t t)                { (void)t; }
void print_string(const char *p)      { if (shim_verbose) fputs(p, stdout); }
void print_string_x(char *p)          { if (shim_verbose) fputs(p, stdout); }
void write_char(char c)               { if (shim_verbose) putchar(c); }
void write_char_no_syslog(char c)     { if (shim_verbose) putchar(c); }
void reset_chip(void)                 { longjmp(reset_jmp, 1); }

int update_run(void)
{
	int v = setjmp(reset_jmp);
	if (v == 1)
		return 1;		/* reset_chip() reached -> update applied */
	if (v == 2)
		return 0;		/* power cut mid-op -> aborted, g_flash.powercut set */
	check_and_flash_update_image();
	return 0;			/* returned normally -> no reset */
}
