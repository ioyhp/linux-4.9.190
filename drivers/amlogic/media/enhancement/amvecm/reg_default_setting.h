/*
 * drivers/amlogic/media/enhancement/amvecm/reg_default_setting.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __REG_DEFAULT_SETTING_H
#define __REG_DEFAULT_SETTING_H

#include <linux/amlogic/media/amvecm/cm.h>
#include "arch/vpp_regs.h"

struct ve_dnlp_curve_param_s dnlp_default = {
	/* ve_dnlp_scurv_low */
	{64, 64, 64, 64, 65, 77, 91, 103, 116, 130, 146, 162, 179, 196, 212,
	233, 253, 272, 295, 318, 340, 363, 386, 406, 427, 447, 468, 487, 504,
	523, 539, 555, 572, 587, 604, 619, 634, 648, 661, 675, 689, 703, 716,
	730, 744, 758, 772, 786, 800, 815, 829, 843, 857, 871, 885, 899, 912,
	924, 935, 940, 940, 940, 940, 940, 940},
	/* ve_dnlp_scurv_mid1 */
	{64, 64, 64, 64, 64, 74, 83, 95, 105, 117, 131, 145, 159, 173, 189,
	206, 223, 243, 263, 282, 306, 333, 361, 389, 409, 429, 447, 467, 484,
	504, 521, 537, 554, 572, 587, 603, 617, 632, 646, 662, 676, 690, 704,
	717, 731, 745, 759, 774, 789, 804, 819, 833, 848, 863, 878, 893, 908,
	920, 931, 940, 940, 940, 940, 940, 940},
	/* ve_dnlp_scurv_mid2 */
	{64, 64, 64, 64, 64, 70, 78, 86, 93, 102, 111, 122, 133, 147, 163,
	181, 197, 215, 233, 256, 278, 299, 323, 346, 366, 388, 408, 430, 449,
	473, 493, 511, 530, 547, 563, 578, 594, 610, 629, 644, 663, 680, 696,
	712, 727, 742, 757, 772, 787, 803, 818, 833, 848, 863, 878, 892, 907,
	918, 930, 940, 940, 940, 940, 940, 940},
	/* ve_dnlp_scurv_hgh1 */
	{64, 64, 64, 64, 64, 68, 72, 78, 82, 89, 96, 104, 111, 123, 137, 152,
	170, 188, 206, 228, 249, 272, 296, 321, 342, 363, 384, 401, 422, 441,
	459, 477, 494, 511, 530, 548, 566, 584, 603, 625, 643, 660, 675, 689,
	706, 724, 739, 756, 771, 787, 804, 820, 835, 850, 865, 880, 895, 911,
	926, 942, 940, 940, 940, 940, 940},
	/* ve_dnlp_scurv_hgh2 */
	{64, 64, 64, 64, 64, 65, 67, 69, 72, 75, 80, 86, 93, 102, 114, 127,
	141, 156, 172, 189, 207, 226, 246, 267, 289, 311, 334, 355, 376, 397,
	416, 434, 453, 471, 490, 509, 530, 549, 569, 588, 607, 626, 644, 663,
	681, 699, 717, 735, 753, 770, 788, 805, 822, 840, 856, 873, 890, 907,
	923, 940, 940, 940, 940, 940, 940},
	/* ve_gain_var_lut49 */
	{10, 120, 177, 208, 228, 242, 255, 265, 276, 284, 292, 297, 300, 300,
	300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300,
	300, 300, 300, 300, 298, 292, 284, 276, 268, 256, 240, 212, 168, 113,
	98, 98, 98, 98, 98, 98, 98},
	/* ve_wext_gain */
	{256, 256, 256, 256, 256, 256, 256, 256, 248, 240, 232, 224, 216, 208,
	200, 192, 184, 176, 168, 160, 152, 144, 136, 128, 124, 120, 116, 112,
	108, 104, 100, 96, 92, 88, 84, 80, 76, 72, 68, 64, 60, 56, 52, 48,
	44, 40, 36, 32},
	/* ve_adp_thrd */
	{14, 19, 22, 24, 25, 25, 25, 24, 22, 18, 16, 15, 15, 15, 15, 15,
	15, 14, 14, 14, 14, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 4},
	/* ve_reg_blk_boost_12 */
	{0, 0, 7, 21, 41, 24, 15, 5, 4, 3, 2, 0, 0},
	/* ve_reg_blk_boost_20 */
	{0, 0, 1, 2, 3, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1, 0},
	/* ve_reg_mono_protect */
	{307, 332, 358, 371, 460, 560},
	/* ve_reg_trend_wht_expand_lut8 */
	{0, 2, 4, 6, 5, 4, 3, 2, 0},
	/* param */
	{1, 0, 2, 0, 0, 5, 5, 255, 5, 15, 1, 16, 128, 1, 0, 8, 45, 16, 32,
	0, 0, 0, 18, 18, 0, 0, 0, 4, 4, 8, 4, 16, 16, 16, 32, 35, 0, 0, 2,
	2, 32, 32, 10, 4, 59, 0, 8, 32, 20, 19, 2, 20, 80, 120, 160, 200,
	0, 1, 1, 10, 48, 1, 0, 1, 2, 614, 400, 600, 256, 128, 250, 2, 2,
	8, 15, 0, 0},
};

