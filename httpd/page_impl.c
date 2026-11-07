// #define REGDBG 1

#include "rtl837x_sfr.h"
#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_storm.h"
#include "rtl837x_port.h"
#include "rtl837x_flash.h"
#include "rtl837x_pins.h"
#include "uip.h"
#include "html_data.h"
#include <stdint.h>
#include "phy.h"
#include "version.h"
#include "machine.h"
#include "rtl837x_stp.h"
#include "rtl837x_lacp.h"
#include "page_impl.h"
#include "syslog.h"
#include "dhcp.h"

// #define DEBUG
#include "debug.h"

#define L2_MAX_TRANSFER 30

#pragma codeseg BANK1
#pragma constseg BANK1

extern __code const struct machine machine;
extern __xdata uint8_t outbuf[TCP_OUTBUF_SIZE];
extern __xdata uint16_t slen;
extern __xdata uint16_t management_vlan;
extern __xdata uint16_t session_timeout;
extern __xdata struct dhcp_state dhcp_state;
extern __xdata uint16_t cont_len;
extern __xdata uint32_t cont_addr;
extern __code const uint8_t * __code const hex;
extern __xdata uip_ipaddr_t uip_hostaddr, uip_draddr, uip_netmask;

extern __xdata uint8_t sfr_data[4];
extern __xdata uint8_t sfp_pins_last;
extern __xdata uint8_t vlan_names[VLAN_NAMES_SIZE];

extern __xdata uint8_t cmd_history[CMD_HISTORY_SIZE];
extern __xdata uint16_t cmd_history_ptr;

extern __xdata struct flash_region_t flash_region;

extern __xdata char sfp_module_vendor[2][17];
extern __xdata char sfp_module_model[2][17];
extern __xdata char sfp_module_serial[2][17];
extern __xdata uint8_t sfp_options[2];

extern __xdata uint16_t lldp_logical_port_status;
extern __xdata bool lldp_enabled;


__code const char * __code const http_storm_names[STORM_TYPES] = { "bcast", "mcast", "ucast", "umcast" };

static void str_x_to_html(__xdata const char *s)
{
	while (*s)
		outbuf[slen++] = *s++;
}

// Convert uint8_t to ascii HEX char push on html-buffer.
void charhex_to_html(char c)
{
	outbuf[slen++] = itohex(c);
}

void json_char_to_html(uint8_t c)
{
	if (c < 0x20 || c > 0x7e || c == '"' || c == '\\')
		c = '.';
	outbuf[slen++] = c;
}

void itoa16_html(uint16_t v) /* up to five digits (65535) */
{
	uint8_t print_zeros = 0;
	uint8_t d;
	d = v / 10000;
	if (d) { char_to_html('0' + d); print_zeros = 1; }
	d = (v / 1000) % 10;
	if (d || print_zeros) { char_to_html('0' + d); print_zeros = 1; }
	d = (v / 100) % 10;
	if (d || print_zeros) { char_to_html('0' + d); print_zeros = 1; }
	d = (v / 10) % 10;
	if (d || print_zeros) char_to_html('0' + d);
	char_to_html('0' + (v % 10));
}

static void temp_to_html(void)
{
	int16_t v;

	reg_read(RTL837X_TM_RESULT);
	v = SFR_DATA_U16;
	if (v < 0) {
		char_to_html('-');
		v = -v;
	}
	itoa_html(v >> 7);
	char_to_html('.');
	itoa_html(((v & 0x7f) * 10) >> 7);
	char_to_html(' ');
	char_to_html('C');
}

uint16_t stat_content(void)
{
	dbg_string("stat_content called\n");
	return 0;
}


uint16_t port_status(void)
{
	dbg_string("port_status called\n");
	return 0;
}

/* Converts sfr_data[] into raw hex string.
   Suppress leading zeros.
*/
void sfr_data_to_html(void)
{
 	uint8_t print_zeros = 0;
	uint8_t val = 0;

	for (uint8_t nibble = 0; nibble < 8; nibble++) {
	  	if (!(nibble & 1))
	        val = sfr_data[nibble>>1];
		// force the swap instruction, itohex() ignores the upper nibble.
		val = (val << 4) | (val >> 4);
		// when print_zeros is not zero, we know that a non-zero number has printed.
		// That have to print all the next numbers.
		print_zeros |= val;
		// only care about lower nibble, that is what is printed.
		print_zeros &= 0x0f;
		if (print_zeros)
			charhex_to_html(val);
	}
	if (print_zeros == 0) {
	    char_to_html('0');
	}
}


void reg_to_html(uint16_t reg)
{
	reg_read_m(reg);
	sfr_data_to_html();
}


void reg_to_html_long(uint16_t reg)
{
	reg_read_m(reg);
	byte_to_html(sfr_data[0]);
	byte_to_html(sfr_data[1]);
	byte_to_html(sfr_data[2]);
	byte_to_html(sfr_data[3]);
}


/* The 64-bit counter of the last STAT_GET as one number: the low word keeps
 * its leading zeros whenever the high word is set. */
void counter_to_html(void)
{
	reg_read_m(RTL837X_STAT_V_HIGH);
	if (SFR_DATA_24 | SFR_DATA_16 | SFR_DATA_8 | SFR_DATA_0) {
		sfr_data_to_html();
		reg_to_html_long(RTL837X_STAT_V_LOW);
	} else {
		reg_to_html(RTL837X_STAT_V_LOW);
	}
}


