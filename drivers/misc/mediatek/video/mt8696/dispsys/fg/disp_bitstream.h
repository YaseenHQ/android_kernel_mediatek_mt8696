/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __DISP_BIT_STREAM_H__
#define __DISP_BIT_STREAM_H__

/* define the struct of bit render */
struct bit_render {
	/* buffer address */
	unsigned char *buffer;
	/* current position */
	int bit_off;
	/* buffer size calculated from byte_size which can be divided by 8 */
	int bit_size;
};

void init_bit_operations(struct bit_render *br, uint8_t *data, int size);
uint32_t read_bits(struct bit_render *br, int n);
uint8_t read_bit(struct bit_render *br);
uint8_t read_byte(struct bit_render *br);
uint32_t read_bytes(struct bit_render *br, int n);
uint32_t read_bits_count(struct bit_render *br);
void skip_bits(struct bit_render *br, int n);
uint8_t av_clip_u8(int a);

#endif /* __DISP_BIT_STREAM_H__ */
