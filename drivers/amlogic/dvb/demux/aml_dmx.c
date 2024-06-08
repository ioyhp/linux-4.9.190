/*
 * drivers/amlogic/dvb/demux/aml_dmx.c
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/poll.h>
#include <linux/crc32.h>
#include <linux/uaccess.h>
#include <asm/div64.h>
#include <linux/jiffies.h>
#include "aml_dmx.h"
#include "sc2_demux/mem_desc.h"
#include "sc2_demux/ts_output.h"
#include "sc2_demux/ts_input.h"
#include "sc2_demux/dvb_reg.h"
#include "../aucpu/aml_aucpu.h"
#include "aml_dsc.h"
#include "dmx_log.h"
#include "aml_dvb.h"

#define dprint_i(fmt, args...)  \
	dprintk(LOG_ERROR, debug_dmx, fmt, ## args)
#define dprint(fmt, args...)   \
	dprintk(LOG_ERROR, debug_dmx, "dmx:" fmt, ## args)
#define pr_dbg(fmt, args...)   \
	dprintk(LOG_DBG, debug_dmx, "dmx:" fmt, ## args)

#define MAX_SEC_FEED_NUM 32
#define MAX_TS_FEED_NUM 32
#define MAX_FILTER_PER_SEC_FEED 32

#define DMX_STATE_FREE      0
#define DMX_STATE_ALLOCATED 1
#define DMX_STATE_SET       2
#define DMX_STATE_READY     3
#define DMX_STATE_GO        4

#define SWDMX_MAX_PID 0x1fff

#define TS_OUTPUT_CHAN_PES_BUF_SIZE		(3 * 188 * 1024)
#define TS_OUTPUT_CHAN_SEC_BUF_SIZE		(188 * 500)
#define TS_OUTPUT_CHAN_PTS_BUF_SIZE		(16 * 500)
#define TS_OUTPUT_CHAN_PTS_SEC_BUF_SIZE		(128 * 1024)
#define TS_OUTPUT_CHAN_DVR_BUF_SIZE		(30 * 1024 * 188)

struct jiffies_pcr {
	u64 last_pcr;
	u64 last_time;
};

#define MAX_PCR_NUM			16
#define MAX_PCR_DIFF		100

//#define OPEN_REGISTER_NODE

static struct jiffies_pcr jiffies_pcr_record[MAX_PCR_NUM];
static u8 pcr_flag[MAX_PCR_NUM];
static struct out_elem *ts_out_elem;

MODULE_PARM_DESC(debug_dmx, "\n\t\t Enable demux debug information");
static int debug_dmx;
module_param(debug_dmx, int, 0644);

MODULE_PARM_DESC(video_buf_size, "\n\t\t set video buf size");
static int video_buf_size = TS_OUTPUT_CHAN_PES_BUF_SIZE;
module_param(video_buf_size, int, 0644);

MODULE_PARM_DESC(audio_buf_size, "\n\t\t set audio buf size");
static int audio_buf_size = TS_OUTPUT_CHAN_PES_BUF_SIZE;
module_param(audio_buf_size, int, 0644);

MODULE_PARM_DESC(pes_buf_size, "\n\t\t set pes buf size");
static int pes_buf_size = TS_OUTPUT_CHAN_PES_BUF_SIZE;
module_param(pes_buf_size, int, 0644);

MODULE_PARM_DESC(sec_buf_size, "\n\t\t set sec buf size");
static int sec_buf_size = TS_OUTPUT_CHAN_SEC_BUF_SIZE;
module_param(sec_buf_size, int, 0644);

MODULE_PARM_DESC(dvr_buf_size, "\n\t\t set sec buf size");
static int dvr_buf_size = TS_OUTPUT_CHAN_DVR_BUF_SIZE;
module_param(dvr_buf_size, int, 0644);

static int out_ts_elem_cb(struct out_elem *pout,
			  char *buf, int count, void *udata,
			  int req_len, int *req_ret);

static inline void _invert_mode(struct dmx_section_filter *filter)
{
	int i;

	for (i = 0; i < DMX_FILTER_SIZE; i++)
		filter->filter_mode[i] ^= 0xff;
}

static int _dmx_open(struct dmx_demux *demux)
{
	struct aml_dmx *pdmx = (struct aml_dmx *)demux->priv;

	if (pdmx->users >= MAX_SW_DEMUX_USERS)
		return -EUSERS;

	pdmx->users++;
	return 0;
}

static int _dmx_close(struct dmx_demux *demux)
{
	struct aml_dmx *pdmx = (struct aml_dmx *)demux->priv;

	if (pdmx->users == 0)
		return -ENODEV;

	pdmx->users--;

	if (pdmx->users == 0) {
		if (pdmx->sc2_input) {
			ts_input_close(pdmx->sc2_input);
			pdmx->sc2_input = NULL;
		}
	}
	//FIXME: release any unneeded resources if users==0
	return 0;
}

static int _dmx_write_from_user(struct dmx_demux *demux,
				const char __user *buf, size_t count)
{
	struct aml_dmx *pdmx = (struct aml_dmx *)demux->priv;
	int ret = 0;

	if (pdmx->reset_init == 0 && pdmx->video_pid != -1) {
		ts_input_write_empty(pdmx->sc2_input, pdmx->video_pid);
		pdmx->reset_init = 1;
	}

	ret = ts_input_write(pdmx->sc2_input, buf, count);

//	if (signal_pending(current))
//		return -EINTR;
	return ret;
}

static struct sw_demux_sec_feed *_dmx_section_feed_alloc(struct aml_dmx *demux)
{
	int i;

	for (i = 0; i < demux->sec_feed_num; i++)
		if (demux->section_feed[i].state == DMX_STATE_FREE)
			break;

	if (i == demux->sec_feed_num)
		return NULL;

	demux->section_feed[i].state = DMX_STATE_ALLOCATED;

	return &demux->section_feed[i];
}

static struct sw_demux_ts_feed *_dmx_ts_feed_alloc(struct aml_dmx *demux)
{
	int i;

	for (i = 0; i < demux->ts_feed_num; i++)
		if (demux->ts_feed[i].state == DMX_STATE_FREE)
			break;

	if (i == demux->ts_feed_num)
		return NULL;

	demux->ts_feed[i].state = DMX_STATE_ALLOCATED;

	return &demux->ts_feed[i];
}

static struct sw_demux_sec_filter *_dmx_dmx_sec_filter_alloc(struct
							     sw_demux_sec_feed
							     * sec_feed)
{
	int i;

	for (i = 0; i < MAX_FILTER_PER_SEC_FEED; i++)
		if (sec_feed->filter[i].state == DMX_STATE_FREE)
			break;

	if (i == MAX_FILTER_PER_SEC_FEED)
		return NULL;

	sec_feed->filter[i].state = DMX_STATE_ALLOCATED;

	return &sec_feed->filter[i];
}

static void prdump(const char *m, const void *data, u32 len)
{
	return;
	if (m)
		dprint_i("%s:\n", m);
	if (data) {
		size_t i = 0;

		const unsigned char *c __attribute__ ((unused)) = data;
		while (len >= 16) {
			dprint_i("%02x %02x %02x %02x %02x %02x %02x %02x ",
				 c[i], c[i + 1], c[i + 2], c[i + 3],
				 c[i + 4], c[i + 5], c[i + 6], c[i + 7]);
			dprint_i("%02x %02x %02x %02x %02x %02x %02x %02x\n",
				 c[i + 8], c[i + 9], c[i + 10],
				 c[i + 11], c[i + 12], c[i + 13],
				 c[i + 14], c[i + 15]);
			len -= 16;
			i += 16;
		}
		while (len >= 8) {
			dprint_i("%02x %02x %02x %02x %02x %02x %02x %02x\n",
				 c[i], c[i + 1], c[i + 2], c[i + 3],
				 c[i + 4], c[i + 5], c[i + 6], c[i + 7]);
			len -= 8;
			i += 8;
		}
		while (len >= 4) {
			dprint_i("%02x %02x %02x %02x\n",
				 c[i], c[i + 1], c[i + 2], c[i + 3]);
			len -= 4;
			i += 4;
		}
		while (len >= 1) {
			dprint_i("%02x ", c[i]);
			len -= 1;
			i += 1;
		}
	}
}

static void _sec_cb(u8 *sec, int len, void *data)
{
	struct dmx_section_filter *source_filter =
	    (struct dmx_section_filter *)data;
	struct sw_demux_sec_feed *sec_feed =
	    (struct sw_demux_sec_feed *)source_filter->parent;
	struct dmxdev_filter *dmxdevfilter = source_filter->priv;
	ssize_t free;

	if (sec_feed->state != DMX_STATE_GO)
		return;

	spin_lock(&dmxdevfilter->dev->lock);
	free = dvb_ringbuffer_free(&dmxdevfilter->buffer);
	if (len > free) {
		pr_info("dvb-core: pid:0x%0x sec buffer isn't enough\n",
			sec_feed->pid);
		spin_unlock(&dmxdevfilter->dev->lock);
		return;
	}
	spin_unlock(&dmxdevfilter->dev->lock);

	if (sec_feed->sec_cb)
		sec_feed->sec_cb(sec, len, NULL, 0, source_filter);
}

static int _ts_out_sec_cb(struct out_elem *pout, char *buf,
			  int count, void *udata, int req_len, int *req_ret)
{
	struct sw_demux_sec_feed *sec_feed = (struct sw_demux_sec_feed *)udata;
	struct aml_dmx *demux =
	    (struct aml_dmx *)sec_feed->sec_feed.parent->priv;
	int ret = 0;

//      dprint("%s\n", __func__);
	if (debug_dmx == 1) {
		pr_dbg("%s len:%d\n", __func__, count);
		prdump("org", buf, 4);
	}
	if (sec_feed->state != DMX_STATE_GO)
		return 0;

//      if (mutex_lock_interruptible(demux->pmutex))
//              return 0;
	ret = swdmx_ts_parser_run(demux->tsp, buf, count);
//      mutex_unlock(demux->pmutex);
	return ret;
}

static int _dmx_ts_feed_set(struct dmx_ts_feed *ts_feed, u16 pid, int ts_type,
			    enum dmx_ts_pes pes_type,
			    size_t circular_buffer_size, ktime_t timeout)
{
	struct sw_demux_ts_feed *feed = (struct sw_demux_ts_feed *)ts_feed;
	struct aml_dmx *demux = (struct aml_dmx *)ts_feed->parent->priv;
	struct dmxdev_filter *filter = ts_feed->priv;
	int sid = 0;
	int format = 0;
	int pcr_num = 0;
	int sec_level = 0;
	enum content_type type = 0;
	int output_mode = 0;
	int mem_size = TS_OUTPUT_CHAN_PES_BUF_SIZE;
	int media_type = 0;
	int cb_id = 0;
	int pts_level = 0;
	int ret = 0;

	pr_dbg("%s pid:0x%0x\n", __func__, pid);

	if (pid >= SWDMX_MAX_PID && pid != 0x2000)
		return -EINVAL;

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	if (ts_type & TS_DECODER) {
		if (pes_type >= DMX_PES_OTHER) {
			mutex_unlock(demux->pmutex);
			return -EINVAL;
		}
	}

	feed->pid = pid;
	feed->ts_type = ts_type;
	feed->pes_type = pes_type;
	feed->state = DMX_STATE_READY;

//      feed->buffer_size = circular_buffer_size;
//      if (feed->buffer_size) {
//              feed->buffer = vmalloc(feed->buffer_size);
//              if (!feed->buffer) {
//                      mutex_unlock(&demux->mutex);
//                      return -ENOMEM;
//              }
//      }

	if (demux->source != INPUT_DEMOD)
		sid = demux->local_sid;
	else
		sid = demux->demod_sid;

	if (pes_type == DMX_PES_PCR0 ||
	    pes_type == DMX_PES_PCR1 ||
	    pes_type == DMX_PES_PCR2 || pes_type == DMX_PES_PCR3) {
		pr_dbg("%s PCR\n", __func__);
		goto HANDLE_PCR;
	}
	if (pes_type == DMX_PES_AUDIO0 ||
	    pes_type == DMX_PES_AUDIO1 ||
	    pes_type == DMX_PES_AUDIO2 || pes_type == DMX_PES_AUDIO3) {
		type = AUDIO_TYPE;
		mem_size = audio_buf_size;
		media_type = (filter->params.pes.flags >>
		DMX_AUDIO_FORMAT_BIT) & 0xFF;
	} else if (pes_type == DMX_PES_VIDEO0 ||
		   pes_type == DMX_PES_VIDEO1 ||
		   pes_type == DMX_PES_VIDEO2 || pes_type == DMX_PES_VIDEO3) {
		type = VIDEO_TYPE;
		mem_size = video_buf_size;
	} else if (pes_type == DMX_PES_SUBTITLE0 ||
		   pes_type == DMX_PES_SUBTITLE1 ||
		   pes_type == DMX_PES_SUBTITLE2 ||
		   pes_type == DMX_PES_SUBTITLE3) {
		type = SUB_TYPE;
		media_type = MEDIA_PES_SUB;
	} else if (pes_type == DMX_PES_TELETEXT0 ||
		   pes_type == DMX_PES_TELETEXT1 ||
		   pes_type == DMX_PES_TELETEXT2 ||
		   pes_type == DMX_PES_TELETEXT3) {
		type = TTX_TYPE;
		mem_size = pes_buf_size;
		media_type = MEDIA_PES_SUB;
	} else {
		type = OTHER_TYPE;
		mem_size = pes_buf_size;
	}
	if (filter->params.pes.flags & DMX_ES_OUTPUT) {
		format = ES_FORMAT;
		pr_dbg("%s ES_FORMAT\n", __func__);
		if (filter->params.pes.flags & DMX_OUTPUT_RAW_MODE) {
			output_mode = 1;
			pr_dbg("%s DMX_OUTPUT_RAW_MODE\n", __func__);
		}
	} else {
		if (filter->params.pes.output == DMX_OUT_TAP ||
			filter->params.pes.output == DMX_OUT_TSDEMUX_TAP) {
			format = PES_FORMAT;
			pr_dbg("%s PES_FORMAT\n", __func__);
		} else {
			format = DVR_FORMAT;
			mem_size = dvr_buf_size;
			pr_dbg("%s DVR_FORMAT\n", __func__);
		}
	}

	sec_level = (filter->params.pes.flags >> 10) & 0x7;

	feed->type = type;
	feed->format = format;
	pr_dbg("%s sec_level:%d\n", __func__, sec_level);
	if (type != VIDEO_TYPE && sec_level != 0) {
		if (aml_aucpu_strm_get_load_firmware_status() != 0) {
			dprint("load aucpu firmware fail, don't use aucpu\n");
			sec_level = 0;
			pts_level = 0;
		} else {
			pts_level = sec_level;
		}
	} else {
		if (aml_aucpu_strm_get_load_firmware_status() != 0)
			pts_level = 0;
		else
			pts_level = sec_level;
	}

	if (format == DVR_FORMAT) {
		feed->ts_out_elem = ts_output_find_dvr(sid);
		if (feed->ts_out_elem) {
			pr_dbg("find same pid elem:0x%lx\n",
			       (unsigned long)(feed->ts_out_elem));
			demux->dvr_ts_output = feed->ts_out_elem;

			if (feed->pid == 0x2000)
				ts_output_add_pid(feed->ts_out_elem, feed->pid,
						0x1fff, demux->id, &cb_id);
			else
				ts_output_add_pid(feed->ts_out_elem, feed->pid,
						0, demux->id, &cb_id);
			ts_output_add_cb(feed->ts_out_elem,
					 out_ts_elem_cb, feed, cb_id,
					 format, 0, demux->id);
			feed->cb_id = cb_id;
			mutex_unlock(demux->pmutex);
			return 0;
		}
	}

	feed->ts_out_elem = ts_output_open(
		sid, demux->id, format, type, media_type, output_mode);
	if (feed->ts_out_elem) {
		if (format == DVR_FORMAT) {
			demux->dvr_ts_output = feed->ts_out_elem;
		}
		if (demux->sec_dvr_size != 0 && format == DVR_FORMAT) {
			ts_output_set_sec_mem(feed->ts_out_elem,
				demux->sec_dvr_buff, demux->sec_dvr_size);
			demux->sec_dvr_size = 0;
			sec_level = 1;
		}
		if (sec_level != 0)
			ret = ts_output_set_mem(feed->ts_out_elem,
					mem_size, sec_level,
					TS_OUTPUT_CHAN_PTS_SEC_BUF_SIZE,
					pts_level);
		else
			ret = ts_output_set_mem(feed->ts_out_elem,
					mem_size, sec_level,
					TS_OUTPUT_CHAN_PTS_BUF_SIZE, pts_level);
		if (ret != 0) {
			ts_output_close(feed->ts_out_elem);
			feed->ts_out_elem = NULL;
			mutex_unlock(demux->pmutex);
			return -1;
		}
		if (feed->pid == 0x2000)
			ts_output_add_pid(feed->ts_out_elem, feed->pid, 0x1fff,
					  demux->id, &cb_id);
		else
			ts_output_add_pid(feed->ts_out_elem, feed->pid, 0,
					  demux->id, &cb_id);
		ts_output_add_cb(feed->ts_out_elem, out_ts_elem_cb, feed,
			cb_id, format, 0, demux->id);
		feed->cb_id = cb_id;
	} else {
		dprint("%s error\n", __func__);
	}

	if (feed->type == VIDEO_TYPE) {
		demux->video_pid = feed->pid;
		demux->reset_init = 0;
	}

	mutex_unlock(demux->pmutex);
	return 0;

HANDLE_PCR:
	{
		int i = 0;

		for (i = 0; i < MAX_PCR_NUM; i++) {
			if (pcr_flag[i] == 0)
				break;
		}
		if (i == MAX_PCR_NUM) {
			dprint("%s error pcr full\n", __func__);
			mutex_unlock(demux->pmutex);
			return -1;
		}
		if (pes_type == DMX_PES_PCR0)
			pcr_num = 0;
		else if (pes_type == DMX_PES_PCR1)
			pcr_num = 1;
		else if (pes_type == DMX_PES_PCR2)
			pcr_num = 2;
		else
			pcr_num = 3;

		demux->pcr_index[pcr_num] = i;
		ts_output_set_pcr(sid, i, pid);
		jiffies_pcr_record[i].last_pcr = 0;
		jiffies_pcr_record[i].last_time = 0;
		pcr_flag[i] = 1;
		feed->type = OTHER_TYPE;
	}
	mutex_unlock(demux->pmutex);
	return 0;
}

static int _dmx_ts_feed_start_filtering(struct dmx_ts_feed *ts_feed)
{
	struct sw_demux_ts_feed *feed = (struct sw_demux_ts_feed *)ts_feed;
	struct aml_dmx *demux = (struct aml_dmx *)ts_feed->parent->priv;
//      swdmx_tsfilter_params  tsfp;

	pr_dbg("%s\n", __func__);
	if (mutex_lock_interruptible(demux->pmutex)) {
		dprint("%s line:%d\n", __func__, __LINE__);
		return -ERESTARTSYS;
	}

	if (feed->state != DMX_STATE_READY || feed->type == NONE_TYPE) {
		mutex_unlock(demux->pmutex);
		return -EINVAL;
	}
//      tsfp.pid = feed->pid;
//      swdmx_ts_filter_set_params(feed->tsf, &tsfp);
//      swdmx_ts_filter_add_ts_packet_cb(feed->tsf, _ts_pkt_cb, ts_feed);

//      if (swdmx_ts_filter_enable(feed->tsf) != SWDMX_OK) {
//              mutex_unlock(demux->pmutex);
//              return -EINVAL;
//      }
	spin_lock_irq(demux->pslock);
	ts_feed->is_filtering = 1;
	feed->state = DMX_STATE_GO;
	spin_unlock_irq(demux->pslock);
	mutex_unlock(demux->pmutex);

	return 0;
}

static int _dmx_ts_feed_stop_filtering(struct dmx_ts_feed *ts_feed)
{
	struct sw_demux_ts_feed *feed = (struct sw_demux_ts_feed *)ts_feed;
	struct aml_dmx *demux = (struct aml_dmx *)ts_feed->parent->priv;

	pr_dbg("%s\n", __func__);
	mutex_lock(demux->pmutex);

	if (feed->state < DMX_STATE_GO) {
		mutex_unlock(demux->pmutex);
		return -EINVAL;
	}

	spin_lock_irq(demux->pslock);
	ts_feed->is_filtering = 0;
	feed->state = DMX_STATE_ALLOCATED;
	spin_unlock_irq(demux->pslock);
	mutex_unlock(demux->pmutex);

	return 0;
}

static int _dmx_section_feed_allocate_filter(struct dmx_section_feed *feed,
					     struct dmx_section_filter **filter)
{
	struct aml_dmx *demux = (struct aml_dmx *)feed->parent->priv;
	struct sw_demux_sec_filter *sec_filter;

	pr_dbg("%s enter\n", __func__);

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	sec_filter = _dmx_dmx_sec_filter_alloc((struct sw_demux_sec_feed *)
					       feed);
	if (!sec_filter) {
		mutex_unlock(demux->pmutex);
		return -EBUSY;
	}
	sec_filter->secf = swdmx_demux_alloc_sec_filter(demux->swdmx);
	if (!sec_filter->secf) {
		sec_filter->state = DMX_STATE_FREE;
		mutex_unlock(demux->pmutex);
		return -EBUSY;
	}

	spin_lock_irq(demux->pslock);
	*filter = &sec_filter->section_filter;
	(*filter)->parent = feed;
//      (*filter)->priv = sec_filter;
	spin_unlock_irq(demux->pslock);
	mutex_unlock(demux->pmutex);
	pr_dbg("%s exit\n", __func__);
	return 0;
}

static int _dmx_section_feed_set(struct dmx_section_feed *feed,
				 u16 pid, size_t circular_buffer_size,
				 int check_crc)
{
	struct sw_demux_sec_feed *sec_feed = (struct sw_demux_sec_feed *)feed;
	struct aml_dmx *demux = (struct aml_dmx *)feed->parent->priv;

	pr_dbg("%s\n", __func__);

	if (pid >= SWDMX_MAX_PID)
		return -EINVAL;

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	sec_feed->pid = pid;
	sec_feed->check_crc = check_crc;
	sec_feed->type = SEC_TYPE;
	sec_feed->state = DMX_STATE_READY;
	mutex_unlock(demux->pmutex);
	return 0;
}

static int _dmx_section_add_filter(struct sw_demux_sec_feed *sec_feed, int i)
{
	struct swdmx_secfilter_params params;

	params.pid = sec_feed->pid;
	params.crc32 = sec_feed->check_crc;

	memcpy(&sec_feed->filter[i].section_filter.filter_value[1],
	       &sec_feed->filter[i].section_filter.filter_value[3],
	       SWDMX_SEC_FILTER_LEN - 1);
	memcpy(&sec_feed->filter[i].section_filter.filter_mask[1],
	       &sec_feed->filter[i].section_filter.filter_mask[3],
	       SWDMX_SEC_FILTER_LEN - 1);
	memcpy(&sec_feed->filter[i].section_filter.filter_mode[1],
	       &sec_feed->filter[i].section_filter.filter_mode[3],
	       SWDMX_SEC_FILTER_LEN - 1);

	_invert_mode(&sec_feed->filter[i].section_filter);

	memcpy(params.value,
	       sec_feed->filter[i].section_filter.filter_value,
	       SWDMX_SEC_FILTER_LEN);
	memcpy(params.mask,
	       sec_feed->filter[i].section_filter.filter_mask,
	       SWDMX_SEC_FILTER_LEN);
	memcpy(params.mode,
	       sec_feed->filter[i].section_filter.filter_mode,
	       SWDMX_SEC_FILTER_LEN);

	if (swdmx_sec_filter_set_params(sec_feed->filter[i].secf,
					&params) != SWDMX_OK)
		return -1;

	swdmx_sec_filter_add_section_cb(sec_feed->filter[i].secf, _sec_cb,
					&sec_feed->filter[i].section_filter);

	if (swdmx_sec_filter_enable(sec_feed->filter[i].secf) != SWDMX_OK)
		return -1;
	return 0;
}

static int _dmx_section_feed_start_filtering(struct dmx_section_feed *feed)
{
	struct sw_demux_sec_feed *sec_feed = (struct sw_demux_sec_feed *)feed;
	struct aml_dmx *demux = (struct aml_dmx *)feed->parent->priv;
	int i = 0;
	int start_flag = 0;
	struct dmx_section_filter *sec_filter;
	struct dmxdev_filter *filter;
	int sec_level = 0;
	int sid = 0;
	int mem_size = 0;
	int cb_id = 0;
	int ret = 0;

	pr_dbg("%s dmx id:%d\n", __func__, demux->id);
	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	if (feed->is_filtering) {
		mutex_unlock(demux->pmutex);
		return -EBUSY;
	}
	for (i = 0; i < MAX_FILTER_PER_SEC_FEED; i++) {
		if (sec_feed->filter[i].state == DMX_STATE_ALLOCATED) {
			if (_dmx_section_add_filter(sec_feed, i) != 0)
				continue;

			sec_feed->filter[i].state = DMX_STATE_GO;
			start_flag = 1;
		} else if (sec_feed->filter[i].state == DMX_STATE_READY) {
			if (swdmx_sec_filter_enable(sec_feed->filter[i].secf) !=
			    SWDMX_OK)
				continue;

			sec_feed->filter[i].state = DMX_STATE_GO;
			start_flag = 1;
		}
	}
	if (start_flag != 1) {
		dprint("%s fail\n", __func__);
		mutex_unlock(demux->pmutex);
		return -1;
	}

	sec_feed->state = DMX_STATE_GO;

	spin_lock_irq(demux->pslock);
	feed->is_filtering = 1;
	spin_unlock_irq(demux->pslock);

	if (sec_feed->sec_out_elem) {
		mutex_unlock(demux->pmutex);
		return 0;
	}
	if (demux->source != INPUT_DEMOD)
		sid = demux->local_sid;
	else
		sid = demux->demod_sid;

	if (sec_feed->sec_out_elem)
		pr_dbg("pid elem:0x%lx exist\n",
		       (unsigned long)(sec_feed->sec_out_elem));

	sec_feed->sec_out_elem = ts_output_find_same_section_pid(
		sid, sec_feed->pid);
	if (sec_feed->sec_out_elem) {
		pr_dbg("find same pid elem:0x%lx\n",
		       (unsigned long)(sec_feed->sec_out_elem));
		ts_output_add_cb(sec_feed->sec_out_elem,
				 _ts_out_sec_cb, sec_feed, demux->id,
				 SECTION_FORMAT, 1, demux->id);
		mutex_unlock(demux->pmutex);
		return 0;
	}
	/*find the section filter flags for sec_level*/
	for (i = 0; i < sec_feed->sec_filter_num; i++) {
		if (sec_feed->filter[i].state != DMX_STATE_FREE) {
			sec_filter = (struct dmx_section_filter *)
			&sec_feed->filter[i].section_filter;
			filter = (struct dmxdev_filter *)sec_filter->priv;

			sec_level = (filter->params.sec.flags >> 10) & 0x7;
			break;
		}
	}
	if (sec_level != 0) {
		if (aml_aucpu_strm_get_load_firmware_status() != 0) {
			dprint("load aucpu firmware fail, don't use aucpu\n");
			sec_level = 0;
		}
	}
	sec_feed->sec_out_elem = ts_output_open(
		sid, demux->id, SECTION_FORMAT, SEC_TYPE, MEDIA_TS_SYS, 0);
	if (sec_feed->sec_out_elem) {
		mem_size = sec_buf_size;
		ret = ts_output_set_mem(sec_feed->sec_out_elem,
				mem_size, sec_level, 0, 0);
		if (ret != 0) {
			ts_output_close(sec_feed->sec_out_elem);
			sec_feed->sec_out_elem = NULL;
			mutex_unlock(demux->pmutex);
			return -1;
		}
		ts_output_add_pid(sec_feed->sec_out_elem, sec_feed->pid, 0,
				  demux->id, &cb_id);
		ts_output_add_cb(sec_feed->sec_out_elem,
				 _ts_out_sec_cb, sec_feed, cb_id,
				 SECTION_FORMAT, 1, demux->id);
	}
	pr_dbg("sec_out_elem:0x%lx\n", (unsigned long)(sec_feed->sec_out_elem));
	mutex_unlock(demux->pmutex);

	return 0;
}

