/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "disp_bitstream.h"
#include "disp_adl_if.h"
#include "disp_fg_common.h"
#include "disp_fg_drv_v2.h"
#include "disp_fg_tbl.h"
#include "disp_fg_debug.h"

static int
disp_fg_av1_params(struct mtk_fg_params *paras,
		   struct bit_render *br, int idx)
{
	struct mtk_fg_param_set *fg_para, *fgp_ref = NULL;
	struct mtk_av1_film_grain_params *av1_para, *av1_para_ref;
	int i, set_idx, apply_units_log2, vsc_flag;
	int num_y_coeffs, num_c_coeffs, luma_only;
	int pred_scale, pred_y_scale, pred_cb_scale, pred_cr_scale;
	int res_y[FG_NUM_Y_POINTS_MAX], res_c[FG_NUM_C_POINTS_MAX], mid, gran;
	int bits_inc, bits_scaling, uv_offset, y_value, uv_value, uv_scale;
	int scale_mult, scale_add, bits_res, cicp_flag, ar_bits;
	int ret = FG_OK;

	pred_cb_scale = 0;
	pred_cr_scale = 0;
	set_idx = read_bits(br, 3);
	fg_para = &paras->sets[set_idx];
	av1_para = &fg_para->av1_para;

	av1_para->apply_grain = read_bit(br);
	if (!av1_para->apply_grain) {
		FG_LOG_D("this set disable fg [%d %d]\n", idx, set_idx);
		return FG_OK;
	}

	av1_para->grain_seed = read_bits(br, 16);
	/* the first frame, update_grain must be 1 */
	av1_para->update_grain = read_bit(br);
	if (!av1_para->update_grain) {
		/**
		 * when update grain is 0
		 * we just need to update grain seed of this set
		 */
		FG_LOG_D("no update_grain flag, skip this set [%d %d]\n",
			 idx, set_idx);
		return FG_OK;
	}


	apply_units_log2  = read_bits(br, 4);
	fg_para->width  = read_bits(br, 12) << apply_units_log2;
	fg_para->height = read_bits(br, 12) << apply_units_log2;
	luma_only = read_bit(br);
	if (luma_only) {
		fg_para->subsampling_x = fg_para->subsampling_y = 0;
	} else {
		fg_para->subsampling_x = read_bit(br);
		fg_para->subsampling_y = read_bit(br);
	}
	/* video_signal_characteristics_flag */
	vsc_flag = read_bit(br);
	if (vsc_flag) {
		fg_para->bit_depth = read_bits(br, 3) + 8;
		cicp_flag = read_bit(br);
		if (cicp_flag) {
			fg_para->reserved[0] = read_bits(br, 8);
			fg_para->reserved[1] = read_bits(br, 8);
			fg_para->reserved[2] = read_bits(br, 8);
			fg_para->reserved[3] = read_bit(br);
		}
	}

	if (paras->has_ref == 1)
		fgp_ref = &paras->sets[0];

	pred_scale = read_bit(br);
	/* prediction must be from different set */
	if (pred_scale && (!fgp_ref || fgp_ref == fg_para)) {
		ret = FG_INVAL_PRE_SCALE;
		goto clean_set;
	}

