/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/slab.h>
#include "disp_vdp_if.h"
#include "disp_adl_if.h"
#include "disp_fg_common.h"
#include "disp_fg_tbl.h"
#include "disp_fg_drv.h"
#include "disp_fg_drv_v2.h"
#include "disp_fg_debug.h"

static struct adl_src_tbl fg_adl_tbl[MAX_FG];
struct disp_fg_info g_fg_info[MAX_FG];
struct mtk_fg_params g_fg_sets[MAX_FG];
bool fg_sw_auto_reg_filter = true;

struct mtk_fg_params *disp_fg_get_sets_buf(u32 hw_id)
{
	if (hw_id >= MAX_FG) {
		FG_ERR("invalid hw id %d\n", hw_id);
		return NULL;
	}

	return &g_fg_sets[hw_id];
}

void disp_fg_clean_sets_buf(u32 hw_id)
{
	if (hw_id >= MAX_FG) {
		FG_ERR("invalid hw id %d\n", hw_id);
		return;
	}

	memset(&g_fg_sets[hw_id], 0, sizeof(struct mtk_fg_params));
}

MS_U8 *disp_fg_get_adl_tbl(u32 hw_id)
{
	struct adl_src_tbl *adl_tbl;

	if (hw_id >= MAX_FG) {
		FG_ERR("invalid hw id %d\n", hw_id);
		return NULL;
	}

	adl_tbl = &fg_adl_tbl[hw_id];

	FG_LOG_I("fg %d adl_tbl base 0x%p\n",
		hw_id, adl_tbl->fgrain.p_data);

	return adl_tbl->fgrain.p_data;
}

void disp_fg_init_adl_tbl(u32 hw_id)
{
	struct adl_src_tbl *adl_tbl;

	if (hw_id >= MAX_FG)
		return;

	FG_FUNC();

	adl_tbl = &fg_adl_tbl[hw_id];

	if (hw_id == MAIN_FG)
		adl_tbl->client = FILM_GRAIN_MAIN;
	else
		adl_tbl->client = FILM_GRAIN_SUB;
	adl_tbl->fgrain.p_data = kmalloc(FILMG_SRC_LEN, GFP_KERNEL);
	adl_tbl->fgrain.used_size = FILMG_SRC_LEN;

	FG_LOG_I("fg %d adl_tbl base 0x%p\n", hw_id, adl_tbl->fgrain.p_data);
}

static void
init_scaling_function(MS_U8 scaling_points[][2], MS_U8 num_points, MS_U8 scaling_lut[])
{
	int i;
	int point;
	int x;
	int delta_y;
	int delta_x;
	MS_S64 delta;

	if (num_points == 0)
		return;

	i = 0;
	for (i = 0; i < scaling_points[0][0]; i++)
		scaling_lut[i] = scaling_points[0][1];

	point = 0;
	x = 0;
	for (point = 0; point < num_points - 1; point++) {
		delta_y = scaling_points[point + 1][1] - scaling_points[point][1];
		delta_x = scaling_points[point + 1][0] - scaling_points[point][0];
		if (delta_x != 0) {
			delta = delta_y * ((65536 + (delta_x >> 1)) / delta_x);
		} else {
			delta_x = 1;
			delta = delta_y * ((65536 + (delta_x >> 1)) / delta_x);
		}

		for (x = 0; x < delta_x; x++) {
			scaling_lut[scaling_points[point][0] + x] =
			scaling_points[point][1] + (int)((x * delta + 32768) >> 16);
		}
	}

	i = 0;
	for (i = scaling_points[num_points - 1][0]; i < 256; i++)
		scaling_lut[i] = scaling_points[num_points - 1][1];
}

void disp_fg_parser_lut(struct mtk_av1_film_grain_params *fg_param,
			struct fg_hw_reg_output *params_hw_reg)
{
	MS_U8 num;

	FG_FUNC();