static int _dmx_section_feed_stop_filtering(struct dmx_section_feed *feed)
{
	struct sw_demux_sec_feed *sec_feed = (struct sw_demux_sec_feed *)feed;
	struct aml_dmx *demux = (struct aml_dmx *)feed->parent->priv;
	int i = 0;
	int start_flag = 0;

	pr_dbg("%s dmx id:%d\n", __func__, demux->id);

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	if (feed->is_filtering == 0) {
		mutex_unlock(demux->pmutex);
		return -EINVAL;
	}
	for (i = 0; i < MAX_FILTER_PER_SEC_FEED; i++) {
		if (sec_feed->filter[i].state == DMX_STATE_GO) {
			if (swdmx_sec_filter_disable(sec_feed->filter[i].secf)
			    != SWDMX_OK)
				continue;

			sec_feed->filter[i].state = DMX_STATE_READY;
			start_flag = 1;
		}
	}
	if (start_flag != 1) {
		dprint("%s no found start filter\n", __func__);
		mutex_unlock(demux->pmutex);
		return 0;
	}

	spin_lock_irq(demux->pslock);
	feed->is_filtering = 0;
	spin_unlock_irq(demux->pslock);
	mutex_unlock(demux->pmutex);

	return 0;
}

static int _dmx_section_feed_release_filter(struct dmx_section_feed *feed,
					    struct dmx_section_filter *filter)
{
	struct sw_demux_sec_feed *sec_feed = (struct sw_demux_sec_feed *)feed;
	struct aml_dmx *demux = (struct aml_dmx *)feed->parent->priv;
	int i = 0;

