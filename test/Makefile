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
        $(BUILDDIR)/test_page_json_8370 \
        $(BUILDDIR)/test_l2_mgmt_8370 \
        $(BUILDDIR)/test_vlan_cpu_8370 \
        $(BUILDDIR)/test_httpd_tx \
        $(BUILDDIR)/test_tick_gate \
        $(BUILDDIR)/test_timer \
        $(BUILDDIR)/test_cpu_vlan_full \
        $(BUILDDIR)/test_cpu_vlan_restricted \
        $(BUILDDIR)/test_cpu_vlan_8372 \
        $(BUILDDIR)/test_nic_rx \
        $(BUILDDIR)/test_lacp

# The table tests link the register mock and one environment file; stub/
# stands in for the two headers a firmware build generates.
TABLE_ENV = support.c hw_mock.c env_tables.c $(SRCROOT)/cmd_editor.c \
            $(SRCROOT)/rtl837x_port.c $(SRCROOT)/httpd/page_impl.c \
            $(SRCROOT)/httpd/html_out.c

all: run

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

# Same sources, but selecting the RTL8370 machine, so machine.h derives
# SOC_FAMILY=RTL836X and the RTL836x branches of page_impl.c / rtl837x_port.c
# are taken. These three are the ONLY host binaries built that way, so they are
# the only place those paths are exercised; this one checks the JSON shape
# of /status.json. SOC_FAMILY is never defined on the command line: machine.h
# derives it from MACHINE_CHIPSET and rejects a manual definition.
$(BUILDDIR)/test_page_json_8370: test_page_json_8370.c $(TABLE_ENV) hw_mock.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -Wno-error -DMACHINE_NAME=SODOLA_SL_SG008W $(filter %.c,$^) -o $@ $(LDFLAGS)

# The management L2 entry port_l2_static_mgmt() stages: its words, the control
# word, and the same words read back through the walk.
$(BUILDDIR)/test_l2_mgmt_8370: test_l2_mgmt_8370.c $(TABLE_ENV) hw_mock.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -Wno-error -DMACHINE_NAME=SODOLA_SL_SG008W $(filter %.c,$^) -o $@ $(LDFLAGS)

# The CPU port's bit in the fabric masks: what a `vlan` command ends up
# programming, and the logical<->fabric translation port isolation does.
$(BUILDDIR)/test_vlan_cpu_8370: test_vlan_cpu_8370.c $(TABLE_ENV) hw_mock.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -Wno-error -DMACHINE_NAME=SODOLA_SL_SG008W $(filter %.c,$^) -o $@ $(LDFLAGS)

# httpd/httpd.c and uip/uip.c are compiled UNMODIFIED from the firmware tree;
# this test carries the environment below them itself.
$(BUILDDIR)/test_httpd_tx: test_httpd_tx.c $(SRCROOT)/httpd/httpd.c $(SRCROOT)/uip/uip.c $(SRCROOT)/timer.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)
# the login sessions httpd.c calls into
$(BUILDDIR)/test_httpd_tx: $(SRCROOT)/sessions.c

# CPU VLAN initialization with full and reduced user-port ranges.
$(BUILDDIR)/test_cpu_vlan_restricted: CFLAGS += -DMACHINE_MAX_PORT=7
$(BUILDDIR)/test_cpu_vlan_8372: CFLAGS += -DMACHINE_MAX_PORT=7 -DMACHINE_MIN_PORT=3 -DRTLP_TEST_IS8373=0
$(BUILDDIR)/test_cpu_vlan_full $(BUILDDIR)/test_cpu_vlan_restricted $(BUILDDIR)/test_cpu_vlan_8372: test_cpu_vlan.c $(TABLE_ENV) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

run: $(TESTS)
	@fail=0; for t in $(TESTS); do \
		echo "=== $$t ==="; \
		$$t || fail=1; \
		echo; \
	done; \
	if [ $$fail -ne 0 ]; then echo "TESTS FAILED"; exit 1; else echo "ALL TESTS PASSED"; fi

clean:
	rm -rf $(BUILDDIR)

.PHONY: all run clean
