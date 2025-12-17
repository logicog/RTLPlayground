// #define REGDBG 1

#include "../rtl837x_sfr.h"
#include "../rtl837x_common.h"
#include "../rtl837x_regs.h"
#include "../rtl837x_port.h"
#include "../rtl837x_flash.h"
#include "uip.h"
#include "../html_data.h"
#include <stdint.h>
#include "../phy.h"
#include "../version.h"
#include "../machine.h"
#include "page_impl.h"

#pragma codeseg BANK1
#pragma constseg BANK1

extern __code const struct machine machine;
extern __xdata uint8_t outbuf[TCP_OUTBUF_SIZE];
extern __xdata uint16_t slen;
extern __xdata uint16_t cont_len;
extern __xdata uint32_t cont_addr;
extern __code uint8_t * __code hex;
extern __xdata uip_ipaddr_t uip_hostaddr, uip_draddr, uip_netmask;
extern __code struct uip_eth_addr uip_ethaddr;

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

__code uint8_t * __code HTTP_RESPONCE_JSON = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n";
__code uint8_t * __code HTTP_RESPONCE_TXT = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";

// Convert uint8_t to ascii HEX char push on html-buffer.
void charhex_to_html(char c)
{
	outbuf[slen++] = itohex(c);
}


// Convert (uint8_t) bool to ascii '0' or '1' char push on html-buffer.
void bool_to_html(char c)
{
	outbuf[slen++] = c ? '1' : '0';
}


void char_to_html(char c)
{
	outbuf[slen++] = c;
}


//  Convert uint8_t to ascii HEX char.
void byte_to_html(uint8_t val)
{
	uint8_t cnt = 2;
	do {
		val = (val >> 4) | (val << 4);
		charhex_to_html(val);
		cnt -= 1;
	} while(cnt);
}

/* Converts a uint8_t to raw string.
   Suppress leading zeros.
*/
void itoa_html(uint8_t v)
{
	uint8_t t = (v / 100);
	// when print_zeros is not zero, we know that a non-zero number has printed.
	// That have to print all the next numbers.
	uint8_t print_zeros = t;
	if (print_zeros)
		char_to_html('0' + t);
	t = (v / 10) % 10;
	print_zeros |= t;
	if (print_zeros)
		char_to_html('0' + t);
	char_to_html('0' + (v % 10));
}


uint16_t stat_content(void)
{
	print_string("stat_content called\n");
	return 0;
}


