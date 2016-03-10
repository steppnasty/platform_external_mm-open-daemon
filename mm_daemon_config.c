/*
   Copyright (C) 2014-2016 Brian Stepp 
      steppnasty@gmail.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#define LOG_TAG "mm-daemon-cfg"

#include <sys/types.h>
#include "mm_daemon.h"
#include "mm_daemon_sensor.h"

static uint32_t isp_events[] = {
    ISP_EVENT_REG_UPDATE,
    ISP_EVENT_START_ACK,
    ISP_EVENT_STOP_ACK,
    ISP_EVENT_IRQ_VIOLATION,
    ISP_EVENT_WM_BUS_OVERFLOW,
    ISP_EVENT_STATS_OVERFLOW,
    ISP_EVENT_CAMIF_ERROR,
    ISP_EVENT_SOF,
    ISP_EVENT_EOF,
    ISP_EVENT_BUF_DIVERT,
    ISP_EVENT_STATS_NOTIFY,
    ISP_EVENT_COMP_STATS_NOTIFY,
};

static uint32_t vfe_stats[] = {
    MSM_ISP_STATS_AEC,
};

static uint32_t mm_daemon_config_stats_offset(int stats_type)
{
    cam_metadata_info_t meta;
    uint32_t p1, p2;

    p1 = (uint32_t)&meta;
    switch (stats_type) {
        case MSM_ISP_STATS_AEC:
            p2 = (uint32_t)&meta.chromatix_lite_ae_stats_data.
                    private_stats_data[0];
            break;
        case MSM_ISP_STATS_AF:
            p2 = (uint32_t)&meta.chromatix_lite_af_stats_data.
                    private_stats_data[0];
            break;
        case MSM_ISP_STATS_AWB:
            p2 = (uint32_t)&meta.chromatix_lite_awb_stats_data.
                    private_stats_data[0];
            break;
        default:
            return 0;
            break;
    }
    return p2 - p1;
}

static uint32_t mm_daemon_config_v4l2_fmt(cam_format_t fmt)
{
    uint32_t val;
    switch(fmt) {
        case CAM_FORMAT_YUV_420_NV12:
        case CAM_FORMAT_YUV_420_NV12_VENUS:
            val = V4L2_PIX_FMT_NV12;
            break;
        case CAM_FORMAT_YUV_420_NV21:
            val = V4L2_PIX_FMT_NV21;
            break;
        case CAM_FORMAT_BAYER_QCOM_RAW_10BPP_GBRG:
        case CAM_FORMAT_BAYER_QCOM_RAW_10BPP_GRBG:
        case CAM_FORMAT_BAYER_QCOM_RAW_10BPP_RGGB:
        case CAM_FORMAT_BAYER_QCOM_RAW_10BPP_BGGR:
            val = V4L2_PIX_FMT_SBGGR10;
            break;
        case CAM_FORMAT_YUV_422_NV61:
            val = V4L2_PIX_FMT_NV61;
            break;
        case CAM_FORMAT_YUV_RAW_8BIT_YUYV:
            val = V4L2_PIX_FMT_YUYV;
            break;
        case CAM_FORMAT_YUV_RAW_8BIT_YVYU:
            val = V4L2_PIX_FMT_YVYU;
            break;
        case CAM_FORMAT_YUV_RAW_8BIT_UYVY:
            val = V4L2_PIX_FMT_UYVY;
            break;
        case CAM_FORMAT_YUV_RAW_8BIT_VYUY:
            val = V4L2_PIX_FMT_VYUY;
            break;
        case CAM_FORMAT_YUV_420_YV12:
            val = V4L2_PIX_FMT_NV12;
            break;
        case CAM_FORMAT_YUV_422_NV16:
            val = V4L2_PIX_FMT_NV16;
            break;
        default:
            val = 0;
            ALOGE("%s: Unknown fmt=%d", __func__, fmt);
            break;
    }
    return val;
}

static void mm_daemon_config_server_cmd(mm_daemon_obj_t *mm_obj, uint8_t cmd,
        int stream_id)
{
    int len;
    mm_daemon_pipe_evt_t pipe_cmd;

    memset(&pipe_cmd, 0, sizeof(pipe_cmd));
    pipe_cmd.cmd = cmd;
    pipe_cmd.val = stream_id;
    len = write(mm_obj->svr_pfds[1], &pipe_cmd, sizeof(pipe_cmd));
    if (len < 1)
        ALOGI("%s: write error", __FUNCTION__);
}

static void mm_daemon_config_sensor_cmd(mm_daemon_sensor_t *mm_snsr,
        uint8_t cmd, int32_t val, int wait)
{
    int len;
    mm_daemon_pipe_evt_t pipe_cmd;

    pthread_mutex_lock(&mm_snsr->lock);
    memset(&pipe_cmd, 0, sizeof(pipe_cmd));
    pipe_cmd.cmd = cmd;
    pipe_cmd.val = val;
    len = write(mm_snsr->pfds[1], &pipe_cmd, sizeof(pipe_cmd));
    if (len < 1)
        ALOGI("%s: write error", __FUNCTION__);
    if (wait)
        pthread_cond_wait(&mm_snsr->cond, &mm_snsr->lock);
    pthread_mutex_unlock(&mm_snsr->lock);
}

static int mm_daemon_config_subscribe(mm_daemon_cfg_t *cfg_obj,
        uint32_t type, int subscribe)
{
    int rc = 0;
    int cmd = subscribe ? VIDIOC_SUBSCRIBE_EVENT : VIDIOC_UNSUBSCRIBE_EVENT;
    struct v4l2_event_subscription sub;

    memset(&sub, 0, sizeof(sub));
    sub.type = type;
    rc = ioctl(cfg_obj->vfe_fd, cmd, &sub);
    if (rc < 0)
        ALOGE("Error: event reg/unreg failed %d", rc);
    return rc;
}

static void mm_daemon_config_set_current_streams(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t hal_stream_type, uint8_t set)
{
    uint8_t stream_type;
    switch (hal_stream_type) {
        case CAM_STREAM_TYPE_PREVIEW:
            stream_type = DAEMON_STREAM_TYPE_PREVIEW;
            break;
        case CAM_STREAM_TYPE_POSTVIEW:
            stream_type = DAEMON_STREAM_TYPE_POSTVIEW;
            break;
        case CAM_STREAM_TYPE_SNAPSHOT:
            stream_type = DAEMON_STREAM_TYPE_SNAPSHOT;
            break;
        case CAM_STREAM_TYPE_VIDEO:
            stream_type = DAEMON_STREAM_TYPE_VIDEO;
            break;
        case CAM_STREAM_TYPE_METADATA:
            stream_type = DAEMON_STREAM_TYPE_METADATA;
            break;
        case CAM_STREAM_TYPE_RAW:
            stream_type = DAEMON_STREAM_TYPE_RAW;
            break;
        case CAM_STREAM_TYPE_OFFLINE_PROC:
            stream_type = DAEMON_STREAM_TYPE_OFFLINE_PROC;
            break;
        default:
            ALOGE("%s: unknown stream type", __FUNCTION__);
            return;
    }
    if (set)
        cfg_obj->current_streams |= stream_type;
    else
        cfg_obj->current_streams &= ~stream_type;
}

static mm_daemon_buf_info *mm_daemon_get_stream_buf(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int i;
    mm_daemon_buf_info *buf = NULL;

    for (i = 0; i < MAX_NUM_STREAM; i++) {
        buf = cfg_obj->stream_buf[i];
        if (buf && buf->stream_info &&
                buf->stream_info->stream_type == stream_type)
            break;
        buf = NULL;
    }
    return buf;
}

static int mm_daemon_get_stream_idx(mm_daemon_cfg_t *cfg_obj,
        int stream_id)
{
    int i;

    for (i = 0; i < MAX_NUM_STREAM; i++) {
        if (cfg_obj->stream_buf[i] && cfg_obj->stream_buf[i]->stream_id ==
                stream_id)
            return i;
    }
    return -EINVAL;
}

static int mm_daemon_config_isp_input_cfg(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    struct msm_vfe_input_cfg input_cfg;

    memset(&input_cfg, 0, sizeof(input_cfg));
    input_cfg.input_src = VFE_PIX_0;
    input_cfg.input_pix_clk = 153600000;
    input_cfg.d.pix_cfg.camif_cfg.pixels_per_line = 640;
    input_cfg.d.pix_cfg.input_format = V4L2_PIX_FMT_NV21;
    rc = ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_INPUT_CFG, &input_cfg);
    return 0;
}

static int mm_daemon_config_isp_stats_req(mm_daemon_cfg_t *cfg_obj,
        int stats_type)
{
    struct msm_vfe_stats_stream_request_cmd stream_req_cmd;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj,
            CAM_STREAM_TYPE_METADATA);

    if (!buf)
        return -ENOMEM;
    memset(&stream_req_cmd, 0, sizeof(stream_req_cmd));
    stream_req_cmd.session_id = cfg_obj->session_id;
    stream_req_cmd.stream_id = buf->stream_id;
    stream_req_cmd.stats_type = stats_type;
    stream_req_cmd.buffer_offset =
            mm_daemon_config_stats_offset(stats_type);
    stream_req_cmd.framedrop_pattern = NO_SKIP;
    if ((ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_REQUEST_STATS_STREAM,
            &stream_req_cmd)) < 0)
        return -1;
    mm_daemon_config_subscribe(cfg_obj,
            ISP_EVENT_STATS_NOTIFY + stats_type, 1);
    buf->stream_handle[stats_type] = stream_req_cmd.stream_handle;
    return 0;
}

static int mm_daemon_config_isp_stats_enqueue(mm_daemon_cfg_t *cfg_obj,
        uint8_t buf_idx)
{
    struct msm_isp_qbuf_info qbuf_info;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj,
            CAM_STREAM_TYPE_METADATA);

    if (!buf)
        return -ENOMEM;
    memset(&qbuf_info, 0, sizeof(qbuf_info));
    qbuf_info.buf_idx = buf_idx;
    qbuf_info.handle = buf->bufq_handle;
    return ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_ENQUEUE_BUF, &qbuf_info);
}

static int mm_daemon_config_isp_stats_cfg(mm_daemon_cfg_t *cfg_obj,
        uint8_t enable)
{
    uint32_t stats_type, i;
    struct msm_vfe_stats_stream_cfg_cmd stream_cfg_cmd;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj,
            CAM_STREAM_TYPE_METADATA);

    if (!buf)
        return -ENOMEM;
    memset(&stream_cfg_cmd, 0, sizeof(stream_cfg_cmd));
    stream_cfg_cmd.enable = enable;
    stream_cfg_cmd.num_streams = ARRAY_SIZE(vfe_stats);
    for (i = 0; i < ARRAY_SIZE(vfe_stats); i++) {
        stats_type = vfe_stats[i];
        stream_cfg_cmd.stream_handle[i] = buf->stream_handle[stats_type];
    }
    return ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_CFG_STATS_STREAM,
            &stream_cfg_cmd);
}

static int mm_daemon_config_isp_stats_release(mm_daemon_cfg_t *cfg_obj,
        int stats_type)
{
    int rc = 0;
    struct msm_vfe_stats_stream_release_cmd stream_release_cmd;
    int isp_stat = ISP_EVENT_STATS_NOTIFY + stats_type;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj,
            CAM_STREAM_TYPE_METADATA);

    if (!buf)
        return -ENOMEM;
    stream_release_cmd.stream_handle =
            buf->stream_handle[stats_type];
    rc = ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_RELEASE_STATS_STREAM,
            &stream_release_cmd);
    mm_daemon_config_subscribe(cfg_obj, isp_stat, 0);
    return rc;
}

static int mm_daemon_config_isp_stats_proc_aec(mm_daemon_cfg_t *cfg_obj,
        struct v4l2_event *isp_event)
{
    struct msm_isp_event_data *buf_event;
    struct msm_isp_stats_event *stats_event;
    int i;
    uint8_t buf_idx;
    uint16_t curr_gain;
    uint32_t stat = 0;
    cam_metadata_info_t *meta;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj,
            CAM_STREAM_TYPE_METADATA);

    if (!buf)
        return -ENOMEM;

    buf_event = (struct msm_isp_event_data *)&(isp_event->u.data[0]);
    stats_event = &buf_event->u.stats;
    buf_idx = stats_event->stats_buf_idxs[MSM_ISP_STATS_AEC];
    meta = (cam_metadata_info_t *)buf->buf_data[buf_idx].vaddr;
    if (cfg_obj->current_streams & DAEMON_STREAM_TYPE_PREVIEW) {
        if (cfg_obj->stat_frame >= 4) {
            curr_gain = cfg_obj->curr_gain;
            for (i = 0; i < MAX_AE_STATS_DATA_SIZE; i++) {
                stat += meta->chromatix_lite_ae_stats_data.
                        private_stats_data[i];
            }
            memset(meta, 0, sizeof(cam_metadata_info_t));
            cfg_obj->exp_level = (stat / MAX_AE_STATS_DATA_SIZE);
            if (cfg_obj->exp_level > 120)
                curr_gain--;
            else if (cfg_obj->exp_level < 50)
                curr_gain++;

            if (curr_gain != cfg_obj->curr_gain) {
                cfg_obj->curr_gain = curr_gain;
                cfg_obj->gain_changed = 1;
            }
            cfg_obj->stat_frame = 0;
        } else {
            memset(meta, 0, sizeof(cam_metadata_info_t));
            cfg_obj->stat_frame++;
        }
    } else if ((cfg_obj->current_streams & DAEMON_STREAM_TYPE_SNAPSHOT) &&
                (cfg_obj->current_streams & DAEMON_STREAM_TYPE_POSTVIEW)) {
        memset(meta, 0, sizeof(cam_metadata_info_t));
        meta->is_prep_snapshot_done_valid = 1;
        meta->prep_snapshot_done_state = DO_NOT_NEED_FUTURE_FRAME;
        meta->is_good_frame_idx_range_valid = 0;
        meta->meta_valid_params.meta_frame_id = buf_event->frame_id;
        meta->is_meta_valid = 1;
    }

    return mm_daemon_config_isp_stats_enqueue(cfg_obj, buf_idx);
}

static int mm_daemon_config_isp_buf_request(mm_daemon_cfg_t *cfg_obj,
        mm_daemon_buf_info *buf_info, int stream_id)
{
    int rc = 0;
    struct msm_isp_buf_request buf_req;

    if ((buf_info) && (buf_info->stream_info_mapped)) {
        if (!buf_info->stream_info->num_bufs)
            return rc;
        memset(&buf_req, 0, sizeof(buf_req));
        buf_req.session_id = cfg_obj->session_id;
        buf_req.stream_id = stream_id;
        buf_req.num_buf = buf_info->stream_info->num_bufs;
        buf_req.buf_type = ISP_PRIVATE_BUF;
        rc = ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_REQUEST_BUF, &buf_req);
        buf_info->bufq_handle = buf_req.handle;
    }
    return rc;
}

static int mm_daemon_config_isp_buf_enqueue(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int i, rc = 0;
    uint32_t j;
    struct msm_isp_qbuf_info qbuf_info;
    struct v4l2_buffer buffer;
    struct v4l2_plane plane[VIDEO_MAX_PLANES];
    cam_stream_buf_plane_info_t *buf_planes = NULL;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj, stream_type);

    if (!buf)
        return -ENOMEM;
    memset(&qbuf_info, 0, sizeof(qbuf_info));
    memset(&buffer, 0, sizeof(buffer));
    buf_planes = &buf->stream_info->buf_planes;
    buffer.length = buf_planes->plane_info.num_planes;
    buffer.m.planes = &plane[0];
    for (i = 0; i < buf->num_stream_bufs; i++) {
        qbuf_info.buffer = buffer;
        qbuf_info.buf_idx = i;
        plane[0].m.userptr = buf->buf_data[i].fd;
        plane[0].data_offset = 0;
        for (j = 1; j < buffer.length; j++) {
            plane[j].m.userptr = buf->buf_data[i].fd;
            plane[j].data_offset = plane[j-1].data_offset +
                    buf_planes->plane_info.mp[j-1].len;
        }
        qbuf_info.handle = buf->bufq_handle;
        if (ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_ENQUEUE_BUF,
                &qbuf_info) == 0) {
            if (stream_type == CAM_STREAM_TYPE_METADATA)
                buf->buf_data[i].vaddr = mmap(0, sizeof(cam_metadata_info_t),
                        PROT_READ|PROT_WRITE, MAP_SHARED, buf->buf_data[i].fd,
                        0);
            buf->buf_data[i].mapped = 1;
        }
    }
    return rc;
}

static int mm_daemon_config_isp_buf_release(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int rc;
    struct msm_isp_buf_request buf_req;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj, stream_type);

    if (!buf)
        return -ENOMEM;
    pthread_mutex_lock(&cfg_obj->lock);
    memset(&buf_req, 0, sizeof(buf_req));
    buf_req.handle = buf->bufq_handle;
    rc = ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_RELEASE_BUF, &buf_req);
    pthread_mutex_unlock(&cfg_obj->lock);
    return rc;
}

static int mm_daemon_config_isp_stream_request(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int i, rc = 0;
    struct msm_vfe_axi_stream_request_cmd stream_cfg_cmd;
    cam_stream_buf_plane_info_t *buf_planes = NULL;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj, stream_type);

    if (!buf)
        return -ENOMEM;
    memset(&stream_cfg_cmd, 0, sizeof(stream_cfg_cmd));
    switch (stream_type) {
        case CAM_STREAM_TYPE_POSTVIEW:
            stream_cfg_cmd.stream_src = PIX_VIEWFINDER;
            break;
        case CAM_STREAM_TYPE_SNAPSHOT:
        case CAM_STREAM_TYPE_VIDEO:
            stream_cfg_cmd.stream_src = PIX_ENCODER;
            break;
        case CAM_STREAM_TYPE_PREVIEW:
            if (cfg_obj->current_streams & DAEMON_STREAM_TYPE_VIDEO)
                stream_cfg_cmd.stream_src = PIX_VIEWFINDER;
            else
                stream_cfg_cmd.stream_src = CAMIF_RAW;
        default:
            stream_cfg_cmd.stream_src = CAMIF_RAW;
            break;
    }
    buf_planes = &buf->stream_info->buf_planes;
    stream_cfg_cmd.burst_count = buf->stream_info->num_of_burst;
    stream_cfg_cmd.session_id = cfg_obj->session_id;
    stream_cfg_cmd.stream_id = buf->stream_id;
    stream_cfg_cmd.output_format = buf->output_format;
    for (i = 0; i < buf_planes->plane_info.num_planes; i++) {
        stream_cfg_cmd.plane_cfg[i].output_height =
                buf_planes->plane_info.mp[i].height;
        stream_cfg_cmd.plane_cfg[i].output_width =
                buf_planes->plane_info.mp[i].width;
        stream_cfg_cmd.plane_cfg[i].output_stride =
                buf_planes->plane_info.mp[i].stride;
    }
    rc = ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_REQUEST_STREAM,
            &stream_cfg_cmd);
    buf->stream_handle[0] = stream_cfg_cmd.axi_stream_handle;
    return rc;
}

static int mm_daemon_config_isp_stream_release(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    struct msm_vfe_axi_stream_release_cmd stream_release_cmd;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj, stream_type);

    if (!buf)
        return -ENOMEM;
    stream_release_cmd.stream_handle = buf->stream_handle[0];
    return ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_RELEASE_STREAM,
            &stream_release_cmd);
}

static int mm_daemon_config_isp_stream_cfg(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type, int cmd)
{
    int rc = -EINVAL;
    struct msm_vfe_axi_stream_cfg_cmd stream_cfg_cmd;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj, stream_type);

    if (!buf)
        return rc;
    pthread_mutex_lock(&cfg_obj->lock);
    memset(&stream_cfg_cmd, 0, sizeof(stream_cfg_cmd));
    switch (stream_type) {
        case CAM_STREAM_TYPE_PREVIEW:
        case CAM_STREAM_TYPE_VIDEO:
            stream_cfg_cmd.num_streams = 1;
            if (buf->stream_info_mapped)
                stream_cfg_cmd.stream_handle[0] = buf->stream_handle[0];
            else
                goto err_fail;
            break;
        case CAM_STREAM_TYPE_SNAPSHOT:
            if (buf->stream_info_mapped)
                stream_cfg_cmd.stream_handle[0] = buf->stream_handle[0];
            else
                goto err_fail;
            buf = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_POSTVIEW);
            if (buf && buf->stream_info_mapped)
                stream_cfg_cmd.stream_handle[1] = buf->stream_handle[0];
            else
                goto err_fail;
            stream_cfg_cmd.num_streams = 2;
            break;
        default:
            goto err_fail;
    }
    stream_cfg_cmd.cmd = cmd;

    rc = ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_CFG_STREAM, &stream_cfg_cmd);
err_fail:
    pthread_mutex_unlock(&cfg_obj->lock);
    return rc;
}


static int mm_daemon_config_vfe_reg_cmd(mm_daemon_cfg_t *cfg_obj,
        int length, void *value, void *cfg_cmd, int num_cfg)
{
    struct msm_vfe_cfg_cmd2 proc_cmd;

    memset(&proc_cmd, 0, sizeof(proc_cmd));
    proc_cmd.cfg_cmd = cfg_cmd;
    proc_cmd.num_cfg = num_cfg;
    proc_cmd.cmd_len = length;
    proc_cmd.cfg_data = value;
    return ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_VFE_REG_CFG, &proc_cmd);
}

static int mm_daemon_config_vfe_stop(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *stpcfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x1E0,
                .len = 4,
            },
            .cmd_type = VFE_WRITE_MB,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x1C,
                .len = 16,
                .cmd_data_offset = 4,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x18,
                .len = 4,
                .cmd_data_offset = 20,
            },
            .cmd_type = VFE_WRITE_MB,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x1C,
                .len = 8,
                .cmd_data_offset = 24,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x1D8,
                .len = 4,
                .cmd_data_offset = 28,
            },
            .cmd_type = VFE_WRITE_MB,
        },
    };
    stpcfg = (uint32_t *)malloc(32);
    p = stpcfg;
    *p++ = 0x2;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0xFFFFFFFF;
    *p++ = 0xFFFFFFFF;
    *p++ = 0x1;
    *p++ = 0xF0000000;
    *p++ = 0x00C00000;
    *p = 0x1;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 32, (void *)stpcfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(stpcfg);
    return rc;
}

static int mm_daemon_config_vfe_reset(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *rstdata, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x1c,
                .len = 4,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x20,
                .len = 4,
                .cmd_data_offset = 4,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x24,
                .len = 4,
                .cmd_data_offset = 8,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x28,
                .len = 4,
                .cmd_data_offset = 12,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x18,
                .len = 4,
                .cmd_data_offset = 16,
            },
            .cmd_type = VFE_WRITE_MB,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x20,
                .len = 4,
                .cmd_data_offset = 20,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x4,
                .len = 4,
                .cmd_data_offset = 24,
            },
            .cmd_type = VFE_WRITE_MB,
        },
                
    };

    rstdata = (uint32_t *)malloc(28);
    p = rstdata;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0xFFFFFFFF;
    *p++ = 0xFFFFFFFF;

    *p++ = 0x1;

    *p++ = 0x400000;
    *p = 0x3FF;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 28, (void *)rstdata,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(rstdata);
    return rc;
}

static int mm_daemon_config_vfe_roll_off(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int rc = 0;
    uint32_t *rocfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x274,
                .len = 16,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x598,
                .cmd_data_offset = 16,
                .len = 8,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.dmi_info = {
                .lo_tbl_offset = 24,
                .len = 1768,
            },
            .cmd_type = VFE_WRITE_DMI_32BIT,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x598,
                .len = 8,
                .cmd_data_offset = 1792,
            },
            .cmd_type = VFE_WRITE,
        },
                
    };
        
    rocfg = (uint32_t *)malloc(1800);
    p = rocfg;
    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT)
        *p++ = 0xC4A251;
    else
        *p++ = 0x18C5028;

    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;

    *p++ = 0x101;
    *p++ = 0x0;

    *p++ = 0x511d5200;  /* 0x5a4 */
    *p++ = 0x4d58437f;
    *p++ = 0x49914ae2;
    *p++ = 0x45d03d9d;
    *p++ = 0x42ba4404;
    *p++ = 0x40863958;
    *p++ = 0x3e683f53;
    *p++ = 0x3c0a34c6;
    *p++ = 0x3b603b9e;
    *p++ = 0x3a3932c1;
    *p++ = 0x39dc3984;
    *p++ = 0x39a33189;
    *p++ = 0x39a038c6;
    *p++ = 0x39e83219;
    *p++ = 0x39ac3972;
    *p++ = 0x3afb3297;
    *p++ = 0x3af23a3c;
    *p++ = 0x3d073437;
    *p++ = 0x3dda3cda;
    *p++ = 0x40d737c6;
    *p++ = 0x42f24291;
    *p++ = 0x46313cef;
    *p++ = 0x4ab64998;
    *p++ = 0x4f214403;
    *p++ = 0x55555533;
    *p++ = 0x58c94d45;
    *p++ = 0xdb7ee8ba;
    *p++ = 0xdebdeaf8;
    *p++ = 0xf089e8e5;
    *p++ = 0xeb51f013;
    *p++ = 0xf4c0f3d5;
    *p++ = 0xf5f2f407;
    *p++ = 0xf519f791;
    *p++ = 0xf5d9f77a;
    *p++ = 0xf96bf7ca;
    *p++ = 0xfb2efcb2;
    *p++ = 0xfc2ffcb1;
    *p++ = 0xfc5d0035;
    *p++ = 0xfb19fd35;
    *p++ = 0x001cfe6b;
    *p++ = 0x0042012c;
    *p++ = 0x014401ac;
    *p++ = 0xfe6afe2d;
    *p++ = 0x009c0186;
    *p++ = 0x01e9ff64;
    *p++ = 0x0507021f;
    *p++ = 0x024e0442;
    *p++ = 0x06060449;
    *p++ = 0x0892060d;
    *p++ = 0x07ff08ad;
    *p++ = 0x0a940d4c;
    *p++ = 0x0b0107d1;
    *p++ = 0x114011a3;
    *p++ = 0x148511b1;
    *p++ = 0x1d2e211d;
    *p++ = 0x1e951eae;
    *p++ = 0x301e2ec2;
    *p++ = 0x37982911;
    *p++ = 0xe5e6e9c7;
    *p++ = 0xe7baee2d;
    *p++ = 0xf154f1c4;
    *p++ = 0xed8cf33a;
    *p++ = 0xf08ef1bd;
    *p++ = 0xf4b7f56c;
    *p++ = 0xf7a1f656;
    *p++ = 0xf829f897;
    *p++ = 0xfa0df905;
    *p++ = 0xfa51ff06;
    *p++ = 0xfbe0fcba;
    *p++ = 0xfd44fc27;
    *p++ = 0xfba1fd3d;
    *p++ = 0xfc3900fe;
    *p++ = 0xfdf4fded;
    *p++ = 0x00f20181;
    *p++ = 0x0189009f;
    *p++ = 0x039f00c5;
    *p++ = 0x010301c2;
    *p++ = 0x0357011a;
    *p++ = 0x03df03d1;
    *p++ = 0x06d80595;
    *p++ = 0x060906fc;
    *p++ = 0x071705da;
    *p++ = 0x0bb90903;
    *p++ = 0x0d9109df;
    *p++ = 0x0da00f6c;
    *p++ = 0x0ed70d50;
    *p++ = 0x19e61a8d;
    *p++ = 0x18be1734;
    *p++ = 0x24c328ed;
    *p++ = 0x2d2f2446;
    *p++ = 0xebe6ee58;
    *p++ = 0xe8f2eed4;
    *p++ = 0xeffdef6d;
    *p++ = 0xf29bf290;
    *p++ = 0xf311f473;
    *p++ = 0xf389f8ff;
    *p++ = 0xf7d5f7e9;
    *p++ = 0xf95bf86c;
    *p++ = 0xf81ef808;
    *p++ = 0xf7c9fda2;
    *p++ = 0xfacbf971;
    *p++ = 0xfa71fbe9;
    *p++ = 0xfa7ffada;
    *p++ = 0xfd82fda6;
    *p++ = 0xfe410013;
    *p++ = 0xffb50152;
    *p++ = 0x023d01b1;
    *p++ = 0x036c017b;
    *p++ = 0x036d01c4;
    *p++ = 0x045f03a2;
    *p++ = 0x06c505ce;
    *p++ = 0x07c8057e;
    *p++ = 0x06840727;
    *p++ = 0x07bd0530;
    *p++ = 0x09630b87;
    *p++ = 0x0b2409da;
    *p++ = 0x0d720c51;
    *p++ = 0x0fbc0a38;
    *p++ = 0x12a2137b;
    *p++ = 0x13e2137e;
    *p++ = 0x242123bd;
    *p++ = 0x25481bd4;
    *p++ = 0xed7eef87;
    *p++ = 0xee19f4a1;
    *p++ = 0xefadf175;
    *p++ = 0xf155f14a;
    *p++ = 0xf5f3f3ef;
    *p++ = 0xf52dfa59;
    *p++ = 0xf603f6fc;
    *p++ = 0xf74af985;
    *p++ = 0xf711f522;
    *p++ = 0xf755f9e7;
    *p++ = 0xf918f7ee;
    *p++ = 0xf9e6fa0d;
    *p++ = 0xfa46fc4c;
    *p++ = 0xfc0ffe2b;
    *p++ = 0xff5cfe9b;
    *p++ = 0x000201cd;
    *p++ = 0x014e01d8;
    *p++ = 0x0197024b;
    *p++ = 0x04fb032c;
    *p++ = 0x06f0030e;
    *p++ = 0x07590785;
    *p++ = 0x08a706e6;
    *p++ = 0x083f0a00;
    *p++ = 0x09b50828;
    *p++ = 0x0a1c094a;
    *p++ = 0x0a78089e;
    *p++ = 0x0bb40f9f;
    *p++ = 0x0d170952;
    *p++ = 0x122a1068;
    *p++ = 0x11710ea7;
    *p++ = 0x1d30204c;
    *p++ = 0x20de1a1a;
    *p++ = 0xed7ff09d;
    *p++ = 0xec74f51c;
    *p++ = 0xf1edf1cd;
    *p++ = 0xf395f2e7;
    *p++ = 0xf3d3f444;
    *p++ = 0xf5ccf9c1;
    *p++ = 0xf54af348;
    *p++ = 0xf51ff714;
    *p++ = 0xf73ff6f0;
    *p++ = 0xf68ef8ed;
    *p++ = 0xf87af6a7;
    *p++ = 0xf914fc0f;
    *p++ = 0xfbc6fcd1;
    *p++ = 0xfc3afe00;
    *p++ = 0xfea7ffbd;
    *p++ = 0xffea00f4;
    *p++ = 0x01510113;
    *p++ = 0x017301f9;
    *p++ = 0x0586056d;
    *p++ = 0x063e035f;
    *p++ = 0x072005f2;
    *p++ = 0x08e60651;
    *p++ = 0x0af20af0;
    *p++ = 0x0a960773;
    *p++ = 0x0a1c0bf0;
    *p++ = 0x0c0c09c4;
    *p++ = 0x0cc30c66;
    *p++ = 0x0c5a0aa1;
    *p++ = 0x0f9b139e;
    *p++ = 0x11fe0d96;
    *p++ = 0x1d571973;
    *p++ = 0x1c7d184c;
    *p++ = 0xeca9f0a0;
    *p++ = 0xecfdf559;
    *p++ = 0xf310f036;
    *p++ = 0xf292f55f;
    *p++ = 0xf4aff40b;
    *p++ = 0xf53cf7e4;
    *p++ = 0xf50bf536;
    *p++ = 0xf4f9f617;
    *p++ = 0xf4fcf484;
    *p++ = 0xf59cf8d1;
    *p++ = 0xf932f940;
    *p++ = 0xf872fb8b;
    *p++ = 0xfba7fc5e;
    *p++ = 0xfcbbfed7;
    *p++ = 0xff12006f;
    *p++ = 0xff2b01a6;
    *p++ = 0x02d802c8;
    *p++ = 0x01a500ad;
    *p++ = 0x041e02b8;
    *p++ = 0x057302ce;
    *p++ = 0x07f10627;
    *p++ = 0x090806d5;
    *p++ = 0x0b600c27;
    *p++ = 0x0b96082f;
    *p++ = 0x0b490c64;
    *p++ = 0x0a9209b8;
    *p++ = 0x0ba50f30;
    *p++ = 0x0e600b23;
    *p++ = 0x11c60fd6;
    *p++ = 0x10d50b40;
    *p++ = 0x19791ba5;
    *p++ = 0x19e115b6;
    *p++ = 0xeaffefb4;
    *p++ = 0xec27f260;
    *p++ = 0xf36bf172;
    *p++ = 0xf2abf66e;
    *p++ = 0xf4ddf53e;
    *p++ = 0xf437f7a3;
    *p++ = 0xf3c3f2bd;
    *p++ = 0xf583f6c4;
    *p++ = 0xf599f4bb;
    *p++ = 0xf5a6f7ee;
    *p++ = 0xf9f2f9f7;
    *p++ = 0xf745fbbb;
    *p++ = 0xfc9ffcde;
    *p++ = 0xfcf1fdaa;
    *p++ = 0xfeaeff64;
    *p++ = 0xfedc0081;
    *p++ = 0x023b02a3;
    *p++ = 0x015b0170;
    *p++ = 0x04bf0377;
    *p++ = 0x059c03d3;
    *p++ = 0x07a5074d;
    *p++ = 0x084005d8;
    *p++ = 0x0b9b0b05;
    *p++ = 0x0bea08ab;
    *p++ = 0x0ad10d57;
    *p++ = 0x0c5d088e;
    *p++ = 0x0d010edd;
    *p++ = 0x0b1f0b1b;
    *p++ = 0x101110c3;
    *p++ = 0x11e80cdf;
    *p++ = 0x1c2e1a51;
    *p++ = 0x1cd11547;
    *p++ = 0xeaebef12;
    *p++ = 0xeae5f46b;
    *p++ = 0xf319f022;
    *p++ = 0xf2e9f32a;
    *p++ = 0xf5eef47e;
    *p++ = 0xf4def8ad;
    *p++ = 0xf496f3eb;
    *p++ = 0xf50af736;
    *p++ = 0xf5f8f538;
    *p++ = 0xf5f2f871;
    *p++ = 0xf8d9f9bd;
    *p++ = 0xf742faf3;
    *p++ = 0xfc26fd34;
    *p++ = 0xfbeafd32;
    *p++ = 0xffafff64;
    *p++ = 0xfe740103;
    *p++ = 0x01ea013d;
    *p++ = 0x025600e2;
    *p++ = 0x05770565;
    *p++ = 0x05b60420;
    *p++ = 0x080f07d6;
    *p++ = 0x083a05dd;
    *p++ = 0x0a800b1e;
    *p++ = 0x0b40083d;
    *p++ = 0x0cf30d6e;
    *p++ = 0x0b8708d8;
    *p++ = 0x0b190d17;
    *p++ = 0x0b8a0a7c;
    *p++ = 0x122d1345;
    *p++ = 0x11620ea3;
    *p++ = 0x1bb01b84;
    *p++ = 0x1cac14b8;
    *p++ = 0xeb3beff1;
    *p++ = 0xeb74f1c1;
    *p++ = 0xf213f00a;
    *p++ = 0xf1bbf3d7;
    *p++ = 0xf4e1f446;
    *p++ = 0xf463f883;
    *p++ = 0xf62af46b;
    *p++ = 0xf728f8c5;
    *p++ = 0xf69bf5d9;
    *p++ = 0xf586f928;
    *p++ = 0xf935f9b2;
    *p++ = 0xf76cfa97;
    *p++ = 0xfbcefcb5;
    *p++ = 0xfac6fdbb;
    *p++ = 0xff80ff9a;
    *p++ = 0xfdfcfefd;
    *p++ = 0x02990227;
    *p++ = 0x01910189;
    *p++ = 0x04c104c4;
    *p++ = 0x06090401;
    *p++ = 0x098a082a;
    *p++ = 0x07ee05d0;
    *p++ = 0x0b1d0b8f;
    *p++ = 0x0ba5098f;
    *p++ = 0x0b6c0c5d;
    *p++ = 0x099907a5;
    *p++ = 0x0b5a0e4d;
    *p++ = 0x0b4b0894;
    *p++ = 0x110413aa;
    *p++ = 0x12da0f87;
    *p++ = 0x1e221ea9;
    *p++ = 0x1fb41628;
    *p++ = 0xeb54ee32;
    *p++ = 0xea63f0ad;
    *p++ = 0xef33f076;
    *p++ = 0xf068f38e;
    *p++ = 0xf3dcf42b;
    *p++ = 0xf296f627;
    *p++ = 0xf83bf5ba;
    *p++ = 0xf6cffa1d;
    *p++ = 0xf853f7fd;
    *p++ = 0xf827f9e3;
    *p++ = 0xf941f8e9;
    *p++ = 0xf7a7fb4f;
    *p++ = 0xfc36fc90;
    *p++ = 0xfb09fcbc;
    *p++ = 0xffb3feb8;
    *p++ = 0xfe04feac;
    *p++ = 0x03300357;
    *p++ = 0x010b01e5;
    *p++ = 0x05f504ed;
    *p++ = 0x05920357;
    *p++ = 0x078c0861;
    *p++ = 0x07d80677;
    *p++ = 0x09fe0a56;
    *p++ = 0x08fe05d5;
    *p++ = 0x09c80c02;
    *p++ = 0x09ee0950;
    *p++ = 0x0d3d0e21;
    *p++ = 0x0b7d0a81;
    *p++ = 0x1224162a;
    *p++ = 0x11ce0f86;
    *p++ = 0x24e62058;
    *p++ = 0x259a1ad3;
    *p++ = 0xe811eae6;
    *p++ = 0xe707ec55;
    *p++ = 0xecd8ee22;
    *p++ = 0xeec1f2a7;
    *p++ = 0xf2eef096;
    *p++ = 0xf17cf584;
    *p++ = 0xf841f795;
    *p++ = 0xf79bf9ba;
    *p++ = 0xfaa7f9f0;
    *p++ = 0xf92dfb29;
    *p++ = 0xfbe6fc02;
    *p++ = 0xf954fad1;
    *p++ = 0xfbecfbc8;
    *p++ = 0xfb46fe0d;
    *p++ = 0xffa7ff67;
    *p++ = 0xfdca0018;
    *p++ = 0x01f50283;
    *p++ = 0x016c002d;
    *p++ = 0x072e04dc;
    *p++ = 0x05210305;
    *p++ = 0x059b076d;
    *p++ = 0x041f04b8;
    *p++ = 0x074108a1;
    *p++ = 0x06d00423;
    *p++ = 0x0a1f0a79;
    *p++ = 0x08ed06de;
    *p++ = 0x0ea011ba;
    *p++ = 0x0ee10ca5;
    *p++ = 0x17f91933;
    *p++ = 0x171913e1;
    *p++ = 0x27ed24d7;
    *p++ = 0x25481d5d;
    *p++ = 0xe0e3e823;
    *p++ = 0xe23fe9fb;
    *p++ = 0xeb86ea52;
    *p++ = 0xe8c6eeb9;
    *p++ = 0xf27ef047;
    *p++ = 0xf051f4f7;
    *p++ = 0xf55af690;
    *p++ = 0xf65df7ef;
    *p++ = 0xfb01f93a;
    *p++ = 0xf949fab7;
    *p++ = 0xfcd6fc67;
    *p++ = 0xf96efb44;
    *p++ = 0xfd2aff0a;
    *p++ = 0xfc9effc6;
    *p++ = 0x0039fee4;
    *p++ = 0xfccbfe06;
    *p++ = 0x02520129;
    *p++ = 0x0154002b;
    *p++ = 0x0365045a;
    *p++ = 0x02ab01a8;
    *p++ = 0x064c060f;
    *p++ = 0x03ce0438;
    *p++ = 0x08870937;
    *p++ = 0x084f05b1;
    *p++ = 0x0c270d4b;
    *p++ = 0x09f809ad;
    *p++ = 0x14811796;
    *p++ = 0x144c0e37;
    *p++ = 0x1ed11d47;
    *p++ = 0x1c5a1a15;
    *p++ = 0x29ac2c53;
    *p++ = 0x2cd52048;
    *p++ = 0xe0a7e223;
    *p++ = 0xdea3e7d6;
    *p++ = 0xe180e515;
    *p++ = 0xe5dee730;
    *p++ = 0xedb0e940;
    *p++ = 0xed3ef14c;
    *p++ = 0xf3c4f449;
    *p++ = 0xf345f80d;
    *p++ = 0xf962f876;
    *p++ = 0xf806f7ef;
    *p++ = 0xfc7efc92;
    *p++ = 0xf8b4fcb6;
    *p++ = 0xfe55fd90;
    *p++ = 0xfd40fd9e;
    *p++ = 0xff68002b;
    *p++ = 0xfba5fe9a;
    *p++ = 0x01b802bd;
    *p++ = 0x018e0032;
    *p++ = 0x04e9066a;
    *p++ = 0x0347022a;
    *p++ = 0x087e040b;
    *p++ = 0x055c0491;
    *p++ = 0x09f50c5e;
    *p++ = 0x0a350910;
    *p++ = 0x12e7126f;
    *p++ = 0x0ec60d87;
    *p++ = 0x162a1cf0;
    *p++ = 0x177e12ea;
    *p++ = 0x2614240a;
    *p++ = 0x23e5204b;
    *p++ = 0x34bf2f0c;
    *p++ = 0x30e62327;

    *p++ = 0x100;
    *p = 0x0;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 1800, (void *)rocfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(rocfg);
    return rc;
}

