/*
   Copyright (C) 2014-2018 Brian Stepp
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
#include "mm_daemon_sock.h"
#include "mm_daemon_csi.h"
#include "mm_daemon_led.h"
#include "mm_daemon_actuator.h"
#include "mm_daemon_util.h"

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
    MSM_ISP_STATS_AF,
    MSM_ISP_STATS_AWB,
};

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

static void mm_daemon_config_parm_flash(mm_daemon_cfg_t *cfg_obj, int32_t mode)
{
    mm_daemon_thread_info *led = cfg_obj->info[LED_DEV];
    enum msm_camera_led_config_t fl = MSM_CAMERA_LED_OFF;

    if (!led || mode != CAM_FLASH_MODE_OFF || mode != CAM_FLASH_MODE_TORCH)
        return;

    if (mode == CAM_FLASH_MODE_TORCH)
        fl = MSM_CAMERA_LED_TORCH;

    mm_daemon_util_subdev_cmd(led, LED_CMD_CONTROL, fl, FALSE);
}

static void mm_daemon_config_parm(mm_daemon_cfg_t *cfg_obj)
{
    int next = 1;
    int current, position;
    parm_buffer_t *c_table = cfg_obj->parm_buf.cfg_buf;
    parm_buffer_t *p_table = cfg_obj->parm_buf.buf;

    c_table = cfg_obj->parm_buf.cfg_buf;
    p_table = cfg_obj->parm_buf.buf;
    current = GET_FIRST_PARAM_ID(p_table);
    position = current;
    while (next) {
        switch (position) {
            case CAM_INTF_PARM_HAL_VERSION: {
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_ANTIBANDING: {
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue) {
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                    if (cfg_obj->sdata->uses_sensor_ctrls)
                        mm_daemon_util_subdev_cmd(cfg_obj->info[SNSR_DEV],
                                SENSOR_CMD_AB, *cvalue, FALSE);
                }
                break;
            }
            case CAM_INTF_PARM_EXPOSURE_COMPENSATION: {
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_AEC_LOCK: {
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_FPS_RANGE: {
                cam_fps_range_t *cvalue =
                        (cam_fps_range_t *)POINTER_OF(current, c_table);
                cam_fps_range_t *pvalue =
                        (cam_fps_range_t *)POINTER_OF(current, p_table);
                if (cvalue->min_fps != pvalue->min_fps ||
                        cvalue->max_fps != pvalue->max_fps)
                    memcpy(cvalue, pvalue, sizeof(cam_fps_range_t));
                break;
            }
            case CAM_INTF_PARM_AWB_LOCK: {
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_WHITE_BALANCE: {
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue) {
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                    if (cfg_obj->sdata->uses_sensor_ctrls)
                        mm_daemon_util_subdev_cmd(cfg_obj->info[SNSR_DEV],
                                SENSOR_CMD_WB, *cvalue, FALSE);
                }
                break;
            }
            case CAM_INTF_PARM_EFFECT: {
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue) {
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                    if (cfg_obj->sdata->uses_sensor_ctrls)
                        mm_daemon_util_subdev_cmd(cfg_obj->info[SNSR_DEV],
                                SENSOR_CMD_EFFECT, *cvalue, FALSE);
                }
                break;
            }
            case CAM_INTF_PARM_BESTSHOT_MODE: {
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_DIS_ENABLE: {
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_LED_MODE: { /* 10 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue) {
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                    mm_daemon_config_parm_flash(cfg_obj, *cvalue);
                }
                break;
            }
            case CAM_INTF_PARM_SHARPNESS: { /* 16 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue) {
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                    if (cfg_obj->sdata->uses_sensor_ctrls)
                        mm_daemon_util_subdev_cmd(cfg_obj->info[SNSR_DEV],
                                SENSOR_CMD_SHARPNESS, *cvalue, FALSE);
                }
                break;
            }
            case CAM_INTF_PARM_CONTRAST: { /* 17 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue) {
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                    if (cfg_obj->sdata->uses_sensor_ctrls)
                        mm_daemon_util_subdev_cmd(cfg_obj->info[SNSR_DEV],
                                SENSOR_CMD_CONTRAST, *cvalue, FALSE);
                }
                break;
            }
            case CAM_INTF_PARM_SATURATION: { /* 18 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue) {
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                    if (cfg_obj->sdata->uses_sensor_ctrls)
                        mm_daemon_util_subdev_cmd(cfg_obj->info[SNSR_DEV],
                                SENSOR_CMD_SATURATION, *cvalue, FALSE);
                }
                break;
            }
            case CAM_INTF_PARM_BRIGHTNESS: { /* 19 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue) {
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                    if (cfg_obj->sdata->uses_sensor_ctrls)
                        mm_daemon_util_subdev_cmd(cfg_obj->info[SNSR_DEV],
                                SENSOR_CMD_BRIGHTNESS, *cvalue, FALSE);
                }
                break;
            }
            case CAM_INTF_PARM_ISO: { /* 20 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_ZOOM: { /* 21 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_ROLLOFF: { /* 22 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_AEC_ALGO_TYPE: { /* 24 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_FOCUS_ALGO_TYPE: { /* 25 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_FOCUS_MODE: { /* 28 */
                uint8_t *cvalue = (uint8_t *)POINTER_OF(current, c_table);
                uint8_t *pvalue = (uint8_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(uint8_t));
                break;
            }
            case CAM_INTF_PARM_SCE_FACTOR: { /* 29 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_MCE: { /* 31 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_HFR: { /* 32 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_REDEYE_REDUCTION: { /* 33 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_ASD_ENABLE: { /* 36 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_RECORDING_HINT: { /* 37 */
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_HDR: { /* 38 */
                cam_hdr_param_t *cvalue =
                        (cam_hdr_param_t *)POINTER_OF(current, c_table);
                cam_hdr_param_t *pvalue =
                        (cam_hdr_param_t *)POINTER_OF(current, p_table);
                if (cvalue->hdr_enable != pvalue->hdr_enable ||
                        cvalue->hdr_need_1x != pvalue->hdr_need_1x ||
                        cvalue->hdr_mode != pvalue->hdr_mode)
                    memcpy(cvalue, pvalue, sizeof(cam_hdr_param_t));
                break;
            }
            case CAM_INTF_PARM_ZSL_MODE: {
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_SET_PP_COMMAND: {
                tune_cmd_t *cvalue =
                        (tune_cmd_t *)POINTER_OF(current, c_table);
                tune_cmd_t *pvalue =
                        (tune_cmd_t *)POINTER_OF(current, p_table);
                if (cvalue->module != pvalue->module ||
                        cvalue->type != cvalue->type ||
                        cvalue->value != pvalue->value)
                    memcpy(cvalue, pvalue, sizeof(tune_cmd_t));
                break;
            }
            case CAM_INTF_PARM_TINTLESS: {
                int32_t *cvalue = (int32_t *)POINTER_OF(current, c_table);
                int32_t *pvalue = (int32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(int32_t));
                break;
            }
            case CAM_INTF_PARM_STATS_DEBUG_MASK: {
                uint32_t *cvalue = (uint32_t *)POINTER_OF(current, c_table);
                uint32_t *pvalue = (uint32_t *)POINTER_OF(current, p_table);
                if (*cvalue != *pvalue)
                    memcpy(cvalue, pvalue, sizeof(uint32_t));
                break;
            }
            default:
                ALOGI("%s: unknown parm %d", __FUNCTION__, position);
                next = 0;
                break;
        }
        position = GET_NEXT_PARAM_ID(current, p_table);
        if (position == 0 || position == current ||
                position >= CAM_INTF_PARM_MAX)
            next = 0;
        else
            current = position;
    }
}

static int32_t mm_daemon_config_get_parm(mm_daemon_cfg_t *cfg_obj, int parm)
{
    return *(int32_t *)POINTER_OF(parm, cfg_obj->parm_buf.cfg_buf);
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

static void mm_daemon_config_stream_set(mm_daemon_cfg_t *cfg_obj,
        cam_stream_type_t stream_type, uint8_t set)
{
    if (set)
        cfg_obj->current_streams |= BIT(stream_type);
    else
        cfg_obj->current_streams &= ~BIT(stream_type);
}

/*==========================================================================
 * FUNCTION   : mm_daemon_get_sensor_mode
 *
 * DESCRIPTION: Determine what sensor mode to be in based on active streams
 *
 * PARAMETERS :
 *   @cfg_obj: pointer to config object
 *
 * RETURN     : sensor stream mode
 *==========================================================================*/
static uint8_t mm_daemon_get_sensor_mode(mm_daemon_cfg_t *cfg_obj)
{
    if ((cfg_obj->current_streams & (SB(SNAPSHOT)|SB(POSTVIEW))) ==
            (SB(SNAPSHOT)|SB(POSTVIEW)))
        return SNAPSHOT;
    else if (cfg_obj->current_streams & SB(VIDEO))
        return VIDEO;
    else
        return PREVIEW;
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
    input_cfg.input_pix_clk = cfg_obj->sdata->vfe_clk_rate;
    input_cfg.d.pix_cfg.camif_cfg.pixels_per_line = 640;
    input_cfg.d.pix_cfg.input_format = V4L2_PIX_FMT_NV21;
    rc = ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_INPUT_CFG, &input_cfg);
    return 0;
}

static uint32_t mm_daemon_config_metadata_get_buf(mm_daemon_cfg_t *cfg_obj)
{
    struct msm_buf_mngr_info buf_info;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj,
            CAM_STREAM_TYPE_METADATA);

    if (!buf)
        return -ENOMEM;

    memset(&buf_info, 0, sizeof(buf_info));
    buf_info.session_id = cfg_obj->session_id;
    buf_info.stream_id = buf->stream_id;

    ioctl(cfg_obj->buf_fd, VIDIOC_MSM_BUF_MNGR_GET_BUF, &buf_info);
    return buf_info.index;
}

static int mm_daemon_config_isp_metadata_buf_done(mm_daemon_cfg_t *cfg_obj,
        struct msm_isp_event_data *sof_event, uint32_t buf_idx)
{
    int rc;
    struct msm_buf_mngr_info buf_info;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj,
            CAM_STREAM_TYPE_METADATA);

    if (!buf)
        return -ENOMEM;

    memset(&buf_info, 0, sizeof(buf_info));
    buf_info.session_id = cfg_obj->session_id;
    buf_info.stream_id = buf->stream_id;
    buf_info.frame_id = sof_event->frame_id;
    buf_info.timestamp = sof_event->timestamp;
    buf_info.index = buf_idx;
    rc = ioctl(cfg_obj->buf_fd, VIDIOC_MSM_BUF_MNGR_BUF_DONE, &buf_info);

    ioctl(cfg_obj->buf_fd, VIDIOC_MSM_BUF_MNGR_PUT_BUF, &buf_info);
    return rc;
}

