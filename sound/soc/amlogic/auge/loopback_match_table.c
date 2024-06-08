/*
 * sound/soc/amlogic/auge/loopback_match_table.c
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
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

#include "loopback_hw.h"
#include "regs.h"

#define LOOPBACKA    0
#define LOOPBACKB    1

struct mux_conf lb_srcs_v1[] = {
	AUDIO_SRC_CONFIG("tdmin_a", 0, EE_AUDIO_LB_A_CTRL0, 0, 0x7),
	AUDIO_SRC_CONFIG("tdmin_b", 1, EE_AUDIO_LB_A_CTRL0, 0, 0x7),
	AUDIO_SRC_CONFIG("tdmin_c", 2, EE_AUDIO_LB_A_CTRL0, 0, 0x7),
	AUDIO_SRC_CONFIG("spdifin", 3, EE_AUDIO_LB_A_CTRL0, 0, 0x7),
	AUDIO_SRC_CONFIG("pdmin", 4, EE_AUDIO_LB_A_CTRL0, 0, 0x7),
	{ /* sentinel */ }
};

struct mux_conf lb_srcs_v2[] = {
	AUDIO_SRC_CONFIG("tdmin_a", 0, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("tdmin_b", 1, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("tdmin_c", 2, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("spdifin", 3, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("pdmin", 4, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("fratv", 5, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("tdmin_lb", 6, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("loopback_a", 7, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("frhdmirx", 8, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("loopback_b", 9, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("spdifin_lb", 10, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("earc_rx_dmac", 11, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("frhdmirx_pao", 12, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("resample_a", 13, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("resample_b", 14, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("vad", 15, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	{ /* sentinel */ }
};

struct mux_conf tdmin_lb_srcs_v1[] = {
	AUDIO_SRC_CONFIG("tdmout_a", 0, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmout_b", 1, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmout_c", 2, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmin_a", 3, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmin_b", 4, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmin_c", 5, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmind_a", 6, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmind_b", 7, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmind_c", 8, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("hdmirx", 9, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("acodec_adc", 10, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	{ /* sentinel */ }
};

struct mux_conf tdmin_lb_srcs_v2[] = {
	AUDIO_SRC_CONFIG("tdmin_a", 0, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmin_b", 1, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmin_c", 2, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("hdmirx", 4, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("acodec_adc", 5, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmout_a", 12, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmout_b", 13, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmout_c", 14, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	{ /* sentinel */ }
};

struct loopback_chipinfo {
	unsigned int id;

	/* reset all modules, after g12a
	 * it has reset bits control for modules
	 */
	bool is_reset_all;

	/* channel and mask switch to ctrl2, ctrl3 from tl1
	 * for datain, 0: channel and mask is controlled by ctrl0
	 *             1: channel and mask is controlled by ctrl2
	 * for datalb, 0: channel and mask is controlled by ctrl1
	 *             1: channel and mask is controlled by ctrl3
	 */
	bool ch_ctrl;

	/* EE_AUDIO_LB_A_CTRL0 bit 27, chnum_en
	 * from tm2 revb, no chnum_en
	 */
	bool chnum_en;

	/* srcs config to make reg compatible */
	struct mux_conf *srcs;
	struct mux_conf *tdmin_lb_srcs;
};

static struct loopback_chipinfo g12a_loopbacka_chipinfo = {
	.id      = LOOPBACKA,
	.chnum_en = true,
	.srcs     = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo tl1_loopbacka_chipinfo = {
	.id      = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = true,
	.srcs	  = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo tl1_loopbackb_chipinfo = {
	.id      = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = true,
	.srcs	  = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo sm1_loopbacka_chipinfo = {
	.id      = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = true,
	.srcs	  = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo sm1_loopbackb_chipinfo = {
	.id      = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = true,
	.srcs	  = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo tm2_loopbacka_chipinfo = {
	.id      = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = true,
	.srcs	  = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo tm2_loopbackb_chipinfo = {
	.id      = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = true,
	.srcs	  = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo tm2_revb_loopbacka_chipinfo = {
	.id      = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = false,
	.srcs	  = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo tm2_revb_loopbackb_chipinfo = {
	.id      = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = false,
	.srcs	  = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static const struct of_device_id loopback_device_id[] = {
	{
		.compatible = "amlogic, snd-loopback",
		.data		= &g12a_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, axg-loopback",
		.data       = &g12a_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-loopback",
		.data       = &g12a_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-loopbacka",
		.data		= &tl1_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-loopbackb",
		.data		= &tl1_loopbackb_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-loopbacka",
		.data		= &sm1_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-loopbackb",
		.data		= &sm1_loopbackb_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-loopbacka",
		.data		= &tm2_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-loopbackb",
		.data		= &tm2_loopbackb_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-loopbacka",
		.data		= &tm2_revb_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-loopbackb",
		.data		= &tm2_revb_loopbackb_chipinfo,
	},
	{}
};
MODULE_DEVICE_TABLE(of, loopback_device_id);