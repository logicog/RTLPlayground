/*
 * test_poe.c - poe/tps23880.c against a simulated TPS23880.
 *
 * The driver is compiled UNMODIFIED; the I2C master is the register mock's
 * hw_i2c_hook, the flash a buffer holding a synthetic image in the format
 * tools/poe/extract_tps23880.py writes (TI's image is not needed here).
 * The simulated chip: two I2C devices of 4 channels, a register file each,
 * and the SRAM download of the datasheet (0x60 control, 0x61 data); after
 * 0x60 = 0x18 register 0x41 reads the version only if the parity and SRAM
 * images arrived complete and in order.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "machine.h"
#include "poe/poe.h"
#include <test/support.h>
#include <test/hw_mock.h>

const struct machine machine;
struct machine_runtime machine_detected = { .isRTL8373 = 0 };
void print_byte(uint8_t v) { (void)v; }
void print_short(uint16_t v) { (void)v; }
void delay(uint16_t t) { (void)t; }

/* RESET pin */
static int reset_low, reset_pulses;
void gpio_output_setup(uint8_t pin, uint8_t v) { if (pin == POE_RESET_GPIO && !v) { reset_low = 1; reset_pulses++; } }
void gpio_input_setup(uint8_t pin) { if (pin == POE_RESET_GPIO) reset_low = 0; }

/* Flash with the image at POE_IMAGE_LOCATION */
struct flash_region_t flash_region;
static uint8_t flash[0x10000];
void flash_read_bulk(uint8_t *dst)
{
	memcpy(dst, flash + (flash_region.addr - POE_IMAGE_LOCATION), flash_region.len);
}

#define SRAM_LEN 0x4000
#define PARITY_LEN 0x800
static uint8_t img_parity[PARITY_LEN], img_sram[SRAM_LEN];

static void make_image(uint8_t version)
{
	uint32_t sum = 0;
	for (int i = 0; i < PARITY_LEN; i++) img_parity[i] = (uint8_t)(i * 7 + 3);
	for (int i = 0; i < SRAM_LEN; i++) img_sram[i] = (uint8_t)(i * 13 + (i >> 8));
	memset(flash, 0xff, sizeof(flash));
	memcpy(flash, "TPSI", 4);
	flash[4] = SRAM_LEN & 0xff; flash[5] = SRAM_LEN >> 8;
	flash[6] = PARITY_LEN & 0xff; flash[7] = PARITY_LEN >> 8;
	flash[8] = version;
	flash[9] = flash[10] = flash[11] = 0;
	memcpy(flash + 16, img_parity, PARITY_LEN);
	memcpy(flash + 16 + PARITY_LEN, img_sram, SRAM_LEN);
	for (int i = 0; i < PARITY_LEN + SRAM_LEN; i++) sum += flash[16 + i];
	flash[12] = sum; flash[13] = sum >> 8; flash[14] = sum >> 16; flash[15] = sum >> 24;
}

/* The simulated TPS23880 */
static uint8_t regs[2][256];
static uint8_t got_parity[PARITY_LEN], got_sram[SRAM_LEN];
static int n_parity, n_sram, bad_order, writes, present = 1;
static uint8_t sram_mode;	// 0x60 value the data goes to

static void tps_power_on(void)
{
	memset(regs, 0, sizeof(regs));
	n_parity = n_sram = bad_order = writes = 0;
	sram_mode = 0;
}