void send_sfp_info(uint8_t sfp)
{
	// This loops over the Vendor-name, Vendor OUI, Vendor PN and Vendor rev ASCII fields
	for (uint8_t i = 16; i < 64; i++) {
		if (!(i & 0xf) && !sfp_read_block(sfp, i, 16))
			return;
		if (i < 20 || i >= 60 || (i >= 36 && i < 40)) // Skip Non-ASCII codes
			continue;
		uint8_t c = sfp_buf[i & 0xf];
		if (c)
			json_char_to_html(c);
	}
}


void sfp_send_data(uint8_t slot, uint8_t reg, uint8_t len)
{
	// maximum supported transfer size is 16 bytes
	if (len > 16)
		return;

	if (!sfp_read_block(slot, reg, len))
		return;

	for (uint8_t i = 0; i < len; i++)
		byte_to_html(sfp_buf[i]);
}


void send_basic_info(void)
{
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);
	dbg_string("send_basic_info called\n");
	str_code_to_html("{" JSONQUOTE("ip_address") ":\"");
	itoa_html(uip_hostaddr[0]); char_to_html('.');
	itoa_html(uip_hostaddr[0] >> 8); char_to_html('.');
	itoa_html(uip_hostaddr[1]); char_to_html('.');
	itoa_html(uip_hostaddr[1] >> 8);
	str_code_to_html("\"," JSONQUOTE("ip_gateway") ":\"");
	itoa_html(uip_draddr[0]); char_to_html('.');
	itoa_html(uip_draddr[0] >> 8); char_to_html('.');
	itoa_html(uip_draddr[1]); char_to_html('.');
	itoa_html(uip_draddr[1] >> 8);
	str_code_to_html("\"," JSONQUOTE("ip_netmask") ":\"");
	itoa_html(uip_netmask[0]); char_to_html('.');
	itoa_html(uip_netmask[0] >> 8); char_to_html('.');
	itoa_html(uip_netmask[1]); char_to_html('.');
	itoa_html(uip_netmask[1] >> 8);
	str_code_to_html("\"," JSONQUOTE("syslog_server") ":\"");
	itoa_html(syslog_state.server_ip[0]); char_to_html('.');
	itoa_html(syslog_state.server_ip[1]); char_to_html('.');
	itoa_html(syslog_state.server_ip[2]); char_to_html('.');
	itoa_html(syslog_state.server_ip[3]); char_to_html(':');
	itoa16_html(syslog_state.server_port);
	str_code_to_html("\"," JSONQUOTE("session_timeout") ":");
	itoa16_html(session_timeout);
	slen += strtox(outbuf + slen, dhcp_state.state == DHCP_OFF ? ("," JSONQUOTE("ip_source") ":" JSONQUOTE("static")) : ("," JSONQUOTE("ip_source") ":" JSONQUOTE("dhcp")));
	slen += strtox(outbuf + slen, "," JSONQUOTE("mac_address") ":\"");
	byte_to_html(uip_ethaddr.addr[0]); char_to_html(':');
	byte_to_html(uip_ethaddr.addr[1]); char_to_html(':');
	byte_to_html(uip_ethaddr.addr[2]); char_to_html(':');
	byte_to_html(uip_ethaddr.addr[3]); char_to_html(':');
	byte_to_html(uip_ethaddr.addr[4]); char_to_html(':');
	byte_to_html(uip_ethaddr.addr[5]);
	str_code_to_html("\"," JSONQUOTE("hostname") ":\"");
	{
		__xdata char *hp = hostname;	/* sanitized on ingest, emit verbatim */
		while (*hp)
			char_to_html(*hp++);
	}
	str_code_to_html("\","JSONQUOTE("sw_ver") ":\"");
	str_code_to_html(VERSION_SW);
	str_code_to_html("\"," JSONQUOTE("build_date") ":\"");
	str_code_to_html(BUILD_DATE);
	str_code_to_html("\"," JSONQUOTE("hw_ver") ":\"");
	str_code_to_html(machine.machine_name);
	str_code_to_html("\"," JSONQUOTE("pcb_ver") ":\"");
	str_code_to_html(MACHINE_PCB_INFO);
	str_code_to_html("\"," JSONQUOTE("chip_temp") ":\"");
	temp_to_html();
	str_code_to_html("\"," JSONQUOTE("flash_size") ":\"");
	str_code_to_html(get_flash_size_str());

	if (machine.n_sfp) {
		str_code_to_html("\"," JSONQUOTE("sfp_slot_0") ":\"");
		send_sfp_info(0);
		if (machine.n_sfp == 2) {
			str_code_to_html("\"," JSONQUOTE("sfp_slot_1") ":\"");
			send_sfp_info(1);
		}
	}
	char_to_html('"');
	char_to_html('}');
}

