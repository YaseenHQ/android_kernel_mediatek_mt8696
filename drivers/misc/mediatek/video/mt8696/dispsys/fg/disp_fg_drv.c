// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/semaphore.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sched/clock.h>
#include <uapi/linux/time.h>
#include <linux/time64.h>

#include "disp_hw_mgr.h"
#include "disp_adl_if.h"
#include "disp_fg_common.h"
#include "disp_fg_drv.h"

static void disp_fg_check_vdec_md(struct mtk_av1_film_grain_params *fg_param)
{
	FG_FUNC();

	FG_CHECk_VALUE(fg_param->num_y_points, 14, 0);

	FG_CHECk_VALUE(fg_param->num_cb_points, 10, 0);

	FG_CHECk_VALUE(fg_param->num_cr_points, 10, 0);

	FG_CHECk_VALUE(fg_param->grain_scaling, 11, 11);

	FG_CHECk_VALUE(fg_param->ar_coeff_lag, 3, 0);

	FG_CHECk_VALUE(fg_param->ar_coeff_shift, 9, 9);

	FG_CHECk_VALUE(fg_param->overlap_flag, 1, 1);

	FG_CHECk_VALUE(fg_param->clip_to_restricted_range, 1, 0);

	FG_CHECk_VALUE(fg_param->chroma_scaling_from_luma, 1, 0);
}

/* vdec metada data -> fg hw reg setting & adl raw data */
static void disp_fg_parser_meta(struct mtk_av1_film_grain_params *fg_param,
				struct fg_hw_reg_output *params_hw_reg,
				struct fg_hw_adl_output *params_hw_adl)
{
	FG_FUNC();

	disp_fg_check_vdec_md(fg_param);

	/* vdec meta data -> fg hw reg setting */
	disp_fg_parser_lut(fg_param, params_hw_reg);
	disp_fg_parser_gns(fg_param, params_hw_reg);

	/* generate adl raw data */
	disp_fg_process_lut(params_hw_reg, params_hw_adl);
	disp_fg_process_gns(params_hw_reg, params_hw_adl);
}

int disp_fg_handler(u32 fg_hw_id,
		    struct mtk_av1_film_grain_params *fg_param,
		    struct mtk_fg_vsc_info *vsc_info,
		    struct adl_src_tbl *adl_tbl)
{
	struct mtk_av1_film_grain_params *param = NULL;
	struct disp_fg_info *info = NULL;
	struct mtk_fg_params *sets_buf = NULL;
	MS_U8 *fg_idx = NULL;

	if (!fg_param || !fg_param->apply_grain)
		return FG_FAIL;

	FG_FUNC();
	sets_buf = disp_fg_get_sets_buf(fg_hw_id);
	if (!sets_buf)
		return FG_FAIL;

	info = &g_fg_info[fg_hw_id];
	fg_idx = &info->fg_idx;
	/* if update_grain == 0, use previous params */
	param = &sets_buf->sets[0].av1_para;
	if (fg_param->update_grain) {
		/* mark the set is selected */
		sets_buf->select_set = true;
		*fg_idx = (*fg_idx + 1) % FG_MAX_PARAM_NS;
		memcpy(param, fg_param,
		       sizeof(struct mtk_av1_film_grain_params));
	} else {
		param->grain_seed = fg_param->grain_seed;
		if (!sets_buf->select_set)
			return FG_FAIL;
	}

	info->hw_reg = &info->fg_params_hw_reg[*fg_idx];
	info->hw_adl = &info->fg_params_hw_adl[*fg_idx];
	info->hw_reg->bit_depth = vsc_info->bit_depth;
	disp_fg_parser_meta(param, info->hw_reg, info->hw_adl);

	if (fg_sw_auto_reg_filter && info->gns_ar_info.ar_block_alloc)
		disp_fg_sw_auto_reg(param, info);
	else if (vsc_info->bit_depth == FG_MINI_BITS)
		disp_fg_regenerate_8b_gns(param, info);
	else
		disp_fg_hw_auto_reg(param, info);

	fg_hal_update_process(fg_hw_id, info->hw_reg, info->hw_adl, adl_tbl);

	return FG_OK;
}