static struct am_regs_s cm_default_legacy = {
	11,
	{
	{REG_TYPE_INDEX_VPPCHROMA,	0x200,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x201,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x202,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x203,    0xffffffff, 0x8000400},
	{REG_TYPE_INDEX_VPPCHROMA,	0x204,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x206,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x207,    0xffffffff, 0x2000000},
	{REG_TYPE_INDEX_VPPCHROMA,	0x208,    0xffffffff, 0x76   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x209,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x20a,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x20f,    0xf, 0x0,   },
	}
};

static struct am_regs_s cm_default = {
	11,
	{
	{REG_TYPE_INDEX_VPPCHROMA,	0x200,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x201,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x202,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x203,    0xffffffff, 0x8000400},
	{REG_TYPE_INDEX_VPPCHROMA,	0x204,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x206,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x207,    0xffffffff, 0x2000000},
	{REG_TYPE_INDEX_VPPCHROMA,	0x208,    0xffffffff, 0x1d   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x209,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x20a,    0xffffffff, 0x0   },
	{REG_TYPE_INDEX_VPPCHROMA,	0x20f,    0xf, 0x0,   },
	}
};

/*sr0 sharpness reg*/
struct am_regs_s sr0_default = {
	94,
	{
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SHARP_HVSIZE, 0xffffffff, 0x02d00240},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SHARP_HVBLANK_NUM,
		0xffffffff, 0x00082060},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_NR_GAUSSIAN_MODE, 0xffffffff, 0x00000010},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_CON_2CIRHPGAIN_TH_RATE,
		0xffffffff, 0x14323218},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_CON_2CIRHPGAIN_LIMIT,
		0xffffffff, 0x50845e00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_CON_2CIRBPGAIN_TH_RATE,
		0xffffffff, 0x14323218},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_CON_2CIRBPGAIN_LIMIT,
		0xffffffff, 0x508d5000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_CON_2DRTHPGAIN_TH_RATE,
		0xffffffff, 0x14323218},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_CON_2DRTHPGAIN_LIMIT,
		0xffffffff, 0x3d3d1f00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_CON_2DRTBPGAIN_TH_RATE,
		0xffffffff, 0x14323218},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_CON_2DRTBPGAIN_LIMIT,
		0xffffffff, 0x38390c00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_CIRFB_LPF_MODE, 0xffffffff, 0x22202220},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_DRTFB_LPF_MODE, 0xffffffff, 0x22202220},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_CIRFB_HP_CORING, 0xffffffff, 0x00020202},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_CIRFB_BP_CORING, 0xffffffff, 0x00020202},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_DRTFB_HP_CORING, 0xffffffff, 0x00020202},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_DRTFB_BP_CORING, 0xffffffff, 0x00020202},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_CIRFB_BLEND_GAIN, 0xffffffff, 0x38402840},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_NR_ALPY_SSD_GAIN_OFST,
		0xffffffff, 0x0000103e},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_NR_ALP0Y_ERR2CURV_TH_RATE,
		0xffffffff, 0x0a195040},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_NR_ALP0Y_ERR2CURV_LIMIT,
		0xffffffff, 0x3f003f00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_NR_ALP0C_ERR2CURV_TH_RATE,
		0xffffffff, 0x0a195040},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_NR_ALP0C_ERR2CURV_LIMIT,
		0xffffffff, 0x3f003f00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_NR_ALP0_MIN_MAX, 0xffffffff, 0x003f003f},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_NR_ALP1_MIERR_CORING,
		0xffffffff, 0x00000003},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_NR_ALP1_ERR2CURV_TH_RATE,
		0xffffffff, 0x00180014},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_NR_ALP1_ERR2CURV_LIMIT,
		0xffffffff, 0x00103f00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_NR_ALP1_MIN_MAX, 0xffffffff, 0x003f003f},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_ALP2_MIERR_CORING,
		0xffffffff, 0x00010001},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_ALP2_ERR2CURV_TH_RATE,
		0xffffffff, 0x0018000a},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_ALP2_ERR2CURV_LIMIT,
		0xffffffff, 0x00402000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_ALP2_MIN_MAX, 0xffffffff, 0x0000003f},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_FINALGAIN_HP_BP,
		0xffffffff, 0x00001414},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_OS_HORZ_CORE_GAIN,
		0xffffffff, 0x08140214},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_OS_VERT_CORE_GAIN,
		0xffffffff, 0x08140214},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_OS_ADPT_MISC,
		0xffffffff, 0x2806c814},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_OS_STATIC, 0xffffffff, 0x2203f03f},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_NR_ENABLE, 0xffffffff, 0x00000000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PK_DRT_SAD_MISC, 0xffffffff, 0x12120018},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_NR_TI_DNLP_BLEND, 0xffffffff, 0x00000407},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_TI_DIR_CORE_ALPHA, 0xffffffff, 0x0a00003f},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_CTI_DIR_ALPHA, 0xffffffff, 0x0400003f},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_LTI_CTI_DF_GAIN, 0xffffffff, 0x0c0c0c0c},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_LTI_CTI_DIR_AC_DBG, 0xffffffff, 0x56ee0000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_HCTI_FLT_CLP_DC, 0xffffffff, 0x05555300},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_HCTI_BST_GAIN, 0xffffffff, 0x050a0a00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_HCTI_BST_CORE, 0xffffffff, 0x03030303},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_HCTI_CON_2_GAIN_0, 0xffffffff, 0x24193c05},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_HCTI_CON_2_GAIN_1, 0xffffffff, 0x4b055014},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_HCTI_OS_MARGIN, 0xffffffff, 0x00000000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_HLTI_FLT_CLP_DC, 0xffffffff, 0x00152100},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_HLTI_BST_GAIN, 0xffffffff, 0x06060600},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_HLTI_BST_CORE, 0xffffffff, 0x03030303},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_HLTI_CON_2_GAIN_0, 0xffffffff, 0x24193c05},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_HLTI_CON_2_GAIN_1, 0xffffffff, 0x66635e24},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_HLTI_OS_MARGIN, 0xffffffff, 0x00000000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_VLTI_FLT_CON_CLP, 0xffffffff, 0x00002a94},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_VLTI_BST_GAIN, 0xffffffff, 0x00202020},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_VLTI_BST_CORE, 0xffffffff, 0x00050503},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_VLTI_CON_2_GAIN_0, 0xffffffff, 0x193c0560},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_VLTI_CON_2_GAIN_1, 0xffffffff, 0x5f501400},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_VCTI_FLT_CON_CLP, 0xffffffff, 0x00002a94},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_VCTI_BST_GAIN, 0xffffffff, 0x00101010},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_VCTI_BST_CORE, 0xffffffff, 0x00050503},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_VCTI_CON_2_GAIN_0, 0xffffffff, 0x193c0560},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_VCTI_CON_2_GAIN_1, 0xffffffff, 0x5f501400},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SHARP_3DLIMIT, 0xffffffff, 0x03c0021c},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_SAD_CTRL, 0xffffffff, 0x060606ff},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_PK_CTRL0, 0xffffffff, 0x00000ffc},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_PK_CTRL1, 0xffffffff, 0x112020cc},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DRTLPF_EN, 0xffffffff, 0x00000000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DRTLPF_ALPHA_0,
		0xffffffff, 0x0405050c},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DRTLPF_ALPHA_1,
		0xffffffff, 0x01040708},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DRTLPF_ALPHA_2,
		0xffffffff, 0x00000000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DRTLPF_ALPHA_OFST,
		0xffffffff, 0x000e000e},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DERING_CTRL, //80
		0xffffffff, 0x0392281c},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DERING_LUMA2PKGAIN_0TO3,
		0xffffffff, 0xffffc81e},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DERING_LUMA2PKGAIN_4TO6,
		0xffffffff, 0x001832ff},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DERING_LUMA2PKOS_0TO3,
		0xffffffff, 0xffffc81e},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DERING_LUMA2PKOS_4TO6,
		0xffffffff, 0x001832ff},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DERING_GAINVS_MADSAD,
		0xffffffff, 0x00000048},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DERING_GAINVS_VR2MAX,
		0xffffffff, 0xffffec20},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DERING_PARAM0,
		0xffffffff, 0x000a2010},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_SR3_DRTLPF_THETA,
		0xffffffff, 0xfec96420},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_DB_FLT_CTRL, 0xffffffff, 0x000222ca},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_DB_FLT_YC_THRD,
		0xffffffff, 0x97659765},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_DB_FLT_RANDLUT,
		0xffffffff, 0x00249249},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_DB_FLT_PXI_THRD,
		0xffffffff, 0x00000000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_DB_FLT_SEED_Y, 0xffffffff, 0x60a52f20},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_DB_FLT_SEED_U, 0xffffffff, 0x60a52f27},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_DB_FLT_SEED_V, 0xffffffff, 0x60a52f22},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PKGAIN_VSLUMA_LUT_L,
		0xffffffff, 0x56667ac8},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP0_PKGAIN_VSLUMA_LUT_H,
		0xffffffff, 0x00000004},
	{0}
	}
};

