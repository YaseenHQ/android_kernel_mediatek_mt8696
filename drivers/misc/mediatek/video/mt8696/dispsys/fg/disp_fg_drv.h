/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#ifndef __DISP_FG_DRV_H__
#define __DISP_FG_DRV_H__

#include "fg_hal.h"
#include "disp_fg_pre_drv.h"

struct mtk_fg_vsc_info;

int disp_fg_handler(u32 fg_hw_id,
		    struct mtk_av1_film_grain_params *fg_param,
		    struct mtk_fg_vsc_info *vsc_info,
		    struct adl_src_tbl *adl_tbl);

#endif
