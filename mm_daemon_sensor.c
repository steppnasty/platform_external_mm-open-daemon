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

#define LOG_TAG "mm-daemon-snsr"

#include "mm_daemon_sensor.h"

static int mm_daemon_sensor_empty_queue(mm_daemon_sensor_t *mm_snsr,
        uint8_t execute);

typedef struct cmd_queue {
    mm_daemon_sensor_cmd_t cmd;
    uint32_t val;
    struct cmd_queue *next;
} cmd_queue_t;
cmd_queue_t *head;

static int mm_daemon_sensor_queue_cmd(mm_daemon_sensor_cmd_t cmd,
        uint32_t val)
{
    cmd_queue_t *tmp_ptr;

    tmp_ptr = (cmd_queue_t *)malloc(sizeof(cmd_queue_t));
    if (tmp_ptr == NULL)
        return -1;
    tmp_ptr->next = NULL;
    tmp_ptr->cmd = val;
    tmp_ptr->cmd = cmd;
    tmp_ptr->next = head->next;
    head->next = tmp_ptr;

    return 0;
}

static mm_daemon_sensor_cmd_t mm_daemon_sensor_dequeue_cmd(uint32_t *val)
{
    mm_daemon_sensor_cmd_t cmd = 0;
    cmd_queue_t *tmp_ptr, *temp = NULL;

    tmp_ptr = head;
    while(tmp_ptr->next != NULL) {
        temp = tmp_ptr;
        tmp_ptr = tmp_ptr->next;
    }
    if (temp) {
        *val = tmp_ptr->val;
        cmd = tmp_ptr->cmd;
        free(tmp_ptr);
        temp->next = NULL;
    }
    return cmd;
}

static int mm_daemon_sensor_cmd(mm_daemon_sensor_t *mm_snsr,
        int cmd, void *setting)
{
    struct sensorb_cfg_data cdata;

    cdata.cfgtype = cmd;
    cdata.cfg.setting = setting;
    return ioctl(mm_snsr->cam_fd, VIDIOC_MSM_SENSOR_CFG, &cdata);
}

static int mm_daemon_sensor_start(mm_daemon_sensor_t *mm_snsr)
{
    int rc = 0;
    struct mm_sensor_regs *regs;
    struct msm_camera_i2c_reg_setting setting;

    regs = mm_snsr->cfg->stop_regs;
    setting.reg_setting = regs->regs;
    setting.size = regs->size;
    setting.data_type = regs->data_type;
    mm_snsr->cam_fd = open(mm_snsr->info->devpath, O_RDWR | O_NONBLOCK);
    if (mm_snsr->cam_fd > 0)
            rc = mm_daemon_sensor_cmd(mm_snsr,
                    CFG_SET_STOP_STREAM_SETTING, (void *)&setting);
    return rc;
}

static int mm_daemon_sensor_stop(mm_daemon_sensor_t *mm_snsr)
{
    int rc = -1;

    if (mm_snsr->sensor_state == SENSOR_POWER_OFF ||
            mm_daemon_sensor_cmd(mm_snsr, CFG_POWER_DOWN, NULL) < 0)
        return rc;
    if (mm_snsr->cfg->ops->deinit)
        mm_snsr->cfg->ops->deinit(mm_snsr->cfg);
    rc = ioctl(mm_snsr->cam_fd, VIDIOC_MSM_SENSOR_RELEASE, NULL);
    mm_snsr->sensor_state = SENSOR_POWER_OFF;
    close(mm_snsr->cam_fd);
    return rc;
}

static int mm_daemon_sensor_power_up(mm_daemon_sensor_t *mm_snsr)
{
    int rc = -1;

    if (mm_snsr->sensor_state == SENSOR_POWER_ON)
        goto done;
    if (mm_daemon_sensor_cmd(mm_snsr, CFG_POWER_UP, NULL) == 0)
        rc = mm_snsr->cfg->ops->init(mm_snsr->cfg);

    if (rc == 0) {
        mm_snsr->sensor_state = SENSOR_POWER_ON;
        rc = mm_daemon_sensor_empty_queue(mm_snsr, 1);
    }
done:
    return rc;
}

static int mm_daemon_sensor_i2c_read(void *snsr,
        uint16_t reg_addr, uint16_t *data,
        enum msm_camera_i2c_data_type data_type)
{
    mm_daemon_sensor_t *mm_snsr = (mm_daemon_sensor_t *)snsr;
    struct msm_camera_i2c_read_config read_config = {
        .reg_addr = reg_addr,
        .data = data,
        .data_type = data_type,
    };
        
    return mm_daemon_sensor_cmd(mm_snsr, CFG_SLAVE_READ_I2C, (void *)&read_config);
}

