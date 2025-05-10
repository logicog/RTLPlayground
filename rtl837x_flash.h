#ifndef _RTL837X_FLASH_H_
#define _RTL837X_FLASH_H_

#include <stdint.h>
uint8_t flash_read_status(void);
void flash_init(void);
void flash_read_uid(void);
void flash_write_enable(void);
void flash_dump(uint32_t addr, uint8_t len);
void flash_read_jedecid(void);
void flash_read_security(uint32_t addr, uint8_t len);

#endif
