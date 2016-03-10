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

#define LOG_TAG "mm-daemon"

#include <sys/types.h>
#include <cutils/properties.h>
#include "mm_daemon.h"

static char subdev_name[32];
static pthread_mutex_t sd_lock;

static uint32_t msm_events[] = {
    MSM_CAMERA_NEW_SESSION,
    MSM_CAMERA_DEL_SESSION,
    MSM_CAMERA_SET_PARM,
    MSM_CAMERA_GET_PARM,
    MSM_CAMERA_MSM_NOTIFY,
};

static void mm_daemon_pack_event(mm_daemon_obj_t *mm_obj, struct v4l2_event *event,
        unsigned int command, uint32_t evt_id, int stream_id, unsigned int status)
{
    struct msm_v4l2_event_data *msm_evt;

    msm_evt = (struct msm_v4l2_event_data *)&event->u.data;
    msm_evt->session_id = mm_obj->session_id;
    msm_evt->stream_id = stream_id;
    msm_evt->command = command;
    msm_evt->status = status;
    event->type = MSM_CAMERA_V4L2_EVENT_TYPE;
    event->id = evt_id;
}

static void mm_daemon_server_config_cmd(mm_daemon_obj_t *mm_obj, uint8_t cmd,
        unsigned int stream_id)
{
    int len;
    mm_daemon_pipe_evt_t pipe_cmd;

    memset(&pipe_cmd, 0, sizeof(pipe_cmd));
    pipe_cmd.cmd = cmd;
    pipe_cmd.val = stream_id;
    len = write(mm_obj->cfg_pfds[1], &pipe_cmd, sizeof(pipe_cmd));
    if (len < 1)
        ALOGI("%s: write error", __FUNCTION__);
}

char *mm_daemon_server_find_subdev(char *dev_name,
        uint32_t group_id, uint32_t sd_type)
{
    int dev_fd = 0;
    int media_dev_num = 0;
    struct media_device_info dev_info;

    pthread_mutex_lock(&sd_lock);
    while (1) {
        char media_dev_name[32];
        snprintf(media_dev_name, sizeof(media_dev_name), "/dev/media%d",
                media_dev_num);
        dev_fd = open(media_dev_name, O_RDWR | O_NONBLOCK);
        if (dev_fd < 0)
            break;
        media_dev_num++;
        memset(&dev_info, 0, sizeof(dev_info));
        if (ioctl(dev_fd, MEDIA_IOC_DEVICE_INFO, &dev_info) < 0) {
            ALOGE("Error: Media dev info ioctl failed: %s", strerror(errno));
            break;
        }

        if (strncmp(dev_info.model, dev_name, sizeof(dev_info.model)) != 0) {
            close(dev_fd);
            dev_fd = 0;
            continue;
        }

        int num_entities = 1;
        while (1) {
            struct media_entity_desc entity;
            memset(&entity, 0, sizeof(entity));
            entity.id = num_entities++;
            if (ioctl(dev_fd, MEDIA_IOC_ENUM_ENTITIES, &entity) < 0)
                break;
            if (entity.group_id != group_id)
                continue;
            if (entity.type == sd_type && entity.group_id == group_id) {
                snprintf(subdev_name, sizeof(subdev_name), "/dev/%s", entity.name);
                break;
            }
        }
        break;
    }
    if (dev_fd > 0)
        close(dev_fd);
    dev_fd = 0;
    pthread_mutex_unlock(&sd_lock);
    return subdev_name;
}

static void mm_daemon_parm(mm_daemon_obj_t *mm_obj)
{
    int next = 1;
    int current, position;
    parm_buffer_t *p_table;

    p_table = mm_obj->parm_buf.buf;
    current = GET_FIRST_PARAM_ID(p_table);
    position = current;
    while (next) {
        switch (position) {
            case CAM_INTF_PARM_HAL_VERSION: {
                int32_t hal_version;
                memcpy(&hal_version, POINTER_OF(current, p_table), sizeof(hal_version));
                break;
            }
            case CAM_INTF_PARM_ANTIBANDING: {
                int32_t value;
                memcpy(&value, POINTER_OF(current, p_table), sizeof(value));
                break;
            }
            case CAM_INTF_PARM_SET_PP_COMMAND: {
                tune_cmd_t pp_cmd;
                memcpy(&pp_cmd, POINTER_OF(current, p_table), sizeof(pp_cmd));
                break;
            }
            case CAM_INTF_PARM_TINTLESS: {
                int32_t value;
                memcpy(&value, POINTER_OF(current, p_table), sizeof(value));
                break;
            }
            default:
                next = 0;
                break;
        }
        position = GET_NEXT_PARAM_ID(current, p_table);
        if (position == 0 || position == current)
            next = 0;
        else
            current = position;
    }
}

