/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __DISP_FG_DRV_V2_H__
#define __DISP_FG_DRV_V2_H__

#include "fg_hal.h"
#define UINT8_MAX		0xFF

bool disp_fg_support_new(struct mtk_vdp_film_grain_md_t *fg_md);
struct mtk_fg_param_set *disp_fg_get_best_param(struct mtk_fg_params *paras);
int disp_fg_handler_v2(u32 hw_id,
		       struct mtk_vdp_film_grain_md_t *fg_md,
		       struct mtk_fg_vsc_info *vsc_info,
		       struct adl_src_tbl *adl_tbl);

#endif /* __DISP_FG_DRV_V2_H__ */