static uint8_t i2c(uint32_t ctrl)
{
	uint8_t dev = (ctrl >> 3) & 0x7f, len = ((ctrl >> 16) & 0xf) + 1;
	uint8_t reg = hw_reg_get(RTL837X_REG_I2C_IN) & 0xff;

	if (!present || (dev != POE_TPS_ADDR && dev != POE_TPS_ADDR + 1) || reset_low)
		return 0;
	if (((ctrl >> 8) & 0xfc) != POE_I2C_BUS)	/* I2CBUS(): bits 7:2 of CTRL 15:8 */
		return 0;	// wrong bus
	uint8_t *r = regs[dev - POE_TPS_ADDR];
	if (ctrl & I2C_CTRL_WRITE) {
		uint8_t v = hw_reg_get(RTL837X_REG_I2C_OUT) & 0xff;
		writes++;
		if (len != 1)
			return 0;
		if (reg == 0x61 && dev == POE_TPS_ADDR) {
			if (sram_mode == 0xc4 && n_parity < PARITY_LEN)
				got_parity[n_parity++] = v;
			else if (sram_mode == 0xc0 && n_sram < SRAM_LEN)
				got_sram[n_sram++] = v;
			else
				bad_order++;
			return 1;
		}
		if (reg == 0x60 && dev == POE_TPS_ADDR) {
			if (v == 0xc4 && n_parity) bad_order++;
			if (v == 0xc0 && n_parity != PARITY_LEN) bad_order++;
			sram_mode = v;
			if (v == 0x18 && n_parity == PARITY_LEN && n_sram == SRAM_LEN && !bad_order
			    && !memcmp(got_parity, img_parity, PARITY_LEN) && !memcmp(got_sram, img_sram, SRAM_LEN))
				r[0x41] = flash[8];
		}
		r[reg] = v;
		return 1;
	}
	uint32_t w = 0;
	for (int i = 0; i < len && i < 4; i++)
		w |= (uint32_t)r[(uint8_t)(reg + i)] << (8 * i);
	hw_reg_set(RTL837X_REG_I2C_OUT, w);
	return 1;
}

