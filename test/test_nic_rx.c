#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_nic.h"

uint8_t rx_headers[16];

/* Model the DMA behavior reported on SWTGW218AS: a transfer starting inside
 * the RX ring wraps at its end. An initial address outside the ring instead
 * reads flat ASIC memory, including stale TX bytes at 0x2800. */
static uint8_t asic[0x8000];
static struct {
	uint8_t before[16];
	uint8_t data[UIP_CONF_BUFFER_ALLOC_SIZE];
	uint8_t after[16];
} destination;

struct dma_request {
	uint8_t *buffer;
	uint16_t ring;
	uint8_t words;
};
static struct dma_request requests[8];
static unsigned request_count, fail_request;
static int failures;

static void check(bool condition, const char *what)
{
	if (!condition) {
		printf("  FAIL: %s\n", what);
		failures++;
	}
}

bool nic_rx_dma(uint8_t *buffer, uint16_t ring, uint8_t words)
{
	size_t bytes = (size_t)words * 8;
	unsigned call = ++request_count;

	if (call <= sizeof(requests) / sizeof(requests[0]))
		requests[call - 1] = (struct dma_request){ buffer, ring, words };
	if (call == fail_request || !words || (size_t)ring + bytes > sizeof(asic))
		return false;
	if (ring < RTL837X_NIC_RX_RING_BYTES) {
		for (size_t i = 0; i < bytes; i++)
			buffer[i] = asic[((size_t)ring + i) % RTL837X_NIC_RX_RING_BYTES];
	} else {
		memcpy(buffer, asic + ring, bytes);
	}
	return true;
}

static void fixture_reset(void)
{
	memset(asic, 0xd7, sizeof(asic));
	memset(&destination, 0xa5, sizeof(destination));
	memset(rx_headers, 0x6c, sizeof(rx_headers));
	memset(requests, 0, sizeof(requests));
	request_count = 0;
	fail_request = 0;
}

static bool filled(const uint8_t *data, size_t len, uint8_t value)
{
	while (len--)
		if (*data++ != value)
			return false;
	return true;
}

static bool guards_intact(void)
{
	return filled(destination.before, sizeof(destination.before), 0xa5)
		&& filled(destination.after, sizeof(destination.after), 0xa5);
}

static bool requests_start_in_rx_ring(void)
{
	for (unsigned i = 0; i < request_count; i++) {
		if (i >= sizeof(requests) / sizeof(requests[0]) || !requests[i].words
		    || (requests[i].ring & 7)
		    || requests[i].ring >= RTL837X_NIC_RX_RING_BYTES)
			return false;
	}
	return true;
}

static void ring_write(uint16_t offset, const uint8_t *data, size_t len)
{
	for (size_t i = 0; i < len; i++)
		asic[((size_t)offset + i) % RTL837X_NIC_RX_RING_BYTES] = data[i];
}

static void put_descriptor(uint16_t offset, uint16_t len)
{
	uint8_t descriptor[8] = { 0x03, 0x00, 0xb0, 0xc8, len, len >> 8, 0, 0 };
	ring_write(offset, descriptor, sizeof(descriptor));
}

static void expect_descriptor(uint16_t len)
{
	uint8_t descriptor[8] = { 0x03, 0x00, 0xb0, 0xc8, len, len >> 8, 0, 0 };
	check(!memcmp(rx_headers, descriptor, sizeof(descriptor)), "the first eight descriptor bytes stay intact");
}

static void pattern(uint8_t *data, size_t len)
{
	for (size_t i = 0; i < len; i++)
		data[i] = (uint8_t)(i * 37 + (i >> 8) + 0x13);
}