static int mm_daemon_sensor_i2c_write(void *snsr,
        uint16_t reg_addr, uint16_t reg_data,
        enum msm_camera_i2c_data_type data_type)
{
    mm_daemon_sensor_t *mm_snsr = (mm_daemon_sensor_t *)snsr;
    struct msm_camera_i2c_reg_array i2c_write_array = {
        .reg_addr = reg_addr,
        .reg_data = reg_data,
    };
    struct msm_camera_i2c_reg_setting setting = {
        .reg_setting = &i2c_write_array,
        .size = 1,
        .data_type = data_type,
    };

    return mm_daemon_sensor_cmd(mm_snsr, CFG_WRITE_I2C_ARRAY, (void *)&setting);
}

static int mm_daemon_sensor_i2c_write_array(void *snsr,
        struct msm_camera_i2c_reg_array *reg_setting, uint16_t size,
        enum msm_camera_i2c_data_type data_type)
{
    mm_daemon_sensor_t *mm_snsr = (mm_daemon_sensor_t *)snsr;
    struct msm_camera_i2c_reg_setting setting = {
        .reg_setting = reg_setting,
        .size = size,
        .data_type = data_type,
    };
    return mm_daemon_sensor_cmd(mm_snsr, CFG_WRITE_I2C_ARRAY, (void *)&setting);
}

static int mm_daemon_sensor_execute_cmd(mm_daemon_sensor_t *mm_snsr,
        mm_daemon_sensor_cmd_t cmd, uint32_t val)
{
    int rc = 0;

    switch (cmd) {
        case SENSOR_CMD_PREVIEW:
            pthread_mutex_lock(&mm_snsr->info->lock);
            if ((rc = mm_snsr->cfg->ops->prev(mm_snsr->cfg)) < 0)
                ALOGE("%s: Error while setting preview mode", __FUNCTION__);
            pthread_cond_signal(&mm_snsr->info->cond);
            pthread_mutex_unlock(&mm_snsr->info->lock);
            break;
        case SENSOR_CMD_SNAPSHOT:
            pthread_mutex_lock(&mm_snsr->info->lock);
            if ((rc = mm_snsr->cfg->ops->snap(mm_snsr->cfg)) < 0)
                ALOGE("%s: Error while setting snapshot mode", __FUNCTION__);
            pthread_cond_signal(&mm_snsr->info->cond);
            pthread_mutex_unlock(&mm_snsr->info->lock);
            break;
        case SENSOR_CMD_GAIN_UPDATE:
            mm_snsr->cfg->curr_gain = val;
            break;
        case SENSOR_CMD_EXP_GAIN:
            if (mm_snsr->cfg->ops->exp_gain)
                mm_snsr->cfg->ops->exp_gain(mm_snsr->cfg, val);
            break;
        case SENSOR_CMD_AB:
            if (mm_snsr->cfg->ops->ab)
                mm_snsr->cfg->ops->ab(mm_snsr->cfg, val);
            break;
        case SENSOR_CMD_WB:
            if (mm_snsr->cfg->ops->wb)
                if (mm_snsr->cfg->ops->wb(mm_snsr->cfg, val) < 0)
                    ALOGW("sensor does not support wb setting %d", val);
            break;
        case SENSOR_CMD_BRIGHTNESS:
            if (mm_snsr->cfg->ops->brightness)
                mm_snsr->cfg->ops->brightness(mm_snsr->cfg, val);
            break;
        case SENSOR_CMD_SATURATION:
            if (mm_snsr->cfg->ops->saturation)
                mm_snsr->cfg->ops->saturation(mm_snsr->cfg, val);
            break;
        case SENSOR_CMD_CONTRAST:
            if (mm_snsr->cfg->ops->contrast)
                mm_snsr->cfg->ops->contrast(mm_snsr->cfg, val);
            break;
        case SENSOR_CMD_EFFECT:
            if (mm_snsr->cfg->ops->effect)
                mm_snsr->cfg->ops->effect(mm_snsr->cfg, val);
            break;
        case SENSOR_CMD_SHARPNESS:
            if (mm_snsr->cfg->ops->sharpness)
                mm_snsr->cfg->ops->sharpness(mm_snsr->cfg, val);
            break;
        case SENSOR_CMD_POWER_UP:
            if (mm_snsr->sensor_state == SENSOR_POWER_OFF)
                mm_daemon_sensor_power_up(mm_snsr);
            break;
        case SENSOR_CMD_SHUTDOWN:
            rc = -1;
            break;
        default:
            ALOGE("%s: Unknown cmd %d", __FUNCTION__, cmd);
    }
    return rc;
}

static int mm_daemon_sensor_read_pipe(mm_daemon_sensor_t *mm_snsr)
{
    ssize_t read_len;
    mm_daemon_pipe_evt_t pipe_cmd;

    read_len = read(mm_snsr->info->pfds[0], &pipe_cmd, sizeof(pipe_cmd));

    if (pipe_cmd.cmd == SENSOR_CMD_POWER_UP ||
            pipe_cmd.cmd == SENSOR_CMD_SHUTDOWN)
        goto execute;

    if (mm_snsr->sensor_state == SENSOR_POWER_OFF) {
        if (pipe_cmd.cmd == SENSOR_CMD_PREVIEW ||
                pipe_cmd.cmd == SENSOR_CMD_SNAPSHOT) {
            ALOGE("%s: Can't start stream before powerup", __FUNCTION__);
            return -1;
        }
        return mm_daemon_sensor_queue_cmd(pipe_cmd.cmd, pipe_cmd.val);
    }

execute:
    return mm_daemon_sensor_execute_cmd(mm_snsr, pipe_cmd.cmd, pipe_cmd.val);
}

