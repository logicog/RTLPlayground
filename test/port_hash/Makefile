test: port_lag_members_set.inc
	gcc -Wall -Wextra -O1 -o test_lag_hash test_lag_hash.c && ./test_lag_hash
port_lag_members_set.inc: ../../rtl837x_port.c
	awk '/^void port_lag_members_set/,/^}/' $< > $@
clean:
	rm -f test_lag_hash port_lag_members_set.inc
