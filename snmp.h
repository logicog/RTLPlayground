#ifndef SNMP_H
#define SNMP_H
#include <stdint.h>
#include <stdbool.h>
#define SNMP_MAX_MESSAGE 484
#define SNMP_MAX_VARBINDS 8
#define SNMP_MAX_ARCS 32
/* BER identifiers, SNMPv2c operations and protocol error/exception values. */
#define ASN_INTEGER 0x02
#define ASN_OCTET_STRING 0x04
#define ASN_NULL 0x05
#define ASN_OBJECT_ID 0x06
#define ASN_SEQUENCE 0x30
#define ASN_IP_ADDRESS 0x40
#define ASN_COUNTER32 0x41
#define ASN_GAUGE32 0x42
#define ASN_TIMETICKS 0x43
#define ASN_OPAQUE 0x44
#define ASN_COUNTER64 0x46
#define ASN_UNSIGNED32 0x47
#define SNMP_VERSION_2C 1
#define SNMP_PDU_GET 0xa0
#define SNMP_PDU_GETNEXT 0xa1
#define SNMP_PDU_GETBULK 0xa5
#define SNMP_PDU_RESPONSE 0xa2
#define SNMP_ERR_TOO_BIG 1
#define SNMP_ERR_GEN_ERR 5
#define SNMP_NO_SUCH_OBJECT 0x80
#define SNMP_NO_SUCH_INSTANCE 0x81
#define SNMP_END_OF_MIB 0x82
#define SNMP_LOOKUP_HW_ERROR 255
/* CLI setters and startup replay are the only configuration path. Default deny. */
void snmp_init(void) __banked;
bool snmp_set_community(__xdata const uint8_t *__xdata value, __xdata uint8_t length) __banked;
bool snmp_set_manager(__xdata const uint8_t *__xdata cidr, __xdata uint8_t length) __banked;
bool snmp_command(void) __banked;
void snmp_show(void) __banked;
/* Authenticated configuration export deliberately contains plaintext community.
 * This writes HTTP output directly, never console/syslog or information.json. */
void snmp_export_config(void) __banked;
/* Volatile enable/disable only; startup snmp on/off restores the saved state. */
bool snmp_set_enabled(__xdata uint8_t enabled) __banked;
/* Non-secret snapshot: enabled, configured, network[4], mask[4].
 * No credential bytes or length; no state changes. Main-loop owner only. */
#define SNMP_STATUS_SIZE 10
void snmp_get_status(__xdata uint8_t *__xdata status) __banked;
void snmp_callback(void) __banked;
uint16_t snmp_process(__xdata const uint8_t *__xdata input, __xdata uint16_t length,
		      __xdata const uint8_t *__xdata source) __banked;
extern __xdata uint8_t snmp_response[SNMP_MAX_MESSAGE];
#endif
