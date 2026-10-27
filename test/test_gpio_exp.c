/*
 * test_gpio_exp.c - GPIOs on an I2C expander. rtl837x_pins.c is compiled
 * unmodified; hw_mock.c plays the I2C master and a PCA9555-style expander.
 * The expected I2C_CTRL words are the ones the Mercury SE106 Pro stock
 * firmware writes for the same transfers.
 */
#include <stdint.h>
#include <stdio.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "machine/machine_defs.h"
#include "hw_mock.h"
#include "support.h"

/* Declared by hand: <unistd.h> clashes with the firmware's sleep() */
extern unsigned int alarm(unsigned int);

int tests_run, tests_failed;
__xdata uint8_t sfp_buf[16];
void print_string(const char *s) { (void)s; }
void print_byte(uint8_t b) { (void)b; }

#define EXP_BUS I2CBUS(GPIO49_I2C_SDA1, GPIO48_I2C_SCL1)

const struct machine machine = {
	.machine_name = "HOSTTEST",
	.gpio_exp = { .addr = 0x21, .i2c = EXP_BUS, .out = { 0x1f, 0x00 }, .dir = { 0x1a, 0x00 } },
};

static uint8_t *exp_regs;

/* A PCA9555 after power-up: all pins inputs, output latches high */
static void boot(void)
{
	hw_reset();
	exp_regs = hw_i2c_dev(EXP_BUS, 0x21);
	exp_regs[0] = exp_regs[1] = 0xff;
	exp_regs[2] = exp_regs[3] = 0xff;
	exp_regs[6] = exp_regs[7] = 0xff;
	gpio_exp_setup();
}

static const struct hw_i2c_xfer *last(void)
{
	return hw_i2c_at(hw_i2c_count() - 1);
}

static void test_setup(void)
{
	boot();
	CHECK(hw_i2c_count() == 4, "setup writes four expander registers");
	CHECK(hw_i2c_at(0)->reg == 2 && hw_i2c_at(1)->reg == 3
	      && hw_i2c_at(2)->reg == 6 && hw_i2c_at(3)->reg == 7,
	      "outputs are written before directions");
	CHECK(exp_regs[2] == 0x1f && exp_regs[3] == 0x00 && exp_regs[6] == 0x1a && exp_regs[7] == 0x00,
	      "registers hold the machine's power-up values");
	CHECK(hw_i2c_at(0)->ctrl == 0x0010250d, "write transfer word matches the stock firmware");
}

static void test_read(void)
{
	boot();
	exp_regs[0] = 0xf7;
	CHECK(!gpio_exp_test(GPIO_EXP(3)), "P0.3 low reads low");
	CHECK(last()->ctrl == 0x00102509 && last()->reg == 0, "read transfer word matches the stock firmware");
	CHECK(gpio_exp_test(GPIO_EXP(1)), "P0.1 high reads high");
	exp_regs[1] = 0xfe;
	CHECK(!gpio_exp_test(GPIO_EXP(8)) && last()->reg == 1, "P1.0 is read from input port 1");
}

static void test_output(void)
{
	int n;

	boot();
	gpio_output_setup(GPIO_EXP(2), 0);
	CHECK(exp_regs[2] == 0x1b && exp_regs[6] == 0x1a, "output setup clears the bit and keeps the pin an output");
	gpio_output_setup(GPIO_EXP(0), 0);
	CHECK(exp_regs[2] == 0x1a, "a second output keeps the first one's level");
	n = hw_i2c_count();
	gpio_output_setup(GPIO_EXP(1), 0);
	CHECK(exp_regs[2] == 0x18 && exp_regs[6] == 0x18, "an input pin can be turned into an output");
	CHECK(hw_i2c_at(n)->reg == 2 && hw_i2c_at(n + 1)->reg == 6, "its level is written before its direction");
	gpio_output_setup(GPIO_EXP(9), 1);
	CHECK(exp_regs[3] == 0x02 && exp_regs[7] == 0x00, "a port 1 pin uses registers 3 and 7");
}

static void test_input(void)
{
	boot();
	gpio_input_setup(GPIO_EXP(5));
	CHECK(exp_regs[6] == 0x3a, "input setup sets the direction bit");
}

static void test_missing(void)
{
	hw_reset();
	gpio_exp_setup();
	CHECK(gpio_exp_test(GPIO_EXP(4)), "an expander that does not answer reads high");
}

static void test_soc_gpio(void)
{
	int n;

	boot();
	n = hw_i2c_count();
	gpio_input_setup(GPIO38);
	CHECK(hw_i2c_count() == n, "SoC GPIOs do not touch the I2C bus");
}

int main(void)
{
	alarm(10);	/* a busy-wait that never ends kills the test */
	test_setup();
	test_read();
	test_output();
	test_input();
	test_missing();
	test_soc_gpio();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed != 0;
}
