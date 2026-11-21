#ifndef _RTL837X_PTP_H_
#define _RTL837X_PTP_H_

#include <stdint.h>
#define PTP_CLK_FREQ 0x10000000
#define INT_PHY_MIN_PORT 4
#define INT_PHY_MAX_PORT 7

extern __xdata uint8_t ptp_sec[6]; // 48bit time in seconds (BE)
extern __xdata uint8_t ptp_nsec[4]; // 30bit of nsec (BE)

void test_f(void);

struct ptp_module {
	uint8_t phy;
	void (*init)(__code const struct ptp_module *self) __banked;
	void (*read)(__code const struct ptp_module *self) __banked;
	void (*get)(__code const struct ptp_module *self) __banked;
	void (*set)(__code const struct ptp_module *self) __banked;
};

void ptp_init(__code const struct ptp_module *mod) __banked;
void ptp_read_time(__code const struct ptp_module *mod) __banked;
void ptp_get_time(__code const struct ptp_module *mod) __banked;
void ptp_read_tx_imestamp(void) __banked;
void ptp_get_clkout(void) __banked;
void ptp_set_time(__code const struct ptp_module *mod) __banked;

// WARNING: This needs to be in the same bank for all modules!!!
void ptp_print_time(void);

#endif
