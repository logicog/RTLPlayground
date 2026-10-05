#ifndef _UPDATE_STAGE_H_
#define _UPDATE_STAGE_H_

#include <stdint.h>

/*
 * Sparse staging of a firmware image into the free flash of the running image
 * (the staging pool, see rtl837x_flash.h). Used on devices that have no room
 * for a second full image: only the sectors of the image that hold something
 * are stored, the apply writes zeros for the rest. The image checksum is
 * computed over the complete upload as before, so both kinds of devices verify
 * the same thing.
 *
 * The code lives in BANK3: no code bank occupies flash the pool can use, so
 * staging cannot erase the code that runs it. The upload handler feeds it the
 * 256-byte pages of the image as they arrive.
 */
#define UPDATE_STAGE_OK		0
#define UPDATE_STAGE_NO_ROOM	1	// the free flash is used up
#define UPDATE_STAGE_OVERLAP	2	// would sit on an address the copy back writes

/*
 * Starts a staging. pool_bottom is the end of the web UI of the running image:
 * the pool stays above it, since the UI is read from flash while the switch
 * runs. Also forgets the state record of an earlier attempt.
 */
void update_stage_begin(uint32_t pool_bottom) __banked;

/* Stages the page just assembled in flash_buf. */
uint8_t update_stage_page(void) __banked;

/* Completes the image after the last page; call with a padded last page. */
uint8_t update_stage_end(void) __banked;

/* Records a completely staged image in the state sector, so it can be applied. */
void update_stage_commit(uint16_t crc) __banked;

/* Forgets a failed staging: nothing is applied from it. */
void update_stage_discard(void) __banked;

#endif
