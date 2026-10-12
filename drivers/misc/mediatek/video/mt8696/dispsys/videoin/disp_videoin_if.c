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


#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include "disp_info.h"
#include "disp_hw_mgr.h"
#include "disp_videoin_if.h"
#include "disp_videoin_debug.h"
#include "videoin_hal.h"
#include "vdout_sys_hal.h"
#include "disp_irq.h"

#define W2D_RET_OK	0
#define W2D_RET_ERROR	-1
#define MAX_FILE_NAME	100
#define W2D_TEST_FRAME_MAX_SIZE	(4096*2160*4)

#define CPT_CAP_NODE_DISABLE	1

struct device *Videoin_dev;
struct device *disp_capture_dev;

enum VIDEO_BIT_MODE bit_mode;
bool is_16_packet;
enum VIDEOIN_YCbCr_FORMAT color_format;

uint32_t dump_len;
struct disp_hw_resolution disp_info;

wait_queue_head_t wait_irq_wq;
atomic_t wait_irq_flag;
uint32_t timeoutmsec = 150;
uint32_t disp_vin_befifo_irq_fd = 0;

uint64_t *buf_va;
dma_addr_t buf_pa;
size_t buf_size;

static int _videoin_default_setting(const struct disp_hw_resolution *info)
{
	int h_start = 0;
	int v_odd_start = 0;
	int v_even_start = 0;

	bit_mode = VIDEOIN_BITMODE_8;
	is_16_packet = false;
	color_format = VIDEOIN_FORMAT_420;

	if (info->height == 480) {
		h_start = 0x7C;

		if (info->is_progressive == true) {
			v_odd_start = 0x2A;
			v_even_start = 0x2A;
		} else {
			v_odd_start = 0x16;
			v_even_start = 0x16;
		}
	} else if (info->height == 576) {
		h_start = 0xEC;
		v_odd_start = 0x2C;
		v_even_start = 0x2C;
	} else if (info->height == 720) {
		h_start = 0xF3;
		v_odd_start = 0x19;
		v_even_start = 0x19;
	} else if (info->height == 1080) {
		h_start = 0xFE;
		if (info->is_progressive == true) {
			v_odd_start = 0x29;
			v_even_start = 0x29;
		} else {
			v_odd_start = 0x15;
			v_even_start = 0x15;
		}
	} else if (info->height == 2160) {
		h_start = 0xD4;
		v_odd_start = 0x2A;
		v_even_start = 0x2A;
	}
	if (info->height == 480)
		vdout_sys_hal_set_new_sd_sel(true);
	else
		vdout_sys_hal_set_new_sd_sel(false);
	vdout_sys_hal_videoin_source_sel(VIDEOIN_SRC_SEL_VDO_BE_FIFO_OUTPUT);
	videoin_hal_set_h(info->width, info->htotal, false,
		is_16_packet, bit_mode);
	videoin_hal_set_v(info->height, color_format);
	videoin_hal_set_color_format(color_format);
	videoin_hal_set_channel_select(SEL_Y_CHN, SEL_CB_CHN, SEL_CR_CHN);
	videoin_hal_set_bitmode(bit_mode, is_16_packet);
	videoin_hal_demode_enable(false);
	videoin_hal_set_active_zone(h_start, v_odd_start, v_even_start);

	return 0;
}

static int disp_videoin_init(struct disp_hw_common_info *info)
{
	uintptr_t reg_base = 0;
	uintptr_t larb4_regbase = 0;
	struct device_node *np;
	unsigned int reg_value;
	unsigned int irq_value = 0;
	unsigned int irq_no = 0;

	struct disp_hw *videoin_drv = disp_videoin_get_drv();

	np = of_find_compatible_node(NULL, NULL, "mediatek,mt8696-videoin");
	if (np == NULL) {
		pr_err("dts error, no videoin device node.\n");
		return 0;
	}

	of_property_read_u32_index(np, "reg", 1, &reg_value);
	of_property_read_u32_index(np, "interrupts", 1, &irq_value);

	reg_base = (uintptr_t)of_iomap(np, 0);
	larb4_regbase = (uintptr_t)of_iomap(np, 1);
	irq_no = irq_of_parse_and_map(np, 0);

	if (irq_no == 0)
		pr_err("videoin get irq from dts fail\n");
	else {
		VIDEOIN_LOG_D("videoin get irq no:%d\n", irq_no);
		videoin_drv->irq[0].value = irq_no;
		videoin_drv->irq[0].irq = DISP_IRQ_VIDEO_IN;
		videoin_drv->irq_num = 1;
	}

	init_waitqueue_head(&wait_irq_wq);
	atomic_set(&wait_irq_flag, 0);

	videoin_hal_init(reg_base, larb4_regbase);
	_videoin_default_setting(info->resolution);
	disp_info = *(info->resolution);

	Videoin_dev = disp_hw_mgr_get_dev();
	videoin_debug_init();
	return 0;
}

