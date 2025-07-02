#ifndef _RTL837X_STDIO_H_
#define _RTL837X_STDIO_H_

// Headers for calls in the common code area (HOME/BANK0)

void print_string(__code char *p);
void print_short(uint16_t a);
void print_byte(uint8_t a);
void print_sfr_data(void);
void print_phy_data(void);
void phy_write(uint16_t phy_mask, uint8_t dev_id, uint16_t reg, uint16_t v);
void phy_read(uint8_t phy_id, uint8_t dev_id, uint16_t reg);
void reg_read(uint16_t reg_addr);
void reg_read_m(uint16_t reg_addr);
void reg_write(uint16_t reg_addr);
void reg_write_m(uint16_t reg_addr);
void delay(uint16_t t);
void sleep(uint16_t t);
void write_char(char c);
void print_reg(uint16_t reg);
uint8_t sfp_read_reg(uint8_t reg);
void reg_bit_set(uint16_t reg_addr, char bit);
void reg_bit_clear(uint16_t reg_addr, char bit);

#endif
