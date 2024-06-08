/*
 * sound/soc/amlogic/auge/earc.c
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
 * Audio External Input/Out drirver
 * such as fratv, frhdmirx
 */
#define DEBUG

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/extcon.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <linux/workqueue.h>
#include <linux/amlogic/iomap.h>

#include <linux/amlogic/media/sound/hdmi_earc.h>
#include <linux/amlogic/media/sound/mixer.h>
#include <linux/amlogic/media/sound/debug.h>
#include "ddr_mngr.h"
#include "earc_hw.h"
#include "audio_utils.h"
#include "earc.h"
#include "frhdmirx_hw.h"
#include "sharebuffer.h"
#include "spdif_hw.h"

#define DRV_NAME "EARC"

#define EARCRX_DEFAULT_LATENCY 100

enum work_event {
	EVENT_NONE,
	EVENT_RX_ANA_AUTO_CAL = 0x1 << 0,
	EVENT_TX_ANA_AUTO_CAL = 0x1 << 1,
};

struct earc_chipinfo {
	unsigned int earc_spdifout_lane_mask;
	bool rx_dmac_sync_int;
};

struct earc {
	struct aml_audio_controller *actrl;
	struct device *dev;

	struct clk *clk_rx_gate;
	struct clk *clk_rx_cmdc;
	struct clk *clk_rx_dmac;
	struct clk *clk_rx_cmdc_srcpll;
	struct clk *clk_rx_dmac_srcpll;
	struct clk *clk_tx_gate;
	struct clk *clk_tx_cmdc;
	struct clk *clk_tx_dmac;
	struct clk *clk_tx_cmdc_srcpll;
	struct clk *clk_tx_dmac_srcpll;

	struct regmap *tx_cmdc_map;
	struct regmap *tx_dmac_map;
	struct regmap *tx_top_map;
	struct regmap *rx_cmdc_map;
	struct regmap *rx_dmac_map;
	struct regmap *rx_top_map;

	struct toddr *tddr;
	struct frddr *fddr;

	int irq_earc_rx;
	int irq_earc_tx;

	/* external connect */
	struct extcon_dev *rx_edev;
	struct extcon_dev *tx_edev;

	/* audio codec type for tx */
	enum audio_coding_types tx_audio_coding_type;

	/* freq for tx dmac clk */
	int tx_dmac_freq;

	/* whether dmac clock is on */
	bool rx_dmac_clk_on;
	spinlock_t rx_lock;  /* rx dmac clk lock */
	bool tx_dmac_clk_on;

	/* do analog auto calibration when bootup */
	struct work_struct work;
	enum work_event event;
	bool rx_bootup_auto_cal;
	bool tx_bootup_auto_cal;

	struct earc_chipinfo *chipinfo;

	/* mute in channel status */
	bool rx_cs_mute;
	bool tx_cs_mute;

	/* Channel Allocation */
	unsigned int tx_cs_lpcm_ca;

	/* channel map */
	struct snd_pcm_chmap *rx_chmap;
	struct snd_pcm_chmap *tx_chmap;

	struct snd_pcm_substream *substreams[2];

	struct work_struct rx_dmac_int_work;
	u8 cds_data[CDS_MAX_BYTES];
	enum sharebuffer_srcs samesource_sel;
};

static struct earc *s_earc;

bool is_earc_spdif(void)
{
	return !!s_earc;
}

#define EARC_BUFFER_BYTES (512 * 1024)
#define EARC_RATES      (SNDRV_PCM_RATE_8000_192000)
#define EARC_FORMATS    (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S24_LE |\
			SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_pcm_hardware earc_hardware = {
	.info =
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_PAUSE,

	.formats = EARC_FORMATS,

	.period_bytes_min = 64,
	.period_bytes_max = 128 * 1024,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = EARC_BUFFER_BYTES,

	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = 32,
};

bool aml_get_earctx_enable(void)
{
	if (s_earc)
	return get_earctx_enable(s_earc->tx_cmdc_map, s_earc->tx_dmac_map);
	return false;
}

static irqreturn_t earc_ddr_isr(int irq, void *data)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)data;

	if (!snd_pcm_running(substream))
		return IRQ_HANDLED;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

static void earcrx_update_attend_event(struct earc *p_earc,
				       bool is_earc, bool state)
{
	if (state) {
		if (is_earc) {
			extcon_set_state_sync(p_earc->rx_edev,
				EXTCON_EARCRX_ATNDTYP_ARC, false);
			extcon_set_state_sync(p_earc->rx_edev,
				EXTCON_EARCRX_ATNDTYP_EARC, state);
		} else {
			extcon_set_state_sync(p_earc->rx_edev,
				EXTCON_EARCRX_ATNDTYP_ARC, state);
			extcon_set_state_sync(p_earc->rx_edev,
				EXTCON_EARCRX_ATNDTYP_EARC, false);
		}
	} else {
		extcon_set_state_sync(p_earc->rx_edev,
			EXTCON_EARCRX_ATNDTYP_ARC, state);
		extcon_set_state_sync(p_earc->rx_edev,
			EXTCON_EARCRX_ATNDTYP_EARC, state);
	}
}

static void earcrx_pll_reset(struct earc *p_earc)
{
	int i = 0, count = 0;

	earcrx_dmac_sync_int_enable(p_earc->rx_top_map, 0);
	earcrx_pll_refresh(p_earc->rx_top_map,
			   RST_BY_SELF,
			   true);
	/* wait pll valid */
	usleep_range(350, 400);
	/* bit 31 is 1, and bit 30 is 0, then need reset pll */
	while (earxrx_get_pll_valid(p_earc->rx_top_map) &&
	       !earxrx_get_pll_valid_auto(
		       p_earc->rx_top_map)) {
		earcrx_pll_refresh(p_earc->rx_top_map,
				   RST_BY_SELF,
				   true);
		pr_info("refresh earcrx pll, i %d\n", i++);
		if (i >= 9) {
			dev_info(p_earc->dev,
				 "refresh earcrx pll failed\n");
			break;
		}
		usleep_range(350, 400);
	}

	/* there are 5 times continues bit 31 and bit 30 is 1 */
	i = 0;
	while (true) {
		if (earxrx_get_pll_valid(p_earc->rx_top_map) &&
		    earxrx_get_pll_valid_auto(p_earc->rx_top_map)) {
			i++;
		} else {
			i = 0;
			count++;
		}
		if (i >= 5 || count >= 2)
			break;
		usleep_range(45, 50);
	}

	i = 0;
	while (earcrx_dmac_get_irqs(p_earc->rx_top_map) & 0x10000000) {
		earcrx_dmac_sync_clr_irqs(p_earc->rx_top_map);
		i++;
		if (i > 5)
			break;
		usleep_range(45, 50);
	}

	earcrx_dmac_sync_int_enable(p_earc->rx_top_map, 1);
	pr_info("earcrx pll %d, auto %d, pengding is 0x%x\n",
		earxrx_get_pll_valid(p_earc->rx_top_map),
		earxrx_get_pll_valid_auto(p_earc->rx_top_map),
		earcrx_dmac_get_irqs(p_earc->rx_top_map));
}

static void valid_auto_work_func(struct work_struct *p_work)
{
	struct earc *p_earc = container_of(p_work,
			      struct earc, rx_dmac_int_work);

	earcrx_pll_reset(p_earc);
}