	params_hw_reg->num_y_points = fg_param->num_y_points;
	if (params_hw_reg->num_y_points) {
		for (num = 0; num < params_hw_reg->num_y_points; num++) {
			params_hw_reg->scaling_points_y[num][0] = fg_param->point_y_value[num];
			params_hw_reg->scaling_points_y[num][1] = fg_param->point_y_scaling[num];
		}
	}

	params_hw_reg->chroma_scaling_from_luma = fg_param->chroma_scaling_from_luma;

	params_hw_reg->num_cb_points = fg_param->num_cb_points;
	if (params_hw_reg->num_cb_points) {
		for (num = 0; num < params_hw_reg->num_cb_points; num++) {
			params_hw_reg->scaling_points_cb[num][0] = fg_param->point_cb_value[num];
			params_hw_reg->scaling_points_cb[num][1] = fg_param->point_cb_scaling[num];
		}
	}

	params_hw_reg->num_cr_points = fg_param->num_cr_points;
	if (params_hw_reg->num_cr_points) {
		for (num = 0; num < params_hw_reg->num_cr_points; num++) {
			params_hw_reg->scaling_points_cr[num][0] = fg_param->point_cr_value[num];
			params_hw_reg->scaling_points_cr[num][1] = fg_param->point_cr_scaling[num];
		}
	}

	/* 96 hw just need 2 bits data */
	if (fg_param->grain_scaling >= FG_GRAIN_SCALE_OFF)
		params_hw_reg->grain_scaling =
			fg_param->grain_scaling - FG_GRAIN_SCALE_OFF;

	params_hw_reg->cb_mult = fg_param->cb_mult;
	params_hw_reg->cb_luma_mult = fg_param->cb_luma_mult;
	params_hw_reg->cb_offset = fg_param->cb_offset;

	params_hw_reg->cr_mult = fg_param->cr_mult;
	params_hw_reg->cr_luma_mult = fg_param->cr_luma_mult;
	params_hw_reg->cr_offset = fg_param->cr_offset;

	params_hw_reg->clip_to_restricted_range = fg_param->clip_to_restricted_range;
}

/* parser grain noise */
void disp_fg_parser_gns(struct mtk_av1_film_grain_params *params,
			struct fg_hw_reg_output *params_hw_reg)
{
	MS_U8 num = 0;
	MS_U8 NumPosLuma = 0;
	MS_U8 NumPosLuma_PlusOne = 0;
	MS_U8 NumPosChroma = 0;

	MS_U8 ar_coeffs_y[FG_AR_COEFFS_MAX] = {0};
	MS_U8 ar_coeffs_cb[FG_AR_COEFFS_MAX] = {0};
	MS_U8 ar_coeffs_cr[FG_AR_COEFFS_MAX] = {0};
	MS_U8 ar_coeff_lag = 0;

	FG_FUNC();

	params_hw_reg->grain_seed = params->grain_seed;
	params_hw_reg->grain_scale_shift = params->grain_scale_shift;

	ar_coeff_lag = params->ar_coeff_lag;
	NumPosLuma = FG_GRAIN_MARGIN_NS * ar_coeff_lag * (ar_coeff_lag + 1);

	if (params_hw_reg->num_y_points) {
		NumPosLuma_PlusOne = 1;
		for (num = 0; num < NumPosLuma; num++)
			ar_coeffs_y[num] =
				(MS_U8)((params->ar_coeffs_y[num]) &
					FG_AR_COEFF_MASK);
	}

	NumPosChroma = NumPosLuma_PlusOne + NumPosLuma;
	if (params_hw_reg->chroma_scaling_from_luma ||
	    params_hw_reg->num_cb_points) {
		for (num = 0; num < NumPosChroma; num++)
			ar_coeffs_cb[num] =
				(MS_U8)((params->ar_coeffs_cb[num]) &
					FG_AR_COEFF_MASK);
	}

	if (params_hw_reg->chroma_scaling_from_luma ||
	    params_hw_reg->num_cr_points) {
		for (num = 0; num < NumPosChroma; num++)
			ar_coeffs_cr[num] =
				(MS_U8)((params->ar_coeffs_cr[num]) &
					FG_AR_COEFF_MASK);
	}

