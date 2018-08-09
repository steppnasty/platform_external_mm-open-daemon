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

#define LOG_TAG "mm-daemon-snsr"

#include "mm_daemon_sensor.h"
#include "mm_daemon_util.h"

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
    int rc = -1;
    struct mm_sensor_regs *regs;
    struct msm_camera_i2c_reg_setting setting;

    regs = mm_snsr->cfg->stop_regs;
    setting.reg_setting = regs->regs;
    setting.size = regs->size;
    setting.data_type = regs->data_type;
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
    mm_snsr->cfg->ops->deinit(mm_snsr->cfg);
    rc = ioctl(mm_snsr->cam_fd, VIDIOC_MSM_SENSOR_RELEASE, NULL);
    mm_snsr->sensor_state = SENSOR_POWER_OFF;
    return rc;
}

static int mm_daemon_sensor_power_up(mm_daemon_sensor_t *mm_snsr)
{
    int rc = -1;

    if (mm_snsr->sensor_state == SENSOR_POWER_ON)
        goto done;
    if (mm_daemon_sensor_cmd(mm_snsr, CFG_POWER_UP, NULL) == 0)
        rc = mm_snsr->cfg->ops->init_regs(mm_snsr->cfg);

    if (rc == 0) {
        mm_snsr->sensor_state = SENSOR_POWER_ON;
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

static int mm_daemon_sensor_read_cmd(mm_daemon_thread_info *info, uint8_t cmd,
        uint32_t val)
{
    mm_daemon_sensor_t *mm_snsr = (mm_daemon_sensor_t *)info->obj;
    int rc = 0;

    switch (cmd) {
    case SENSOR_CMD_PREVIEW:
        if ((rc = mm_snsr->cfg->ops->prev(mm_snsr->cfg)) < 0)
            ALOGE("%s: Error while setting preview mode", __FUNCTION__);
        break;
    case SENSOR_CMD_VIDEO:
        if (mm_snsr->cfg->ops->video) {
            if ((rc = mm_snsr->cfg->ops->video(mm_snsr->cfg)) < 0)
                ALOGE("%s: Error while setting video mode", __FUNCTION__);
        }
        break;
    case SENSOR_CMD_SNAPSHOT:
        if ((rc = mm_snsr->cfg->ops->snap(mm_snsr->cfg)) < 0)
            ALOGE("%s: Error while setting snapshot mode", __FUNCTION__);
        break;
    case SENSOR_CMD_GAIN_UPDATE:
        mm_snsr->cfg->curr_gain = val;
        break;
    case SENSOR_CMD_EXP_GAIN:
        if (mm_snsr->cfg->ops->exp_gain)
            rc = mm_snsr->cfg->ops->exp_gain(mm_snsr->cfg,
                    (val & 0xFFFF), (val >> 16));
        break;
    case SENSOR_CMD_AB:
        if (mm_snsr->cfg->ops->ab)
            mm_snsr->cfg->ops->ab(mm_snsr->cfg, val);
        break;
    case SENSOR_CMD_WB:
        if (mm_snsr->cfg->ops->wb)
            mm_snsr->cfg->ops->wb(mm_snsr->cfg, val);
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

static void mm_daemon_sensor_shutdown(mm_daemon_thread_info *info)
{
    mm_daemon_sensor_t *mm_snsr = (mm_daemon_sensor_t *)info->obj;

    if (mm_snsr) {
        mm_daemon_sensor_stop(mm_snsr);
        if (mm_snsr->cam_fd) {
            close(mm_snsr->cam_fd);
            mm_snsr->cam_fd = 0;
        }
        free(info->obj);
        info->obj = NULL;
    }
}

static int mm_daemon_sensor_set_ops(mm_daemon_thread_info *info,
        mm_daemon_sensor_t *mm_snsr)
{
    mm_snsr->cfg = (mm_sensor_cfg_t *)info->data;
    if (!mm_snsr->cfg ||
            !mm_snsr->cfg->ops ||
            !mm_snsr->cfg->ops->init_regs ||
            !mm_snsr->cfg->ops->init_data ||
            !mm_snsr->cfg->ops->deinit ||
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
    mm_snsr->cfg->ops->init_data(mm_snsr->cfg);
    return 0;
error:
    ALOGE("Error loading sensor ops");
    return -1;
}

static int mm_daemon_sensor_init(mm_daemon_thread_info *info)
{
    mm_daemon_sensor_t *mm_snsr = NULL;

    mm_snsr = (mm_daemon_sensor_t *)calloc(1, sizeof(mm_daemon_sensor_t));
    if (!mm_snsr)
        return -EINVAL;

    mm_snsr->cam_fd = open(info->devpath, O_RDWR | O_NONBLOCK);
    if (mm_snsr->cam_fd < 0)
        goto cam_error;

    if (mm_daemon_sensor_set_ops(info, mm_snsr) < 0)
        goto init_error;

    if (mm_daemon_sensor_start(mm_snsr) < 0) {
        mm_daemon_sensor_stop(mm_snsr);
        goto init_error;
    }
    info->obj = (void *)mm_snsr;
    return 0;

init_error:
    if (mm_snsr->cam_fd) {
        close(mm_snsr->cam_fd);
        mm_snsr->cam_fd = 0;
    }
cam_error:
    if (mm_snsr)
        free(mm_snsr);
    return -EINVAL;
}

static struct mm_daemon_thread_ops mm_daemon_snsr_thread_ops = {
    .init = mm_daemon_sensor_init,
    .shutdown = mm_daemon_sensor_shutdown,
    .cmd = mm_daemon_sensor_read_cmd,
};

void mm_daemon_snsr_load(mm_daemon_sd_info *sd, mm_daemon_sd_info *camif,
        mm_daemon_sd_info *act)
{
    mm_sensor_cfg_t *cfg = NULL;
    struct sensorb_cfg_data cdata;
    char path[PATH_MAX];
    int fd;

    fd = open(sd->devpath, O_RDWR | O_NONBLOCK);
    if (fd < 0)
        return;

    memset(&cdata, 0, sizeof(cdata));
    cdata.cfgtype = CFG_GET_SENSOR_INFO;
    if (ioctl(fd, VIDIOC_MSM_SENSOR_CFG, &cdata) < 0) {
        close(fd);
        return;
    }
    close(fd);

    snprintf(path, PATH_MAX, "/system/lib/libmmdaemon_%s.so",
            cdata.cfg.sensor_info.sensor_name);
    sd->handle = dlopen(path, RTLD_NOW);
    if (sd->handle == NULL) {
        char const *err_str = dlerror();
        ALOGE("Error loading %s: %s", path, err_str?err_str:"unknown");
        return;
    }
    ALOGI("Successfully loaded %s sensor", cdata.cfg.sensor_info.sensor_name);
    cfg = (mm_sensor_cfg_t *)dlsym(sd->handle, "sensor_cfg_obj");
    if (!cfg) {
        dlclose(sd->handle);
        return;
    }
    sd->data = (void *)cfg;
    sd->ops = (void *)&mm_daemon_snsr_thread_ops;
    camif->data = cfg->data->csi_params;
    act->data = cfg->data->act_params;
}