static irqreturn_t earc_rx_isr(int irq, void *data)
{
	struct earc *p_earc = (struct earc *)data;
	unsigned int status0 = earcrx_cdmc_get_irqs(p_earc->rx_top_map);

	if (status0)
		earcrx_cdmc_clr_irqs(p_earc->rx_top_map, status0);

	if (status0 & INT_EARCRX_CMDC_TIMEOUT) {
		earcrx_update_attend_event(p_earc,
					   false, false);

		aud_dbg(p_earc->dev, "EARCRX_CMDC_TIMEOUT\n");
	}

	if (status0 & INT_EARCRX_CMDC_IDLE2) {
		earcrx_update_attend_event(p_earc,
					   false, true);

		aud_dbg(p_earc->dev, "EARCRX_CMDC_IDLE2\n");
	}
	if (status0 & INT_EARCRX_CMDC_IDLE1) {
		earcrx_update_attend_event(p_earc,
					   false, false);

		aud_dbg(p_earc->dev, "EARCRX_CMDC_IDLE1\n");
	}
	if (status0 & INT_EARCRX_CMDC_DISC2)
		aud_dbg(p_earc->dev, "EARCRX_CMDC_DISC2\n");
	if (status0 & INT_EARCRX_CMDC_DISC1)
		aud_dbg(p_earc->dev, "EARCRX_CMDC_DISC1\n");
	if (status0 & INT_EARCRX_CMDC_EARC) {
		u8 latency = EARCRX_DEFAULT_LATENCY;

		earcrx_cmdc_set_latency(p_earc->rx_cmdc_map, &latency);
		earcrx_cmdc_set_cds(p_earc->rx_cmdc_map, p_earc->cds_data);
		earcrx_update_attend_event(p_earc,
					   true, true);

		aud_dbg(p_earc->dev, "EARCRX_CMDC_EARC\n");
	}

	if (status0 & INT_EARCRX_CMDC_LOSTHB)
		aud_dbg(p_earc->dev, "EARCRX_CMDC_LOSTHB\n");

	if (status0 & INT_EARCRX_CMDC_STATUS_CH)
		aud_dbg(p_earc->dev, "EARCRX_CMDC_STATUS_CH\n");

	if (p_earc->rx_dmac_clk_on) {
		unsigned int status1 = earcrx_dmac_get_irqs(p_earc->rx_top_map);
		unsigned int mask = earcrx_dmac_get_mask(p_earc->rx_top_map);

		status1 = status1 & mask;

		if (status1)
			earcrx_dmac_clr_irqs(p_earc->rx_top_map, status1);

		if (status1 & INT_EARCRX_ANA_RST_C_EARCRX_DIV2_HOLD_SET)
			aud_dbg(p_earc->dev, "EARCRX_ANA_RST_C_EARCRX_DIV2_HOLD_SET\n");

		if (status1 & INT_EARCRX_ANA_RST_C_NEW_FORMAT_SET) {
			aud_dbg(p_earc->dev, "EARCRX_ANA_RST_C_NEW_FORMAT_SET\n");

			earcrx_pll_refresh(p_earc->rx_top_map,
					   RST_BY_SELF, true);
		}

		if (status1 & INT_EARCRX_ERR_CORRECT_C_BCHERR_INT_SET)
			aud_dbg(p_earc->dev, "EARCRX_ERR_CORRECT_BCHERR\n");
		if (status1 & INT_ARCRX_BIPHASE_DECODE_R_PARITY_ERR)
			aud_dbg(p_earc->dev, "ARCRX_R_PARITY_ERR\n");

		if (status0 & INT_EARCRX_CMDC_HB_STATUS)
			aud_dbg(p_earc->dev, "EARCRX_CMDC_HB_STATUS\n");
		if (status1 & INT_ARCRX_BIPHASE_DECODE_C_FIND_PAPB)
			aud_dbg(p_earc->dev, "ARCRX_C_FIND_PAPB\n");
		if (status1 & INT_ARCRX_BIPHASE_DECODE_C_VALID_CHANGE)
			aud_dbg(p_earc->dev, "ARCRX_C_VALID_CHANGE\n");
		if (status1 & INT_ARCRX_BIPHASE_DECODE_C_FIND_NONPCM2PCM)
			aud_dbg(p_earc->dev, "ARCRX_C_FIND_NONPCM2PCM\n");
		if (status1 & INT_ARCRX_BIPHASE_DECODE_C_PCPD_CHANGE)
			aud_dbg(p_earc->dev, "ARCRX_C_PCPD_CHANGE\n");
		if (status1 & INT_ARCRX_BIPHASE_DECODE_C_CH_STATUS_CHANGE) {
			int mute = earcrx_get_cs_mute(p_earc->rx_dmac_map);

			if (p_earc->rx_cs_mute != mute) {
				p_earc->rx_cs_mute = mute;
				earcrx_pll_refresh(p_earc->rx_top_map,
						   RST_BY_SELF, true);
			}
		}
		if (status1 & INT_ARCRX_BIPHASE_DECODE_I_SAMPLE_MODE_CHANGE)
			aud_dbg(p_earc->dev, "ARCRX_I_SAMPLE_MODE_CHANGE\n");
		if (p_earc->chipinfo->rx_dmac_sync_int &&
		    status1 & INT_EARCRX_DMAC_VALID_AUTO_NEG_INT_SET) {
			earcrx_dmac_sync_int_enable(p_earc->rx_top_map, 0);
			schedule_work(&p_earc->rx_dmac_int_work);
			pr_info("%s EARCRX_DMAC_VALID_AUTO_NEG_INT_SET\n",
				__func__);
		}
	}

	return IRQ_HANDLED;
}

static void earctx_update_attend_event(struct earc *p_earc,
				       bool is_earc, bool state)
{
	if (state) {
		if (is_earc) {
			extcon_set_state_sync(p_earc->tx_edev,
					      EXTCON_EARCTX_ATNDTYP_ARC,
					      false);
			extcon_set_state_sync(p_earc->tx_edev,
					      EXTCON_EARCTX_ATNDTYP_EARC,
					      state);
		} else {
			extcon_set_state_sync(p_earc->tx_edev,
					      EXTCON_EARCTX_ATNDTYP_ARC,
					      state);
			extcon_set_state_sync(p_earc->tx_edev,
					      EXTCON_EARCTX_ATNDTYP_EARC,
					      false);
		}
	} else {
		extcon_set_state_sync(p_earc->tx_edev,
				      EXTCON_EARCTX_ATNDTYP_ARC,
				      state);
		extcon_set_state_sync(p_earc->tx_edev,
				      EXTCON_EARCTX_ATNDTYP_EARC,
				      state);
	}
}

static irqreturn_t earc_tx_isr(int irq, void *data)
{
	struct earc *p_earc = (struct earc *)data;
	unsigned int status0 = earctx_cdmc_get_irqs(p_earc->tx_top_map);
	struct snd_pcm_substream *substream =
		p_earc->substreams[SNDRV_PCM_STREAM_PLAYBACK];
	unsigned long flags;

	if (status0)
		earctx_cdmc_clr_irqs(p_earc->tx_top_map, status0);

	if (substream) {
		snd_pcm_stream_lock_irqsave(substream, flags);
		if (snd_pcm_running(substream) &&
		    (earctx_cmdc_get_attended_type(p_earc->tx_cmdc_map)
			== ATNDTYP_DISCNCT)) {
			snd_pcm_stop(substream, SNDRV_PCM_STATE_XRUN);
			pr_info("snd_pcm_stop, eARC disconnect\n");
		}
		snd_pcm_stream_unlock_irqrestore(substream, flags);
	}

	if (status0 & INT_EARCTX_CMDC_EARC) {
		earctx_update_attend_event(p_earc,
					   true, true);
		aud_dbg(p_earc->dev, "EARCTX_CMDC_EARC\n");
	}

	if (status0 & INT_EARCTX_CMDC_DISC1)
		aud_dbg(p_earc->dev, "EARCTX_CMDC_DISC1\n");
	if (status0 & INT_EARCTX_CMDC_DISC2)
		aud_dbg(p_earc->dev, "INT_EARCTX_CMDC_DISC2\n");
	if (status0 & INT_EARCTX_CMDC_STATUS_CH)
		aud_dbg(p_earc->dev, "EARCTX_CMDC_STATUS_CH\n");
	if (status0 & INT_EARCTX_CMDC_HB_STATUS)
		aud_dbg(p_earc->dev, "EARCTX_CMDC_HB_STATUS\n");
	if (status0 & INT_EARCTX_CMDC_LOSTHB)
		aud_dbg(p_earc->dev, "EARCTX_CMDC_LOSTHB\n");
	if (status0 & INT_EARCTX_CMDC_TIMEOUT)
		aud_dbg(p_earc->dev, "EARCTX_CMDC_TIMEOUT\n");
	if (status0 & INT_EARCTX_CMDC_RECV_NACK)
		aud_dbg(p_earc->dev, "EARCTX_CMDC_RECV_NACK\n");
	if (status0 & INT_EARCTX_CMDC_RECV_NORSP)
		aud_dbg(p_earc->dev, "EARCTX_CMDC_RECV_NORSP\n");
	if (status0 & INT_EARCTX_CMDC_RECV_UNEXP)
		aud_dbg(p_earc->dev, "EARCTX_CMDC_RECV_UNEXP\n");

	if (status0 & INT_EARCTX_CMDC_IDLE1) {
		earctx_update_attend_event(p_earc,
					   false, false);

		aud_dbg(p_earc->dev, "EARCTX_CMDC_IDLE1\n");
	}
	if (status0 & INT_EARCTX_CMDC_IDLE2) {
		earctx_update_attend_event(p_earc,
					   false, true);

		aud_dbg(p_earc->dev, "EARCTX_CMDC_IDLE2\n");
	}

	if (p_earc->tx_dmac_clk_on) {
		unsigned int status1 = earctx_dmac_get_irqs(p_earc->tx_top_map);

		if (status1)
			earctx_dmac_clr_irqs(p_earc->tx_top_map, status1);

		if (status1 & INT_EARCTX_FEM_C_HOLD_CLR)
			aud_dbg(p_earc->dev, "EARCTX_FEM_C_HOLD_CLR\n");
		if (status1 & INT_EARCTX_FEM_C_HOLD_START)
			aud_dbg(p_earc->dev, "EARCTX_FEM_C_HOLD_START\n");
		if (status1 & INT_EARCTX_ERRCORR_C_FIFO_THD_LESS_PASS)
			aud_dbg(p_earc->dev, "EARCTX_ECCFIFO_OVERFLOW\n");
		if (status1 & INT_EARCTX_ERRCORR_C_FIFO_OVERFLOW)
			aud_dbg(p_earc->dev, "EARCTX_ECC_FIFO_OVERFLOW\n");
		if (status1 & INT_EARCTX_ERRCORR_C_FIFO_EMPTY)
			aud_dbg(p_earc->dev, "EARCTX_ECC_FIFO_EMPTY\n");
	}

	return IRQ_HANDLED;
}