	/* ar_coeff_shift minus 6 */
	params_hw_reg->ar_coeff_shift = params->ar_coeff_shift -
					FG_AR_COEFF_SHIT_MINUS;
	params_hw_reg->overlap_flag = params->overlap_flag;

	memset(params_hw_reg->ar_coeffs_y, 0,
	       sizeof(params_hw_reg->ar_coeffs_y));
	memset(params_hw_reg->ar_coeffs_cb, 0,
	       sizeof(params_hw_reg->ar_coeffs_cb));
	memset(params_hw_reg->ar_coeffs_cr, 0,
	       sizeof(params_hw_reg->ar_coeffs_cr));

	/* copy ar_coeffs_y[] */
	if (params_hw_reg->num_y_points) {
		NumPosLuma_PlusOne = 1;

		if (ar_coeff_lag == 3) {
			memcpy(params_hw_reg->ar_coeffs_y, ar_coeffs_y,
			       sizeof(params_hw_reg->ar_coeffs_y));
		} else if (ar_coeff_lag == 2) {
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_8] =
				ar_coeffs_y[FG_AR_COEFF_IDX_0];
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_9] =
				ar_coeffs_y[FG_AR_COEFF_IDX_1];
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_10] =
				ar_coeffs_y[FG_AR_COEFF_IDX_2];
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_11] =
				ar_coeffs_y[FG_AR_COEFF_IDX_3];
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_12] =
				ar_coeffs_y[FG_AR_COEFF_IDX_4];

			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_15] =
				ar_coeffs_y[FG_AR_COEFF_IDX_5];
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_16] =
				ar_coeffs_y[FG_AR_COEFF_IDX_6];
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_17] =
				ar_coeffs_y[FG_AR_COEFF_IDX_7];
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_18] =
				ar_coeffs_y[FG_AR_COEFF_IDX_8];
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_19] =
				ar_coeffs_y[FG_AR_COEFF_IDX_9];

			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_22] =
				ar_coeffs_y[FG_AR_COEFF_IDX_10];
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_23] =
				ar_coeffs_y[FG_AR_COEFF_IDX_11];
		} else if (ar_coeff_lag == 1) {
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_16] =
				ar_coeffs_y[FG_AR_COEFF_IDX_0];
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_17] =
				ar_coeffs_y[FG_AR_COEFF_IDX_1];
			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_18] =
				ar_coeffs_y[FG_AR_COEFF_IDX_2];

			params_hw_reg->ar_coeffs_y[FG_AR_COEFF_IDX_23] =
				ar_coeffs_y[FG_AR_COEFF_IDX_3];
		}
	} else {
		NumPosLuma_PlusOne = 0;
	}

	NumPosChroma = NumPosLuma_PlusOne + NumPosLuma;
	/* copy ar_coeffs_cb[] */
	if (params_hw_reg->chroma_scaling_from_luma ||
	    params_hw_reg->num_cb_points) {
		if (ar_coeff_lag == 3) {
			memcpy(params_hw_reg->ar_coeffs_cb, ar_coeffs_cb,
			       sizeof(params_hw_reg->ar_coeffs_cb));
		} else if (ar_coeff_lag == 2) {
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_8] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_0];
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_9] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_1];
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_10] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_2];
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_11] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_3];
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_12] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_4];

			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_15] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_5];
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_16] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_6];
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_17] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_7];
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_18] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_8];
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_19] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_9];

			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_22] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_10];
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_23] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_11];
			if (NumPosLuma_PlusOne == 1)
				params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_24]
					= ar_coeffs_cb[FG_AR_COEFF_IDX_12];
		} else if (ar_coeff_lag == 1) {
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_16] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_0];
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_17] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_1];
			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_18] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_2];

			params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_23] =
				ar_coeffs_cb[FG_AR_COEFF_IDX_3];
			if (NumPosLuma_PlusOne == 1)
				params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_24]
					= ar_coeffs_cb[FG_AR_COEFF_IDX_4];
		} else if (ar_coeff_lag == 0) {
			if (NumPosLuma_PlusOne == 1)
				params_hw_reg->ar_coeffs_cb[FG_AR_COEFF_IDX_24]
					= ar_coeffs_cb[FG_AR_COEFF_IDX_0];
		}
	}

	/* copy ar_coeffs_cr[] */
	if (params_hw_reg->chroma_scaling_from_luma ||
	    params_hw_reg->num_cr_points) {
		if (ar_coeff_lag == 3) {
			memcpy(params_hw_reg->ar_coeffs_cr, ar_coeffs_cr,
			       sizeof(params_hw_reg->ar_coeffs_cr));
		} else if (ar_coeff_lag == 2) {
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_8] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_0];
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_9] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_1];
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_10] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_2];
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_11] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_3];
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_12] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_4];

			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_15] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_5];
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_16] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_6];
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_17] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_7];
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_18] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_8];
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_19] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_9];

			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_22] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_10];
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_23] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_11];
			if (NumPosLuma_PlusOne == 1)
				params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_24]
					= ar_coeffs_cr[FG_AR_COEFF_IDX_12];
		} else if (ar_coeff_lag == 1) {
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_16] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_0];
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_17] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_1];
			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_18] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_2];

			params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_23] =
				ar_coeffs_cr[FG_AR_COEFF_IDX_3];
			if (NumPosLuma_PlusOne == 1)
				params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_24]
					= ar_coeffs_cr[FG_AR_COEFF_IDX_4];
		} else if (ar_coeff_lag == 0) {
			if (NumPosLuma_PlusOne == 1)
				params_hw_reg->ar_coeffs_cr[FG_AR_COEFF_IDX_24]
					= ar_coeffs_cr[FG_AR_COEFF_IDX_0];
		}
	}
}

