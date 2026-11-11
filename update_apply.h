#ifndef _UPDATE_APPLY_H_
#define _UPDATE_APPLY_H_

/*
 * At boot, check for an image staged in flash by the web upload and, if one is
 * present and its CRC is good, copy it over the running image and reset.
 *
 * Talks to the flash only through the flash interface (rtl837x_flash.h), so it
 * builds and runs on the host test bench against a modelled flash device.
 * Must stay resident (no codeseg pragma): it erases and writes the code banks,
 * so it cannot run from one.
 */
void check_and_flash_update_image(void);

#endif
