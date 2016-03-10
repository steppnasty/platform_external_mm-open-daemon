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

#include <media/msm_cam_sensor.h>
#include "mm_daemon_sensor.h"

#define MSB(word) (word & 0xFF00) >> 8
#define LSB(word) (word & 0x00FF)

static struct msm_camera_i2c_reg_array s5k4e1gx_start_settings[] = {
    {0x0100, 0x01, 0},
};

static struct msm_camera_i2c_reg_array s5k4e1gx_stop_settings[] = {
    {0x0100, 0x00, 0},
};

static struct msm_camera_i2c_reg_array s5k4e1gx_groupon_settings[] = {
    {0x0104, 0x01, 0},
};

static struct msm_camera_i2c_reg_array s5k4e1gx_groupoff_settings[] = {
    {0x0104, 0x00, 0},
};

static struct msm_camera_i2c_reg_array s5k4e1gx_prev_settings[] = {
    {0x034C, 0x05, 0},/* x_output size msb */
    {0x034D, 0x18, 0},/* x_output size lsb */
    {0x034E, 0x03, 0},/* y_output size msb */
    {0x034F, 0xD4, 0},/* y_output size lsb */
    {0x0381, 0x01, 0},/* x_even_inc */
    {0x0383, 0x01, 0},/* x_odd_inc */
    {0x0385, 0x01, 0},/* y_even_inc */
    {0x0387, 0x03, 0},/* y_odd_inc 03(10b AVG) */
    {0x30A9, 0x02, 0},/* Horizontal Binning On */
    {0x300E, 0xEB, 0},/* Vertical Binning On */
    {0x0105, 0x01, 0},/* mask corrupted frame */
    {0x0340, 0x03, 0},/* Frame Length */
    {0x0341, 0xE0, 0},
    {0x300F, 0x82, 0},/* CDS Test */
    {0x3013, 0xC0, 0},/* rst_offset1 */
    {0x3017, 0xA4, 0},/* rmb_init */
    {0x301B, 0x88, 0},/* comp_bias */
    {0x3110, 0x10, 0},/* PCLK inv */
    {0x3117, 0x0C, 0},/* PCLK delay */
    {0x3119, 0x0F, 0},/* V H Sync Strength */
    {0x311A, 0xFA, 0},/* Data PCLK Strength */
};

static struct msm_camera_i2c_reg_array s5k4e1gx_init_settings[] = {
    {0x302B, 0x00, 0},
    {0x30BC, 0xA0, 0},
    {0x30BE, 0x08, 0},
    {0x0305, 0x06, 0},
    {0x0306, 0x00, 0},
    {0x0307, 0x50, 0},
    {0x30B5, 0x00, 0},
    {0x30F1, 0xB0, 0},
    {0x0101, 0x00, 0},
    {0x034C, 0x05, 0},
    {0x034D, 0x18, 0},
    {0x034E, 0x03, 0},
    {0x034F, 0xD4, 0},
    {0x0381, 0x01, 0},
    {0x0383, 0x01, 0},
    {0x0385, 0x01, 0},
    {0x0387, 0x03, 0},
    {0x30A9, 0x02, 0},
    {0x300E, 0xEB, 0},
    {0x0340, 0x03, 0},
    {0x0341, 0xE0, 0},
    {0x0342, 0x0A, 0},
    {0x0343, 0xB2, 0},
    {0x3110, 0x10, 0},
    {0x3117, 0x0C, 0},
    {0x3119, 0x0F, 0},
    {0x311A, 0xFA, 0},
    {0x0104, 0x01, 0},
    {0x0204, 0x00, 0},
    {0x0205, 0x20, 0},
    {0x0202, 0x03, 0},
    {0x0203, 0x1F, 0},
    {0x0104, 0x00, 0},
    {0x3110, 0x10, 0},
};