uint16_t port_status(void)
{
	print_string("port_status called\n");
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


void reg_to_html(register uint16_t reg)
{
	reg_read_m(reg);
	sfr_data_to_html();
}


void send_sfp_info(uint8_t sfp)
{
	// This loops over the Vendor-name, Vendor OUI, Vendor PN and Vendor rev ASCII fields
	for (uint8_t i = 20; i < 60; i++) {
		if (i >= 36 && i < 40) // Skip Non-ASCII codes
			continue;
		uint8_t c = sfp_read_reg(sfp, i);
		if (c && c != 0xa0) // a0 is the byte read from a non-existant I2C EEPROM
			char_to_html(c);
	}
}


void sfp_send_data(uint8_t slot, uint8_t reg, uint8_t len)
{
	if (reg & 0x80) {	// Configure SFP readings address (0x51) as I2C device address
		reg &= 0x7f;
		REG_WRITE(RTL837X_REG_I2C_CTRL, 0x00, 0x1 << (I2C_MEM_ADDR_WIDTH-16) | len & 0xf,  0x51 >> 5, (0x51 << 3) & 0xff);
	} else {
		REG_WRITE(RTL837X_REG_I2C_CTRL, 0x00, 0x1 << (I2C_MEM_ADDR_WIDTH-16) | len & 0xf,  0x50 >> 5, (0x50 << 3) & 0xff);
	}

	reg_read_m(RTL837X_REG_I2C_CTRL);
	sfr_mask_data(1, 0xfc, machine.sfp_port[slot].i2c == 0 ? SCL_PIN << 5 | SDA_PIN_0 << 2 : SCL_PIN << 5 | SDA_PIN_1 << 2 );
	reg_write_m(RTL837X_REG_I2C_CTRL);

	REG_WRITE(RTL837X_REG_I2C_IN, 0, 0, 0, reg);

	// Execute I2C Read
	reg_bit_set(RTL837X_REG_I2C_CTRL, 0);

	// Wait for execution to finish
	do {
		reg_read_m(RTL837X_REG_I2C_CTRL);
	} while (sfr_data[3] & 0x1);

	for (uint8_t i = 0; i < len & 0xf; i++) {
		if (!(i & 0x3))
			reg_read_m(RTL837X_REG_I2C_OUT + (i >> 2));
		if (len & 0x80)
			char_to_html(sfr_data[3 - (i & 0x3)]);
		else
			byte_to_html(sfr_data[3 - (i & 0x3)]);
	}
}


void send_basic_info(void)
{
	slen = strtox(outbuf, HTTP_RESPONCE_JSON);
	print_string("send_basic_info called\n");
	slen += strtox(outbuf + slen, "{\"ip_address\":\"");
	itoa_html(uip_hostaddr[0]); char_to_html('.');
	itoa_html(uip_hostaddr[0] >> 8); char_to_html('.');
	itoa_html(uip_hostaddr[1]); char_to_html('.');
	itoa_html(uip_hostaddr[1] >> 8);
	slen += strtox(outbuf + slen, "\",\"ip_gateway\":\"");
	itoa_html(uip_draddr[0]); char_to_html('.');
	itoa_html(uip_draddr[0] >> 8); char_to_html('.');
	itoa_html(uip_draddr[1]); char_to_html('.');
	itoa_html(uip_draddr[1] >> 8);
	slen += strtox(outbuf + slen, "\",\"ip_netmask\":\"");
	itoa_html(uip_netmask[0]); char_to_html('.');
	itoa_html(uip_netmask[0] >> 8); char_to_html('.');
	itoa_html(uip_netmask[1]); char_to_html('.');
	itoa_html(uip_netmask[1] >> 8);
	slen += strtox(outbuf + slen, "\",\"mac_address\":\"");
	byte_to_html(uip_ethaddr.addr[0]); char_to_html(':');
	byte_to_html(uip_ethaddr.addr[1]); char_to_html(':');
	byte_to_html(uip_ethaddr.addr[2]); char_to_html(':');
	byte_to_html(uip_ethaddr.addr[3]); char_to_html(':');
	byte_to_html(uip_ethaddr.addr[4]); char_to_html(':');
	byte_to_html(uip_ethaddr.addr[5]);
	slen += strtox(outbuf + slen, "\",\"sw_ver\":\"");
	slen += strtox(outbuf + slen, VERSION_SW);
	slen += strtox(outbuf + slen, "\",\"hw_ver\":\"");
	slen += strtox(outbuf + slen, machine.machine_name);
	slen += strtox(outbuf + slen, "\",\"sfp_slot_0\":\"");
	send_sfp_info(0);
	char_to_html('"');
	if (machine.n_sfp == 2) {
		slen += strtox(outbuf + slen, ",\"sfp_slot_1\":\"");
		send_sfp_info(1);
		char_to_html('"');
	}
	char_to_html('}');
}


void send_vlan(uint16_t vlan)
{
	slen = strtox(outbuf, HTTP_RESPONCE_JSON);
	print_string("sending VLAN\n");
	//{"members":"0x00060011"}
	slen += strtox(outbuf + slen, "{\"members\":\"0x");
	vlan_get(vlan);
	sfr_data_to_html();
	slen += strtox(outbuf + slen, "\",\"name\":\"");
	__xdata uint16_t n = vlan_name(vlan);
	if (n== 0xffff) {
		print_string("VLAN has no name\n");
	} else {
		while(vlan_names[n] && vlan_names[n] != ' ')
			char_to_html(vlan_names[n++]);
	}
	slen += strtox(outbuf + slen, "\"}");
}


void send_counters(char port)
{
	print_string("send_counters called: "); print_byte(port); write_char('\n');
	slen = strtox(outbuf, HTTP_RESPONCE_JSON);
	print_string("sending counters\n");

	port--;
	uint8_t i = machine.phys_to_log_port[port];
	slen += strtox(outbuf + slen, "{\"portNum\":");
	itoa_html(i + 1);
	for (uint8_t j = 0; j < 0x3f; j++) {
		STAT_GET(j, i);
		slen += strtox(outbuf + slen, ",\"cnt_");
		itoa_html(j);
		slen += strtox(outbuf + slen, "\":\"0x");
		reg_to_html(RTL837X_STAT_V_HIGH);
		reg_to_html(RTL837X_STAT_V_LOW);
		char_to_html('\"');
	}
	char_to_html('}');
}


void send_mirror(void)
{
	print_string("send_mirror called\n");
	slen = strtox(outbuf, HTTP_RESPONCE_JSON);

	reg_read_m(RTL837x_MIRROR_CTRL);
	uint8_t mPort = sfr_data[3];
	if (mPort & 1) {
		slen += strtox(outbuf + slen, "{\"enabled\":1,\"mPort\":");
	} else {
		slen += strtox(outbuf + slen, "{\"enabled\":0,\"mPort\":");
	}
	itoa_html(machine.log_to_phys_port[mPort >> 1]);

	reg_read_m(RTL837x_MIRROR_CONF);
	uint16_t m = sfr_data[0];
	m = (m << 8) | sfr_data[1];
	slen += strtox(outbuf + slen, ",\"mirror_rx\":\"");
	for (uint8_t i = 0; i < 16; i++) {
		bool_to_html(!!(m & 0x8000));
		m <<= 1;
	}
	m = sfr_data[2];
	m = (m << 8) | sfr_data[3];
	slen += strtox(outbuf + slen, "\",\"mirror_tx\":\"");
	for (uint8_t i = 0; i < 16; i++) {
		bool_to_html(!!(m & 0x8000));
		m <<= 1;
	}
	char_to_html('\"');
	char_to_html('}');
}


void send_lag(void)
{
	print_string("send_lag called\n");
	slen = strtox(outbuf, HTTP_RESPONCE_JSON);

	char_to_html('[');
	for (uint8_t l=0; l < 4; l++) {
		slen += strtox(outbuf + slen, "{\"lagNum\":");
		itoa_html(l);
		slen += strtox(outbuf + slen, ",\"members\":\"");
		reg_read_m(RTL837X_TRK_MBR_CTRL_BASE + (l << 2));
		uint16_t ports = ((uint16_t)sfr_data[2] << 8) | sfr_data[3];
		for (uint8_t i = 0; i < 16; i++) {
			bool_to_html(!!(ports & 0x8000));
			ports <<= 1;
		}
		slen += strtox(outbuf + slen, "\",\"hash\":\"");
		reg_read_m(RTL837X_TRK_HASH_CTRL_BASE + (l << 2));
		sfr_data_to_html();
		slen += strtox(outbuf + slen, "\"},");
	}
	slen -=1; // remove comma
	char_to_html(']');
}


void send_eee(void)
{
	print_string("send_eee called\nsending EEE status\n");
	slen = strtox(outbuf, HTTP_RESPONCE_JSON);

	reg_read_m(RTL8373_PHY_EEE_ABLTY);
	uint8_t eee_ablty = sfr_data[3];

	char_to_html('[');
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		slen += strtox(outbuf + slen, "{\"portNum\":");
		itoa_html(machine.log_to_phys_port[i]);

		if (machine.is_sfp[i]) {
			slen += strtox(outbuf + slen, ",\"isSFP\":1");
		} else {
			slen += strtox(outbuf + slen, ",\"isSFP\":0,\"eee\":\"");
			uint16_t v;
			phy_read(i, PHY_MMD_AN, PHY_EEE_ADV2);
			v = SFR_DATA_U16;
			bool_to_html(v & PHY_EEE_BIT_2G5);

			phy_read(i, PHY_MMD_AN, PHY_EEE_ADV);
			v = SFR_DATA_U16;
			bool_to_html(v & PHY_EEE_BIT_1G);
			bool_to_html(v & PHY_EEE_BIT_100M);

			phy_read(i, PHY_MMD_AN, PHY_EEE_LP_ABILITY2);
			v = SFR_DATA_U16;
			slen += strtox(outbuf + slen, "\",\"eee_lp\":\"");
			bool_to_html (v & PHY_EEE_BIT_2G5);

			phy_read(i, PHY_MMD_AN, PHY_EEE_LP_ABILITY);
			v = SFR_DATA_U16;
			bool_to_html(v & PHY_EEE_BIT_1G);
			bool_to_html(v & PHY_EEE_BIT_100M);

			slen += strtox(outbuf + slen, "\",\"active\":");
			bool_to_html(eee_ablty & (1 << i));
		}
		char_to_html('}');
		if (i < machine.max_port)
			char_to_html(',');
		else
			char_to_html(']');
	}
}