static int earc_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct earc *p_earc;
	int ret = 0;

	p_earc = (struct earc *)dev_get_drvdata(dev);

	snd_soc_set_runtime_hwparams(substream, &earc_hardware);
	ret = snd_pcm_lib_preallocate_pages(substream, SNDRV_DMA_TYPE_DEV,
		dev, EARC_BUFFER_BYTES / 2, EARC_BUFFER_BYTES);
	if (ret)
		goto err_mem;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		p_earc->fddr = aml_audio_register_frddr(dev,
			p_earc->actrl,
			earc_ddr_isr, substream, false);
		if (p_earc->fddr == NULL) {
			ret = -ENXIO;
			dev_err(dev, "failed to claim frddr\n");
			goto err_ddr;
		}
	} else {
		p_earc->tddr = aml_audio_register_toddr(dev,
			p_earc->actrl,
			earc_ddr_isr, substream);
		if (p_earc->tddr == NULL) {
			ret = -ENXIO;
			dev_err(dev, "failed to claim toddr\n");
			goto err_ddr;
		}
	}

	runtime->private_data = p_earc;

	return 0;
err_ddr:
	snd_pcm_lib_preallocate_free(substream);
err_mem:
	return ret;
}

static int earc_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = runtime->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		aml_audio_unregister_frddr(p_earc->dev, substream);
	else
		aml_audio_unregister_toddr(p_earc->dev, substream);

	runtime->private_data = NULL;
	snd_pcm_lib_preallocate_free(substream);

	return 0;
}

static int earc_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
			params_buffer_bytes(hw_params));
}

static int earc_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_lib_free_pages(substream);

	return 0;
}

static int earc_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = runtime->private_data;
	unsigned int start_addr, end_addr, int_addr;
	unsigned int period, threshold;

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - FIFO_BURST;
	period	 = frames_to_bytes(runtime, runtime->period_size);
	int_addr = period / FIFO_BURST;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_earc->fddr;

		/*
		 * Contrast minimum of period and fifo depth,
		 * and set the value as half.
		 */
		threshold = min(period, fr->chipinfo->fifo_depth);
		threshold /= 2;
		/* Use all the fifo */
		aml_frddr_set_fifos(fr, fr->chipinfo->fifo_depth, threshold);

		aml_frddr_set_buf(fr, start_addr, end_addr);
		aml_frddr_set_intrpt(fr, int_addr);
	} else {
		struct toddr *to = p_earc->tddr;

		/*
		 * Contrast minimum of period and fifo depth,
		 * and set the value as half.
		 */
		threshold = min(period, to->chipinfo->fifo_depth);
		threshold /= 2;
		aml_toddr_set_fifos(to, threshold);

		aml_toddr_set_buf(to, start_addr, end_addr);
		aml_toddr_set_intrpt(to, int_addr);
	}

	return 0;
}

static snd_pcm_uframes_t earc_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = runtime->private_data;
	unsigned int addr, start_addr;
	snd_pcm_uframes_t frames;

	start_addr = runtime->dma_addr;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		addr = aml_frddr_get_position(p_earc->fddr);
	else
		addr = aml_toddr_get_position(p_earc->tddr);

	frames = bytes_to_frames(runtime, addr - start_addr);
	if (frames > runtime->buffer_size)
		frames = 0;

	return frames;
}

int earc_silence(struct snd_pcm_substream *substream, int channel,
		    snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	char *ppos;
	int n;

	n = frames_to_bytes(runtime, count);
	ppos = runtime->dma_area + frames_to_bytes(runtime, pos);
	memset(ppos, 0, n);

	return 0;
}

static int earc_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static struct snd_pcm_ops earc_ops = {
	.open      = earc_open,
	.close     = earc_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = earc_hw_params,
	.hw_free   = earc_hw_free,
	.prepare   = earc_prepare,
	.pointer   = earc_pointer,
	.silence   = earc_silence,
	.mmap      = earc_mmap,
};

static int earc_pcm_add_chmap_ctls(struct earc *p_earc, struct snd_pcm *pcm);
static int earc_new(struct snd_soc_pcm_runtime *rtd)
{
	struct device *dev = rtd->platform->dev;
	struct earc *p_earc = (struct earc *)dev_get_drvdata(dev);

	earc_pcm_add_chmap_ctls(p_earc, rtd->pcm);

	return 0;
}

struct snd_soc_platform_driver earc_platform = {
	.ops = &earc_ops,
	.pcm_new = earc_new,
};

static int earc_dai_prepare(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int bit_depth = snd_pcm_format_width(runtime->format);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_earc->fddr;
		enum frddr_dest dst = EARCTX_DMAC;
		unsigned int fifo_id;
		enum attend_type type =
			earctx_cmdc_get_attended_type(p_earc->tx_cmdc_map);
		struct iec_cnsmr_cs cs_info;

		if (type == ATNDTYP_DISCNCT) {
			dev_err(p_earc->dev,
				"Neither eARC_TX or ARC_TX is attended!\n");
			return -ENOTCONN;
		}

		aud_dbg(p_earc->dev,
			"%s connected, Expected frddr dst:%s\n",
			(type == ATNDTYP_ARC) ? "ARC" : "eARC",
			frddr_src_get_str(dst));
		fifo_id = aml_frddr_get_fifo_id(fr);

		aml_frddr_set_format(fr,
				     runtime->channels,
				     runtime->rate,
				     bit_depth - 1,
				     spdifout_get_frddr_type(bit_depth));
		aml_frddr_select_dst(fr, dst);

		earctx_dmac_init(p_earc->tx_top_map,
				 p_earc->tx_dmac_map,
				 p_earc->chipinfo->earc_spdifout_lane_mask);
		earctx_dmac_set_format(p_earc->tx_dmac_map,
				       fr->fifo_id,
				       bit_depth - 1,
				       spdifout_get_frddr_type(bit_depth));

		iec_get_cnsmr_cs_info(&cs_info,
				      p_earc->tx_audio_coding_type,
				      runtime->channels,
				      runtime->rate);
		earctx_set_cs_info(p_earc->tx_dmac_map,
				   p_earc->tx_audio_coding_type,
				   &cs_info,
				   &p_earc->tx_cs_lpcm_ca);

		earctx_set_cs_mute(p_earc->tx_dmac_map, p_earc->tx_cs_mute);
	} else {
		struct toddr *to = p_earc->tddr;
		unsigned int msb = 0, lsb = 0, toddr_type = 0;
		unsigned int src = EARCRX_DMAC;
		struct toddr_fmt fmt;
		enum attend_type type =
			earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);

		if (type == ATNDTYP_DISCNCT) {
			dev_err(p_earc->dev,
				"Neither eARC_RX or ARC_RX is attended!\n");
			return -ENOTCONN;
		}

		aud_dbg(p_earc->dev,
			"%s connected, Expected toddr src:%s\n",
			(type == ATNDTYP_ARC) ? "ARC" : "eARC",
			toddr_src_get_str(src));

		if (bit_depth == 32)
			toddr_type = 3;
		else if (bit_depth == 24)
			toddr_type = 4;
		else
			toddr_type = 0;

		msb = 28 - 1;
		if (bit_depth == 16)
			lsb = 28 - bit_depth;
		else
			lsb = 4;

		fmt.type      = toddr_type;
		fmt.msb       = msb;
		fmt.lsb       = lsb;
		fmt.endian    = 0;
		fmt.bit_depth = bit_depth;
		fmt.ch_num    = runtime->channels;
		fmt.rate      = runtime->rate;

		aml_toddr_select_src(to, src);
		aml_toddr_set_format(to, &fmt);

		earcrx_dmac_init(p_earc->rx_top_map,
				 p_earc->rx_dmac_map,
				 p_earc->chipinfo->rx_dmac_sync_int);
		earcrx_arc_init(p_earc->rx_dmac_map);
	}

	return 0;
}

static void earctx_update_clk(struct earc *p_earc,
			      unsigned int channels,
			      unsigned int rate)
{
	unsigned int multi = audio_multi_clk(p_earc->tx_audio_coding_type);
	unsigned int freq = rate * 128 * 5; /* 5, falling edge */

	aud_dbg(p_earc->dev, "set %dX normal dmac clk\n", multi);

	freq *= multi;
	if (freq == p_earc->tx_dmac_freq) {
		aud_dbg(p_earc->dev, "tx dmac clk, rate:%d, set freq: %d, get freq:%lu\n",
			rate,
			freq,
			clk_get_rate(p_earc->clk_tx_dmac));
		return;
	}

	p_earc->tx_dmac_freq = freq;
	clk_set_rate(p_earc->clk_tx_dmac_srcpll, freq);
	clk_set_rate(p_earc->clk_tx_dmac, freq);

	aud_dbg(p_earc->dev, "tx dmac clk, rate:%d, set freq: %d, get freq:%lu\n",
		rate,
		freq,
		clk_get_rate(p_earc->clk_tx_dmac));
}

