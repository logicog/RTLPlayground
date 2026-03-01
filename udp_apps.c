
#include "stdint.h"
#include "udp_apps.h"

static uint8_t callbacknr;

void udp_callbacks(void)
{
	// Calling more than one callback from the same UIP_UDP_APPCALL does not work; we use a simple dispatcher
	if (callbacknr == 0) {
		dhcp_callback();
		callbacknr = 1;
	} else  {
		syslog_callback();
		callbacknr = 0;
	}
}