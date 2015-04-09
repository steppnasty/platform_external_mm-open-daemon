/*
   Copyright (C) 2014-2015 Brian Stepp 
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

#define LOG_TAG "mm-daemon"

#include <sys/types.h>
#include <media/msm_camera.h>
#include <media/msm_isp.h>
#include <linux/media.h>

#include <cutils/properties.h>

#include "mm_daemon.h"

#define SET_PARM_BIT32(parm, parm_arr) \
    (parm_arr[parm/32] |= (1<<(parm%32)))

static const char *mm_daemon_server_find_subdev(mm_daemon_obj_t *mm_obj,
        enum msm_cam_subdev_type sd_type)
{
    int dev_fd = 0;
    int media_dev_num = 0;
    char subdev_name[32];
    struct media_device_info dev_info;

    while (1) {
        char media_dev_name[32];
        snprintf(media_dev_name, sizeof(media_dev_name), "/dev/media%d",
                media_dev_num);
        dev_fd = open(media_dev_name, O_RDWR | O_NONBLOCK);
        if (dev_fd < 0) {
            ALOGI("%s: Media subdevice query done", __FUNCTION__);
            break;
        }
        media_dev_num++;
        if (ioctl(dev_fd, MEDIA_IOC_DEVICE_INFO, &dev_info) < 0) {
            ALOGE("Error: Media dev info ioctl failed: %s", strerror(errno));
            break;
        }

        if (strncmp(dev_info.model, QCAMERA_SERVER_NAME, sizeof(dev_info.model) != 0)) {
            close(dev_fd);
            continue;
        }

        int num_entities = 1;
        while (1) {
            struct media_entity_desc entity;
            memset(&entity, 0, sizeof(entity));
            entity.id = num_entities++;
            if (ioctl(dev_fd, MEDIA_IOC_ENUM_ENTITIES, &entity) < 0) {
                ALOGI("Done enumerating media entities");
                break;
            }
            if (entity.type == MEDIA_ENT_T_V4L2_SUBDEV && entity.group_id == sd_type) {
                snprintf(subdev_name, sizeof(subdev_name), "/dev/v4l-subdev%d", entity.revision);
                mm_obj->sd_revision[sd_type] = entity.revision;
            }
        }
    }
    if (dev_fd > 0)
        close(dev_fd);
    return subdev_name;
}

static void mm_daemon_vfe_close(mm_daemon_obj_t *mm_obj)
{
    struct msm_vfe_cfg_cmd cfgcmd;
    struct msm_isp_cmd vfecmd;

    memset(&cfgcmd, 0, sizeof(cfgcmd));
    memset(&vfecmd, 0, sizeof(vfecmd));
    vfecmd.id = VFE_CMD_STOP;
    vfecmd.length = 0;
    vfecmd.value = NULL;
    cfgcmd.cmd_type = CMD_GENERAL;
    cfgcmd.length = sizeof(vfecmd);
    cfgcmd.value = (void *)&vfecmd;
    ioctl(mm_obj->cfg_fd, MSM_CAM_IOCTL_CONFIG_VFE, &cfgcmd);   
}

static int mm_daemon_ctrl_cmd_done(mm_daemon_obj_t *mm_obj,
        struct msm_ctrl_cmd *ctrl_cmd, int length, void *value)
{
    struct msm_ctrl_cmd new_ctrl_cmd;
    struct msm_camera_v4l2_ioctl_t ioctl_ptr;
    uint8_t *ctrl_data = NULL;
    int rc = 0;
    int value_len = length - sizeof(struct msm_ctrl_cmd);

    memset(&new_ctrl_cmd, 0, sizeof(new_ctrl_cmd));
    memset(&ioctl_ptr, 0, sizeof(ioctl_ptr));

    ioctl_ptr.ioctl_ptr = ctrl_cmd;

    new_ctrl_cmd.type = ctrl_cmd->type;
    new_ctrl_cmd.status = CAM_CTRL_ACCEPTED;
    new_ctrl_cmd.evt_id = ctrl_cmd->evt_id;
    new_ctrl_cmd.length = value_len;

    ctrl_cmd->status = CAM_CTRL_ACCEPTED;
    ctrl_cmd->length = length;

    ctrl_data = malloc(length);
    memcpy((void *)ctrl_data, (void *)&new_ctrl_cmd, sizeof(struct msm_ctrl_cmd));
    if (value)
        memcpy((void *)(ctrl_data + sizeof(new_ctrl_cmd)), value,
                value_len);
    ctrl_cmd->value = (void *)ctrl_data;

    rc = ioctl(mm_obj->server_fd, MSM_CAM_V4L2_IOCTL_CTRL_CMD_DONE, &ioctl_ptr);
    free(ctrl_data);

    return rc;
}

static void mm_daemon_set_parm_bit(cam_prop_t *properties)
{
    SET_PARM_BIT32(CAMERA_SET_PARM_DIMENSION, properties->parm);
    SET_PARM_BIT32(CAMERA_GET_CAPABILITIES, properties->parm);
}

static void mm_daemon_set_parm(cam_ctrl_dimension_t *parms)
{
    parms->video_width = 640;
    parms->video_height = 480;
    parms->picture_width = 2592;
    parms->picture_height = 1944;
    parms->display_width = 800;
    parms->display_height = 480;
    parms->ui_thumbnail_width = 320;
    parms->ui_thumbnail_height = 240;
    parms->raw_picture_width = 640;
    parms->raw_picture_height = 480;
}

static void mm_daemon_set_properties(cam_prop_t *properties)
{
    mm_daemon_set_parm_bit(properties);
    properties->max_pict_width = 2592;
    properties->max_pict_height = 1944;
    properties->max_preview_width = 720;
    properties->max_preview_height = 480;
    properties->max_video_width = 720;
    properties->max_video_height = 480;
    properties->default_preview_width = 720;
    properties->default_preview_height = 480;
    properties->preview_format = CAMERA_YUV_420_NV12;
}

static void mm_daemon_parm_get_focal_length(focus_distances_info_t *focus_distance)
{
    focus_distance->focus_distance[0] = 0.0f;
    focus_distance->real_gain = 0.5f;
    focus_distance->exp_time = 0.0f;
}

static void mm_daemon_parm_frame_offset(cam_frame_resolution_t *frame_offset)
{
    int i;
    int num_planes = 2;
    frame_offset->frame_offset.num_planes = num_planes;
    frame_offset->frame_offset.mp[0].len = 0x4b000;
    frame_offset->frame_offset.mp[1].len = 0x26000;
    for (i = 0; i < num_planes; i++)
        frame_offset->frame_offset.mp[i].offset = 0;
    frame_offset->frame_offset.frame_len = 0x71000;
}

static int mm_daemon_vfe_cmd(mm_daemon_obj_t *mm_obj, int cmd_type, int cmd,
        int length, void *value, unsigned int ioctl_cmd)
{
    int rc = 0;
    struct msm_vfe_cfg_cmd cfgcmd;
    struct msm_isp_cmd vfecmd;

    memset(&cfgcmd, 0, sizeof(cfgcmd));
    memset(&vfecmd, 0, sizeof(vfecmd));
    vfecmd.id = cmd;
    vfecmd.length = length;
    vfecmd.value = value;
    cfgcmd.cmd_type = cmd_type;
    cfgcmd.length = sizeof(vfecmd);
    cfgcmd.value = (void *)&vfecmd;
    rc = ioctl(mm_obj->cfg_fd, ioctl_cmd, &cfgcmd);

    return rc;
}

static int mm_daemon_config_sensor(mm_daemon_obj_t *mm_obj, int rs, int type)
{
    int rc = 0;
    struct sensor_cfg_data cdata;

    cdata.mode = 0;
    cdata.cfgtype = type;
    cdata.rs = rs;
    rc = ioctl(mm_obj->cfg_fd, MSM_CAM_IOCTL_SENSOR_IO_CFG, &cdata);
    return rc;
}

static int mm_daemon_config_set_subdev(mm_daemon_obj_t *mm_obj,
        enum msm_cam_subdev_type sdev_type)
{
    int rc = 0;
    struct msm_mctl_set_sdev_data set_data;

    set_data.sdev_type = sdev_type;
    set_data.revision = mm_obj->sd_revision[sdev_type];

    rc = ioctl(mm_obj->cfg_fd, MSM_CAM_IOCTL_SET_MCTL_SDEV, &set_data);
    return rc;
}

static int mm_daemon_config_unset_subdev(mm_daemon_obj_t *mm_obj,
        enum msm_cam_subdev_type sdev_type)
{
    int rc = 0;
    struct msm_mctl_set_sdev_data set_data;

    set_data.sdev_type = sdev_type;
    set_data.revision = mm_obj->sd_revision[sdev_type];

    rc = ioctl(mm_obj->cfg_fd, MSM_CAM_IOCTL_UNSET_MCTL_SDEV, &set_data);
    return rc;
}

static int mm_daemon_start_preview(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t operation_mode;

    rc = mm_daemon_config_sensor(mm_obj, MSM_SENSOR_RES_QTR, CFG_SET_MODE);
    if (rc < 0)
        goto end;
    mm_obj->curr_gain = 0x20;
    rc = mm_daemon_config_exp_gain(mm_obj, mm_obj->curr_gain, 0x31f);
    if (rc < 0)
        goto end;
    operation_mode = VFE_OUTPUTS_PREVIEW_AND_VIDEO;
    rc = ioctl(mm_obj->cfg_fd, MSM_CAM_IOCTL_SET_VFE_OUTPUT_TYPE, &operation_mode);
    if (rc < 0)
        goto end;

    rc = mm_daemon_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_RESET, 0, NULL,
            MSM_CAM_IOCTL_CONFIG_VFE);
    if (rc < 0)
        goto end;

end:
    return rc;
}

static int mm_daemon_set_fmt_mplane(mm_daemon_obj_t *mm_obj,
        struct msm_ctrl_cmd *ctrl_cmd, struct img_plane_info *plane_info)
{
    int rc = 0;

    plane_info->plane[0].offset = 0x0;
    plane_info->plane[0].size = 0x4b000;
    plane_info->plane[1].offset = 0x0;
    plane_info->plane[1].size = 0x26000;
    mm_obj->inst_handle = plane_info->inst_handle;
    ctrl_cmd->length = sizeof(struct img_plane_info);
    ctrl_cmd->value = plane_info;
    ctrl_cmd->status = 1;
    return rc;
}

static int mm_daemon_open_config(mm_daemon_obj_t *mm_obj, char *config_node)
{
    char config_dev[32];
    int rc = 0;
    int n_try = 2;

    snprintf(config_dev, sizeof(config_dev), "/dev/msm_camera/%s",
            config_node);
    do {
        n_try--;
        mm_obj->cfg_fd = open(config_dev, O_RDWR | O_NONBLOCK);
        if ((mm_obj->cfg_fd > 0) || (errno != EIO) || (n_try <= 0)) {
            ALOGI("%s: config opened", __FUNCTION__);
            break;
        }
        usleep(10000);
    } while (n_try > 0);

    if (mm_obj->cfg_fd <= 0) {
        ALOGE("%s: cannot open config device", __FUNCTION__);
        rc = -1;
    }
    return rc;
}

static int mm_daemon_vpe_start(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    struct msm_mctl_post_proc_cmd mctl_pp_cmd;
    struct msm_vpe_clock_rate clk_rate;

    memset(&mctl_pp_cmd, 0, sizeof(mctl_pp_cmd));
    memset(&clk_rate, 0, sizeof(clk_rate));
    mctl_pp_cmd.type = MSM_PP_CMD_TYPE_VPE;
    mctl_pp_cmd.cmd.id = VPE_CMD_ENABLE;
    mctl_pp_cmd.cmd.length = sizeof(clk_rate);
    mctl_pp_cmd.cmd.value = &clk_rate;
    rc = ioctl(mm_obj->cfg_fd, MSM_CAM_IOCTL_MCTL_POST_PROC, &mctl_pp_cmd);
    if (rc == 0)
        mm_obj->vpe_enabled = 1;
    return rc;
}

static int mm_daemon_proc_ctrl_cmd(mm_daemon_obj_t *mm_obj,
        struct msm_ctrl_cmd *ctrl_cmd, void *cmd_data)
{
    int rc = 0;
    int length;
    struct msm_ctrl_cmd *priv_ctrl_cmd;
    void *cmd_value;

    priv_ctrl_cmd = (struct msm_ctrl_cmd *)cmd_data;
    cmd_value = (void *)((uint32_t)cmd_data + sizeof(struct msm_ctrl_cmd));

    switch (priv_ctrl_cmd->type) {
        case CAMERA_SET_PARM_DIMENSION: { /* 1 */
            cam_ctrl_dimension_t parms;
            length = sizeof(struct msm_ctrl_cmd) + sizeof(parms);
            memset(&parms, 0, sizeof(parms));
            mm_daemon_set_parm(&parms);
            rc = mm_daemon_ctrl_cmd_done(mm_obj, ctrl_cmd, length, (void *)&parms);
            break;
        }
        case CAMERA_SET_PARM_EXPOSURE_COMPENSATION: { /* 8 */
            break;
        }
        case CAMERA_SET_PARM_FPS: { /* 16 */
            break;
        }
        case CAMERA_SET_PARM_BESTSHOT_MODE: { /* 27 */
            break;
        }
        case CAMERA_SET_PARM_ROLLOFF: { /* 37 */
            int8_t shadeValue = 0;
            length = sizeof(struct msm_ctrl_cmd) + sizeof(int8_t);
            rc = mm_daemon_ctrl_cmd_done(mm_obj, ctrl_cmd, length, (void *)&shadeValue);
            break;
        }
        case CAMERA_GET_PARM_MAXZOOM: { /* 47 */
            int maxzoom = 2;
            length = sizeof(struct msm_ctrl_cmd) + sizeof(int);
            rc = mm_daemon_ctrl_cmd_done(mm_obj, ctrl_cmd, length, (void *)&maxzoom);
            break;
        }
        case CAMERA_GET_PARM_ZOOMRATIOS: { /* 48 */
            break;
        }
        case CAMERA_SET_PARM_LED_MODE: { /* 50 */
            break;
        }
        case CAMERA_SET_FPS_MODE: { /* 56 */
            break;
        }
        case CAMERA_GET_CAPABILITIES: { /* 71 */
            cam_prop_t properties;
            length = sizeof(struct msm_ctrl_cmd) + sizeof(properties);
            memset(&properties, 0, sizeof(properties));
            mm_daemon_set_properties(&properties);
            rc = mm_daemon_ctrl_cmd_done(mm_obj, ctrl_cmd, length, (void *)&properties);
            break;
        }
        case CAMERA_GET_PARM_FOCAL_LENGTH: { /* 81 */
            focus_distances_info_t focus_distance;
            length = sizeof(struct msm_ctrl_cmd) + sizeof(focus_distance);
            memset(&focus_distance, 0, sizeof(focus_distance));
            mm_daemon_parm_get_focal_length(&focus_distance);
            rc = mm_daemon_ctrl_cmd_done(mm_obj, ctrl_cmd, length,
                    (void *)&focus_distance);
            break;
        }
        case CAMERA_SET_PARM_WAVELET_DENOISE: { /* 84 */
            break;
        }
        case CAMERA_SET_PARM_MCE: { /* 85 */
            int32_t MCEValue;
            length = sizeof(struct msm_ctrl_cmd) + sizeof(int32_t);
            rc = mm_daemon_ctrl_cmd_done(mm_obj, ctrl_cmd, length, (void *)&MCEValue);
            break;
        }
        case CAMERA_SET_PARM_HFR: { /* 89 */
            break;
        }
        case CAMERA_SET_REDEYE_REDUCTION: { /* 90 */
            break;
        }
        case CAMERA_SET_PARM_PREVIEW_FORMAT: { /* 94 */
            int preview_format;
            length = sizeof(struct msm_ctrl_cmd) + sizeof(int32_t);
            preview_format = CAMERA_YUV_420_NV21;
            rc = mm_daemon_ctrl_cmd_done(mm_obj, ctrl_cmd, length,
                    (void *)&preview_format);
            break;
        }
        case CAMERA_SET_RECORDING_HINT: { /* 107 */
            uint32_t recording_hint;
            length = sizeof(struct msm_ctrl_cmd) + sizeof(int32_t);
            rc = mm_daemon_ctrl_cmd_done(mm_obj, ctrl_cmd, length,
                    (void *)&recording_hint);
            break;
        }
        case CAMERA_GET_PARM_MAX_HFR_MODE: { /* 111 */
            camera_hfr_mode_t hfr_mode;
            length = sizeof(struct msm_ctrl_cmd) + sizeof(hfr_mode);
            memset(&hfr_mode, 0, sizeof(hfr_mode));
            rc = mm_daemon_ctrl_cmd_done(mm_obj, ctrl_cmd, length, (void *)&hfr_mode);
            break;
        }
        case CAMERA_GET_PARM_FRAME_RESOLUTION: { /* 126 */
            cam_frame_resolution_t frame_offset;
            length = sizeof(struct msm_ctrl_cmd) + sizeof(frame_offset);
            memset(&frame_offset, 0, sizeof(frame_offset));
            memcpy(&frame_offset, cmd_value, sizeof(frame_offset));
            mm_daemon_parm_frame_offset(&frame_offset);
            rc = mm_daemon_ctrl_cmd_done(mm_obj, ctrl_cmd, length,
                    (void *)&frame_offset);
            break;
        }
        case CAMERA_SET_INFORM_STARTPREVIEW: { /* 135 */
            int32_t didx = 0;
            length = sizeof(struct msm_ctrl_cmd) + sizeof(int32_t);
            rc = mm_daemon_ctrl_cmd_done(mm_obj, ctrl_cmd, length, (void *)&didx);
            break;
        }
        default:
            ALOGI("%s: unknown ctrl type %d", __FUNCTION__, priv_ctrl_cmd->type);
            break;
    }
    return rc;
}