	pr_dbg("%s dmx id:%d\n", __func__, demux->id);

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	if (sec_feed->type != SEC_TYPE) {
		mutex_unlock(demux->pmutex);
		return -EINVAL;
	}
	for (i = 0; i < MAX_FILTER_PER_SEC_FEED; i++) {
		if (sec_feed->filter[i].state != DMX_STATE_FREE &&
		    (&sec_feed->filter[i].section_filter) == filter) {
			swdmx_sec_filter_free(sec_feed->filter[i].secf);
			sec_feed->filter[i].secf = NULL;
			memset(filter, 0, sizeof(struct dmx_section_filter));
			sec_feed->filter[i].state = DMX_STATE_FREE;
			break;
		}
	}

	mutex_unlock(demux->pmutex);

	return 0;
}

int check_dmx_filter_buff(struct dmx_ts_feed *feed, int req_len)
{
	struct dmxdev_filter *dmxdevfilter = feed->priv;
	ssize_t free;
	struct dvb_ringbuffer *buffer;

	spin_lock(&dmxdevfilter->dev->lock);
	if (dmxdevfilter->params.pes.output == DMX_OUT_DECODER) {
		spin_unlock(&dmxdevfilter->dev->lock);
		return 0;
	}

	if (dmxdevfilter->params.pes.output == DMX_OUT_TAP ||
		dmxdevfilter->params.pes.output == DMX_OUT_TSDEMUX_TAP)
		buffer = &dmxdevfilter->buffer;
	else
		buffer = &dmxdevfilter->dev->dvr_buffer;
	if (buffer->error) {
		spin_unlock(&dmxdevfilter->dev->lock);
		return 0;
	}
	free = dvb_ringbuffer_free(buffer);
	if (req_len > free) {
		pr_info("dvb-core: buffer isn't enough\n");
		spin_unlock(&dmxdevfilter->dev->lock);
		return 1;
	}
	spin_unlock(&dmxdevfilter->dev->lock);
	return 0;
}

