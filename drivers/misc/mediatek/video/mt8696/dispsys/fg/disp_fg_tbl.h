/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __DISP_FG_TBL_H__
#define __DISP_FG_TBL_H__

#include "linux/types.h"

#define GAUSSIAN_SEQUENCE_SIZE		2048
#define GAUSS_BITS			11
#define GAUSS_VALUE	(gaussian_sequence[get_random_number(GAUSS_BITS)])

#define CB_SEED_MASK			0xB524
#define CR_SEED_MASK			0x49D8

extern const int gaussian_sequence[];

void init_random_register(u16 seed);
int get_random_number(int bits);
int round2(int x, u8 n);

#endif /* __DISP_FG_TBL_H__ */