static void mm_daemon_capability_fill(mm_daemon_obj_t *mm_obj)
{
    cam_capability_t mm_cap;

    memset(&mm_cap, 0, sizeof(cam_capability_t));
    mm_cap.picture_sizes_tbl[0].width = 2592;
    mm_cap.picture_sizes_tbl[0].height = 1952;
    mm_cap.picture_sizes_tbl[1].width = 2592;
    mm_cap.picture_sizes_tbl[1].height = 1936;
    mm_cap.picture_sizes_tbl[2].width = 2592;
    mm_cap.picture_sizes_tbl[2].height = 1456;
    mm_cap.picture_sizes_tbl_cnt = 3;

    mm_cap.preview_sizes_tbl_cnt = 2;
    mm_cap.preview_sizes_tbl[0].width = 1280;
    mm_cap.preview_sizes_tbl[0].height = 720;
    mm_cap.preview_sizes_tbl[1].width = 640;
    mm_cap.preview_sizes_tbl[1].height = 480;

    mm_cap.video_sizes_tbl_cnt = 2;
    mm_cap.video_sizes_tbl[0].width = 1280;
    mm_cap.video_sizes_tbl[0].height = 720;
    mm_cap.video_sizes_tbl[1].width = 640;
    mm_cap.video_sizes_tbl[1].height = 480;

    mm_cap.scale_picture_sizes_cnt = 4;
    mm_cap.scale_picture_sizes[0].width = 640;
    mm_cap.scale_picture_sizes[0].height = 480;
    mm_cap.scale_picture_sizes[1].width = 512;
    mm_cap.scale_picture_sizes[1].height = 384;
    mm_cap.scale_picture_sizes[2].width = 384;
    mm_cap.scale_picture_sizes[2].height = 288;
    mm_cap.scale_picture_sizes[3].width = 0;
    mm_cap.scale_picture_sizes[3].height = 0;
    

    mm_cap.supported_focus_modes_cnt = 1;
    mm_cap.supported_focus_modes[0] = CAM_FOCUS_MODE_FIXED;

    mm_cap.supported_preview_fmt_cnt = 4;
    mm_cap.supported_preview_fmts[0] = CAM_FORMAT_YUV_420_NV12;
    mm_cap.supported_preview_fmts[1] = CAM_FORMAT_YUV_420_NV21;
    mm_cap.supported_preview_fmts[2] = CAM_FORMAT_YUV_420_NV21_ADRENO;
    mm_cap.supported_preview_fmts[3] = CAM_FORMAT_YUV_420_YV12;

    mm_cap.zoom_supported = 1;
    mm_cap.zoom_ratio_tbl_cnt = 2;
    mm_cap.zoom_ratio_tbl[0] = 1;
    mm_cap.zoom_ratio_tbl[1] = 2;

    mm_cap.focal_length = 3.53;

    mm_cap.supported_iso_modes[0] = CAM_ISO_MODE_AUTO;
    mm_cap.supported_iso_modes[1] = CAM_ISO_MODE_DEBLUR;
    mm_cap.supported_iso_modes[2] = CAM_ISO_MODE_100;
    mm_cap.supported_iso_modes[3] = CAM_ISO_MODE_200;
    mm_cap.supported_iso_modes[4] = CAM_ISO_MODE_400;
    mm_cap.supported_iso_modes[5] = CAM_ISO_MODE_800;
    mm_cap.supported_iso_modes_cnt = 6;

    mm_cap.supported_flash_modes[0] = CAM_FLASH_MODE_OFF;
    mm_cap.supported_flash_modes[1] = CAM_FLASH_MODE_AUTO;
    mm_cap.supported_flash_modes[2] = CAM_FLASH_MODE_ON;
    mm_cap.supported_flash_modes[3] = CAM_FLASH_MODE_TORCH;
    mm_cap.supported_flash_modes_cnt = 4;

    mm_cap.fps_ranges_tbl[0].min_fps = 9.0;
    mm_cap.fps_ranges_tbl[0].max_fps = 29.453;
    mm_cap.fps_ranges_tbl_cnt = 1;

    mm_cap.sensor_mount_angle = 90;

    memcpy(mm_obj->cap_buf.vaddr, &mm_cap, sizeof(mm_cap));
}