void disp_fg_process_lut(struct fg_hw_reg_output *params_hw_reg,
			 struct fg_hw_adl_output *params_hw_adl)
{
	memset(params_hw_adl->u8scaling_lut_y256, 0, sizeof(params_hw_adl->u8scaling_lut_y256));
	memset(params_hw_adl->u8scaling_lut_cb256, 0, sizeof(params_hw_adl->u8scaling_lut_cb256));
	memset(params_hw_adl->u8scaling_lut_cr256, 0, sizeof(params_hw_adl->u8scaling_lut_cr256));

	FG_FUNC();

	init_scaling_function(params_hw_reg->scaling_points_y,
			      params_hw_reg->num_y_points,
			      params_hw_adl->u8scaling_lut_y256);

	if (params_hw_reg->chroma_scaling_from_luma) {
		memcpy(params_hw_adl->u8scaling_lut_cb256,
		       params_hw_adl->u8scaling_lut_y256,
		       sizeof(params_hw_adl->u8scaling_lut_y256));
		memcpy(params_hw_adl->u8scaling_lut_cr256,
		       params_hw_adl->u8scaling_lut_y256,
		       sizeof(params_hw_adl->u8scaling_lut_y256));
	} else {
		init_scaling_function(params_hw_reg->scaling_points_cb,
				      params_hw_reg->num_cb_points,
				      params_hw_adl->u8scaling_lut_cb256);
		init_scaling_function(params_hw_reg->scaling_points_cr,
				      params_hw_reg->num_cr_points,
				      params_hw_adl->u8scaling_lut_cr256);
	}
}

void disp_fg_process_gns(struct fg_hw_reg_output *params_hw_reg,
			 struct fg_hw_adl_output *params_hw_adl)
{
	MS_U8 luma_block_size_y = FG_LUMA_BLOCK_SIZE_Y;
	MS_U8 luma_block_size_x = FG_LUMA_BLOCK_SIZE_X;
	MS_U8 chroma_block_size_y = FG_CHROMA_BLOCK_SIZE_Y;
	MS_U8 chroma_block_size_x = FG_CHROMA_BLOCK_SIZE_X;
	MS_U8 luma_grain_stride = 0;
	MS_U8 chroma_grain_stride = 0;
	MS_U16 grain_seed = 0;


