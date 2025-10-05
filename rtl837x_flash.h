#ifndef _RTL837X_FLASH_H_
#define _RTL837X_FLASH_H_

void flash_init(uint8_t enable_dio);
void flash_read_uid(void);
void flash_write_enable(void);
void flash_dump(uint8_t len);
void flash_read_jedecid(void);
void flash_read_security(uint32_t addr, uint8_t len);
void flash_sector_erase(uint32_t addr);
void flash_read_bulk(__xdata uint8_t *dst);
void flash_write_bytes(__xdata uint8_t *ptr);
#endif
