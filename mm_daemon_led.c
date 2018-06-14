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

#include "mm_daemon_led.h"
#include "mm_daemon_util.h"

static int mm_daemon_led_control(mm_daemon_led_t *mm_led,
        enum msm_camera_led_config_t cfgtype)
{
    struct msm_camera_led_cfg_t cdata;

    switch (cfgtype) {
    case MSM_CAMERA_LED_OFF:
    case MSM_CAMERA_LED_INIT:
        mm_led->state = LED_OFF;
        break;
    case MSM_CAMERA_LED_RELEASE:
        mm_led->state = LED_INIT;
        break;
    case MSM_CAMERA_LED_LOW:
    case MSM_CAMERA_LED_HIGH:
    case MSM_CAMERA_LED_TORCH:
        mm_led->state = LED_ON;
        break;
    default:
        return 0;
    }

    cdata.cfgtype = cfgtype;
    return ioctl(mm_led->fd, VIDIOC_MSM_FLASH_LED_DATA_CFG, &cdata);
}

static void mm_daemon_led_shutdown(mm_daemon_thread_info *info)
{
    mm_daemon_led_t *mm_led = (mm_daemon_led_t *)info->obj;

    if (mm_led) {
        if (mm_led->fd) {
            mm_daemon_led_control(mm_led, MSM_CAMERA_LED_RELEASE);
            close(mm_led->fd);
            mm_led->fd = 0;
        }
        free(info->obj);
        info->obj = NULL;
    }
}

static int mm_daemon_led_cmd(mm_daemon_thread_info *info, uint8_t cmd,
        uint32_t val)
{
    int rc = 0;
    mm_daemon_led_t *mm_led = (mm_daemon_led_t *)info->obj;

    switch (cmd) {
    case LED_CMD_CONTROL:
        mm_daemon_led_control(mm_led, val);
        break;
    case LED_CMD_SHUTDOWN:
        rc = -1;
    default:
        break;
    }

    return rc;
}

static int mm_daemon_led_init(mm_daemon_thread_info *info)
{
    mm_daemon_led_t *mm_led = NULL;

    mm_led = (mm_daemon_led_t *)calloc(1, sizeof(mm_daemon_led_t));
    if (!mm_led)
        return -EINVAL;

    mm_led->fd = open(info->devpath, O_RDWR | O_NONBLOCK);
    if (mm_led->fd > 0) {
        if (mm_daemon_led_control(mm_led, MSM_CAMERA_LED_INIT) == 0) {
            info->obj = (void *)mm_led;
            return 0;
        }
    }

    if (mm_led->fd) {
        close(mm_led->fd);
        mm_led->fd = 0;
    }
    if (mm_led)
        free(mm_led);

    return -EINVAL;
}

static struct mm_daemon_thread_ops mm_daemon_led_thread_ops = {
    .init = mm_daemon_led_init,
    .shutdown = mm_daemon_led_shutdown,
    .cmd = mm_daemon_led_cmd,
};

void mm_daemon_led_load(mm_daemon_sd_info *sd)
{
    sd->ops = (void *)&mm_daemon_led_thread_ops;
}