static int mm_daemon_config_vfe_fov(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int rc = 0;
    uint32_t *fov_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x360,
                .len = 8,
            },
            .cmd_type = VFE_WRITE,
        },
                
    };

    fov_cfg = (uint32_t *)malloc(8);
    p = fov_cfg;
    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
        *p++ = 0xA23;
        *p = 0x80799;
    } else {
        *p++ = 0x0000050B;
        if (cfg_obj->current_streams & DAEMON_STREAM_TYPE_VIDEO)
            *p = 0x007C0351;
        else
            *p = 0x000203CA;
    }

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 8, (void *)fov_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(fov_cfg);
    return rc;
}

static int mm_daemon_config_vfe_main_scaler(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int rc = 0;
    uint32_t *ms_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x368,
                .len = 28,
            },
            .cmd_type = VFE_WRITE,
        },
                
    };

    ms_cfg = (uint32_t *)malloc(28);
    p = ms_cfg;
    *p++ = 0x3;
    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
        *p++ = 0x0A200A24;
        *p++ = 0x00310065;
        *p++ = 0x00000000;
        *p++ = 0x07900792;
        *p++ = 0x00310043;
    } else if (cfg_obj->current_streams & DAEMON_STREAM_TYPE_VIDEO) {
        *p++ = 0x0500050C;
        *p++ = 0x00310266;
        *p++ = 0x00000000;
        *p++ = 0x02D002D6;
        *p++ = 0x00310222;
    } else {
        *p++ = 0x0280050C;
        *p++ = 0x003204CC;
        *p++ = 0x00000000;
        *p++ = 0x01E003C9;
        *p++ = 0x003204CC;
    }
    *p = 0x0;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 28, (void *)ms_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(ms_cfg);
    return rc;
}

