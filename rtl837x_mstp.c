/*
 * MST configuration of the Multiple Spanning Tree Protocol for the RTL837x platform
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_mstp.h"
#include "rtl837x_stp.h"

#pragma codeseg BANK4
#pragma constseg BANK4

__xdata char     mstp_region[MSTP_NAME_LEN + 1];
__xdata uint16_t mstp_revision;
__xdata uint8_t  mstp_digest[16];
__xdata uint8_t  mstp_dg_step;
__xdata uint16_t mstp_lo;
__xdata uint16_t mstp_hi;
__xdata uint8_t  mstp_msti;

__xdata uint8_t  mstp_map[2048];	/* MSTI of each VID, a nibble per VID, VID 0 in the low nibble of byte 0 */
__xdata uint8_t  mstp_inner[16];	/* digest of the inner HMAC pass */
__xdata uint32_t mstp_md[4];
__xdata uint32_t mstp_blk[16];
__xdata uint32_t md5_a, md5_b, md5_c, md5_d, md5_f;
__xdata uint8_t  md5_i, md5_g, md5_s;
__xdata uint16_t mstp_v;
__xdata uint16_t mstp_used;
__xdata uint16_t mstp_count[16];	/* VLANs mapped to each instance */

#define BLK	((__xdata uint8_t *)mstp_blk)

/* Digest of the table that maps every VLAN to the CIST, as computed once. */
static __code const uint8_t mstp_digest_empty[16] = {
	0xac, 0x36, 0x17, 0x7f, 0x50, 0x28, 0x3c, 0xd4,
	0xb8, 0x38, 0x21, 0xd8, 0xab, 0x26, 0xde, 0x62
};

static __code const uint8_t mstp_key[16] = {
	0x13, 0xac, 0x06, 0xa6, 0x2e, 0x47, 0xfd, 0x51,
	0xf9, 0x5d, 0x2b, 0xa2, 0x43, 0xcd, 0x03, 0x46
};

static __code const uint32_t md5_k[64] = {
	0xd76aa478UL, 0xe8c7b756UL, 0x242070dbUL, 0xc1bdceeeUL, 0xf57c0fafUL, 0x4787c62aUL, 0xa8304613UL, 0xfd469501UL,
	0x698098d8UL, 0x8b44f7afUL, 0xffff5bb1UL, 0x895cd7beUL, 0x6b901122UL, 0xfd987193UL, 0xa679438eUL, 0x49b40821UL,
	0xf61e2562UL, 0xc040b340UL, 0x265e5a51UL, 0xe9b6c7aaUL, 0xd62f105dUL, 0x02441453UL, 0xd8a1e681UL, 0xe7d3fbc8UL,
	0x21e1cde6UL, 0xc33707d6UL, 0xf4d50d87UL, 0x455a14edUL, 0xa9e3e905UL, 0xfcefa3f8UL, 0x676f02d9UL, 0x8d2a4c8aUL,
	0xfffa3942UL, 0x8771f681UL, 0x6d9d6122UL, 0xfde5380cUL, 0xa4beea44UL, 0x4bdecfa9UL, 0xf6bb4b60UL, 0xbebfbc70UL,
	0x289b7ec6UL, 0xeaa127faUL, 0xd4ef3085UL, 0x04881d05UL, 0xd9d4d039UL, 0xe6db99e5UL, 0x1fa27cf8UL, 0xc4ac5665UL,
	0xf4292244UL, 0x432aff97UL, 0xab9423a7UL, 0xfc93a039UL, 0x655b59c3UL, 0x8f0ccc92UL, 0xffeff47dUL, 0x85845dd1UL,
	0x6fa87e4fUL, 0xfe2ce6e0UL, 0xa3014314UL, 0x4e0811a1UL, 0xf7537e82UL, 0xbd3af235UL, 0x2ad7d2bbUL, 0xeb86d391UL
};

static __code const uint8_t md5_rot[16] = {
	7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21
};
static __code const uint8_t md5_g0[4] = { 0, 1, 5, 0 };
static __code const uint8_t md5_gstep[4] = { 1, 5, 3, 7 };
static __code const uint32_t md5_iv[4] = {
	0x67452301UL, 0xefcdab89UL, 0x98badcfeUL, 0x10325476UL
};


static uint8_t mstp_get(uint16_t vid) __reentrant
{
	return (mstp_map[vid >> 1] >> ((vid & 1) << 2)) & 0x0f;
}


static void mstp_put(uint16_t vid, uint8_t msti) __reentrant
{
	md5_s = mstp_get(vid);
	if (md5_s == msti)
		return;
	if (md5_s && !--mstp_count[md5_s])
		mstp_used &= ~((uint16_t)1 << md5_s);
	if (msti && !mstp_count[msti]++)
		mstp_used |= (uint16_t)1 << msti;
	if (vid & 1)
		mstp_map[vid >> 1] = (mstp_map[vid >> 1] & 0x0f) | (msti << 4);
	else
		mstp_map[vid >> 1] = (mstp_map[vid >> 1] & 0xf0) | msti;
}


uint8_t mstp_vid_msti(uint16_t vid) __banked
{
	return mstp_get(vid);
}


void mstp_defaults(void) __banked
{
	for (mstp_v = 0; mstp_v < sizeof(mstp_map); mstp_v++)
		mstp_map[mstp_v] = 0;
	for (md5_i = 0; md5_i < 16; md5_i++)
		mstp_count[md5_i] = 0;
	mstp_used = 0;
	mstp_region[0] = 0;
	mstp_revision = 0;
	for (md5_i = 0; md5_i < 16; md5_i++)
		mstp_digest[md5_i] = mstp_digest_empty[md5_i];
	mstp_dg_step = MSTP_DG_DONE;
}