static int disp_videoin_deinit(void)
{
	videoin_hal_clock(false);
	return 0;
}

static int disp_videoin_suspend(void)
{
	/* videoin_hal_clock(false); */
	return 0;
}

static int disp_videoin_resume(void)
{
	/* videoin_hal_clock(true); */
	return 0;
}

int disp_videoin_clk_enable(bool en)
{
	videoin_hal_clock(en);
	return 0;
}

static int disp_videoin_change_resolution(const struct disp_hw_resolution *info)
{
	if (disp_info.res_mode != info->res_mode)
		disp_info = *info;
	pr_info("disp videoin change res =%d\n", disp_info.res_mode);
	return 0;
	/*
	_videoin_default_setting(info);
	return 0;
	*/
}

static int disp_videoin_irq_handle(uint32_t irq)
{
	static unsigned int irq_num = VIDEOIN_IRQ_FIRST;

	switch (irq) {
	case DISP_IRQ_VIDEO_IN:
		if (irq_num == VIDEOIN_IRQ_FIRST) {
			videoin_shadow(true);
			videoin_hal_enable(false);
			irq_num++;
		} else {
			atomic_set(&wait_irq_flag, 1);
			wake_up(&wait_irq_wq);
			irq_num = VIDEOIN_IRQ_FIRST;
		}
		break;
	case DISP_IRQ_BEFIFO:
		if (disp_vin_befifo_irq_fd == 1) {
			if (irq_num == VIDEOIN_IRQ_FIRST) {
				videoin_shadow(true);
				videoin_hal_enable(false);
				irq_num++;
			} else {
				atomic_set(&wait_irq_flag, 1);
				wake_up(&wait_irq_wq);
				irq_num = VIDEOIN_IRQ_FIRST;
				disp_vin_befifo_irq_fd = 0;
			}

			//VIDEOIN_LOG_I("videoin irq_num = %d\n", irq_num);
		}
		break;
	default:
		break;
	}
	return 0;
}

int disp_videoin_wait_frame_done(void)
{
	int ret = 0;

	VIDEOIN_LOG_I("vin start to wait frame done\n");
	atomic_set(&wait_irq_flag, 0);
	disp_vin_befifo_irq_fd = 1;
	ret = wait_event_timeout(wait_irq_wq,
				 atomic_read(&wait_irq_flag),
				 msecs_to_jiffies(timeoutmsec));
	if (!ret)
		VIDEOIN_LOG_I("vin wait timed out\n");
	else
		VIDEOIN_LOG_I("vin wait done\n");

	return ret;
}

static int videoin_alloc_transit_buf(enum VIDEO_BIT_MODE bit_mode,
	enum VIDEOIN_YCbCr_FORMAT color_fmt, uint32_t width, uint32_t height)
{
	uint32_t vin_buf_y_size = 0;
	uint32_t vin_buf_cbcr_size = 0;

	if (width == 0 || height == 0) {
		VIDEOIN_LOG_E("vin transt buf size error\n");
		return W2D_RET_ERROR;
	}

	vin_buf_y_size = width * height;
	vin_buf_y_size *= ((bit_mode == VIDEOIN_BITMODE_8) ? 1 : 2);

	if (color_fmt == VIDEOIN_FORMAT_444)
		vin_buf_cbcr_size = vin_buf_y_size * 2;
	else if (color_fmt == VIDEOIN_FORMAT_422)
		vin_buf_cbcr_size = vin_buf_y_size;
	else if (color_fmt == VIDEOIN_FORMAT_420)
		vin_buf_cbcr_size = vin_buf_y_size / 2;
	else {
		VIDEOIN_LOG_E("colorfmt not support by vin\n");
		return W2D_RET_ERROR;
	}