int mm_daemon_notify(mm_daemon_obj_t *mm_obj)
{
    struct v4l2_event ev;
    struct msm_camera_v4l2_ioctl_t ioctl_ptr;
    struct msm_ctrl_cmd ctrl_cmd;
    struct msm_isp_event_ctrl isp_evt;
    uint8_t ctrl_cmd_data[512];
    void *cmd_value;
    int rc = 0;

    memset(&ev, 0, sizeof(ev));
    memset(&ioctl_ptr, 0, sizeof(ioctl_ptr));
    memset(&ctrl_cmd, 0, sizeof(struct msm_ctrl_cmd));
    memset(&isp_evt, 0, sizeof(isp_evt));
    memset(&ctrl_cmd_data, 0, 512);

    isp_evt.isp_data.ctrl.value = (uint32_t *)&ctrl_cmd_data;
    /* Stick a pointer to isp_evt into the payload */
    ioctl_ptr.ioctl_ptr = (void *)&isp_evt;

    /* V4L2 dequeue the queue index */
    rc = ioctl(mm_obj->server_fd, VIDIOC_DQEVENT, &ev);
    if (rc < 0)
        goto dequeue_error;

    isp_evt.isp_data.ctrl.queue_idx = ev.u.data[0];
    /* MSM dequeue the control data */
    if (ioctl(mm_obj->server_fd, MSM_CAM_V4L2_IOCTL_GET_EVENT_PAYLOAD,
            &ioctl_ptr) < 0)
        goto dequeue_error;

    ioctl_ptr.ioctl_ptr = &ctrl_cmd;
    cmd_value = malloc(isp_evt.isp_data.ctrl.length);
    memcpy(cmd_value, &ctrl_cmd_data, isp_evt.isp_data.ctrl.length);
    ctrl_cmd.evt_id = isp_evt.isp_data.ctrl.evt_id;
    ctrl_cmd.length = 0;

    switch (isp_evt.isp_data.ctrl.type) {
        case MSM_V4L2_VID_CAP_TYPE: /* 0 */
            mm_daemon_set_fmt_mplane(mm_obj, &ctrl_cmd,
                    (struct img_plane_info *)cmd_value);
            rc = ioctl(mm_obj->server_fd, MSM_CAM_V4L2_IOCTL_CTRL_CMD_DONE, &ioctl_ptr);
            break;
        case MSM_V4L2_STREAM_ON: /* 1 */
            ctrl_cmd.status = 1;
            rc = ioctl(mm_obj->server_fd, MSM_CAM_V4L2_IOCTL_CTRL_CMD_DONE, &ioctl_ptr);
            if (rc < 0)
                break;
            if (mm_daemon_start_preview(mm_obj) < 0)
                goto general_error;
            break;
        case MSM_V4L2_STREAM_OFF: /* 2 */
            ctrl_cmd.status = 1;
            rc = ioctl(mm_obj->server_fd, MSM_CAM_V4L2_IOCTL_CTRL_CMD_DONE, &ioctl_ptr);
            break;
        case MSM_V4L2_SET_CTRL: /* 6 */
            ctrl_cmd.length = 0;
            ctrl_cmd.status = 1;
            rc = ioctl(mm_obj->server_fd, MSM_CAM_V4L2_IOCTL_CTRL_CMD_DONE, &ioctl_ptr);
            break;
        case MSM_V4L2_OPEN: /* 10 */
            ctrl_cmd.status = 0;
            if (mm_daemon_open_config(mm_obj, (char *)cmd_value) == 0) {
                if (mm_daemon_config_start(mm_obj) == 0)
                    ctrl_cmd.status = 1;
            } else {
                free(cmd_value);
                return -1;
            }

            rc = ioctl(mm_obj->server_fd, MSM_CAM_V4L2_IOCTL_CTRL_CMD_DONE, &ioctl_ptr);
            /* TODO: move sensor config to config thread */
            rc = mm_daemon_config_sensor(mm_obj, MSM_SENSOR_RES_QTR, CFG_SENSOR_INIT);
            mm_obj->vfe_fd = open(mm_daemon_server_find_subdev(mm_obj,
                    VFE_DEV), O_RDWR | O_NONBLOCK);
            if (mm_obj->vfe_fd <= 0) {
                ALOGE("%s: cannot open vfe device", __FUNCTION__);
                return -1;
            } else
                ALOGI("%s: vfe opened", __FUNCTION__);
            mm_daemon_config_set_subdev(mm_obj, VFE_DEV);
            rc = ioctl(mm_obj->vfe_fd, VIDIOC_MSM_VFE_INIT);
            break;
        case MSM_V4L2_CLOSE: /* 11 */
            ctrl_cmd.status = 1;
            if (mm_obj->vfe_fd) {
                ioctl(mm_obj->vfe_fd, VIDIOC_MSM_VFE_RELEASE);
                close(mm_obj->vfe_fd);
                mm_obj->vfe_fd = 0;
            }
            rc = ioctl(mm_obj->server_fd, MSM_CAM_V4L2_IOCTL_CTRL_CMD_DONE, &ioctl_ptr);
            mm_obj->state = STATE_STOPPED;
            break;
        case MSM_V4L2_SET_CTRL_CMD: /* 12 */
            if (mm_daemon_proc_ctrl_cmd(mm_obj, &ctrl_cmd, cmd_value) < 0)
                goto general_error;
            break;
        default:
            ALOGI("%s: unknown type %d", __FUNCTION__, isp_evt.isp_data.ctrl.type);
            break;
    }
    if (cmd_value)
        free(cmd_value);

    return rc;

dequeue_error:
    ALOGE("%s: Error dequeuing event", __FUNCTION__);
general_error:
    if (cmd_value)
        free(cmd_value);
    rc = -1;
    mm_obj->state = STATE_STOPPED;
    return rc;
}