static int out_ts_elem_cb(struct out_elem *pout, char *buf,
			  int count, void *udata, int req_len, int *req_ret)
{
	struct dmx_ts_feed *source_feed = (struct dmx_ts_feed *)udata;
	struct sw_demux_ts_feed *ts_feed = (struct sw_demux_ts_feed *)udata;

	if (ts_feed->state != DMX_STATE_GO)
		return 0;

	/*if found dvb-core buff isn't enough, return*/
	if (req_len && req_ret) {
		*req_ret = check_dmx_filter_buff(source_feed, req_len);
		if (*req_ret == 1)
			return 0;
	}
	if (ts_feed->ts_cb)
		ts_feed->ts_cb(buf, count, NULL, 0, source_feed);
	return count;
}

static int _dmx_allocate_ts_feed(struct dmx_demux *dmx,
				 struct dmx_ts_feed **ts_feed,
				 dmx_ts_cb callback)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;
	struct sw_demux_ts_feed *feed;

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	feed = _dmx_ts_feed_alloc(demux);
	if (!feed) {
		mutex_unlock(demux->pmutex);
		dprint("%s alloc fail\n", __func__);
		return -EBUSY;
	}

	feed->type = NONE_TYPE;
	feed->ts_cb = callback;
	feed->ts_out_elem = NULL;
	feed->pes_type = -1;
	feed->pid = -1;

	(*ts_feed) = &feed->ts_feed;
	(*ts_feed)->parent = dmx;
//      (*ts_feed)->priv = feed;
	(*ts_feed)->is_filtering = 0;
	(*ts_feed)->start_filtering = _dmx_ts_feed_start_filtering;
	(*ts_feed)->stop_filtering = _dmx_ts_feed_stop_filtering;
	(*ts_feed)->set = _dmx_ts_feed_set;

	mutex_unlock(demux->pmutex);

	return 0;
}