void send_status(void)
{
	slen = strtox(outbuf, HTTP_RESPONCE_JSON);
	print_string("sending status\n");
	char_to_html('[');

	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		slen += strtox(outbuf + slen, "{\"portNum\":");
		itoa_html(machine.log_to_phys_port[i]);

		if (machine.is_sfp[i]) {
			slen += strtox(outbuf + slen, ",\"isSFP\":1,\"enabled\":");
			if (!(sfp_pins_last & (0x1 << ((machine.is_sfp[i] - 1) << 2)))) {
				bool_to_html(1);
				slen += strtox(outbuf + slen,",\"sfp_options\":\"0x");
				byte_to_html(sfp_options[machine.is_sfp[i]-1]);
				if (sfp_options[machine.is_sfp[i]-1] & 0x40) {
					sfp_send_data(machine.is_sfp[i] - 1, 92, 1);
					slen += strtox(outbuf + slen,"\",\"sfp_temp\":\"0x");
					sfp_send_data(machine.is_sfp[i] - 1, 224, 2);
					slen += strtox(outbuf + slen,"\",\"sfp_vcc\":\"0x");
					sfp_send_data(machine.is_sfp[i] - 1, 226, 2);
					slen += strtox(outbuf + slen,"\",\"sfp_txbias\":\"0x");
					sfp_send_data(machine.is_sfp[i] - 1, 228, 2);
					slen += strtox(outbuf + slen,"\",\"sfp_txpower\":\"0x");
					sfp_send_data(machine.is_sfp[i] - 1, 230, 2);
					slen += strtox(outbuf + slen,"\",\"sfp_rxpower\":\"0x");
					sfp_send_data(machine.is_sfp[i] - 1, 232, 2);
					slen += strtox(outbuf + slen,"\",\"sfp_state\":\"0x");
					sfp_send_data(machine.is_sfp[i] - 1, 238, 1);
				}
				slen += strtox(outbuf + slen,"\",\"sfp_vendor\":\"");
				for (register uint8_t s = 0; s < 16; s++)
					outbuf[slen++] = sfp_module_vendor[machine.is_sfp[i]-1][s];
				slen += strtox(outbuf + slen,"\",\"sfp_model\":\"");
				for (register uint8_t s = 0; s < 16; s++)
					outbuf[slen++] = sfp_module_model[machine.is_sfp[i]-1][s];
				slen += strtox(outbuf + slen,"\",\"sfp_serial\":\"");
				for (register uint8_t s = 0; s < 16; s++)
					outbuf[slen++] = sfp_module_serial[machine.is_sfp[i]-1][s];
				char_to_html('"');
			} else {
				bool_to_html(0);
			}
		} else {
			slen += strtox(outbuf + slen, ",\"isSFP\":0,\"enabled\":");
			phy_read(i, 0x1f, 0xa610);
			bool_to_html(SFR_DATA_8 == 0x20);
		}

		slen += strtox(outbuf + slen, ",\"link\":");

		if (i < 8)
			reg_read_m(RTL837X_REG_LINKS);
		else
			reg_read_m(RTL837X_REG_LINKS_89);
		uint8_t b = sfr_data[3 - ((i & 7) >> 1)];
		b = (i & 1) ? b >> 4 : b & 0xf;
		char_to_html('0' + b);

		STAT_GET(STAT_COUNTER_TX_PKTS, i);
		slen += strtox(outbuf + slen, ",\"txG\":\"0x");
		reg_to_html(RTL837X_STAT_V_HIGH);
		reg_to_html(RTL837X_STAT_V_LOW);

		slen += strtox(outbuf + slen, "\",\"txB\":\"0x");
		STAT_GET(STAT_COUNTER_ERR_PKTS, i);
		reg_to_html(RTL837X_STAT_V_LOW);	// 32 bit Tx Packet errors

		slen += strtox(outbuf + slen, "\",\"rxG\":\"0x");
		STAT_GET(STAT_COUNTER_RX_PKTS, i);
		reg_to_html(RTL837X_STAT_V_HIGH);
		reg_to_html(RTL837X_STAT_V_LOW);

		slen += strtox(outbuf + slen, "\",\"rxB\":\"0x");
		STAT_GET(STAT_COUNTER_ERR_PKTS, i);
		reg_to_html(RTL837X_STAT_V_HIGH);	// 32bit RX packet errors
		slen += strtox(outbuf + slen, "\"}");
		if (i < machine.max_port)
			char_to_html(',');
		else
			char_to_html(']');
	}
}