static int mm_daemon_config_vfe_s2y(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int rc = 0;
    uint32_t *s2y_cfg, *p;
    uint32_t s2y_width, s2y_height;
    cam_dimension_t dim;
    mm_daemon_buf_info *buf = NULL;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x4d0,
                .len = 20,
            },
            .cmd_type = VFE_WRITE,
        },
                
    };

    if (cfg_obj->current_streams & DAEMON_STREAM_TYPE_POSTVIEW)
        buf = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_POSTVIEW);
    else
        buf = mm_daemon_get_stream_buf(cfg_obj, stream_type);
    if (!buf)
        return -ENOMEM;
    dim = buf->stream_info->dim;
    s2y_width = dim.width << 16;
    s2y_height = dim.height << 16;
    if (cfg_obj->current_streams & DAEMON_STREAM_TYPE_POSTVIEW) {
        buf = mm_daemon_get_stream_buf(cfg_obj, stream_type);
        if (!buf)
            return -ENOMEM;
        dim = buf->stream_info->dim;
    }
    s2y_width |= dim.width;
    s2y_height |= dim.height;

    s2y_cfg = (uint32_t *)malloc(20);
    p = s2y_cfg;
    *p++ = 0x00000003;
    *p++ = s2y_width;
    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
        *p++ = 0x00220666;
        *p++ = s2y_height;
        *p = 0x00220444;
    } else {
        *p++ = 0x00310000;
        *p++ = s2y_height;
        *p = 0x00310000;
    }

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 20, (void *)s2y_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(s2y_cfg);
    return rc;
}