static int _dmx_release_ts_feed(struct dmx_demux *dmx,
				struct dmx_ts_feed *ts_feed)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;
	struct sw_demux_ts_feed *feed;
	int sid;
	int pcr_num = 0;
	int pcr_index = 0;
	int ret = 0;

	if (!ts_feed)
		return 0;

	feed = (struct sw_demux_ts_feed *)ts_feed;

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	if (feed->type == VIDEO_TYPE) {
		demux->video_pid = -1;
		demux->reset_init = -1;
	}

	if (feed->ts_out_elem) {
		pr_dbg("%s pid:%d\n", __func__, feed->pid);
		ts_output_remove_pid(feed->ts_out_elem, feed->pid);
		ts_output_remove_cb(feed->ts_out_elem,
				out_ts_elem_cb, feed, feed->cb_id, 0);
		if (feed->format == DVR_FORMAT) {
			ret = ts_output_close(feed->ts_out_elem);
			if (ret == 0)
				demux->dvr_ts_output = NULL;
		} else {
			ts_output_close(feed->ts_out_elem);
		}
	}

	switch (feed->pes_type) {
	case DMX_PES_PCR0:
	case DMX_PES_PCR1:
	case DMX_PES_PCR2:
	case DMX_PES_PCR3:
		if (demux->source != INPUT_DEMOD)
			sid = demux->local_sid;
		else
			sid = demux->demod_sid;

		if (feed->pes_type == DMX_PES_PCR0)
			pcr_num = 0;
		else if (feed->pes_type == DMX_PES_PCR1)
			pcr_num = 1;
		else if (feed->pes_type == DMX_PES_PCR2)
			pcr_num = 2;
		else
			pcr_num = 3;

		pcr_index = demux->pcr_index[pcr_num];
		if (pcr_index >= 0 && pcr_index < MAX_PCR_NUM) {
			ts_output_set_pcr(sid, pcr_index, -1);
			jiffies_pcr_record[pcr_index].last_pcr = 0;
			jiffies_pcr_record[pcr_index].last_time = 0;
			pcr_flag[pcr_index] = 0;
			demux->pcr_index[pcr_num] = -1;
		}
		break;
	default:
		break;
	}

	feed->state = DMX_STATE_FREE;
	feed->ts_out_elem = NULL;

	mutex_unlock(demux->pmutex);
	return 0;
}

static int _dmx_allocate_section_feed(struct dmx_demux *dmx,
				      struct dmx_section_feed **feed,
				      dmx_section_cb callback)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;
	struct sw_demux_sec_feed *sec_feed;
	int i;

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	sec_feed = _dmx_section_feed_alloc(demux);
	if (!sec_feed) {
		mutex_unlock(demux->pmutex);
		return -EBUSY;
	}

	sec_feed->sec_filter_num = MAX_FILTER_PER_SEC_FEED;
	sec_feed->filter = vmalloc(sizeof(*sec_feed->filter) *
				   sec_feed->sec_filter_num);
	if (!sec_feed->filter) {
		sec_feed->state = DMX_STATE_FREE;
		mutex_unlock(demux->pmutex);
		return -EBUSY;
	}
	for (i = 0; i < sec_feed->sec_filter_num; i++)
		sec_feed->filter[i].state = DMX_STATE_FREE;

	sec_feed->sec_cb = callback;
	sec_feed->type = SEC_TYPE;

	(*feed) = &sec_feed->sec_feed;
	(*feed)->parent = dmx;
//      (*feed)->priv = sec_feed;
	(*feed)->is_filtering = 0;

	(*feed)->set = _dmx_section_feed_set;
	(*feed)->allocate_filter = _dmx_section_feed_allocate_filter;
	(*feed)->start_filtering = _dmx_section_feed_start_filtering;
	(*feed)->stop_filtering = _dmx_section_feed_stop_filtering;
	(*feed)->release_filter = _dmx_section_feed_release_filter;

	sec_feed->sec_out_elem = NULL;

	pr_dbg("%s\n", __func__);

	mutex_unlock(demux->pmutex);
	return 0;
}

static int _dmx_release_section_feed(struct dmx_demux *dmx,
				     struct dmx_section_feed *feed)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;
	struct sw_demux_sec_feed *sec_feed;

	sec_feed = (struct sw_demux_sec_feed *)feed;

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	if (sec_feed->sec_out_elem) {
		ts_output_remove_pid(sec_feed->sec_out_elem, sec_feed->pid);
		ts_output_remove_cb(sec_feed->sec_out_elem, _ts_out_sec_cb,
				    sec_feed, sec_feed->cb_id, 1);
		ts_output_close(sec_feed->sec_out_elem);
	}

	sec_feed->state = DMX_STATE_FREE;
	sec_feed->sec_out_elem = NULL;

	if (sec_feed->filter)
		vfree(sec_feed->filter);

	mutex_unlock(demux->pmutex);
	pr_dbg("%s\n", __func__);
	return 0;
}

static int _dmx_add_frontend(struct dmx_demux *dmx,
			     struct dmx_frontend *frontend)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;
	struct list_head *head = &demux->frontend_list;

	list_add(&frontend->connectivity_list, head);

	return 0;
}

static int _dmx_remove_frontend(struct dmx_demux *dmx,
				struct dmx_frontend *frontend)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;
	struct list_head *pos, *n, *head = &demux->frontend_list;

	list_for_each_safe(pos, n, head) {
		if (DMX_FE_ENTRY(pos) == frontend) {
			list_del(pos);
			return 0;
		}
	}

	return -ENODEV;
}

static struct list_head *_dmx_get_frontends(struct dmx_demux *dmx)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;

	if (list_empty(&demux->frontend_list))
		return NULL;

	return &demux->frontend_list;
}

static int _dmx_connect_frontend(struct dmx_demux *dmx,
				 struct dmx_frontend *frontend)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;

	mutex_lock(demux->pmutex);

	if (dmx->frontend) {
		mutex_unlock(demux->pmutex);
		return -EINVAL;
	}
	dmx->frontend = frontend;
	mutex_unlock(demux->pmutex);
	return 0;
}

static int _dmx_disconnect_frontend(struct dmx_demux *dmx)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx;

	mutex_lock(demux->pmutex);

	dmx->frontend = NULL;
	mutex_unlock(demux->pmutex);
	return 0;
}

static int _dmx_get_pes_pids(struct dmx_demux *dmx, u16 *pids)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx;

	memcpy(pids, demux->pids, 5 * sizeof(u16));
	return 0;
}

int dmx_get_pcr(struct dmx_demux *dmx, unsigned int num, u64 *pcr)
{
	int ret = 0;
	int pcr_index = 0;
	struct aml_dmx *demux = (struct aml_dmx *)dmx;

//      mutex_lock(demux->pmutex);
	if (num >= EACH_DMX_MAX_PCR_NUM) {
//              mutex_unlock(demux->pmutex);
		dprint("dmx id:%d, num:%d invalid\n", demux->id, num);
		return 0;
	}
	pcr_index = demux->pcr_index[num];
	if (pcr_index < 0 || pcr_index > MAX_PCR_NUM) {
//              mutex_unlock(demux->pmutex);
//		dprint("invalid pcr index:%d\n", pcr_index);
		return -1;
	}
	ret = ts_output_get_pcr(pcr_index, pcr);
	if (ret != 0) {
//              mutex_unlock(demux->pmutex);
		return -1;
	}
//      mutex_unlock(demux->pmutex);
	return 0;
}