	int gauss_sec_shift;
	int i;
	int j;

	FG_FUNC();

	grain_seed = params_hw_reg->grain_seed;

	luma_grain_stride = luma_block_size_x; /* 82 */
	chroma_grain_stride = chroma_block_size_x; /* 44 */

	gauss_sec_shift = FG_MAX_BITS - params_hw_reg->bit_depth +
		params_hw_reg->grain_scale_shift;

	FG_LOG_D("grain_seed %u\n", grain_seed);
	FG_LOG_D("bit_depth %u grain_scale_shift %u gauss_sec_shift %d\n",
		params_hw_reg->bit_depth,
		params_hw_reg->grain_scale_shift,
		gauss_sec_shift);

	init_random_register(grain_seed);

	if (params_hw_reg->num_y_points) {
		/* 73 x 82 = 5986 */
		for (i = 0; i < luma_block_size_y; i++) {	/* 73 */
			for (j = 0; j < luma_block_size_x; j++)	/* 82 */
				params_hw_adl->y_grain_block[i *
				luma_grain_stride + j] =
				round2(GAUSS_VALUE, gauss_sec_shift);
		}
	} else {
		memset(params_hw_adl->y_grain_block, 0,
			sizeof(params_hw_adl->y_grain_block));
	}

	if (params_hw_reg->chroma_scaling_from_luma ||
	    params_hw_reg->num_cb_points) {
		init_random_register(grain_seed ^ CB_SEED_MASK);
		/* 38 x 44 = 1672 */
		for (i = 0; i < chroma_block_size_y; i++) {
			for (j = 0; j < chroma_block_size_x; j++)
				params_hw_adl->cb_grain_block[i *
				chroma_grain_stride + j] =
				round2(GAUSS_VALUE, gauss_sec_shift);
		}
	} else {
		memset(params_hw_adl->cb_grain_block, 0,
			sizeof(params_hw_adl->cb_grain_block));
	}


	if (params_hw_reg->chroma_scaling_from_luma ||
	    params_hw_reg->num_cr_points) {
		init_random_register(grain_seed ^ CR_SEED_MASK);
		for (i = 0; i < chroma_block_size_y; i++) {
			for (j = 0; j < chroma_block_size_x; j++)
				params_hw_adl->cr_grain_block[i *
				chroma_grain_stride + j] =
				round2(GAUSS_VALUE, gauss_sec_shift);
		}
	} else {
		memset(params_hw_adl->cr_grain_block, 0,
			sizeof(params_hw_adl->cr_grain_block));
	}
}

static uint64_t disp_fg_gettimeofday(void)
{
	struct timespec64 t;

	ktime_get_ts64(&t);

	return (t.tv_sec & FG_SEC_MASK) * FG_ONE_SEC_PER_NS + t.tv_nsec;
}

void disp_fg_sw_auto_reg(struct mtk_av1_film_grain_params *fg_param,
			 struct disp_fg_info *info)
{
	uint64_t start_timer, end_timer;
	int block_size = FG_LUMA_BLOCK_SIZE_X * FG_LUMA_BLOCK_SIZE_Y;
	int block_c_size = FG_CHROMA_BLOCK_SIZE_X * FG_CHROMA_BLOCK_SIZE_Y;
	int i;

	FG_FUNC();

	if (info == NULL) {
		FG_ERR("info is NULL\n");
		return;
	}

	start_timer = disp_fg_gettimeofday();
	info->hw_reg->force_write = 1;
	disp_fg_init_gns_grain_info(fg_param,
				    info->hw_reg,
				    &info->gns_info);
	disp_fg_pre_process_gns(fg_param, &info->gns_info, info->hw_adl,
				&info->gns_ar_info);
	disp_fg_pre_process_ar_coeffs(info->hw_reg);

