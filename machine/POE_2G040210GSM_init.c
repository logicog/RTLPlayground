/*
 * Boot hook for the MokerLink POE-2G040210GSM (2M-PCB49-V1.1).
 *
 * 1. LED pad 6 drives port 6's LED (right SFP), and pads 0-7 are disabled at
 *    reset.
 *
 * 2. PoE. The RTL8238B on the PoE daughterboard (I2C 0x20 and 0x21 on GPIO47
 *    SDA / GPIO46 SCL) does nothing until an image is uploaded to it, which
 *    the stock firmware does at every boot. This repeats the stock sequence,
 *    taken from a disassembly of the stock flash:
 *      - reset: write 0x20 to register 0x1a, wait 100 ms
 *      - write 03 00 00 00 to 0xec, wait 1 ms, read 0xec
 *      - if bit 2 of its first byte is clear: write 00 00 20 39 to 0x18,
 *        wait 10 ms, write 03 00 00 00 to 0xec again
 *      - read 0xe8: 2 picks image 1, anything else image 0
 *      - write the whole image to register 0xf4 as one I2C transfer
 *      - wait 10 ms, read 0xec: bit 5 of its first byte set = loaded
 *    The registers are bytes, laid out like a TI TPS23861's (0x1a is the
 *    reset register), and multi-byte accesses cover consecutive registers in
 *    address order, in both directions.
 *
 *    The image is the PoE controller vendor's firmware and isn't part of this
 *    repository. tools/add_pse_image.py puts it into a built firmware image at
 *    PSE_IMAGE_START; without it nothing is written to the chip and PoE stays
 *    off. The image is one transfer of 8 or 21 KB, more than the RTL8372's
 *    I2C master moves (16 bytes), so this bit-bangs the two GPIOs, as the stock
 *    firmware does. The lines are driven open drain: the output latches stay 0
 *    and only the direction bits change.
 *
 *    Image region, PSE_IMAGE_START: "PSE1", the lengths of image 0 and image 1
 *    (uint32, little endian), image 0, image 1. It sits right after the last
 *    code bank and below the web UI, inside the span a web update rewrites
 *    (0x00000 up to CONFIG_START).
 */
#include "machine_init_defs.h"
#include "rtl837x_flash.h"

extern __xdata uint8_t sfr_data[4];
extern __xdata struct flash_region_t flash_region;
extern __xdata uint8_t flash_buf[FLASH_BUF_SIZE];

#define PSE_IMAGE_START	0x34000
#define PSE_IMAGE_MAX	0xc000
#define PSE_HDR_LEN	12
#define PSE_ADDR	0x20

// GPIO46 and GPIO47 are bits 14 and 15 of the GPIO 32-63 registers, which
// are bits 6 and 7 of sfr_data[2]
#define SCL_BIT		0x40
#define SDA_BIT		0x80

#define I2C_RETRIES	10

static __xdata uint8_t gpio_dir[4];
static __xdata uint8_t i2c_low;
static __xdata uint8_t pse_buf[4];
static __xdata uint32_t img_len[2];

static void i2c_wait(void)
{
	__xdata volatile uint8_t i;

	for (i = 0; i < 4; i++)
		;
}

// Drive the lines in i2c_low low and release the others
static void i2c_lines(void)
{
	sfr_data[0] = gpio_dir[0];
	sfr_data[1] = gpio_dir[1];
	sfr_data[2] = (gpio_dir[2] & ~(SCL_BIT | SDA_BIT)) | i2c_low;
	sfr_data[3] = gpio_dir[3];
	reg_write_m(RTL837X_REG_GPIO_32_63_DIRECTION);
	i2c_wait();
}

static void scl_low(void)
{
	i2c_low |= SCL_BIT;
	i2c_lines();
}

static void scl_high(void)
{
	__xdata uint16_t t = 1000;

	i2c_low &= ~SCL_BIT;
	i2c_lines();
	// The slave may stretch the clock
	while (!gpio_pin_test(GPIO46_I2C_SCL0) && --t)
		;
}

static void sda_set(uint8_t v)
{
	if (v)
		i2c_low &= ~SDA_BIT;
	else
		i2c_low |= SDA_BIT;
	i2c_lines();
}

// A start condition, also used as the repeated start
static void i2c_start(void)
{
	sda_set(1);
	scl_high();
	sda_set(0);
	scl_low();
}

static void i2c_stop(void)
{
	sda_set(0);
	scl_high();
	sda_set(1);
}

// Returns true if the slave acknowledged the byte
static bool i2c_write_byte(uint8_t b)
{
	__xdata uint8_t i;
	__xdata bool ack;

	for (i = 0; i < 8; i++) {
		sda_set(b & 0x80);
		b <<= 1;
		scl_high();
		scl_low();
	}
	sda_set(1);
	scl_high();
	ack = !gpio_pin_test(GPIO47_I2C_SDA0);
	scl_low();
	return ack;
}

static uint8_t i2c_read_byte(bool ack)
{
	__xdata uint8_t i;
	__xdata uint8_t b = 0;

	sda_set(1);
	for (i = 0; i < 8; i++) {
		scl_high();
		b <<= 1;
		if (gpio_pin_test(GPIO47_I2C_SDA0))
			b |= 1;
		scl_low();
	}
	sda_set(!ack);
	scl_high();
	scl_low();
	sda_set(1);
	return b;
}

