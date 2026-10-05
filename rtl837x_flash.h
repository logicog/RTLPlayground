#ifndef _RTL837X_FLASH_H_
#define _RTL837X_FLASH_H_

// SPI FLASH MEMORY PAGE SIZE.
#define FLASH_PAGE_SIZE 0x100
// SPI FLASH MEMORY SECTOR SIZE = ERASE SIZE.
#define FLASH_SECTOR_SIZE 0x1000

#if (FLASH_SECTOR_SIZE % FLASH_PAGE_SIZE) != 0
#error "FLASH_SECTOR_SIZE must be a multiple of FLASH_PAGE_SIZE"
#endif


void flash_init(uint8_t enable_dio);
void flash_read_uid(void);
void flash_dump(uint8_t len);
void flash_read_jedecid(void);
void flash_read_security(void);
void flash_sector_erase(void);
void flash_read_bulk(__xdata uint8_t *dst);
void flash_write_bytes(__xdata uint8_t *ptr);
__code const char* get_flash_size_str(void);

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
	uint32_t pool_bottom;			// where the pool of this staging starts
	uint8_t present[UPDATE_SECTORS/8];	// image sector -> stored in pool
} update_state_t;

extern __xdata update_state_t update_state;

void update_state_read(void);
void update_state_write(void);
void update_state_clear(void);

/*
 * Devices with room for a second full image stage it above the running image;
 * everything else stages the image into the free space of the running image
 * (the sparse staging pool), sector by sector. Only the sectors that are not
 * all zero are stored, so the pool only needs to hold the parts of the image
 * that are actually used.
 */
// The pool is flash that neither a code bank nor (above bottom) served UI data
// sits in. Returns the sector the pool starts on.
uint32_t update_pool_set_bottom(uint32_t bottom);

uint32_t update_pool_addr(uint16_t staged_idx);
uint16_t update_pool_index(uint16_t sector);
uint8_t update_pool_conflict(uint32_t addr);
uint8_t update_pool_target(uint16_t sector);

#endif

