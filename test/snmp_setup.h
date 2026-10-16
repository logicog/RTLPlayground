#ifndef TEST_SNMP_SETUP_H
#define TEST_SNMP_SETUP_H
#include <assert.h>
#include <stdio.h>
#include "snmp.h"
#include "snmp_internal.h"
/* Fixture convenience only; every mutation uses the real CLI setters. */
static bool test_snmp_configure(const uint8_t *community, uint8_t length, const uint8_t *network,
				const uint8_t *mask)
{
	char cidr[19];
	unsigned prefix = 0;
	if (!length) {
		snmp_init();
		return true;
	}
	for (unsigned i = 0; i < 4; i++)
		for (unsigned bit = 128; bit; bit >>= 1)
			prefix += !!(mask[i] & bit);
	int size = snprintf(cidr, sizeof(cidr), "%u.%u.%u.%u/%u", network[0], network[1],
			    network[2], network[3], prefix);
	assert(size > 0 && (unsigned)size < sizeof(cidr));
	return snmp_set_manager((const uint8_t *)cidr, size) &&
	       snmp_set_community(community, length) && snmp_set_enabled(1);
}
#endif