static void mm_daemon_notify(mm_daemon_obj_t *mm_obj)
{
    struct v4l2_event ev;
    struct v4l2_event new_ev;
    struct msm_v4l2_event_data *msm_evt = NULL;
    unsigned int status = MSM_CAMERA_ERR_CMD_FAIL;

    memset(&ev, 0, sizeof(ev));
    memset(&new_ev, 0, sizeof(new_ev));
    if (ioctl(mm_obj->server_fd, VIDIOC_DQEVENT, &ev) < 0) {
        ALOGE("%s: Error dequeueing event", __FUNCTION__);
        goto cmd_ack;
    }
    msm_evt = (struct msm_v4l2_event_data *)ev.u.data;

    if (ev.id != MSM_CAMERA_NEW_SESSION && mm_obj->cfg_state == STATE_STOPPED) {
        ALOGE("ERROR: Unable to handle command without active config thread");
        if (mm_obj->cfg_shutdown)
            goto cmd_ack;
        else {
            mm_obj->cfg_shutdown = 1;
            mm_daemon_pack_event(mm_obj, &new_ev, CAM_EVENT_TYPE_DAEMON_DIED,
                    MSM_CAMERA_PRIV_SHUTDOWN, msm_evt->stream_id, MSM_CAMERA_STATUS_SUCCESS);
            ioctl(mm_obj->server_fd, MSM_CAM_V4L2_IOCTL_NOTIFY, &new_ev);
            return;
        }
    }
    switch (ev.id) {
        case MSM_CAMERA_NEW_SESSION:
            mm_obj->session_id = msm_evt->session_id;
            mm_obj->stream_id = msm_evt->stream_id;
            pthread_mutex_lock(&mm_obj->mutex);
            mm_daemon_config_open(mm_obj);
            pthread_cond_wait(&mm_obj->cond, &mm_obj->mutex);
            pthread_mutex_unlock(&mm_obj->mutex);
            status = MSM_CAMERA_CMD_SUCESS;
            break;
        case MSM_CAMERA_DEL_SESSION:
            mm_daemon_server_config_cmd(mm_obj, CFG_CMD_SHUTDOWN,
                    msm_evt->stream_id);
            mm_daemon_config_close(mm_obj);
            status = MSM_CAMERA_CMD_SUCESS;
            break;
        case MSM_CAMERA_SET_PARM:
            switch (msm_evt->command) {
                case MSM_CAMERA_PRIV_STREAM_ON:
                    mm_daemon_server_config_cmd(mm_obj, CFG_CMD_STREAM_START,
                            msm_evt->stream_id);
                    break;
                case MSM_CAMERA_PRIV_STREAM_OFF:
                    mm_daemon_server_config_cmd(mm_obj, CFG_CMD_STREAM_STOP,
                            msm_evt->stream_id);
                    break;
                case MSM_CAMERA_PRIV_NEW_STREAM:
                    mm_obj->stream_id = msm_evt->stream_id;
                    if (msm_evt->stream_id > 0)
                        mm_daemon_server_config_cmd(mm_obj, CFG_CMD_NEW_STREAM,
                                msm_evt->stream_id);
                    break;
                case MSM_CAMERA_PRIV_DEL_STREAM:
                    if (msm_evt->stream_id > 0)
                        mm_daemon_server_config_cmd(mm_obj, CFG_CMD_DEL_STREAM,
                                msm_evt->stream_id);
                    break;
                case CAM_PRIV_PARM:
                    if (mm_obj->parm_buf.mapped)
                        mm_daemon_parm(mm_obj);
                    break;
                case MSM_CAMERA_PRIV_S_FMT:
                case MSM_CAMERA_PRIV_SHUTDOWN:
                case MSM_CAMERA_PRIV_STREAM_INFO_SYNC:
                case CAM_PRIV_STREAM_INFO_SYNC:
                case CAM_PRIV_STREAM_PARM:
                    break;
                default:
                    goto cmd_ack;
                    break;
            }
            status = MSM_CAMERA_CMD_SUCESS;
            break;
        case MSM_CAMERA_GET_PARM:
            switch (msm_evt->command) {
                case MSM_CAMERA_PRIV_QUERY_CAP:
                    if (mm_obj->cap_buf.mapped) {
                        mm_daemon_capability_fill(mm_obj);
                        status = MSM_CAMERA_CMD_SUCESS;
                    } else
                        ALOGE("%s: Error: Capability buffer not mapped",
                                __FUNCTION__);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
cmd_ack:
    mm_daemon_pack_event(mm_obj, &new_ev, 0, ev.id, msm_evt->stream_id, status);
    ioctl(mm_obj->server_fd, MSM_CAM_V4L2_IOCTL_CMD_ACK, &new_ev);
}

int mm_daemon_server_pipe_cmd(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    ssize_t read_len;
    struct v4l2_event ev;
    mm_daemon_pipe_evt_t pipe_cmd;

    read_len = read(mm_obj->svr_pfds[0], &pipe_cmd, sizeof(pipe_cmd));
    switch (pipe_cmd.cmd) {
        case SERVER_CMD_MAP_UNMAP_DONE:
            mm_daemon_pack_event(mm_obj, &ev, CAM_EVENT_TYPE_MAP_UNMAP_DONE,
                    MSM_CAMERA_MSM_NOTIFY, pipe_cmd.val,
                    MSM_CAMERA_STATUS_SUCCESS);
            rc = ioctl(mm_obj->server_fd, MSM_CAM_V4L2_IOCTL_NOTIFY, &ev);
            break;
        default:
            ALOGI("%s: Unknown command on pipe", __FUNCTION__);
    }
    return rc;
}

int mm_daemon_poll_fn(mm_daemon_obj_t *mm_obj)
{
    int rc = 0, i;
    struct pollfd fds[2];

    pipe(mm_obj->svr_pfds);
    mm_obj->state = STATE_POLL;
    fds[0].fd = mm_obj->server_fd;
    fds[1].fd = mm_obj->svr_pfds[0];
    do {
        for (i = 0; i < 2; i++)
            fds[i].events = POLLIN|POLLRDNORM|POLLPRI;
        rc = poll(fds, 2, -1);
        if (rc > 0) {
            if ((fds[0].revents & POLLIN) &&
                    (fds[0].revents & POLLRDNORM))
                mm_daemon_notify(mm_obj);
            else if ((fds[1].revents & POLLIN) &&
                    (fds[1].revents & POLLRDNORM))
                mm_daemon_server_pipe_cmd(mm_obj);
            else
                usleep(1000);
        } else {
            usleep(100);
            continue;
        }
    } while (mm_obj->state == STATE_POLL);
    return rc;
}

static int mm_daemon_subscribe(mm_daemon_obj_t *mm_obj, int subscribe)
{
    int rc = 0;
    uint32_t i;
    int cmd = subscribe ? VIDIOC_SUBSCRIBE_EVENT : VIDIOC_UNSUBSCRIBE_EVENT;
    struct v4l2_event_subscription sub;

    memset(&sub, 0, sizeof(sub));
    sub.type = MSM_CAMERA_V4L2_EVENT_TYPE;
    for (i = 0; i < ARRAY_SIZE(msm_events); i++) {
        sub.id = msm_events[i];
        rc = ioctl(mm_obj->server_fd, cmd, &sub);
        if (rc < 0) {
            ALOGE("Error: event reg/unreg failed %d", rc);
            return rc;
        }
    }
    return rc;
}

int mm_daemon_open()
{
    int rc = 0;
    int n_try = 2;
    mm_daemon_obj_t *mm_obj;

    mm_obj = (mm_daemon_obj_t *)malloc(sizeof(mm_daemon_obj_t));
    if (mm_obj == NULL) {
        rc = -1;
        goto mem_error;
    }
    memset(mm_obj, 0, sizeof(mm_daemon_obj_t));

    pthread_mutex_init(&sd_lock, NULL);
    pthread_mutex_init(&(mm_obj->mutex), NULL);
    pthread_mutex_init(&(mm_obj->cfg_lock), NULL);
    pthread_cond_init(&(mm_obj->cond), NULL);

    /* Open the server device */
    do {
        n_try--;
        mm_obj->server_fd = open(mm_daemon_server_find_subdev(
                MSM_CONFIGURATION_NAME, QCAMERA_VNODE_GROUP_ID,
                MEDIA_ENT_T_DEVNODE_V4L), O_RDWR | O_NONBLOCK);
        if ((mm_obj->server_fd > 0) || (errno != EIO) || (n_try <= 0))
            break;
        usleep(10000);
    } while (n_try > 0);

    if (mm_obj->server_fd <= 0) {
        ALOGE("%s: Failed to open server device", __FUNCTION__);
        rc = -1;
        goto server_error;
    }

    rc = mm_daemon_subscribe(mm_obj, 1);
    if (rc < 0) {
        ALOGE("%s: event subscription error", __FUNCTION__);
        goto error;
    }

    rc = mm_daemon_poll_fn(mm_obj);
    if (rc < 0) {
        ALOGE("%s: poll error", __FUNCTION__);
        goto error;
    }

error:
    if (mm_obj->server_fd > 0) {
        close(mm_obj->server_fd);
        mm_obj->server_fd = 0;
    }
server_error:
    pthread_mutex_destroy(&(mm_obj->mutex));
    pthread_cond_destroy(&(mm_obj->cond));
    pthread_mutex_destroy(&sd_lock);
    pthread_mutex_destroy(&(mm_obj->cfg_lock));
    free(mm_obj);
mem_error:
    return rc;
}

int main()
{
    int rc;
    rc = mm_daemon_open();
    return rc;
}