	av1_para_ref = &fgp_ref->av1_para;
	pred_y_scale = pred_scale ? read_bit(br) : 0;
	if (pred_y_scale) {
		scale_mult = read_bits(br, 9);
		scale_add = read_bits(br, 9);
		bits_res = read_bits(br, 3);
		if (bits_res) {
			av1_para->num_y_points = av1_para_ref->num_y_points;
			for (i = 0; i < av1_para->num_y_points; i++)
				res_y[i] = read_bits(br, bits_res);
			gran = read_bits(br, 3);
			for (i = 0; i < av1_para->num_y_points; i++) {
				av1_para->point_y_value[i] =
					av1_para_ref->point_y_value[i];
				mid = av1_para_ref->point_y_scaling[i];
				mid = ((mid * (scale_mult - 256) + 8) >> 4)
					+ scale_add - 256;
				mid += (res_y[i]
					- (1 << (bits_res - 1))) * gran;
				av1_para->point_y_scaling[i] = av_clip_u8(mid);
			}
		}
	} else {
		av1_para->num_y_points = read_bits(br, 4);
		if (av1_para->num_y_points > FG_NUM_Y_POINTS_MAX) {
			ret = FG_YPOINT_EXCEED;
			goto clean_set;
		} else if (av1_para->num_y_points) {
			y_value = 0;
			bits_inc = read_bits(br, 3) + 1;
			bits_scaling = read_bits(br, 2) + 5;
			for (i = 0; i < av1_para->num_y_points; i++) {
				y_value += read_bits(br, bits_inc);
				if (y_value > UINT8_MAX) {
					ret = FG_YVALUE_EXCEED;
					goto clean_set;
				}
				av1_para->point_y_value[i] = y_value;
				av1_para->point_y_scaling[i] =
					read_bits(br, bits_scaling);
			}
		}
	}

	FG_LOG_D("off luma pred[%d %d %d %d] w/h[%d %d %d %d]\n",
		 apply_units_log2, luma_only,
		 pred_scale, pred_y_scale,
		 fg_para->width, fg_para->height,
		 fg_para->subsampling_x, fg_para->subsampling_y);

	if (luma_only)
		av1_para->chroma_scaling_from_luma = 0;
	else
		av1_para->chroma_scaling_from_luma = read_bit(br);

	if (luma_only || av1_para->chroma_scaling_from_luma) {
		av1_para->num_cb_points = 0;
		av1_para->num_cr_points = 0;
	} else {
		/* read cb points */
		pred_cb_scale = pred_scale ? read_bit(br) : 0;
		if (pred_cb_scale) {
			uv_scale = read_bits(br, 9);
			uv_offset = read_bits(br, 9);
			bits_res = read_bits(br, 3);
			av1_para->cb_mult = av1_para_ref->cb_mult;
			av1_para->cb_luma_mult = av1_para_ref->cb_luma_mult;
			av1_para->cb_offset = av1_para_ref->cb_offset;
			if (bits_res) {
				av1_para->num_cb_points =
					av1_para_ref->num_cb_points;
				for (i = 0; i < av1_para->num_cb_points; i++)
					res_c[i] = read_bits(br, bits_res);
				gran = read_bits(br, 3);
				for (i = 0; i < av1_para->num_cb_points; i++) {
					av1_para->point_cb_value[i] =
						av1_para_ref->point_cb_value[i];
					mid = av1_para_ref->point_cb_scaling[i];
					mid = (mid * (uv_scale - 256) + 8) >> 4;
					mid = mid + uv_offset - 256;
					mid += (res_c[i]
						- (1 << (bits_res - 1))) * gran;
					av1_para->point_cb_scaling[i] =
						av_clip_u8(mid);
				}
			}
		} else {
			uv_value = 0;
			av1_para->num_cb_points = read_bits(br, 4);
			if (av1_para->num_cb_points > FG_NUM_C_POINTS_MAX) {
				ret = FG_CB_POINT_EXCEED;
				goto clean_set;
			}
			bits_inc = read_bits(br, 3) + 1;
			bits_scaling = read_bits(br, 2) + 5;
			uv_offset = read_bits(br, 8);
			for (i = 0; i < av1_para->num_cb_points; i++) {
				uv_value += read_bits(br, bits_inc);
				if (uv_value > UINT8_MAX) {
					ret = FG_CB_VALUE_EXCEED;
					goto clean_set;
				}
				av1_para->point_cb_value[i] = uv_value;
				av1_para->point_cb_scaling[i] =
				read_bits(br, bits_scaling) + uv_offset;
			}
		}

		/* read cr points */
		pred_cr_scale = pred_scale ? read_bit(br) : 0;
		if (pred_cr_scale) {
			uv_scale = read_bits(br, 9);
			uv_offset = read_bits(br, 9);
			bits_res = read_bits(br, 3);
			av1_para->cr_mult = av1_para_ref->cr_mult;
			av1_para->cr_luma_mult = av1_para_ref->cr_luma_mult;
			av1_para->cr_offset = av1_para_ref->cr_offset;
			if (bits_res) {
				av1_para->num_cr_points =
					av1_para_ref->num_cr_points;
				for (i = 0; i < av1_para->num_cr_points; i++)
					res_c[i] = read_bits(br, bits_res);
				gran = read_bits(br, 3);
				for (i = 0; i < av1_para->num_cr_points; i++) {
					av1_para->point_cr_value[i] =
						av1_para_ref->point_cr_value[i];
					mid = av1_para_ref->point_cr_scaling[i];
					mid = (mid * (uv_scale - 256) + 8) >> 4;
					mid = mid + uv_offset - 256;
					mid += (res_c[i]
						-(1 << (bits_res - 1))) * gran;
					av1_para->point_cr_scaling[i] =
						av_clip_u8(mid);
				}
			}
		} else {
			uv_value = 0;
			av1_para->num_cr_points = read_bits(br, 4);
			if (av1_para->num_cr_points > FG_NUM_C_POINTS_MAX) {
				ret = FG_CR_POINT_EXCEED;
				goto clean_set;
			}
			bits_inc = read_bits(br, 3) + 1;
			bits_scaling = read_bits(br, 2) + 5;
			uv_offset = read_bits(br, 8);
			for (i = 0; i < av1_para->num_cr_points; i++) {
				uv_value += read_bits(br, bits_inc);
				if (uv_value > UINT8_MAX) {
					ret = FG_CR_VALUE_EXCEED;
					goto clean_set;
				}
				av1_para->point_cr_value[i] = uv_value;
				av1_para->point_cr_scaling[i] =
				read_bits(br, bits_scaling) + uv_offset;
			}
		}
	}