static int mm_daemon_config_vfe_s2cbcr(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int rc = 0;
    uint32_t *s2cbcr_cfg, *p;
    uint32_t s2cbcr_width;
    uint32_t s2cbcr_height;
    cam_dimension_t dim;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj, stream_type);
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x4e4,
                .len = 20,
            },
            .cmd_type = VFE_WRITE,
        },
                
    };

    if (!buf)
        return -ENOMEM;
    dim = buf->stream_info->dim;

    if (cfg_obj->current_streams & DAEMON_STREAM_TYPE_VIDEO) {
        s2cbcr_width = 640 << 16;
        s2cbcr_height = 360 << 16;
    } else {
        s2cbcr_width = 320 << 16;
        s2cbcr_height = 240 << 16;
    }
    s2cbcr_width |= dim.width;
    s2cbcr_height |= dim.height;

    s2cbcr_cfg = (uint32_t *)malloc(20);
    p = s2cbcr_cfg;
    *p++ = 0x00000003;
    *p++ = s2cbcr_width;
    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
        *p++ = 0x00120666;
        *p++ = s2cbcr_height;
        *p = 0x00120444;
    } else {
        *p++ = 0x00320000;
        *p++ = s2cbcr_height;
        *p = 0x00320000;
    }

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 20, (void *)s2cbcr_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(s2cbcr_cfg);
    return rc;
}

static int mm_daemon_config_vfe_axi(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int rc = 0;
    uint32_t *axiout, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x3c,
                .len = 20,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x58,
                .len = 16,
                .cmd_data_offset = 20,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x70,
                .len = 16,
                .cmd_data_offset = 36,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x88,
                .len = 16,
                .cmd_data_offset = 52,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0xa0,
                .len = 16,
                .cmd_data_offset = 68,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0xb8,
                .len = 16,
                .cmd_data_offset = 84,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0xd0,
                .len = 16,
                .cmd_data_offset = 100,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0xe8,
                .len = 12,
                .cmd_data_offset = 116,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x38,
                .len = 4,
                .cmd_data_offset = 128,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    axiout = (uint32_t *)malloc(132);
    p = axiout;
    *p++ = 0x2AAA771;
    *p++ = 0x1;

    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT)
        *p++ = 0x203;
    else
        *p++ = 0x1A03;
    
    *p++ = 0x0;
    *p++ = 0x0;

    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT)
        *p++ = 0x22;
    else
        *p++ = 0x12F;

    if (cfg_obj->current_streams & DAEMON_STREAM_TYPE_VIDEO) {
        *p++ = 0x4F02CF;
        *p++ = 0xA02CF2;
    } else {
        *p++ = 0x2701DF;
        *p++ = 0x501DF2;
    }
    *p++ = 0x0;

    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
        *p++ = 0x340022;
        *p++ = 0xA1078F;
        *p++ = 0x14478F2;
    } else {
        *p++ = 0x1C8012F;
        *p++ = 0x002701DF;
        *p++ = 0x00501DF2;
    }
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;

    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT)
        *p++ = 0x230010;
    else
        *p++ = 0x1300097;

    if (cfg_obj->current_streams & DAEMON_STREAM_TYPE_VIDEO) {
        *p++ = 0x4F0167;
        *p++ = 0xA01672;
    } else {
        *p++ = 0x2700EF;
        *p++ = 0x500EF2;
    }
    *p++ = 0x0;

    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
        *p++ = 0x57011D;
        *p++ = 0xA103C7;
        *p++ = 0x1443C72;
    } else {
        *p++ = 0x2F80097;
        *p++ = 0x2700EF;
        *p++ = 0x500EF2;
    }
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x3fff;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 132, (void *)axiout,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(axiout);
    return rc;
}