int dmx_get_stc(struct dmx_demux *dmx, unsigned int num,
		u64 *stc, unsigned int *base)
{
	int ret = 0;
	u64 jiffes_90K = 0;
	u64 cur_pcr = 0;
	int pcr_diff = 0;
	int jiffes_diff = 0;
	int pcr_index = 0;
	struct aml_dmx *demux = (struct aml_dmx *)dmx;

	mutex_lock(demux->pmutex);
//      pr_dbg("%s num:%d\n", __func__, num);
	*base = 1;
	jiffes_90K = jiffies_to_msecs(get_jiffies_64()) * 90;
	if (num >= EACH_DMX_MAX_PCR_NUM) {
		*stc = jiffes_90K;
		mutex_unlock(demux->pmutex);
		dprint("dmx id:%d, num:%d invalid\n", demux->id, num);
		return 0;
	}
	pcr_index = demux->pcr_index[num];
	if (pcr_index < 0 || pcr_index > MAX_PCR_NUM) {
		*stc = jiffes_90K;
		mutex_unlock(demux->pmutex);
		dprint("invalid pcr index:%d\n", pcr_index);
		return 0;
	}
	ret = ts_output_get_pcr(pcr_index, &cur_pcr);
//      pr_dbg("%s pcr:0x%llx, jiffes_90K:0x%llx\n",
//              __func__, cur_pcr, jiffes_90K);
	if (ret != 0) {
		*stc = jiffes_90K;
	} else {
//      pr_dbg("pcr %d last pcr:0x%llx, last time:0x%llx\n",
//              pcr_index, jiffies_pcr_record[pcr_index].last_pcr,
//              jiffies_pcr_record[pcr_index].last_time);
		if (jiffies_pcr_record[pcr_index].last_pcr == 0) {
			*stc = jiffes_90K;
			jiffies_pcr_record[pcr_index].last_pcr = cur_pcr;
			jiffies_pcr_record[pcr_index].last_time = jiffes_90K;
		} else {
			pcr_diff = cur_pcr -
			    jiffies_pcr_record[pcr_index].last_pcr;
			jiffes_diff = jiffes_90K -
			    jiffies_pcr_record[pcr_index].last_time;
//                      pr_dbg("pcr diff:0x%x, jiffes_diff:0x%x\n",
//                                      pcr_diff, jiffes_diff);
			if (jiffes_diff && (abs(pcr_diff) / jiffes_diff
					    > MAX_PCR_DIFF)) {
				jiffies_pcr_record[pcr_index].last_pcr =
				    cur_pcr;
				jiffies_pcr_record[pcr_index].last_time =
				    jiffes_90K;
				*stc = jiffes_90K;
			} else {
				if (jiffes_diff)
					*stc =
					    jiffies_pcr_record
					    [pcr_index].last_time +
					    pcr_diff / jiffes_diff *
					    jiffes_diff;
				else
					*stc = jiffes_90K;

				jiffies_pcr_record[pcr_index].last_pcr =
				    cur_pcr;
				jiffies_pcr_record[pcr_index].last_time =
				    jiffes_90K;
			}
		}
		pr_dbg("pcr %d last pcr:0x%llx, last time:0x%llx\n",
		       pcr_index, jiffies_pcr_record[pcr_index].last_pcr,
		       jiffies_pcr_record[pcr_index].last_time);
	}
	mutex_unlock(demux->pmutex);
	return 0;
}

static int _dmx_set_input(struct dmx_demux *demux, int source)
{
	struct aml_dmx *pdmx = (struct aml_dmx *)demux;
	int sec_level = 0;

	pr_dbg("%s local:%d, input:%d\n", __func__, pdmx->source, source);
//      if (pdmx->source == source)
//              return 0;
	if (mutex_lock_interruptible(pdmx->pmutex))
		return -ERESTARTSYS;
	if (source == INPUT_LOCAL || source == INPUT_LOCAL_SEC) {
		pr_dbg("%s local:%d\n", __func__, source);
		if (!pdmx->sc2_input) {
			if (source == INPUT_LOCAL_SEC)
				sec_level = 1;
			pdmx->sc2_input = ts_input_open(pdmx->id, sec_level);
			if (!pdmx->sc2_input) {
				dprint("ts_input_open fail\n");
				mutex_unlock(pdmx->pmutex);
				return -ENODEV;
			}
		} else {
			if (source != pdmx->source) {
				ts_input_close(pdmx->sc2_input);
				pdmx->sc2_input = NULL;

				if (source == INPUT_LOCAL_SEC)
					sec_level = 1;
				pdmx->sc2_input =
					ts_input_open(pdmx->id, sec_level);
				if (!pdmx->sc2_input) {
					dprint("ts_input_open fail\n");
					mutex_unlock(pdmx->pmutex);
					return -ENODEV;
				}
			}
		}
	} else {
		pr_dbg("%s remote\n", __func__);
		if (pdmx->sc2_input) {
			ts_input_close(pdmx->sc2_input);
			pdmx->sc2_input = NULL;
		}
	}
	pdmx->source = source;
	dsc_set_source(pdmx->id, source);
	mutex_unlock(pdmx->pmutex);
	return 0;
}

int _dmx_get_ts_mem_info(struct dmx_demux *dmx,
			 struct dmx_ts_feed *feed, struct dmx_mem_info *info)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;

	struct sw_demux_ts_feed *ts_feed = (struct sw_demux_ts_feed *)feed;

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;
	if (ts_feed && ts_feed->ts_out_elem)
		ts_output_get_mem_info(ts_feed->ts_out_elem,
				       &info->dmx_total_size,
				       &info->dmx_buf_phy_start,
				       &info->dmx_free_size, &info->wp_offset,
				       &info->newest_pts);
	mutex_unlock(demux->pmutex);
	return 0;
}

int _dmx_get_sec_mem_info(struct dmx_demux *dmx,
			  struct dmx_section_feed *feed,
			  struct dmx_mem_info *info)
{
	struct sw_demux_sec_feed *sec_feed;
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;

	sec_feed = (struct sw_demux_sec_feed *)feed;

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	if (sec_feed && sec_feed->sec_out_elem)
		ts_output_get_mem_info(sec_feed->sec_out_elem,
				       &info->dmx_total_size,
				       &info->dmx_buf_phy_start,
				       &info->dmx_free_size, &info->wp_offset,
				       &info->newest_pts);
	mutex_unlock(demux->pmutex);
	return 0;
}

int _dmx_get_mem_info(struct dmx_demux *dmx, struct dmx_filter_mem_info *info)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;
	int filter_num = 0;
	int i = 0, j = 0;
	int free_mem = 0;
	int total_mem = 0;
	__u64 newest_pts = 0;
	struct sw_demux_ts_feed *ts_feed;
	struct sw_demux_sec_feed *section_feed;
	struct dmxdev_filter *filter;
	unsigned int buf_phy_start;
	unsigned int wp_offset;
	struct filter_mem_info *pinfo;

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	for (i = 0; i < demux->ts_feed_num; i++) {
		if (demux->ts_feed[i].state < DMX_STATE_ALLOCATED)
			continue;

		ts_feed = &demux->ts_feed[i];
		if (ts_feed->type == NONE_TYPE ||
			ts_feed->type == SEC_TYPE ||
			ts_feed->type == OTHER_TYPE)
			continue;

		filter = ts_feed->ts_feed.priv;

		free_mem = dvb_ringbuffer_free(&filter->buffer);
		total_mem = filter->buffer.size;

		pinfo = &info->info[filter_num];

		if (ts_feed->type == VIDEO_TYPE)
			pinfo->type = DMX_VIDEO_TYPE;
		else if (ts_feed->type == AUDIO_TYPE)
			pinfo->type = DMX_AUDIO_TYPE;
		else if (ts_feed->type == SUB_TYPE)
			pinfo->type = DMX_SUBTITLE_TYPE;
		else if (ts_feed->type == TTX_TYPE)
			pinfo->type = DMX_TELETEXT_TYPE;

		pinfo->pid = ts_feed->pid;
		pinfo->filter_info.dvb_core_free_size = free_mem;
		pinfo->filter_info.dvb_core_total_size = total_mem;

		free_mem = 0;
		total_mem = 0;
		buf_phy_start = 0;
		wp_offset = 0;
		newest_pts = 0;

		ts_output_get_mem_info(ts_feed->ts_out_elem,
			   &total_mem,
			   &buf_phy_start,
			   &free_mem, &wp_offset, &newest_pts);
		pinfo->filter_info.dmx_buf_phy_start = buf_phy_start;
		pinfo->filter_info.dmx_free_size = free_mem;
		pinfo->filter_info.dmx_total_size = total_mem;
		pinfo->filter_info.wp_offset = wp_offset;
		pinfo->filter_info.newest_pts = newest_pts;
		filter_num++;
	}

	for (i = 0; i < demux->sec_feed_num; i++) {
		if (demux->section_feed[i].state < DMX_STATE_ALLOCATED)
			continue;

		section_feed = &demux->section_feed[i];

		for (j = 0; j < section_feed->sec_filter_num; j++) {
			if (section_feed->filter[j].state != DMX_STATE_GO)
				continue;

			filter = section_feed->filter[j].section_filter.priv;
			free_mem = dvb_ringbuffer_free(&filter->buffer);
			total_mem = filter->buffer.size;

			pinfo = &info->info[filter_num];
			pinfo->type = DMX_SECTION_TYPE;
			pinfo->pid = section_feed->pid;
			pinfo->filter_info.dvb_core_free_size = free_mem;
			pinfo->filter_info.dvb_core_total_size = total_mem;

			free_mem = 0;
			total_mem = 0;
			buf_phy_start = 0;
			wp_offset = 0;

			ts_output_get_mem_info(section_feed->sec_out_elem,
				   &total_mem,
				   &buf_phy_start,
				   &free_mem, &wp_offset, NULL);
			pinfo->filter_info.dmx_buf_phy_start = buf_phy_start;
			pinfo->filter_info.dmx_free_size = free_mem;
			pinfo->filter_info.dmx_total_size = total_mem;
			pinfo->filter_info.wp_offset = wp_offset;

			filter_num++;
		}
	}

	info->filter_num = filter_num;
	mutex_unlock(demux->pmutex);
	return 0;
}