void send_vlan(uint16_t vlan)
{
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);
	dbg_string("sending VLAN\n");
	vlan_get(vlan);
	str_code_to_html("{" JSONQUOTE("untagged_members") ":[");

	uint32_t m = ((uint32_t)sfr_data[0] << 24) |
				((uint32_t)sfr_data[1] << 16) |
				((uint32_t)sfr_data[2] <<  8) |
				((uint32_t)sfr_data[3]);

	uint16_t members = (uint16_t)(m & 0x3FF);
	uint16_t untagged = (uint16_t)(((m >> 10) & 0x3FF) & members);

	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		if(members & (1<<i)) {
			if((untagged & (1<<i))) {
				itoa_html(machine.log_to_phys_port[i]);
				json_char_to_html(',');
			}
		}
	}
	if(outbuf[slen - 1] == ',') {
		slen--;
	}

	str_code_to_html("]," JSONQUOTE("tagged_members") ":[");
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		if(members & (1<<i)) {
			if(!(untagged & (1<<i))) {
				itoa_html(machine.log_to_phys_port[i]);
				json_char_to_html(',');
			}
		}
	}
	if(outbuf[slen - 1] == ','){
		slen--;
	}

	str_code_to_html("]," JSONQUOTE("name") ":\"");
	__xdata uint16_t n = vlan_name(vlan);
	if (n== 0xffff) {
		dbg_string("VLAN has no name\n");
	} else {
		while(vlan_names[n] && vlan_names[n] != ' ')
			json_char_to_html(vlan_names[n++]);
	}

	str_code_to_html("\"," JSONQUOTE("pvid") ":[");
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		if (port_pvid_get(i) == vlan) { // i is _logical port_
			json_add_array_uint_item(machine.log_to_phys_port[i]);
		}
	}
	char_to_html(']');
	char_to_html('}');
}

/* Send counters
 * Only accepts physical port 1..9.
 * Returns an error if the port physical don't exists.
 */
bool send_counters(uint8_t phys_port)
{
	uint8_t phys_port_idx = phys_port - 1;
	if (phys_port_idx > (MACHINE_MAX_PORT - MACHINE_MIN_PORT))
		goto err;
	uint8_t log_port = machine.phys_to_log_port[phys_port_idx];

	dbg_string("send_counters called: "); dbg_byte(phys_port_idx); dbg_char('\n');
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);
	dbg_string("sending counters\n"); dbg_byte(phys_port_idx);

	char_to_html('[');
	for (uint8_t counter = 0; counter < 0x37; counter++) {
		STAT_GET(counter, log_port);
		str_code_to_html("\"0x");
		counter_to_html();
		char_to_html('\"');
		if (counter != 0x36)
			char_to_html(',');
	}
	char_to_html(']');

	return false;

err:
	dbg_string("Error: counters: phy_port_idx don't exists\n");
	return true;
}


void send_l2(uint16_t idx)
{
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);
	dbg_string("sending L2\n");
	dbg_short(idx);
	__xdata uint8_t entries_left = L2_MAX_TRANSFER;

	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & TBL_EXECUTE);

	/* The L2 table in the ASIC can hold up to 4096 (0x1000) entries, which
	 * are accessed using an index. The index is the hash of the MAC address
	 * and forwarding ID (basically VID). The hash-table is 4-way associative,
	 * i.e. for a given hash value, 4 entries with that same hash can be stored
	 * (i.e. the hash points to a bucket with up to 4 entries).
	 * When the table or a hash bucket is full, further entries will lead to
	 * L2 flooding.
	 * To find all entries, we start with entry-index 0 and iteratively search for
	 * the next entry (with the next higher index), until we arrive again at the first
	 * entry. The indices are sorted, so if an entry has a smaller index than
	 * the previous one, we know that we have wrapped around the entire table.
	 */
	__xdata uint16_t entry = idx & 0xfff;
	__xdata uint16_t first_entry = 0xffff; // An illegal entry index
	__bit first = true;
	char_to_html('[');
	while (1) {
		entries_left--;
		uint8_t port = 0;
		uint8_t lag;
		reg_read_m(RTL837x_TBL_DATA_0);
		REG_WRITE(RTL837x_TBL_DATA_0, sfr_data[0], sfr_data[1] & 0xfc, sfr_data[2] | (TBL_LUTREAD_NEXT_L2UC << 6), sfr_data[3]);

		REG_WRITE(RTL837X_TBL_CTRL, entry >> 8, entry, TBL_L2_UNICAST, TBL_EXECUTE);
		do {
			reg_read_m(RTL837X_TBL_CTRL);
		} while (sfr_data[3] & TBL_EXECUTE);

		reg_read_m(RTL837x_L2_DATA_OUT_B);
		__bit valid = (sfr_data[0] & 0x20) != 0;
		if (valid) {
			/* separator + 74-byte worst-case entry + closing "]" */
			if (slen + 76 > TCP_OUTBUF_SIZE)
				break;
			if (!first)
				char_to_html(',');
			first = false;

			// VLAN, taken from the read above instead of reading the register twice
			str_code_to_html("{" JSONQUOTE("vlan") ":\"");
			charhex_to_html(sfr_data[0] & 0x0f);
			byte_to_html(sfr_data[1]);

			// MAC
			str_code_to_html("\"," JSONQUOTE("mac") ":\"");
			byte_to_html(sfr_data[2]); char_to_html(':');
			byte_to_html(sfr_data[3]); char_to_html(':');
			port = (sfr_data[0] >> 6) & 0x3;
			reg_read_m(RTL837x_L2_DATA_OUT_A);
			byte_to_html(sfr_data[0]); char_to_html(':');
			byte_to_html(sfr_data[1]); char_to_html(':');
			byte_to_html(sfr_data[2]); char_to_html(':');
			byte_to_html(sfr_data[3]);

			// type
			reg_read_m(RTL837x_L2_DATA_OUT_C);
			if (sfr_data[1] & 0x1)
				str_code_to_html("\"," JSONQUOTE("type") ":" JSONQUOTE("s") "," JSONQUOTE("port") ":");
			else
				str_code_to_html("\"," JSONQUOTE("type")":" JSONQUOTE("l") "," JSONQUOTE("port") ":");

			port |= (sfr_data[3] & 0x3) << 2;

			if ( port == CPU_LOGICAL_PORT )
				itoa_html(CPU_PHYSICAL_PORT);
			else
				itoa_html(machine.log_to_phys_port[port]);

			str_code_to_html("," JSONQUOTE("lag") ":");
			lag = port_lag_of(port);
			itoa_html(lag == PORT_LAG_NONE ? 0 : lag + 1);
		}

		// Index
		reg_read_m(RTL837x_TBL_DATA_0);
		entry = (((uint16_t)sfr_data[2] & 0x0f) << 8) | sfr_data[3];
		if (valid) {
			str_code_to_html("," JSONQUOTE("idx") ":\"");
			byte_to_html(entry >> 8);
			byte_to_html(entry);
			char_to_html('"');
			char_to_html('}');
		}
		entry += 1; // We want the next entry following after the current entry

		if (first_entry == 0xffff)
			first_entry = entry;
		else if (first_entry == entry || !entries_left)
			break;
	}
	char_to_html(']');
}


