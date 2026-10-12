/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _DISP_VIDEOIN_IF_H_
#define _DISP_VIDEOIN_IF_H_

#include <linux/types.h>
#include <linux/printk.h>

#include "videoin_hal.h"
#include "vdout_sys_hal.h"

#define VIDEOIN_DRV_NAME  "disp_drv_videoin"

#define VIDEO_IN_USE_DEFAULT_CONFIG

#define VIDEOIN_IRQ_FIRST 0


enum COLOR_FORMAT {
	FORMAT_YUV420,
	FORMAT_YUV422,
	FORMAT_YUV444,
	FORMAT_RGB888,
	FORMAT_ARGB8888
};

void capture_frame(uint32_t id, uint16_t width, uint16_t height,
			enum VIDEO_BIT_MODE bit_mode,
			enum COLOR_FORMAT fmt, uint8_t *cap_buf_va);

void disp_capture_frame_end(void);
void W2D_test(uint32_t enable, uint32_t src_sel, uint32_t color_fmt,
				uint32_t bit_mode, bool mode, bool en_align,
				uint16_t w, uint16_t h, uint16_t x, uint16_t y);

void disp_videoin_dump_data(bool enable,
		enum VIDEOIN_SRC_SEL src, enum VIDEOIN_YCbCr_FORMAT fmt,
		enum VIDEO_BIT_MODE bit_mode, bool en_demode, bool en_align,
		dma_addr_t dump_addr_pa, uint16_t offset_x, uint16_t offset_y,
		uint16_t dump_width, uint16_t dump_height);
int disp_videoin_clk_enable(bool en);
int disp_videoin_wait_frame_done(void);
void disp_videoin_disable(void);

extern uint64_t *buf_va;

#endif