static int _dmx_set_hw_source(struct dmx_demux *dmx, int hw_source)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;
	struct aml_dvb *advb = aml_get_dvb_device();

	pr_dbg("%s dmx%d source:%d\n", __func__, demux->id, hw_source);

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	if (hw_source >= DMA_0 && hw_source <= DMA_7) {
		demux->local_sid = hw_source - DMA_0;
		demux->demod_sid = -1;
		ts_output_update_filter(demux->id, demux->local_sid);
		dsc_set_sid(demux->id, INPUT_LOCAL, demux->local_sid);
	} else if (hw_source >= FRONTEND_TS0 && hw_source <= FRONTEND_TS7) {
		demux->ts_index = hw_source - FRONTEND_TS0;
		if (advb->ts[demux->ts_index].ts_sid != -1 &&
			advb->ts[demux->ts_index].ts_sid != demux->demod_sid) {
			demux->demod_sid = advb->ts[demux->ts_index].ts_sid;
			ts_output_update_filter(demux->id, demux->demod_sid);
			dsc_set_sid(demux->id, INPUT_DEMOD, demux->demod_sid);
		}
		demux->local_sid = -1;
	}
	mutex_unlock(demux->pmutex);
	return 0;
}

static int _dmx_get_hw_source(struct dmx_demux *dmx, int *hw_source)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;

	pr_dbg("%s dmx%d\n", __func__, demux->id);

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	if (demux->source == INPUT_DEMOD)
		*hw_source = demux->ts_index + FRONTEND_TS0;
	else
		*hw_source = demux->local_sid + DMA_0;

	mutex_unlock(demux->pmutex);
	return 0;
}

static int _dmx_set_sec_mem(struct dmx_demux *dmx, struct dmx_sec_mem *sec_mem)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;

	pr_dbg("%s dmx%d\n", __func__, demux->id);

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;

	demux->sec_dvr_buff = sec_mem->buff;
	demux->sec_dvr_size = sec_mem->size;
	mutex_unlock(demux->pmutex);
	return 0;
}

static int _dmx_get_dvr_mem(struct dmx_demux *dmx,
			struct dvr_mem_info *info)
{
	struct aml_dmx *demux = (struct aml_dmx *)dmx->priv;
	unsigned int total_mem = 0;
	unsigned int buf_phy_start = 0;
	unsigned int free_mem = 0;
	unsigned int wp_offset = 0;

	pr_dbg("%s dmx%d\n", __func__, demux->id);

	if (mutex_lock_interruptible(demux->pmutex))
		return -ERESTARTSYS;
	if (demux->dvr_ts_output)
		ts_output_get_mem_info(demux->dvr_ts_output,
			   &total_mem,
			   &buf_phy_start,
			   &free_mem, &wp_offset, NULL);

	info->wp_offset = wp_offset;
	mutex_unlock(demux->pmutex);
	return 0;
}

void dmx_init_hw(int sid_num, int *sid_info)
{
	ts_output_init(sid_num, sid_info);
	ts_input_init();
	SC2_bufferid_init();
	memset(pcr_flag, 0, sizeof(pcr_flag));
	memset(jiffies_pcr_record, 0, sizeof(jiffies_pcr_record));
}

int dmx_init(struct aml_dmx *pdmx, struct dvb_adapter *dvb_adapter)
{
	int ret;
	int i = 0;

	pdmx->dmx.capabilities =
	    (DMX_TS_FILTERING | DMX_SECTION_FILTERING |
	     DMX_MEMORY_BASED_FILTERING);
	pdmx->dmx.priv = pdmx;

	pdmx->ts_feed_num = MAX_TS_FEED_NUM;
	pdmx->ts_feed = vmalloc(sizeof(*pdmx->ts_feed) * pdmx->ts_feed_num);
	if (!pdmx->ts_feed)
		return -ENOMEM;

	for (i = 0; i < pdmx->ts_feed_num; i++)
		pdmx->ts_feed[i].state = DMX_STATE_FREE;

	pdmx->sec_feed_num = MAX_SEC_FEED_NUM;
	pdmx->section_feed = vmalloc(sizeof(*pdmx->section_feed) *
				     pdmx->sec_feed_num);
	if (!pdmx->section_feed) {
		vfree(pdmx->ts_feed);
		pdmx->ts_feed = NULL;
		return -ENOMEM;
	}

	for (i = 0; i < pdmx->sec_feed_num; i++)
		pdmx->section_feed[i].state = DMX_STATE_FREE;

	INIT_LIST_HEAD(&pdmx->frontend_list);

	for (i = 0; i < DMX_PES_OTHER; i++)
		pdmx->pids[i] = 0xffff;

	for (i = 0; i < EACH_DMX_MAX_PCR_NUM; i++)
		pdmx->pcr_index[i] = 0xffff;

	pdmx->dmx.open = _dmx_open;
	pdmx->dmx.close = _dmx_close;
	pdmx->dmx.write = _dmx_write_from_user;
	pdmx->dmx.allocate_ts_feed = _dmx_allocate_ts_feed;
	pdmx->dmx.release_ts_feed = _dmx_release_ts_feed;
	pdmx->dmx.allocate_section_feed = _dmx_allocate_section_feed;
	pdmx->dmx.release_section_feed = _dmx_release_section_feed;

	pdmx->dmx.add_frontend = _dmx_add_frontend;
	pdmx->dmx.remove_frontend = _dmx_remove_frontend;
	pdmx->dmx.get_frontends = _dmx_get_frontends;
	pdmx->dmx.connect_frontend = _dmx_connect_frontend;
	pdmx->dmx.disconnect_frontend = _dmx_disconnect_frontend;
	pdmx->dmx.get_pes_pids = _dmx_get_pes_pids;
	pdmx->dmx.get_stc = dmx_get_stc;
	pdmx->dmx.set_input = _dmx_set_input;
	pdmx->dmx.get_sec_mem_info = _dmx_get_sec_mem_info;
	pdmx->dmx.get_ts_mem_info = _dmx_get_ts_mem_info;
	pdmx->dmx.set_hw_source = _dmx_set_hw_source;
	pdmx->dmx.get_hw_source = _dmx_get_hw_source;
	pdmx->dmx.get_dmx_mem_info = _dmx_get_mem_info;
	pdmx->dmx.set_sec_mem = _dmx_set_sec_mem;
	pdmx->dmx.get_dvr_mem = _dmx_get_dvr_mem;
	pdmx->dev.filternum = (MAX_TS_FEED_NUM + MAX_SEC_FEED_NUM);
	pdmx->dev.demux = &pdmx->dmx;
	pdmx->dev.capabilities = DMXDEV_CAP_DUPLEX;
	ret = dvb_dmxdev_init(&pdmx->dev, dvb_adapter);
	if (ret < 0) {
		dprint("dvb_dmxdev_init failed: error %d\n", ret);
		vfree(pdmx->ts_feed);
		return -1;
	}
	pdmx->dev.dvr_dvbdev->writers = MAX_SW_DEMUX_USERS;

	pdmx->mem_fe.source = DMX_MEMORY_FE;
	ret = pdmx->dmx.add_frontend(&pdmx->dmx, &pdmx->mem_fe);
	if (ret < 0) {
		dprint("dvb_dmxdev_init add frontend: error %d\n", ret);
		vfree(pdmx->ts_feed);
		return -1;
	}
	pdmx->dmx.connect_frontend(&pdmx->dmx, &pdmx->mem_fe);
	if (ret < 0) {
		dprint("dvb_dmxdev_init connect frontend: error %d\n", ret);
		vfree(pdmx->ts_feed);
		return -1;
	}
	pdmx->buf_warning_level = 60;
	pdmx->init = 1;
	pdmx->sec_dvr_size = 0;
	pdmx->reset_init = -1;
	pdmx->video_pid = -1;
//      dvb_net_init(dvb_adapter, &dmx->dvb_net, &pdmx->dmx);

	return 0;
}