static int mm_daemon_config_vfe_chroma_en(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *chroma_en, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x3c4,
                .len = 36,
            },
            .cmd_type = VFE_WRITE,
        },
                
    };

    chroma_en = (uint32_t *)malloc(36);
    p = chroma_en;
    *p++ = 0x0000004d;
    *p++ = 0x00000096;
    *p++ = 0x0000001d;
    *p++ = 0x0;
    *p++ = 0x00800080;
    *p++ = 0x0fa90fa9;
    *p++ = 0x00800080;
    *p++ = 0x0fd70fd7;
    *p = 0x00800080;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 36, (void *)chroma_en,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(chroma_en);
    return rc;
}

static int mm_daemon_config_vfe_color_cor(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *color_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x388,
                .len = 52,
            },
            .cmd_type = VFE_WRITE,
        },
                
    };

    color_cfg = (uint32_t *)malloc(52);
    p = color_cfg;
    *p++ = 0xc2;
    *p++ = 0xff3;
    *p++ = 0xfcc;
    *p++ = 0xf95;
    *p++ = 0xef;
    *p++ = 0xffd;
    *p++ = 0xf7e;
    *p++ = 0xf;
    *p++ = 0xf4;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p = 0x0;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 52, (void *)color_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(color_cfg);
    return rc;
}

static int mm_daemon_config_vfe_asf(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int rc = 0;
    uint32_t *asf_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x4a0,
                .len = 48,
            },
            .cmd_type = VFE_WRITE,
        },
                
    };

    asf_cfg = (uint32_t *)malloc(48);
    p = asf_cfg;
    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
        *p++ = 0x000028bd;
        *p++ = 0x000c0c12;
        *p++ = 0xd828d828;
        *p++ = 0x00038f00;
        *p++ = 0x00408000;
        *p++ = 0x00f00000;
        *p++ = 0x00400e08;
        *p++ = 0x03082081;
        *p++ = 0x00204084;
    } else {
        *p++ = 0x0000604d;
        *p++ = 0x0008080d;
        *p++ = 0xd828d828;
        *p++ = 0x3c000000;
        *p++ = 0x00408038;
        *p++ = 0x3c000000;
        *p++ = 0x00438008;
        *p++ = 0x02000000;
        *p++ = 0x00608008;
    }
    *p++ = 0x0;
    *p++ = 0x0;
    *p = 0x0;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 48, (void *)asf_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(asf_cfg);
    return rc;
}

static int mm_daemon_config_vfe_white_balance(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int rc = 0;
    uint32_t wb_cfg;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x384,
                .len = 4,
            },
            .cmd_type = VFE_WRITE,
        },
                
    };

    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT)
        wb_cfg = 0x02010080;
    else
        wb_cfg = 0x33e5a8d;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 4, (void *)&wb_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    return rc;
}

static int mm_daemon_config_vfe_black_level(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *bl_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x264,
                .len = 16,
            },
            .cmd_type = VFE_WRITE,
        },
                
    };

    bl_cfg = (uint32_t *)malloc(16);
    p = bl_cfg;
    *p++ = 0xf9;
    *p++ = 0xf9;
    *p++ = 0xfe;
    *p = 0xfe;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 16, (void *)bl_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(bl_cfg);
    return rc;
}

static int mm_daemon_config_vfe_rgb_gamma(mm_daemon_cfg_t *cfg_obj)
{
    uint32_t gamma_cfg = 0;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x3bc,
                .len = 4,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    return mm_daemon_config_vfe_reg_cmd(cfg_obj, 4, (void *)&gamma_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
}

static int mm_daemon_config_vfe_rgb_gamma_chbank(mm_daemon_cfg_t *cfg_obj, int banksel)
{
    int rc = 0;
    uint32_t *gamma_cfg, *p;
    uint32_t banksel_val = 0x100 + banksel;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x598,
                .len = 8,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.dmi_info = {
                .lo_tbl_offset = 8,
                .len = 128,
            },
            .cmd_type = VFE_WRITE_DMI_16BIT,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x598,
                .len = 8,
                .cmd_data_offset = 136,
            },
            .cmd_type = VFE_WRITE,
        },
                
    };
                
    gamma_cfg = (uint32_t *)malloc(144);
    p = gamma_cfg;
    *p++ = banksel_val;
    *p++ = 0x0;

    *p++ = 0x0c0d0d00;
    *p++ = 0x0a240b19;
    *p++ = 0x09380a2e;
    *p++ = 0x08490841;
    *p++ = 0x07590851;
    *p++ = 0x06660660;
    *p++ = 0x0572066c;
    *p++ = 0x047d0677;
    *p++ = 0x04860581;
    *p++ = 0x048f058a;
    *p++ = 0x04960393;
    *p++ = 0x039e049a;
    *p++ = 0x04a403a1;
    *p++ = 0x03ab03a8;
    *p++ = 0x03b103ae;
    *p++ = 0x03b602b4;
    *p++ = 0x03bc03b9;
    *p++ = 0x03c102bf;
    *p++ = 0x03c602c4;
    *p++ = 0x02cb02c9;
    *p++ = 0x02d003cd;
    *p++ = 0x02d402d2;
    *p++ = 0x02d903d6;
    *p++ = 0x02dd02db;
    *p++ = 0x02e102df;
    *p++ = 0x02e502e3;
    *p++ = 0x02e902e7;
    *p++ = 0x02ed02eb;
    *p++ = 0x02f102ef;
    *p++ = 0x02f502f3;
    *p++ = 0x02f902f7;
    *p++ = 0x00ff02fb;

    *p++ = 0x100;
    *p = 0x0;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 144, (void *)gamma_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(gamma_cfg);
    return rc;
}

static int mm_daemon_config_vfe_camif(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int rc = 0;
    uint32_t *camif_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x1e4,
                .len = 32,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    camif_cfg = (uint32_t *)malloc(32);
    p = camif_cfg;
    *p++ = 0x100;
    *p++ = 0x00;
    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
        *p++ = 0x7A80A30;
        *p++ = 0xA2F;
        *p++ = 0x7A7;
    } else {
        *p++ = 0x3D40518;
        *p++ = 0x517;
        *p++ = 0x3D3;
    }
    *p++ = 0xffffffff;
    *p++ = 0x00;
    *p++ = 0x3fff3fff;
    
    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 32, (void *)camif_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(camif_cfg);
    return rc;
}

static int mm_daemon_config_vfe_demux(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type)
{
    int rc = 0;
    uint32_t *demux_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x284,
                .len = 20,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    demux_cfg = (uint32_t *)malloc(20);
    p = demux_cfg;
    *p++ = 0x1;
    if (stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
        *p++ = 0x008d008d;
        *p++ = 0x00cf012d;
    } else {
        *p++ = 0x00800080;
        *p++ = 0x00800080;
    }
    *p++ = 0x9c;
    *p++ = 0xca;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 20, (void *)demux_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(demux_cfg);
    return rc;
}

static int mm_daemon_config_vfe_out_clamp(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *clamp_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x524,
                .len = 8,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    clamp_cfg = (uint32_t *)malloc(8);
    p = clamp_cfg;
    *p++ = 0xffffff;
    *p = 0x0;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 8, (void *)clamp_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(clamp_cfg);
    return rc;
}

static int mm_daemon_config_vfe_frame_skip(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *skip_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x504,
                .len = 32,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    skip_cfg = (uint32_t *)malloc(32);
    p = skip_cfg;
    *p++ = 0x1f;
    *p++ = 0x1f;
    *p++ = 0xffffffff;
    *p++ = 0xffffffff;
    *p++ = 0x1f;
    *p++ = 0x1f;
    *p++ = 0xffffffff;
    *p++ = 0xffffffff;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 32, (void *)skip_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(skip_cfg);
    return rc;
}

static int mm_daemon_config_vfe_chroma_subs(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *chroma_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x4f8,
                .len = 12,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    chroma_cfg = (uint32_t *)malloc(12);
    p = chroma_cfg;
    *p++ = 0x30;
    *p++ = 0x0;
    *p = 0x0;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 12, (void *)chroma_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(chroma_cfg);
    return rc;
}

static int mm_daemon_config_vfe_sk_enhance(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *sk_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x418,
                .len = 136,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    sk_cfg = (uint32_t *)malloc(136);
    p = sk_cfg;
    *p++ = 0x110a28;
    *p++ = 0x461128;
    *p++ = 0x414628;
    *p++ = 0x2d4128;
    *p++ = 0xa2d28;
    *p++ = 0xfdece2;
    *p++ = 0xe7fde2;
    *p++ = 0xc9e7e2;
    *p++ = 0xa6c9e2;
    *p++ = 0xeca6e2;
    *p++ = 0x400;
    *p++ = 0x400;
    *p++ = 0x400;
    *p++ = 0x400;
    *p++ = 0x400;
    *p++ = 0x400;
    *p++ = 0x4000000;
    *p++ = 0x4000000;
    *p++ = 0x4000000;
    *p++ = 0x4000000;
    *p++ = 0x4000000;
    *p++ = 0x4000000;
    *p++ = 0xa00000;
    *p++ = 0xa00000;
    *p++ = 0xa00000;
    *p++ = 0xa00000;
    *p++ = 0xa00000;
    *p++ = 0xa00000;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p = 0x0;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 136, (void *)sk_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(sk_cfg);
    return rc;
}

static int mm_daemon_config_vfe_op_mode(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *op_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x14,
                .len = 4,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x10,
                .len = 4,
                .cmd_data_offset = 4,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x52c,
                .len = 4,
                .cmd_data_offset = 8,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x35c,
                .len = 4,
                .cmd_data_offset = 12,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x530,
                .len = 4,
                .cmd_data_offset = 16
            },
            .cmd_type = VFE_WRITE,
        },
    };

    op_cfg = (uint32_t *)malloc(20);
    p = op_cfg;

    *p++ = 0x00000211;
    *p++ = 0x01e27c17;
    *p++ = 0x00000000;
    *p++ = 0x00000000;
    *p = 0x00000001;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 20, (void *)op_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(op_cfg);
    return rc;
}

static int mm_daemon_config_vfe_asf_update(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *asf_updt, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x4A0,
                .len = 36,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    asf_updt = (uint32_t *)malloc(36);
    p = asf_updt;
    if (cfg_obj->current_streams & DAEMON_STREAM_TYPE_VIDEO) {
        *p++ = 0x0000506D;
        *p++ = 0x000C0C0D;
        *p++ = 0xD828D828;
    } else {
        *p++ = 0x00008005;
        *p++ = 0x0;
        *p++ = 0x0;
    }
    *p++ = 0x3c000000;
    *p++ = 0x408038;
    *p++ = 0x3c000000;
    *p++ = 0x438008;
    if (cfg_obj->current_streams & DAEMON_STREAM_TYPE_VIDEO) {
        *p++ = 0x02041040;
        *p = 0x00407047;
    } else {
        *p++ = 0x0;
        *p = 0x0;
    }

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 36, (void *)asf_updt,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(asf_updt);
    return rc;
}

static int mm_daemon_config_vfe_update(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *update_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x3c0,
                .len = 4,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x260,
                .len = 4,
                .cmd_data_offset = 4,
            },
            .cmd_type = VFE_WRITE_MB,
        },
    };

    update_cfg = (uint32_t *)malloc(8);
    p = update_cfg;
    *p++ = 0x1;
    *p = 0x1;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 8, (void *)update_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(update_cfg);
    return rc;
}

