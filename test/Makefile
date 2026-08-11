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
CFLAGS   = -std=gnu11 -g -O1 -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas \
           -Wno-builtin-declaration-mismatch -DRTLP_HOST_TEST \
           -fsanitize=address,undefined -fno-omit-frame-pointer \
           -include sdcc_shim.h -I. -I$(SRCROOT) -I$(SRCROOT)/httpd -I$(SRCROOT)/uip
LDFLAGS  = -fsanitize=address,undefined

BUILDDIR = build

# One binary per subsystem under test.
TESTS = $(BUILDDIR)/test_cmd_editor

all: run

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# cmd_editor.c is compiled UNMODIFIED from the firmware tree.
$(BUILDDIR)/test_cmd_editor: test_cmd_editor.c support.c $(SRCROOT)/cmd_editor.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

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
