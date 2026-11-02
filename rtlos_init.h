#ifndef _RTL837X_INIT_H_
#define _RTL837X_INIT_H_

#if (SOC_FAMILY == RTL836X)
void rtl8370_init(void) __banked;
#elif (SOC_FAMILY == RTL837X)
void rtl8372_init(void) __banked;
void rtl8373_init(void) __banked;
#endif

#endif
