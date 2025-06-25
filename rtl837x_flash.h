#ifndef _RTL837X_FLASH_H_
#define _RTL837X_FLASH_H_

void flash_init(uint8_t enable_dio) __banked;
void flash_read_uid(void) __banked;
void flash_write_enable(void)__banked ;
void flash_dump(uint32_t addr, uint8_t len) __banked;
void flash_read_jedecid(void) __banked;
void flash_read_security(uint32_t addr, uint8_t len)__banked ;
void flash_block_erase(uint32_t addr) __banked;
void flash_write_bytes(uint32_t addr, __xdata uint8_t *ptr, uint8_t len)__banked ;
#endif