static struct msm_camera_i2c_reg_array s5k4e1gx_snap_settings[] = {
    {0x0100, 0x00, 0},
    {0x034C, 0x0A, 0},
    {0x034D, 0x30, 0},
    {0x034E, 0x07, 0},
    {0x034F, 0xA8, 0},
    {0x0381, 0x01, 0},
    {0x0383, 0x01, 0},
    {0x0385, 0x01, 0},
    {0x0387, 0x01, 0},
    {0x30A9, 0x03, 0},
    {0x300E, 0xE8, 0},
    {0x0105, 0x01, 0},
    {0x0340, 0x07, 0},
    {0x0341, 0xB4, 0},
    {0x300F, 0x82, 0},
    {0x3013, 0xC0, 0},
    {0x3017, 0xA4, 0},
    {0x301B, 0x88, 0},
    {0x3110, 0x10, 0},
    {0x3117, 0x0C, 0},
    {0x3119, 0x0F, 0},
    {0x311A, 0xFA, 0},
};

static int mm_daemon_sensor_cmd(mm_daemon_sensor_t *mm_snsr,
        int cmd, void *setting)
{
    struct sensorb_cfg_data cdata;

    cdata.cfgtype = cmd;
    cdata.cfg.setting = setting;
    return ioctl(mm_snsr->cam_fd, VIDIOC_MSM_SENSOR_CFG, &cdata);
}

static int mm_daemon_sensor_stream_start(mm_daemon_sensor_t *mm_snsr)
{
    struct msm_camera_i2c_reg_setting setting;

    setting.reg_setting = &s5k4e1gx_start_settings[0];
    setting.size = ARRAY_SIZE(s5k4e1gx_start_settings);
    setting.data_type = MSM_CAMERA_I2C_BYTE_DATA;
    return mm_daemon_sensor_cmd(mm_snsr, CFG_WRITE_I2C_ARRAY,
            (void *)&setting);
}

static int mm_daemon_sensor_stream_stop(mm_daemon_sensor_t *mm_snsr)
{
    struct msm_camera_i2c_reg_setting setting;

    setting.reg_setting = &s5k4e1gx_stop_settings[0];
    setting.size = ARRAY_SIZE(s5k4e1gx_stop_settings);
    setting.data_type = MSM_CAMERA_I2C_BYTE_DATA;
    return mm_daemon_sensor_cmd(mm_snsr, CFG_WRITE_I2C_ARRAY,
            (void *)&setting);
}

static int mm_daemon_sensor_power_up(mm_daemon_sensor_t *mm_snsr)
{
    int rc = -1;
    struct msm_camera_i2c_reg_setting setting;

    if (mm_snsr->sensor_state == SENSOR_POWER_ON)
        goto done;
    if (mm_daemon_sensor_cmd(mm_snsr, CFG_POWER_UP, NULL) == 0) {
        setting.reg_setting = &s5k4e1gx_init_settings[0];
        setting.size = ARRAY_SIZE(s5k4e1gx_init_settings);
        setting.data_type = MSM_CAMERA_I2C_BYTE_DATA;
        rc = mm_daemon_sensor_cmd(mm_snsr, CFG_WRITE_I2C_ARRAY,
                (void *)&setting);
    }

    if (rc == 0)
        mm_snsr->sensor_state = SENSOR_POWER_ON;
done:
    return rc;
}

static int mm_daemon_sensor_start(mm_daemon_sensor_t *mm_snsr)
{
    int rc = 0;
    struct msm_camera_i2c_reg_setting setting;

    setting.reg_setting = &s5k4e1gx_stop_settings[0];
    setting.size = ARRAY_SIZE(s5k4e1gx_stop_settings);
    mm_snsr->cam_fd = open(mm_daemon_server_find_subdev(
            MSM_CONFIGURATION_NAME, MSM_CAMERA_SUBDEV_SENSOR,
            MEDIA_ENT_T_V4L2_SUBDEV), O_RDWR | O_NONBLOCK);
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
    rc = ioctl(mm_snsr->cam_fd, VIDIOC_MSM_SENSOR_RELEASE, NULL);
    mm_snsr->sensor_state = SENSOR_POWER_OFF;
    close(mm_snsr->cam_fd);
    return rc;
}

