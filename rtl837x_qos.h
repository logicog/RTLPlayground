#ifndef _RTL837X_QOS_H_
#define _RTL837X_QOS_H_

#include <stdint.h>

#define QOS_QUEUES		8
#define QOS_PRIORITIES		8
#define QOS_WEIGHT_MAX		127

void qos_dot1q_set(uint8_t on) __banked;
void qos_pcp_set(uint8_t pcp, uint8_t priority) __banked;
void qos_queue_set(uint8_t port, uint8_t queue, uint8_t weight) __banked;
void qos_show(void) __banked;

#endif