int sharebuffer_earctx_prepare(struct snd_pcm_substream *substream,
	struct frddr *fr, enum aud_codec_types type)
{
	enum attend_type earc_type;
	struct snd_pcm_runtime *runtime = substream->runtime;
	int bit_depth = snd_pcm_format_width(runtime->format);
	struct iec_cnsmr_cs cs_info;
	int ret;

	if (!s_earc)
		return -ENOTCONN;

	earc_type = earctx_cmdc_get_attended_type(s_earc->tx_cmdc_map);
	if (earc_type == ATNDTYP_DISCNCT) {
		dev_err(s_earc->dev, "Neither eARC_TX or ARC_TX is attended!\n");
		return -ENOTCONN;
	}

	if (IS_ERR(s_earc->clk_tx_dmac) || IS_ERR(s_earc->clk_tx_dmac_srcpll))
		return -ENOTCONN;

	/* tx dmac clk */
	ret = clk_prepare_enable(s_earc->clk_tx_dmac);
	if (ret) {
		dev_err(s_earc->dev,
			"Can't enable earc clk_tx_dmac: %d\n", ret);
		return ret;
	}
	s_earc->tx_dmac_clk_on = true;

	ret = clk_prepare_enable(s_earc->clk_tx_dmac_srcpll);
	if (ret) {
		dev_err(s_earc->dev,
			"Can't enable earc clk_tx_dmac_srcpll:%d\n", ret);
		return ret;
	}

	earctx_update_clk(s_earc, runtime->channels, runtime->rate);
	earctx_dmac_init(s_earc->tx_top_map,
			 s_earc->tx_dmac_map,
			 s_earc->chipinfo->earc_spdifout_lane_mask);
	earctx_dmac_set_format(s_earc->tx_dmac_map,
			       fr->fifo_id,
			       bit_depth - 1,
			       spdifout_get_frddr_type(bit_depth));
	iec_get_cnsmr_cs_info(&cs_info,
			      s_earc->tx_audio_coding_type,
			      runtime->channels,
			      runtime->rate);
	earctx_set_cs_info(s_earc->tx_dmac_map,
			   s_earc->tx_audio_coding_type,
			   &cs_info,
			   &s_earc->tx_cs_lpcm_ca);
	earctx_set_cs_mute(s_earc->tx_dmac_map, s_earc->tx_cs_mute);

	return 0;
}

void aml_earctx_enable(bool enable)
{
	if (!s_earc)
		return;
	earctx_enable(s_earc->tx_top_map,
		s_earc->tx_cmdc_map,
		s_earc->tx_dmac_map,
		s_earc->tx_audio_coding_type,
		enable);
}

static int earc_dai_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "eARC/ARC TX enable\n");

			aml_frddr_enable(p_earc->fddr, true);
			earctx_enable(p_earc->tx_top_map,
				      p_earc->tx_cmdc_map,
				      p_earc->tx_dmac_map,
				      p_earc->tx_audio_coding_type,
				      true);
		} else {
			dev_info(substream->pcm->card->dev, "eARC/ARC RX enable\n");

			aml_toddr_enable(p_earc->tddr, true);
			earcrx_enable(p_earc->rx_cmdc_map,
				      p_earc->rx_dmac_map,
				      true);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "eARC/ARC TX disable\n");

			earctx_enable(p_earc->tx_top_map,
				      p_earc->tx_cmdc_map,
				      p_earc->tx_dmac_map,
				      p_earc->tx_audio_coding_type,
				      false);
			aml_frddr_enable(p_earc->fddr, false);
		} else {
			dev_info(substream->pcm->card->dev, "eARC/ARC RX disable\n");

			earcrx_enable(p_earc->rx_cmdc_map,
				      p_earc->rx_dmac_map,
				      false);
			aml_toddr_enable(p_earc->tddr, false);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int earc_dai_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int channels = params_channels(params);
	unsigned int rate = params_rate(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		earctx_update_clk(p_earc, channels, rate);

	return 0;
}

static int earc_dai_set_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	if (clk_id == 1) {
		/* RX */
		clk_set_rate(p_earc->clk_rx_dmac, 500000000);

		aud_dbg(p_earc->dev,
			"earc rx cmdc clk:%lu rx dmac clk:%lu\n",
			clk_get_rate(p_earc->clk_rx_cmdc),
			clk_get_rate(p_earc->clk_rx_dmac));
	}

	return 0;
}

static int earc_dai_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	int ret;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (IS_ERR(p_earc->clk_tx_dmac) ||
		    IS_ERR(p_earc->clk_tx_dmac_srcpll))
			return -ENOTCONN;
		/* tx dmac clk */
		ret = clk_prepare_enable(p_earc->clk_tx_dmac);
		if (ret) {
			dev_err(p_earc->dev,
			"Can't enable earc clk_tx_dmac: %d\n", ret);
			goto err;
		}
		p_earc->tx_dmac_clk_on = true;

		ret = clk_prepare_enable(p_earc->clk_tx_dmac_srcpll);
		if (ret) {
			dev_err(p_earc->dev,
			"Can't enable earc clk_tx_dmac_srcpll:%d\n", ret);
			goto err;
		}
	} else {
		unsigned long flags;

		if (IS_ERR(p_earc->clk_rx_dmac) ||
		    IS_ERR(p_earc->clk_rx_dmac_srcpll))
			return -ENOTCONN;
		/* rx dmac clk */
		ret = clk_prepare_enable(p_earc->clk_rx_dmac);
		if (ret) {
			dev_err(p_earc->dev,
			"Can't enable earc clk_rx_dmac: %d\n", ret);
			goto err;
		}
		spin_lock_irqsave(&p_earc->rx_lock, flags);
		p_earc->rx_dmac_clk_on = true;
		spin_unlock_irqrestore(&p_earc->rx_lock, flags);

		ret = clk_prepare_enable(p_earc->clk_rx_dmac_srcpll);
		if (ret) {
			dev_err(p_earc->dev,
			"Can't enable earc clk_rx_dmac_srcpll: %d\n", ret);
			goto err;
		}

		if (p_earc->chipinfo->rx_dmac_sync_int &&
		    earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map) ==
						  ATNDTYP_EARC) {
			earcrx_pll_reset(p_earc);
		}
	}

	p_earc->substreams[substream->stream] = substream;

	return 0;

err:
	dev_err(p_earc->dev, "failed enable clock\n");

	return -EINVAL;
}


static void earc_dai_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (!IS_ERR(p_earc->clk_tx_dmac)) {
			clk_disable_unprepare(p_earc->clk_tx_dmac);
			p_earc->tx_dmac_clk_on = false;
			p_earc->tx_dmac_freq = 0;
		}
		if (!IS_ERR(p_earc->clk_tx_dmac_srcpll))
			clk_disable_unprepare(p_earc->clk_tx_dmac_srcpll);
	} else {
		if (!IS_ERR(p_earc->clk_rx_dmac)) {
			unsigned long flags;

			spin_lock_irqsave(&p_earc->rx_lock, flags);
			p_earc->rx_dmac_clk_on = false;
			spin_unlock_irqrestore(&p_earc->rx_lock, flags);
			clk_disable_unprepare(p_earc->clk_rx_dmac);
		}
		if (!IS_ERR(p_earc->clk_rx_dmac_srcpll))
			clk_disable_unprepare(p_earc->clk_rx_dmac_srcpll);
	}

	p_earc->substreams[substream->stream] = NULL;
}

static struct snd_soc_dai_ops earc_dai_ops = {
	.prepare    = earc_dai_prepare,
	.trigger    = earc_dai_trigger,
	.hw_params  = earc_dai_hw_params,
	.set_sysclk = earc_dai_set_sysclk,
	.startup	= earc_dai_startup,
	.shutdown	= earc_dai_shutdown,
};

static struct snd_soc_dai_driver earc_dai[] = {
	{
		.name     = "EARC/ARC",
		.id       = 0,
		.playback = {
		      .channels_min = 1,
		      .channels_max = 32,
		      .rates        = EARC_RATES,
		      .formats      = EARC_FORMATS,
		},
		.capture = {
		     .channels_min = 1,
		     .channels_max = 32,
		     .rates        = EARC_RATES,
		     .formats      = EARC_FORMATS,
		},
		.ops    = &earc_dai_ops,
	},
};


static const char *const attended_type[] = {
	"DISCONNECT",
	"ARC",
	"eARC"
};

