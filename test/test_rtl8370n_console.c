#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define main console_firmware_main
#define reboot console_reboot
#include "../tools/eth_console/console.c"
#undef reboot
#undef main

static uint16_t registers[65536];
static unsigned settled, status_reads;
static bool bus_stuck;

/* A new command may initially report idle before the controller latches it.
 * The vendor routine always executes settling NOPs before its first poll. */
static void mock_settle(void)
{
	settled = 1;
}

static uint8_t mock_status(void)
{
	uint16_t address = (uint16_t)SFR_REG_ADDRH << 8 | SFR_REG_ADDRL;

	status_reads++;
	if (!settled)
		return 0;
	if (bus_stuck)
		return 1;
	if (SFR_EXEC_GO == SFR_EXEC_READ_REG) {
		SFR_DATA_8 = registers[address] >> 8;
		SFR_DATA_0 = registers[address];
	} else if (SFR_EXEC_GO == SFR_EXEC_WRITE_REG) {
		registers[address] = (uint16_t)SFR_DATA_24 << 8 | SFR_DATA_16;
	}
	SFR_EXEC_GO = 0;
	settled = 0;
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 1) {
		const char *cases[] = {"latency", "bus-timeout", "dma-timeout"};

		for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
			int status;
			pid_t child = fork();

			assert(child >= 0);
			if (!child) {
				execl(argv[0], argv[0], cases[i], (char *)0);
				_exit(127);
			}
			assert(waitpid(child, &status, 0) == child);
			assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		}
		return 0;
	}
	assert(argc == 2);
	if (!strcmp(argv[1], "latency")) {
		registers[0x1300] = 0x6368;
		SFR_DATA_8 = 0xde;
		SFR_DATA_0 = 0xad;
		assert(reg_read(0x1300) == 0x6368);
		reg_write(0x131c, 0x1076);
		assert(registers[0x131c] == 0x1076);
	} else if (!strcmp(argv[1], "bus-timeout")) {
		bus_stuck = true;
		settled = 1;
		(void)reg_read(0x1300);
		assert(status_reads > 0 && status_reads < 70000);
		/* A failed read must not feed stale data into a later hardware write. */
		reg_write(0x130c, 0xffff);
		assert(SFR_EXEC_GO == SFR_EXEC_READ_REG);
		assert(nic_tx(60) != 0);
		assert(SFR_NIC_CTRL == 0);
	} else if (!strcmp(argv[1], "dma-timeout")) {
		/* A stuck DMA must not be followed by TXCMDR, even on a later retry. */
		registers[0x1a08] = 128;
		registers[0x1a52] = 0x300;
		assert(nic_tx(60) != 0);
		assert(registers[0x1a0d] == 0);
		assert(nic_tx(60) != 0);
		assert(registers[0x1a0d] == 0);
	} else {
		abort();
	}
	puts(argv[1]);
	return 0;
}