static int mm_daemon_config_vfe_stats_aec(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *aec_stats, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x534,
                .len = 8,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    aec_stats = (uint32_t *)malloc(8);
    p = aec_stats;
    *p++ = 0x50000000;
    *p = 0xff03b04f;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 8, (void *)aec_stats,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(aec_stats);
    return rc;
}

static int mm_daemon_config_vfe_stats_awb(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *awb_stats, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x54c,
                .len = 32,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    awb_stats = (uint32_t *)malloc(32);
    p = awb_stats;
    *p++ = 0x50000000;
    *p++ = 0xff03b04f;
    *p++ = 0x00000af1;
    *p++ = 0x0098005a;
    *p++ = 0x01010f9d;
    *p++ = 0xf010f010;
    *p++ = 0x4021203d;
    *p = 0x4009d082;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 32, (void *)awb_stats,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(awb_stats);
    return rc;
}

static int mm_daemon_config_vfe_mce(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *mce_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x3E8,
                .len = 36,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    mce_cfg = (uint32_t *)malloc(36);
    p = mce_cfg;
    *p++ = 0xEBC81E0A;
    *p++ = 0x99AA8000;
    *p++ = 0x001A7F2D;
    *p++ = 0xEBC82814;
    *p++ = 0x86AA8000;
    *p++ = 0x0013F400;
    *p++ = 0xFFEB9650;
    *p++ = 0x86AA8000;
    *p = 0x001319F4;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 36, (void *)mce_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(mce_cfg);
    return rc;
}

static int mm_daemon_config_start_stats(mm_daemon_cfg_t *cfg_obj)
{
    uint32_t i;
    int rc = -1;

    for (i = 0; i < ARRAY_SIZE(vfe_stats); i++)
        mm_daemon_config_isp_stats_req(cfg_obj, vfe_stats[i]);
    if ((mm_daemon_config_isp_stats_cfg(cfg_obj, 1)) == 0) {
        for (i = 0; i < ARRAY_SIZE(vfe_stats); i++) {
            switch (vfe_stats[i]) {
                case MSM_ISP_STATS_AEC:
                    rc = mm_daemon_config_vfe_stats_aec(cfg_obj);
                    break;
                case MSM_ISP_STATS_AWB:
                    rc = mm_daemon_config_vfe_stats_awb(cfg_obj);
                    break;
                default:
                    ALOGE("%s: unsupported stats type", __FUNCTION__);
                    break;
            }
        }
    }
    return rc;
}

static int mm_daemon_config_stop_stats(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t i;

    mm_daemon_config_isp_stats_cfg(cfg_obj, 0);
    for (i = 0; i < ARRAY_SIZE(vfe_stats); i++)
        rc = mm_daemon_config_isp_stats_release(cfg_obj, vfe_stats[i]);
    return rc;
}

static int mm_daemon_config_start_preview(mm_daemon_cfg_t *cfg_obj)
{
    cam_stream_type_t stream_type = CAM_STREAM_TYPE_PREVIEW;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj, stream_type);

    if (!buf)
        return -ENOMEM;
    cfg_obj->first_frame_rcvd = 0;
    if (buf->stream_info->num_bufs)
        mm_daemon_config_isp_buf_enqueue(cfg_obj, stream_type);
    mm_daemon_config_vfe_reset(cfg_obj);
    mm_daemon_config_vfe_roll_off(cfg_obj, stream_type);
    mm_daemon_config_vfe_fov(cfg_obj, stream_type);
    mm_daemon_config_vfe_main_scaler(cfg_obj, stream_type);
    mm_daemon_config_vfe_s2y(cfg_obj, stream_type);
    mm_daemon_config_vfe_s2cbcr(cfg_obj, stream_type);
    mm_daemon_config_vfe_axi(cfg_obj, stream_type);
    mm_daemon_config_vfe_chroma_en(cfg_obj);
    mm_daemon_config_vfe_color_cor(cfg_obj);
    mm_daemon_config_vfe_asf(cfg_obj, stream_type);
    mm_daemon_config_vfe_white_balance(cfg_obj, stream_type);
    mm_daemon_config_vfe_black_level(cfg_obj);
    mm_daemon_config_vfe_rgb_gamma(cfg_obj);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 2);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 4);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 6);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 12);
    mm_daemon_config_vfe_camif(cfg_obj, stream_type);
    mm_daemon_config_vfe_demux(cfg_obj, stream_type);
    mm_daemon_config_vfe_out_clamp(cfg_obj);
    mm_daemon_config_vfe_frame_skip(cfg_obj);
    mm_daemon_config_vfe_chroma_subs(cfg_obj);
    mm_daemon_config_vfe_sk_enhance(cfg_obj);
    mm_daemon_config_vfe_op_mode(cfg_obj);
    mm_daemon_config_start_stats(cfg_obj);
    mm_daemon_config_isp_input_cfg(cfg_obj);
    mm_daemon_config_isp_stream_request(cfg_obj, stream_type);
    return mm_daemon_config_isp_stream_cfg(cfg_obj, stream_type, START_STREAM);
}

static void mm_daemon_config_stop_preview(mm_daemon_cfg_t *cfg_obj)
{
    cam_stream_type_t stream_type = CAM_STREAM_TYPE_PREVIEW;

    mm_daemon_config_isp_stream_cfg(cfg_obj, stream_type, STOP_STREAM);
    mm_daemon_config_isp_stream_release(cfg_obj, stream_type);
    mm_daemon_config_vfe_stop(cfg_obj);
}

static int mm_daemon_config_start_snapshot(mm_daemon_cfg_t *cfg_obj)
{
    cam_stream_type_t stream_type = CAM_STREAM_TYPE_SNAPSHOT;

    mm_daemon_config_vfe_reset(cfg_obj);
    mm_daemon_config_vfe_roll_off(cfg_obj, stream_type);
    mm_daemon_config_vfe_fov(cfg_obj, stream_type);
    mm_daemon_config_vfe_main_scaler(cfg_obj, stream_type);
    mm_daemon_config_vfe_s2y(cfg_obj, stream_type);
    mm_daemon_config_vfe_s2cbcr(cfg_obj, stream_type);
    mm_daemon_config_vfe_axi(cfg_obj, stream_type);
    mm_daemon_config_vfe_chroma_en(cfg_obj);
    mm_daemon_config_vfe_color_cor(cfg_obj);
    mm_daemon_config_vfe_asf(cfg_obj, stream_type);
    mm_daemon_config_vfe_white_balance(cfg_obj, stream_type);
    mm_daemon_config_vfe_black_level(cfg_obj);
    mm_daemon_config_vfe_rgb_gamma(cfg_obj);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 2);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 4);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 6);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 12);
    mm_daemon_config_vfe_camif(cfg_obj, stream_type);
    mm_daemon_config_vfe_demux(cfg_obj, stream_type);
    mm_daemon_config_vfe_out_clamp(cfg_obj);
    mm_daemon_config_vfe_chroma_subs(cfg_obj);
    mm_daemon_config_vfe_sk_enhance(cfg_obj);
    mm_daemon_config_vfe_op_mode(cfg_obj);
    mm_daemon_config_start_stats(cfg_obj);
    mm_daemon_config_isp_stream_request(cfg_obj, CAM_STREAM_TYPE_POSTVIEW);
    mm_daemon_config_isp_stream_request(cfg_obj, stream_type);
    return mm_daemon_config_isp_stream_cfg(cfg_obj, stream_type, START_STREAM);
}

static void mm_daemon_config_stop_snapshot(mm_daemon_cfg_t *cfg_obj)
{
    mm_daemon_config_isp_stream_cfg(cfg_obj, CAM_STREAM_TYPE_SNAPSHOT,
            STOP_STREAM);
    mm_daemon_config_isp_stream_release(cfg_obj, CAM_STREAM_TYPE_POSTVIEW);
    mm_daemon_config_isp_stream_release(cfg_obj, CAM_STREAM_TYPE_SNAPSHOT);
    mm_daemon_config_vfe_stop(cfg_obj);
}

static int mm_daemon_config_start_video(mm_daemon_cfg_t *cfg_obj)
{
    cam_stream_type_t stream_type = CAM_STREAM_TYPE_VIDEO;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj, stream_type);

    if (!buf)
        return -ENOMEM;
    cfg_obj->first_frame_rcvd = 0;
    if (buf->stream_info->num_bufs)
        mm_daemon_config_isp_buf_enqueue(cfg_obj, stream_type);
    mm_daemon_config_isp_input_cfg(cfg_obj);
    mm_daemon_config_isp_stream_request(cfg_obj, stream_type);
    return mm_daemon_config_isp_stream_cfg(cfg_obj, stream_type, START_STREAM);
}

static void mm_daemon_config_stop_video(mm_daemon_cfg_t *cfg_obj)
{
    cam_stream_type_t stream_type = CAM_STREAM_TYPE_VIDEO;

    mm_daemon_config_isp_stream_cfg(cfg_obj, stream_type, STOP_STREAM);
    mm_daemon_config_isp_stream_release(cfg_obj, stream_type);
}

static void mm_daemon_config_isp_evt_sof(mm_daemon_cfg_t *cfg_obj)
{
    if (cfg_obj->current_streams & DAEMON_STREAM_TYPE_PREVIEW &&
            !cfg_obj->first_frame_rcvd) {
        mm_daemon_config_vfe_fov(cfg_obj, CAM_STREAM_TYPE_PREVIEW);
        mm_daemon_config_vfe_main_scaler(cfg_obj, CAM_STREAM_TYPE_PREVIEW);
        mm_daemon_config_vfe_s2y(cfg_obj, CAM_STREAM_TYPE_PREVIEW);
        mm_daemon_config_vfe_s2cbcr(cfg_obj, CAM_STREAM_TYPE_PREVIEW);
        mm_daemon_config_vfe_asf_update(cfg_obj);
        mm_daemon_config_vfe_mce(cfg_obj);
        mm_daemon_config_vfe_update(cfg_obj);
        cfg_obj->first_frame_rcvd = 1;
    }
}

static int mm_daemon_config_isp_evt(mm_daemon_cfg_t *cfg_obj,
        struct v4l2_event *isp_event)
{
    int rc = 0;

    switch (isp_event->type) {
        case ISP_EVENT_SOF:
            mm_daemon_config_isp_evt_sof(cfg_obj);
            break;
        case ISP_EVENT_STATS_NOTIFY + MSM_ISP_STATS_AEC:
            rc = mm_daemon_config_isp_stats_proc_aec(cfg_obj, isp_event);
            break;
        case ISP_EVENT_COMP_STATS_NOTIFY:
            break;
        default:
            ALOGI("%s: Unknown event %d", __FUNCTION__,
                    isp_event->type);
            break;
    }
    return rc;
}

static int mm_daemon_config_dequeue(mm_daemon_obj_t *mm_obj,
        mm_daemon_cfg_t *cfg_obj)
{
    struct v4l2_event isp_event;
    int rc = 0;

    memset(&isp_event, 0, sizeof(isp_event));
    rc = ioctl(cfg_obj->vfe_fd, VIDIOC_DQEVENT, &isp_event);
    if (rc < 0) {
        ALOGE("%s: Error dequeueing event", __FUNCTION__);
        mm_obj->cfg_state = STATE_STOPPED;
        return rc;
    }

    return mm_daemon_config_isp_evt(cfg_obj, &isp_event);
}

