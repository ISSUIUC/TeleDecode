/*
 * Copyright © 2012 Keith Packard <keithp@keithp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#ifndef _AO_PACKET_H_
#define _AO_PACKET_H_

#include <stdint.h>

#define AO_MAX_CALLSIGN		8
#define AO_PACKET_MAX		64
#define AO_PACKET_SYN		(uint8_t) 0xff

struct ao_packet {
	uint8_t		addr;
	uint8_t		len;
	uint8_t		seq;
	uint8_t		ack;
	uint8_t		d[AO_PACKET_MAX];
	uint8_t		callsign[AO_MAX_CALLSIGN];
};

struct ao_packet_recv {
	struct ao_packet	packet;
	int8_t			rssi;
	uint8_t			status;
};

#endif /* _AO_PACKET_H_ */
