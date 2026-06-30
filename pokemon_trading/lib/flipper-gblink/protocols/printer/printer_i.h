// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#ifndef PRINTER_I_H
#define PRINTER_I_H

#include <protocols/printer/include/printer_proto.h>

#define START_L_BYTE		0x88
#define START_H_BYTE		0x33
#define ALIVE_BYTE		0x81

#define CMD_INIT		0x01
#define CMD_PRINT		0x02
#define CMD_TRANSFER		0x10
#define CMD_DATA		0x04
#define CMD_STATUS		0x0f

/* These constants are private to the printer protocol */
#define LINE_BUF_SZ		640 // (TILE_SIZE * WIDTH * 2)
#define TRANSFER_SZ		3584 // (16*16*14) // Image minus frame

/* NOTE:
 * These numbers are empirically gathered from a few different games thus far.
 * There are many notes floating around the internet of the GB Printer having
 * a 100 ms limit between packets where it will reset. However, I've seen
 * Pokemon Pinball wait 99.5 ms between packets after a print command which is
 * a bit too close for comfort. As this code tracks timestamps _after_ each byte,
 * that ends up just over 110 ms which trips the hard timeout and resets state.
 * This often cofuses the hell out of games.
 *
 * Additionally, on the other end of the spectrum, Pokemon Gold absolutely uses
 * the hard timeout to reset the printer between packets. It waits ~278 ms, when
 * it could just send an init command.
 *
 * Even more silly, Pokemon Pinball has a fun quirk where if the print completes
 * immediately (usually the Flipper will mark a print complete with a single
 * packet turnaround), it asks for status a couple of times, then starts (presumably)
 * another status packet, but the second byte in the transfer is stopped mid-byte.
 * Between that point and the start of the next, real packet, is 30 ms, 32.6 ms
 * if you go from end of last byte received to start of next, real packet.
 *
 * This means there is some "soft" timeout that the printer uses to reset a packet
 * transfer in progress, but don't reset the whole printer state.
 *
 * There are wisps of some "unknown" bit timeout of 1.49 ms. But I've not yet
 * seen that in action.
 *
 * As far as I know, no one has dumped and reverse engineered the ROM of the
 * Game Boy Printer directly. I think all of the existing documentation was from
 * reverse engineering the communication channel. Maybe someday I'll dump the
 * GB Printer ROM and try to better understand all of it.
 *
 * Additionally, the gameboy camera seems to hint at a 2 second busy timeout, that
 * is that the printer needs to return a printing status within 2 seconds of starting
 * the print. There is also a hint of a 20 second print timeout, that is that the
 * printer needs to return print complete within 20 seconds of starting the print.
 * These two values are not really used in this library yet.
 */
#define HARD_TIMEOUT_US 125000
#define SOFT_TIMEOUT_US 20000

enum packet_state {
	START_L,
	START_H,
	COMMAND,
	COMPRESS,
	LEN_L,
	LEN_H,
	DATA,
	CKSUM_L,
	CKSUM_H,
	ALIVE,
	STATUS,
};

struct packet {
	uint8_t cmd;
	bool compress;
	uint16_t len; // This is stored in the flipper endianness, arrives LSB first from GB, unmodified in code
	uint8_t line_buf[LINE_BUF_SZ]; // 640 bytes, enough for two lines of tiles
	uint16_t cksum; // This is stored in the flipper endianness, arrives LSB first from GB
	uint8_t status;
	
	/* These are not part of the packet, but used by us */
	uint16_t cksum_calc;
	size_t line_buf_sz;
	bool print_complete;
	uint8_t zero_counter;
	enum packet_state state;
	uint32_t time;
};

#define THREAD_FLAGS_EXIT	(1 << 0)
#define THREAD_FLAGS_DATA	(1 << 1)
#define THREAD_FLAGS_PRINT	(1 << 2)
#define THREAD_FLAGS_COMPLETE	(1 << 3)
#define THREAD_FLAGS_ALL	(THREAD_FLAGS_EXIT | THREAD_FLAGS_DATA | THREAD_FLAGS_PRINT | THREAD_FLAGS_COMPLETE)

struct printer_proto {
	void *gblink_handle;

	void (*callback)(void *cb_context, struct gb_image *image, enum cb_reason reason);
	void *cb_context;

	struct packet *packet; //packet data used by send()/receive() for tracking

	struct gb_image *image; // Details of the current image being sent/received

	FuriThread *thread;
};

#endif // PRINTER_I_H
