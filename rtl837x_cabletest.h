#ifndef _RTL837X_CABLETEST_H_
#define _RTL837X_CABLETEST_H_

#include <stdint.h>
#include <stdbool.h>

#define CABLE_RESULT_UNSPEC		0
#define CABLE_RESULT_OK			1
#define CABLE_RESULT_OPEN		2
#define CABLE_RESULT_SAME_SHORT		3
#define CABLE_RESULT_CROSS_SHORT	4
#define CABLE_RESULT_MISMATCH		5

void rtl_phy_cable_test_start(uint8_t port) __banked;
bool rtl_phy_cable_test_get_status(uint8_t port) __banked;

void cabletest_run(void) __banked;
void cabletest_show(void) __banked;
void cabletest_tick(void) __banked;

#endif