	if (info->hw_reg->bit_depth == FG_MINI_BITS) {
		for (i = 0; i < block_size; i++) {
			if (i < block_c_size) {
				info->hw_adl->cb_grain_block[i] =
					info->hw_adl->cb_grain_block[i] << 2;
				info->hw_adl->cr_grain_block[i] =
					info->hw_adl->cr_grain_block[i] << 2;
			}
			info->hw_adl->y_grain_block[i] =
				info->hw_adl->y_grain_block[i] << 2;
		}
		info->hw_reg->bit_depth = FG_DEFAULT_BIT;
	}

	end_timer = disp_fg_gettimeofday();

	FG_SW_PER("sw filter %llu ns start %llu %llu\n",
		  (end_timer - start_timer),
		  start_timer,
		  end_timer);
}

void disp_fg_hw_auto_reg(struct mtk_av1_film_grain_params *fg_param,
			 struct disp_fg_info *info)
{
	uint32_t i = 0;
	MS_S32 *y_grain_block = info->hw_adl->y_grain_block;
	MS_S32 *cb_grain_block = info->hw_adl->cb_grain_block;
	MS_S32 *cr_grain_block = info->hw_adl->cr_grain_block;

	FG_FUNC();

	info->hw_reg->force_write = 0;

	for (i = 0; i < FG_LUAM_BLOCK_SIZE; i++)
		y_grain_block[i] = REVISE_VAL(y_grain_block[i],
					      FG_GRAIN_VALUE_MAX);

	for (i = 0; i < FG_CHROMA_BLOCK_SIZE; i++) {
		cb_grain_block[i] = REVISE_VAL(cb_grain_block[i],
					       FG_GRAIN_VALUE_MAX);
		cr_grain_block[i] = REVISE_VAL(cr_grain_block[i],
					       FG_GRAIN_VALUE_MAX);
	}
}

void disp_fg_sw_auto_reg_filter_enable(bool enable)
{
	FG_FUNC();

	fg_sw_auto_reg_filter = enable;
}

void disp_fg_free_gns_ar_scale_info(u32 hw_id)
{
	struct disp_fg_info *info = NULL;

	FG_FUNC();

	if (hw_id >= MAX_FG) {
		FG_ERR("invalid hw id %u\n", hw_id);
		return;
	}

	info = &g_fg_info[hw_id];

	disp_fg_free_gns_ar_block(&info->gns_ar_info);

	info->vsc_info.crop_w = 0;
	info->vsc_info.crop_h = 0;
	info->vsc_info.width = 0;
	info->vsc_info.height = 0;
}

void disp_fg_update_scale_info(u32 hw_id, struct mtk_fg_vsc_info *vsc_info)
{
	struct disp_fg_info *info = NULL;

	FG_FUNC();

	if (hw_id >= MAX_FG) {
		FG_ERR("invalid hw id %u\n", hw_id);
		return;
	}

	if (!fg_sw_auto_reg_filter)
		return;

	info = &g_fg_info[hw_id];

	if ((vsc_info->crop_w != vsc_info->width) ||
	    (vsc_info->crop_h != vsc_info->height)) {

		FG_LOG_D("scale_info [%ux%u -> %ux%u]\n",
			 vsc_info->crop_w,
			 vsc_info->crop_h,
			 vsc_info->width,
			 vsc_info->height);

		memcpy(&info->vsc_info, vsc_info,
		       sizeof(struct mtk_fg_vsc_info));
		disp_fg_init_gns_basic_info(&info->gns_info);
		disp_fg_update_gns_ar_block_info(&info->gns_info,
						 &info->vsc_info,
						 &info->gns_ar_info);
	}
}

void disp_fg_regenerate_8b_gns(struct mtk_av1_film_grain_params *fg_param,
			       struct disp_fg_info *info)
{
	uint64_t start_timer, end_timer;
	int block_size = FG_LUMA_BLOCK_SIZE_X * FG_LUMA_BLOCK_SIZE_Y;
	int block_c_size = FG_CHROMA_BLOCK_SIZE_X * FG_CHROMA_BLOCK_SIZE_Y;
	int i = 0;