	av1_para->grain_scaling = read_bits(br, 2) + 8;
	av1_para->ar_coeff_lag = read_bits(br, 2);
	num_y_coeffs = 2 * av1_para->ar_coeff_lag *
			(av1_para->ar_coeff_lag + 1);
	if (av1_para->num_y_points || pred_y_scale) {
		ar_bits = read_bits(br, 2) + 5;
		num_c_coeffs = num_y_coeffs + 1;
		for (i = 0; i < num_y_coeffs; i++)
			av1_para->ar_coeffs_y[i] =
			read_bits(br, ar_bits) - (1 << (ar_bits - 1));
		for (i = num_y_coeffs; i < FG_AR_COEFFS_MAX; i++)
			av1_para->ar_coeffs_y[i] = 0;
	} else {
		num_c_coeffs = num_y_coeffs;
	}

	if (av1_para->chroma_scaling_from_luma ||
	    av1_para->num_cb_points || pred_cb_scale) {
		ar_bits = read_bits(br, 2) + 5;
		for (i = 0; i < num_c_coeffs; i++)
			av1_para->ar_coeffs_cb[i] =
			read_bits(br, ar_bits) - (1 << (ar_bits - 1));
		for (i = num_c_coeffs; i < FG_AR_COEFFS_MAX; i++)
			av1_para->ar_coeffs_cb[i] = 0;
	}

	if (av1_para->chroma_scaling_from_luma ||
	    av1_para->num_cr_points || pred_cr_scale) {
		ar_bits = read_bits(br, 2) + 5;
		for (i = 0; i < num_c_coeffs; i++)
			av1_para->ar_coeffs_cr[i] =
			read_bits(br, ar_bits) - (1 << (ar_bits - 1));
		for (i = num_c_coeffs; i < FG_AR_COEFFS_MAX; i++)
			av1_para->ar_coeffs_cr[i] = 0;
	}

