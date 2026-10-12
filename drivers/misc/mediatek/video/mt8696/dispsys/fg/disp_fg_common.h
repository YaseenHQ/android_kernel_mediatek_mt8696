/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __DISP_FG_COMMON_H__
#define __DISP_FG_COMMON_H__

#include "fg_hal.h"
#include "fg_info.h"
#include "disp_fg_pre_drv.h"

#define FG_MAX_PARAM_NS			2
#define FG_AR_COEFF_SHIT_MINUS		6
#define FG_AR_COEFF_MASK		0xFF

#define FG_GRAIN_VALUE_MAX		1024

#define FG_GRAIN_MARGIN_NS		2
#define FG_AR_COEFF_IDX_0		0
#define FG_AR_COEFF_IDX_1		1
#define FG_AR_COEFF_IDX_2		2
#define FG_AR_COEFF_IDX_3		3
#define FG_AR_COEFF_IDX_4		4
#define FG_AR_COEFF_IDX_5		5
#define FG_AR_COEFF_IDX_6		6
#define FG_AR_COEFF_IDX_7		7
#define FG_AR_COEFF_IDX_8		8
#define FG_AR_COEFF_IDX_9		9
#define FG_AR_COEFF_IDX_10		10
#define FG_AR_COEFF_IDX_11		11
#define FG_AR_COEFF_IDX_12		12
#define FG_AR_COEFF_IDX_13		13
#define FG_AR_COEFF_IDX_14		14
#define FG_AR_COEFF_IDX_15		15
#define FG_AR_COEFF_IDX_16		16
#define FG_AR_COEFF_IDX_17		17
#define FG_AR_COEFF_IDX_18		18
#define FG_AR_COEFF_IDX_19		19
#define FG_AR_COEFF_IDX_20		20
#define FG_AR_COEFF_IDX_21		21
#define FG_AR_COEFF_IDX_22		22
#define FG_AR_COEFF_IDX_23		23
#define FG_AR_COEFF_IDX_24		24

#define FG_LEFT_PAD			3
/* padding to offset for AR coefficients */
#define FG_RIGHT_PAD			3
#define FG_TOP_PAD			3
#define FG_BOTTOM_PAD			0
/* maximum lag used for stabilization of AR coefficients */
#define FG_AR_PADDING			3
#define FG_LUMA_SUBBLOCK_SIZE_Y		32
#define FG_LUMA_SUBBLOCK_SIZE_X		32
#define FG_CHROMA_SUBBLOCK_SIZE_Y	16
#define FG_CHROMA_SUBBLOCK_SIZE_X	16

#define FG_LUMA_SUBBLOCK_NS		2
#define FG_CHROMA_SUBBLOCK_NS		2

#define FG_MAX_BITS			12
#define FG_DEFAULT_BIT			10
#define FG_MINI_BITS			8
#define FG_CB_LINE			(7 << 5)
#define FG_CR_LINE			(11 << 5)
#define FG_GRAIN_SCALE_OFF		8

#define FG_ONE_SEC_PER_NS		(1000000000)
#define FG_SEC_MASK			(0xFFF)

#define FG_CHECk_VALUE(para, over, handle)		\
do {							\
	if ((para) > (over)) {				\
		FG_ERR("%s %d overflow %d!!!\n", #para, (para), (over)); \
		(para) = (handle);			\
	}						\
} while (0)

struct video_buffer_info;

struct disp_fg_info {
	MS_U8 fg_idx;
	struct fg_hw_reg_output fg_params_hw_reg[FG_MAX_PARAM_NS];
	struct fg_hw_adl_output fg_params_hw_adl[FG_MAX_PARAM_NS];
	struct fg_hw_reg_output *hw_reg;
	struct fg_hw_adl_output *hw_adl;
	struct fg_gns_basic_info gns_info;
	struct mtk_fg_vsc_info vsc_info;
	struct fg_gns_ar_block_info gns_ar_info;
};
extern struct disp_fg_info g_fg_info[MAX_FG];
extern bool fg_sw_auto_reg_filter;

struct mtk_fg_params *disp_fg_get_sets_buf(u32 hw_id);
void disp_fg_clean_sets_buf(u32 hw_id);
MS_U8 *disp_fg_get_adl_tbl(u32 hw_id);
void disp_fg_init_adl_tbl(u32 hw_id);
void disp_fg_parser_lut(struct mtk_av1_film_grain_params *fg_param,
			struct fg_hw_reg_output *params_hw_reg);
void disp_fg_parser_gns(struct mtk_av1_film_grain_params *params,
			struct fg_hw_reg_output *params_hw_reg);
void disp_fg_process_lut(struct fg_hw_reg_output *params_hw_reg,
			 struct fg_hw_adl_output *params_hw_adl);
void disp_fg_process_gns(struct fg_hw_reg_output *params_hw_reg,
			 struct fg_hw_adl_output *params_hw_adl);
void disp_fg_sw_auto_reg(struct mtk_av1_film_grain_params *fg_param,
			 struct disp_fg_info *info);
void disp_fg_hw_auto_reg(struct mtk_av1_film_grain_params *fg_param,
			 struct disp_fg_info *info);
void disp_fg_regenerate_8b_gns(struct mtk_av1_film_grain_params *fg_param,
			       struct disp_fg_info *info);
void disp_fg_sw_auto_reg_filter_enable(bool enable);
void disp_fg_update_scale_info(u32 hw_id, struct mtk_fg_vsc_info *vsc_info);
void disp_fg_free_gns_ar_scale_info(u32 hw_id);
void disp_fg_config_hal(u32 hw_id,
		       struct video_buffer_info *buf,
		       struct mtk_fg_vsc_info *vsc_info);

#endif /* __DISP_FG_COMMON_H__ */