static int mm_daemon_config_pipe_cmd(mm_daemon_obj_t *mm_obj,
        mm_daemon_cfg_t *cfg_obj, mm_daemon_sensor_t *mm_snsr)
{
    int rc = 0;
    ssize_t read_len;
    int stream_id, idx;
    cam_stream_type_t stream_type;
    mm_daemon_pipe_evt_t pipe_cmd;
    mm_daemon_buf_info *buf = NULL;

    read_len = read(mm_obj->cfg_pfds[0], &pipe_cmd, sizeof(pipe_cmd));
    stream_id = pipe_cmd.val;
    switch (pipe_cmd.cmd) {
        case CFG_CMD_STREAM_START:
            idx = mm_daemon_get_stream_idx(cfg_obj, stream_id);
            if (idx < 0)
                return idx;
            buf = cfg_obj->stream_buf[idx];
            if (buf->streamon == 0) {
                stream_type = buf->stream_info->stream_type;
                switch (stream_type) {
                    case CAM_STREAM_TYPE_PREVIEW:
                        mm_daemon_config_sensor_cmd(mm_snsr,
                                SENSOR_CMD_PREVIEW, 0, 1);
                        rc = mm_daemon_config_start_preview(cfg_obj);
                        break;
                    case CAM_STREAM_TYPE_VIDEO:
                        rc = mm_daemon_config_start_video(cfg_obj);
                        break;
                    case CAM_STREAM_TYPE_SNAPSHOT:
                    case CAM_STREAM_TYPE_POSTVIEW:
                    case CAM_STREAM_TYPE_METADATA:
                    case CAM_STREAM_TYPE_OFFLINE_PROC:
                        if (buf->stream_info->num_bufs)
                            mm_daemon_config_isp_buf_enqueue(cfg_obj,
                                    stream_type);
                        if (stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
                            mm_daemon_config_sensor_cmd(mm_snsr,
                                    SENSOR_CMD_SNAPSHOT, 0, 1);
                            rc = mm_daemon_config_start_snapshot(cfg_obj);
                        }
                        break;
                    default:
                        ALOGE("%s: unknown stream type %d",
                                __FUNCTION__, stream_type);
                        rc = -1;
                        break;
                }
                if (rc == 0)
                    buf->streamon = 1;
            }
            break;
        case CFG_CMD_STREAM_STOP:
            idx = mm_daemon_get_stream_idx(cfg_obj, stream_id);
            if (idx < 0)
                return idx;
            buf = cfg_obj->stream_buf[idx];
            if (buf->streamon && buf->stream_info) {
                stream_type = buf->stream_info->stream_type;
                switch (stream_type) {
                    case CAM_STREAM_TYPE_PREVIEW:
                        mm_daemon_config_stop_preview(cfg_obj);
                        break;
                    case CAM_STREAM_TYPE_POSTVIEW:
                        break;
                    case CAM_STREAM_TYPE_SNAPSHOT:
                        mm_daemon_config_stop_snapshot(cfg_obj);
                        break;
                    case CAM_STREAM_TYPE_VIDEO:
                        mm_daemon_config_stop_video(cfg_obj);
                        break;
                    case CAM_STREAM_TYPE_METADATA:
                        mm_daemon_config_stop_stats(cfg_obj);
                        break;
                    default:
                        ALOGE("%s: unknown stream type %d",
                                __FUNCTION__, stream_type);
                        break;
                }
                buf->streamon = 0;
            }
            break;
        case CFG_CMD_NEW_STREAM:
            for (idx = 0; idx < MAX_NUM_STREAM; idx++) {
                if (cfg_obj->stream_buf[idx] == NULL)
                    break;
            }
            if (idx == MAX_NUM_STREAM)
                return -ENOMEM;
            mm_daemon_config_sensor_cmd(mm_snsr, SENSOR_CMD_POWER_UP, 0, 0);
            cfg_obj->stream_buf[idx] = (mm_daemon_buf_info *)malloc(
                    sizeof(mm_daemon_buf_info));
            memset(cfg_obj->stream_buf[idx], 0, sizeof(mm_daemon_buf_info));
            cfg_obj->stream_buf[idx]->stream_id = stream_id;
            break;
        case CFG_CMD_DEL_STREAM:
            idx = mm_daemon_get_stream_idx(cfg_obj, stream_id);
            if (idx < 0)
                return idx;
            if (cfg_obj->stream_buf[idx]) {
                free(cfg_obj->stream_buf[idx]);
                cfg_obj->stream_buf[idx] = NULL;
                break;
            }
        case CFG_CMD_SHUTDOWN:
            rc = -1;
        default:
            break;
    }
    return rc;
}

static mm_daemon_socket_t *mm_daemon_socket_create(
        mm_daemon_obj_t *mm_obj, char *device_name)
{
    struct sockaddr_un sockaddr;
    mm_daemon_socket_t *sock_obj = NULL;
    int sock_fd;
    int dev_id;

    sscanf(device_name, "/dev/video%u", &dev_id);
    sock_obj = (mm_daemon_socket_t *)malloc(
            sizeof(mm_daemon_socket_t));
    if (sock_obj == NULL)
        return NULL;

    sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        ALOGE("%s: Error creating socket", __FUNCTION__);
        free(sock_obj);
        return NULL;
    }
    memset(sock_obj, 0, sizeof(mm_daemon_socket_t));
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sun_family = AF_UNIX;
    snprintf(sockaddr.sun_path, sizeof(sockaddr.sun_path),
            "/data/cam_socket%d", dev_id);
    if ((unlink(sockaddr.sun_path)) != 0 && errno != ENOENT)
        ALOGE("%s: failed to unlink socket '%s': %s", __FUNCTION__,
                sockaddr.sun_path, strerror(errno));
    else {
        if ((bind(sock_fd, (struct sockaddr *)&sockaddr,
                sizeof(sockaddr))) == 0) {
            ALOGI("%s: bound sock_fd=%d %s", __FUNCTION__, sock_fd,
                    sockaddr.sun_path);
            chmod(sockaddr.sun_path, 0660);
            sock_obj->sock_fd = sock_fd;
            sock_obj->daemon_obj = mm_obj;
            sock_obj->sock_addr = sockaddr;
            sock_obj->dev_id = dev_id;
            return sock_obj;
        } else
            ALOGE("%s: unable to bind socket", __FUNCTION__);
    }
        
    close(sock_fd);
    free(sock_obj);
    return NULL;
}

static int mm_daemon_proc_packet_map(mm_daemon_socket_t *mm_sock,
        cam_sock_packet_t *packet, int rcvd_fd)
{
    struct v4l2_event ev;
    struct ion_fd_data ion_data;
    int idx, stream_id, buf_idx, rc = 0;
    mm_daemon_buf_info *buf = NULL;
    mm_daemon_obj_t *mm_obj = (mm_daemon_obj_t *)mm_sock->daemon_obj;
    mm_daemon_cfg_t *cfg_obj = mm_sock->cfg_obj;

    memset(&ev, 0, sizeof(ev));
    memset(&ion_data, 0, sizeof(ion_data));
    stream_id = packet->payload.buf_map.stream_id;
    buf_idx = packet->payload.buf_map.frame_idx;
    switch (packet->payload.buf_map.type) {
        case CAM_MAPPING_BUF_TYPE_CAPABILITY:
            if (mm_obj->cap_buf.mapped)
                return -EINVAL;
            mm_obj->cap_buf.vaddr = mmap(0, sizeof(cam_capability_t),
                    PROT_READ|PROT_WRITE, MAP_SHARED, rcvd_fd, 0);
            if (mm_obj->cap_buf.vaddr == MAP_FAILED) {
                close(rcvd_fd);
                rc = -1;
            } else {
                mm_obj->cap_buf.fd = rcvd_fd;
                mm_obj->cap_buf.mapped = 1;
            }
            break;
        case CAM_MAPPING_BUF_TYPE_PARM_BUF:
            mm_obj->parm_buf.buf = (parm_buffer_t *)mmap(0,
                    sizeof(parm_buffer_t), PROT_READ|PROT_WRITE, MAP_SHARED,
                    rcvd_fd, 0);
            if (mm_obj->parm_buf.buf == MAP_FAILED) {
                close(rcvd_fd);
                rc = -1;
            } else {
                mm_obj->parm_buf.fd = rcvd_fd;
                mm_obj->parm_buf.mapped = 1;
            }
            break;
        case CAM_MAPPING_BUF_TYPE_STREAM_BUF:
            idx = mm_daemon_get_stream_idx(cfg_obj, stream_id);
            if (idx < 0) {
                rc = idx;
                break;
            }
            buf = cfg_obj->stream_buf[idx];
            if (buf_idx < CAM_MAX_NUM_BUFS_PER_STREAM) {
                buf->buf_data[buf_idx].fd = rcvd_fd;
                buf->buf_data[buf_idx].len = packet->payload.buf_map.size;
                buf->num_stream_bufs++;
            }
            break;
        case CAM_MAPPING_BUF_TYPE_STREAM_INFO:
            idx = mm_daemon_get_stream_idx(cfg_obj, stream_id);
            if (idx >= 0)
                buf = cfg_obj->stream_buf[idx];
            if (buf == NULL) {
                ALOGE("%s: no buffer for stream %d", __FUNCTION__, stream_id);
                close(rcvd_fd);
                break;
            }
            buf->stream_info =
                    (cam_stream_info_t *)mmap(0, sizeof(cam_stream_info_t),
                    PROT_READ|PROT_WRITE, MAP_SHARED, rcvd_fd, 0);
            if (buf->stream_info == MAP_FAILED) {
                ALOGE("%s: stream_info mapping failed for stream %d",
                        __FUNCTION__, mm_obj->stream_id);
                close(rcvd_fd);
                break;
            }
            buf->stream_info_mapped = 1;
            buf->fd = rcvd_fd;
            if (buf->stream_info->stream_type == CAM_STREAM_TYPE_METADATA)
                buf->output_format = V4L2_PIX_FMT_META;
            else
                buf->output_format =
                        mm_daemon_config_v4l2_fmt(buf->stream_info->fmt);
            mm_daemon_config_set_current_streams(cfg_obj,
                    buf->stream_info->stream_type, 1);
            cfg_obj->num_streams++;
            rc = mm_daemon_config_isp_buf_request(cfg_obj, buf, stream_id);
            break;
        default:
            break;
    }
    mm_daemon_config_server_cmd(mm_obj, SERVER_CMD_MAP_UNMAP_DONE, stream_id);
    return rc;
}

static int mm_daemon_proc_packet_unmap(mm_daemon_socket_t *mm_sock,
        cam_sock_packet_t *packet)
{
    struct v4l2_event ev;
    int idx, stream_id, buf_idx, rc = 0;
    mm_daemon_buf_info *buf = NULL;
    mm_daemon_obj_t *mm_obj = (mm_daemon_obj_t *)mm_sock->daemon_obj;
    mm_daemon_cfg_t *cfg_obj = mm_sock->cfg_obj;

    memset(&ev, 0, sizeof(ev));
    stream_id = packet->payload.buf_unmap.stream_id;
    
    switch (packet->payload.buf_unmap.type) {
        case CAM_MAPPING_BUF_TYPE_CAPABILITY:
            if (!mm_obj->cap_buf.mapped)
                ALOGE("%s: capability buf not mapped", __FUNCTION__);
            else {
                if (munmap(mm_obj->cap_buf.vaddr, sizeof(cam_capability_t)))
                    ALOGE("%s: Failed to unmap memory at %p : %s",
                            __FUNCTION__, mm_obj->cap_buf.vaddr,
                            strerror(errno));
                close(mm_obj->cap_buf.fd);
                mm_obj->cap_buf.fd = 0;
                mm_obj->cap_buf.mapped = 0;
            }
            break;
        case CAM_MAPPING_BUF_TYPE_PARM_BUF:
            if (!mm_obj->parm_buf.mapped)
                ALOGE("%s: parm buf not mapped", __FUNCTION__);
            else {
                if (munmap(mm_obj->parm_buf.buf, sizeof(parm_buffer_t)))
                    ALOGE("%s: Failed to unmap memory at %p : %s",
                            __FUNCTION__, mm_obj->parm_buf.buf,
                            strerror(errno));
                close(mm_obj->parm_buf.fd);
                mm_obj->parm_buf.fd = 0;
                mm_obj->parm_buf.mapped = 0;
            }
            break;
        case CAM_MAPPING_BUF_TYPE_STREAM_INFO:
            idx = mm_daemon_get_stream_idx(cfg_obj, stream_id);
            if (idx >= 0)
                buf = cfg_obj->stream_buf[idx];
            if (buf && buf->stream_info_mapped) {
                mm_daemon_config_isp_buf_release(cfg_obj,
                        buf->stream_info->stream_type);
                for (buf_idx = 0; buf_idx < buf->num_stream_bufs; buf_idx++) {
                    if (buf->stream_info->stream_type ==
                            CAM_STREAM_TYPE_METADATA) {
                        if (buf->buf_data[buf_idx].mapped)
                            munmap(buf->buf_data[buf_idx].vaddr,
                                    sizeof(cam_metadata_info_t));
                    }
                    close(buf->buf_data[buf_idx].fd);
                    buf->buf_data[buf_idx].mapped = 0;
                    buf->buf_data[buf_idx].fd = 0;
                }
                mm_daemon_config_set_current_streams(cfg_obj,
                        buf->stream_info->stream_type, 0);
                if (munmap(buf->stream_info, sizeof(cam_stream_info_t)))
                    ALOGE("%s Failed to unmap memory at %p : %s",
                            __FUNCTION__, buf->stream_info,
                            strerror(errno));
                buf->stream_info = NULL;
                close(buf->fd);
                buf->fd = 0;
                buf->stream_info_mapped = 0;
            }
            cfg_obj->num_streams--;
            break;
        default:
            break;
    }
    mm_daemon_config_server_cmd(mm_obj, SERVER_CMD_MAP_UNMAP_DONE, stream_id);
    return rc;
}