int main(void)
{
	hw_reset();
	hw_i2c_hook = i2c;

	printf("[test] bring-up downloads the image and configures the ports\n");
	make_image(1);
	tps_power_on();
	CHECK(poe_bringup() == 0, "poe_bringup succeeds");
	CHECK(reset_pulses == 1 && !reset_low, "one reset pulse, RESET released");
	CHECK(n_parity == PARITY_LEN && !memcmp(got_parity, img_parity, PARITY_LEN), "parity image arrived complete");
	CHECK(n_sram == SRAM_LEN && !memcmp(got_sram, img_sram, SRAM_LEN), "SRAM image arrived complete");
	CHECK(!bad_order && regs[0][0x41] == 1 && poe_fw == 1, "in order, the chip runs version 1");
	CHECK(regs[0][0x29] == 0x33 && regs[1][0x29] == 0x33, "2-pair 30 W on both devices");
	CHECK(regs[0][0x12] == 0 && regs[1][0x12] == 0 && regs[0][0x14] == 0 && regs[1][0x14] == 0,
	      "every port off until the config enables it");
	CHECK(regs[0][0x19] == 0xf0 && regs[1][0x19] == 0xf0, "and powered off");
	CHECK(poe_port_set(4, 1) == 0 && regs[0][0x12] == 0xc0 && (regs[0][0x14] & 0x88) == 0x88,
	      "`poe port 4 on`: channel 4 auto, detection and classification on");
	CHECK(regs[1][0x12] == 0, "port 8, the uplink, stays off");
	tps_power_on();
	CHECK(poe_bringup() == 0 && regs[0][0x12] == 0xc0 && regs[0][0x19] == 0x70,
	      "`poe load` after a power cut: port 4 enabled again, the rest off");
	poe_port_set(4, 0);

	printf("[test] a broken image is not downloaded\n");
	make_image(1);
	flash[16 + 100] ^= 1;
	tps_power_on();
	reset_pulses = 0;
	CHECK(poe_bringup() == 1 && writes == 0 && reset_pulses == 0, "checksum mismatch: nothing written, no reset");
	make_image(1);
	flash[0] = 'X';
	CHECK(poe_bringup() == 1 && writes == 0, "no image: nothing written");

	printf("[test] a wrong version is reported\n");
	make_image(1);
	tps_power_on();
	flash[8] = 2;	/* the version is outside the checksum; the chip reports the header's */
	CHECK(poe_bringup() == 0, "version 2 image runs as version 2");
	tps_power_on();
	img_sram[5] ^= 0xff;	/* the chip sees other bytes than the image: no version */
	CHECK(poe_bringup() == 2, "an image that does not run is reported");
	make_image(1);

	printf("[test] boot leaves a running chip alone\n");
	tps_power_on();
	CHECK(poe_bringup() == 0, "loaded once");
	poe_port_set(4, 1);
	poe_port_set(9, 1);
	poe_port_set(4, 0);	/* the switch reboots and forgets: poe_init reads the chip */
	poe_port_set(9, 0);
	regs[0][0x12] = 0xc0;	/* channel 4 auto */
	regs[1][0x12] = 0x0c;	/* channel 6 (port 9) auto */
	reset_pulses = 0;
	n_parity = n_sram = 0;
	poe_init();
	CHECK(reset_pulses == 0 && n_sram == 0 && poe_fw == 1, "poe_init on a running chip: no reset, no download");
	CHECK(regs[0][0x12] == 0 && regs[1][0x12] == 0, "and everything it powered is off");
	poe_port_num = 4; poe_get_port();
	CHECK(!poe_st_admin, "and shows as disabled until the config enables it");
	poe_global_set(0);
	tps_power_on();
	poe_init();
	CHECK(reset_pulses == 1 && n_sram == SRAM_LEN && poe_fw == 1 && regs[0][0x12] == 0,
	      "after a power cut poe_init downloads, ports off");

	printf("[test] ports on and off\n");
	poe_global_set(1);
	CHECK(poe_port_set(5, 0) == 1 && poe_port_set(6, 1) == 1, "ports 5 and 6 have no PoE");
	CHECK(poe_port_set(9, 0) == 0, "port 9 off");
	CHECK(regs[1][0x12] == 0x33 && regs[1][0x19] == 0x20, "channel 6: mode off, powered off");
	poe_port_num = 9; poe_get_port();
	CHECK(poe_st_valid && !poe_st_admin, "port 9 shows disabled");
	CHECK(poe_port_set(9, 1) == 0 && regs[1][0x12] == 0x3f && (regs[1][0x14] & 0x22) == 0x22
	      && regs[1][0x18] == 0x22, "port 9 on: auto mode, detection restarted");
	poe_global_set(0);
	CHECK(regs[0][0x12] == 0 && regs[1][0x12] == 0, "global off: all channels off");
	poe_global_set(1);
	CHECK(regs[0][0x12] == 0xff && regs[1][0x12] == 0x3f, "global on: wired channels auto, channel 8 not");

	printf("[test] status\n");
	regs[1][0x10] = 0x20 | 0x02;	/* channel 6 (device 2, channel 2): power good */
	regs[1][0x0d] = 0x44;		/* class 4, detection valid */
	regs[1][0x34] = 0x00; regs[1][0x35] = 0x08;	/* 2048 * 61.039 uA = 125 mA */
	regs[1][0x36] = 0x80; regs[1][0x37] = 0x38;	/* 14464 * 3.662 mV = 52.967 V */
	poe_port_num = 9; poe_get_port();
	CHECK(poe_st_valid && poe_st_admin && poe_st_on && poe_st_class == 4, "port 9: on, class 4");
	CHECK(poe_st_ma == 125 && poe_st_mv == 52967, "125 mA at 52.967 V");
	poe_port_num = 1; poe_get_port();
	CHECK(poe_st_valid && !poe_st_on && poe_st_class == 0xff && poe_st_ma == 0, "port 1: nothing connected");
	poe_port_num = 5; poe_get_port();
	CHECK(!poe_st_valid, "port 5 has no PoE");
	regs[0][0x2e] = 0x80; regs[0][0x2f] = 0x39;	/* 14720 -> 53.904 V */
	regs[0][0x2c] = 100;				/* 100 * 0.652 - 20 = 45 C */
	CHECK(poe_get_global() == 0 && poe_vin_mv == 53904 && poe_temp == 45, "53.9 V in, 45 C");
	present = 0;
	CHECK(poe_get_global() == 1, "no answer is reported");

	printf("PoE: %d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}