static void mm_daemon_sensor_exp_gain(mm_daemon_sensor_t *mm_snsr,
        uint32_t line)
{
    struct msm_camera_i2c_reg_setting setting;
    uint16_t gain, max_legal_gain = 0x0200;
    uint32_t ll_ratio;
    uint32_t fl_lines;
    uint32_t ll_pck = 2738;
    uint32_t offset;

    if (mm_snsr->curr_stream == DAEMON_STREAM_TYPE_PREVIEW)
        fl_lines = 992;
    else
        fl_lines = 1972;
    gain = mm_snsr->curr_gain;
    memset(&setting, 0, sizeof(setting));
    offset = 4;
    if (gain > max_legal_gain)
        gain = max_legal_gain;

    line = (line * 0x400);

    if (fl_lines < (line / 0x400))
        ll_ratio = (line / (fl_lines - offset));
    else
        ll_ratio = 0x400;
    ll_pck = ll_pck * ll_ratio;
    line = line / ll_ratio;

    {
        struct msm_camera_i2c_reg_array exp_settings[] = {
            s5k4e1gx_groupon_settings[0],
            {0x0204, MSB(gain), 0},
            {0x0205, LSB(gain), 0},
            {0x0342, MSB(ll_pck / 0x400), 0},
            {0x0343, LSB(ll_pck / 0x400), 0},
            {0x0202, MSB(line), 0},
            {0x0203, LSB(line), 0},
            s5k4e1gx_groupoff_settings[0],
        };
    
        setting.reg_setting = &exp_settings[0];
        setting.size = ARRAY_SIZE(exp_settings);
        setting.data_type = MSM_CAMERA_I2C_BYTE_DATA;
        mm_daemon_sensor_cmd(mm_snsr, CFG_WRITE_I2C_ARRAY,
            (void *)&setting);
    }
}

static int mm_daemon_sensor_preview(mm_daemon_sensor_t *mm_snsr)
{
    int rc = -1;
    struct msm_camera_i2c_reg_setting setting;

    mm_snsr->curr_stream = DAEMON_STREAM_TYPE_PREVIEW;
    mm_daemon_sensor_stream_stop(mm_snsr);
    setting.reg_setting = &s5k4e1gx_prev_settings[0];
    setting.size = ARRAY_SIZE(s5k4e1gx_prev_settings);
    setting.data_type = MSM_CAMERA_I2C_BYTE_DATA;
    rc = mm_daemon_sensor_cmd(mm_snsr, CFG_WRITE_I2C_ARRAY, (void *)&setting);
    mm_daemon_sensor_stream_start(mm_snsr);
    mm_daemon_sensor_exp_gain(mm_snsr, 0x31F);
    return rc;
}

static int mm_daemon_sensor_snapshot(mm_daemon_sensor_t *mm_snsr)
{
    int rc = -1;
    struct msm_camera_i2c_reg_setting setting;

    mm_snsr->curr_stream = DAEMON_STREAM_TYPE_SNAPSHOT;
    mm_daemon_sensor_stream_stop(mm_snsr);
    setting.reg_setting = &s5k4e1gx_snap_settings[0];
    setting.size = ARRAY_SIZE(s5k4e1gx_snap_settings);
    setting.data_type = MSM_CAMERA_I2C_BYTE_DATA;
    rc = mm_daemon_sensor_cmd(mm_snsr, CFG_WRITE_I2C_ARRAY, (void *)&setting);
    mm_daemon_sensor_stream_start(mm_snsr);
    mm_daemon_sensor_exp_gain(mm_snsr, 0x7A8);
    return rc;
}