void l2_delete(uint16_t idx)
{
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);
	dbg_string("L2 DELETE\n");
	dbg_short(idx);
	do {
		reg_read(RTL837X_TBL_CTRL);
	} while (SFR_DATA_0 & TBL_EXECUTE);
	str_code_to_html("{\"result\":");
	// First, search for the entry based on the index
	reg_read_m(RTL837x_TBL_DATA_0);
	sfr_data[1] &= 0xfc;
	sfr_data[2] |= (TBL_LUTREAD_NEXT_L2UC << 6);
	reg_write_m(RTL837x_TBL_DATA_0);

	REG_WRITE(RTL837X_TBL_CTRL, (idx >> 8) & 0xf, idx, TBL_L2_UNICAST, TBL_EXECUTE);
	do {
		reg_read(RTL837X_TBL_CTRL);
	} while (SFR_DATA_0 & 0x1);
	reg_read_m(RTL837x_L2_DATA_OUT_B);
	if (!(SFR_DATA_24 & 0x20)) {
		char_to_html('0');
	} else {
		__bit is_our_mac_addr = uip_ethaddr.addr[0] == SFR_DATA_8 && uip_ethaddr.addr[1] == SFR_DATA_0;
		sfr_data[0] &= 0x3f; // Clear SPA
		reg_write_m(RTL837x_TBL_DATA_IN_B);

		// Second half of MAC is copied
		reg_read_m(RTL837x_L2_DATA_OUT_A);
		if (is_our_mac_addr && uip_ethaddr.addr[2] == SFR_DATA_24 && uip_ethaddr.addr[3] == SFR_DATA_16
		    && uip_ethaddr.addr[4] == SFR_DATA_8 && uip_ethaddr.addr[5] == SFR_DATA_0) {
			// the switch's own entry keeps management reachable
			char_to_html('0');
		} else {
			reg_write_m(RTL837x_TBL_DATA_IN_A);

			reg_read_m(RTL837x_L2_DATA_OUT_C);
			sfr_data[3] &= 0xc0; // Clear age, auth and second part of ports
			sfr_data[1] &= 0xfe; // Clear nosalearn
			reg_write_m(RTL837x_TBL_DATA_IN_C);

			reg_read_m(RTL837x_TBL_DATA_0);
			sfr_data[2] = TBL_L2_UNICAST;
			reg_write_m(RTL837x_TBL_DATA_0);

			REG_WRITE(RTL837X_TBL_CTRL, idx >> 8, idx, TBL_L2_UNICAST, TBL_WRITE | TBL_EXECUTE);
			do {
				reg_read(RTL837X_TBL_CTRL);
			} while (SFR_DATA_0 & TBL_EXECUTE);

			char_to_html('1');
		}
	}
	char_to_html('}');
}


void send_mirror(void)
{
	dbg_string("send_mirror called\n");
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);

	reg_read_m(RTL837x_MIRROR_CTRL);
	uint8_t mPort = sfr_data[3];
	if (mPort & 1) {
		str_code_to_html("{" JSONQUOTE("enabled") ":true," JSONQUOTE("mirrorPort") ":");
	} else {
		str_code_to_html("{" JSONQUOTE("enabled") ":false," JSONQUOTE("mirrorPort") ":");
	}
	itoa_html(machine.log_to_phys_port[mPort >> 1]);

	reg_read_m(RTL837x_MIRROR_CONF);
	uint16_t m = (sfr_data[0] << 8) | sfr_data[1];
	str_code_to_html("," JSONQUOTE("mirror_rx") ":[");
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		if((m >> i) & 1) {
			json_add_array_uint_item(machine.log_to_phys_port[i]);
		}
	}
	if ((m >> CPU_LOGICAL_PORT) & 1)
		json_add_array_uint_item(CPU_PHYSICAL_PORT);

	str_code_to_html("]," JSONQUOTE("mirror_tx") ":[");
	m = (sfr_data[2] << 8) | sfr_data[3];
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		if((m >> i ) & 1) {
			json_add_array_uint_item(machine.log_to_phys_port[i]);
		}
	}
	if ((m >> CPU_LOGICAL_PORT) & 1)
		json_add_array_uint_item(CPU_PHYSICAL_PORT);

	char_to_html(']');
	char_to_html('}');
}


