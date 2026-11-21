#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl8221_ptp.h"
#include "rtl837x_port.h"
#include "chacha.h"
#include "machine.h"

#pragma codeseg BANK4
#pragma constseg BANK4

extern __code const struct machine machine;

void chacha_20(void);
void chacha_update(void);
void chacha_count(void);

// Random number generators

extern __xdata struct chacha20_t __at(0xbf00) chacha20;
extern __xdata uint8_t sfr_data[4];

__xdata uint8_t plaintext[64];
__xdata uint8_t cyphertext[64];
__xdata uint8_t random_64[8];

/*
 * Create 32 random number in sfr_data
 */
void get_random_32(void)
{
	// In order to get a new random numner, this bit has to be set each time!
	reg_bit_set(RTL837X_RLDP_RLPP, RLDP_RND_EN);
	reg_read_m(RTL837X_RAND_NUM0);
}


void get_random_64(void) __banked
{
	__code uint8_t chacha_c[16] =
		{ 0x61, 0x70, 0x78, 0x65,  0x33, 0x20, 0x64, 0x6e,
		  0x79, 0x62, 0x2d, 0x32,  0x6b, 0x20, 0x65, 0x74 };
	__code uint8_t key[32] =
		{ 0x57, 0x25, 0xfe, 0x5b,  0x84, 0xfa, 0xa8, 0x34,
		  0x50, 0x3d, 0xa9, 0x02,  0x22, 0x0b, 0x68, 0x47,
		  0x54, 0xa4, 0xdf, 0x1e,  0x67, 0xff, 0xff, 0xa3,
		  0xaf, 0xac, 0x2f, 0x92,  0xd4, 0xd2, 0x66, 0x43 };
	
	__code uint8_t nonce[12] = {
		  0x40, 0xa4, 0xdf, 0x1e,  0x5c, 0xae, 0x20, 0x99,
		  0x02, 0x4f, 0x03, 0x06 };
	uint8_t j;
		  
	// Gather Randomness from various sources,
	// the uptime seconds and packet counters are mostly 0s, though, so XOR them with the linear shift register
	get_random_32();
	plaintext[0] = SFR_DATA_24;
	plaintext[1] = SFR_DATA_16;
	plaintext[2] = SFR_DATA_8;
	plaintext[3] = SFR_DATA_0;
	machine.ptp[0].read(&machine.ptp[0]);
	plaintext[0] ^= ptp_sec[2];
	plaintext[1] ^= ptp_sec[3];
	plaintext[2] ^= ptp_sec[4];
	plaintext[3] ^= ptp_sec[5];
	plaintext[4] = ptp_nsec[0];
	plaintext[5] = ptp_nsec[1];
	plaintext[6] = ptp_nsec[2];
	plaintext[7] = ptp_nsec[3];

	j = 8;
	for (uint8_t i = MACHINE_MIN_PORT; i < MACHINE_MIN_PORT + 6; i++) {
		get_random_32();
		plaintext[j] = SFR_DATA_24;
		plaintext[j+1] = SFR_DATA_16;
		plaintext[j+2] = SFR_DATA_8;
		plaintext[j+3] = SFR_DATA_0;
		STAT_GET(STAT_COUNTER_RX_PKTS, i);
		reg_read(RTL837X_STAT_V_LOW);
		plaintext[j++] ^= SFR_DATA_24;
		plaintext[j++] ^= SFR_DATA_16;
		plaintext[j++] ^= SFR_DATA_8;
		plaintext[j++] ^= SFR_DATA_0;
		get_random_32();
		plaintext[j] = SFR_DATA_24;
		plaintext[j+1] = SFR_DATA_16;
		plaintext[j+2] = SFR_DATA_8;
		plaintext[j+3] = SFR_DATA_0;
		STAT_GET(STAT_COUNTER_TX_PKTS, i);
		reg_read(RTL837X_STAT_V_LOW);
		plaintext[j++] ^= SFR_DATA_24;
		plaintext[j++] ^= SFR_DATA_16;
		plaintext[j++] ^= SFR_DATA_8;
		plaintext[j++] ^= SFR_DATA_0;
	}
	
	get_random_32();
	plaintext[56] = SFR_DATA_24;
	plaintext[57] = SFR_DATA_16;
	plaintext[58] = SFR_DATA_8;
	plaintext[59] = SFR_DATA_0;
	machine.ptp[machine.n_ptp - 1].read(&machine.ptp[machine.n_ptp - 1]);
	plaintext[56] ^= ptp_sec[2];
	plaintext[57] ^= ptp_sec[3];
	plaintext[58] ^= ptp_sec[4];
	plaintext[59] ^= ptp_sec[5];
	plaintext[60] = ptp_nsec[0];
	plaintext[61] = ptp_nsec[1];
	plaintext[62] = ptp_nsec[2];
	plaintext[63] = ptp_nsec[3];

	memcpyc(chacha20.constant, chacha_c, 16);
	memcpyc(chacha20.key, key, 32);
	chacha20.cnt[0] = chacha20.cnt[1] = chacha20.cnt[2] = chacha20.cnt[3] = 0;
	memcpyc(chacha20.nonce, nonce, 12);

	chacha20.plaintext = plaintext;
	chacha20.length = 64;
	chacha20.cyphertext = cyphertext;
	chacha20_encrypt();

	for(uint8_t i=0; i < 8; i++)
		random_64[i] = 0;

	for (uint8_t i=0; i < 64; i++)
		random_64[i & 0x7] ^= cyphertext[i];

}
