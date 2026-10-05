#ifndef _UPDATE_POOL_H_
#define _UPDATE_POOL_H_

#include <stdint.h>
#include "rtl837x_common.h"

/*
 * Update state, kept in one flash sector so it survives the reset that applies
 * an update. STAGING: the image is in the pool, present[] marks its sectors.
 * APPLY: the copy over the running image has started.
 */
typedef struct {
	uint32_t magic;
	uint8_t flags;
	uint16_t staged;			// image sectors stored in the pool
	uint16_t crc;				// CRC16 of the complete image (0xb001)
	uint16_t crc_apply;			// CRC16 of the sectors the apply writes
	uint32_t pool_bottom;			// where the pool of this staging starts
	uint8_t present[UPDATE_SECTORS/8];	// image sector -> stored in pool
} update_state_t;

extern __xdata update_state_t update_state;

void update_state_read(void);
void update_state_write(void);
void update_state_clear(void);

// The pool is flash that neither a code bank nor (above bottom) served UI data
// sits in. Returns the sector the pool starts on.
uint32_t update_pool_set_bottom(uint32_t bottom);
uint32_t update_pool_addr(uint16_t staged_idx);
uint16_t update_pool_index(uint16_t sector);
uint8_t update_pool_conflict(uint32_t addr);
uint8_t update_pool_target(uint16_t sector);

#endif
