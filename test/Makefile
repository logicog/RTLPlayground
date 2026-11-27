# Host unit-test harness for RTLPlayground firmware logic.
#
# Compiles selected firmware translation units with host gcc + AddressSanitizer,
# force-including sdcc_shim.h to erase 8051 keywords, and links them against
# mocked hardware edges (support.c). No SDCC, no flashing, no hardware.
#
#   make        build + run all tests
#   make run    run tests
#   make clean

CC       = gcc
SRCROOT  = ..
# -include shim first so __xdata/__code/__banked vanish before any firmware header.
CFLAGS   = -std=gnu11 -g -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unknown-pragmas \
           -Wno-pointer-sign \
           -DRTLP_HOST_TEST \
           -fsanitize=address,undefined -fno-omit-frame-pointer \
           -include sdcc_shim.h -Istub -I$(SRCROOT) -I$(SRCROOT)/httpd -I$(SRCROOT)/uip
ifeq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS  += -Wno-builtin-declaration-mismatch
endif
LDFLAGS  = -fsanitize=address,undefined

BUILDDIR = build

# One binary per subsystem under test.
TESTS = $(BUILDDIR)/test_cmd_editor \
        $(BUILDDIR)/test_port_tables \
        $(BUILDDIR)/test_page_json \
        $(BUILDDIR)/test_httpd_tx \
        $(BUILDDIR)/test_startup_crlf \
        $(BUILDDIR)/test_tick_gate \
        $(BUILDDIR)/test_health_idle \
        $(BUILDDIR)/test_timer \
        $(BUILDDIR)/test_cpu_vlan_full \
        $(BUILDDIR)/test_cpu_vlan_restricted \
        $(BUILDDIR)/test_cpu_vlan_8372 \
        $(BUILDDIR)/test_nic_rx \
        $(BUILDDIR)/test_lacp \
        $(BUILDDIR)/test_igmp

# The table tests link the register mock and one environment file; stub/
# stands in for the two headers a firmware build generates.
TABLE_ENV = support.c hw_mock.c env_tables.c $(SRCROOT)/cmd_editor.c \
            $(SRCROOT)/rtl837x_port.c $(SRCROOT)/httpd/page_impl.c \
            $(SRCROOT)/httpd/html_out.c

all: run stp

stp:
	$(MAKE) -C stp

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# cmd_editor.c is compiled UNMODIFIED from the firmware tree.
$(BUILDDIR)/test_cmd_editor: test_cmd_editor.c support.c $(SRCROOT)/cmd_editor.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# rtl837x_port.c and httpd/page_impl.c are compiled UNMODIFIED from the firmware tree.
$(BUILDDIR)/test_port_tables: test_port_tables.c $(TABLE_ENV) hw_mock.h | $(BUILDDIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

$(BUILDDIR)/test_tick_gate: test_tick_gate.c support.c $(SRCROOT)/cmd_editor.c $(SRCROOT)/timer.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILDDIR)/test_health_idle: test_health_idle.c support.c $(SRCROOT)/cmd_editor.c $(SRCROOT)/health_idle.h | $(BUILDDIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

# timer.c is compiled UNMODIFIED from the firmware tree.
$(BUILDDIR)/test_timer: test_timer.c support.c $(SRCROOT)/cmd_editor.c $(SRCROOT)/timer.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Exercise the production RX ring reader; only the hardware DMA edge is mocked.
# Discard unused TX functions so this RX fixture needs no TX register mocks.
$(BUILDDIR)/test_nic_rx: test_nic_rx.c $(SRCROOT)/rtl837x_nic.c $(SRCROOT)/rtl837x_nic.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections $(filter %.c,$^) -o $@ $(LDFLAGS) -Wl,--gc-sections

$(BUILDDIR)/test_page_json: test_page_json.c $(TABLE_ENV) hw_mock.h | $(BUILDDIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

# rtl837x_lacp.c is compiled UNMODIFIED from the firmware tree.
$(BUILDDIR)/test_lacp: test_lacp.c $(SRCROOT)/rtl837x_lacp.c $(SRCROOT)/timer.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILDDIR)/test_igmp: test_igmp.c $(SRCROOT)/rtl837x_igmp.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# httpd/httpd.c and uip/uip.c are compiled UNMODIFIED from the firmware tree;
# this test carries the environment below them itself.
$(BUILDDIR)/test_httpd_tx: test_httpd_tx.c $(SRCROOT)/httpd/httpd.c $(SRCROOT)/uip/uip.c $(SRCROOT)/timer.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)
# the login sessions httpd.c calls into
$(BUILDDIR)/test_httpd_tx: $(SRCROOT)/sessions.c