	av1_para->ar_coeff_shift = read_bits(br, 2) + 6;
	av1_para->grain_scale_shift = read_bits(br, 2);
	if (av1_para->num_cb_points && !pred_cb_scale) {
		av1_para->cb_mult = read_bits(br, 8);
		av1_para->cb_luma_mult = read_bits(br, 8);
		av1_para->cb_offset = read_bits(br, 9);
	}
	if (av1_para->num_cr_points && !pred_cr_scale) {
		av1_para->cr_mult = read_bits(br, 8);
		av1_para->cr_luma_mult = read_bits(br, 8);
		av1_para->cr_offset = read_bits(br, 9);
	}

	av1_para->overlap_flag = read_bit(br);
	av1_para->clip_to_restricted_range = read_bit(br);

	/* use first set as reference only if it was fully parsered */
	if (idx == 0)
		paras->has_ref = 1;

	return FG_OK;

clean_set:
	memset(fg_para, 0, sizeof(struct mtk_fg_param_set));
	return ret;
}

static int
disp_fg_av1_payload(struct mtk_fg_params *paras,
		    struct bit_render *br, int idx)
{
	int payload_4byte, payload_size, payload_bits, start_position;
	int ret = FG_OK;

	start_position = read_bits_count(br);
	payload_4byte = read_bit(br);
	payload_size = read_bits(br, payload_4byte ? 2 : 8);
	if (payload_size == 0) {
		FG_LOG_I("payload_size is zero!\n");
		return ret;
	}

	ret = disp_fg_av1_params(paras, br, idx);
	if (ret < 0)
		return ret;

	payload_bits = read_bits_count(br) - start_position;
	FG_LOG_D("payload bits start %d read %d total %d\n",
		 start_position, payload_bits, payload_size * 8);
	if (payload_bits > payload_size * 8) {
		FG_ERR("payload_bits too large [%d %d]\n",
			payload_bits, payload_size);
		return FG_METADATA_EXCEED;
	}

	skip_bits(br, payload_size * 8 - payload_bits);

	return FG_OK;
}

static int
disp_fg_set_best_param(struct mtk_fg_params *paras,
		       struct mtk_fg_vsc_info *vsc_info)
{
	struct mtk_fg_param_set *cur_set, *best_set = NULL;
	int delta_w[FG_MAX_SETS], delta_h[FG_MAX_SETS];
	int select = 0, disp_w, disp_h, crop_w, crop_h, bit_depth;
	int i = 0, delta_idx = 0;

	crop_w = vsc_info->crop_w;
	crop_h = vsc_info->crop_h;
	disp_w = vsc_info->width;
	disp_h = vsc_info->height;

	for (i = 0; i < (paras->num_sets + 1); i++) {
		cur_set = &paras->sets[i];
		bit_depth = cur_set->bit_depth;

		if (!cur_set->av1_para.apply_grain) {
			FG_LOG_D("skip set[%d %d][%d %d]crop[%d %d %d %d]\n",
				 paras->num_sets, bit_depth,
				 cur_set->width, cur_set->height,
				 crop_w, crop_h, disp_w, disp_h);
			continue;
		}
		/* check format, decode format always be 4:2:0 */
		if ((!cur_set->subsampling_x) || (!cur_set->subsampling_y) ||
		    ((bit_depth != 0) && (bit_depth != vsc_info->bit_depth))) {
			FG_LOG_D("not the same with decode format[%d %d %d][%d %d]\n",
				 i, cur_set->subsampling_x, cur_set->subsampling_y,
				 bit_depth, vsc_info->bit_depth);
			continue;
		}

		delta_w[i] = abs(cur_set->width - disp_w);
		delta_h[i] = abs(cur_set->height - disp_h);
		if (!best_set) {
			best_set = cur_set;
			select = i + 1;
			delta_idx = i;
		} else {
			/* here i must be greater than 0 */
			/* compare with previous one */
			/* select the one with smaller delta w and h */
			if ((delta_w[i] < delta_w[delta_idx]) &&
			    (delta_h[i] < delta_h[delta_idx])) {
				best_set = cur_set;
				select = i + 1;
				delta_idx = i;
			}
		}

		/* select the best one according to DB command */
		if (fg_dbg_get_select() > 0 &&
		    fg_dbg_get_select() <= FG_MAX_SETS)
			select = fg_dbg_get_select();

		FG_LOG_D("better set[%d %d %d][%d %d %d %d]crop[%d %d %d %d]\n",
			 select, i, paras->num_sets,
			 best_set->bit_depth, vsc_info->bit_depth,
			 best_set->width, best_set->height,
			 crop_w, crop_h, disp_w, disp_h);

		if (best_set->bit_depth == 0)
			best_set->bit_depth = vsc_info->bit_depth;
	}

