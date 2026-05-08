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
void flash_write_enable(void);
void flash_dump(uint8_t len);
void flash_read_jedecid(void);
void flash_read_security(void);
void flash_sector_erase(void);
void flash_read_bulk(__xdata uint8_t *dst);
void flash_write_bytes(__xdata uint8_t *ptr);
__code char* get_flash_size_str(void);

#endif