static int ss_prepare(struct snd_pcm_substream *substream,
			void *pfrddr,
			int samesource_sel,
			int lane_i2s,
			enum aud_codec_types type,
			int share_lvl,
			int separated)
{
	pr_debug("%s() %d, lvl %d\n", __func__, __LINE__, share_lvl);
	sharebuffer_prepare(
		substream,
		pfrddr,
		samesource_sel,
		lane_i2s,
		type,
		share_lvl,
		separated);

	return 0;
}

static int ss_trigger(int cmd, int samesource_sel, bool reenable)
{
	pr_debug("%s() ss %d\n", __func__, samesource_sel);
	sharebuffer_trigger(cmd, samesource_sel, reenable);

	return 0;
}

struct samesrc_ops earctx_ss_ops = {
	.prepare = ss_prepare,
	.trigger = ss_trigger
};

const struct soc_enum attended_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(attended_type),
			attended_type);

static int earcrx_get_attend_type(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum attend_type type =
		earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);

	ucontrol->value.integer.value[0] = type;

	return 0;
}

static int earcrx_set_attend_type(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);

	if (state != CMDC_ST_IDLE2)
		return 0;

	/* only support set cmdc from idle to ARC */

	return 0;
}

static int earcrx_arc_get_enable(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum attend_type type =
		earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);

	ucontrol->value.integer.value[0] = (bool)(type == ATNDTYP_ARC);

	return 0;
}

static int earcrx_arc_set_enable(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	if (!p_earc)
		return 0;

	earcrx_cmdc_arc_connect(
		p_earc->rx_cmdc_map,
		(bool)ucontrol->value.integer.value[0]);

	return 0;
}

static int earctx_get_attend_type(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum attend_type type;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	type = earctx_cmdc_get_attended_type(p_earc->tx_cmdc_map);

	ucontrol->value.integer.value[0] = type;

	return 0;
}

static int earctx_set_attend_type(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	state = earctx_cmdc_get_state(p_earc->tx_cmdc_map);

	if (state != CMDC_ST_IDLE2)
		return 0;

	/* only support set cmdc from idle to ARC */

	return 0;
}

static int earcrx_get_latency(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state;
	u8  val = 0;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earcrx_cmdc_get_latency(p_earc->rx_cmdc_map, &val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int earcrx_set_latency(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	u8 latency = ucontrol->value.integer.value[0];
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earcrx_cmdc_set_latency(p_earc->rx_cmdc_map, &latency);

	return 0;
}

static int earcrx_get_cds(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	struct soc_bytes_ext *bytes_ext =
		(struct soc_bytes_ext *)kcontrol->private_value;
	u8 *value = (u8 *)ucontrol->value.bytes.data;
	int i;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	for (i = 0; i < bytes_ext->max; i++)
		*value++ = p_earc->cds_data[i];

	return 0;
}

static int earcrx_set_cds(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	memcpy(p_earc->cds_data, ucontrol->value.bytes.data, CDS_MAX_BYTES);

	state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);
	if (state == CMDC_ST_EARC)
		earcrx_cmdc_set_cds(p_earc->rx_cmdc_map, p_earc->cds_data);

	return 0;
}

int earcrx_get_mute(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	unsigned long flags;
	int mute = 0;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->rx_dmac_clk_on)
		mute = earcrx_get_cs_mute(p_earc->rx_dmac_map);
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	ucontrol->value.integer.value[0] = mute;

	return 0;
}

int earcrx_get_audio_coding_type(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum audio_coding_types coding_type;
	enum attend_type type;
	unsigned long flags;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	if (!p_earc->rx_dmac_clk_on)
		return 0;

	type = earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);
	spin_lock_irqsave(&p_earc->rx_lock, flags);
	coding_type = earcrx_get_cs_fmt(p_earc->rx_dmac_map, type);
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	ucontrol->value.integer.value[0] = coding_type;

	return 0;
}

int earcrx_get_freq(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum audio_coding_types coding_type = 0;
	enum attend_type type;
	int freq = 0;
	unsigned long flags;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	if (!p_earc->rx_dmac_clk_on)
		return 0;

	type = earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);
	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->rx_dmac_clk_on) {
		coding_type = earcrx_get_cs_fmt(p_earc->rx_dmac_map, type);
		freq = earcrx_get_cs_freq(p_earc->rx_dmac_map, coding_type);
	}
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	ucontrol->value.integer.value[0] = freq;

	return 0;
}

int earcrx_get_word_length(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	unsigned long flags;
	int wlen = 0;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->rx_dmac_clk_on)
		wlen = earcrx_get_cs_word_length(p_earc->rx_dmac_map);
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	ucontrol->value.integer.value[0] = wlen;

	return 0;
}

static int earctx_get_latency(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state;
	u8 val = 0;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	state = earctx_cmdc_get_state(p_earc->tx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earctx_cmdc_get_latency(p_earc->tx_cmdc_map, &val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int earctx_set_latency(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	u8 latency = ucontrol->value.integer.value[0];
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	state = earctx_cmdc_get_state(p_earc->tx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earctx_cmdc_set_latency(p_earc->tx_cmdc_map, &latency);

	return 0;
}

static int earctx_get_cds(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	struct soc_bytes_ext *bytes_ext =
		(struct soc_bytes_ext *)kcontrol->private_value;
	u8 *value = (u8 *)ucontrol->value.bytes.data;
	enum cmdc_st state;
	u8 data[256];
	int i;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	state = earctx_cmdc_get_state(p_earc->tx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earctx_cmdc_get_cds(p_earc->tx_cmdc_map, data);

	for (i = 0; i < bytes_ext->max; i++)
		*value++ = data[i];

	return 0;
}

int earctx_get_audio_coding_type(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	ucontrol->value.integer.value[0] = p_earc->tx_audio_coding_type;

	return 0;
}

int earctx_set_audio_coding_type(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum audio_coding_types new_coding_type =
				ucontrol->value.integer.value[0];
	enum audio_coding_types last_coding_type;
	struct frddr *fr;
	enum attend_type type;
	struct iec_cnsmr_cs cs_info;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	last_coding_type = p_earc->tx_audio_coding_type;
	fr = p_earc->fddr;

	if (new_coding_type == last_coding_type)
		return 0;

	p_earc->tx_audio_coding_type = new_coding_type;

	if (!p_earc->tx_dmac_clk_on)
		return 0;

	aud_dbg(p_earc->dev,
		"tx audio coding type: 0x%02x\n",
		new_coding_type);

	type = earctx_cmdc_get_attended_type(p_earc->tx_cmdc_map);

	/* Update dmac clk ? */
	earctx_update_clk(p_earc, fr->channels, fr->rate);

	/* Update ECC enable/disable */
	earctx_compressed_enable(p_earc->tx_dmac_map,
				 type, new_coding_type, true);

	/* Update Channel Status in runtime */
	iec_get_cnsmr_cs_info(&cs_info,
			      p_earc->tx_audio_coding_type,
			      fr->channels,
			      fr->rate);
	earctx_set_cs_info(p_earc->tx_dmac_map,
			   p_earc->tx_audio_coding_type,
			   &cs_info,
			   &p_earc->tx_cs_lpcm_ca);

	return 0;
}

int earctx_get_mute(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	if (!p_earc->tx_dmac_clk_on)
		return 0;

	ucontrol->value.integer.value[0] = p_earc->tx_cs_mute;

	return 0;
}

/*
 * eARC TX asserts the Channel Status Mute bit
 * to eARC RX before format change
 */
int earctx_set_mute(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	bool mute = ucontrol->value.integer.value[0];

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	p_earc->tx_cs_mute = mute;

	if (!p_earc->tx_dmac_clk_on)
		return 0;

	earctx_set_cs_mute(p_earc->tx_dmac_map, mute);

	return 0;
}

static int earcrx_get_iec958(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	unsigned long flags;
	int cs = 0;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->rx_dmac_clk_on)
		cs = earcrx_get_cs_iec958(p_earc->rx_dmac_map);
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	ucontrol->value.iec958.status[0] = (cs >> 0) & 0xff;
	ucontrol->value.iec958.status[1] = (cs >> 8) & 0xff;
	ucontrol->value.iec958.status[2] = (cs >> 16) & 0xff;
	ucontrol->value.iec958.status[3] = (cs >> 24) & 0xff;

	aud_dbg(p_earc->dev,
		"x get[AES0=%#x AES1=%#x AES2=%#x AES3=%#x]\n",
		ucontrol->value.iec958.status[0],
		ucontrol->value.iec958.status[1],
		ucontrol->value.iec958.status[2],
		ucontrol->value.iec958.status[3]
	);

	return 0;
}

static int earctx_get_iec958(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	int cs;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	if (!p_earc->tx_dmac_clk_on)
		return 0;

	cs = earctx_get_cs_iec958(p_earc->tx_dmac_map);

	ucontrol->value.iec958.status[0] = (cs >> 0) & 0xff;
	ucontrol->value.iec958.status[1] = (cs >> 8) & 0xff;
	ucontrol->value.iec958.status[2] = (cs >> 16) & 0xff;
	ucontrol->value.iec958.status[3] = (cs >> 24) & 0xff;

	aud_dbg(p_earc->dev,
		"x get[AES0=%#x AES1=%#x AES2=%#x AES3=%#x]\n",
		ucontrol->value.iec958.status[0],
		ucontrol->value.iec958.status[1],
		ucontrol->value.iec958.status[2],
		ucontrol->value.iec958.status[3]
	);

	return 0;
}

static int earctx_set_iec958(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	int cs = 0;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	if (!p_earc->tx_dmac_clk_on)
		return 0;

	cs |= ucontrol->value.iec958.status[0];
	cs |= ucontrol->value.iec958.status[1] << 8;
	cs |= ucontrol->value.iec958.status[2] << 16;
	cs |= ucontrol->value.iec958.status[3] << 24;

	earctx_set_cs_iec958(p_earc->tx_dmac_map, cs);

	aud_dbg(p_earc->dev,
		"x put [AES0=%#x AES1=%#x AES2=%#x AES3=%#x]\n",
		ucontrol->value.iec958.status[0],
		ucontrol->value.iec958.status[1],
		ucontrol->value.iec958.status[2],
		ucontrol->value.iec958.status[3]
		);

	return 0;
}

static const struct snd_pcm_chmap_elem snd_pcm_chmaps[] = {
	{ .channels = 2,
	  .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR }
	},
	{ .channels = 3,
	  .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR,
		   SNDRV_CHMAP_LFE }
	},
	{ .channels = 6,
	  .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR,
		   SNDRV_CHMAP_RL, SNDRV_CHMAP_RR,
		   SNDRV_CHMAP_FC, SNDRV_CHMAP_LFE }
	},
	{ .channels = 8,
	  .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR,
		   SNDRV_CHMAP_RL, SNDRV_CHMAP_RR,
		   SNDRV_CHMAP_FC, SNDRV_CHMAP_LFE,
		   SNDRV_CHMAP_RLC, SNDRV_CHMAP_RRC }
	}
};