static void mm_daemon_config_isp_set_metadata(mm_daemon_cfg_t *cfg_obj,
        struct v4l2_event *isp_event, uint32_t buf_idx)
{
    struct msm_isp_event_data *buf_event;
    cam_metadata_info_t *meta;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj,
            CAM_STREAM_TYPE_METADATA);

    if (!buf)
        return;

    buf_event = (struct msm_isp_event_data *)&(isp_event->u.data[0]);
    meta = (cam_metadata_info_t *)mmap(0, sizeof(cam_metadata_info_t),
            PROT_READ|PROT_WRITE, MAP_SHARED, buf->buf_data[buf_idx].fd, 0);
    if (meta == MAP_FAILED)
        return;
    memset(meta, 0, sizeof(cam_metadata_info_t));

    if (mm_daemon_get_sensor_mode(cfg_obj) == SNAPSHOT) {
        meta->is_prep_snapshot_done_valid = 1;
        meta->is_good_frame_idx_range_valid = 0;
    } else {
        meta->is_ae_params_valid = cfg_obj->ae.meta.is_ae_params_valid;
        meta->ae_params.flash_needed = cfg_obj->ae.flash_needed;
        meta->is_prep_snapshot_done_valid =
                cfg_obj->ae.meta.is_prep_snapshot_done_valid;
        meta->focus_data.focus_state = cfg_obj->af.meta.state;
        meta->is_focus_valid = cfg_obj->af.meta.is_focus_valid;
    }
    memset(&cfg_obj->ae.meta, 0, sizeof(struct mm_daemon_ae_metadata));
    memset(&cfg_obj->af.meta, 0, sizeof(struct mm_daemon_af_metadata));
    meta->meta_valid_params.meta_frame_id = buf_event->frame_id;
    meta->is_meta_valid = 1;
    munmap(meta, sizeof(cam_metadata_info_t));
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
        buf_req.buf_type = ISP_PRIVATE_BUF;
        buf_req.num_buf = buf_info->stream_info->num_bufs;
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
                &qbuf_info) == 0)
            buf->buf_data[i].mapped = 1;
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
            if (mm_daemon_get_sensor_mode(cfg_obj) == VIDEO)
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

static int mm_daemon_config_vfe_roll_off(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    struct mm_sensor_stream_attr *sattr;
    enum mm_sensor_stream_type mode = mm_daemon_get_sensor_mode(cfg_obj);
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

    sattr = cfg_obj->sdata->attr[mode];

    if (!sattr || !sattr->ro_cfg)
        return 0;

    rocfg = (uint32_t *)malloc(1800);
    p = rocfg;

    *p++ = sattr->ro_cfg;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;

    *p++ = 0x101;
    *p++ = 0x0;

    *p++ = 0x511d5200;
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

static int mm_daemon_config_vfe_fov(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    struct mm_sensor_stream_attr *sattr;
    enum mm_sensor_stream_type mode = mm_daemon_get_sensor_mode(cfg_obj);
    uint32_t pix_offset;
    uint32_t fov_w, fov_h;
    uint32_t reg_w, reg_h;
    cam_dimension_t dim;
    mm_daemon_buf_info *buf;
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

    sattr = cfg_obj->sdata->attr[mode];

    switch (mode) {
    case SNAPSHOT:
        buf = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_SNAPSHOT);
        break;
    case VIDEO:
        buf = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_VIDEO);
        break;
    case PREVIEW:
        buf = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_PREVIEW);
        break;
    default:
        return -EINVAL;
    }

    if (!buf || !sattr)
        return -ENOMEM;

    dim = buf->stream_info->dim;

    reg_h = sattr->h;
    reg_w = sattr->w;
    if (((float)dim.height/dim.width) < ((float)sattr->h/sattr->w)) {
        reg_w -= 12;
        fov_h = (uint32_t)(((float)dim.height/dim.width) * reg_w);
        pix_offset = (uint32_t)((float)(((int)sattr->h - 6) - fov_h)/2);
        reg_h = (pix_offset << 16) | (fov_h + pix_offset);
    } else if (((float)dim.height/dim.width) > ((float)sattr->h/sattr->w)) {
        reg_h -= 6;
        fov_w = (uint32_t)(((float)dim.width/dim.height) * (reg_h * sattr->vscale));
        pix_offset = (uint32_t)((float)(((int)sattr->w - 12) - fov_w)/2);
        reg_w = (pix_offset << 16) | (fov_w + pix_offset);
    }

    fov_cfg = (uint32_t *)malloc(8);
    p = fov_cfg;
    *p++ = reg_w - 1;
    *p = reg_h - 1;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 8, (void *)fov_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(fov_cfg);
    return rc;
}

static int mm_daemon_config_vfe_main_scaler(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    struct mm_sensor_stream_attr *sattr;
    enum mm_sensor_stream_type mode = mm_daemon_get_sensor_mode(cfg_obj);
    cam_dimension_t dim;
    mm_daemon_buf_info *buf;
    uint32_t reg_w, reg_h;
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

    sattr = cfg_obj->sdata->attr[mode];

    switch (mode) {
    case SNAPSHOT:
        buf = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_SNAPSHOT);
        break;
    case VIDEO:
	buf = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_VIDEO);
        break;
    case PREVIEW:
        buf = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_PREVIEW);
        break;
    default:
        return -EINVAL;
    }

    if (!buf || !sattr)
        return -ENOMEM;
    dim = buf->stream_info->dim;

    reg_h = sattr->h;
    reg_w = sattr->w;
    if (((float)dim.height/dim.width) < ((float)sattr->h/sattr->w)) {
        reg_w -= 12;
        reg_h = (uint16_t)(reg_w * ((float)dim.height/dim.width));
    } else if (((float)dim.height/dim.width) > ((float)sattr->h/sattr->w)) {
        reg_h -= 6;
        reg_w = (uint16_t)(((float)reg_h * sattr->vscale) * ((float)dim.width/dim.height));
    }

    ms_cfg = (uint32_t *)malloc(28);
    p = ms_cfg;
    *p++ = 0x3;
    *p++ = reg_w | (dim.width << 16);
    *p++ = 0x00310000;
    *p++ = 0x0;
    *p++ = reg_h | (dim.height << 16);
    *p++ = 0x00310000;
    *p = 0x0;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 28, (void *)ms_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(ms_cfg);
    return rc;
}

static int mm_daemon_config_vfe_s2y(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *s2y_cfg, *p;
    mm_daemon_buf_info *vb, *rb;
    enum mm_sensor_stream_type mode = mm_daemon_get_sensor_mode(cfg_obj);
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x4d0,
                .len = 20,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    switch (mode) {
    case SNAPSHOT:
        vb = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_POSTVIEW);
        rb = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_SNAPSHOT);
        break;
    case VIDEO:
        vb = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_PREVIEW);
	rb = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_VIDEO);
        break;
    case PREVIEW:
        vb = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_PREVIEW);
        rb = vb;
        break;
    default:
        return -EINVAL;
    }

    if (!vb || !rb)
        return -ENOMEM;

    s2y_cfg = (uint32_t *)malloc(20);
    p = s2y_cfg;
    *p++ = 0x3;
    *p++ = vb->stream_info->dim.width << 16 | rb->stream_info->dim.width;
    *p++ = 0x00310000;
    *p++ = vb->stream_info->dim.height << 16 | rb->stream_info->dim.height;
    *p = 0x00310000;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 20, (void *)s2y_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(s2y_cfg);
    return rc;
}

static int mm_daemon_config_vfe_s2cbcr(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *s2cbcr_cfg, *p;
    mm_daemon_buf_info *vb, *rb;
    enum mm_sensor_stream_type mode = mm_daemon_get_sensor_mode(cfg_obj);
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x4E4,
                .len = 20,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    switch (mode) {
    case SNAPSHOT:
        vb = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_POSTVIEW);
        rb = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_SNAPSHOT);
        break;
    case VIDEO:
        vb = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_PREVIEW);
	rb = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_VIDEO);
        break;
    case PREVIEW:
        vb = mm_daemon_get_stream_buf(cfg_obj, CAM_STREAM_TYPE_PREVIEW);
        rb = vb;
        break;
    default:
        return -EINVAL;
    }

    if (!vb || !rb)
        return -ENOMEM;

    s2cbcr_cfg = (uint32_t *)malloc(20);
    p = s2cbcr_cfg;
    *p++ = 0x00000003;
    *p++ = (vb->stream_info->dim.width/2) << 16 | rb->stream_info->dim.width;
    *p++ = 0x0;
    *p++ = (vb->stream_info->dim.height/2) << 16 | rb->stream_info->dim.height;
    *p = 0x0;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 20, (void *)s2cbcr_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(s2cbcr_cfg);
    return rc;
}

static int mm_daemon_config_vfe_axi(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *axiout, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x38,
                .len = 16,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x58,
                .len = 4,
                .cmd_data_offset = 16,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x70,
                .len = 12,
                .cmd_data_offset = 20,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0xb8,
                .len = 4,
                .cmd_data_offset = 32,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0xd0,
                .len = 12,
                .cmd_data_offset = 36,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    axiout = (uint32_t *)malloc(48);
    p = axiout;
    *p++ = 0x3FFF;
    *p++ = 0x2AAA771;
    *p++ = 0x1;

    if (mm_daemon_get_sensor_mode(cfg_obj) == SNAPSHOT) {
        *p++ = 0x203;
        *p++ = 0x22;
        *p++ = 0x340022;
        *p++ = 0xA1078F;
        *p++ = 0x14478F2;
        *p++ = 0x230010;
        *p++ = 0x57011D;
        *p++ = 0xA103C7;
        *p++ = 0x1443C72;
    } else {
        *p++ = 0x1A03;
        *p++ = 0x12F;
        *p++ = 0x1C8012F;
        *p++ = 0x002701DF;
        *p++ = 0x00501DF2;
        *p++ = 0x1300097;
        *p++ = 0x2F80097;
        *p++ = 0x2700EF;
        *p++ = 0x500EF2;
    }

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 48, (void *)axiout,
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

    if (cfg_obj->sdata->uses_sensor_ctrls)
        return 0;

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
                .len = 36,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    if (cfg_obj->sdata->uses_sensor_ctrls)
        return 0;

    color_cfg = (uint32_t *)malloc(36);
    p = color_cfg;
    *p++ = 0xC2;
    *p++ = 0xFF3;
    *p++ = 0xFCC;
    *p++ = 0xF95;
    *p++ = 0xEF;
    *p++ = 0xFFD;
    *p++ = 0xF7E;
    *p++ = 0xF;
    *p   = 0xF4;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 36, (void *)color_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(color_cfg);
    return rc;
}

