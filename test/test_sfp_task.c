/*
 * test_sfp_task.c - the SFP module read stepped from the main loop.
 *
 * sfp.c and rtl837x_pins.c are compiled UNMODIFIED from the firmware tree
 * against the register mock, whose I2C master stays busy for a chosen number
 * of polls. The main loop is modelled as: handle_sfp() every SFP_TICK_STEP
 * ticks, sfp_task() on every pass, one tick per pass.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "machine.h"
#include "rtl837x_pins.h"
#include "sfp.h"
#include <test/hw_mock.h>
#include <test/support.h>

#define SFP_TICK_STEP 10
#define PIN_DET0 30
#define PIN_LOS0 29
#define PIN_DET1 50
#define PIN_LOS1 51
#define BUS0 0x04
#define BUS1 0x08

const struct machine machine = {
	.machine_name = "HOSTTEST",
	.n_sfp = 2,
	.sfp_port[0] = { .pin_detect = PIN_DET0, .pin_los = PIN_LOS0, .pin_tx_disable = GPIO_NA, .sds = 1, .i2c = BUS0 },
	.sfp_port[1] = { .pin_detect = PIN_DET1, .pin_los = PIN_LOS1, .pin_tx_disable = GPIO_NA, .sds = 0, .i2c = BUS1 },
};
struct machine_runtime machine_detected = { .isRTL8373 = 1 };

volatile uint32_t ticks;

void print_byte(uint8_t b)
{
	static const char hex[] = "0123456789abcdef";
	write_char(hex[b >> 4]);
	write_char(hex[b & 0xf]);
}

/* the firmware's own gpio_pin_test, which lives in rtlplayground.c */
bool gpio_pin_test(uint8_t pin)
{
	reg_read_m(RTL837X_REG_GPIO_00_31_INPUT + (pin > 31 ? 4 : 0));
	return sfr_data[3 - ((pin >> 3) & 3)] & (1 << (pin & 7));
}

static int sds_calls;
static uint8_t sds_last_id, sds_last_mode;
void sds_config(uint8_t sds, uint8_t mode)
{
	sds_calls++;
	sds_last_id = sds;
	sds_last_mode = mode;
}

static int sds_off_calls;
static uint8_t sds_off_id;
void sds_config_mac(uint8_t sds, uint8_t mode)
{
	if (mode == SDS_OFF) {
		sds_off_calls++;
		sds_off_id = sds;
	}
}

/* ---------- helpers ---------- */

static void pin_set(uint8_t pin, bool high)
{
	uint16_t reg = RTL837X_REG_GPIO_00_31_INPUT + (pin > 31 ? 4 : 0);
	uint32_t v = hw_reg_get(reg);
	if (high)
		v |= 1u << (pin & 31);
	else
		v &= ~(1u << (pin & 31));
	hw_reg_set(reg, v);
}

static void module_insert(uint8_t slot, bool present)
{
	pin_set(slot ? PIN_DET1 : PIN_DET0, !present);
}

static void eeprom_text(uint8_t bus, uint8_t reg, const char *text, uint8_t len)
{
	memset(&hw_eeprom[bus >> 2][0][reg], ' ', len);
	memcpy(&hw_eeprom[bus >> 2][0][reg], text, strlen(text));
}

static void eeprom_checksums(uint8_t bus)
{
	uint8_t *e = hw_eeprom[bus >> 2][0];
	uint8_t sum = 0;

	for (int i = 0; i < 63; i++)
		sum += e[i];
	e[63] = sum;
	sum = 0;
	for (int i = 64; i < 95; i++)
		sum += e[i];
	e[95] = sum;
}

static void eeprom_module(uint8_t bus, const char *vendor, const char *pn, const char *rev,
			  const char *sn, uint8_t rate, uint8_t options)
{
	hw_eeprom[bus >> 2][0][11] = 0x01;
	hw_eeprom[bus >> 2][0][12] = rate;
	eeprom_text(bus, 20, vendor, 16);
	eeprom_text(bus, 40, pn, 16);
	eeprom_text(bus, 56, rev, 4);
	eeprom_text(bus, 68, sn, 16);
	hw_eeprom[bus >> 2][0][92] = options;
	eeprom_checksums(bus);
}

static void world_reset(void)
{
	hw_reset();
	pin_set(PIN_DET0, true);
	pin_set(PIN_DET1, true);
	pin_set(PIN_LOS0, true);
	pin_set(PIN_LOS1, true);
	ticks = 0;
	sfp_pins_last = 0x22;
	memset(sfp_state, 0, sizeof(sfp_state));
	memset(sfp_speed, 0, sizeof(sfp_speed));
	memset(sfp_options, 0, sizeof(sfp_options));
	sfp_ready = 0;
	setup_sfp_gpio();
	sds_calls = 0;
	sds_off_calls = 0;
	out_reset();
}