int mm_daemon_poll_fn(mm_daemon_obj_t *mm_obj)
{
    int rc = 0, i;
    struct pollfd fds[2];

    mm_obj->state = STATE_POLL;
    do {
        for (i = 0; i < 2; i++) {
            fds[i].fd = mm_obj->server_fd;
            fds[i].events = POLLIN|POLLRDNORM|POLLPRI;
        }
        rc = poll(fds, 2, -1);
        if (rc > 0) {
            if (fds[0].revents & POLLPRI)
                    rc = mm_daemon_notify(mm_obj);
            else
                usleep(1000);
        } else {
            usleep(100);
            continue;
        }
    } while (mm_obj->state == STATE_POLL);
    return rc;
}

int mm_daemon_evt_sub(mm_daemon_obj_t *mm_obj, int subscribe)
{
    int rc = 0;
    struct v4l2_event_subscription sub;

    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_PRIVATE_START + MSM_CAM_RESP_V4L2;
    if (subscribe == 0) {
        rc = ioctl(mm_obj->server_fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub);
        if (rc < 0) {
            ALOGE("%s: unsubscribe event rc=%d", __FUNCTION__, rc);
            return rc;
        }
    } else {
        rc = ioctl(mm_obj->server_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
        if (rc < 0) {
            ALOGE("%s: subscribe event rc=%d", __FUNCTION__, rc);
            return rc;
        }
    }
    return rc;
}

static int mm_daemon_proc_packet_map(mm_daemon_obj_t *mm_obj,
        cam_sock_packet_t *packet)
{
    int rc = 0;

    switch (packet->payload.frame_fd_map.ext_mode) {
        case MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW:
            break;
        default:
            break;
    }
    return rc;
}

static int mm_daemon_proc_packet(mm_daemon_socket_t *mm_sock,
        cam_sock_packet_t *packet)
{
    int rc = 0;
    mm_daemon_obj_t *mm_obj = (mm_daemon_obj_t *)mm_sock->daemon_obj;

    switch (packet->msg_type) {
        case CAM_SOCK_MSG_TYPE_FD_MAPPING:
            rc = mm_daemon_proc_packet_map(mm_obj, packet);
            break;
        case CAM_SOCK_MSG_TYPE_FD_UNMAPPING:
            break;
        default:
            rc = -1;
            break;
    }
    return rc;
}

static void *mm_daemon_sock_mon(void *data)
{
    mm_daemon_socket_t *mm_sock = (mm_daemon_socket_t *)data;
    cam_sock_packet_t packet;
    struct msghdr msgh;
    struct iovec iov[1];
    char control[CMSG_SPACE(sizeof(int))];
    int rc = 0;
    int i;

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

        mm_daemon_proc_packet(mm_sock, &packet);
    } while (mm_sock->state == SOCKET_STATE_CONNECTED);
    return NULL;
}

