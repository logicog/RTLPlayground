#ifndef _RTL837X_PTP_H_
#define _RTL837X_PTP_H_

#include <stdint.h>
#define PTP_CLK_FREQ 0x10000000
#define INT_PHY_MIN_PORT 4
#define INT_PHY_MAX_PORT 7

void ptp_init(void) __banked;
void ptp_rtl8224_init(void) __banked;
void ptp_rtl8221_init(uint8_t phy) __banked;
void ptp_get_time(void) __banked;
void ptp_rtl8224_get_time(void) __banked;
void ptp_rtl8221_get_time(uint8_t phy) __banked;
void ptp_read_tx_imestamp(void) __banked;
void ptp_get_clkout(void) __banked;
void ptp_set_time(void) __banked;
void ptp_rtl8224_set_time(void) __banked;
void ptp_rtl8221_set_time(uint8_t phy) __banked;

#endif