	if (info == NULL) {
		FG_ERR("info is NULL\n");
		return;
	}
	if (info->hw_reg->bit_depth != FG_MINI_BITS)
		return;

	FG_FUNC();
	start_timer = disp_fg_gettimeofday();

	info->hw_reg->force_write = 1;
	disp_fg_init_gns_basic_info(&info->gns_info);
	disp_fg_init_gns_grain_info(fg_param,
				    info->hw_reg,
				    &info->gns_info);
	disp_fg_pre_process_gns_only(fg_param, &info->gns_info,
				     info->hw_adl,
				     &info->gns_ar_info);
	disp_fg_pre_process_ar_coeffs(info->hw_reg);

	for (i = 0; i < block_size; i++) {
		if (i < block_c_size) {
			info->hw_adl->cb_grain_block[i] =
				info->hw_adl->cb_grain_block[i] << 2;
			info->hw_adl->cr_grain_block[i] =
				info->hw_adl->cr_grain_block[i] << 2;
		}
		info->hw_adl->y_grain_block[i] =
			info->hw_adl->y_grain_block[i] << 2;
	}

	info->hw_reg->bit_depth = FG_DEFAULT_BIT;

	end_timer = disp_fg_gettimeofday();

	FG_SW_PER("sw filter %llu ns start %llu %llu\n",
		  (end_timer - start_timer),
		  start_timer,
		  end_timer);
}

static void fg_dbg_dump_table(u32 hw_id)
{
	struct disp_fg_info *info = NULL;
	u8 fg_idx = 0;
	int block_size = FG_LUMA_BLOCK_SIZE_X * FG_LUMA_BLOCK_SIZE_Y;
	int block_c_size = FG_CHROMA_BLOCK_SIZE_X * FG_CHROMA_BLOCK_SIZE_Y;
	int i = 0;

	if (!fg_dbg_get_table())
		return;

	info = &g_fg_info[hw_id];
	fg_idx = info->fg_idx;
	if (fg_idx < FG_MAX_PARAM_NS) {
		info->hw_reg = &info->fg_params_hw_reg[fg_idx];
		info->hw_adl = &info->fg_params_hw_adl[fg_idx];
	}

	if (info->hw_adl) {
		FG_LOG_I("dump raw data table start\n");
		for (i = 0; i < FILMG_SF_LEN; i++)
			pr_info("scaling y 0x%4.4x %px:%hhu\n",
				i,
				&info->hw_adl->u8scaling_lut_y256[i],
				info->hw_adl->u8scaling_lut_y256[i]);
		for (i = 0; i < FILMG_SF_LEN; i++)
			pr_info("scaling cb 0x%4.4x %px:%hhu\n",
				i,
				&info->hw_adl->u8scaling_lut_cb256[i],
				info->hw_adl->u8scaling_lut_cb256[i]);
		for (i = 0; i < FILMG_SF_LEN; i++)
			pr_info("scaling cr 0x%4.4x %px:%hhu\n",
				i,
				&info->hw_adl->u8scaling_lut_cr256[i],
				info->hw_adl->u8scaling_lut_cr256[i]);

		for (i = 0; i < block_size; i++)
			pr_info("grain_block y 0x%4.4x %px:[%d %x]\n",
				i, &info->hw_adl->y_grain_block[i],
				info->hw_adl->y_grain_block[i],
				info->hw_adl->y_grain_block[i]);
		for (i = 0; i < block_c_size; i++)
			pr_info("grain_block cb 0x%4.4x %px:[%d %x]\n",
				i, &info->hw_adl->cb_grain_block[i],
				info->hw_adl->cb_grain_block[i],
				info->hw_adl->cb_grain_block[i]);
		for (i = 0; i < block_c_size; i++)
			pr_info("grain_block cr 0x%4.4x %px:[%d %x]\n",
				i, &info->hw_adl->cr_grain_block[i],
				info->hw_adl->cr_grain_block[i],
				info->hw_adl->cr_grain_block[i]);
		FG_LOG_I("dump raw data table done\n");
	}
}