/* Finish the digest before it goes into a BPDU or decides a region match.
 * 132 blocks at once; the incremental steps only pick up the leftovers of a
 * table change made while STP was off. */
void mstp_digest_now(void) __banked
{
	while (mstp_dg_step < MSTP_DG_DONE)
		mstp_digest_step();
}


void mstp_vids_set(void) __banked
{
	for (mstp_v = mstp_lo; mstp_v <= mstp_hi; mstp_v++)
		mstp_put(mstp_v, mstp_msti);
	mstp_dg_step = 0;
}


void mstp_msti_clear(void) __banked
{
	for (mstp_v = 1; mstp_v <= MSTP_VID_MAX; mstp_v++)
		if (mstp_get(mstp_v) == mstp_msti)
			mstp_put(mstp_v, 0);
	mstp_dg_step = 0;
}


static void md5_block(void) __reentrant
{
	md5_a = mstp_md[0];
	md5_b = mstp_md[1];
	md5_c = mstp_md[2];
	md5_d = mstp_md[3];
	for (md5_i = 0; md5_i < 64; md5_i++) {
		if (!(md5_i & 0x0f))
			md5_g = md5_g0[md5_i >> 4];
		else
			md5_g = (md5_g + md5_gstep[md5_i >> 4]) & 0x0f;
		switch (md5_i >> 4) {
		case 0:
			md5_f = (md5_b & md5_c) | (~md5_b & md5_d);
			break;
		case 1:
			md5_f = (md5_d & md5_b) | (~md5_d & md5_c);
			break;
		case 2:
			md5_f = md5_b ^ md5_c ^ md5_d;
			break;
		default:
			md5_f = md5_c ^ (md5_b | ~md5_d);
		}
		md5_f += md5_a + md5_k[md5_i] + mstp_blk[md5_g];
		md5_a = md5_d;
		md5_d = md5_c;
		md5_c = md5_b;
		md5_s = md5_rot[((md5_i >> 4) << 2) | (md5_i & 3)];
		md5_b += (md5_f << md5_s) | (md5_f >> (32 - md5_s));
	}
	mstp_md[0] += md5_a;
	mstp_md[1] += md5_b;
	mstp_md[2] += md5_c;
	mstp_md[3] += md5_d;
}


void mstp_digest_step(void) __banked
{
	if (mstp_dg_step >= MSTP_DG_DONE)
		return;
	for (md5_i = 0; md5_i < 64; md5_i++)
		BLK[md5_i] = 0;
	if (mstp_dg_step == 0 || mstp_dg_step == 130) {
		for (md5_i = 0; md5_i < 4; md5_i++)
			mstp_md[md5_i] = md5_iv[md5_i];
		for (md5_i = 0; md5_i < 64; md5_i++)
			BLK[md5_i] = (md5_i < 16 ? mstp_key[md5_i] : 0) ^ (mstp_dg_step ? 0x5c : 0x36);
	} else if (mstp_dg_step <= 128) {
		mstp_v = (uint16_t)(mstp_dg_step - 1) << 5;
		for (md5_i = 0; md5_i < 32; md5_i++)
			BLK[(md5_i << 1) + 1] = mstp_get(mstp_v + md5_i);
	} else if (mstp_dg_step == 129) {
		BLK[0] = 0x80;
		BLK[57] = 0x02;
		BLK[58] = 0x01;
	} else {
		for (md5_i = 0; md5_i < 16; md5_i++)
			BLK[md5_i] = mstp_inner[md5_i];
		BLK[16] = 0x80;
		BLK[56] = 0x80;
		BLK[57] = 0x02;
	}
	md5_block();
	if (mstp_dg_step == 129) {
		for (md5_i = 0; md5_i < 16; md5_i++)
			mstp_inner[md5_i] = ((__xdata uint8_t *)mstp_md)[md5_i];
	} else if (mstp_dg_step == 131) {
		for (md5_i = 0; md5_i < 16; md5_i++)
			mstp_digest[md5_i] = ((__xdata uint8_t *)mstp_md)[md5_i];
	}
	mstp_dg_step++;
}


void mstp_show(void) __banked
{
	print_string("MSTP region \"");
	print_string_x(mstp_region);
	print_string("\" revision ");
	itoa_short(mstp_revision);
	print_string(" hops ");
	itoa(stp_maxhops);
	print_string("\ndigest ");
	if (mstp_dg_step < MSTP_DG_DONE) {
		print_string(stp_enabled ? "pending\n" : "pending, worked out while STP is on\n");
	} else {
		for (md5_i = 0; md5_i < 16; md5_i++)
			print_byte(mstp_digest[md5_i]);
		write_char('\n');
	}
	for (mstp_msti = 1; mstp_msti <= MSTP_MSTIS; mstp_msti++) {
		if (!((mstp_used >> mstp_msti) & 1))
			continue;
		md5_g = 0;
		for (mstp_lo = 1; mstp_lo <= MSTP_VID_MAX; mstp_lo++) {
			if (mstp_get(mstp_lo) != mstp_msti)
				continue;
			mstp_hi = mstp_lo;
			while (mstp_hi < MSTP_VID_MAX && mstp_get(mstp_hi + 1) == mstp_msti)
				mstp_hi++;
			if (!md5_g) {
				print_string("MSTI ");
				itoa(mstp_msti);
				print_string(" vlan ");
				md5_g = 1;
			} else {
				write_char(',');
			}
			itoa_short(mstp_lo);
			if (mstp_hi != mstp_lo) {
				write_char('-');
				itoa_short(mstp_hi);
			}
			mstp_lo = mstp_hi;
		}
		if (md5_g)
			write_char('\n');
	}
}