void send_lag(void)
{
	dbg_string("send_lag called\n");
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);

	str_code_to_html("{" JSONQUOTE("lags") ":[");
	for (uint8_t l=0; l < 4; l++) {
		bool isLACP = lacp_lag_ports[l] > 0;
		str_code_to_html("{" JSONQUOTE("lagNum") ":");
		itoa_html(l);
		str_code_to_html("," JSONQUOTE("type") ":");

		if (isLACP) {
			str_code_to_html(JSONQUOTE("lacp"));
		} else {
			str_code_to_html(JSONQUOTE("static"));
		}

		str_code_to_html("," JSONQUOTE("members") ":[");
		if(isLACP) {
			/* LACP configured members */
			for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
				if((lacp_lag_ports[l] >> i) & 1) {
					json_add_array_uint_item(machine.log_to_phys_port[i]);
				}
			}
		} else {
			/* Static Members */
			uint16_t ports = port_lag_members_get(l);
			for (uint8_t i = 16; i > 0; i--) {
				if(!!(ports & 0x8000)) {
					json_add_array_uint_item(machine.log_to_phys_port[i-1]);
				}
				ports <<= 1;
			}	
		}
		char_to_html(']');
		if(isLACP) {
			str_code_to_html("," JSONQUOTE("active_members") ":[");
			/* LACP Active Members */
			for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
				if((lacp_members_last[l] >> i) & 1) {
					json_add_array_uint_item(machine.log_to_phys_port[i]);
				}
			}
			str_code_to_html("]," JSONQUOTE("valid") ":");
			bool_to_json(lacp_agg_valid[l]);
			str_code_to_html("," JSONQUOTE("aggregator") ":\"");
			for (uint8_t j = 0; j < MAC_ADDR_LEN; j++)
				byte_to_html(lacp_agg_sys[l][j]);
			char_to_html('"');
		}
		str_code_to_html("," JSONQUOTE("hash") ":\"");
		reg_read_m(RTL837X_TRK_HASH_CTRL_BASE + (l << 2));
		sfr_data_to_html();

		char_to_html('"');
		char_to_html('}');
		char_to_html(',');
	}
	slen -=1; // remove comma
	str_code_to_html("]," JSONQUOTE("ports") ":[");
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		str_code_to_html("{" JSONQUOTE("p") ":");
		itoa_html(machine.log_to_phys_port[i]);
		str_code_to_html("," JSONQUOTE("lag") ":");
		itoa_html(lacp_port_lag[i]);
		str_code_to_html("," JSONQUOTE("a") ":\"");
		byte_to_html(lacp_actor_state[i]);
		str_code_to_html("\"," JSONQUOTE("pt") ":\"");
		byte_to_html(lacp_partner_state[i]);
		str_code_to_html("\"," JSONQUOTE("rs") ":");
		itoa_html(lacp_rx_state[i]);
		str_code_to_html("," JSONQUOTE("rx") ":\"");
		byte_to_html(lacp_rx_count[i] >> 8);
		byte_to_html(lacp_rx_count[i]);
		str_code_to_html("\"," JSONQUOTE("psys") ":\"");
		for (uint8_t j = 0; j < MAC_ADDR_LEN; j++)
			byte_to_html(lacp_partner_sys[i][j]);
		str_code_to_html("\"},");
	}
	slen -= 1; // remove comma
	char_to_html(']');
	char_to_html('}');
}

void send_lldp(void)
{
	dbg_string("send_lldp called\n");
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);

	str_code_to_html("{" JSONQUOTE("on") ":");
	bool_to_json(lldp_enabled);
	str_code_to_html("," JSONQUOTE("portStatus") ":[");

	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		str_code_to_html("{" JSONQUOTE("port") ":");
		itoa_html(machine.log_to_phys_port[i]);

		str_code_to_html("," JSONQUOTE("name") ":\"");
		for (uint8_t j = 0; j < PORT_NAME_SIZE && port_names[i][j]; j++) {
			json_char_to_html(port_names[i][j]);
		}

		str_code_to_html("\"," JSONQUOTE("permitted") ":");
		bool_to_json(1 & (lldp_logical_port_status >> i));
		char_to_html('}');

		if (i < MACHINE_MAX_PORT)
			char_to_html(',');
	}
	char_to_html(']');
	char_to_html('}');
}


void send_eee(void)
{
	dbg_string("send_eee called\nsending EEE status\n");
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);

	reg_read_m(RTL8373_PHY_EEE_ABLTY);
	uint8_t eee_ablty = sfr_data[3];

	char_to_html('[');
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		str_code_to_html("{" JSONQUOTE("port") ":");
		itoa_html(machine.log_to_phys_port[i]);

		if (machine.is_sfp[i]) {
			str_code_to_html("," JSONQUOTE("isSFP") ":true");
		} else {
			str_code_to_html("," JSONQUOTE("isSFP") ":false," JSONQUOTE("advertised") ":[");
			uint16_t v;
			phy_read(i, PHY_MMD_AN, PHY_EEE_ADV2);
			v = SFR_DATA_U16;

			if(v & PHY_EEE_BIT_2G5)
				json_add_array_string_item(JSONQUOTE("2.5G"));

			phy_read(i, PHY_MMD_AN, PHY_EEE_ADV);
			v = SFR_DATA_U16;
			if(v & PHY_EEE_BIT_1G)
				json_add_array_string_item(JSONQUOTE("1G"));
			if(v & PHY_EEE_BIT_100M)
				json_add_array_string_item(JSONQUOTE("100M"));

			phy_read(i, PHY_MMD_AN, PHY_EEE_LP_ABILITY2);
			v = SFR_DATA_U16;
			str_code_to_html("]," JSONQUOTE("linkPartner") ":[");
			if (v & PHY_EEE_BIT_2G5)
				json_add_array_string_item(JSONQUOTE("2.5G"));

			phy_read(i, PHY_MMD_AN, PHY_EEE_LP_ABILITY);
			v = SFR_DATA_U16;
			if(v & PHY_EEE_BIT_1G)
				json_add_array_string_item(JSONQUOTE("1G"));
			if(v & PHY_EEE_BIT_100M)
				json_add_array_string_item(JSONQUOTE("100M"));

			str_code_to_html("]," JSONQUOTE("active") ":");
			bool_to_json(eee_ablty & (1 << i));
		}
		char_to_html('}');
		if (i < MACHINE_MAX_PORT)
			char_to_html(',');
	}
	char_to_html(']');
}


