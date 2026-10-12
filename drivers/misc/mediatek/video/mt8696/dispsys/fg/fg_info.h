/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __FG_INFO_H__
#define __FG_INFO_H__
#include "disp_info.h"

/* define error code for film grain */
#define FG_OK			0
#define FG_INVAL_POINT		-1
#define FG_NOT_ENABLE		-2
#define FG_INVAL_PRE_SCALE	-3
#define FG_YPOINT_EXCEED	-4
#define FG_YVALUE_EXCEED	-5
#define FG_CB_POINT_EXCEED	-6
#define FG_CB_VALUE_EXCEED	-7
#define FG_CR_POINT_EXCEED	-8
#define FG_CR_VALUE_EXCEED	-9
#define FG_METADATA_EXCEED	-10
#define FG_INVAL_BEST_SET	-11
#define FG_INVAL_PARAM		-12
#define FG_FAIL			-13

/* define parameters limits for film grain */
#define FG_MAX_SETS		8
#define FG_NUM_Y_POINTS_MAX		14
#define FG_NUM_C_POINTS_MAX		10

#define FG_DEFAULT_LOG		(1 << 0)
#define FG_FUNC_LOG		(1 << 1)
#define FG_DEBUG_LOG		(1 << 2)
#define FG_SW_FILTER_LOG	(1 << 3)
#define FG_SW_PERFORMANCE_LOG	(1 << 4)

extern u32 fg_dbg_level;

#define fg_printf(level, string, args...)			\
do {								\
	if (fg_dbg_level & (level)) {				\
		pr_info(string, ##args);			\
	}							\
} while (0)

#define FG_LOG_I(string, args...) fg_printf(FG_DEFAULT_LOG,	\
	"[FG] %s | %d "string, \
	__func__, __LINE__, ##args)

#define FG_LOG_D(string, args...) fg_printf(FG_DEBUG_LOG,	\
	"[FG] %s | %d "string, \
	__func__, __LINE__, ##args)

#define FG_FUNC() fg_printf(FG_FUNC_LOG, "[FG] %s | %d\n",	\
	__func__, __LINE__)

#define FG_ERR(string, args...) pr_info("[FG] %s | %d error: "string,	\
	__func__, __LINE__, ##args)

#define FG_SW_FILTER(string, args...) \
	fg_printf(FG_SW_FILTER_LOG, "[FG] "string, ##args)

#define FG_SW_PER(string, args...) \
	fg_printf(FG_SW_PERFORMANCE_LOG, "[FG] "string, ##args)

struct mtk_fg_vsc_info {
	/**
	 * Color format of the frame buffer
	 */
	u32 format;

	/**
	 * Frame buffer crop range
	 */
	int crop_w, crop_h, pitch;

	/**
	 * Frame buffer display range
	 */
	int width, height;

	/**
	 * Bit depth of the display data
	 */
	int bit_depth;
};

struct mtk_fg_param_set {

	/**
	* Intended display resolution.
	*/
	int width, height;

	/**
	* Intended subsampling ratio, or 0 for luma-only streams.
	*/
	int subsampling_x, subsampling_y;

	/**
	* Intended bit depth, or 0 for unknown/unspecified.
	*/
	int bit_depth;

	/**
	* reserved for future use color_range primaries trc space.
	*/
	int reserved[4];

	/**
	* Film grain parameters that follow this entry.
	*/
	struct mtk_av1_film_grain_params av1_para;
};

struct mtk_fg_params {
	int afgs_enable;
	int num_sets;
	/* the range will be 1~9 */
	int select_set;
	bool is_match;
	int has_ref;
	struct mtk_fg_param_set sets[8];
};

#endif /* __FG_INFO_H__ */
