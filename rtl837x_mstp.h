#ifndef _RTL837X_MSTP_H_
#define _RTL837X_MSTP_H_

#include <stdint.h>

#define MSTP_MSTIS	15
#define MSTP_NAME_LEN	32
#define MSTP_VID_MAX	4094
#define MSTP_DG_DONE	132

extern __xdata char     mstp_region[MSTP_NAME_LEN + 1];
extern __xdata uint16_t mstp_revision;
extern __xdata uint8_t  mstp_digest[16];
extern __xdata uint8_t  mstp_dg_step;	/* next block of the digest computation, MSTP_DG_DONE when current */
extern __xdata uint16_t mstp_lo;	/* arguments of mstp_vids_set() and mstp_msti_clear() */
extern __xdata uint16_t mstp_hi;
extern __xdata uint8_t  mstp_msti;
extern __xdata uint16_t mstp_used;	/* bit per instance with at least one VLAN */

void mstp_defaults(void) __banked;
void mstp_vids_set(void) __banked;
void mstp_msti_clear(void) __banked;
uint8_t mstp_vid_msti(uint16_t vid) __banked;
void mstp_digest_step(void) __banked;
void mstp_digest_now(void) __banked;
void mstp_show(void) __banked;

#endif
