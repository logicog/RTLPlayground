#ifndef _UPDATE_STAGE_H_
#define _UPDATE_STAGE_H_

#include <stdint.h>

/*
 * Sparse staging: only the image sectors that hold something are stored in the
 * free flash of the running image (see rtl837x_flash.h), the apply writes zeros
 * for the rest. Lives in BANK3, which the pool never covers.
 */
#define UPDATE_STAGE_OK		0
#define UPDATE_STAGE_NO_ROOM	1	// free flash used up
#define UPDATE_STAGE_OVERLAP	2	// on an address the copy back writes

void update_stage_begin(uint32_t pool_bottom) __banked;	// end of the web UI
uint8_t update_stage_page(void) __banked;		// stage the page in flash_buf
uint8_t update_stage_end(void) __banked;		// complete the image
void update_stage_commit(uint16_t crc) __banked;	// make it applicable
void update_stage_discard(void) __banked;		// forget it again

#endif
