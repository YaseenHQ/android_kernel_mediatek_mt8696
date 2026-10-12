/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include "disp_bitstream.h"
#include "fg_info.h"

/* initialize the bit reader with the given data and size */
void init_bit_operations(struct bit_render *br, uint8_t *data, int size)
{
	br->buffer = data;
	br->bit_off = 0;
	br->bit_size = size * 8;
}

/* Function to read 'n' bits from the bit_render structure */
uint32_t read_bits(struct bit_render *br, int n)
{
	int current_bit, byte_off, bit_in_byte;
	uint32_t bit = 0, result = 0, i = 0;;

	if (!br || !br->buffer) {
		FG_ERR("Invalid bit_render structure or buffer is NULL\n");
		return 0;
	}

	if (n <= 0 || n > 32) {
		FG_ERR("Requested bit count out of valid range (1-32)\n");
		return 0;
	}

	if (br->bit_off + n > br->bit_size) {
		FG_ERR("Not enough bits left\n");
		return 0;
	}

	current_bit = br->bit_off;
	for (i = 0; i < n; i++) {
		byte_off = (current_bit / 8);
		bit_in_byte = current_bit % 8;
		bit = (br->buffer[byte_off] >> (7 - bit_in_byte)) & 1;
		result = (result << 1) | bit;
		current_bit++;
	}

	/* Move the bit offset forward by n bits */
	br->bit_off += n;
	return result;
}

/* Function to read a single bit */
uint8_t read_bit(struct bit_render *br)
{
	return read_bits(br, 1);
}

/* Function to read a single byte */
uint8_t read_byte(struct bit_render *br)
{
	return read_bits(br, 8);
}

/* Function to read 'n' bytes from the bit_render structure byte */
uint32_t read_bytes(struct bit_render *br, int n)
{
	if (n <= 0 || n > 4) {
		FG_ERR("Requested byte count out of valid range (1-4)\n");
		return 0;
	}
	return read_bits(br, n * 8);
}

/* Function to get the number of read bits in the bit_render structure */
uint32_t read_bits_count(struct bit_render *br)
{
	return br->bit_off;
}

/* Function to skip 'n' bits in the bit_render structure */
void skip_bits(struct bit_render *br, int n)
{
	if (!br) {
		FG_ERR("Invalid bit_render pointer\n");
		return;
	}

	if (br->bit_off + n > br->bit_size) {
		FG_ERR("Cannot skip beyond the available amount of bits\n");
		return;
	}

	br->bit_off += n;
}

/* Clip a signed integer value into the 0-255 range */
uint8_t av_clip_u8(int a)
{
	if (a & (~0xFF))
		return 0;
	else
		return a;
}