	buf_size = vin_buf_y_size + vin_buf_cbcr_size;

	buf_va = dma_alloc_coherent(Videoin_dev, buf_size,
			&buf_pa, GFP_KERNEL);

	VIDEOIN_LOG_I("videoin alloc transit buf(%d %d %d) 0x%p 0x%x\n",
		width, height, buf_size, buf_va, buf_pa);

	return W2D_RET_OK;
}

static int videoin_free_transit_buf(void)
{
	if (buf_va != NULL) {
		dma_free_coherent(Videoin_dev, buf_size,
		buf_va, buf_pa);
		buf_va = NULL;
		buf_pa = 0;
		buf_size = 0;
		VIDEOIN_LOG_I("videoin transit buf free done\n");
	}

	return W2D_RET_OK;
}

void disp_capture_frame_end(void)
{
	videoin_free_transit_buf();
	VIDEOIN_LOG_I("videoin release buffer\n");
}

void capture_frame(uint32_t id, uint16_t width, uint16_t height,
		enum VIDEO_BIT_MODE bit_mode,
		enum COLOR_FORMAT fmt, uint8_t *cap_buf_va)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	char fileName[MAX_FILE_NAME];
	struct file *capture_fp = NULL;
	loff_t pos = 0;
	int ret = 0;
	unsigned int i = 0, length = 0, dump_length = 0;
	uint8_t *ptr_f = NULL;
	uint8_t *ptr_m = NULL;
	uint8_t *ptr = NULL;

	bool is_packet = false;
	dma_addr_t addr_mva;
	uint64_t *addr_va = NULL;

	memset(fileName, 0, MAX_FILE_NAME);

	ret = snprintf(fileName, MAX_FILE_NAME,
		"/sdcard/capture_frame_%d.yuv", id);

	if (ret <= 0)
		return;

	capture_fp = filp_open(fileName,
		O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE, 0644);
	if (IS_ERR(capture_fp))
		VIDEOIN_LOG_E("open file %s fail\n", fileName);

	VIDEOIN_LOG_I("capture %d, dump %s\n", id, fileName);

	/* write date */
	VIDEOIN_LOG_I("write frame to file!\n");

	if (bit_mode == VIDEOIN_BITMODE_8) {
		is_packet = false;
	} else if (bit_mode == VIDEOIN_BITMODE_10) {
		is_packet = true;
	} else if (bit_mode == VIDEOIN_BITMODE_12) {
		is_packet = true;
	} else {
		VIDEOIN_LOG_E("bpp error %d\n", bit_mode);
		return;
	}

	dump_length = (is_packet ? (width * height * 2) : (width * height));
	VIDEOIN_LOG_I("dump_length = %d, fmt = %d bit_mode = %d\n",
		dump_length, fmt, bit_mode);

	ret = kernel_write(capture_fp,
		(const char *)(const uint8_t *)cap_buf_va, dump_length,
				&pos);
	if (ret < 0)
		VIDEOIN_LOG_E("write error y ret %d\n", ret);

	if (fmt == FORMAT_YUV422 ||
		fmt == FORMAT_YUV420) {

		length = dump_length;
		if (fmt == FORMAT_YUV420)
			length = dump_length / 2;

		addr_va = dma_alloc_coherent(Videoin_dev, length,
			&addr_mva, GFP_KERNEL);

		ptr_f = (uint8_t *)addr_va;
		ptr_m = ((uint8_t *)addr_va) + (length / 2);
		ptr = (uint8_t *)cap_buf_va + dump_length;

		VIDEOIN_LOG_I("all mem va(0x%p, 0x%p), length = %d\n",
				ptr_f, ptr_m, length);

		if (!is_packet) {
			for (i = 0; i < (length / 2); i++) {
				ptr_f[i] = ptr[2 * i];
				ptr_m[i] = ptr[2 * i + 1];
			}

		} else {
			for (i = 0; i < (length / 2); i += 2) {
				ptr_f[i] = ptr[2 * i + 0];
				ptr_f[i + 1] = ptr[2 * i + 1];
				ptr_m[i] = ptr[2 * i + 2];
				ptr_m[i + 1] = ptr[2 * i + 3];
			}
		}
		ret = kernel_write(capture_fp,
			(const char *)addr_va,
			length, &pos);
		if (ret < 0)
			VIDEOIN_LOG_E("write error v %d\n", ret);

		if (addr_va != NULL)
			dma_free_coherent(Videoin_dev, length,
				addr_va, addr_mva);

	} else {
		ret = kernel_write(capture_fp,
			(const char *)(cap_buf_va + dump_length), dump_length * 2,
			 &pos);
		if (ret < 0)
			VIDEOIN_LOG_E("write error u ret %d\n", ret);

		if (fmt == FORMAT_ARGB8888) {
			ret = kernel_write(capture_fp,
				(const char *)(cap_buf_va + dump_length * 3),
				dump_length, &pos);
			if (ret < 0)
				VIDEOIN_LOG_E("write error b %d\n", ret);
		}
	}

	VIDEOIN_LOG_I("write frame done, size %d!\n", dump_length);
	filp_close(capture_fp, NULL);