static int mm_daemon_sensor_empty_queue(mm_daemon_sensor_t *mm_snsr,
        uint8_t execute)
{
    mm_daemon_sensor_cmd_t cmd;
    uint32_t val = 0;
    int rc = 0;

    do {
        cmd = mm_daemon_sensor_dequeue_cmd(&val);
        if (execute && cmd)
            rc = mm_daemon_sensor_execute_cmd(mm_snsr, cmd, val);
    } while(cmd && rc >= 0);
    return rc;
}

static int mm_daemon_sensor_set_ops(mm_daemon_sensor_t *mm_snsr)
{
    mm_snsr->cfg = (mm_sensor_cfg_t *)mm_snsr->info->data;
    if (!mm_snsr->cfg ||
            !mm_snsr->cfg->ops ||
            !mm_snsr->cfg->ops->init ||
            !mm_snsr->cfg->ops->prev ||
            !mm_snsr->cfg->ops->snap ||
            !mm_snsr->cfg->stop_regs ||
            !mm_snsr->cfg->data)
        goto error;
    if (!mm_snsr->cfg->ops->i2c_read)
        mm_snsr->cfg->ops->i2c_read = &mm_daemon_sensor_i2c_read;
    if (!mm_snsr->cfg->ops->i2c_write)
        mm_snsr->cfg->ops->i2c_write = &mm_daemon_sensor_i2c_write;
    if (!mm_snsr->cfg->ops->i2c_write_array)
        mm_snsr->cfg->ops->i2c_write_array = &mm_daemon_sensor_i2c_write_array;
    mm_snsr->cfg->mm_snsr = (void *)mm_snsr;
    return 0;
error:
    ALOGE("Error loading sensor settings");
    return -1;
}

static int mm_daemon_sensor_prepare(mm_daemon_sensor_t *mm_snsr)
{
    pipe(mm_snsr->info->pfds);

    if (mm_daemon_sensor_set_ops(mm_snsr) < 0)
        return -1;

    head = (cmd_queue_t *)calloc(1, sizeof(cmd_queue_t));
    if (!head)
        return -1;

    if (mm_daemon_sensor_start(mm_snsr) < 0) {
        mm_daemon_sensor_stop(mm_snsr);
        return -1;
    }
    return 0;
}

static void *mm_daemon_sensor_thread(void *data)
{
    struct pollfd pfd;
    mm_daemon_sensor_t *mm_snsr = (mm_daemon_sensor_t *)data;

    if (mm_daemon_sensor_prepare(mm_snsr) < 0)
        goto error;

    pfd.fd = mm_snsr->info->pfds[0];
    do {
        pfd.events = POLLIN|POLLRDNORM;
        mm_snsr->info->state = STATE_POLL;
        if (poll(&pfd, 1, -1) > 0) {
            mm_snsr->info->state = STATE_BUSY;
            if ((pfd.revents & POLLIN) &&
                    (pfd.revents & POLLRDNORM)) {
                if ((mm_daemon_sensor_read_pipe(mm_snsr)) < 0) {
                    mm_snsr->info->state = STATE_STOPPED;
                    break;
                }
            } else
                usleep(1000);
        } else {
            usleep(100);
            continue;
        }
    } while (mm_snsr->info->state != STATE_STOPPED);
    mm_daemon_sensor_stop(mm_snsr);
    mm_daemon_sensor_empty_queue(mm_snsr, 0);
error:
    if (head)
        free(head);
    if (mm_snsr)
        free(mm_snsr);
    return NULL;
}

int mm_daemon_sensor_open(mm_daemon_thread_info *info)
{
    mm_daemon_sensor_t *mm_snsr = NULL;

    mm_snsr = (mm_daemon_sensor_t *)malloc(sizeof(mm_daemon_sensor_t));
    if (mm_snsr == NULL)
        return -1;
    memset(mm_snsr, 0, sizeof(mm_daemon_sensor_t));

    mm_snsr->info = info;

    pthread_mutex_init(&(info->lock), NULL);
    pthread_cond_init(&(info->cond), NULL);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&info->pid, &attr, mm_daemon_sensor_thread, (void *)mm_snsr);
    pthread_attr_destroy(&attr);

    info->mm_snsr = (void *)mm_snsr;

    return 0;
}

int mm_daemon_sensor_close(mm_daemon_thread_info *info)
{
    void *rc;

    pthread_join(info->pid, &rc);
    pthread_mutex_destroy(&(info->lock));
    pthread_cond_destroy(&(info->cond));
    return (int)rc;
}