static void t_last_descriptor(void)
{
	static const uint8_t root_bpdu[68] = {
		0x01, 0x80, 0xc2, 0, 0, 0, 0xd0, 0xea, 0x11, 0x12, 0x34, 0x56,
		0x88, 0x99, 0x04, 0, 0, 0, 0, 8,
		0, 0x27, 0x42, 0x42, 3, 0, 0, 2, 2, 0x3c,
		0x10, 0, 0xd0, 0xea, 0x11, 0x12, 0x34, 0x56, 0, 0, 0, 0,
		0x10, 0, 0xd0, 0xea, 0x11, 0x12, 0x34, 0x56, 0x80, 0x0a,
		0, 0, 0x14, 0, 2, 0, 0x0f, 0, 0
	};
	uint8_t stale_tx[72] = { 0 };
	uint16_t last = RTL837X_NIC_RX_RING_BYTES - 8;

	printf("[test] the last RX descriptor wraps its BPDU payload before DMA\n");
	fixture_reset();
	put_descriptor(last, sizeof(root_bpdu));
	ring_write(0, root_bpdu, sizeof(root_bpdu));
	memcpy(stale_tx, root_bpdu, sizeof(root_bpdu));
	stale_tx[17] = 0x20;
	stale_tx[19] = 1; /* retained TX destination mask, not ingress port 8 */
	memcpy(asic + RTL837X_NIC_RX_RING_BYTES, stale_tx, sizeof(stale_tx));
	check(nic_rx_header(last), "the final descriptor is readable");
	expect_descriptor(sizeof(root_bpdu));
	check(nic_rx_packet(destination.data, last + 8), "a payload address at the RX ring limit wraps to zero");
	check(!memcmp(destination.data, root_bpdu, sizeof(root_bpdu)), "the complete upstream BPDU is read instead of stale TX bytes");
	check(destination.data[19] == 8, "the upstream ingress tag survives the ring boundary");
	check(request_count == 2 && requests[0].ring == last && requests[0].words == 1
	      && requests[1].ring == 0 && requests[1].words == 9,
	      "descriptor and rounded payload DMA both start in the RX ring");
	check(requests_start_in_rx_ring() && guards_intact(), "DMA starts inside the RX ring and preserves destination guards");
	expect_descriptor(sizeof(root_bpdu));
}

static void t_payload_layouts(void)
{
	static const struct {
		uint16_t descriptor, len;
	} cases[] = {
		{ 0x0100, 8 }, { 0x0100, 16 }, { 0x0100, 64 },
		{ 0x0100, 68 }, { 0x0100, 72 },
		{ RTL837X_NIC_RX_RING_BYTES - 16, 8 },
		{ RTL837X_NIC_RX_RING_BYTES - 16, 16 },
		{ RTL837X_NIC_RX_RING_BYTES - 16, 68 },
		{ RTL837X_NIC_RX_RING_BYTES - 72, 64 },
		{ RTL837X_NIC_RX_RING_BYTES - 48, 68 }
	};
	uint8_t frame[80];

	printf("[test] payload DMA handles ordinary, exact-boundary, and crossing frames\n");
	pattern(frame, sizeof(frame));
	for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		size_t rounded = (cases[i].len + 7u) & ~7u;
		fixture_reset();
		put_descriptor(cases[i].descriptor, cases[i].len);
		ring_write(cases[i].descriptor + 8, frame, rounded);
		check(nic_rx_header(cases[i].descriptor), "the descriptor transfer completes");
		check(nic_rx_packet(destination.data, cases[i].descriptor + 8), "the payload transfer completes");
		check(!memcmp(destination.data, frame, rounded), "every payload and permitted padding byte reaches the correct destination offset");
		check(filled(destination.data + rounded, sizeof(destination.data) - rounded, 0xa5), "DMA does not write beyond the rounded frame");
		check(request_count == 2, "the payload uses one DMA request even when crossing the ring end");
		check(requests_start_in_rx_ring() && guards_intact(), "crossing DMA starts inside the RX ring and preserves destination guards");
		expect_descriptor(cases[i].len);
	}
}