#define TX_CHMAP "eARC_TX Channel Map"

static bool support_chmap(enum audio_coding_types coding_type)
{
	if ((coding_type == AUDIO_CODING_TYPE_STEREO_LPCM) ||
	    (coding_type == AUDIO_CODING_TYPE_MULTICH_2CH_LPCM) ||
	    (coding_type == AUDIO_CODING_TYPE_MULTICH_8CH_LPCM))
		return true;

	return false;
}

static bool support_ca(unsigned int ca)
{
	if ((ca == AIF_8CH) ||
	    (ca == AIF_6CH) ||
	    (ca == AIF_3CH) ||
	    (ca == AIF_2CH))
		return true;

	return false;
}

static void convert_ca2chmap(unsigned int ca, struct snd_pcm_chmap *chmap)
{
	if (!chmap)
		return;

	if (ca == AIF_8CH) {
		chmap->channel_mask = (1U << 8);
		chmap->max_channels = 8;
	} else if (ca == AIF_6CH) {
		chmap->channel_mask = (1U << 6);
		chmap->max_channels = 6;
	} else if (ca == AIF_3CH) {
		chmap->channel_mask = (1U << 3);
		chmap->max_channels = 3;
	} else if (ca == AIF_2CH) {
		chmap->channel_mask = (1U << 2);
		chmap->max_channels = 2;
	} else {
		chmap->channel_mask = 0;
		chmap->max_channels = 0;
	}
}

static int earcrx_get_ca(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum audio_coding_types coding_type;
	unsigned long flags;

	if (!p_earc || IS_ERR(p_earc->tx_top_map)) {
		ucontrol->value.integer.value[0] = 0xff;
		return 0;
	}

	if (!p_earc->rx_dmac_clk_on) {
		ucontrol->value.integer.value[0] = 0xff;
		return 0;
	}

	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->rx_dmac_clk_on) {
		coding_type = earcrx_get_cs_fmt(p_earc->rx_dmac_map,
					earcrx_cmdc_get_attended_type(
						p_earc->rx_cmdc_map));

		ucontrol->value.integer.value[0] =
			earcrx_get_cs_ca(p_earc->rx_dmac_map);
	} else {
		ucontrol->value.integer.value[0] = 0xff;
	}
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	return 0;
}

static int earctx_get_chmap(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcm_chmap *info = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = info->private_data;
	struct snd_pcm_chmap *chmap;
	enum audio_coding_types coding_type;
	int ca;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		goto err_chmap;

	chmap = p_earc->tx_chmap;
	coding_type = p_earc->tx_audio_coding_type;

	/* Not in runtime, only show what's set. */
	if (!p_earc->tx_dmac_clk_on) {
		ca = p_earc->tx_cs_lpcm_ca;
		goto show_chmap;
	}

	if (support_chmap(coding_type))
		ca = p_earc->tx_cs_lpcm_ca;
	else
		ca = 0;

show_chmap:
	convert_ca2chmap(ca, chmap);

	return 0;

/* no chmap is found */
err_chmap:

	return 0;
}

static int earctx_set_chmap(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcm_chmap *info = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = info->private_data;
	enum audio_coding_types coding_type;
	int ca = ucontrol->value.integer.value[0];

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	if (!support_ca(ca))
		return -EINVAL;

	coding_type = p_earc->tx_audio_coding_type;
	p_earc->tx_cs_lpcm_ca = ca;

	if (!p_earc->tx_dmac_clk_on)
		return 0;

	if (!support_chmap(coding_type))
		ca = 0;

	/* runtime, update to channel status */
	earctx_set_cs_ca(p_earc->tx_dmac_map, ca);

	return 0;
}

static int earc_pcm_add_chmap_ctls(struct earc *p_earc, struct snd_pcm *pcm)
{
	struct snd_kcontrol *kctl;
	int ret = 0;

	/* TX Channel Map */
	if (!IS_ERR(p_earc->tx_top_map)) {
		ret = snd_pcm_add_chmap_ctls(pcm,
					     SNDRV_PCM_STREAM_PLAYBACK,
					     snd_pcm_chmaps, 8, 0,
					     &p_earc->tx_chmap);
		if (ret < 0)
			return ret;

		p_earc->tx_chmap->private_data = p_earc;
		kctl = p_earc->tx_chmap->kctl;
		kctl->vd[0].access |= SNDRV_CTL_ELEM_ACCESS_READWRITE;
		kctl->vd[0].access |= SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE;
		kctl->get = earctx_get_chmap;
		kctl->put = earctx_set_chmap;
		memcpy(kctl->id.name, TX_CHMAP, strlen(TX_CHMAP));
	}
	return 0;
}

static const struct snd_kcontrol_new earc_controls[] = {
	SOC_SINGLE_BOOL_EXT("eARC ARC Switch",
			    0,
			    earcrx_arc_get_enable,
			    earcrx_arc_set_enable),

	SOC_ENUM_EXT("eARC_RX attended type",
		     attended_type_enum,
		     earcrx_get_attend_type,
		     earcrx_set_attend_type),

	SOC_ENUM_EXT("eARC_TX attended type",
		     attended_type_enum,
		     earctx_get_attend_type,
		     earctx_set_attend_type),

	SND_SOC_BYTES_EXT("eARC_RX Latency",
			  1,
			  earcrx_get_latency,
			  earcrx_set_latency),

	SND_SOC_BYTES_EXT("eARC_TX Latency",
			  1,
			  earctx_get_latency,
			  earctx_set_latency),

	SND_SOC_BYTES_EXT("eARC_RX CDS",
			  CDS_MAX_BYTES,
			  earcrx_get_cds,
			  earcrx_set_cds),

	SND_SOC_BYTES_EXT("eARC_TX CDS",
			  CDS_MAX_BYTES,
			  earctx_get_cds,
			  NULL),

	SOC_ENUM_EXT("eARC_RX Audio Coding Type",
		     audio_coding_type_enum,
		     earcrx_get_audio_coding_type,
		     NULL),

	SOC_ENUM_EXT("eARC_TX Audio Coding Type",
		     audio_coding_type_enum,
		     earctx_get_audio_coding_type,
		     earctx_set_audio_coding_type),

	SND_INT("eARC_RX Channel Allocation",
		earcrx_get_ca,
		NULL),

	SOC_SINGLE_BOOL_EXT("eARC_RX CS Mute",
			    0,
			    earcrx_get_mute,
			    NULL),

	SOC_SINGLE_BOOL_EXT("eARC_TX CS Mute",
			    0,
			    earctx_get_mute,
			    earctx_set_mute),

	SND_INT("eARC_RX Audio Sample Frequency",
		earcrx_get_freq,
		NULL),

	SND_INT("eARC_RX Audio Word Length",
		earcrx_get_word_length,
		NULL),

	/* Status cchanel controller */
	SND_IEC958(SNDRV_CTL_NAME_IEC958("", CAPTURE, DEFAULT),
		   earcrx_get_iec958,
		   NULL),

	SND_IEC958(SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
		   earctx_get_iec958,
		   earctx_set_iec958),
};