	/* check if the best selected set is valid */
	if (select <= 0 || select > FG_MAX_SETS) {
		paras->is_match = false;
		return FG_INVAL_BEST_SET;
	} else {
		paras->select_set = select;
		if (delta_w[select - 1] == 0)
			paras->is_match = true;
		else
			paras->is_match = false;
	}

	return select;
}

static int
disp_fg_load_param(struct disp_fg_info *info, struct mtk_fg_params *paras,
		   struct mtk_fg_vsc_info *vsc_info)
{
	struct mtk_fg_param_set *para_set = NULL;
	struct mtk_av1_film_grain_params *av1_para = NULL;
	struct fg_hw_reg_output *hw_reg = NULL;

	FG_FUNC();
	para_set = disp_fg_get_best_param(paras);
	if (!para_set) {
		FG_ERR("no best para set found!\n");
		return FG_INVAL_BEST_SET;
	}

	hw_reg = info->hw_reg;
	/* follow customer golden, use source bit depth */
	hw_reg->bit_depth = vsc_info->bit_depth;

	av1_para = &para_set->av1_para;

	/* vdec meta data -> fg hw reg setting */
	disp_fg_parser_lut(av1_para, hw_reg);
	disp_fg_parser_gns(av1_para, hw_reg);

	/* generate adl raw data */
	disp_fg_process_lut(hw_reg, info->hw_adl);
	disp_fg_process_gns(hw_reg, info->hw_adl);

	return FG_OK;
}

static int
disp_fg_afgs_parser(struct mtk_fg_params *paras,
		    struct mtk_vdp_film_grain_md_t *fg_md,
		    struct mtk_fg_vsc_info *vsc_info,
		    bool is_fg_ref)
{
	struct bit_render gbit_r, *bit_r = &gbit_r;
	int ret = 0, i = 0;
	u32 off = 0;

	if (is_fg_ref) {
		if (fg_md->len == fg_md->len_all)
			return FG_INVAL_PARAM;

		off = fg_md->offset_i - fg_md->offset;
		init_bit_operations(bit_r, &fg_md->buff[off], fg_md->len_i);
	} else {
		init_bit_operations(bit_r, fg_md->buff, fg_md->len);
	}
	/* skip version country code & provider code */
	skip_bits(bit_r, 4 * 8);

	paras->afgs_enable = read_bit(bit_r);
	if (!paras->afgs_enable) {
		ret = FG_NOT_ENABLE;
		goto error;
	}

	/* reserved */
	skip_bits(bit_r, 4);
	paras->num_sets = read_bits(bit_r, 3);
	if (paras->num_sets >= FG_MAX_SETS)
		paras->num_sets = FG_MAX_SETS - 1;

	for (i = 0; i < (paras->num_sets + 1); i++) {
		ret = disp_fg_av1_payload(paras, bit_r, i);
		if (ret < 0)
			goto error;
	}
	/* reset the reference flag after parsing */
	paras->has_ref = 0;

	ret = disp_fg_set_best_param(paras, vsc_info);
	if (ret < 0)
		goto error;

	return FG_OK;

error:
	return ret;
}

/**
 * @brief Check whether film grain is supported or not
 * @param [in] fg_md Pointer to the film grain metadata
 */