static int mm_daemon_start_sock_thread(mm_daemon_socket_t *mm_sock)
{
    pthread_create(&mm_sock->pid, NULL, mm_daemon_sock_mon, (void *)mm_sock);
    return 0;
}

int mm_daemon_open()
{
    int sock;
    struct sockaddr_un sockaddr;
    int rc = 0;
    int n_try = 2;
    int cam_id = 0;
    int i;
    mm_daemon_obj_t *mm_obj;

    mm_obj = (mm_daemon_obj_t *)malloc(sizeof(mm_daemon_obj_t));
    if (mm_obj == NULL) {
        rc = -1;
        goto mem_error;
    }

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        ALOGE("%s: Error creating socket", __FUNCTION__);
        goto error;
    }
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sun_family = AF_UNIX;
    snprintf(sockaddr.sun_path, sizeof(sockaddr.sun_path), "/data/cam_socket%d",
            cam_id);
    rc = unlink(sockaddr.sun_path);
    if (rc != 0 && errno != ENOENT) {
        ALOGE("%s: failed to unlink old socket '%s': %s", __FUNCTION__,
                sockaddr.sun_path, strerror(errno));
        goto error;
    }
    rc = bind(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (rc) {
        ALOGE("%s: socket=%d %s", __FUNCTION__, sock, strerror(errno));
        goto socket_error;
    }

    chmod(sockaddr.sun_path, 0660);
    mm_obj->sock_obj.sock_fd = sock;
    mm_obj->sock_obj.daemon_obj = mm_obj;

    mm_daemon_start_sock_thread(&mm_obj->sock_obj);

    /* Open the server device */
    do {
        n_try--;
        mm_obj->server_fd = open("/dev/video100", O_RDWR | O_NONBLOCK);
        if ((mm_obj->server_fd > 0) || (errno != EIO) || (n_try <= 0)) {
            ALOGI("%s: server opened", __FUNCTION__);
            break;
        }
        usleep(10000);
    } while (n_try > 0);

    if (mm_obj->server_fd <= 0) {
        ALOGE("%s: cannot open server device", __FUNCTION__);
        rc = -1;
        goto server_error;
    }

    mm_obj->ion_fd = open("/dev/ion", O_RDONLY);
    pthread_mutex_init(&mm_obj->mutex, NULL);

    rc = mm_daemon_evt_sub(mm_obj, 1);
    if (rc < 0) {
        ALOGE("%s: event subscription error", __FUNCTION__);
        goto error;
    }

    rc = mm_daemon_poll_fn(mm_obj);
    if (rc < 0) {
        ALOGE("%s: poll error", __FUNCTION__);
        goto poll_error;
    }

poll_error:
    ALOGI("Shutting down");
    mm_daemon_config_stop(mm_obj);
#if 0
    mm_daemon_config_unset_subdev(mm_obj, CSIC_DEV);
#endif
    mm_daemon_config_unset_subdev(mm_obj, VFE_DEV);
    if (mm_obj->vfe_fd > 0) {
        close(mm_obj->vfe_fd);
        mm_obj->vfe_fd = 0;
    }
    if (mm_obj->cfg_fd > 0) {
        close(mm_obj->cfg_fd);
        mm_obj->cfg_fd = 0;
    }
socket_error:
    unlink(sockaddr.sun_path);
    close(sock);
error:
    pthread_mutex_destroy(&mm_obj->mutex);
mctl_error:
    if (mm_obj->server_fd > 0) {
        close(mm_obj->server_fd);
        mm_obj->server_fd = 0;
    }
    close(mm_obj->ion_fd);
server_error:
    free(mm_obj);
mem_error:
    return rc;
}


int main(int argc, char **argv)
{
    int rc;
    rc = mm_daemon_open();
    return rc;
}