static void t_maximum_frames(void)
{
	uint8_t frame[(UIP_CONF_BUFFER_SIZE + 7u) & ~7u];
	static const uint16_t lengths[] = { 1, 7, 1549, 1550, 1551, 1552, 1553, 1554, 1555, 1556 };

	printf("[test] rounded maximum frames fit without overwriting their descriptor\n");
	pattern(frame, sizeof(frame));
	for (unsigned layout = 0; layout < 3; layout++) {
		uint16_t descriptor = layout == 0 ? 0x0100
			: RTL837X_NIC_RX_RING_BYTES - (layout == 1 ? 16 : 8);
		for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
			uint16_t len = lengths[i];
			size_t rounded = (len + 7u) & ~7u;
			fixture_reset();
			put_descriptor(descriptor, len);
			ring_write(descriptor + 8, frame, rounded);
			check(nic_rx_header(descriptor), "the maximum-frame descriptor transfer completes");
			check(nic_rx_packet(destination.data, descriptor + 8), "all supported frame lengths fit the destination");
			check(!memcmp(destination.data, frame, rounded), "the full rounded frame is preserved");
			check(filled(destination.data + rounded, sizeof(destination.data) - rounded, 0xa5), "rounded DMA writes no bytes beyond the available destination");
			check(request_count == 2, "maximum frames use a single payload DMA request");
			check(requests_start_in_rx_ring() && guards_intact(), "rounded DMA starts inside the RX ring and preserves destination guards");
			expect_descriptor(len);
		}
	}
}

static void t_invalid_frames(void)
{
	static const uint16_t invalid_lengths[] = { 0, UIP_CONF_BUFFER_SIZE + 1, 0xffff };
	static const uint16_t invalid_pointers[] = { 1, 7, RTL837X_NIC_RX_RING_BYTES + 8, 0xfff8 };

	printf("[test] invalid lengths and DMA addresses are rejected before transfer\n");
	for (unsigned i = 0; i < sizeof(invalid_lengths) / sizeof(invalid_lengths[0]); i++) {
		fixture_reset();
		rx_headers[4] = invalid_lengths[i];
		rx_headers[5] = invalid_lengths[i] >> 8;
		check(!nic_rx_packet(destination.data, 0x0100), "unsupported or overflowing lengths are rejected");
		check(!request_count && filled(destination.data, sizeof(destination.data), 0xa5) && guards_intact(),
		      "invalid frame lengths cause no DMA or destination changes");
	}
	for (unsigned i = 0; i < sizeof(invalid_pointers) / sizeof(invalid_pointers[0]); i++) {
		fixture_reset();
		rx_headers[4] = 68;
		rx_headers[5] = 0;
		check(!nic_rx_header(invalid_pointers[i]), "an invalid descriptor address is rejected");
		check(!nic_rx_packet(destination.data, invalid_pointers[i]), "an invalid payload address is rejected");
		check(!request_count && guards_intact(), "invalid addresses never reach DMA");
	}
	fixture_reset();
	check(!nic_rx_header(RTL837X_NIC_RX_RING_BYTES) && !request_count,
	      "a descriptor at the ring limit is rejected rather than wrapped");
}

static void t_dma_failure(void)
{
	uint8_t frame[UIP_CONF_BUFFER_ALLOC_SIZE];
	static const uint16_t lengths[] = { 68, UIP_CONF_BUFFER_SIZE };

	printf("[test] failed descriptor and single-payload DMA transfers propagate failure\n");
	fixture_reset();
	fail_request = 1;
	check(!nic_rx_header(0x0100) && request_count == 1, "descriptor DMA failure reaches the caller");
	pattern(frame, sizeof(frame));
	for (unsigned layout = 0; layout < 3; layout++) {
		uint16_t descriptor = layout == 0 ? 0x0100
			: RTL837X_NIC_RX_RING_BYTES - (layout == 1 ? 16 : 8);
		for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
			uint16_t len = lengths[i];
			fixture_reset();
			put_descriptor(descriptor, len);
			ring_write(descriptor + 8, frame, (len + 7u) & ~7u);
			check(nic_rx_header(descriptor), "the descriptor precedes the injected payload failure");
			fail_request = 2;
			check(!nic_rx_packet(destination.data, descriptor + 8), "payload DMA failure reaches the caller");
			check(request_count == fail_request, "a failed payload DMA is not retried or split");
			check(filled(destination.data, sizeof(destination.data), 0xa5) && guards_intact(),
			      "an injected failure before DMA preserves the destination and guards");
			expect_descriptor(len);
		}
	}
}

int main(void)
{
	t_last_descriptor();
	t_payload_layouts();
	t_maximum_frames();
	t_invalid_frames();
	t_dma_failure();
	if (failures) {
		printf("%d NIC RX check(s) failed\n", failures);
		return 1;
	}
	puts("all NIC RX checks passed");
	return 0;
}