static unsigned max_reads_per_pass;
static unsigned passes_while_busy;

/* One main loop pass per tick, as idle() does it */
static void run(unsigned n)
{
	while (n--) {
		unsigned long before;
		bool busy = hw_reg_get(RTL837X_REG_I2C_CTRL) & 1;

		if (ticks % SFP_TICK_STEP == 0)
			handle_sfp();
		before = hw_reads;
		watchdog_arm(2, "sfp_task");
		sfp_task();
		watchdog_disarm();
		if (hw_reads - before > max_reads_per_pass)
			max_reads_per_pass = hw_reads - before;
		if (busy)
			passes_while_busy++;
		ticks++;
	}
}

static bool out_has(const char *s)
{
	return strstr(out_buf, s) != NULL;
}

static int out_count(const char *s)
{
	int n = 0;
	for (const char *p = out_buf; (p = strstr(p, s)); p++)
		n++;
	return n;
}

static void fail_next_transaction(void)
{
	unsigned started = hw_i2c_started;

	for (int n = 0; n < 1000 && hw_i2c_started == started; n++)
		run(1);
	hw_i2c_fail = true;
	run(5);
}

int main(void)
{
	printf("test_sfp_task: SFP module read from the main loop\n");

	/* T1: a module appears, the read starts SFP_WAKE_TICKS later and takes
	 * one I2C transaction per step; nothing spins while the master is busy. */
	world_reset();
	hw_i2c_busy_reads = 3;
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	hw_eeprom[BUS0 >> 2][1][224 & 0x7f] = 0x2a;
	module_insert(0, true);
	run(1);
	CHECK(out_has("<MODULE INSERTED>  Slot: 1"), "T1 insertion reported by the poller");
	CHECK(hw_i2c_started == 0 && sds_calls == 0, "T1 nothing read before the wake delay");
	run(SFP_WAKE_TICKS - 1);
	CHECK(hw_i2c_started == 0, "T1 still nothing one tick before the wake delay ends");
	max_reads_per_pass = 0;
	passes_while_busy = 0;
	run(SFP_WAKE_TICKS);
	CHECK(hw_i2c_started == 10, "T1 ten transactions: A0h 0 to 95 in nine, then the diagnostics");
	CHECK(sfp_ready == 0x1 && sfp_dom[0][0] == 0x2a, "T1 diagnostics kept and the slot marked as read");
	CHECK(sds_calls == 1 && sds_last_id == 1 && sds_last_mode == SDS_10GR, "T1 SerDes set to 10G on sds 1");
	CHECK(!strcmp(sfp_module_vendor[0], "FS"), "T1 vendor stored and trimmed");
	CHECK(!strcmp(sfp_module_model[0], "SFP-10GSR-85"), "T1 model stored");
	CHECK(!strcmp(sfp_module_serial[0], "F1234567"), "T1 serial stored");
	CHECK(sfp_options[0] == 0x40, "T1 options byte stored");
	CHECK(out_has("  Rate: 67  Encoding: 01  Module: FS SFP-10GSR-85 F1234567\n"),
	      "T1 module line printed once the module is read");
	CHECK(hw_i2c_polls == 10 * 3, "T1 every transaction polled exactly busy_reads times");
	CHECK(passes_while_busy == 10 * 2, "T1 the loop went round twice per transaction while the master was busy");
	CHECK(max_reads_per_pass < 12, "T1 a pass never does more than one transaction's worth of register reads");
	hw_i2c_started = 0;
	hw_eeprom[BUS0 >> 2][1][224 & 0x7f] = 0x2b;
	run(300);
	CHECK(hw_i2c_started == 2 && sds_calls == 1, "T1 the module is read once, its diagnostics once a second");
	CHECK(sfp_dom[0][0] == 0x2b, "T1 the kept diagnostics follow the module");

	/* T2: an I2C error in the middle of the read is reported, and the read
	 * is repeated from the start after the retry delay. */
	world_reset();
	hw_i2c_busy_reads = 2;
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 1);
	CHECK(hw_i2c_started == 1, "T2 first transaction in flight");
	hw_i2c_fail = true;
	run(1);
	CHECK(out_has("SFP: an I2C read failed, retrying"), "T2 failure reported");
	CHECK(sds_calls == 0, "T2 SerDes untouched after a failure");
	out_reset();
	run(SFP_RETRY_TICKS + 40);
	CHECK(!out_has("INSERTED"), "T2 the retry is not reported as a new module");
	CHECK(sds_calls == 1 && !strcmp(sfp_module_serial[0], "F1234567"), "T2 second attempt completes");
	CHECK(hw_i2c_started == 1 + 10, "T2 the retry starts from the first transaction");

	/* T3: two modules inserted at once are read one after the other, each
	 * from its own bus. */
	world_reset();
	eeprom_module(BUS0, "VENDOR-A", "PN-A", "1", "SN-A", 0x0d, 0);
	eeprom_module(BUS1, "VENDOR-B", "PN-B", "2", "SN-B", 0x1e, 0);
	module_insert(0, true);
	module_insert(1, true);
	run(SFP_WAKE_TICKS + 60);
	CHECK(hw_i2c_started == 18 && sds_calls == 2, "T3 both modules read");
	CHECK(!strcmp(sfp_module_vendor[0], "VENDOR-A") && !strcmp(sfp_module_serial[0], "SN-A"), "T3 slot 1 from bus 0");
	CHECK(!strcmp(sfp_module_vendor[1], "VENDOR-B") && !strcmp(sfp_module_serial[1], "SN-B"), "T3 slot 2 from bus 1");
	CHECK(sds_last_id == 0 && sds_last_mode == SDS_HSG, "T3 slot 2 SerDes set to 2.5G last");
	CHECK(out_has("Module: VENDOR-A") && out_has("Module: VENDOR-B"), "T3 both module lines printed");

	/* T4: the DDM quirk adds one diagnostic read, and a failure of that
	 * read is not an error. */
	world_reset();
	eeprom_module(BUS0, "QSFPTEK", "QT-SFP+-T", "A", "Q1", 0x67, 0);
	hw_eeprom[BUS0 >> 2][1][226 & 0x7f] = 0x12;
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 40);
	CHECK(hw_i2c_started == 11 && sds_calls == 1, "T4 quirk read done, then the diagnostics");
	CHECK(sfp_options[0] == 0x40, "T4 DDM enabled from the diagnostic read");
	world_reset();
	hw_i2c_busy_reads = 2;
	eeprom_module(BUS0, "QSFPTEK", "QT-SFP+-T", "A", "Q1", 0x67, 0);
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 1 + 9);
	CHECK(hw_i2c_started == 10 && sds_calls == 0, "T4 diagnostic read in flight");
	hw_i2c_fail = true;
	run(20);
	CHECK(hw_i2c_started == 10 && sds_calls == 1 && sfp_options[0] == 0, "T4 failed diagnostic read leaves DDM off");
	CHECK(!out_has("retrying"), "T4 no retry after the diagnostic read");

	/* T5: a module pulled during its read fails and is not reported again
	 * while absent. */
	world_reset();
	hw_i2c_busy_reads = 2;
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 1);
	module_insert(0, false);
	hw_i2c_fail = true;
	run(1);
	CHECK(out_has("retrying"), "T5 failure reported after removal");
	out_reset();
	run(300);
	CHECK(sds_calls == 0 && !out_has("INSERTED"), "T5 nothing more while the slot is empty");
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 50);
	CHECK(sds_calls == 1, "T5 reinserted module is read");

	/* T6: a module with external calibration has its constants read once,
	 * together with the module. */
	world_reset();
	eeprom_module(BUS1, "VENDOR-B", "PN-B", "2", "SN-B", 0x1e, 0x50);
	for (uint8_t i = 0; i < 36; i++)
		hw_eeprom[BUS1 >> 2][1][(184 + i) & 0x7f] = 0x80 + i;
	module_insert(1, true);
	run(SFP_WAKE_TICKS + 60);
	CHECK(hw_i2c_started == 13 && sds_calls == 1, "T6 module, three calibration blocks and the diagnostics");
	CHECK(sfp_cal[1][0] == 0x80 && sfp_cal[1][16] == 0x90 && sfp_cal[1][35] == 0xa3, "T6 calibration kept from A2h 56 to 91");
	hw_i2c_started = 0;
	run(400);
	CHECK(hw_i2c_started == 2, "T6 only the diagnostics are read again");

	/* T7: a failed refresh of the diagnostics is not a failed module. */
	world_reset();
	hw_i2c_busy_reads = 2;
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 40);
	CHECK(sfp_ready == 0x1, "T7 module read");
	hw_i2c_fail = true;
	out_reset();
	run(250);
	CHECK(sfp_ready == 0x1 && sds_calls == 1, "T7 the module stays read");
	CHECK(!out_has("retrying") && !out_has("INSERTED"), "T7 no retry of the module read");

	/* T8: a pulled module is no longer read nor reported as read. */
	world_reset();
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 40);
	CHECK(sfp_ready == 0x1, "T8 module read");
	module_insert(0, false);
	run(SFP_TICK_STEP);
	CHECK(sfp_ready == 0, "T8 removal clears the slot");
	hw_i2c_started = 0;
	run(400);
	CHECK(hw_i2c_started == 0, "T8 nothing is read from an empty slot");

	/* T9: a transaction that never finishes is given up on after
	 * SFP_IO_TICKS and the module is read again after the retry delay. */
	world_reset();
	hw_i2c_busy_reads = 100000;
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 1);
	CHECK(hw_i2c_started == 1, "T9 first transaction in flight");
	run(SFP_IO_TICKS - 1);
	CHECK(!out_has("retrying"), "T9 still waiting one tick before the timeout");
	run(2);
	CHECK(out_has("retrying") && sds_calls == 0, "T9 the stuck transaction is given up on");
	hw_i2c_busy_reads = 2;
	out_reset();
	run(SFP_RETRY_TICKS + 40);
	CHECK(sds_calls == 1 && sfp_ready == 0x1, "T9 the module is read on the retry");

	/* T10: a module pulled while its last transaction is in flight is not
	 * published from the data that transaction still delivers. */
	world_reset();
	hw_i2c_busy_reads = 12;
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	module_insert(0, true);
	run(1);
	run(SFP_WAKE_TICKS - 1);
	run(1 + 9 * 11);
	CHECK(hw_i2c_started == 10 && sds_calls == 0, "T10 last transaction in flight");
	module_insert(0, false);
	out_reset();
	run(20);
	CHECK(out_has("<MODULE REMOVED>") && sfp_ready == 0 && sds_calls == 0,
	      "T10 the pulled module is not published");
	module_insert(0, true);
	run(SFP_TICK_STEP + SFP_WAKE_TICKS + 150);
	CHECK(sds_calls == 1 && sfp_ready == 0x1, "T10 the module is read again once it is back");

	/* T11: a bad base checksum turns the SerDes off and the slot waits for
	 * the module to be pulled, without retrying. */
	world_reset();
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	hw_eeprom[BUS0 >> 2][0][63]++;
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 40);
	CHECK(out_has("SFP: EEPROM checksum error"), "T11 checksum error reported");
	CHECK(hw_i2c_started == 6 && sds_calls == 0 && sfp_ready == 0, "T11 the read stops at the base checksum");
	CHECK(sds_off_calls == 1 && sds_off_id == 1, "T11 SerDes of the slot turned off");
	out_reset();
	run(500);
	CHECK(hw_i2c_started == 6 && !out_has("SFP"), "T11 nothing more while the module stays");
	module_insert(0, false);
	run(SFP_TICK_STEP);
	CHECK(out_has("<MODULE REMOVED>"), "T11 removal reported");
	eeprom_checksums(BUS0);
	module_insert(0, true);
	run(SFP_TICK_STEP + SFP_WAKE_TICKS + 40);
	CHECK(sds_calls == 1 && sfp_ready == 0x1, "T11 a good module in the same slot is read");
	world_reset();
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	hw_eeprom[BUS0 >> 2][0][63]++;
	sfp_speed[0] = SFP_SPEED_10G;
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 40);
	CHECK(sds_calls == 1 && sfp_ready == 0x1 && sds_off_calls == 0, "T11 with the speed set by hand the checksum is not enforced");

	/* T12: a bad extended checksum drops the diagnostics but keeps the
	 * module. */
	world_reset();
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	hw_eeprom[BUS0 >> 2][0][95]++;
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 40);
	CHECK(sds_calls == 1 && sfp_ready == 0x1 && sds_off_calls == 0, "T12 module published");
	CHECK(sfp_options[0] == 0 && hw_i2c_started == 9, "T12 diagnostics treated as absent");

	/* T13: a rate that maps to no SerDes mode is a fault until the speed is
	 * set by hand, which reads the module again. */
	world_reset();
	eeprom_module(BUS0, "DAC", "CABLE", "A", "D1", 0x00, 0);
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 40);
	CHECK(out_has("SFP: unknown rate") && sds_calls == 0 && sds_off_calls == 1, "T13 unknown rate is a fault");
	sfp_speed[0] = SFP_SPEED_10G;
	sfp_state[0] = SFP_ABSENT;
	handle_sfp();
	run(SFP_WAKE_TICKS + 40);
	CHECK(sds_calls == 1 && sds_last_mode == SDS_10GR && sfp_ready == 0x1, "T13 forced speed reads and configures the module");

	/* T14: the read is given up on after SFP_TRIES failed attempts. */
	world_reset();
	hw_i2c_busy_reads = 2;
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	module_insert(0, true);
	for (int k = 0; k < SFP_TRIES; k++)
		fail_next_transaction();
	CHECK(out_count("retrying") == SFP_TRIES - 1, "T14 every failure but the last is retried");
	CHECK(out_has("SFP: I2C read failed") && sds_off_calls == 1 && sds_calls == 0, "T14 then the slot is a fault");
	{
		unsigned started = hw_i2c_started;
		run(1000);
		CHECK(hw_i2c_started == started, "T14 no more reads while the module stays");
	}

	/* T15: a fault in one slot does not stop the diagnostics of the other. */
	world_reset();
	eeprom_module(BUS0, "BAD", "PN-A", "1", "SN-A", 0x67, 0);
	hw_eeprom[BUS0 >> 2][0][63]++;
	eeprom_module(BUS1, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	hw_eeprom[BUS1 >> 2][1][224 & 0x7f] = 0x2a;
	module_insert(0, true);
	module_insert(1, true);
	run(SFP_WAKE_TICKS + 60);
	CHECK(sfp_ready == 0x2 && sds_off_calls == 1 && sfp_dom[1][0] == 0x2a, "T15 slot 1 a fault, slot 2 read");
	hw_eeprom[BUS1 >> 2][1][224 & 0x7f] = 0x2b;
	run(300);
	CHECK(sfp_dom[1][0] == 0x2b, "T15 slot 2 diagnostics still refreshed");

	/* T16: a retry after an error past the first transaction checks the
	 * sums from scratch. */
	world_reset();
	hw_i2c_busy_reads = 2;
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 1 + 3);
	CHECK(hw_i2c_started == 4, "T16 fourth transaction in flight");
	hw_i2c_fail = true;
	run(SFP_RETRY_TICKS + 40);
	CHECK(sds_calls == 1 && sfp_ready == 0x1 && sds_off_calls == 0, "T16 the retry passes the checksums");

	/* T17: a module pulled while its diagnostics are being refreshed. */
	world_reset();
	hw_i2c_busy_reads = 18;
	eeprom_module(BUS0, "FS", "SFP-10GSR-85", "A", "F1234567", 0x67, 0x40);
	hw_eeprom[BUS0 >> 2][1][224 & 0x7f] = 0x2a;
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 400);
	CHECK(sfp_ready == 0x1 && sfp_dom[0][0] == 0x2a, "T17 module read");
	{
		unsigned started = hw_i2c_started;
		for (int n = 0; n < 400 && hw_i2c_started == started; n++)
			run(1);
		CHECK(hw_i2c_started == started + 1, "T17 refresh in flight");
	}
	hw_eeprom[BUS0 >> 2][1][224 & 0x7f] = 0x2b;
	module_insert(0, false);
	run(60);
	CHECK(sfp_ready == 0 && sfp_dom[0][0] == 0x2a, "T17 the refresh of a pulled module is dropped");

	/* T18: setting the speed while the module is being read starts the read
	 * again with that speed. */
	world_reset();
	hw_i2c_busy_reads = 2;
	eeprom_module(BUS0, "FS", "SFP-1G", "A", "F1", 0x00, 0);
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 1 + 3);
	CHECK(hw_i2c_started == 4 && sfp_state[0] == SFP_READ, "T18 read in flight");
	sfp_speed[0] = SFP_SPEED_1G;
	sfp_state[0] = SFP_ABSENT;
	handle_sfp();
	run(SFP_WAKE_TICKS + 40);
	CHECK(hw_i2c_started == 4 + 9 && sds_calls == 1 && sds_last_mode == SDS_1000BX_FIBER, "T18 read again from the start with the speed set");
	CHECK(sds_off_calls == 0, "T18 the abandoned read left no fault");

	/* T19: with a bad extended checksum the DDM quirk still decides by its
	 * test read. */
	world_reset();
	eeprom_module(BUS0, "QSFPTEK", "QT-SFP+-T", "A", "Q1", 0x67, 0);
	hw_eeprom[BUS0 >> 2][0][95]++;
	hw_eeprom[BUS0 >> 2][1][226 & 0x7f] = 0x12;
	module_insert(0, true);
	run(SFP_WAKE_TICKS + 40);
	CHECK(hw_i2c_started == 11 && sfp_options[0] == 0x40 && sds_calls == 1, "T19 quirk test read done, DDM on");

	printf("\n%s (%d of %d checks failed)\n", tests_failed ? "TESTS FAILED" : "ALL PASS", tests_failed, tests_run);
	return tests_failed ? 1 : 0;
}