static const struct snd_soc_component_driver earc_component = {
	.controls       = earc_controls,
	.num_controls   = ARRAY_SIZE(earc_controls),
	.name		= DRV_NAME,
};

struct earc_chipinfo sm1_earc_chipinfo = {
	.earc_spdifout_lane_mask = EARC_SPDIFOUT_LANE_MASK_V1,
	.rx_dmac_sync_int = false,
};

struct earc_chipinfo tm2_earc_chipinfo = {
	.earc_spdifout_lane_mask = EARC_SPDIFOUT_LANE_MASK_V1,
	.rx_dmac_sync_int = false,
};

struct earc_chipinfo tm2_revb_earc_chipinfo = {
	.earc_spdifout_lane_mask = EARC_SPDIFOUT_LANE_MASK_V2,
	.rx_dmac_sync_int = true,
};

static const struct of_device_id earc_device_id[] = {
	{
		.compatible = "amlogic, sm1-snd-earc",
		.data = &sm1_earc_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-snd-earc",
		.data = &tm2_earc_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-snd-earc",
		.data = &tm2_revb_earc_chipinfo,
	},
	{},
};

MODULE_DEVICE_TABLE(of, earc_device_id);

static const unsigned int earcrx_extcon[] = {
	EXTCON_EARCRX_ATNDTYP_ARC,
	EXTCON_EARCRX_ATNDTYP_EARC,
	EXTCON_NONE,
};

static int earcrx_extcon_register(struct earc *p_earc)
{
	int ret = 0;

	/* earc or arc connect */
	p_earc->rx_edev = devm_extcon_dev_allocate(p_earc->dev, earcrx_extcon);
	if (IS_ERR(p_earc->rx_edev)) {
		dev_err(p_earc->dev, "failed to allocate earc extcon!!!\n");
		ret = -ENOMEM;
		return ret;
	}
	p_earc->rx_edev->dev.parent  = p_earc->dev;
	p_earc->rx_edev->name = "earcrx";

	dev_set_name(&p_earc->rx_edev->dev, "earcrx");
	ret = extcon_dev_register(p_earc->rx_edev);
	if (ret < 0) {
		dev_err(p_earc->dev, "earc extcon failed to register!!\n");
		return ret;
	}

	return ret;
}

void earc_hdmitx_hpdst(bool st)
{
	struct earc *p_earc = s_earc;

	if (!p_earc)
		return;

	dev_info(p_earc->dev, "HDMITX cable is %s\n",
		 st ? "plugin" : "plugout");

	if (!p_earc->rx_bootup_auto_cal) {
		p_earc->rx_bootup_auto_cal = true;
		p_earc->event |= EVENT_RX_ANA_AUTO_CAL;
		schedule_work(&p_earc->work);
	}

	/* rx cmdc init */
	earcrx_cmdc_init(p_earc->rx_top_map,
			 st,
			 p_earc->chipinfo->rx_dmac_sync_int);

	if (st)
		earcrx_cmdc_int_mask(p_earc->rx_top_map);

	earcrx_cmdc_arc_connect(p_earc->rx_cmdc_map, st);

	earcrx_cmdc_hpd_detect(p_earc->rx_cmdc_map, st);
}

static int earcrx_cmdc_setup(struct earc *p_earc)
{
	int ret = 0;

	/* set cmdc clk */
	audiobus_update_bits(EE_AUDIO_CLK_GATE_EN1, 0x1 << 6, 0x1 << 6);
	if (!IS_ERR(p_earc->clk_rx_cmdc)) {
		clk_set_rate(p_earc->clk_rx_cmdc, 10000000);

		ret = clk_prepare_enable(p_earc->clk_rx_cmdc);
		if (ret) {
			dev_err(p_earc->dev, "Can't enable earc clk_rx_cmdc\n");
			return ret;
		}
	}

	ret = devm_request_threaded_irq(p_earc->dev,
					p_earc->irq_earc_rx,
					NULL,
					earc_rx_isr,
					IRQF_TRIGGER_HIGH |
					IRQF_ONESHOT,
					"earc_rx",
					p_earc);
	if (ret) {
		dev_err(p_earc->dev, "failed to claim earc_rx %u\n",
			p_earc->irq_earc_rx);
		return ret;
	}

	/* Default: arc arc_initiated */
	earcrx_cmdc_arc_connect(p_earc->rx_cmdc_map, true);

	return ret;
}

static const unsigned int earctx_extcon[] = {
	EXTCON_EARCTX_ATNDTYP_ARC,
	EXTCON_EARCTX_ATNDTYP_EARC,
	EXTCON_NONE,
};

static int earctx_extcon_register(struct earc *p_earc)
{
	int ret = 0;

	/* earc or arc connect */
	p_earc->tx_edev = devm_extcon_dev_allocate(p_earc->dev, earctx_extcon);
	if (IS_ERR(p_earc->tx_edev)) {
		dev_err(p_earc->dev, "failed to allocate earc extcon!!!\n");
		ret = -ENOMEM;
		return ret;
	}
	p_earc->tx_edev->dev.parent  = p_earc->dev;
	p_earc->tx_edev->name = "earctx";

	dev_set_name(&p_earc->tx_edev->dev, "earctx");
	ret = extcon_dev_register(p_earc->tx_edev);
	if (ret < 0) {
		dev_err(p_earc->dev, "earc extcon failed to register!!\n");
		return ret;
	}

	return ret;
}

void earc_hdmirx_hpdst(int earc_port, bool st)
{
	struct earc *p_earc = s_earc;

	if (!p_earc)
		return;

	dev_info(p_earc->dev, "HDMIRX cable port:%d is %s\n",
		 earc_port,
		 st ? "plugin" : "plugout");

	if (!p_earc->tx_bootup_auto_cal) {
		p_earc->tx_bootup_auto_cal = true;
		p_earc->event |= EVENT_TX_ANA_AUTO_CAL;
		schedule_work(&p_earc->work);
	}

	/* tx cmdc anlog init */
	earctx_cmdc_init(p_earc->tx_top_map, st);

	earctx_cmdc_arc_connect(p_earc->tx_cmdc_map, st);
	earctx_cmdc_hpd_detect(p_earc->tx_top_map,
			       p_earc->tx_cmdc_map,
			       earc_port, st);
}

static int earctx_cmdc_setup(struct earc *p_earc)
{
	int ret = 0;

	/* set cmdc clk */
	audiobus_update_bits(EE_AUDIO_CLK_GATE_EN1, 0x1 << 5, 0x1 << 5);
	if (!IS_ERR(p_earc->clk_tx_cmdc)) {
		clk_set_rate(p_earc->clk_tx_cmdc, 10000000);

		ret = clk_prepare_enable(p_earc->clk_tx_cmdc);
		if (ret) {
			dev_err(p_earc->dev, "Can't enable earc clk_tx_cmdc\n");
			return ret;
		}
	}

	ret = devm_request_threaded_irq(p_earc->dev,
					p_earc->irq_earc_tx,
					NULL,
					earc_tx_isr,
					IRQF_TRIGGER_HIGH |
					IRQF_ONESHOT,
					"earc_tx",
					p_earc);
	if (ret) {
		dev_err(p_earc->dev, "failed to claim earc_tx %u\n",
			p_earc->irq_earc_tx);
		return ret;
	}

	/* Default: no time out to connect RX */
	earctx_cmdc_set_timeout(p_earc->tx_cmdc_map, 1);
	/* Default: arc arc_initiated */
	earctx_cmdc_int_mask(p_earc->tx_top_map);

	return ret;
}

static void earc_work_func(struct work_struct *work)
{
	struct earc *p_earc = container_of(work, struct earc, work);

	/* RX */
	if ((!IS_ERR(p_earc->rx_top_map)) &&
	    (p_earc->event & EVENT_RX_ANA_AUTO_CAL)) {
		p_earc->event &= ~EVENT_RX_ANA_AUTO_CAL;
		earcrx_ana_auto_cal(p_earc->rx_top_map);
	}

	/* TX */
	if ((!IS_ERR(p_earc->tx_top_map)) &&
	    (p_earc->event & EVENT_TX_ANA_AUTO_CAL)) {
		p_earc->event &= ~EVENT_TX_ANA_AUTO_CAL;
		earctx_ana_auto_cal(p_earc->tx_top_map);
	}
}