static int mm_daemon_proc_packet(mm_daemon_socket_t *mm_sock,
        cam_sock_packet_t *packet, int rcvd_fd)
{
    int rc = -1;

    switch (packet->msg_type) {
    case CAM_MAPPING_TYPE_FD_MAPPING:
        rc = mm_daemon_proc_packet_map(mm_sock, packet, rcvd_fd);
        break;
    case CAM_MAPPING_TYPE_FD_UNMAPPING:
        rc = mm_daemon_proc_packet_unmap(mm_sock, packet);
        break;
    default:
        break;
    }
    return rc;
}

static void *mm_daemon_sock_thread(void *data)
{
    mm_daemon_socket_t *mm_sock = (mm_daemon_socket_t *)data;
    cam_sock_packet_t packet;
    struct msghdr msgh;
    struct iovec iov[1];
    struct cmsghdr *cmsghp = NULL;
    char control[CMSG_SPACE(sizeof(int))];
    int rcvd_fd = -1;
    int rc = 0;

    mm_sock->state = SOCKET_STATE_CONNECTED;

    do {
        memset(&msgh, 0, sizeof(msgh));
        memset(&packet, 0, sizeof(cam_sock_packet_t));
        msgh.msg_name = NULL;
        msgh.msg_namelen = 0;
        msgh.msg_control = control;
        msgh.msg_controllen = sizeof(control);

        iov[0].iov_base = &packet;
        iov[0].iov_len = sizeof(cam_sock_packet_t);

        msgh.msg_iov = iov;
        msgh.msg_iovlen = 1;

        rc = recvmsg(mm_sock->sock_fd, &(msgh), 0);
        if (((cmsghp = CMSG_FIRSTHDR(&msgh)) != NULL) &&
                (cmsghp->cmsg_len == CMSG_LEN(sizeof(int)))) {
            if (cmsghp->cmsg_level == SOL_SOCKET &&
                    cmsghp->cmsg_type == SCM_RIGHTS)
                rcvd_fd = *((int *)CMSG_DATA(cmsghp));
            else
                ALOGE("%s: Unexpected Control Msg", __FUNCTION__);
        }

        if ((mm_daemon_proc_packet(mm_sock, &packet, rcvd_fd)) < 0)
            mm_sock->state = SOCKET_STATE_DISCONNECTED;
    } while (mm_sock->state == SOCKET_STATE_CONNECTED);
    return NULL;
}

static int mm_daemon_start_sock_thread(mm_daemon_socket_t *mm_sock)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&mm_sock->pid, &attr,
            mm_daemon_sock_thread, (void *)mm_sock);
    pthread_attr_destroy(&attr);
    return 0;
}

static void mm_daemon_config_sock_close(mm_daemon_socket_t *mm_sock)
{
    int socket_fd;
    struct sockaddr_un sock_addr;
    struct msghdr msgh;
    struct iovec iov[1];
    void *rc;
    cam_sock_packet_t packet;

    socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd < 0)
        return;

    memset(&packet, 0, sizeof(packet));
    memset(&sock_addr, 0, sizeof(sock_addr));
    memset(&msgh, 0, sizeof(msgh));
    sock_addr.sun_family = AF_UNIX;
    snprintf(sock_addr.sun_path, sizeof(sock_addr.sun_path),
            "/data/cam_socket%d", mm_sock->dev_id);
    if ((connect(socket_fd, (struct sockaddr *)&sock_addr,
            sizeof(sock_addr))) != 0) {
        close(socket_fd);
        return;
    }

    packet.msg_type = CAM_MAPPING_TYPE_MAX;
    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    iov[0].iov_base = &packet;
    iov[0].iov_len = sizeof(packet);
    msgh.msg_iov = iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = NULL;
    msgh.msg_controllen = 0;

    sendmsg(socket_fd, &(msgh), 0);
    pthread_join(mm_sock->pid, &rc);
    close(socket_fd);
}

static void mm_daemon_config_buf_close(mm_daemon_obj_t *mm_obj,
        mm_daemon_cfg_t *cfg_obj)
{
    int i, j;
    mm_daemon_buf_info *buf = NULL;
    for (i = 0; i < MAX_NUM_STREAM; i++) {
        if (cfg_obj->stream_buf[i] == NULL)
            continue;
        buf = cfg_obj->stream_buf[i];
        if (buf->streamon && buf->stream_info_mapped &&
                buf->stream_info->stream_type == CAM_STREAM_TYPE_PREVIEW)
            mm_daemon_config_stop_preview(cfg_obj);
        if (buf->stream_info_mapped)
            mm_daemon_config_isp_buf_release(cfg_obj,
                    buf->stream_info->stream_type);
        if (buf->num_stream_bufs > 0) {
            for (j = 0; j < CAM_MAX_NUM_BUFS_PER_STREAM; j++) {
                if (buf->buf_data[j].mapped) {
                    close(buf->buf_data[j].fd);
                    buf->buf_data[j].mapped = 0;
                    buf->buf_data[j].fd = 0;
                }
            }
        }
        if (buf->stream_info_mapped) {
            munmap(buf->stream_info, sizeof(cam_stream_info_t));
            buf->stream_info = NULL;
            close(buf->fd);
            buf->fd = 0;
            buf->stream_info_mapped = 0;
        }
        free(buf);
        cfg_obj->stream_buf[i] = NULL;
    }
    if (mm_obj->cap_buf.mapped) {
        munmap(mm_obj->cap_buf.vaddr, sizeof(cam_capability_t));
        close(mm_obj->cap_buf.fd);
        mm_obj->cap_buf.fd = 0;
        mm_obj->cap_buf.mapped = 0;
    }
    if (mm_obj->parm_buf.mapped) {
        munmap(mm_obj->parm_buf.buf, sizeof(parm_buffer_t));
        close(mm_obj->parm_buf.fd);
        mm_obj->parm_buf.fd = 0;
        mm_obj->parm_buf.mapped = 0;
    }
}

static void *mm_daemon_config_thread(void *data)
{
    uint32_t i;
    struct pollfd fds[2];
    mm_daemon_socket_t *mm_sock = NULL;
    mm_daemon_sensor_t *mm_snsr = NULL;
    mm_daemon_obj_t *mm_obj = (mm_daemon_obj_t *)data;
    mm_daemon_cfg_t *cfg_obj = NULL;

    pthread_mutex_lock(&mm_obj->cfg_lock);
    cfg_obj = (mm_daemon_cfg_t *)malloc(sizeof(mm_daemon_cfg_t));

    memset(cfg_obj, 0, sizeof(mm_daemon_cfg_t));
    pthread_mutex_lock(&mm_obj->mutex);
    pthread_mutex_init(&(cfg_obj->lock), NULL);
    cfg_obj->session_id = mm_obj->session_id;
    cfg_obj->vfe_fd = open(mm_daemon_server_find_subdev(
                MSM_CONFIGURATION_NAME, MSM_CAMERA_SUBDEV_VFE,
                MEDIA_ENT_T_V4L2_SUBDEV), O_RDWR | O_NONBLOCK);
    if (cfg_obj->vfe_fd <= 0) {
        ALOGE("%s: failed to open VFE device", __FUNCTION__);
        pthread_cond_signal(&mm_obj->cond);
        pthread_mutex_unlock(&mm_obj->mutex);
        goto cfg_close;
    }
    mm_sock = mm_daemon_socket_create(mm_obj,
            mm_daemon_server_find_subdev(MSM_CAMERA_NAME,
            QCAMERA_VNODE_GROUP_ID, MEDIA_ENT_T_DEVNODE_V4L));

    if (mm_sock == NULL) {
        ALOGE("%s: failed to create socket", __FUNCTION__);
        pthread_cond_signal(&mm_obj->cond);
        pthread_mutex_unlock(&mm_obj->mutex);
        goto vfe_close;
    }
    mm_sock->cfg_obj = cfg_obj;
    mm_daemon_start_sock_thread(mm_sock);

    mm_snsr = mm_daemon_sensor_open();

    for (i = 0; i < ARRAY_SIZE(isp_events); i++) {
        mm_daemon_config_subscribe(cfg_obj, isp_events[i], 1);
    }

    cfg_obj->curr_gain = mm_snsr->curr_gain;

    cfg_obj->ion_fd = open("/dev/ion", O_RDONLY);

    mm_obj->cfg_state = STATE_POLL;
    pthread_cond_signal(&mm_obj->cond);
    pthread_mutex_unlock(&mm_obj->mutex);
    fds[0].fd = cfg_obj->vfe_fd;
    fds[1].fd = mm_obj->cfg_pfds[0];
    do {
        for (i = 0; i < 2; i++)
            fds[i].events = POLLIN|POLLRDNORM|POLLPRI;
        if (poll(fds, 2, -1) > 0) {
            if (fds[0].revents & POLLPRI)
                mm_daemon_config_dequeue(mm_obj, cfg_obj);
            else if ((fds[1].revents & POLLIN) &&
                    (fds[1].revents & POLLRDNORM)) {
                if ((mm_daemon_config_pipe_cmd(mm_obj, cfg_obj,
                        mm_snsr)) < 0) {
                    mm_daemon_config_sensor_cmd(mm_snsr,
                            SENSOR_CMD_SHUTDOWN, 0, 0);
                    mm_obj->cfg_state = STATE_STOPPED;
                    break;
                }
            } else
                usleep(1000);
            if (cfg_obj->gain_changed) {
                mm_daemon_config_sensor_cmd(mm_snsr, SENSOR_CMD_GAIN_UPDATE,
                        cfg_obj->curr_gain, 0);
                mm_daemon_config_sensor_cmd(mm_snsr, SENSOR_CMD_EXP_GAIN,
                        0x3D4, 0);
                cfg_obj->gain_changed = 0;
            }
        } else {
            usleep(100);
            continue;
        }
    } while (mm_obj->cfg_state == STATE_POLL);

    mm_daemon_config_sock_close(mm_sock);
    if (mm_sock != NULL) {
        unlink(mm_sock->sock_addr.sun_path);
        close(mm_sock->sock_fd);
        free(mm_sock);
    }
    mm_daemon_config_buf_close(mm_obj, cfg_obj);

    if (cfg_obj->ion_fd > 0) {
        close(cfg_obj->ion_fd);
        cfg_obj->ion_fd = 0;
    }
    if (mm_snsr) {
        mm_daemon_sensor_close(mm_snsr);
        free(mm_snsr);
    }
vfe_close:
    if (cfg_obj->vfe_fd > 0) {
        close(cfg_obj->vfe_fd);
        cfg_obj->vfe_fd = 0;
    }
cfg_close:
    pthread_mutex_unlock(&mm_obj->cfg_lock);
    pthread_mutex_destroy(&(cfg_obj->lock));
    free(cfg_obj);
    return NULL;
}

int mm_daemon_config_open(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    rc = pipe(mm_obj->cfg_pfds);
    if (rc < 0)
        return rc;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&mm_obj->cfg_pid, &attr, mm_daemon_config_thread,
            (void *)mm_obj);
    pthread_attr_destroy(&attr);
    return rc;
}

int mm_daemon_config_close(mm_daemon_obj_t *mm_obj)
{
    void *rc;

    pthread_join(mm_obj->cfg_pid, &rc);
    return (int)rc;
}
