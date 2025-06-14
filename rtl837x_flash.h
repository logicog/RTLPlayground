#ifndef _RTL837X_FLASH_H_
#define _RTL837X_FLASH_H_

#include <stdint.h>
uint8_t flash_read_status(void);
void flash_init(uint8_t enable_dio);
void flash_read_uid(void);
void flash_write_enable(void);
void flash_dump(uint32_t addr, uint8_t len);
void flash_read_jedecid(void);
void flash_read_security(uint32_t addr, uint8_t len);
void flash_block_erase(uint32_t addr);
void flash_write_bytes(uint32_t addr, __xdata uint8_t *ptr, uint8_t len);
#endif