#endif
}

static void disp_videoin_get_timing(uint16_t height,
			enum VIDEOIN_SRC_SEL src,
			bool *uv_swap, int *h_start,
			int *v_odd_start, int *v_even_start)
{
	if (height == 480) {
		switch (src) {
		case VIDEOIN_SRC_SEL_MVDO_FE_FIFO_INPUT:
			*h_start = 0x10060;
			*v_odd_start = 0x24;
			*v_even_start = 0x24;
			break;
		case VIDEOIN_SRC_SEL_SVDO_FE_FIFO_INPUT:
			*h_start = 0x10060;
			*v_odd_start = 0x24;
			*v_even_start = 0x24;
			break;
		case VIDEOIN_SRC_SEL_FHD_FE_FIFO_INPUT:
			*h_start = 0x1007B;
			*v_odd_start = 0x24;
			*v_even_start = 0x24;
			break;
		case VIDEOIN_SRC_SEL_UHD_FE_FIFO_INPUT:
			*h_start = 0x1007B;
			*v_odd_start = 0x24;
			*v_even_start = 0x24;
			break;
		case VIDEOIN_SRC_SEL_VDO_BE_FIFO_OUTPUT2:
		case VIDEOIN_SRC_SEL_VDO_BE_FIFO_OUTPUT:
			*h_start = 0x1003C;
			*v_odd_start = 0x27;
			*v_even_start = 0x27;
			break;
		case VIDEOIN_SRC_SEL_RGB2HDMI_OUTPUT:
			*h_start = 0x10078;
			*v_odd_start = 0x1E;
			*v_even_start = 0x1E;
			*uv_swap = true;
			break;
		case VIDEOIN_SRC_SEL_HDMI_RX_OUTPUT:
		case VIDEOIN_SRC_SEL_SD_PPF_OUTPUT:
		case VIDEOIN_SRC_SEL_P2I_OUTPUT:
		case VIDEOIN_SRC_SEL_VM_TOP_OUTPUT:
			break;
		default:
			VIDEOIN_LOG_E("Unknown dump point, src: %d\n", src);
			break;
		}
	} else if (height == 576) {

		switch (src) {
		case VIDEOIN_SRC_SEL_MVDO_FE_FIFO_INPUT:
			*h_start = 0X1006A;
			*v_odd_start = 0X2D;
			*v_even_start = 0X2D;
			break;
		case VIDEOIN_SRC_SEL_SVDO_FE_FIFO_INPUT:
			*h_start = 0X1006A;
			*v_odd_start = 0X2D;
			*v_even_start = 0X2D;
			break;
		case VIDEOIN_SRC_SEL_FHD_FE_FIFO_INPUT:
			*h_start = 0x10085;
			*v_odd_start = 0x2C;
			*v_even_start = 0x2C;
			break;
		case VIDEOIN_SRC_SEL_UHD_FE_FIFO_INPUT:
			*h_start = 0x10085;
			*v_odd_start = 0x2C;
			*v_even_start = 0x2C;
			break;
		case VIDEOIN_SRC_SEL_VDO_BE_FIFO_OUTPUT2:
		case VIDEOIN_SRC_SEL_VDO_BE_FIFO_OUTPUT:
			*h_start = 0x10044;
			*v_odd_start = 0x2F;
			*v_even_start = 0x2F;
			break;
		case VIDEOIN_SRC_SEL_RGB2HDMI_OUTPUT:
			*h_start = 0x10082;
			*v_odd_start = 0x27;
			*v_even_start = 0x27;
			*uv_swap = true;
			break;
		case VIDEOIN_SRC_SEL_HDMI_RX_OUTPUT:
		case VIDEOIN_SRC_SEL_SD_PPF_OUTPUT:
		case VIDEOIN_SRC_SEL_P2I_OUTPUT:
		case VIDEOIN_SRC_SEL_VM_TOP_OUTPUT:
			break;
		default:
			VIDEOIN_LOG_E("Unknown dump point, src: %d\n", src);
			break;
		}

	} else if (height == 720) {
		switch (src) {
		case VIDEOIN_SRC_SEL_MVDO_FE_FIFO_INPUT:
			*h_start = 0x100EC;
			*v_odd_start = 0x19;
			*v_even_start = 0x19;
			break;
		case VIDEOIN_SRC_SEL_SVDO_FE_FIFO_INPUT:
			*h_start = 0x100EC;
			*v_odd_start = 0x19;
			*v_even_start = 0x19;
			break;
		case VIDEOIN_SRC_SEL_FHD_FE_FIFO_INPUT:
			*h_start = 0x10106;
			*v_odd_start = 0x19;
			*v_even_start = 0x19;
			break;
		case VIDEOIN_SRC_SEL_UHD_FE_FIFO_INPUT:
			*h_start = 0x10106;
			*v_odd_start = 0x19;
			*v_even_start = 0x19;
			break;
		case VIDEOIN_SRC_SEL_VDO_BE_FIFO_OUTPUT2:
		case VIDEOIN_SRC_SEL_VDO_BE_FIFO_OUTPUT:
			*h_start = 0x100DC;
			*v_odd_start = 0x1C;
			*v_even_start = 0x1C;
			break;
		case VIDEOIN_SRC_SEL_RGB2HDMI_OUTPUT:
			*h_start = 0x100DB;
			*v_odd_start = 0x1A;
			*v_even_start = 0x1A;
			*uv_swap = true;
			break;
		case VIDEOIN_SRC_SEL_HDMI_RX_OUTPUT:
		case VIDEOIN_SRC_SEL_SD_PPF_OUTPUT:
		case VIDEOIN_SRC_SEL_P2I_OUTPUT:
		case VIDEOIN_SRC_SEL_VM_TOP_OUTPUT:
			break;
		default:
			VIDEOIN_LOG_E("Unknown dump point, src: %d\n", src);
			break;
		}

	} else if (height == 1080) {
		switch (src) {
		case VIDEOIN_SRC_SEL_MVDO_FE_FIFO_INPUT:
			*h_start = 0x100A8;
			*v_odd_start = 0x2A;
			*v_even_start = 0x2A;
			break;
		case VIDEOIN_SRC_SEL_SVDO_FE_FIFO_INPUT:
			*h_start = 0x100A8;
			*v_odd_start = 0x29;
			*v_even_start = 0x29;
			break;
		case VIDEOIN_SRC_SEL_FHD_FE_FIFO_INPUT:
			*h_start = 0x100C1;
			*v_odd_start = 0X2A;
			*v_even_start = 0X2A;
			break;
		case VIDEOIN_SRC_SEL_UHD_FE_FIFO_INPUT:
			*h_start = 0x100C1;
			*v_odd_start = 0X2A;
			*v_even_start = 0X2A;
			break;
		case VIDEOIN_SRC_SEL_VDO_BE_FIFO_OUTPUT2:
		case VIDEOIN_SRC_SEL_VDO_BE_FIFO_OUTPUT:
			*h_start = 0x10094;
			*v_odd_start = 0x2c;
			*v_even_start = 0x2c;
			break;
		case VIDEOIN_SRC_SEL_RGB2HDMI_OUTPUT:
			*h_start = 0x10093;
			*v_odd_start = 0x2A;
			*v_even_start = 0x2A;
			*uv_swap = true;
			break;
		case VIDEOIN_SRC_SEL_HDMI_RX_OUTPUT:
		case VIDEOIN_SRC_SEL_SD_PPF_OUTPUT:
		case VIDEOIN_SRC_SEL_P2I_OUTPUT:
		case VIDEOIN_SRC_SEL_VM_TOP_OUTPUT:
			break;
		default:
			VIDEOIN_LOG_E("Unknown dump point, src: %d\n", src);
			break;
		}

	} else if (height == 2160) {
		switch (src) {
		case VIDEOIN_SRC_SEL_MVDO_FE_FIFO_INPUT:
			*h_start = 0x10107;
			*v_odd_start = 0x54;
			*v_even_start = 0x54;
			break;
		case VIDEOIN_SRC_SEL_SVDO_FE_FIFO_INPUT:
			*h_start = 0x10106;
			*v_odd_start = 0x54;
			*v_even_start = 0x54;
			break;
		case VIDEOIN_SRC_SEL_FHD_FE_FIFO_INPUT:
			*h_start = 0X10181;
			*v_odd_start = 0x52;
			*v_even_start = 0x52;
			break;
		case VIDEOIN_SRC_SEL_UHD_FE_FIFO_INPUT:
			*h_start = 0x10181;
			*v_odd_start = 0x52;
			*v_even_start = 0x52;
			break;
		case VIDEOIN_SRC_SEL_VDO_BE_FIFO_OUTPUT2:
		case VIDEOIN_SRC_SEL_VDO_BE_FIFO_OUTPUT:
			*h_start = 0x10128;
			*v_odd_start = 0x55;
			*v_even_start = 0x55;
			break;
		case VIDEOIN_SRC_SEL_RGB2HDMI_OUTPUT:
			*h_start = 0x10128;
			*v_odd_start = 0x55;
			*v_even_start = 0x55;
			*uv_swap = true;
			break;
		case VIDEOIN_SRC_SEL_HDMI_RX_OUTPUT:
		case VIDEOIN_SRC_SEL_SD_PPF_OUTPUT:
		case VIDEOIN_SRC_SEL_P2I_OUTPUT:
		case VIDEOIN_SRC_SEL_VM_TOP_OUTPUT:
			break;
		default:
			VIDEOIN_LOG_E("Unknown dump point, src: %d\n", src);
			break;
		}

	}
}

