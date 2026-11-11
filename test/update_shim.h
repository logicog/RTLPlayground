#ifndef UPDATE_SHIM_H
#define UPDATE_SHIM_H

#include <stdint.h>
#include "flash_dev.h"

/* The modelled flash the firmware's flash interface is bound to. */
extern flashdev_t g_flash;

/* Set non-zero to let the firmware's console output through to stdout. */
extern int shim_verbose;

/* Run check_and_flash_update_image() against g_flash.
 * Returns 1 if it reset the chip (i.e. applied an update), 0 if it returned
 * normally (too small / no image / bad CRC). */
int update_run(void);

/* crc16_host.c */
uint16_t crc16_host_step(uint16_t crc, uint8_t b);
uint16_t crc16_host_buf(const uint8_t *d, uint32_t n);

#endif