void send_config(void)
{
	print_string("send_config called\n");
	__xdata uint32_t pos = CONFIG_START; // 70000 , 6c000 / 0xc000 = 9

	extern __xdata uint16_t len_left = CONFIG_LEN;
	slen = strtox(outbuf, HTTP_RESPONCE_TXT);
	while (read_flash((CONFIG_START-CODE0_SIZE) / CODE_BANK_SIZE + 1,
		(__code uint8_t *) (((CONFIG_START + len_left - CODE0_SIZE) % CODE_BANK_SIZE) + CODE0_SIZE + len_left)) == 0xff) {
		print_short(len_left);
		len_left--;
	}
	len_left++;

	if (len_left > (TCP_OUTBUF_SIZE - slen)) {
		cont_len = len_left - (TCP_OUTBUF_SIZE - slen);
		len_left = TCP_OUTBUF_SIZE - slen;
		cont_addr = len_left;
	}
	flash_region.addr = CONFIG_START;
	flash_region.len = len_left;
	flash_read_bulk(outbuf + slen);
	slen += len_left;
}

void send_cmd_log(void)
{
	print_string("send_cmd_log called\n");
	slen = strtox(outbuf, HTTP_RESPONCE_TXT);
	__xdata uint16_t p = (cmd_history_ptr + 1) & CMD_HISTORY_MASK;
	__xdata uint8_t found_begin = 0;
	print_string("History ptr: ");
	print_short(cmd_history_ptr); write_char('\n');
	while (p != cmd_history_ptr) {
		if (!cmd_history[p] || cmd_history[p] == '\n')
			found_begin = 1;
		if (found_begin && cmd_history[p])
			outbuf[slen++] = cmd_history[p];
		p = (p + 1) & CMD_HISTORY_MASK;
	}
}