/*sr1 sharpness reg*/
struct am_regs_s sr1_default = {
	94,
	{
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SHARP_HVSIZE, 0xffffffff, 0x02d00240},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SHARP_HVBLANK_NUM,
		0xffffffff, 0x00082060},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_NR_GAUSSIAN_MODE, 0xffffffff, 0x00000010},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_CON_2CIRHPGAIN_TH_RATE,
		0xffffffff, 0x14323218},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_CON_2CIRHPGAIN_LIMIT,
		0xffffffff, 0x50845e00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_CON_2CIRBPGAIN_TH_RATE,
		0xffffffff, 0x14323218},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_CON_2CIRBPGAIN_LIMIT,
		0xffffffff, 0x508d5000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_CON_2DRTHPGAIN_TH_RATE,
		0xffffffff, 0x14323218},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_CON_2DRTHPGAIN_LIMIT,
		0xffffffff, 0x3d3d1f00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_CON_2DRTBPGAIN_TH_RATE,
		0xffffffff, 0x14323218},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_CON_2DRTBPGAIN_LIMIT,
		0xffffffff, 0x38390c00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_CIRFB_LPF_MODE, 0xffffffff, 0x22202220},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_DRTFB_LPF_MODE, 0xffffffff, 0x22202220},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_CIRFB_HP_CORING, 0xffffffff, 0x00020202},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_CIRFB_BP_CORING, 0xffffffff, 0x00020202},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_DRTFB_HP_CORING, 0xffffffff, 0x00020202},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_DRTFB_BP_CORING, 0xffffffff, 0x00020202},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_CIRFB_BLEND_GAIN, 0xffffffff, 0x38402840},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_NR_ALPY_SSD_GAIN_OFST,
		0xffffffff, 0x0000103e},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_NR_ALP0Y_ERR2CURV_TH_RATE,
		0xffffffff, 0x0a195040},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_NR_ALP0Y_ERR2CURV_LIMIT,
		0xffffffff, 0x3f003f00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_NR_ALP0C_ERR2CURV_TH_RATE,
		0xffffffff, 0x0a195040},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_NR_ALP0C_ERR2CURV_LIMIT,
		0xffffffff, 0x3f003f00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_NR_ALP0_MIN_MAX, 0xffffffff, 0x003f003f},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_NR_ALP1_MIERR_CORING,
		0xffffffff, 0x00000003},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_NR_ALP1_ERR2CURV_TH_RATE,
		0xffffffff, 0x00180014},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_NR_ALP1_ERR2CURV_LIMIT,
		0xffffffff, 0x00103f00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_NR_ALP1_MIN_MAX, 0xffffffff, 0x003f003f},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_ALP2_MIERR_CORING,
		0xffffffff, 0x00010001},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_ALP2_ERR2CURV_TH_RATE,
		0xffffffff, 0x0018000a},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_ALP2_ERR2CURV_LIMIT,
		0xffffffff, 0x00402000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_ALP2_MIN_MAX, 0xffffffff, 0x0000003f},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_FINALGAIN_HP_BP,
		0xffffffff, 0x00001414},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_OS_HORZ_CORE_GAIN,
		0xffffffff, 0x08140214},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_OS_VERT_CORE_GAIN,
		0xffffffff, 0x08140214},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_OS_ADPT_MISC,
		0xffffffff, 0x2806c814},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_OS_STATIC, 0xffffffff, 0x2203f03f},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_NR_ENABLE, 0xffffffff, 0x00000000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PK_DRT_SAD_MISC, 0xffffffff, 0x12120018},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_NR_TI_DNLP_BLEND, 0xffffffff, 0x00000407},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_TI_DIR_CORE_ALPHA, 0xffffffff, 0x0a00003f},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_CTI_DIR_ALPHA, 0xffffffff, 0x0400003f},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_LTI_CTI_DF_GAIN, 0xffffffff, 0x0c0c0c0c},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_LTI_CTI_DIR_AC_DBG, 0xffffffff, 0x56ee0000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_HCTI_FLT_CLP_DC, 0xffffffff, 0x05555300},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_HCTI_BST_GAIN, 0xffffffff, 0x050a0a00},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_HCTI_BST_CORE, 0xffffffff, 0x03030303},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_HCTI_CON_2_GAIN_0, 0xffffffff, 0x24193c05},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_HCTI_CON_2_GAIN_1, 0xffffffff, 0x4b055014},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_HCTI_OS_MARGIN, 0xffffffff, 0x00000000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_HLTI_FLT_CLP_DC, 0xffffffff, 0x00152100},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_HLTI_BST_GAIN, 0xffffffff, 0x06060600},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_HLTI_BST_CORE, 0xffffffff, 0x03030303},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_HLTI_CON_2_GAIN_0, 0xffffffff, 0x24193c05},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_HLTI_CON_2_GAIN_1, 0xffffffff, 0x66635e24},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_HLTI_OS_MARGIN, 0xffffffff, 0x00000000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_VLTI_FLT_CON_CLP, 0xffffffff, 0x00002a94},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_VLTI_BST_GAIN, 0xffffffff, 0x00202020},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_VLTI_BST_CORE, 0xffffffff, 0x00050503},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_VLTI_CON_2_GAIN_0, 0xffffffff, 0x193c0560},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_VLTI_CON_2_GAIN_1, 0xffffffff, 0x5f501400},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_VCTI_FLT_CON_CLP, 0xffffffff, 0x00002a94},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_VCTI_BST_GAIN, 0xffffffff, 0x00101010},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_VCTI_BST_CORE, 0xffffffff, 0x00050503},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_VCTI_CON_2_GAIN_0, 0xffffffff, 0x193c0560},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_VCTI_CON_2_GAIN_1, 0xffffffff, 0x5f501400},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SHARP_3DLIMIT, 0xffffffff, 0x03c0021c},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_SAD_CTRL, 0xffffffff, 0x060606ff},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_PK_CTRL0, 0xffffffff, 0x00000ffc},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_PK_CTRL1, 0xffffffff, 0x112020cc},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DRTLPF_EN, 0xffffffff, 0x0000000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DRTLPF_ALPHA_0,
		0xffffffff, 0x0405050c},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DRTLPF_ALPHA_1,
		0xffffffff, 0x01040708},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DRTLPF_ALPHA_2,
		0xffffffff, 0x00000000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DRTLPF_ALPHA_OFST,
		0xffffffff, 0x000e000e},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DERING_CTRL,
		0xffffffff, 0x0392281c},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DERING_LUMA2PKGAIN_0TO3,
		0xffffffff, 0xffffc81e},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DERING_LUMA2PKGAIN_4TO6,
		0xffffffff, 0x001832ff},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DERING_LUMA2PKOS_0TO3,
		0xffffffff, 0xffffc81e},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DERING_LUMA2PKOS_4TO6,
		0xffffffff, 0x001832ff},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DERING_GAINVS_MADSAD,
		0xffffffff, 0x00000048},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DERING_GAINVS_VR2MAX,
		0xffffffff, 0xffffec20},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DERING_PARAM0,
		0xffffffff, 0x000a2010},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_SR3_DRTLPF_THETA,
		0xffffffff, 0xfec96420},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_DB_FLT_CTRL, 0xffffffff, 0x000222ca},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_DB_FLT_YC_THRD,
		0xffffffff, 0x97659765},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_DB_FLT_RANDLUT,
		0xffffffff, 0x00249249},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_DB_FLT_PXI_THRD,
		0xffffffff, 0x00000000},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_DB_FLT_SEED_Y, 0xffffffff, 0x60a52f20},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_DB_FLT_SEED_U, 0xffffffff, 0x60a52f27},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_DB_FLT_SEED_V, 0xffffffff, 0x60a52f22},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PKGAIN_VSLUMA_LUT_L,
		0xffffffff, 0x56667ac8},
	{REG_TYPE_OFFSET_VCBUS, SRSHARP1_PKGAIN_VSLUMA_LUT_H,
		0xffffffff, 0x00000004},
	{0}
	}
};

#endif