static int mm_daemon_config_vfe_asf(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *asf_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x4A0,
                .len = 48,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    if (cfg_obj->sdata->uses_sensor_ctrls)
        return 0;

    asf_cfg = (uint32_t *)malloc(48);
    p = asf_cfg;
    if (mm_daemon_get_sensor_mode(cfg_obj) == SNAPSHOT) {
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

static int mm_daemon_config_vfe_white_balance(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    int32_t wb_mode;
    uint32_t wb_reg;
    enum mm_sensor_stream_type mode = mm_daemon_get_sensor_mode(cfg_obj);
    struct mm_sensor_awb_config *awb_cfg;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x384,
                .len = 4,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    awb_cfg = cfg_obj->sdata->awb_cfg[mode];

    if (!awb_cfg)
        return 0;

    wb_mode = mm_daemon_config_get_parm(cfg_obj, CAM_INTF_PARM_WHITE_BALANCE);

    wb_reg = awb_cfg->wb[wb_mode];
    if (!wb_reg)
        wb_reg = awb_cfg->wb[0];

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 4, (void *)&wb_reg,
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

    if (cfg_obj->sdata->uses_sensor_ctrls)
        return 0;

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

    if (cfg_obj->sdata->uses_sensor_ctrls)
        return 0;

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

    if (cfg_obj->sdata->uses_sensor_ctrls)
        return 0;

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

static int mm_daemon_config_vfe_camif(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    struct mm_sensor_stream_attr *sattr;
    enum mm_sensor_stream_type mode = mm_daemon_get_sensor_mode(cfg_obj);
    uint32_t camif_width, camif_type;
    uint32_t *camif_cfg, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x1E4,
                .len = 32,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    if (cfg_obj->info[CSI_DEV])
        switch (cfg_obj->info[CSI_DEV]->type) {
        case MM_CSIC:
            camif_type = 0x40;
            break;
        default:
            ALOGE("%s: Unsupported camif type", __FUNCTION__);
            return -EINVAL;
            break;
    } else
        camif_type = 0x100;

    sattr = cfg_obj->sdata->attr[mode];

    if (cfg_obj->sdata->uses_sensor_ctrls)
        camif_width = sattr->w * 2;
    else
        camif_width = sattr->w;

    camif_cfg = (uint32_t *)malloc(32);
    p = camif_cfg;
    *p++ = camif_type;
    *p++ = 0x00;
    *p++ = (sattr->h << 16) | camif_width;
    *p++ = camif_width - 1;
    *p++ = sattr->h - 1;
    *p++ = 0xffffffff;
    *p++ = 0x00;
    *p++ = 0x3fff3fff;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 32, (void *)camif_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(camif_cfg);
    return rc;
}

static int mm_daemon_config_vfe_demux(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    int32_t wb_mode;
    enum mm_sensor_stream_type mode = mm_daemon_get_sensor_mode(cfg_obj);
    uint32_t *demux_cfg, *p;
    struct mm_sensor_awb_config *awb_cfg;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x284,
                .len = 20,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    wb_mode = mm_daemon_config_get_parm(cfg_obj, CAM_INTF_PARM_WHITE_BALANCE);
    awb_cfg = cfg_obj->sdata->awb_cfg[mode];

    demux_cfg = (uint32_t *)malloc(20);
    p = demux_cfg;
    if (!cfg_obj->sdata->vfe_dmux_cfg || !awb_cfg) {
        *p++ = 0x3;
        *p++ = 0x800080;
        *p++ = 0x800080;
        *p++ = 0x9CAC;
        *p = 0x9CAC;
    } else {
        *p++ = 0x1;
        if (!awb_cfg->dmx_wb1[wb_mode] || !awb_cfg->dmx_wb2[wb_mode]) {
            *p++ = awb_cfg->dmx_wb1[0];
            *p++ = awb_cfg->dmx_wb2[0];
        } else {
            *p++ = awb_cfg->dmx_wb1[wb_mode];
            *p++ = awb_cfg->dmx_wb2[wb_mode];
        }
        *p++ = ((cfg_obj->sdata->vfe_dmux_cfg & 0xFF00) >> 8);
        *p = (cfg_obj->sdata->vfe_dmux_cfg & 0x00FF);
    }

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

    if (cfg_obj->sdata->uses_sensor_ctrls)
        return 0;

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

static int mm_daemon_config_vfe_module(mm_daemon_cfg_t *cfg_obj)
{
    uint32_t module_cfg = cfg_obj->sdata->vfe_module_cfg;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x10,
                .len = 4,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    return mm_daemon_config_vfe_reg_cmd(cfg_obj, 4, (void *)&module_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
}

static int mm_daemon_config_vfe_op_mode(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *op_cfg, *p, vfe_cfg_off;
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
                .reg_offset = 0x52c,
                .len = 4,
                .cmd_data_offset = 4,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x35c,
                .len = 4,
                .cmd_data_offset = 8,
            },
            .cmd_type = VFE_WRITE,
        },
        {
            .u.rw_info = {
                .reg_offset = 0x530,
                .len = 4,
                .cmd_data_offset = 12,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    if (cfg_obj->info[CSI_DEV])
        vfe_cfg_off = cfg_obj->sdata->vfe_cfg_off + 0x30;
    else
        vfe_cfg_off = cfg_obj->sdata->vfe_cfg_off;

    op_cfg = (uint32_t *)malloc(16);
    p = op_cfg;
    *p++ = vfe_cfg_off;
    *p++ = 0x0;
    *p++ = 0x0;
    *p = cfg_obj->sdata->stats_enable;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 16, (void *)op_cfg,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(op_cfg);
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

static int mm_daemon_config_vfe_stats_af(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    uint32_t *af_stats, *p;
    struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[] = {
        {
            .u.rw_info = {
                .reg_offset = 0x53C,
                .len = 16,
            },
            .cmd_type = VFE_WRITE,
        },
    };

    af_stats = (uint32_t *)malloc(16);
    p = af_stats;
    *p++ = 0x1E70286;
    *p++ = 0xE8138;
    *p++ = 0xA781E;
    *p++ = 0x1FFA3FF;

    rc = mm_daemon_config_vfe_reg_cmd(cfg_obj, 16, (void *)af_stats,
            (void *)&reg_cfg_cmd, ARRAY_SIZE(reg_cfg_cmd));
    free(af_stats);
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

    if (cfg_obj->sdata->uses_sensor_ctrls)
        return 0;

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

static uint32_t mm_daemon_config_stats_stream_request(mm_daemon_cfg_t *cfg_obj,
        int stats_type)
{
    struct msm_vfe_stats_stream_request_cmd stream_req_cmd;
    mm_daemon_stats_buf_info *stat = NULL;

    stat = cfg_obj->stats_buf[stats_type];
    if (!stat)
            return 0;

    memset(&stream_req_cmd, 0, sizeof(stream_req_cmd));
    stream_req_cmd.session_id = cfg_obj->session_id;
    stream_req_cmd.stream_id = stat->stream_id;
    stream_req_cmd.stats_type = stats_type;
    stream_req_cmd.buffer_offset = 0;
    stream_req_cmd.framedrop_pattern = NO_SKIP;

    if (ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_REQUEST_STATS_STREAM,
            &stream_req_cmd) < 0)
        return 0;

    mm_daemon_config_subscribe(cfg_obj, ISP_EVENT_STATS_NOTIFY + stats_type, 1);
    return stream_req_cmd.stream_handle;
}

static void mm_daemon_config_stats_stream_release(mm_daemon_cfg_t *cfg_obj,
        int stats_type)
{
    struct msm_vfe_stats_stream_release_cmd cmd;
    mm_daemon_stats_buf_info *stat = cfg_obj->stats_buf[stats_type];

    if (!stat)
        return;

    memset(&cmd, 0, sizeof(cmd));
    cmd.stream_handle = stat->stream_handle;
    ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_RELEASE_STATS_STREAM, &cmd);
    mm_daemon_config_subscribe(cfg_obj, ISP_EVENT_STATS_NOTIFY + stats_type, 0);
    stat->stream_handle = 0;
}

static int mm_daemon_config_stats_buf_alloc(mm_daemon_cfg_t *cfg_obj)
{
    int j, rc = 0;
    int32_t len;
    uint32_t i;
    struct ion_allocation_data alloc_data;
    struct ion_fd_data fd_data;
    enum msm_isp_stats_type stats_type;
    mm_daemon_stats_buf_info *stat;

    for (i = 0; i < ARRAY_SIZE(vfe_stats); i++) {
        stats_type = vfe_stats[i];
        stat = cfg_obj->stats_buf[stats_type];
        len = 1000;
        if (!stat || !len)
            continue;
        for (j = 0; j < stat->buf_cnt; j++) {
            memset(&fd_data, 0, sizeof(fd_data));
            memset(&alloc_data, 0, sizeof(alloc_data));
            alloc_data.len = (len + 4095) & (~4095);
            alloc_data.align = 4096;
            alloc_data.flags = ION_HEAP(ION_CAMERA_HEAP_ID);
            alloc_data.heap_mask = alloc_data.flags;
            rc = ioctl(cfg_obj->ion_fd, ION_IOC_ALLOC, &alloc_data);
            if (rc < 0) {
                ALOGE("%s: Error allocating stat heap", __FUNCTION__);
                return rc;
            }
            fd_data.handle = alloc_data.handle;
            if (ioctl(cfg_obj->ion_fd, ION_IOC_SHARE, &fd_data) < 0) {
                struct ion_handle_data handle_data;
                memset(&handle_data, 0, sizeof(handle_data));
                handle_data.handle = alloc_data.handle;
                rc = -errno;
                ALOGE("%s: ION_IOC_SHARE failed with error %s",
                        __FUNCTION__, strerror(errno));
                ioctl(cfg_obj->ion_fd, ION_IOC_FREE, &handle_data);
                return rc;
            }
            stat->buf_data[j].fd = fd_data.fd;
            stat->buf_data[j].handle = alloc_data.handle;
            stat->buf_data[j].len = alloc_data.len;
            stat->buf_data[j].vaddr = mmap(0, alloc_data.len,
                        PROT_READ|PROT_WRITE, MAP_SHARED,
                        stat->buf_data[j].fd, 0);
            if (stat->buf_data[j].vaddr != MAP_FAILED)
                stat->buf_data[j].mapped = 1;
        }
    }
    return rc;
}

static void mm_daemon_config_stats_buf_dealloc(mm_daemon_cfg_t *cfg_obj)
{
    int j;
    size_t i;
    struct ion_handle_data handle_data;
    mm_daemon_stats_buf_info *stat = NULL;

    for (i = 0; i < ARRAY_SIZE(vfe_stats); i++) {
        stat = cfg_obj->stats_buf[vfe_stats[i]];
        if (!stat)
            continue;

        for (j = 0; j < stat->buf_cnt; j++) {
            memset(&handle_data, 0, sizeof(handle_data));
            if (stat->buf_data[j].mapped) {
                munmap(stat->buf_data[j].vaddr, stat->buf_data[j].len);
                stat->buf_data[j].vaddr = NULL;
                if (stat->buf_data[j].fd > 0) {
                    close(stat->buf_data[j].fd);
                    stat->buf_data[j].fd = 0;
                }
                stat->buf_data[j].mapped = 0;
            }
            if (stat->buf_data[j].handle) {
                handle_data.handle = stat->buf_data[j].handle;
                if (ioctl(cfg_obj->ion_fd, ION_IOC_FREE, &handle_data) < 0)
                    ALOGE("%s: failed to free ION buffer", __FUNCTION__);
                stat->buf_data[j].handle = NULL;
                stat->buf_data[j].len = 0;
            }
        }
    }
}

static int mm_daemon_config_stats_buf_request(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    int num_buf = STATS_BUFFER_MAX;
    size_t i;
    uint32_t stream_id;
    enum msm_isp_stats_type type;
    struct msm_isp_buf_request buf_req;

    if (!cfg_obj->sdata->stats_enable)
        return rc;

    for (i = 0; i < ARRAY_SIZE(vfe_stats); i++) {
        type = vfe_stats[i];
        stream_id = (type | ISP_NATIVE_BUF_BIT);
        memset(&buf_req, 0, sizeof(buf_req));
        buf_req.session_id = cfg_obj->session_id;
        buf_req.stream_id = stream_id;
        buf_req.buf_type = ISP_PRIVATE_BUF;
        buf_req.num_buf = num_buf;
        rc = ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_REQUEST_BUF, &buf_req);
        if (rc < 0) {
            ALOGE("%s: Buffer request for stats type %d failed",
                    __FUNCTION__, type);
            continue;
        }
        if (!cfg_obj->stats_buf[type])
            continue;
        cfg_obj->stats_buf[type]->bufq_handle = buf_req.handle;
        cfg_obj->stats_buf[type]->buf_cnt = num_buf;
        cfg_obj->stats_buf[type]->stream_id = stream_id;
    }

    return rc;
}

static int mm_daemon_config_stats_buf_enqueue(mm_daemon_cfg_t *cfg_obj)
{
    int j, rc = 0;
    uint32_t i;
    struct msm_isp_qbuf_info qbuf_info;
    struct v4l2_buffer buffer;
    struct v4l2_plane plane[VIDEO_MAX_PLANES];
    enum msm_isp_stats_type stats_type;
    mm_daemon_stats_buf_info *stat;

    memset(&qbuf_info, 0, sizeof(qbuf_info));
    memset(&buffer, 0, sizeof(buffer));
    buffer.length = 1;
    buffer.m.planes = &plane[0];
    for (i = 0; i < ARRAY_SIZE(vfe_stats); i++) {
        stats_type = vfe_stats[i];
        stat = cfg_obj->stats_buf[stats_type];
        if (!stat)
            continue;
        for (j = 0; j < stat->buf_cnt; j++) {
            if (stat->buf_data[j].mapped) {
                qbuf_info.buffer = buffer;
                qbuf_info.buf_idx = j;
                plane[0].m.userptr = stat->buf_data[j].fd;
                plane[0].data_offset = 0;
                qbuf_info.handle = stat->bufq_handle;
                ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_ENQUEUE_BUF,
                        &qbuf_info);
            }
        }
    }
    return rc;
}

static int mm_daemon_config_stats_buf_requeue(mm_daemon_cfg_t *cfg_obj,
        uint32_t stats_type, uint8_t buf_idx)
{
    struct msm_isp_qbuf_info qbuf_info;
    mm_daemon_stats_buf_info *stat;

    stat = cfg_obj->stats_buf[stats_type];
    memset(&qbuf_info, 0, sizeof(qbuf_info));
    qbuf_info.dirty_buf = 1;
    qbuf_info.buf_idx = buf_idx;
    qbuf_info.handle = stat->bufq_handle;
    return ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_ENQUEUE_BUF, &qbuf_info);
}

static void mm_daemon_config_stats_buf_release(mm_daemon_cfg_t *cfg_obj)
{
    size_t i;
    struct msm_isp_buf_request buf_req;

    for (i = 0; i < ARRAY_SIZE(vfe_stats); i++) {
        if (!cfg_obj->stats_buf[vfe_stats[i]])
            continue;
        memset(&buf_req, 0, sizeof(buf_req));
        buf_req.handle = cfg_obj->stats_buf[vfe_stats[i]]->bufq_handle;
        ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_RELEASE_BUF, &buf_req);
        cfg_obj->stats_buf[vfe_stats[i]]->bufq_handle = 0;
    }
}

static void mm_daemon_config_stats(mm_daemon_cfg_t *cfg_obj, uint16_t req_stats,
        int enable)
{
    size_t i, j;
    uint16_t stats = 0;
    struct msm_vfe_stats_stream_cfg_cmd cc;

    if ((cfg_obj->sdata->stats_enable & req_stats) == 0)
        return;

    memset(&cc, 0, sizeof(cc));
    cc.enable = enable;
    for (i = 0, j = 0; i < MSM_ISP_STATS_MAX; i++) {
        if (!((req_stats >> i) & 0x1) || !cfg_obj->stats_buf[i])
            continue;

        if ((enable && (BIT(i) & cfg_obj->enabled_stats)) ||
                (!enable && !(BIT(i) & cfg_obj->enabled_stats)))
            continue;

        if (enable)
            cfg_obj->stats_buf[i]->stream_handle =
                    mm_daemon_config_stats_stream_request(cfg_obj, i);
        if (cfg_obj->stats_buf[i]->stream_handle) {
            stats |= BIT(i);
            cc.stream_handle[cc.num_streams] =
                    cfg_obj->stats_buf[i]->stream_handle;
            cc.num_streams++;
        }
        req_stats &= ~BIT(i);
        if (!req_stats)
            break;
    }
    if (!cc.num_streams)
        return;

    if (ioctl(cfg_obj->vfe_fd, VIDIOC_MSM_ISP_CFG_STATS_STREAM, &cc) == 0) {
        for (i = 0; i < MSM_ISP_STATS_MAX; i++) {
            if ((stats >> i) & 0x1) {
                if (enable) {
                    if (cfg_obj->stats_buf[i]->ops.start)
                        cfg_obj->stats_buf[i]->ops.start(cfg_obj);
                } else
                    mm_daemon_config_stats_stream_release(cfg_obj, i);
            }
        }
    }

    if (enable)
        cfg_obj->enabled_stats |= stats;
    else
        cfg_obj->enabled_stats &= ~stats;
}

static int mm_daemon_config_exp_gain(mm_daemon_cfg_t *cfg_obj, uint16_t gain,
        uint16_t line, uint8_t wait)
{
    uint32_t val = gain | (line << 16);

    if (line == cfg_obj->ae.c_line && gain == cfg_obj->ae.c_gain)
        return 1;

    mm_daemon_util_subdev_cmd(cfg_obj->info[SNSR_DEV],
            SENSOR_CMD_EXP_GAIN, val, wait);
    cfg_obj->ae.c_gain = gain;
    cfg_obj->ae.c_line = line;

    return 0;
}

static void mm_daemon_config_prepare_snapshot(mm_daemon_cfg_t *cfg_obj)
{
    if (!cfg_obj->info[LED_DEV])
        return;

    cfg_obj->prep_snapshot = 1;
    mm_daemon_util_subdev_cmd(cfg_obj->info[LED_DEV],
            LED_CMD_CONTROL, MSM_CAMERA_LED_LOW, FALSE);
}

static int mm_daemon_config_start_preview(mm_daemon_cfg_t *cfg_obj)
{
    cam_stream_type_t stream_type = CAM_STREAM_TYPE_PREVIEW;
    mm_daemon_buf_info *buf = mm_daemon_get_stream_buf(cfg_obj, stream_type);

    if (!buf)
        return -ENOMEM;

    if (cfg_obj->info[CSI_DEV])
        mm_daemon_util_subdev_cmd(cfg_obj->info[CSI_DEV],
                CSI_CMD_CFG, 0, FALSE);

    mm_daemon_util_subdev_cmd(cfg_obj->info[SNSR_DEV], SENSOR_CMD_SET_MODE,
            mm_daemon_get_sensor_mode(cfg_obj), TRUE);

    mm_daemon_util_subdev_cmd(cfg_obj->info[ACT_DEV],
            ACT_CMD_INIT_FOCUS, 0, FALSE);

    if (buf->stream_info->num_bufs)
        mm_daemon_config_isp_buf_enqueue(cfg_obj, stream_type);
    mm_daemon_config_vfe_roll_off(cfg_obj);
    mm_daemon_config_vfe_fov(cfg_obj);
    mm_daemon_config_vfe_main_scaler(cfg_obj);
    mm_daemon_config_vfe_s2y(cfg_obj);
    mm_daemon_config_vfe_s2cbcr(cfg_obj);
    mm_daemon_config_vfe_axi(cfg_obj);
    mm_daemon_config_vfe_chroma_en(cfg_obj);
    mm_daemon_config_vfe_color_cor(cfg_obj);
    mm_daemon_config_vfe_asf(cfg_obj);
    mm_daemon_config_vfe_white_balance(cfg_obj);
    mm_daemon_config_vfe_black_level(cfg_obj);
    mm_daemon_config_vfe_mce(cfg_obj);
    mm_daemon_config_vfe_rgb_gamma(cfg_obj);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 2);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 4);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 6);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 12);
    mm_daemon_config_vfe_camif(cfg_obj);
    mm_daemon_config_vfe_demux(cfg_obj);
    mm_daemon_config_vfe_out_clamp(cfg_obj);
    mm_daemon_config_vfe_frame_skip(cfg_obj);
    mm_daemon_config_vfe_chroma_subs(cfg_obj);
    mm_daemon_config_vfe_sk_enhance(cfg_obj);
    mm_daemon_config_vfe_op_mode(cfg_obj);
    mm_daemon_config_isp_input_cfg(cfg_obj);
    mm_daemon_config_isp_stream_request(cfg_obj, stream_type);
    mm_daemon_config_isp_stream_cfg(cfg_obj, stream_type, START_STREAM);
    return 0;
}

