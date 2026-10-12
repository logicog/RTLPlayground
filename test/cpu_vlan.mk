# Invoke: make -C test -f Makefile -f cpu_vlan.mk run-cpu-vlan
CPU_TESTS=build/test_cpu_vlan_full build/test_cpu_vlan_restricted build/test_cpu_vlan_8372
build/test_cpu_vlan_full: test_cpu_vlan.c $(TABLE_ENV) | build
	$(CC) $(CFLAGS) -no-pie $(filter %.c,$^) -o $@ $(LDFLAGS)
build/test_cpu_vlan_restricted: test_cpu_vlan.c $(TABLE_ENV) | build
	$(CC) $(CFLAGS) -no-pie -DRTLP_TEST_MAX_PORT=7 $(filter %.c,$^) -o $@ $(LDFLAGS)
build/test_cpu_vlan_8372: test_cpu_vlan.c $(TABLE_ENV) | build
	$(CC) $(CFLAGS) -no-pie -DRTLP_TEST_MAX_PORT=7 -DRTLP_TEST_MIN_PORT=3 -DRTLP_TEST_IS8373=0 $(filter %.c,$^) -o $@ $(LDFLAGS)
run-cpu-vlan: $(CPU_TESTS)
	@for t in $(CPU_TESTS); do $$t || exit 1; done