void send_bandwidth(void)
{
	dbg_string("send_bandwidth called\n");
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);
	char_to_html('[');
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		str_code_to_html("{" JSONQUOTE("port") ":");
		itoa_html(machine.log_to_phys_port[i]);
		str_code_to_html("," JSONQUOTE("iLimited") ":");
		reg_read_m(RTL837X_IGBW_PORT_CTRL + i * 4);
		bool_to_json(sfr_data[1] & 0x10);
		str_code_to_html("," JSONQUOTE("iBW") ":\"0x");
		byte_to_html(sfr_data[1] & 0x0f);
		byte_to_html(sfr_data[2]);
		byte_to_html(sfr_data[3]);
		str_code_to_html("\"," JSONQUOTE("iAction") ":");
		if(reg_bit_test(RTL837X_IGBW_PORT_FC_CTRL, i)){
			str_code_to_html(JSONQUOTE("fc"));
		} else {
			str_code_to_html(JSONQUOTE("drop"));
		}
		reg_read_m(RTL837X_EGBW_PORT_CTRL + i * 1024);
		str_code_to_html("," JSONQUOTE("eLimited") ":");
		bool_to_json(sfr_data[1] & 0x10);
		str_code_to_html("," JSONQUOTE("eBW") ":\"0x");
		byte_to_html(sfr_data[1] & 0x0f);
		byte_to_html(sfr_data[2]);
		byte_to_html(sfr_data[3]);
		char_to_html('"');
		char_to_html('}');
		if (i < MACHINE_MAX_PORT)
			char_to_html(',');
	}
	char_to_html(']');
}



void send_storm(void)
{
	__xdata uint8_t i, t, idx;

	slen = strtox(outbuf, HTTP_RESPONSE_JSON);
	char_to_html('[');
	for (i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		str_code_to_html("{" JSONQUOTE("port") ":");
		itoa_html(machine.log_to_phys_port[i]);
		char_to_html(',');

		for (t = 0; t < STORM_TYPES; t++) {
			char_to_html('"');
			str_code_to_html(http_storm_names[t]);
			str_code_to_html("\":{" JSONQUOTE("enabled") ":");
			bool_to_json(reg_bit_test(RTL837X_STORM_CTRL + (t << 2), i));

			str_code_to_html("," JSONQUOTE("unit") ":");
			idx = STORM_METER(i, t);

			if (reg_bit_test(RTL837X_METER_MODE + ((idx >> 5) << 2), idx & 0x1f)) {
				str_code_to_html(JSONQUOTE("pps"));
			} else {
				str_code_to_html(JSONQUOTE("kbit/s"));
			}

			str_code_to_html("," JSONQUOTE("rate") ":\"0x");

			reg_read_m(RTL837X_METER_RATE + (STORM_METER(i, t) << 2));
			byte_to_html(sfr_data[1]);
			byte_to_html(sfr_data[2]);
			byte_to_html(sfr_data[3]);

			char_to_html('"');

			char_to_html('}');
			if(t < STORM_TYPES-1)
				char_to_html(',');
		}
		char_to_html('}');
		if (i < MACHINE_MAX_PORT)
			char_to_html(',');
	}
	char_to_html(']');
}


void send_isolation(void)
{
	__xdata uint16_t allow;

	dbg_string("send_isolation called\n");
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);
	char_to_html('[');
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		str_code_to_html("{" JSONQUOTE("port") ":");
		itoa_html(machine.log_to_phys_port[i]);
		str_code_to_html("," JSONQUOTE("allow") ":[");
		allow = port_isolation_get(i);
		for (uint8_t j = MACHINE_MIN_PORT; j <= MACHINE_MAX_PORT; j++) {
			if ((allow >> j) & 1) {
				json_add_array_uint_item(machine.log_to_phys_port[j]);
			}
		}
		if((allow >> CPU_LOGICAL_PORT) & 1)
			json_add_array_uint_item(CPU_PHYSICAL_PORT);
		
		str_code_to_html("]}");
		if (i < MACHINE_MAX_PORT)
			char_to_html(',');
	}
	char_to_html(']');
}

