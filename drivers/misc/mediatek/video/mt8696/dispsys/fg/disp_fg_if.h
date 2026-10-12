/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __DISP_FG_IF_H__
#define __DISP_FG_IF_H__

#define FG_DRV_NAME "disp_drv_fg"
#define DISP_FG_SOF_NR		5

struct video_buffer_info;
struct disp_fg_sof {
	u32 sof_start;
	u32 sof_end;
};

void disp_fg_force_bypass(u32 fg_hw_id, bool bypass);
void disp_fg_config(u32 fg_hw_id, struct video_buffer_info *buf,
		    struct mtk_disp_vdp_cap *vdp_cap);
int disp_fg_set_dbg_level_enable(uint32_t level);

extern int fg_sec_init(void);
#endif