static void i2c_setup(void)
{
	// GPIO46/47 as GPIO: PIN_MUX_1 bits 7-10 clear (they are at reset)
	reg_read_m(RTL837X_PIN_MUX_1);
	sfr_mask_data(0, 0x80, 0x00);
	sfr_mask_data(1, 0x07, 0x00);
	reg_write_m(RTL837X_PIN_MUX_1);

	reg_bit_clear(RTL837X_REG_GPIO_32_63_OUTPUT, 14);
	reg_bit_clear(RTL837X_REG_GPIO_32_63_OUTPUT, 15);
	reg_bit_clear(RTL837X_REG_GPIO_32_63_DIRECTION, 14);
	reg_bit_clear(RTL837X_REG_GPIO_32_63_DIRECTION, 15);
	reg_read_m(RTL837X_REG_GPIO_32_63_DIRECTION);
	gpio_dir[0] = sfr_data[0];
	gpio_dir[1] = sfr_data[1];
	gpio_dir[2] = sfr_data[2];
	gpio_dir[3] = sfr_data[3];
	i2c_low = 0;
	i2c_lines();
}

// Start a transfer and send the device and register address, with the stock
// firmware's retries. Returns false if the device doesn't answer.
static bool pse_begin(uint8_t reg)
{
	__xdata uint8_t i;

	for (i = 0; i < I2C_RETRIES; i++) {
		i2c_start();
		if (i2c_write_byte(PSE_ADDR << 1) && i2c_write_byte(reg))
			return true;
		i2c_stop();
	}
	return false;
}

static void pse_set(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	pse_buf[0] = a;
	pse_buf[1] = b;
	pse_buf[2] = c;
	pse_buf[3] = d;
}

// Write the 4 bytes in pse_buf to a register, in that order
static bool pse_write(uint8_t reg)
{
	__xdata uint8_t i;

	if (!pse_begin(reg))
		return false;
	for (i = 0; i < 4; i++) {
		if (!i2c_write_byte(pse_buf[i])) {
			i2c_stop();
			return false;
		}
	}
	i2c_stop();
	return true;
}

// Reset the chip: one byte, 0x20, to the reset register 0x1a. The chip may not
// acknowledge the byte as it resets, so only the address phase has to succeed.
static bool pse_reset(void)
{
	if (!pse_begin(0x1a))
		return false;
	i2c_write_byte(0x20);
	i2c_stop();
	return true;
}

// Read 4 bytes from a register into pse_buf, in the order received
static bool pse_read(uint8_t reg)
{
	__xdata uint8_t i;

	if (!pse_begin(reg))
		return false;
	i2c_start();
	if (!i2c_write_byte((PSE_ADDR << 1) | 1)) {
		i2c_stop();
		return false;
	}
	for (i = 0; i < 4; i++)
		pse_buf[i] = i2c_read_byte(i < 3);
	i2c_stop();
	return true;
}

static uint32_t le32(__xdata uint8_t *p)
{
	return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool pse_image_header(void)
{
	flash_region.addr = PSE_IMAGE_START;
	flash_region.len = PSE_HDR_LEN;
	flash_read_bulk(flash_buf);
	if (flash_buf[0] != 'P' || flash_buf[1] != 'S' || flash_buf[2] != 'E' || flash_buf[3] != '1')
		return false;
	img_len[0] = le32(flash_buf + 4);
	img_len[1] = le32(flash_buf + 8);
	if (img_len[0] > PSE_IMAGE_MAX || img_len[1] > PSE_IMAGE_MAX
	    || PSE_HDR_LEN + img_len[0] + img_len[1] > PSE_IMAGE_MAX)
		return false;
	return img_len[0] || img_len[1];
}

// Send an image from flash to register 0xf4 as one transfer. Like the stock
// firmware, a data byte without an acknowledge doesn't stop the transfer.
static bool pse_write_image(uint32_t addr, uint32_t len)
{
	__xdata uint32_t a = addr;
	__xdata uint32_t left = len;
	__xdata uint16_t n, i;

	if (!pse_begin(0xf4))
		return false;
	while (left) {
		n = left > FLASH_BUF_SIZE ? FLASH_BUF_SIZE : left;
		flash_region.addr = a;
		flash_region.len = n;
		flash_read_bulk(flash_buf);
		for (i = 0; i < n; i++)
			i2c_write_byte(flash_buf[i]);
		a += n;
		left -= n;
	}
	i2c_stop();
	return true;
}

static void pse_load(void)
{
	__xdata uint8_t img;
	__xdata uint32_t addr;

	if (!pse_image_header()) {
		print_string("PSE: no RTL8238B image in this firmware, PoE stays off\n");
		return;
	}
	i2c_setup();

	if (!pse_reset()) {
		print_string("PSE: no answer from the RTL8238B\n");
		return;
	}
	delay(100);

	pse_set(0x03, 0x00, 0x00, 0x00);
	pse_write(0xec);
	delay(1);
	pse_read(0xec);
	if (!(pse_buf[0] & 0x04)) {
		pse_set(0x00, 0x00, 0x20, 0x39);
		pse_write(0x18);
		delay(10);
		pse_set(0x03, 0x00, 0x00, 0x00);
		pse_write(0xec);
	}

	pse_read(0xe8);
	img = (pse_buf[0] == 2 && !pse_buf[1] && !pse_buf[2] && !pse_buf[3]) ? 1 : 0;
	if (!img_len[img]) {
		print_string("PSE: no image for this chip type\n");
		return;
	}
	addr = PSE_IMAGE_START + PSE_HDR_LEN;
	if (img)
		addr += img_len[0];

	print_string("PSE: loading image "); write_char('0' + img); write_char('\n');
	if (!pse_write_image(addr, img_len[img])) {
		print_string("PSE: load failed, no answer\n");
		return;
	}
	delay(10);
	pse_read(0xec);
	print_string(pse_buf[0] & 0x20 ? "PSE: load ok\n" : "PSE: load failed\n");
	delay(10);
}

void machine_custom_init(void) __banked
{
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 6);
	pse_load();
}
