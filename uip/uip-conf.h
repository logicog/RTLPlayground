/**
 * \addtogroup uipopt
 * @{
 */

/**
 * \name Project-specific configuration options
 * @{
 *
 * uIP has a number of configuration options that can be overridden
 * for each project. These are kept in a project-specific uip-conf.h
 * file and all configuration names have the prefix UIP_CONF.
 */

/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack
 *
 * $Id: uip-conf.h,v 1.6 2006/06/12 08:00:31 adam Exp $
 */

/**
 * \file
 *         An example uIP configuration file
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#ifndef __UIP_CONF_H__
#define __UIP_CONF_H__

#include <stdint.h>

#define UIP_CONF_EXTERNAL_BUFFER
#define UIP_ARCH_CHKSUM 1
#define UIP_ARCH_IPCHKSUM 1

/**
 * 8 bit datatype
 *
 * This typedef defines the 8-bit type used throughout uIP.
 *
 * \hideinitializer
 */
typedef uint8_t u8_t;

/**
 * 16 bit datatype
 *
 * This typedef defines the 16-bit type used throughout uIP.
 *
 * \hideinitializer
 */
typedef uint16_t u16_t;

/**
 * Statistics datatype
 *
 * This typedef defines the dataype used for keeping statistics in
 * uIP.
 *
 * \hideinitializer
 */
typedef unsigned short uip_stats_t;

/**
 * Maximum number of TCP connections. TODO: Increase this, but also make the socket state/buffer per-connection.
 *
 * \hideinitializer
 */
#define UIP_CONF_MAX_CONNECTIONS 1

/**
 * uip_periodic() runs from idle() roughly once per system tick
 * (SYS_TICK_HZ, 200 Hz; interrupt wake-ups add sweeps, so the timeout
 * can only fire early, never late). A silent ESTABLISHED peer is
 * dropped after about 30 s: with a single connection slot it would
 * otherwise wedge the web UI until a power cycle.
 */
#define UIP_CONF_IDLE_PERIODS 200
#define UIP_CONF_IDLE_TIMEOUT 30

/**
 * Maximum number of listening TCP ports. TODO: increase this!
 *
 * \hideinitializer
 */
#define UIP_CONF_MAX_LISTENPORTS 1

/**
 * uIP buffer size.
 *
 * Sized to the largest frame the CPU port accepts on ingress: anything above
 * that the NIC drops in hardware, so a larger buffer only costs XDATA. Measured
 * with ICMP, which bypasses MSS and so probes the hardware directly: a 1502-byte
 * payload is answered and 1503 is not, which puts the frame at 1556 bytes of
 * uip_buf. The limit is the NIC's rather than a port's, so how the frame was
 * tagged on the wire does not change it.
 *
 * \hideinitializer
 */
#define UIP_CONF_BUFFER_SIZE     1556

/**
 * Bytes of the buffer kept out of the advertised MSS.
 *
 * \hideinitializer
 */
#define UIP_CONF_BUFFER_EXTRA    30

/**
 * CPU byte order.
 *
 * \hideinitializer
 */
#define UIP_CONF_BYTE_ORDER      LITTLE_ENDIAN

/**
 * Logging on or off
 *
 * \hideinitializer
 */
#define UIP_CONF_LOGGING         1

/**
 * UDP support on or off
 *
 * \hideinitializer
 */
#define UIP_CONF_UDP             1

/**
 * UDP checksums on or off
 * The RTL8372/3 are able to calculate and verify all L2 and L3 checksums
 * in hardware. The TX checksums are configured in the CPU-tag of outgoing
 * packets, while the RTL837X_NIC_RX_CTRL register configures the verification
 * of the checksum of inbound packets and subsequent possible drop.
 *
 * \hideinitializer
 */
#define UIP_CONF_UDP_CHECKSUMS   0

/**
 * uIP statistics on or off
 *
 * \hideinitializer
 */
#define UIP_CONF_STATISTICS      1

/* Here we include the header file for the application(s) we use in
   our project. */
/*#include "smtp.h"*/
#include "httpd.h"
#include "udp_apps.h"
/*#include "telnetd.h"*/
/*#include "webserver.h" */
/*#include "dhcpc.h"*/
/*#include "resolv.h"*/
/*#include "webclient.h"*/

#endif /* __UIP_CONF_H__ */

/** @} */
/** @} */