bool disp_fg_support_new(struct mtk_vdp_film_grain_md_t *fg_md)
{
	uint32_t country_code, provider_code, provider_orient_code;
	struct bit_render gbit_r, *bit_r = &gbit_r;

	init_bit_operations(bit_r, fg_md->buff, fg_md->len);
	country_code = read_byte(bit_r);
	if (country_code != 0xB5) {
		FG_LOG_D("country code not match %x\n", country_code);
		return false;
	}

	provider_code = read_bytes(bit_r, 2);
	if (provider_code != 0x5890) {
		FG_LOG_D("provider code not match %x\n", provider_code);
		return false;
	}

	provider_orient_code = read_byte(bit_r);
	if (provider_orient_code != 0x01) {
		FG_LOG_D("oriented code not match %x\n", provider_orient_code);
		return false;
	}

	FG_LOG_D("AFGS version code[%02x%04x%02x]\n",
		 country_code, provider_code, provider_orient_code);

	return true;
}

/**
 * @brief Get the best film grain parameters from the parsed parameters
 * @param [in] paras Pointer to the parsed film grain parameters
 */
struct mtk_fg_param_set *disp_fg_get_best_param(struct mtk_fg_params *paras)
{
	int select;

	if (!paras->afgs_enable)
		return NULL;

	select = paras->select_set - 1;
	/* check if the best selected set is valid */
	if (select >= FG_MAX_SETS)
		return NULL;

	return &paras->sets[select];
}

/**
 * @brief Configures the HAL with the given film grain parameters
 * @param [in] hw_id The hardware ID
 * @param [in] fg_md Pointer to the film grain parameters
 * @param [in] vsc_info Pointer to the vsc_info
 */
int disp_fg_handler_v2(u32 hw_id,
		       struct mtk_vdp_film_grain_md_t *fg_md,
		       struct mtk_fg_vsc_info *vsc_info,
		       struct adl_src_tbl *adl_tbl)
{
	struct mtk_av1_film_grain_params *param = NULL;
	struct disp_fg_info *info = NULL;
	struct mtk_fg_params *sets_buf = NULL;
	struct mtk_fg_param_set *set_buf = NULL;
	MS_U8 *fg_idx = NULL;
	int ret = 0;

	FG_FUNC();
	sets_buf = disp_fg_get_sets_buf(hw_id);
	if (!sets_buf) {
		ret = FG_INVAL_POINT;
		return ret;
	}

	ret = disp_fg_afgs_parser(sets_buf, fg_md, vsc_info, false);
	/* After the first parse is completed, regardless of the
	 * return value, if it is not the best match setting and there
	 * is more than one setting, then it is necessary to parse the
	 * key frame metadata to obtain the best match setting.
	 */
	if (ret < 0 || !sets_buf->is_match) {
		FG_LOG_D("%s %d ret %d is_match %d num_sets %d",
			 __func__, __LINE__, ret,
			 sets_buf->is_match,
			 sets_buf->num_sets);

		if (sets_buf->num_sets > 0)
			ret = disp_fg_afgs_parser(
				sets_buf, fg_md, vsc_info, true);
		if (ret < 0)
			return ret;
	}

	info = &g_fg_info[hw_id];
	/* if update_grain == 0, use previous params */
	set_buf = disp_fg_get_best_param(sets_buf);
	if (!set_buf) {
		ret = FG_INVAL_POINT;
		return ret;
	}

	param = &set_buf->av1_para;
	fg_idx = &info->fg_idx;
	if (param->update_grain)
		*fg_idx = (*fg_idx + 1) % FG_MAX_PARAM_NS;
	info->hw_reg = &info->fg_params_hw_reg[*fg_idx];
	info->hw_adl = &info->fg_params_hw_adl[*fg_idx];
	ret = disp_fg_load_param(info, sets_buf, vsc_info);
	if (ret < 0)
		return ret;

	if (vsc_info->bit_depth == FG_MINI_BITS)
		disp_fg_regenerate_8b_gns(param, info);
	else
		info->hw_reg->force_write = 0;

	fg_hal_update_process(hw_id, info->hw_reg, info->hw_adl, adl_tbl);

	return FG_OK;
}
