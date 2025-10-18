#ifndef __PAGE_IMPL_H__
#define __PAGE_IMPL_H__

void send_counters(char port);
void send_status(void);
void send_vlan(register uint16_t vlan);
void send_basic_info(void);
void send_eee(void);

#endif