$(BUILDDIR)/test_startup_crlf: test_startup_crlf.py $(SRCROOT)/cmd_parser.c $(SRCROOT)/rtl837x_common.h | $(BUILDDIR)
	python3 $< --emit > $(BUILDDIR)/startup_crlf.c
	$(CC) $(CFLAGS) $(BUILDDIR)/startup_crlf.c $(SRCROOT)/passwd_hash.c -o $@ $(LDFLAGS) -no-pie

$(BUILDDIR)/test_httpd_tx $(BUILDDIR)/test_startup_crlf: $(SRCROOT)/passwd_hash.c $(SRCROOT)/passwd_hash.h $(SRCROOT)/httpd/request_buffer.h $(SRCROOT)/crypto/blake3.h

# CPU VLAN initialization with full and reduced user-port ranges.
$(BUILDDIR)/test_cpu_vlan_restricted: CFLAGS += -DMACHINE_MAX_PORT=7
$(BUILDDIR)/test_cpu_vlan_8372: CFLAGS += -DMACHINE_MAX_PORT=7 -DMACHINE_MIN_PORT=3 -DRTLP_TEST_IS8373=0
$(BUILDDIR)/test_cpu_vlan_full $(BUILDDIR)/test_cpu_vlan_restricted $(BUILDDIR)/test_cpu_vlan_8372: test_cpu_vlan.c $(TABLE_ENV) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

# Check the same C and assembly as the firmware when the 8051 tools are installed.
# Neither crypto object needs a board configuration or web assets.
# At XRAM base 0xb1 the working state crosses a 256-byte page boundary.
SDCC ?= $(firstword $(wildcard $(SRCROOT)/toolchain/sdcc-*/bin/sdcc) $(shell command -v sdcc))
S51 ?= $(shell command -v ucsim_51 || command -v s51)

run: $(TESTS)
ifneq ($(and $(SDCC),$(S51)),)
	$(SDCC) -mmcs51 -I$(SRCROOT) -c $(SRCROOT)/crypto/blake3.c -o $(BUILDDIR)/blake3.rel
	$(patsubst %sdcc,%sdas8051,$(SDCC)) -plosgff -o $(BUILDDIR)/blake_regs.rel $(SRCROOT)/crypto/blake_regs.asm
	$(SDCC) -mmcs51 -I$(SRCROOT) --xram-loc 0xb1 -Wl-bBANK4=0x4000 test_blake3.c $(BUILDDIR)/blake3.rel $(BUILDDIR)/blake_regs.rel -o $(BUILDDIR)/blake3-test.ihx
	printf 'run\nquit\n' | timeout 30 $(S51) -q -t 8051 -c - -I 'if=xram[0xffff],out=$(BUILDDIR)/digest.bin' $(BUILDDIR)/blake3-test.ihx
	test "$$(od -An -v -tx1 -N32 $(BUILDDIR)/digest.bin | tr -d ' \n')" = 7545b332ac37d71ec8a24e908031ac68764ccc013f25af318f315deb565eed01
	test "$$(od -An -v -tx1 -j32 $(BUILDDIR)/digest.bin | tr -d ' \n')" = 2047414b746c001b0fef90efb8a73cec8df2ab16e0d80d14a8b43238b2a89873
else
	@echo "SKIP 8051 BLAKE3 vectors (requires SDCC and uCsim)"
endif
	@fail=0; for t in $(TESTS); do \
		echo "=== $$t ==="; \
		$$t || fail=1; \
		echo; \
	done; \
	if [ $$fail -ne 0 ]; then echo "TESTS FAILED"; exit 1; else echo "ALL TESTS PASSED"; fi

clean:
	rm -rf $(BUILDDIR)
	$(MAKE) -C stp clean

.PHONY: all run stp clean
