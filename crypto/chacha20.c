#include "rtl837x_common.h"
#include "chacha.h"

void chacha_20(void);
void chacha_update(void);
void chacha_count(void);

// generate a block of ChaCha20 keystream as per RFC7539


__xdata struct chacha20_t __at(0x7000) chacha20;
__xdata uint8_t plaintext[256];
__xdata uint8_t cyphertext[256];

void chacha20_print_block(void)
{
        for (uint8_t i=0; i < 64; i++) {
            print_byte(*(uint8_t * __xdata)(chacha20.wstate + i));
	    if (i%4 == 3)
		    write_char(' ');
        }
}


void chacha20_encrypt(void)
{
	__xdata uint8_t *p = chacha20.plaintext;

	while (chacha20.length) {
		register uint8_t i;
		chacha_count();
		memcpy(chacha20.wstate, chacha20.wstate + 64, 64);
#ifdef DEBUG
		chacha20_print_block(); write_char('\n');
#endif
		chacha_20();
		chacha_update();
#ifdef DEBUG
		chacha20_print_block(); write_char('\n');
#endif
		for (i = 0; i < ((chacha20.length > 64) ? 64 : chacha20.length); i++)
			*chacha20.cyphertext++ = *p++ ^ chacha20.wstate[i];
		chacha20.length -= chacha20.length > 64 ? 64 : chacha20.length;
#ifdef DEBUG
		print_string("\n  round done\n");
#endif
	};
}


// Test the example of RFC 7539 Section 2.4.2
void chacha20_test(void)
{
	__code uint8_t chacha_c[16] =
		{ 0x61, 0x70, 0x78, 0x65,  0x33, 0x20, 0x64, 0x6e,
		  0x79, 0x62, 0x2d, 0x32,  0x6b, 0x20, 0x65, 0x74 };

	__code uint8_t key[32] =
		{ 0x03, 0x02, 0x01, 0x00,  0x07, 0x06, 0x05, 0x04,
		  0x0b, 0x0a, 0x09, 0x08,  0x0f, 0x0e, 0x0d, 0x0c,
		  0x13, 0x12, 0x11, 0x10,  0x17, 0x16, 0x15, 0x14,
		  0x1b, 0x1a, 0x19, 0x18,  0x1f, 0x1e, 0x1d, 0x1c };
	
	__code uint8_t nonce[12] = {
		  0x00, 0x00, 0x00, 0x00,  0x4a, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00 };

	__code uint8_t sunscreen[] = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for " \
				     "the future, sunscreen would be it.";

/*
	Ciphertext Sunscreen (from RFC 7539 Section 2.4.2):
	000  6e 2e 35 9a 25 68 f9 80 41 ba 07 28 dd 0d 69 81  n.5.%h..A..(..i.
	016  e9 7e 7a ec 1d 43 60 c2 0a 27 af cc fd 9f ae 0b  .~z..C`..'......
	032  f9 1b 65 c5 52 47 33 ab 8f 59 3d ab cd 62 b3 57  ..e.RG3..Y=..b.W
	048  16 39 d6 24 e6 51 52 ab 8f 53 0c 35 9f 08 61 d8  .9.$.QR..S.5..a.
	064  07 ca 0d bf 50 0d 6a 61 56 a3 8e 08 8a 22 b6 5e  ....P.jaV....".^
	080  52 bc 51 4d 16 cc f8 06 81 8c e9 1a b7 79 37 36  R.QM.........y76
	096  5a f9 0b bf 74 a3 5b e6 b4 0b 8e ed f2 78 5e 42  Z...t.[......x^B
	112  87 4d 
*/
	strcpy(plaintext, sunscreen);
	memcpyc(chacha20.constant, chacha_c, 16);
	memcpyc(chacha20.key, key, 32);
	chacha20.cnt = 0;
	memcpyc(chacha20.nonce, nonce, 12);
	chacha20.plaintext = plaintext;
	chacha20.length = strlen_x(plaintext);
	chacha20.cyphertext = cyphertext;

	print_string("Encrypting...\n");
	chacha20_encrypt();
	print_string("Cypertext:\n");
	uint16_t len = strlen_x(plaintext);
	for (uint8_t i = 0; i < len; i++) {
		print_byte(cyphertext[i]); write_char(' ');
	}
}