static void mm_daemon_config_stop_preview(mm_daemon_cfg_t *cfg_obj)
{
    cam_stream_type_t stream_type = CAM_STREAM_TYPE_PREVIEW;

    mm_daemon_config_isp_stream_cfg(cfg_obj, stream_type, STOP_STREAM);
    mm_daemon_config_isp_stream_release(cfg_obj, stream_type);
    if (cfg_obj->prep_snapshot)
        mm_daemon_util_subdev_cmd(cfg_obj->info[LED_DEV],
            LED_CMD_CONTROL, MSM_CAMERA_LED_OFF, FALSE);
    mm_daemon_config_vfe_stop(cfg_obj);
}

static int mm_daemon_config_start_snapshot(mm_daemon_cfg_t *cfg_obj)
{
    cam_stream_type_t stream_type = CAM_STREAM_TYPE_SNAPSHOT;

    mm_daemon_util_subdev_cmd(cfg_obj->info[SNSR_DEV], SENSOR_CMD_SET_MODE,
            mm_daemon_get_sensor_mode(cfg_obj), TRUE);

    mm_daemon_config_vfe_roll_off(cfg_obj);
    mm_daemon_config_vfe_fov(cfg_obj);
    mm_daemon_config_vfe_main_scaler(cfg_obj);
    mm_daemon_config_vfe_s2y(cfg_obj);
    mm_daemon_config_vfe_s2cbcr(cfg_obj);
    mm_daemon_config_vfe_axi(cfg_obj);
    mm_daemon_config_vfe_chroma_en(cfg_obj);
    mm_daemon_config_vfe_color_cor(cfg_obj);
    mm_daemon_config_vfe_asf(cfg_obj);
    mm_daemon_config_vfe_white_balance(cfg_obj);
    mm_daemon_config_vfe_black_level(cfg_obj);
    mm_daemon_config_vfe_rgb_gamma(cfg_obj);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 2);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 4);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 6);
    mm_daemon_config_vfe_rgb_gamma_chbank(cfg_obj, 12);
    if (cfg_obj->prep_snapshot) {
        mm_daemon_util_subdev_cmd(cfg_obj->info[LED_DEV],
                LED_CMD_CONTROL, MSM_CAMERA_LED_HIGH, FALSE);
        cfg_obj->prep_snapshot = 0;
    }
    mm_daemon_config_vfe_camif(cfg_obj);
    mm_daemon_config_vfe_demux(cfg_obj);
    mm_daemon_config_vfe_out_clamp(cfg_obj);
    mm_daemon_config_vfe_chroma_subs(cfg_obj);
    mm_daemon_config_vfe_sk_enhance(cfg_obj);
    mm_daemon_config_vfe_op_mode(cfg_obj);
    mm_daemon_config_isp_stream_request(cfg_obj, CAM_STREAM_TYPE_POSTVIEW);
    mm_daemon_config_isp_stream_request(cfg_obj, stream_type);
    return mm_daemon_config_isp_stream_cfg(cfg_obj, stream_type, START_STREAM);
}