static int disp_fg_get_selected_set(u32 hw_id)
{
	struct mtk_fg_params *sets_buf = NULL;

	sets_buf = disp_fg_get_sets_buf(hw_id);
	if (!sets_buf)
		return 0;

	return sets_buf->select_set;
}

void disp_fg_config_hal(u32 hw_id,
		       struct video_buffer_info *buf,
		       struct mtk_fg_vsc_info *vsc_info)
{
	struct mtk_av1_film_grain_params param, *param_ptr = &param;
	struct mtk_vdp_film_grain_md_t *fg_md = NULL;
	struct mtk_fg_params *sets_buf = NULL;
	struct mtk_fg_param_set *set_buf = NULL;
	struct adl_src_tbl *adl_tbl = NULL;
	int ret = 0;

	FG_FUNC();

	adl_tbl = &fg_adl_tbl[hw_id];

	/* check if the film grain v2 is supported */
	fg_md = &buf->film_grain_info;
	if (disp_fg_support_new(fg_md)) {
		fg_dbg_dump_meta(fg_md);
		ret = disp_fg_handler_v2(hw_id, fg_md, vsc_info, adl_tbl);
		if (ret) {
			if (disp_fg_get_selected_set(hw_id) <= 0) {
				/* no valid set is selected */
				/* means fg not been configured before */
				FG_ERR("fg handle v2 failed %d\n", ret);
				goto bypass_fg;
			} else {
				FG_ERR("fg handle v2 failed ret %d\n", ret);
				/* keep the previous film grain setting */
				return;
			}
		}

		/* dump parameter for debug */
		if (!fg_dbg_dump_param_state())
			goto config_fg;
		sets_buf = disp_fg_get_sets_buf(hw_id);
		if (!sets_buf)
			goto config_fg;
		set_buf = disp_fg_get_best_param(sets_buf);
		if (!set_buf)
			goto config_fg;
		FG_LOG_I("best set[%d %d][%d %d %d %d]crop[%d %d %d %d]\n",
			sets_buf->select_set, sets_buf->num_sets,
			set_buf->bit_depth, vsc_info->bit_depth,
			set_buf->width, set_buf->height,
			vsc_info->crop_w, vsc_info->crop_h,
			vsc_info->width, vsc_info->height);
		fg_dbg_dump_param(&set_buf->av1_para);
	} else {
		if (fg_md->len != sizeof(struct mtk_av1_film_grain_params)) {
			FG_ERR("fg metadata size not correct[%zu %u]\n",
				 sizeof(struct mtk_av1_film_grain_params),
				 fg_md->len);
			goto bypass_fg;
		}
		disp_fg_update_scale_info(hw_id, vsc_info);
		memcpy(param_ptr, fg_md->buff, fg_md->len);
		ret = disp_fg_handler(hw_id, param_ptr, vsc_info, adl_tbl);
		if (ret) {
			if (disp_fg_get_selected_set(hw_id) <= 0) {
				/* no valid set is selected */
				/* means fg not been configured before */
				FG_ERR("fg handle failed %d\n", ret);
				goto bypass_fg;
			} else {
				FG_ERR("fg handle failed ret %d\n", ret);
				/* keep the previous film grain setting */
				return;
			}
		}
		/* dump parameter for debug */
		if (fg_dbg_dump_param_state())
			fg_dbg_dump_param(param_ptr);
	}
config_fg:
	/* configure film grain table*/
	disp_adl_cfg_client_en(adl_tbl->client, 1, 0);
	disp_filmg_config_adl_table(adl_tbl);
	FG_LOG_D("fg %d adl_tbl client %d used_size %d base %p\n",
		hw_id,
		adl_tbl->client,
		adl_tbl->fgrain.used_size,
		adl_tbl->fgrain.p_data);

	fg_dbg_dump_table(hw_id);

	return;

bypass_fg:
	fg_hal_bypass(hw_id, true);
}
