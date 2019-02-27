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

//#define LOG_NDEBUG 0
#define LOG_TAG "mm-daemon"

#include <sys/types.h>
#include <cutils/properties.h>
#include "mm_daemon.h"

#define MM_CONFIG_NAME MSM_CONFIGURATION_NAME
#define MM_CAMERA_NAME MSM_CAMERA_NAME

static char subdev_name[32];

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
    len = write(mm_obj->cfg->pfds[1], &pipe_cmd, sizeof(pipe_cmd));
    if (len < 1)
        ALOGI("%s: write error", __FUNCTION__);
}

static void mm_daemon_server_find_subdev(mm_daemon_sd_obj_t *sd)
{
    unsigned int i;
    struct media_device_info dev_info;

    struct daemon_subdevs {
        char *dev_name;
        uint32_t group_id;
        uint32_t sd_type;
    } subdevs[] = {
        {MM_CAMERA_NAME, QCAMERA_VNODE_GROUP_ID, MEDIA_ENT_T_DEVNODE_V4L},
        {MM_CONFIG_NAME, QCAMERA_VNODE_GROUP_ID, MEDIA_ENT_T_DEVNODE_V4L},
        {MM_CONFIG_NAME, MSM_CAMERA_SUBDEV_VFE, MEDIA_ENT_T_V4L2_SUBDEV},
        {MM_CONFIG_NAME, MSM_CAMERA_SUBDEV_SENSOR, MEDIA_ENT_T_V4L2_SUBDEV},
        {MM_CONFIG_NAME, MSM_CAMERA_SUBDEV_BUF_MNGR, MEDIA_ENT_T_V4L2_SUBDEV},
        {MM_CONFIG_NAME, MSM_CAMERA_SUBDEV_CSIC, MEDIA_ENT_T_V4L2_SUBDEV},
        {MM_CONFIG_NAME, MSM_CAMERA_SUBDEV_LED_FLASH, MEDIA_ENT_T_V4L2_SUBDEV},
        {MM_CONFIG_NAME, MSM_CAMERA_SUBDEV_ACTUATOR, MEDIA_ENT_T_V4L2_SUBDEV},
    };

    for (i = 0; i < ARRAY_SIZE(subdevs); i++) {
        int dev_fd = 0;
        int media_dev_num = 0;
        char *dev_name = subdevs[i].dev_name;
        uint32_t group_id = subdevs[i].group_id;
        uint32_t sd_type = subdevs[i].sd_type;

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
                    snprintf(subdev_name, sizeof(subdev_name), "/dev/%s",
                            entity.name);
                    switch (group_id) {
                    case QCAMERA_VNODE_GROUP_ID:
                        if (strncmp(dev_info.model, MM_CAMERA_NAME,
                                sizeof(dev_info.model)) == 0) {
                            if (sd->num_cameras < MSM_MAX_CAMERA_SENSORS) {
                                sd->camera_sd[sd->num_cameras].type = MM_SOCK;
                                snprintf(sd->camera_sd[sd->num_cameras].devpath,
                                        sizeof(subdev_name), "%s", subdev_name);
                                mm_daemon_sock_load(
                                        &sd->camera_sd[sd->num_cameras]);
                                sd->num_cameras++;
                            }
                        } else if (strncmp(dev_info.model, MM_CONFIG_NAME,
                                sizeof(dev_info.model)) == 0) {
                            sd->msm_sd.type = MM_CFG;
                            snprintf(sd->msm_sd.devpath,
                                    sizeof(subdev_name), "%s", subdev_name);
                        }
                        break;
                    case MSM_CAMERA_SUBDEV_VFE:
                        sd->vfe_sd.found = 1;
                        sd->vfe_sd.type = MM_VFE;
                        snprintf(sd->vfe_sd.devpath, sizeof(subdev_name),
                                "%s", subdev_name);
                        break;
                    case MSM_CAMERA_SUBDEV_SENSOR:
                        if (sd->num_sensors < MSM_MAX_CAMERA_SENSORS) {
                            sd->sensor_sd[sd->num_sensors].found = 1;
                            sd->sensor_sd[sd->num_sensors].type = MM_SNSR;
                            snprintf(sd->sensor_sd[sd->num_sensors].devpath,
                                    sizeof(subdev_name), "%s", subdev_name);
                            mm_daemon_snsr_load(
                                 &sd->sensor_sd[sd->num_sensors],
                                 &sd->csi[sd->num_sensors],
                                 &sd->act[sd->num_sensors]);
                            sd->num_sensors++;
                        }
                        break;
                    case MSM_CAMERA_SUBDEV_BUF_MNGR:
                        sd->buf.found = 1;
                        sd->buf.type = MM_BUF;
                        snprintf(sd->buf.devpath, sizeof(subdev_name),
                                "%s", subdev_name);
                        break;
                    case MSM_CAMERA_SUBDEV_CSIPHY:
                        if (sd->num_csi < MSM_MAX_CAMERA_SENSORS) {
                            sd->csi[sd->num_csi].found = 1;
                            sd->csi[sd->num_csi].type = MM_CSIPHY;
                            snprintf(sd->csi[sd->num_csi].devpath,
                                    sizeof(subdev_name), "%s", subdev_name);
                            mm_daemon_csi_load(&sd->csi[sd->num_csi]);
                            sd->num_csi++;
                        }
                        break;
                    case MSM_CAMERA_SUBDEV_CSID:
                        if (sd->num_csi < MSM_MAX_CAMERA_SENSORS) {
                            sd->csi[sd->num_csi].found = 1;
                            sd->csi[sd->num_csi].type = MM_CSID;
                            snprintf(sd->csi[sd->num_csi].devpath,
                                    sizeof(subdev_name), "%s", subdev_name);
                            mm_daemon_csi_load(&sd->csi[sd->num_csi]);
                            sd->num_csi++;
                        }
                        break;
                    case MSM_CAMERA_SUBDEV_CSIC:
                        if (sd->num_csi < MSM_MAX_CAMERA_SENSORS) {
                            sd->csi[sd->num_csi].found = 1;
                            sd->csi[sd->num_csi].type = MM_CSIC;
                            snprintf(sd->csi[sd->num_csi].devpath,
                                    sizeof(subdev_name), "%s", subdev_name);
                            mm_daemon_csi_load(&sd->csi[sd->num_csi]);
                            sd->num_csi++;
                        }
                        break;
                    case MSM_CAMERA_SUBDEV_LED_FLASH:
                        sd->led.found = 1;
                        sd->led.type = MM_LED;
                        snprintf(sd->led.devpath, sizeof(subdev_name),
                                "%s", subdev_name);
                        mm_daemon_led_load(&sd->led);
                        break;
                    case MSM_CAMERA_SUBDEV_ACTUATOR: {
                        uint32_t subdev_id;
                        int act_fd = open(subdev_name, O_RDWR | O_NONBLOCK);

                        if (act_fd < 0)
                            break;

                        if (ioctl(act_fd, VIDIOC_MSM_SENSOR_GET_SUBDEV_ID,
                                &subdev_id) < 0) {
                            close(act_fd);
                            break;
                        }
                        close(act_fd);
                        sd->act[subdev_id].found = 1;
                        sd->act[subdev_id].type = MM_ACT;
                        snprintf(sd->act[subdev_id].devpath, sizeof(subdev_name),
                                "%s", subdev_name);
                        mm_daemon_act_load(&sd->act[subdev_id]);
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
        }
        if (dev_fd)
            close(dev_fd);
        dev_fd = 0;
    }
}

static void mm_daemon_notify(mm_daemon_sd_obj_t *sd)
{
    struct v4l2_event ev;
    struct v4l2_event new_ev;
    struct msm_v4l2_event_data *msm_evt = NULL;
    unsigned int status = MSM_CAMERA_ERR_CMD_FAIL;
    mm_daemon_obj_t *mm_obj = sd->mm_obj;

    memset(&ev, 0, sizeof(ev));
    memset(&new_ev, 0, sizeof(new_ev));
    if (ioctl(mm_obj->server_fd, VIDIOC_DQEVENT, &ev) < 0) {
        ALOGE("%s: Error dequeueing event", __FUNCTION__);
        goto cmd_ack;
    }
    msm_evt = (struct msm_v4l2_event_data *)ev.u.data;

    if (ev.id != MSM_CAMERA_NEW_SESSION && mm_obj->cfg == NULL) {
        ALOGE("ERROR: Unable to handle command without active config thread");
        if (mm_obj->cfg_shutdown)
            goto cmd_ack;
        else {
            mm_obj->cfg_shutdown = 1;
            mm_daemon_pack_event(mm_obj, &new_ev, CAM_EVENT_TYPE_DAEMON_DIED,
                    MSM_CAMERA_PRIV_SHUTDOWN, msm_evt->stream_id,
                    MSM_CAMERA_STATUS_SUCCESS);
            ioctl(mm_obj->server_fd, MSM_CAM_V4L2_IOCTL_NOTIFY, &new_ev);
            return;
        }
    }
    ALOGV("%s: event id=%d", __FUNCTION__, __LINE__, ev.id);
    switch (ev.id) {
    case MSM_CAMERA_NEW_SESSION:
        if (mm_obj->cfg)
            break;
        mm_obj->session_id = msm_evt->session_id;
        mm_obj->stream_id = msm_evt->stream_id;
        mm_obj->cfg = mm_daemon_config_open(sd, msm_evt->session_id,
                mm_obj->svr_pfds[1]);
        status = MSM_CAMERA_CMD_SUCESS;
        break;
    case MSM_CAMERA_DEL_SESSION:
        mm_daemon_server_config_cmd(mm_obj, CFG_CMD_SHUTDOWN,
                msm_evt->stream_id);
        mm_daemon_config_close(mm_obj->cfg);
        mm_obj->cfg = NULL;
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
            if (msm_evt->stream_id > 0) {
                pthread_mutex_lock(&mm_obj->cfg->lock);
                mm_daemon_server_config_cmd(mm_obj, CFG_CMD_NEW_STREAM,
                        msm_evt->stream_id);
                pthread_cond_wait(&mm_obj->cfg->cond, &mm_obj->cfg->lock);
                pthread_mutex_unlock(&mm_obj->cfg->lock);
            }
            break;
        case MSM_CAMERA_PRIV_DEL_STREAM:
            if (!msm_evt->stream_id)
                break;
            mm_daemon_server_config_cmd(mm_obj, CFG_CMD_DEL_STREAM,
                    msm_evt->stream_id);
            break;
        case CAM_PRIV_PARM:
            mm_daemon_server_config_cmd(mm_obj, CFG_CMD_PARM,
                    msm_evt->stream_id);
            break;
        case CAM_PRIV_DO_AUTO_FOCUS:
            mm_daemon_server_config_cmd(mm_obj, CFG_CMD_DO_AUTO_FOCUS,
                    msm_evt->stream_id);
            break;
        case CAM_PRIV_CANCEL_AUTO_FOCUS:
            mm_daemon_server_config_cmd(mm_obj, CFG_CMD_CANCEL_AUTO_FOCUS,
                    msm_evt->stream_id);
            break;
        case CAM_PRIV_PREPARE_SNAPSHOT:
            mm_daemon_server_config_cmd(mm_obj, CFG_CMD_PREPARE_SNAPSHOT,
                    msm_evt->stream_id);
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
            if (mm_obj->cap_buf_mapped)
                status = MSM_CAMERA_CMD_SUCESS;
            else
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

static int mm_daemon_server_pipe_cmd(mm_daemon_obj_t *mm_obj)
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
    case SERVER_CMD_CAP_BUF_MAP:
        mm_obj->cap_buf_mapped = pipe_cmd.val;
        break;
    default:
        ALOGI("%s: Unknown command on pipe", __FUNCTION__);
        break;
    }
    return rc;
}

static int mm_daemon_poll_fn(mm_daemon_sd_obj_t *sd)
{
    int rc = 0, i;
    struct pollfd fds[2];
    mm_daemon_obj_t *mm_obj = sd->mm_obj;

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
                mm_daemon_notify(sd);
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

static int mm_daemon_open()
{
    int rc = 0;
    int n_try = 2;
    mm_daemon_obj_t *mm_obj;
    mm_daemon_sd_obj_t *sd;

    mm_obj = (mm_daemon_obj_t *)calloc(1, sizeof(mm_daemon_obj_t));
    if (mm_obj == NULL) {
        rc = -1;
        goto mem_error;
    }

    sd = (mm_daemon_sd_obj_t *)calloc(1, sizeof(mm_daemon_sd_obj_t));
    if (sd == NULL) {
        rc = -1;
        goto tdata_error;
    }

    sd->mm_obj = mm_obj;

    mm_daemon_server_find_subdev(sd);

    /* Open the server device */
    do {
        n_try--;
        mm_obj->server_fd = open(sd->msm_sd.devpath, O_RDWR | O_NONBLOCK);
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

    rc = mm_daemon_poll_fn(sd);
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
    free(sd);
tdata_error:
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