static int mm_daemon_sensor_pipe_cmd(mm_daemon_sensor_t *mm_snsr)
{
    int rc = 0;
    ssize_t read_len;
    mm_daemon_pipe_evt_t pipe_cmd;

    read_len = read(mm_snsr->pfds[0], &pipe_cmd, sizeof(pipe_cmd));

    switch (pipe_cmd.cmd) {
        case SENSOR_CMD_PREVIEW:
            pthread_mutex_lock(&mm_snsr->lock);
            mm_daemon_sensor_preview(mm_snsr);
            pthread_cond_signal(&mm_snsr->cond);
            pthread_mutex_unlock(&mm_snsr->lock);
            break;
        case SENSOR_CMD_SNAPSHOT:
            pthread_mutex_lock(&mm_snsr->lock);
            mm_daemon_sensor_snapshot(mm_snsr);
            pthread_cond_signal(&mm_snsr->cond);
            pthread_mutex_unlock(&mm_snsr->lock);
            break;
        case SENSOR_CMD_GAIN_UPDATE:
            mm_snsr->curr_gain = pipe_cmd.val;
            break;
        case SENSOR_CMD_EXP_GAIN:
            mm_daemon_sensor_exp_gain(mm_snsr, pipe_cmd.val);
            break;
        case SENSOR_CMD_POWER_UP:
            if (mm_snsr->sensor_state == SENSOR_POWER_OFF)
                mm_daemon_sensor_power_up(mm_snsr);
            break;
        case SENSOR_CMD_SHUTDOWN:
            rc = -1;
            break;
        default:
            ALOGE("%s: Unknown cmd on pipe %d", __FUNCTION__, pipe_cmd.cmd);
    }
    return rc;
}

static void *mm_daemon_sensor_thread(void *data)
{
    struct pollfd pfd;
    mm_daemon_sensor_t *mm_snsr = (mm_daemon_sensor_t *)data;

    pipe(mm_snsr->pfds);

    if (mm_daemon_sensor_start(mm_snsr) < 0)
        goto thread_close;

    mm_snsr->state = STATE_POLL;   
    pfd.fd = mm_snsr->pfds[0];
    do {
        pfd.events = POLLIN|POLLRDNORM;
        if (poll(&pfd, 1, -1) > 0) {
            if ((pfd.revents & POLLIN) &&
                    (pfd.revents & POLLRDNORM)) {
                if ((mm_daemon_sensor_pipe_cmd(mm_snsr)) < 0) {
                    mm_snsr->state = STATE_STOPPED;
                    break;
                }
            } else
                usleep(1000);
        } else {
            usleep(100);
            continue;
        }
    } while (mm_snsr->state == STATE_POLL);
thread_close:
    mm_daemon_sensor_stop(mm_snsr);
    return NULL;
}

mm_daemon_sensor_t *mm_daemon_sensor_open()
{
    mm_daemon_sensor_t *mm_snsr = NULL;

    mm_snsr = (mm_daemon_sensor_t *)malloc(sizeof(mm_daemon_sensor_t));
    if (mm_snsr == NULL)
        return NULL;
    memset(mm_snsr, 0, sizeof(mm_daemon_sensor_t));

    mm_snsr->curr_gain = DEFAULT_EXP_GAIN;
    pthread_mutex_init(&(mm_snsr->lock), NULL);
    pthread_cond_init(&(mm_snsr->cond), NULL);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&mm_snsr->pid, &attr, mm_daemon_sensor_thread, (void *)mm_snsr);
    pthread_attr_destroy(&attr);

    return mm_snsr;
}

int mm_daemon_sensor_close(mm_daemon_sensor_t *mm_snsr)
{
    void *rc;

    pthread_join(mm_snsr->pid, &rc);
    pthread_mutex_destroy(&(mm_snsr->lock));
    pthread_cond_destroy(&(mm_snsr->cond));
    return (int)rc;
}