void send_status(void)
{
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);
	dbg_string("sending status\n");
	char_to_html('[');

	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		str_code_to_html("{" JSONQUOTE("port") ":");
		itoa_html(machine.log_to_phys_port[i]);
		str_code_to_html("," JSONQUOTE("name") ":\"");
		for (uint8_t j = 0; j < PORT_NAME_SIZE && port_names[i][j]; j++) {
			json_char_to_html(port_names[i][j]);
		}
		char_to_html('"');

		if (machine.is_sfp[i]) {
			uint8_t sfp = machine.is_sfp[i] - 1;
			str_code_to_html("," JSONQUOTE("isSFP") ":true," JSONQUOTE("enabled") ":");
			if (!(sfp_pins_last & (0x1 << (sfp << 2)))) {
				bool_to_json(1);
				str_code_to_html("," JSONQUOTE("sfp_options") ":\"0x");
				byte_to_html(sfp_options[sfp]);
				if (sfp_options[sfp] & 0x40) {
					str_code_to_html("\"," JSONQUOTE("sfp_temp") ":\"0x");
					sfp_send_data(sfp, 224, 2);
					str_code_to_html("\"," JSONQUOTE("sfp_vcc") ":\"0x");
					sfp_send_data(sfp, 226, 2);
					str_code_to_html("\"," JSONQUOTE("sfp_txbias") ":\"0x");
					sfp_send_data(sfp, 228, 2);
					str_code_to_html("\"," JSONQUOTE("sfp_txpower") ":\"0x");
					sfp_send_data(sfp, 230, 2);
					str_code_to_html("\"," JSONQUOTE("sfp_rxpower") ":\"0x");
					sfp_send_data(sfp, 232, 2);
					if (sfp_options[sfp] & 0x10) {
						str_code_to_html("\"," JSONQUOTE("sfp_temp_cal") ":\"0x");
						sfp_send_data(sfp, 212, 4);
						str_code_to_html("\"," JSONQUOTE("sfp_vcc_cal") ":\"0x");
						sfp_send_data(sfp, 216, 4);
						str_code_to_html("\"," JSONQUOTE("sfp_txbias_cal") ":\"0x");
						sfp_send_data(sfp, 204, 4);
						str_code_to_html("\"," JSONQUOTE("sfp_txpower_cal") ":\"0x");
						sfp_send_data(sfp, 208, 4);
						str_code_to_html("\"," JSONQUOTE("sfp_rxpower_cal") ":\"0x");
						sfp_send_data(sfp, 184, 16);
						sfp_send_data(sfp, 200, 4);
					}
					str_code_to_html("\"," JSONQUOTE("sfp_state") ":\"0x");
					sfp_send_data(sfp, 238, 1);
				}
				str_code_to_html("\"," JSONQUOTE("sfp_vendor") ":\"");
				str_x_to_html(sfp_module_vendor[sfp]);
				str_code_to_html("\"," JSONQUOTE("sfp_model") ":\"");
				str_x_to_html(sfp_module_model[sfp]);
				str_code_to_html("\"," JSONQUOTE("sfp_serial") ":\"");
				str_x_to_html(sfp_module_serial[sfp]);
				str_code_to_html("\"," JSONQUOTE("sfp_los") ":");
				if (machine.sfp_port[sfp].pin_los == GPIO_NA) {
					str_code_to_html("null");
				} else {
					bool_to_json(sfp_pins_last & (0x2 << (sfp << 2)));
				}
			} else {
				bool_to_json(0);
			}
		} else {
			str_code_to_html("," JSONQUOTE("isSFP") ":false," JSONQUOTE("enabled") ":");
			phy_read(i, PHY_MMD31, 0xa610);
			bool_to_json(SFR_DATA_8 == 0x20);
			str_code_to_html("," JSONQUOTE("adv") ":[");

			phy_read(i, PHY_MMD_AN, PHY_ANEG_MGBASE_CTRL);
			uint16_t w = SFR_DATA_U16;
			if(!!(w & 0x80))
				json_add_array_string_item(JSONQUOTE("2.5G"));

			phy_read(i, PHY_MMD31, PHY_MMD31_GBCR);
			w = SFR_DATA_U16;
			if(!!(w & 0x0200))
				json_add_array_string_item(JSONQUOTE("1G"));
				
			phy_read(i, PHY_MMD_AN, PHY_ANEG_ADV);
			w = SFR_DATA_U16;
			if(!!(w & 0x0100))
				json_add_array_string_item(JSONQUOTE("100M-FD"));
			if(!!(w & 0x80))
				json_add_array_string_item(JSONQUOTE("100M-HD"));
			if(!!(w & 0x40))
				json_add_array_string_item(JSONQUOTE("10M-FD"));
			if(!!(w & 0x20))
				json_add_array_string_item(JSONQUOTE("10M-HD"));

			char_to_html(']');
		}

		str_code_to_html("," JSONQUOTE("link") ":\"");
		
		uint8_t b = 0;
		// Determine link state
		reg_read_m(RTL837X_REG_LINKS_STS);
		if(!((sfr_data[(i / 8) + 1] >> (i % 8 ) & 1))) {
			str_code_to_html("DOWN");
		}
		else
		{
			//Determine link speed
			if (i < 8)
				reg_read_m(RTL837X_REG_LINKS);
			else
				reg_read_m(RTL837X_REG_LINKS_89);
			
			b = sfr_data[3 - ((i & 7) >> 1)];
			b = ((i & 1) ? b >> 4 : b & 0xf);
			switch(b){
				case 0:
					str_code_to_html("10M");
				break;
				case 1:
					str_code_to_html("100M");
				break;
				case 2:
					str_code_to_html("1000M");
				break;
				case 3:
					str_code_to_html("500M");
				break;
				case 4:
					str_code_to_html("10G");
				break;
				case 5:
					str_code_to_html("2.5G");
				break;
				case 6:
					str_code_to_html("5G");
				break;
			}
		}

		str_code_to_html("\"," JSONQUOTE("mtu") ":\"0x");
		reg_read_m(RTL8373_REG_MAC_L2_PORT_MAX_LEN + ((uint16_t) i << 8));
		uint16_t mtu = SFR_DATA_U16 & 0x3fff;
		byte_to_html(mtu >> 8);
		byte_to_html(mtu & 0xff);

		STAT_GET(STAT_COUNTER_TX_PKTS, i);
		str_code_to_html("\"," JSONQUOTE("txG") ":\"0x");
		counter_to_html();

		str_code_to_html("\"," JSONQUOTE("txB") ":\"0x");
		STAT_GET(STAT_COUNTER_ERR_PKTS, i);
		reg_to_html(RTL837X_STAT_V_LOW);	// 32 bit Tx Packet errors

		str_code_to_html("\"," JSONQUOTE("rxG") ":\"0x");
		STAT_GET(STAT_COUNTER_RX_PKTS, i);
		counter_to_html();

		str_code_to_html("\"," JSONQUOTE("rxB") ":\"0x");
		STAT_GET(STAT_COUNTER_ERR_PKTS, i);
		reg_to_html(RTL837X_STAT_V_HIGH);	// 32bit RX packet errors

		str_code_to_html("\"," JSONQUOTE("macs") ":\"0x");
		reg_to_html(RTL837X_L2_LRN_PORT_CONSTRT_CNT + (i << 2));
		char_to_html('"');
		char_to_html('}');

		//298 chars worst case for a copper port, SPF is about 650
		if(slen > TCP_OUTBUF_SIZE - 650) {
			dbg_string("send_status truncated");
			break;
		}

		if (i < MACHINE_MAX_PORT)
			char_to_html(',');
	}
	char_to_html(']');
}