static void mm_daemon_config_stop_snapshot(mm_daemon_cfg_t *cfg_obj)
{
    if (cfg_obj->prep_snapshot) {
        mm_daemon_util_subdev_cmd(cfg_obj->info[LED_DEV],
                LED_CMD_CONTROL, MSM_CAMERA_LED_OFF, FALSE);
        cfg_obj->prep_snapshot = 0;
    }
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

static int mm_daemon_config_sk_pkt_map(mm_daemon_cfg_t *cfg_obj,
       struct mm_daemon_sk_pkt *sk_pkt)
{
    mm_daemon_thread_info *info = NULL;
    cam_sock_packet_t *packet = NULL;
    int idx, stream_id, buf_idx, rc = -EINVAL;
    mm_daemon_buf_info *buf = NULL;

    info = cfg_obj->info[SOCK_DEV];
    packet = (cam_sock_packet_t *)sk_pkt->data;
    stream_id = packet->payload.buf_map.stream_id;
    buf_idx = packet->payload.buf_map.frame_idx;

    pthread_mutex_lock(&info->lock);
    switch (packet->payload.buf_map.type) {
    case CAM_MAPPING_BUF_TYPE_CAPABILITY:
        if (cfg_obj->cap_buf.mapped)
            break;
        cfg_obj->cap_buf.vaddr = mmap(0, sizeof(cam_capability_t),
                PROT_READ|PROT_WRITE, MAP_SHARED, sk_pkt->fd, 0);
        if (cfg_obj->cap_buf.vaddr == MAP_FAILED) {
            close(sk_pkt->fd);
            sk_pkt->fd = 0;
        } else {
            cfg_obj->cap_buf.fd = sk_pkt->fd;
            cfg_obj->cap_buf.mapped = 1;
            memcpy(cfg_obj->cap_buf.vaddr, cfg_obj->sdata->cap,
                    sizeof(cam_capability_t));
            mm_daemon_util_pipe_cmd(cfg_obj->cfg->cb_pfd,
                    SERVER_CMD_CAP_BUF_MAP, 1);
            rc = 0;
        }
        break;
    case CAM_MAPPING_BUF_TYPE_PARM_BUF:
        cfg_obj->parm_buf.buf = (parm_buffer_t *)mmap(0,
                sizeof(parm_buffer_t), PROT_READ|PROT_WRITE, MAP_SHARED,
                sk_pkt->fd, 0);
        if (cfg_obj->parm_buf.buf == MAP_FAILED) {
            close(sk_pkt->fd);
            sk_pkt->fd = 0;
        } else {
            cfg_obj->parm_buf.fd = sk_pkt->fd;
            cfg_obj->parm_buf.mapped = 1;
            cfg_obj->parm_buf.cfg_buf = (parm_buffer_t *)calloc(1,
                    sizeof(parm_buffer_t));
            if (cfg_obj->parm_buf.cfg_buf == NULL)
                break;
            rc = 0;
        }
        break;
    case CAM_MAPPING_BUF_TYPE_STREAM_BUF:
        idx = mm_daemon_get_stream_idx(cfg_obj, stream_id);
        if (idx < 0)
            break;
        buf = cfg_obj->stream_buf[idx];
        if (buf_idx < CAM_MAX_NUM_BUFS_PER_STREAM) {
            buf->buf_data[buf_idx].fd = sk_pkt->fd;
            buf->buf_data[buf_idx].len = packet->payload.buf_map.size;
            buf->num_stream_bufs++;
            rc = 0;
        }
        break;
    case CAM_MAPPING_BUF_TYPE_STREAM_INFO:
        idx = mm_daemon_get_stream_idx(cfg_obj, stream_id);
        if (idx >= 0)
            buf = cfg_obj->stream_buf[idx];
        if (buf == NULL) {
            ALOGE("%s: no buffer for stream %d", __FUNCTION__, idx);
            close(sk_pkt->fd);
            sk_pkt->fd = 0;
            break;
        }
        buf->stream_info =
                (cam_stream_info_t *)mmap(0, sizeof(cam_stream_info_t),
                PROT_READ|PROT_WRITE, MAP_SHARED, sk_pkt->fd, 0);
        if (buf->stream_info == MAP_FAILED) {
            ALOGE("%s: stream_info mapping failed for stream %d",
                    __FUNCTION__, stream_id);
            close(sk_pkt->fd);
            sk_pkt->fd = 0;
            break;
        }
        buf->stream_info_mapped = 1;
        buf->fd = sk_pkt->fd;
        if (cfg_obj->current_streams == 0) {
            mm_daemon_config_vfe_reset(cfg_obj);
            mm_daemon_config_vfe_module(cfg_obj);
        }
        mm_daemon_config_stream_set(cfg_obj,
                buf->stream_info->stream_type, 1);
        if (buf->stream_info->stream_type == CAM_STREAM_TYPE_METADATA) {
            buf->output_format = V4L2_PIX_FMT_META;
            rc = 0;
        } else {
            buf->output_format =
                    mm_daemon_config_v4l2_fmt(buf->stream_info->fmt);
            rc = mm_daemon_config_isp_buf_request(cfg_obj, buf, stream_id);
        }

        break;
    default:
        break;
    }

    pthread_cond_signal(&info->cond);
    pthread_mutex_unlock(&info->lock);
    return rc;
}

static int mm_daemon_config_sk_pkt_unmap(mm_daemon_cfg_t *cfg_obj,
        struct mm_daemon_sk_pkt *sk_pkt)
{
    cam_sock_packet_t *packet = NULL;
    int idx, stream_id, buf_idx;
    mm_daemon_buf_info *buf = NULL;
    mm_daemon_thread_info *info = NULL;

    if (!sk_pkt || !sk_pkt->data)
        return -EINVAL;

    info = cfg_obj->info[SOCK_DEV];
    packet = (cam_sock_packet_t *)sk_pkt->data;
    stream_id = packet->payload.buf_unmap.stream_id;
    pthread_mutex_lock(&info->lock);

    switch (packet->payload.buf_unmap.type) {
    case CAM_MAPPING_BUF_TYPE_CAPABILITY:
        if (!cfg_obj->cap_buf.mapped)
            ALOGE("%s: capability buf not mapped", __FUNCTION__);
        else {
            if (munmap(cfg_obj->cap_buf.vaddr, sizeof(cam_capability_t)))
                ALOGE("%s: Failed to unmap memory at %p : %s",
                        __FUNCTION__, cfg_obj->cap_buf.vaddr,
                        strerror(errno));
            close(cfg_obj->cap_buf.fd);
            cfg_obj->cap_buf.fd = 0;
            cfg_obj->cap_buf.mapped = 0;
            mm_daemon_util_pipe_cmd(cfg_obj->cfg->cb_pfd,
                    SERVER_CMD_CAP_BUF_MAP, 0);
        }
        break;
    case CAM_MAPPING_BUF_TYPE_PARM_BUF:
        if (!cfg_obj->parm_buf.mapped)
            ALOGE("%s: parm buf not mapped", __FUNCTION__);
        else {
            if (munmap(cfg_obj->parm_buf.buf, sizeof(parm_buffer_t)))
                ALOGE("%s: Failed to unmap memory at %p : %s",
                        __FUNCTION__, cfg_obj->parm_buf.buf,
                        strerror(errno));
            close(cfg_obj->parm_buf.fd);
            cfg_obj->parm_buf.fd = 0;
            cfg_obj->parm_buf.mapped = 0;
            free(cfg_obj->parm_buf.cfg_buf);
        }
        break;
    case CAM_MAPPING_BUF_TYPE_STREAM_INFO:
        idx = mm_daemon_get_stream_idx(cfg_obj, stream_id);
        if (idx >= 0)
            buf = cfg_obj->stream_buf[idx];
        if (buf && buf->stream_info_mapped) {
            if (buf->stream_info->stream_type != CAM_STREAM_TYPE_METADATA)
                mm_daemon_config_isp_buf_release(cfg_obj,
                        buf->stream_info->stream_type);
            for (buf_idx = 0; buf_idx < buf->num_stream_bufs; buf_idx++) {
                if (buf->stream_info->stream_type ==
                        CAM_STREAM_TYPE_METADATA) {
                    if (buf->buf_data[buf_idx].mapped) {
                        munmap(buf->buf_data[buf_idx].vaddr,
                                sizeof(cam_metadata_info_t));
                    }
                }
                close(buf->buf_data[buf_idx].fd);
                buf->buf_data[buf_idx].mapped = 0;
                buf->buf_data[buf_idx].fd = 0;
            }
            mm_daemon_config_stream_set(cfg_obj,
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
        break;
    default:
        break;
    }

    pthread_cond_signal(&info->cond);
    pthread_mutex_unlock(&info->lock);
    return 0;
}

static void mm_daemon_config_auto_exposure(mm_daemon_cfg_t *cfg_obj,
        uint32_t buf_idx)
{
    mm_daemon_stats_buf_info *stat = cfg_obj->stats_buf[MSM_ISP_STATS_AEC];
    enum mm_sensor_stream_type mode = mm_daemon_get_sensor_mode(cfg_obj);
    struct mm_sensor_aec_config *aec_cfg = cfg_obj->sdata->aec_cfg;
    int i;
    int32_t led_mode;
    int32_t stat_val = 0;
    int16_t gain_adj = 0;
    int16_t line_adj = 0;
    uint16_t gain = cfg_obj->ae.c_gain;
    uint16_t line = cfg_obj->ae.c_line;
    uint16_t max_line_adj = 100;
    uint16_t low_th = aec_cfg->target[mode].low_th;
    uint16_t high_th = aec_cfg->target[mode].high_th;
    uint16_t target = aec_cfg->target[mode].tgt;
    uint8_t flash_needed = FALSE;
    uint16_t work_buf[256];

    if (cfg_obj->info[SNSR_DEV]->state != STATE_POLL ||
            mm_daemon_config_get_parm(cfg_obj, CAM_INTF_PARM_AEC_LOCK) ||
            !aec_cfg)
        return;

    led_mode = mm_daemon_config_get_parm(cfg_obj,
            CAM_INTF_PARM_LED_MODE);

    memcpy(work_buf, stat->buf_data[buf_idx].vaddr, 512);
    memset(stat->buf_data[buf_idx].vaddr, 0, stat->buf_data[buf_idx].len);
    for (i = 0; i < 256; i++)
        stat_val += work_buf[i];
    stat_val /= i;

    if (cfg_obj->ae.frm_cnt < aec_cfg->frame_skip) {
        cfg_obj->ae.frm_cnt++;
        return;
    } else
        cfg_obj->ae.frm_cnt = 0;

    if (stat_val < (target - low_th) || stat_val > (target + high_th)) {
        gain_adj = (int32_t)(target - stat_val) / 100;
        if ((gain_adj > 0 && gain == aec_cfg->gain_max) ||
                (gain_adj < 0 && gain == aec_cfg->gain_min) ||
                (line != aec_cfg->default_line[mode])) {
            line_adj = gain_adj * aec_cfg->line_mult;
            if (line_adj > max_line_adj)
                line_adj = max_line_adj;
            else if (line_adj < max_line_adj * -1)
                line_adj = max_line_adj * -1;
            gain_adj = 0;
        }

        if (line_adj) {
            if ((line > aec_cfg->default_line[mode] && line +
                    line_adj < aec_cfg->default_line[mode]) ||
                    (line < aec_cfg->default_line[mode] &&
                    line + line_adj > aec_cfg->default_line[mode]))
                line = aec_cfg->default_line[mode];
            else if (line + line_adj < aec_cfg->line_min)
                line = aec_cfg->line_min;
            else if (line + line_adj > aec_cfg->line_max)
                line = aec_cfg->line_max;
            else
                line += line_adj;
        } else {
            if (line == aec_cfg->default_line[mode]) {
                if (gain + gain_adj < aec_cfg->gain_min)
                    gain = aec_cfg->gain_min;
                else if (gain + gain_adj > aec_cfg->gain_max)
                    gain = aec_cfg->gain_max;
                else
                    gain += gain_adj;
            } else if (line > aec_cfg->default_line[mode]) {
                if (line + line_adj < aec_cfg->default_line[mode])
                    line = aec_cfg->default_line[mode];
                else
                    line += line_adj;
            } else {
                if (line + line_adj > aec_cfg->default_line[mode])
                    line = aec_cfg->default_line[mode];
                else
                    line += line_adj;
            }
        }
    }

    if (gain >= aec_cfg->flash_threshold && led_mode == CAM_FLASH_MODE_AUTO)
        flash_needed = TRUE;

    if (mm_daemon_config_exp_gain(cfg_obj, gain, line, FALSE)) {
        if (cfg_obj->prep_snapshot)
            cfg_obj->ae.meta.is_prep_snapshot_done_valid = TRUE;
    }

    cfg_obj->ae.flash_needed = flash_needed;
    cfg_obj->ae.meta.is_ae_params_valid = TRUE;
}

static void mm_daemon_config_auto_focus_start(mm_daemon_cfg_t *cfg_obj)
{
    if (!cfg_obj->info[ACT_DEV])
        return;

    if (cfg_obj->ae.flash_needed || (mm_daemon_config_get_parm(cfg_obj,
            CAM_INTF_PARM_LED_MODE) == CAM_FLASH_MODE_ON))
        mm_daemon_config_prepare_snapshot(cfg_obj);

    if (cfg_obj->af.curr_step_pos != 0) {
        mm_daemon_util_subdev_cmd(cfg_obj->info[ACT_DEV],
                ACT_CMD_DEFAULT_FOCUS, 0, FALSE);
        memset(&cfg_obj->af, 0, sizeof(struct mm_daemon_af_info));
    }
    mm_daemon_config_stats(cfg_obj, BIT(MSM_ISP_STATS_AF), 1);
}

static void mm_daemon_config_auto_focus_stop(mm_daemon_cfg_t *cfg_obj)
{
    mm_daemon_config_stats(cfg_obj, BIT(MSM_ISP_STATS_AF), 0);
}

static void mm_daemon_config_auto_focus(mm_daemon_cfg_t *cfg_obj,
        uint32_t buf_idx)
{
    struct mm_daemon_act_params *act;
    uint32_t total_steps;
    int num_step = 0;
    int focus_dir = 0;
    uint16_t curr_stat_val;
    uint8_t work_buf[3];
    uint8_t frame_wait_count = 4;
    mm_daemon_stats_buf_info *stat = cfg_obj->stats_buf[MSM_ISP_STATS_AF];

    if (!cfg_obj->info[ACT_DEV] || (cfg_obj->prep_snapshot &&
            !mm_daemon_config_get_parm(cfg_obj,
            CAM_INTF_PARM_AEC_LOCK)) || (cfg_obj->info[ACT_DEV]->state !=
            STATE_POLL))
        return;

    if (cfg_obj->af.state != MM_FOCUS_INIT) {
        if (cfg_obj->af.frm_cnt < frame_wait_count) {
            cfg_obj->af.frm_cnt++;
            return;
        } else
            cfg_obj->af.frm_cnt = 0;
    }

    memcpy(work_buf, stat->buf_data[buf_idx].vaddr, 3);
    memset(stat->buf_data[buf_idx].vaddr, 0, stat->buf_data[buf_idx].len);
    curr_stat_val = (work_buf[2] << 8) | work_buf[1];
    if (curr_stat_val > cfg_obj->af.focus_stat_val) {
        cfg_obj->af.focus_stat_val = curr_stat_val;
        cfg_obj->af.focus_step_pos = cfg_obj->af.curr_step_pos;
    }

    act = (struct mm_daemon_act_params *)cfg_obj->sdata->act_params;
    total_steps = act->act_info->af_tuning_params.total_steps;
    num_step = cfg_obj->af.focus_step_pos - cfg_obj->af.curr_step_pos;
    if (num_step > 0)
        focus_dir = MSM_ACTUATOR_MOVE_SIGNED_NEAR;
    else if (num_step < 0)
        focus_dir = MSM_ACTUATOR_MOVE_SIGNED_FAR;

    switch (cfg_obj->af.state) {
    case MM_FOCUS_INIT:
        cfg_obj->af.state = MM_FOCUS_SCANNING;
        cfg_obj->af.incr_step = total_steps / 2;
        mm_daemon_util_subdev_cmd(cfg_obj->info[ACT_DEV], ACT_CMD_MOVE_FOCUS,
                cfg_obj->af.incr_step, FALSE);
        break;
    case MM_FOCUS_SCANNING:
        cfg_obj->af.incr_step /= 2;
        if (cfg_obj->af.incr_step == 1)
            cfg_obj->af.state = MM_FOCUS_SCANNING_DONE;

        mm_daemon_util_subdev_cmd(cfg_obj->info[ACT_DEV],
                ACT_CMD_MOVE_FOCUS, cfg_obj->af.incr_step * focus_dir, FALSE);
        break;
    case MM_FOCUS_SCANNING_DONE:
        if (cfg_obj->af.curr_step_pos == cfg_obj->af.focus_step_pos) {
            cfg_obj->af.meta.state = CAM_AF_FOCUSED;
            mm_daemon_config_auto_focus_stop(cfg_obj);
        } else
            mm_daemon_util_subdev_cmd(cfg_obj->info[ACT_DEV],
                    ACT_CMD_MOVE_FOCUS, cfg_obj->af.focus_step_pos -
                    cfg_obj->af.curr_step_pos, FALSE);
        break;
    default:
        break;
    }
    cfg_obj->af.meta.is_focus_valid = 1;
}

static void mm_daemon_config_auto_white_balance(mm_daemon_cfg_t *cfg_obj,
        uint32_t buf_idx)
{
    mm_daemon_stats_buf_info *stat = cfg_obj->stats_buf[MSM_ISP_STATS_AWB];

    memset(stat->buf_data[buf_idx].vaddr, 0, stat->buf_data[buf_idx].len);
    cam_wb_mode_type wb = mm_daemon_config_get_parm(cfg_obj,
            CAM_INTF_PARM_WHITE_BALANCE);
    if (cfg_obj->wb.curr_wb != wb) {
        mm_daemon_config_vfe_white_balance(cfg_obj);
        mm_daemon_config_vfe_update(cfg_obj);
        cfg_obj->wb.curr_wb = wb;
    }
}

static int mm_daemon_config_isp_evt(mm_daemon_cfg_t *cfg_obj,
        struct v4l2_event *isp_event)
{
    int rc = 0;
    uint32_t buf_idx;
    enum msm_isp_stats_type stats_type;
    struct msm_isp_stats_event *stats_event;
    struct msm_isp_event_data *event_data =
            (struct msm_isp_event_data *)&(isp_event->u.data[0]);

    switch (isp_event->type) {
        case ISP_EVENT_SOF:
            cfg_obj->stat_frames = 0;
            break;
        case ISP_EVENT_STATS_NOTIFY + MSM_ISP_STATS_AEC:
        case ISP_EVENT_STATS_NOTIFY + MSM_ISP_STATS_AF:
        case ISP_EVENT_STATS_NOTIFY + MSM_ISP_STATS_AWB:
            stats_type = isp_event->type - ISP_EVENT_STATS_NOTIFY;
            stats_event = &event_data->u.stats;
            buf_idx = stats_event->stats_buf_idxs[stats_type];
            mm_daemon_stats_buf_info *stat = cfg_obj->stats_buf[stats_type];

            if (!stat)
                break;
            cfg_obj->stat_frames |= BIT(stats_type);
            if (stat->ops.proc)
                stat->ops.proc(cfg_obj, buf_idx);
            rc = mm_daemon_config_stats_buf_requeue(cfg_obj, stats_type, buf_idx);
            break;
        case ISP_EVENT_COMP_STATS_NOTIFY:
            break;
        default:
            ALOGE("%s: Unknown event %d", __FUNCTION__, isp_event->type);
            break;
    }
    if (cfg_obj->stat_frames == cfg_obj->enabled_stats) {
        buf_idx = mm_daemon_config_metadata_get_buf(cfg_obj);
        mm_daemon_config_isp_set_metadata(cfg_obj, isp_event, buf_idx);
        mm_daemon_config_isp_metadata_buf_done(cfg_obj, event_data, buf_idx);
    }
    return rc;
}

static int mm_daemon_config_dequeue(mm_daemon_cfg_t *cfg_obj)
{
    struct v4l2_event isp_event;
    int rc = 0;

    memset(&isp_event, 0, sizeof(isp_event));
    rc = ioctl(cfg_obj->vfe_fd, VIDIOC_DQEVENT, &isp_event);
    if (rc < 0) {
        ALOGE("%s: Error dequeueing event", __FUNCTION__);
        return rc;
    }

    return mm_daemon_config_isp_evt(cfg_obj, &isp_event);
}

static int mm_daemon_config_read_pipe(mm_daemon_cfg_t *cfg_obj)
{
    int rc = 0;
    ssize_t read_len;
    int stream_id, idx;
    cam_stream_type_t stream_type;
    mm_daemon_pipe_evt_t pipe_cmd;
    mm_daemon_buf_info *buf = NULL;

    read_len = read(cfg_obj->cfg->pfds[0], &pipe_cmd, sizeof(pipe_cmd));
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
                rc = mm_daemon_config_start_preview(cfg_obj);
                break;
            case CAM_STREAM_TYPE_VIDEO:
                rc = mm_daemon_config_start_video(cfg_obj);
                break;
            case CAM_STREAM_TYPE_METADATA:
                if (cfg_obj->enabled_stats)
                    mm_daemon_config_stats(cfg_obj,
                            cfg_obj->enabled_stats, false);
                mm_daemon_config_stats(cfg_obj,
                        BIT(MSM_ISP_STATS_AEC) |
                        BIT(MSM_ISP_STATS_AWB), true);
                break;
            case CAM_STREAM_TYPE_SNAPSHOT:
            case CAM_STREAM_TYPE_POSTVIEW:
            case CAM_STREAM_TYPE_OFFLINE_PROC:
                if (buf->stream_info->num_bufs)
                    mm_daemon_config_isp_buf_enqueue(cfg_obj,
                            stream_type);
                if (stream_type == CAM_STREAM_TYPE_SNAPSHOT)
                    rc = mm_daemon_config_start_snapshot(cfg_obj);
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
                if (cfg_obj->enabled_stats)
                    mm_daemon_config_stats(cfg_obj,
                            cfg_obj->enabled_stats, false);
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
        pthread_mutex_lock(&cfg_obj->cfg->lock);
        for (idx = 0; idx < MAX_NUM_STREAM; idx++) {
            if (cfg_obj->stream_buf[idx] == NULL)
                break;
        }
        if (idx == MAX_NUM_STREAM)
            return -ENOMEM;
        mm_daemon_util_subdev_cmd(cfg_obj->info[SNSR_DEV],
                SENSOR_CMD_POWER_UP, 0, FALSE);
        cfg_obj->stream_buf[idx] = (mm_daemon_buf_info *)malloc(
                sizeof(mm_daemon_buf_info));
        memset(cfg_obj->stream_buf[idx], 0, sizeof(mm_daemon_buf_info));
        cfg_obj->stream_buf[idx]->stream_id = stream_id;
        pthread_cond_signal(&cfg_obj->cfg->cond);
        pthread_mutex_unlock(&cfg_obj->cfg->lock);
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
    case CFG_CMD_PARM:
        if (cfg_obj->parm_buf.mapped)
            mm_daemon_config_parm(cfg_obj);
        break;
    case CFG_CMD_CANCEL_AUTO_FOCUS:
        mm_daemon_config_auto_focus_stop(cfg_obj);
        break;
    case CFG_CMD_DO_AUTO_FOCUS:
        mm_daemon_config_auto_focus_start(cfg_obj);
        break;
    case CFG_CMD_PREPARE_SNAPSHOT:
        mm_daemon_config_prepare_snapshot(cfg_obj);
        break;
    case CFG_CMD_MAP_UNMAP_DONE:
        mm_daemon_util_pipe_cmd(cfg_obj->cfg->cb_pfd,
                SERVER_CMD_MAP_UNMAP_DONE, pipe_cmd.val);
        break;
    case CFG_CMD_SK_PKT_MAP:
        rc = mm_daemon_config_sk_pkt_map(cfg_obj,
                (struct mm_daemon_sk_pkt *)pipe_cmd.val);
        mm_daemon_util_pipe_cmd(cfg_obj->cfg->cb_pfd,
                SERVER_CMD_MAP_UNMAP_DONE, pipe_cmd.val);
        break;
    case CFG_CMD_SK_PKT_UNMAP:
        rc = mm_daemon_config_sk_pkt_unmap(cfg_obj,
                (struct mm_daemon_sk_pkt *)pipe_cmd.val);
        mm_daemon_util_pipe_cmd(cfg_obj->cfg->cb_pfd,
                SERVER_CMD_MAP_UNMAP_DONE, pipe_cmd.val);
        break;
    case CFG_CMD_AF_ACT_POS:
        cfg_obj->af.curr_step_pos = pipe_cmd.val;
        break;
    case CFG_CMD_ERR:
    case CFG_CMD_SHUTDOWN:
        rc = -1;
        break;
    default:
        break;
    }
    return rc;
}

static void mm_daemon_config_buf_close(mm_daemon_cfg_t *cfg_obj)
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
    if (cfg_obj->cap_buf.mapped) {
        munmap(cfg_obj->cap_buf.vaddr, sizeof(cam_capability_t));
        close(cfg_obj->cap_buf.fd);
        cfg_obj->cap_buf.fd = 0;
        cfg_obj->cap_buf.mapped = 0;
    }
    if (cfg_obj->parm_buf.mapped) {
        munmap(cfg_obj->parm_buf.buf, sizeof(parm_buffer_t));
        close(cfg_obj->parm_buf.fd);
        cfg_obj->parm_buf.fd = 0;
        cfg_obj->parm_buf.mapped = 0;
        free(cfg_obj->parm_buf.cfg_buf);
    }
}

static void mm_daemon_config_thread_stop(mm_daemon_cfg_t *cfg_obj)
{
    mm_daemon_thread_info *info = NULL;
    int i;

    for (i = 0; i < MAX_DEV; i++) {
        info = cfg_obj->info[i];
        if (info && info->ops && info->ops->stop)
            info->ops->stop(info);
    }
}

static void mm_daemon_config_thread_close(mm_daemon_cfg_t *cfg_obj)
{
    mm_daemon_thread_info *info = NULL;
    int i;

    for (i = 0; i < MAX_DEV; i++) {
        info = cfg_obj->info[i];
        if (info) {
            mm_daemon_util_thread_close(info);
            cfg_obj->info[i] = NULL;
        }
    }
}

static void mm_daemon_config_stats_init(mm_daemon_cfg_t *cfg_obj)
{
    uint16_t stats_enable = cfg_obj->sdata->stats_enable;
    size_t i;

    for (i = 0; i < MSM_ISP_STATS_MAX; i++) {
        if (!((stats_enable >> i) & 0x1))
            continue;

        cfg_obj->stats_buf[i] = (mm_daemon_stats_buf_info *)
                calloc(1, sizeof(mm_daemon_stats_buf_info));

        if (cfg_obj->stats_buf[i] == NULL)
            continue;

        switch (i) {
        case MSM_ISP_STATS_AEC:
            cfg_obj->stats_buf[i]->ops.start = mm_daemon_config_vfe_stats_aec;
            cfg_obj->stats_buf[i]->ops.proc = mm_daemon_config_auto_exposure;
            break;
        case MSM_ISP_STATS_AF:
            cfg_obj->stats_buf[i]->ops.start = mm_daemon_config_vfe_stats_af;
            cfg_obj->stats_buf[i]->ops.proc = mm_daemon_config_auto_focus;
            break;
        case MSM_ISP_STATS_AWB:
            cfg_obj->stats_buf[i]->ops.start = mm_daemon_config_vfe_stats_awb;
            cfg_obj->stats_buf[i]->ops.proc = mm_daemon_config_auto_white_balance;
            break;
        default:
            break;
        }
        stats_enable &= ~BIT(i);
        if (!stats_enable)
            break;
    }
    mm_daemon_config_stats_buf_request(cfg_obj);
    mm_daemon_config_stats_buf_alloc(cfg_obj);
    mm_daemon_config_stats_buf_enqueue(cfg_obj);
}

static void mm_daemon_config_stats_deinit(mm_daemon_cfg_t *cfg_obj)
{
    uint32_t type;
    size_t i;

    mm_daemon_config_stats_buf_release(cfg_obj);
    mm_daemon_config_stats_buf_dealloc(cfg_obj);
    for (i = 0; i < ARRAY_SIZE(vfe_stats); i++) {
        type = vfe_stats[i];
        if (cfg_obj->stats_buf[type]) {
            free(cfg_obj->stats_buf[type]);
            cfg_obj->stats_buf[type] = NULL;
        }
    }
}

static void *mm_daemon_config_thread(void *data)
{
    size_t i;
    int cam_idx, ret = 0;
    struct pollfd fds[2];
    mm_daemon_thread_info *info = (mm_daemon_thread_info *)data;
    mm_daemon_sd_obj_t *sd = (mm_daemon_sd_obj_t *)info->data;
    mm_daemon_cfg_t *cfg_obj = NULL;
    mm_sensor_cfg_t *s_cfg = NULL;

    cfg_obj = (mm_daemon_cfg_t *)calloc(1, sizeof(mm_daemon_cfg_t));

    cfg_obj->cfg = info;
    pthread_mutex_lock(&cfg_obj->cfg->lock);
    pthread_mutex_init(&(cfg_obj->lock), NULL);
    cfg_obj->session_id = info->sid;
    cam_idx = info->sid - 1;
    s_cfg = (mm_sensor_cfg_t *)sd->sensor_sd[cam_idx].data;
    if (!s_cfg)
        goto cfg_close;
    cfg_obj->sdata = s_cfg->data;

    /* SOCK */
    cfg_obj->info[SOCK_DEV] = mm_daemon_util_thread_open(
            &sd->camera_sd[cam_idx], cam_idx,
            cfg_obj->cfg->pfds[1]);
    if (cfg_obj->info[SOCK_DEV] == NULL) {
        ALOGE("%s: failed to create socket", __FUNCTION__);
        pthread_cond_signal(&cfg_obj->cfg->cond);
        pthread_mutex_unlock(&cfg_obj->cfg->lock);
        goto cfg_close;
    }

    /* VFE */
    cfg_obj->vfe_fd = open(sd->vfe_sd.devpath, O_RDWR | O_NONBLOCK);
    if (cfg_obj->vfe_fd <= 0) {
        ALOGE("%s: failed to open VFE device at %s", __FUNCTION__, sd->vfe_sd.devpath);
        pthread_cond_signal(&cfg_obj->cfg->cond);
        pthread_mutex_unlock(&cfg_obj->cfg->lock);
        goto thread_close;
    }

    /* BUF */
    cfg_obj->buf_fd = open(sd->buf.devpath, O_RDWR | O_NONBLOCK);
    if (cfg_obj->buf_fd <= 0) {
        ALOGE("%s: failed to open genbuf device", __FUNCTION__);
        pthread_cond_signal(&cfg_obj->cfg->cond);
        pthread_mutex_unlock(&cfg_obj->cfg->lock);
        goto thread_close;
    }

    /* SNSR */
    cfg_obj->info[SNSR_DEV] = mm_daemon_util_thread_open(
            &sd->sensor_sd[cam_idx],
            cam_idx, cfg_obj->cfg->pfds[1]);
    if (!cfg_obj->info[SNSR_DEV]) {
        ALOGE("%s: Failed to launch sensor thread", __FUNCTION__);
        pthread_cond_signal(&cfg_obj->cfg->cond);
        pthread_mutex_unlock(&cfg_obj->cfg->lock);
        goto thread_close;
    }

    /* CSI */
    if (sd->num_csi) {
        cfg_obj->info[CSI_DEV] = mm_daemon_util_thread_open(
                &sd->csi[cfg_obj->sdata->csi_dev],
                cam_idx, cfg_obj->cfg->pfds[1]);
        if (!cfg_obj->info[CSI_DEV])
            goto thread_close;
    }

    /* LED */
    if (sd->led.found)
        cfg_obj->info[LED_DEV] = mm_daemon_util_thread_open(
                &sd->led, cam_idx, cfg_obj->cfg->pfds[1]);

    /* ACT */
    if (sd->act[cfg_obj->sdata->act_id].found)
        cfg_obj->info[ACT_DEV] = mm_daemon_util_thread_open(
                &sd->act[cfg_obj->sdata->act_id], cam_idx,
                cfg_obj->cfg->pfds[1]);

    for (i = 0; i < ARRAY_SIZE(isp_events); i++)
        mm_daemon_config_subscribe(cfg_obj, isp_events[i], 1);

    /* ION */
    cfg_obj->ion_fd = open("/dev/ion", O_RDONLY);
    if (cfg_obj->ion_fd < 0) {
        ALOGE("failed to open ion device");
        goto thread_close;
    }

    /* STATS */
    if (cfg_obj->sdata->stats_enable)
        mm_daemon_config_stats_init(cfg_obj);

    if (cfg_obj->sdata->aec_cfg) {
        cfg_obj->ae.c_gain = cfg_obj->sdata->aec_cfg->default_gain;
        cfg_obj->ae.c_line = cfg_obj->sdata->aec_cfg->default_line[PREVIEW];
    }

    info->state = STATE_POLL;
    pthread_cond_signal(&cfg_obj->cfg->cond);
    pthread_mutex_unlock(&cfg_obj->cfg->lock);
    fds[0].fd = cfg_obj->vfe_fd;
    fds[0].events = POLLIN|POLLRDNORM|POLLPRI;
    fds[1].fd = cfg_obj->cfg->pfds[0];
    fds[1].events = POLLIN|POLLRDNORM|POLLPRI;
    do {
        if (poll(fds, 2, -1) > 0) {
            if (fds[0].revents & POLLPRI)
                ret = mm_daemon_config_dequeue(cfg_obj);
            else if ((fds[1].revents & POLLIN) &&
                    (fds[1].revents & POLLRDNORM))
                ret = mm_daemon_config_read_pipe(cfg_obj);
            else
                usleep(1000);
            if (ret < 0) {
                mm_daemon_util_subdev_cmd(cfg_obj->info[SNSR_DEV],
                        SENSOR_CMD_SHUTDOWN, 0, FALSE);
                info->state = STATE_STOPPED;
                break;
            }
        } else {
            usleep(100);
            continue;
        }
    } while (info->state == STATE_POLL);

    mm_daemon_config_thread_stop(cfg_obj);

    if (cfg_obj->sdata->stats_enable)
        mm_daemon_config_stats_deinit(cfg_obj);

    mm_daemon_config_buf_close(cfg_obj);

    for (i = 0; i < ARRAY_SIZE(isp_events); i++)
        mm_daemon_config_subscribe(cfg_obj, isp_events[i], 0);

    if (cfg_obj->ion_fd > 0) {
        close(cfg_obj->ion_fd);
        cfg_obj->ion_fd = 0;
    }
thread_close:
    mm_daemon_config_thread_close(cfg_obj);
    if (cfg_obj->buf_fd > 0) {
        close(cfg_obj->buf_fd);
        cfg_obj->buf_fd = 0;
    }
    if (cfg_obj->vfe_fd > 0) {
        close(cfg_obj->vfe_fd);
        cfg_obj->vfe_fd = 0;
    }
cfg_close:
    pthread_mutex_destroy(&(cfg_obj->lock));
    free(cfg_obj);
    return NULL;
}

mm_daemon_thread_info *mm_daemon_config_open(mm_daemon_sd_obj_t *sd,
        uint8_t session_id, int32_t cb_pfd)
{
    mm_daemon_thread_info *info = NULL;

    if ((session_id > sd->num_sensors) ||
            (session_id <= 0)) {
        ALOGE("%s: invalid session_id %d", __FUNCTION__, session_id);
        return NULL;
    }

    info = (mm_daemon_thread_info *)calloc(1, sizeof(mm_daemon_thread_info));
    if (info == NULL)
        return NULL;

    if (pipe(info->pfds) < 0) {
        free(info);
        return NULL;
    }
    info->sid = session_id;
    info->cb_pfd = cb_pfd;
    info->data = (void *)sd;
    pthread_mutex_init(&(info->lock), NULL);
    pthread_cond_init(&(info->cond), NULL);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_mutex_lock(&info->lock);
    pthread_create(&info->pid, &attr, mm_daemon_config_thread,
            (void *)info);
    pthread_cond_wait(&info->cond, &info->lock);
    pthread_mutex_unlock(&info->lock);
    pthread_attr_destroy(&attr);
    return info;
}

int mm_daemon_config_close(mm_daemon_thread_info *info)
{
    void *rc;

    if (!info)
        return -EINVAL;

    pthread_join(info->pid, &rc);
    pthread_mutex_destroy(&(info->lock));
    pthread_cond_destroy(&(info->cond));
    free(info);
    return (int)rc;
}