int dmx_destroy(struct aml_dmx *pdmx)
{
	if (pdmx->init) {
		vfree(pdmx->ts_feed);
		vfree(pdmx->section_feed);
		if (pdmx->sc2_input) {
			ts_input_close(pdmx->sc2_input);
			pdmx->sc2_input = NULL;
		}
		dvb_dmxdev_release(&pdmx->dev);
		pdmx->init = 0;
	}
	return 0;
}

#ifdef OPEN_REGISTER_NODE
static int reg_addr;

static ssize_t register_addr_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "%x\n", reg_addr);
	return ret;
}

static ssize_t register_addr_store(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t size)
{
	int addr = 0;
	/*addr = simple_strtol(buf, 0, 16); */
	long value = 0;

	if (kstrtol(buf, 0, &value) == 0)
		addr = (int)value;
	reg_addr = addr;
	return size;
}

static ssize_t register_value_show(struct class *class,
				   struct class_attribute *attr, char *buf)
{
	int ret, value;

	value = READ_CBUS_REG(reg_addr);
	ret = sprintf(buf, "%x\n", value);
	return ret;
}

static ssize_t register_value_store(struct class *class,
				    struct class_attribute *attr,
				    const char *buf, size_t size)
{
	int value = 0;
	/*value = simple_strtol(buf, 0, 16); */
	long val = 0;

	if (kstrtol(buf, 0, &val) == 0)
		value = (int)val;
	WRITE_CBUS_REG(reg_addr, value);
	return size;
}
#endif

static ssize_t dump_register_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	sc2_dump_register();
	return 0;
}

static ssize_t dump_register_store(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t size)
{
	return size;
}

static ssize_t dump_filter_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	ssize_t size;
	struct aml_dvb *advb = aml_get_dvb_device();

	if (mutex_lock_interruptible(&advb->mutex))
		return -ERESTARTSYS;

	size = ts_output_dump_info(buf);
	mutex_unlock(&advb->mutex);
	return size;
}

static ssize_t dump_filter_store(struct class *class,
				 struct class_attribute *attr,
				 const char *buf, size_t size)
{
	return size;
}

static ssize_t cache_status_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	ssize_t size;
	struct aml_dvb *advb = aml_get_dvb_device();

	if (mutex_lock_interruptible(&advb->mutex))
		return -ERESTARTSYS;

	size = cache_status_info(buf);
	mutex_unlock(&advb->mutex);
	return size;
}

static ssize_t cache_status_store(struct class *class,
				 struct class_attribute *attr,
				 const char *buf, size_t size)
{
	int cache0_count = 0;
	int cache1_count = 0;
	int ret = 0;

	struct aml_dvb *advb = aml_get_dvb_device();

	if (mutex_lock_interruptible(&advb->mutex))
		return -ERESTARTSYS;

	dprint_i("%s\n", buf);
	if (!strncmp(buf, "clear", 5)) {
		cache_clear();
	} else if (!strncmp(buf, "adjust", 6)) {
		ret = sscanf(buf + 7, "%d %d", &cache0_count, &cache1_count);
		if (ret == 2 && cache0_count > 0 && cache1_count > 0)
			cache_adjust(cache0_count, cache1_count);
	}
	mutex_unlock(&advb->mutex);
	return size;
}

static ssize_t dump_av_level_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	struct aml_dvb *advb = aml_get_dvb_device();
	struct dmx_filter_mem_info info;
	int i = 0, h;
	int r, total = 0;
	struct filter_mem_info *fpinfo;

	for (h = 0; h < DMX_DEV_COUNT; h++) {
		if (!advb->dmx[h].swdmx)
			continue;

		memset(&info, 0, sizeof(struct dmx_filter_mem_info));
		_dmx_get_mem_info(&advb->dmx[h].dmx, &info);
		for (i = 0; i < info.filter_num; i++) {
			if (info.info[i].type != DMX_VIDEO_TYPE &&
				info.info[i].type != DMX_AUDIO_TYPE)
				continue;

			fpinfo = &info.info[i];
			if (fpinfo->type == DMX_VIDEO_TYPE)
				r = sprintf(buf, "video info:\n");
			else
				r = sprintf(buf, "audio info:\n ");

			buf += r;
			total += r;

			r = sprintf(buf, "	pid:0x%0x\n", fpinfo->pid);
			buf += r;
			total += r;

			r = sprintf(buf,
					"	buf addr:0x%0x\n",
					fpinfo->filter_info.dmx_buf_phy_start);
			buf += r;
			total += r;

			r = sprintf(buf,
					"	buf size:0x%0x\n",
					fpinfo->filter_info.dmx_total_size);
			buf += r;
			total += r;

			r = sprintf(buf,
					"	buf level:0x%0x\n",
					fpinfo->filter_info.dmx_total_size -
					fpinfo->filter_info.dmx_free_size);
			buf += r;
			total += r;

			r = sprintf(buf,
					"	buf space:0x%0x\n",
					fpinfo->filter_info.dmx_free_size);
			buf += r;
			total += r;

			r = sprintf(buf,
					"	buf wp:0x%0x\n",
					fpinfo->filter_info.wp_offset);
			buf += r;
			total += r;

			r = sprintf(buf,
				"	cache size:0x%0x\n",
				fpinfo->filter_info.dvb_core_total_size);
			buf += r;
			total += r;

			r = sprintf(buf,
				"	cache level:0x%0x\n",
				fpinfo->filter_info.dvb_core_total_size -
				fpinfo->filter_info.dvb_core_free_size);
			buf += r;
			total += r;

			r = sprintf(buf,
				"	cache space:0x%0x\n",
				fpinfo->filter_info.dvb_core_free_size);
			buf += r;
			total += r;
		}
	}
	return total;
}

static ssize_t dump_ts_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t dump_ts_store(struct class *class,
				 struct class_attribute *attr,
				 const char *buf, size_t size)
{
	struct aml_dvb *advb = aml_get_dvb_device();
	int ret = 0;
	int sid = 0;
	long val = 0;

	if (kstrtol(buf, 0, &val) == 0) {
		sid = (int)val;
		dprint("dump sid:0x%0x\n", sid);
	} else {
		dprint("%s parameter fail\n", buf);
		return size;
	}
	if (sid == -1 && !ts_out_elem) {
		dprint("no dump_ts to free\n");
		return size;
	}
	if (mutex_lock_interruptible(&advb->mutex))
		return -ERESTARTSYS;
	if (sid == -1 && ts_out_elem) {
		ts_output_set_dvr_dump(0);
		ts_output_remove_pid(ts_out_elem, 0x1fff);
		ts_output_close(ts_out_elem);
		ts_out_elem = NULL;
		dprint("have free dump_ts");
		mutex_unlock(&advb->mutex);
		return size;
	}

	ts_out_elem = ts_output_open(sid, 0, DVR_FORMAT, OTHER_TYPE, 0, 0);
	if (ts_out_elem) {
		ret = ts_output_set_mem(ts_out_elem,
					TS_OUTPUT_CHAN_DVR_BUF_SIZE, 0,
					TS_OUTPUT_CHAN_PTS_BUF_SIZE, 0);
		if (ret != 0) {
			ts_output_close(ts_out_elem);
			ts_out_elem = NULL;
			mutex_unlock(&advb->mutex);
			return size;
		}
		ts_output_set_dvr_dump(1);
		ts_output_add_pid(ts_out_elem, 0x1fff, 0x1fff, 0, NULL);
		dprint("create dump ts success\n");
	} else {
		dprint("%s error\n", __func__);
	}

	mutex_unlock(&advb->mutex);
	return size;
}
static struct class_attribute aml_dmx_class_attrs[] = {
#ifdef OPEN_REGISTER_NODE
	__ATTR(register_addr, 0644, register_addr_show,
	       register_addr_store),
	__ATTR(register_value, 0644, register_value_show,
	       register_value_store),
#endif
	__ATTR(dump_register, 0644, dump_register_show,
	       dump_register_store),
	__ATTR(dump_filter, 0644, dump_filter_show,
	       dump_filter_store),
	__ATTR(dump_av_level, 0644, dump_av_level_show,
		   NULL),
	__ATTR(cache_status, 0644, cache_status_show,
	       cache_status_store),
	__ATTR(dump_ts, 0644, dump_ts_show,
	       dump_ts_store),
	__ATTR_NULL
};

static struct class aml_dmx_class = {
	.name = "dmx",
	.class_attrs = aml_dmx_class_attrs,
};

int dmx_regist_dmx_class(void)
{
	if (class_register(&aml_dmx_class) < 0)
		dprint("register class error\n");

	return 0;
}

int dmx_unregist_dmx_class(void)
{
	class_unregister(&aml_dmx_class);
	return 0;
}