static int earc_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct device *dev = &pdev->dev;
	struct aml_audio_controller *actrl = NULL;
	struct earc *p_earc = NULL;
	int ret = 0;
	int ss = 0;

	p_earc = devm_kzalloc(dev, sizeof(struct earc), GFP_KERNEL);
	if (!p_earc)
		return -ENOMEM;

	ret = of_property_read_u32(dev->of_node, "samesource_sel", &ss);
	if (ret < 0)
		p_earc->samesource_sel = SHAREBUFFER_NONE;
	else
		p_earc->samesource_sel = ss;

	p_earc->dev = dev;
	dev_set_drvdata(dev, p_earc);

	p_earc->chipinfo = (struct earc_chipinfo *)
		of_device_get_match_data(dev);

	if (!p_earc->chipinfo) {
		dev_warn_once(dev, "check whether to update earc chipinfo\n");
		return -EINVAL;
	}

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (node_prt == NULL)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
			platform_get_drvdata(pdev_parent);
	p_earc->actrl = actrl;

	p_earc->tx_cmdc_map = regmap_resource(dev, "tx_cmdc");
	if (!p_earc->tx_cmdc_map) {
		dev_err(dev,
			"Can't get earctx_cmdc regmap!!\n");
	}
	p_earc->tx_dmac_map = regmap_resource(dev, "tx_dmac");
	if (!p_earc->tx_dmac_map) {
		dev_err(dev,
			"Can't get earctx_dmac regmap!!\n");
	}
	p_earc->tx_top_map = regmap_resource(dev, "tx_top");
	if (!p_earc->tx_top_map) {
		dev_err(dev,
			"Can't get earctx_top regmap!!\n");
	}

	p_earc->rx_cmdc_map = regmap_resource(dev, "rx_cmdc");
	if (!p_earc->rx_cmdc_map) {
		dev_err(dev,
			"Can't get earcrx_cdmc regmap!!\n");
	}
	p_earc->rx_dmac_map = regmap_resource(dev, "rx_dmac");
	if (!p_earc->rx_dmac_map) {
		dev_err(dev,
			"Can't get earcrx_dmac regmap!!\n");
	}
	p_earc->rx_top_map = regmap_resource(dev, "rx_top");
	if (!p_earc->rx_top_map) {
		dev_err(dev,
			"Can't get earcrx_top regmap!!\n");
	}

	/* clock gate */
	p_earc->clk_rx_gate = devm_clk_get(&pdev->dev, "rx_gate");
	if (IS_ERR(p_earc->clk_rx_gate)) {
		dev_err(&pdev->dev,
			"Can't get earc gate\n");
		/*return PTR_ERR(p_earc->clk_rx_gate);*/
	}
	/* RX */
	spin_lock_init(&p_earc->rx_lock);
	p_earc->clk_rx_cmdc = devm_clk_get(&pdev->dev, "rx_cmdc");
	if (IS_ERR(p_earc->clk_rx_cmdc)) {
		dev_err(&pdev->dev,
			"Can't get clk_rx_cmdc\n");
		return PTR_ERR(p_earc->clk_rx_cmdc);
	}
	p_earc->clk_rx_dmac = devm_clk_get(&pdev->dev, "rx_dmac");
	if (IS_ERR(p_earc->clk_rx_dmac)) {
		dev_err(&pdev->dev,
			"Can't get clk_rx_dmac\n");
		return PTR_ERR(p_earc->clk_rx_dmac);
	}
	p_earc->clk_rx_cmdc_srcpll = devm_clk_get(&pdev->dev, "rx_cmdc_srcpll");
	if (IS_ERR(p_earc->clk_rx_cmdc_srcpll)) {
		dev_err(&pdev->dev,
			"Can't get clk_rx_cmdc_srcpll\n");
		return PTR_ERR(p_earc->clk_rx_cmdc_srcpll);
	}
	p_earc->clk_rx_dmac_srcpll = devm_clk_get(&pdev->dev, "rx_dmac_srcpll");
	if (IS_ERR(p_earc->clk_rx_dmac_srcpll)) {
		dev_err(&pdev->dev,
			"Can't get clk_rx_dmac_srcpll\n");
		return PTR_ERR(p_earc->clk_rx_dmac_srcpll);
	}
	ret = clk_set_parent(p_earc->clk_rx_cmdc, p_earc->clk_rx_cmdc_srcpll);
	if (ret) {
		dev_err(dev,
			"Can't set clk_rx_cmdc parent clock\n");
		ret = PTR_ERR(p_earc->clk_rx_cmdc);
		return ret;
	}
	ret = clk_set_parent(p_earc->clk_rx_dmac, p_earc->clk_rx_dmac_srcpll);
	if (ret) {
		dev_err(dev,
			"Can't set clk_rx_dmac parent clock\n");
		ret = PTR_ERR(p_earc->clk_rx_dmac);
		return ret;
	}

	/* TX */
	p_earc->clk_tx_cmdc = devm_clk_get(&pdev->dev, "tx_cmdc");
	if (IS_ERR(p_earc->clk_tx_cmdc)) {
		dev_err(&pdev->dev,
			"Check whether support eARC TX\n");
	}
	p_earc->clk_tx_dmac = devm_clk_get(&pdev->dev, "tx_dmac");
	if (IS_ERR(p_earc->clk_tx_dmac)) {
		dev_err(&pdev->dev,
			"Check whether support eARC TX\n");
	}
	p_earc->clk_tx_cmdc_srcpll = devm_clk_get(&pdev->dev, "tx_cmdc_srcpll");
	if (IS_ERR(p_earc->clk_tx_cmdc_srcpll)) {
		dev_err(&pdev->dev,
			"Check whether support eARC TX\n");
	}
	p_earc->clk_tx_dmac_srcpll = devm_clk_get(&pdev->dev, "tx_dmac_srcpll");
	if (IS_ERR(p_earc->clk_tx_dmac_srcpll)) {
		dev_err(&pdev->dev,
			"Check whether support eARC TX\n");
	}
	if (!IS_ERR(p_earc->clk_tx_cmdc) &&
		!IS_ERR(p_earc->clk_tx_cmdc_srcpll)) {
		ret = clk_set_parent(p_earc->clk_tx_cmdc,
				p_earc->clk_tx_cmdc_srcpll);
		if (ret) {
			dev_err(dev,
				"Can't set clk_tx_cmdc parent clock\n");
			ret = PTR_ERR(p_earc->clk_tx_cmdc);
			return ret;
		}
	}
	if (!IS_ERR(p_earc->clk_tx_dmac) &&
		!IS_ERR(p_earc->clk_tx_dmac_srcpll)) {
		ret = clk_set_parent(p_earc->clk_tx_dmac,
				p_earc->clk_tx_dmac_srcpll);
		if (ret) {
			dev_err(dev,
				"Can't set clk_tx_dmac parent clock\n");
			ret = PTR_ERR(p_earc->clk_tx_dmac);
			return ret;
		}
	}

	/* irqs */
	p_earc->irq_earc_rx =
		platform_get_irq_byname(pdev, "earc_rx");
	if (p_earc->irq_earc_rx < 0) {
		dev_err(dev, "platform get irq earc_rx failed\n");
		return p_earc->irq_earc_rx;
	}
	p_earc->irq_earc_tx =
		platform_get_irq_byname(pdev, "earc_tx");
	if (p_earc->irq_earc_tx < 0)
		dev_err(dev, "platform get irq earc_tx failed, Check whether support eARC TX\n");

	dev_info(dev, "%s, irq_earc_rx:%d, irq_earc_tx:%d\n",
		 __func__, p_earc->irq_earc_rx, p_earc->irq_earc_tx);

	ret = snd_soc_register_component(&pdev->dev,
				&earc_component,
				earc_dai,
				ARRAY_SIZE(earc_dai));
	if (ret) {
		dev_err(&pdev->dev,
			"snd_soc_register_component failed\n");
		return ret;
	}

	s_earc = p_earc;

	/* select hdmirx arc source from earctx spdif */
	arc_earc_source_select(EARCTX_SPDIF_TO_HDMIRX);

	/* RX */
	if (!IS_ERR(p_earc->rx_top_map)) {
		earcrx_extcon_register(p_earc);
		earcrx_cmdc_setup(p_earc);
	}

	/* TX */
	if (!IS_ERR(p_earc->tx_top_map)) {
		earctx_extcon_register(p_earc);
		earctx_cmdc_setup(p_earc);
		earctx_ss_ops.private = p_earc;
		register_samesrc_ops(SHAREBUFFER_EARCTX, &earctx_ss_ops);
	}

	if ((!IS_ERR(p_earc->rx_top_map)) ||
	    (!IS_ERR(p_earc->tx_top_map))) {
		INIT_WORK(&p_earc->work, earc_work_func);
		INIT_WORK(&p_earc->rx_dmac_int_work, valid_auto_work_func);
	}

	dev_err(dev, "registered eARC platform\n");

	return devm_snd_soc_register_platform(dev, &earc_platform);
}

struct platform_driver earc_driver = {
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = earc_device_id,
	},
	.probe = earc_platform_probe,
};

static int __init earc_init(void)
{
	return platform_driver_register(&earc_driver);
}
arch_initcall_sync(earc_init);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic eARC/ARC TX/RX ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("Platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, earc_device_id);