void disp_videoin_dump_data(bool enable,
		enum VIDEOIN_SRC_SEL src, enum VIDEOIN_YCbCr_FORMAT fmt,
		enum VIDEO_BIT_MODE bit_mode, bool en_demode, bool en_align,
		dma_addr_t dump_addr_pa, uint16_t offset_x, uint16_t offset_y,
		uint16_t dump_width, uint16_t dump_height)
{
	int h_start = 0;
	int v_odd_start = 0;
	int v_even_start = 0;
	bool is_uv_swap;

	uint16_t htotal = disp_info.htotal;
	/* uint16_t vtotal = dovi_res.vtotal; */
	uint16_t width = disp_info.width;
	uint16_t height = disp_info.height;

	enum VIDEO_CHN_SEL cb_sel;
	enum VIDEO_CHN_SEL cr_sel;

	bool is_444;

	dma_addr_t y_dump_addr_pa = 0;
	dma_addr_t cb_dump_addr_pa = 0;
	dma_addr_t cr_dump_addr_pa = 0;

	VIDEOIN_LOG_I("res(%d %d), crop(%d %d %d %d)\n",
		width, height, offset_x, offset_y, dump_width, dump_height);

	if (!enable) {
		videoin_hal_enable(enable);
		return;
	}

	if (bit_mode == VIDEOIN_BITMODE_8) {
		is_16_packet = false;
	} else if (bit_mode == VIDEOIN_BITMODE_10) {
		is_16_packet = true;
	} else if (bit_mode == VIDEOIN_BITMODE_12) {
		is_16_packet = true;
	} else {
		VIDEOIN_LOG_E("bpp error %d\n", bit_mode);
		return;
	}

	if (!is_16_packet)
		dump_len = dump_width * dump_height;
	else if (is_16_packet)
		dump_len = dump_width * dump_height * 2;

	y_dump_addr_pa = dump_addr_pa;
	cb_dump_addr_pa = dump_addr_pa + dump_len;
	cr_dump_addr_pa = dump_addr_pa + 2 * dump_len;

	if (fmt == VIDEOIN_FORMAT_420)
		is_444 = false;
	else if (fmt == VIDEOIN_FORMAT_422)
		is_444 = false;
	else if (fmt == VIDEOIN_FORMAT_444)
		is_444 = true;
	else if (fmt > VIDEOIN_FORMAT_444) {
		VIDEOIN_LOG_E("color fmt error %d\n", fmt);
		return;
	}

	is_uv_swap = false;

	VIDEOIN_LOG_I("height = %d, src = %d\n", height, src);

	disp_videoin_get_timing(height, src, &is_uv_swap, &h_start,
			&v_odd_start, &v_even_start);
	h_start += offset_x;
	v_odd_start += offset_y;
	v_even_start += offset_y;

	if (fmt != VIDEOIN_FORMAT_444) {
		if ((offset_x % 2) == 0)
			is_uv_swap = false;
		else
			is_uv_swap = true;
		if (en_demode == true)
			is_uv_swap = true;
	}

	if (is_uv_swap) {
		cb_sel = SEL_CR_CHN;
		cr_sel = SEL_CB_CHN;
	} else {
		cb_sel = SEL_CB_CHN;
		cr_sel = SEL_CR_CHN;
	}

	VIDEOIN_LOG_I("en_align = %d, bit_mode = %d, fmt = %d, is_uv_swap = %d\n",
		en_align, bit_mode, fmt, is_uv_swap);

	if (enable) {
		videoin_hal_update_addr(y_dump_addr_pa,
					cb_dump_addr_pa,
					cr_dump_addr_pa, is_444);
		vdout_sys_hal_videoin_source_sel(src);
		videoin_hal_set_h(dump_width, htotal, is_444,
			is_16_packet, bit_mode);

		videoin_hal_set_h_align(en_align);
		videoin_hal_set_v(dump_height, fmt);
		videoin_hal_set_color_format(fmt);
		if (src == VIDEOIN_SRC_SEL_MVDO_FE_FIFO_INPUT ||
			src == VIDEOIN_SRC_SEL_SVDO_FE_FIFO_INPUT) {
			if (fmt == VIDEOIN_FORMAT_444)
				videoin_hal_set_channel_select(1, 2, 0);
			else
				videoin_hal_set_channel_select(1, 0, 2);
			videoin_hal_demode_enable(en_demode);
		} else {
			videoin_hal_set_channel_select(SEL_Y_CHN,
				cb_sel, cr_sel);
			videoin_hal_demode_enable(en_demode);
		}
		videoin_hal_set_bitmode(bit_mode, is_16_packet);
		videoin_hal_set_active_zone(h_start,
			v_odd_start, v_even_start);
		videoin_idk_set(enable, dump_width, height);
		//videoin_idk_set(enable, height);
		videoin_sys_hal_idk_set(enable, disp_info);
		//videoin_set_address_range(dump_width, dump_height, bit_mode, fmt);
	}
	videoin_hal_enable(enable);
}