void send_config(void)
{
	dbg_string("send_config called\n");
	__xdata uint32_t pos = CONFIG_START;
	__xdata uint16_t valid_len = 0;
	__xdata uint16_t len_left = CONFIG_LEN;
	__xdata uint8_t flash_buf[256];
	
	slen = strtox(outbuf, HTTP_RESPONSE_TXT);
	
	// Scan through config to find the end of valid data (null terminator)
	do {
		flash_region.addr = pos;
		flash_region.len = (len_left > 256) ? 256 : len_left;
		flash_read_bulk(flash_buf);
		
		// Look for null terminator in this chunk
		for (uint16_t i = 0; i < flash_region.len; i++) {
			if (flash_buf[i] == 0) {
				// Found end of valid data
				valid_len += i;
				goto found_end;
			}
		}
		
		valid_len += flash_region.len;
		len_left -= flash_region.len;
		pos += flash_region.len;
	} while (len_left > 0);
	
found_end:
	// Now send the valid data
	if (valid_len > (TCP_OUTBUF_SIZE - slen)) {
		cont_len = valid_len - (TCP_OUTBUF_SIZE - slen);
		valid_len = TCP_OUTBUF_SIZE - slen;
		cont_addr = CONFIG_START + valid_len;
	}
	
	flash_region.addr = CONFIG_START;
	flash_region.len = valid_len;
	flash_read_bulk(outbuf + slen);
	slen += valid_len;
}

void send_cmd_log(void)
{
	dbg_string("send_cmd_log called\n");
	slen = strtox(outbuf, HTTP_RESPONSE_TXT);
	__xdata uint16_t p = (cmd_history_ptr + 1) & CMD_HISTORY_MASK;
	__xdata uint8_t found_begin = 0;
	dbg_string("History ptr: ");
	dbg_short(cmd_history_ptr); dbg_char('\n');
	while (p != cmd_history_ptr && slen < TCP_OUTBUF_SIZE) {
		if (!cmd_history[p] || cmd_history[p] == '\n')
			found_begin = 1;
		if (found_begin && cmd_history[p])
			outbuf[slen++] = cmd_history[p];
		p = (p + 1) & CMD_HISTORY_MASK;
	}
}


void send_vlanlist(void)
{
	/* Worst case per entry: {"id":4094,"name":"<117-char name>"} = 138 bytes
	 * (name bound: CMD_BUF_SIZE=128 minus command prefix); +1 for closing ']'.
	 * At worst case ~18 VLANs fit; typical configs with short names fit many more. */
	__xdata uint16_t i;
	__xdata uint16_t n;
	uint8_t first = 1;

	slen = strtox(outbuf, HTTP_RESPONSE_JSON);
	str_code_to_html("{" JSONQUOTE("mgmt") ":");
	itoa16_html(management_vlan);
	str_code_to_html("," JSONQUOTE("vlan") ":[");

	for (i = 1; i < 4095; i++) {
		vlan_get(i);
		if (!(sfr_data[0] & 0x02)) /* bit 1: VLAN table entry valid */
			continue;

		if (slen + 141 > TCP_OUTBUF_SIZE) /* comma + 138-byte worst-case entry + closing "]}" */
			break;

		if (!first)
			char_to_html(',');
		first = 0;

		str_code_to_html("{" JSONQUOTE("id") ":");
		itoa16_html(i);
		str_code_to_html("," JSONQUOTE("name") ":\"");

		n = vlan_name(i);
		if (n != 0xffff) {
			while (vlan_names[n] && vlan_names[n] != ' ')
				json_char_to_html(vlan_names[n++]);
		}

		char_to_html('"');
		char_to_html('}');

	}
	char_to_html(']');
	char_to_html('}');
}
