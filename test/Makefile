.DEFAULT_GOAL := all
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
           -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer \
           -include sdcc_shim.h -Istub -I. -I$(SRCROOT) -I$(SRCROOT)/httpd -I$(SRCROOT)/uip -I$(SRCROOT)/machine
ifeq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS  += -Wno-builtin-declaration-mismatch
endif
LDFLAGS  = -fsanitize=address,undefined
# Use the native linker's supported dead-code flag for mixed firmware benches.
# Neither GNU --gc-sections nor -no-pie is forced on non-GNU linkers.
GC_LDFLAGS := $(shell printf 'int main(void){return 0;}\n' | $(CC) -x c - -o /dev/null -Wl,--gc-sections >/dev/null 2>&1 && printf '%s' '-Wl,--gc-sections')
ifeq ($(GC_LDFLAGS),)
GC_LDFLAGS := $(shell printf 'int main(void){return 0;}\n' | $(CC) -x c - -o /dev/null -Wl,-dead_strip >/dev/null 2>&1 && printf '%s' '-Wl,-dead_strip')
endif

GC_CFLAGS := $(shell printf 'int main(void){return 0;}\n' | $(CC) -Werror -ffunction-sections -fdata-sections -x c - -c -o /dev/null >/dev/null 2>&1 && printf '%s' '-ffunction-sections -fdata-sections')

BUILDDIR = build

# One binary per subsystem under test.
TESTS = $(BUILDDIR)/test_cmd_editor \
        $(BUILDDIR)/test_port_tables \
        $(BUILDDIR)/test_page_json \
        $(BUILDDIR)/test_httpd_tx \
        $(BUILDDIR)/test_tick_gate \
        $(BUILDDIR)/test_nic_rx \
        $(BUILDDIR)/test_lacp

# The table tests link the register mock and one environment file; stub/
# stands in for the two headers a firmware build generates.
TABLE_ENV = support.c hw_mock.c env_tables.c $(SRCROOT)/cmd_editor.c \
            $(SRCROOT)/rtl837x_port.c $(SRCROOT)/httpd/page_impl.c

TESTS += $(BUILDDIR)/test_rx_bounds

