#ifndef __HELLO_WORLD_H__
#define __HELLO_WORLD_H__

/* Since this file will be included by uip.h, we cannot include uip.h
   here. But we might need to include uipopt.h if we need the u8_t and
   u16_t datatypes. */
#include "uipopt.h"

/* Next, we define the uip_tcp_appstate_t datatype. This is the state
   of our application, and the memory required for this state is
   allocated together with each TCP connection. One application state
   for each TCP connection. */
typedef struct httpd_state {
   char transmitted;
} uip_tcp_appstate_t;

/* Finally we define the application function to be called by uIP. */
void httpd_appcall(void);
#ifndef UIP_APPCALL
#define UIP_APPCALL httpd_appcall
#endif /* UIP_APPCALL */

void httpd_init(void) __banked;

#endif
