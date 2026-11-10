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
static bool spi_stuck, spi_read_stuck, flash_wip;
static unsigned spi_polls, spi_reads, spi_commands;

static uint8_t mock_flash_busy(void)
{
	spi_polls++;
	if (spi_stuck) {
		assert(spi_polls < 140000); /* Controller failure must not trigger nested timeout rounds. */
		return 1;
	}
	if (SFR_FLASH_EXEC_GO) {
		SFR_FLASH_EXEC_GO = 0;
		spi_commands++;
		if (SFR_FLASH_TCONF == 0x04) {
			spi_reads++;
			if (spi_read_stuck) {
				spi_stuck = true;
				return 1;
			}
			SFR_FLASH_DATA0 = 0xa1;
			SFR_FLASH_DATA8 = 0xb2;
			SFR_FLASH_DATA16 = 0xc3;
			SFR_FLASH_DATA24 = 0xd4;
		} else {
			SFR_FLASH_DATA0 = flash_wip ? 1 : 2;
		}
	}
	return 0;
}

static void request(uint8_t op, uint16_t address, uint8_t count)
{
	memset(rxb, 0, sizeof(rxb));
	rxb[12] = 0x88;
	rxb[13] = 0xb5;
	rxb[14] = op;
	rxb[15] = 37;
	if (op == OP_FLASH_READ) {
		rxb[17] = address >> 8;
		rxb[18] = address;
		rxb[19] = count;
	} else {
		rxb[16] = address >> 8;
		rxb[17] = address;
	}
	handle(60);
}

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
		const char *cases[] = {"latency", "bus-timeout", "dma-timeout",
			"spi-stuck", "spi-read-timeout", "flash-busy", "flash-read",
			"flash-protection", "log-boundary"};

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
	} else if (!strcmp(argv[1], "spi-stuck")) {
		spi_stuck = true;
		request(OP_FLASH_READ, 0, 4);
		assert(txb[F_STAT] == ST_TIMEOUT);
		assert(SFR_FLASH_EXEC_GO == 0 && spi_commands == 0);
		assert(txb[F_DATA] == 0); /* Failed reads return padding, not flash data. */
	} else if (!strcmp(argv[1], "spi-read-timeout")) {
		spi_read_stuck = true;
		request(OP_FLASH_READ, 0, 8);
		assert(txb[F_STAT] == ST_TIMEOUT);
		assert(spi_reads == 1);
		assert(txb[F_DATA] == 0);
		unsigned commands = spi_commands;
		spi_polls = 0;
		flash_armed = 1;
		assert(flash_erase_4k(0) == ST_TIMEOUT);
		assert(flash_program(0, rxb, 4) == ST_TIMEOUT);
		assert(spi_commands == commands && SFR_FLASH_EXEC_GO == 0);
	} else if (!strcmp(argv[1], "flash-busy")) {
		flash_wip = true;
		/* Flash WIP may outlast many controller transactions, but no data read may start. */
		spi_polls = 0;
		request(OP_FLASH_READ, 0, 4);
		assert(txb[F_STAT] == ST_TIMEOUT);
		assert(spi_reads == 0);
	} else if (!strcmp(argv[1], "flash-read")) {
		request(OP_FLASH_READ, 0, 4);
		assert(txb[F_STAT] == ST_OK && txb[F_SEQ] == 37);
		assert(!memcmp(txb + F_DATA, "\xa1\xb2\xc3\xd4", 4));
	} else if (!strcmp(argv[1], "flash-protection")) {
		assert(flash_erase_4k(0) == ST_DENIED);
		flash_armed = 1;
		assert(flash_erase_4k(0x1fc000) == ST_DENIED);
		assert(flash_program(0x1fbfff, rxb, 2) == ST_DENIED);
		assert(flash_erase_4k(1) == ST_BAD_ARG);
		assert(spi_commands == 0);
	} else if (!strcmp(argv[1], "log-boundary")) {
		const uint16_t lengths[] = {256, 512, 1024};
		io_failed = 1; /* Inspect replies without involving NIC hardware. */
		for (unsigned j = 0; j < sizeof(lengths) / sizeof(lengths[0]); j++) {
			log_len = lengths[j];
			for (unsigned i = 0; i < log_len; i++)
				log_buf[i] = i;
			uint16_t at = 0;
			while (at < log_len) {
				request(OP_LOG, at, 0);
				uint16_t n = (uint16_t)txb[F_DATA] << 8 | txb[F_DATA + 1];
				assert(n > 0 && n <= DATA_MAX - 2 && at + n <= log_len);
				assert(!memcmp(txb + F_DATA + 2, log_buf + at, n));
				at += n;
			}
			request(OP_LOG, at, 0);
			assert(txb[F_DATA] == 0 && txb[F_DATA + 1] == 0);
		}
	} else {
		abort();
	}
	puts(argv[1]);
	return 0;
}