$(BUILDDIR)/test_rx_bounds: test_rx_bounds.c $(SRCROOT)/uip/uip_rx.h | $(BUILDDIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

all: run

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# cmd_editor.c is compiled UNMODIFIED from the firmware tree.
$(BUILDDIR)/test_cmd_editor: test_cmd_editor.c support.c $(SRCROOT)/cmd_editor.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# rtl837x_port.c and httpd/page_impl.c are compiled UNMODIFIED from the firmware tree.
$(BUILDDIR)/test_port_tables: test_port_tables.c $(TABLE_ENV) hw_mock.h | $(BUILDDIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

$(BUILDDIR)/test_tick_gate: test_tick_gate.c support.c $(SRCROOT)/cmd_editor.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Exercise the production RX ring reader; only the hardware DMA edge is mocked.
# Discard unused TX functions so this RX fixture needs no TX register mocks.
$(BUILDDIR)/test_nic_rx: test_nic_rx.c $(SRCROOT)/rtl837x_nic.c $(SRCROOT)/rtl837x_nic.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections $(filter %.c,$^) -o $@ $(LDFLAGS) -Wl,--gc-sections

$(BUILDDIR)/test_page_json: test_page_json.c $(TABLE_ENV) hw_mock.h | $(BUILDDIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

# rtl837x_lacp.c is compiled UNMODIFIED from the firmware tree.
$(BUILDDIR)/test_lacp: test_lacp.c $(SRCROOT)/rtl837x_lacp.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# httpd/httpd.c and uip/uip.c are compiled UNMODIFIED from the firmware tree;
# this test carries the environment below them itself.
$(BUILDDIR)/test_httpd_tx: test_httpd_tx.c $(SRCROOT)/httpd/httpd.c $(SRCROOT)/uip/uip.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

run: $(TESTS) snmp snmp-horaco-profile snmp-status snmp-cli snmp-replay config-replay
	@fail=0; for t in $(TESTS); do \
		echo "=== $$t ==="; \
		$$t || fail=1; \
		echo; \
	done; \
	if [ $$fail -ne 0 ]; then echo "TESTS FAILED"; exit 1; else echo "ALL TESTS PASSED"; fi

clean:
	rm -rf $(BUILDDIR)

.PHONY: all run clean


# SNMP host benches compile the same firmware sources; listener is opt-in here.
SNMPFLAGS = $(CFLAGS) -DSNMP
SNMPENV = $(SRCROOT)/snmp.c $(SRCROOT)/snmp_control.c $(SRCROOT)/uip/uip.c $(SRCROOT)/httpd/httpd.c
$(BUILDDIR)/test_snmp: test_snmp.c test_httpd_tx.c $(SNMPENV) | $(BUILDDIR)
	$(CC) $(SNMPFLAGS) test_snmp.c $(SNMPENV) -o $@ $(LDFLAGS)

$(BUILDDIR)/test_snmp_hw: test_snmp_hw.c $(SRCROOT)/snmp_hw.c | $(BUILDDIR)
	$(CC) $(SNMPFLAGS) test_snmp_hw.c -o $@ $(LDFLAGS)

$(BUILDDIR)/test_snmp_nine: test_snmp.c test_httpd_tx.c $(SNMPENV) | $(BUILDDIR)
	$(CC) $(SNMPFLAGS) -DSNMP_TEST_NINE test_snmp.c $(SNMPENV) -o $@ $(LDFLAGS)

$(BUILDDIR)/test_snmp_hw_six: test_snmp_hw.c $(SRCROOT)/snmp_hw.c | $(BUILDDIR)
	$(CC) $(SNMPFLAGS) -DSNMP_HW_SIX test_snmp_hw.c -o $@ $(LDFLAGS)

.PHONY: snmp snmp-unit
snmp-unit: $(BUILDDIR)/test_snmp $(BUILDDIR)/test_snmp_nine $(BUILDDIR)/test_snmp_hw $(BUILDDIR)/test_snmp_hw_six
	$(BUILDDIR)/test_snmp
	$(BUILDDIR)/test_snmp_nine
	$(BUILDDIR)/test_snmp_hw
	$(BUILDDIR)/test_snmp_hw selector
	$(BUILDDIR)/test_snmp_hw prebusy
	$(BUILDDIR)/test_snmp_hw link
	$(BUILDDIR)/test_snmp_hw speed
	$(BUILDDIR)/test_snmp_hw_six


# Exercise the actual dispatcher, ARP module and UDP service implementations.
TRANSPORTDEPS = $(SRCROOT)/syslog.c $(SRCROOT)/dhcp.c $(SRCROOT)/uip/uip_arp.c $(SRCROOT)/snmp.h
TRANSPORTENV = $(SNMPENV) $(SRCROOT)/udp_apps.c transport_arp.c \
               transport_dhcp.c $(SRCROOT)/dns.c $(SRCROOT)/ntp.c transport_syslog.c
$(BUILDDIR)/test_snmp_transport: test_snmp_transport.c test_snmp.c test_httpd_tx.c snmp_wire_fixtures.h $(TRANSPORTENV) $(TRANSPORTDEPS) | $(BUILDDIR)
	$(CC) $(SNMPFLAGS) $(GC_CFLAGS) test_snmp_transport.c $(TRANSPORTENV) -o $@ $(LDFLAGS) $(GC_LDFLAGS)
$(BUILDDIR)/test_snmp_transport_nine: test_snmp_transport.c test_snmp.c test_httpd_tx.c snmp_wire_fixtures.h $(TRANSPORTENV) $(TRANSPORTDEPS) | $(BUILDDIR)
	$(CC) $(SNMPFLAGS) -DSNMP_TEST_NINE -ffunction-sections -fdata-sections test_snmp_transport.c $(TRANSPORTENV) -o $@ $(LDFLAGS) $(GC_LDFLAGS)
snmp: snmp-unit $(BUILDDIR)/test_snmp_transport $(BUILDDIR)/test_snmp_transport_nine
	$(BUILDDIR)/test_snmp_transport
	$(BUILDDIR)/test_snmp_transport_nine

# Real HORACO profile compatibility; all hardware access is mocked.
$(BUILDDIR)/test_snmp_horaco_profile: test_snmp_horaco_profile.c test_snmp.c test_httpd_tx.c $(SNMPENV) $(SRCROOT)/machine/SWTGW218AS.c | $(BUILDDIR)
	$(CC) $(SNMPFLAGS) -DMACHINE_SWTGW218AS -DMACHINE_NAME=SWTGW218AS test_snmp_horaco_profile.c $(SNMPENV) $(SRCROOT)/machine/SWTGW218AS.c -o $@ $(LDFLAGS)
.PHONY: snmp-horaco-profile
snmp-horaco-profile: $(BUILDDIR)/test_snmp_horaco_profile
	$(BUILDDIR)/test_snmp_horaco_profile

# Stage C: actual runtime snapshot and information.json serialization.
$(BUILDDIR)/test_snmp_status_json: test_snmp_status_json.c test_page_json.c $(TABLE_ENV) $(SRCROOT)/snmp.c $(SRCROOT)/snmp_control.c | $(BUILDDIR)
	$(CC) $(SNMPFLAGS) $(GC_CFLAGS) $(filter-out test_page_json.c,$(filter %.c,$^)) -o $@ $(LDFLAGS) $(GC_LDFLAGS)
.PHONY: snmp-status
snmp-status: $(BUILDDIR)/test_snmp_status_json
	$(BUILDDIR)/test_snmp_status_json

NODE ?= node
.PHONY: snmp-status-gui
snmp-status-gui:
	cd $(SRCROOT) && $(NODE) test/test_snmp_status_gui.cjs

# Live community/ACL and timestamp behavior with the real packet engine.
$(BUILDDIR)/test_snmp_cli: test_snmp_cli.c test_snmp.c test_httpd_tx.c $(SNMPENV) | $(BUILDDIR)
	$(CC) $(SNMPFLAGS) test_snmp_cli.c $(SNMPENV) -o $@ $(LDFLAGS)
.PHONY: snmp-cli
snmp-cli: $(BUILDDIR)/test_snmp_cli
	$(BUILDDIR)/test_snmp_cli

# Preserve the full SNMP suites as an explicit CI entry point.
.PHONY: snmp-all snmp-replay snmp-gui
snmp-all: snmp snmp-horaco-profile snmp-status snmp-cli snmp-replay snmp-gui
snmp-replay:
	cd $(SRCROOT) && REPLAY_CC="$(CC)" python3 test/test_snmp_control_replay.py
snmp-gui:
	cd $(SRCROOT) && $(NODE) test/test_snmp_status_gui.cjs
	cd $(SRCROOT) && $(NODE) test/test_snmp_switch_gui.cjs
	cd $(SRCROOT) && $(NODE) test/test_snmp_switch_save.cjs
	cd $(SRCROOT) && $(NODE) test/test_snmp_privacy_gui.cjs

$(addprefix $(BUILDDIR)/,test_snmp test_snmp_nine test_snmp_horaco_profile test_snmp_status_json test_snmp_cli test_snmp_transport test_snmp_transport_nine): snmp_setup.h $(SRCROOT)/snmp_internal.h $(SRCROOT)/snmp.h

.PHONY: config-replay
config-replay:
	cd $(SRCROOT) && CC="$(CC)" python3 test/test_config_replay.py