void W2D_test(uint32_t enable, uint32_t src_sel, uint32_t color_fmt,
				uint32_t bit_mode, bool mode, bool en_align,
				uint16_t w, uint16_t h, uint16_t x, uint16_t y)
{

	videoin_alloc_transit_buf(bit_mode, color_fmt, w, h);

	disp_videoin_dump_data(enable,
		(enum VIDEOIN_SRC_SEL)(src_sel << 16),
		(enum VIDEOIN_YCbCr_FORMAT)color_fmt,
		(enum VIDEO_BIT_MODE)bit_mode, mode, en_align,
		buf_pa, x, y,
		w, h);
}

/***************** driver************/
struct disp_hw disp_videoin_driver = {
	.name = VIDEOIN_DRV_NAME,
	.init = disp_videoin_init,
	.deinit = disp_videoin_deinit,
	.start = NULL,
	.stop = NULL,
	.suspend = disp_videoin_suspend,
	.resume = disp_videoin_resume,
	.get_info = NULL,
	.change_resolution = disp_videoin_change_resolution,
	.config = NULL,
	.irq_handler = disp_videoin_irq_handle,
	.set_listener = NULL,
	.wait_event = NULL,
	.dump = NULL,
};

struct disp_hw *disp_videoin_get_drv(void)
{
	return &disp_videoin_driver;
}


