/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#ifndef __DISP_FG_DEBUG_H__
#define __DISP_FG_DEBUG_H__

#include "disp_info.h"
#include <linux/kernel.h>

void fg_debug_init(void);
void fg_debug_exit(void);

#ifndef FG_STR_CONVERT
#define FG_STR_CONVERT(p, val, base, action)                 \
	do {                                                  \
		int ret = 0;                                      \
		const char *tmp;                                  \
		tmp = strsep(p, ",");                             \
		if (tmp == NULL)                                  \
			break;                                        \
		ret = kstrtouint(tmp, 0, (unsigned int *)val);\
		if (ret != 0) {                                   \
			FG_ERR(                                   \
			"[ERROR]kstrtouint return error: %d\n" \
				"  file : %s, line : %d\n", \
				ret, __FILE__, __LINE__); \
			action; \
			} \
	} while (0)
#endif

struct mtk_fg_debug_info {
	bool debug_en;
	bool dump_para;
	bool dump_meta;
	bool dump_table;
	/* for afgs1 max set is 8 (num 1~9) */
	u32 force_select;
	u32 reserved;
};

enum fg_hw_id;

extern void fg_sec_status(void);
extern void fg_hal_reg_dump(enum fg_hw_id hw_id, u32 len);
void fg_hal_reg_write(u32 addr, u32 val);
bool fg_dbg_dump_param_state(void);
void fg_dbg_dump_param(struct mtk_av1_film_grain_params *param);
int fg_dbg_get_select(void);
void fg_dbg_dump_meta(struct mtk_vdp_film_grain_md_t *fg_md);
bool fg_dbg_get_table(void);
#endif /* __DISP_VDP_DEBUG_H__ */
