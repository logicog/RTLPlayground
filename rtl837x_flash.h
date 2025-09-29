#ifndef _RTL837X_FLASH_H_
#define _RTL837X_FLASH_H_

void flash_init(uint8_t enable_dio);
void flash_read_uid(void);
void flash_write_enable(void);
void flash_dump(register uint32_t addr, register uint8_t len);
void flash_read_jedecid(void);
void flash_read_security(uint32_t addr, uint8_t len);
void flash_sector_erase(uint32_t addr);
void flash_read_bulk(register __xdata uint8_t *dst, __xdata uint32_t src, register uint16_t len);
void flash_write_bytes(__xdata uint32_t addr, register __xdata uint8_t *ptr, register uint16_t len);
void flash_find_mark(__xdata uint32_t src, register uint16_t len, __code uint8_t *mark);
#endif
