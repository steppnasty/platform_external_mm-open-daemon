/*
   Copyright (C) 2017 Brian Stepp 
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

#include "mm_daemon_csi.h"
#include "mm_daemon_util.h"

static int mm_daemon_csic_cfg(mm_daemon_csi_t *mm_csi)
{
    struct csic_cfg_data cdata;

    if (mm_csi->params) {
        cdata.cfgtype = CSIC_CFG;
        cdata.csic_params = mm_csi->params;
        if (mm_csi->params && ioctl(mm_csi->fd, VIDIOC_MSM_CSIC_IO_CFG,
                &cdata) == 0)
            return 0;
    }
    return -EINVAL;
}

static int mm_daemon_csic_set_params(mm_daemon_csi_t *mm_csi, uint32_t val)
{
    int rc = 0;

    mm_csi->params = (struct msm_camera_csic_params *)val;

    return rc;
}

static int mm_daemon_csic_shutdown(mm_daemon_csi_t *mm_csi)
{
    struct csic_cfg_data cdata;
    int rc = 0;

    if (mm_csi->fd) {
        cdata.cfgtype = CSIC_RELEASE;
        rc = ioctl(mm_csi->fd, VIDIOC_MSM_CSIC_IO_CFG, &cdata);
        close(mm_csi->fd);
    }
    mm_csi->state = CSI_POWER_OFF;

    return rc;
}

static int mm_daemon_csi_read_pipe(mm_daemon_thread_info *info,
        mm_daemon_csi_t *mm_csi)
{
    int rc = 0;
    ssize_t read_len;
    mm_daemon_pipe_evt_t pipe_cmd;

    read_len = read(info->pfds[0], &pipe_cmd, sizeof(pipe_cmd));

    switch (pipe_cmd.cmd) {
        case CSI_CMD_CFG:
            rc = mm_daemon_csic_cfg(mm_csi);
            break;
        case CSI_CMD_SET_PARAMS:
            rc = mm_daemon_csic_set_params(mm_csi, pipe_cmd.val);
            break;
        case CSI_CMD_SHUTDOWN:
            rc = -1;
            break;
        default:
            break;
    }

    return rc;
}

static mm_daemon_csi_t *mm_daemon_csic_init(mm_daemon_thread_info *info)
{
    mm_daemon_csi_t *mm_csi = NULL;
    struct csic_cfg_data cdata;

    mm_csi = (mm_daemon_csi_t *)calloc(1, sizeof(mm_daemon_csi_t));
    if (!mm_csi)
        return NULL;

    mm_csi->fd = open(info->devpath, O_RDWR | O_NONBLOCK);
    if (mm_csi->fd > 0) {
        cdata.cfgtype = CSIC_INIT;
        if (ioctl(mm_csi->fd, VIDIOC_MSM_CSIC_IO_CFG, &cdata) == 0) {
            mm_csi->state = CSI_POWER_ON;
            return mm_csi;
        }
    }

    if (mm_csi->fd)
        close(mm_csi->fd);
    if (mm_csi)
        free(mm_csi);
    return NULL;
}

static void *mm_daemon_csi_thread(void *data)
{
    mm_daemon_thread_info *info = (mm_daemon_thread_info *)data;
    mm_daemon_csi_t *mm_csi = NULL;
    struct pollfd pfd;

    mm_csi = mm_daemon_csic_init(info);
    if (!mm_csi) {
        mm_daemon_util_pipe_cmd(info->cb_pfd, CFG_CMD_CSI_ERR, info->type);
        goto error;
    }

    pfd.fd = info->pfds[0];
    do {
        pfd.events = POLLIN|POLLRDNORM;
        if (mm_daemon_util_set_thread_state(info, STATE_POLL) < 0)
            break;
        if (poll(&pfd, 1, -1) > 0) {
            if (mm_daemon_util_set_thread_state(info, STATE_BUSY) < 0)
                break;
            if ((pfd.revents & POLLIN) &&
                    (pfd.revents & POLLRDNORM)) {
                if ((mm_daemon_csi_read_pipe(info, mm_csi)) < 0) {
                    break;
                }
            } else
                usleep(1000);
        } else {
            usleep(100);
            continue;
        }
    } while (mm_daemon_util_set_thread_state(info, 0) == 0);
    mm_daemon_csic_shutdown(mm_csi);
error:
    if (mm_csi)
        free(mm_csi);
    mm_daemon_util_set_thread_state(info, STATE_STOPPED);
    return NULL;
}

static struct mm_daemon_thread_ops mm_daemon_csi_thread_ops = {
    .thread = mm_daemon_csi_thread,
};

void mm_daemon_csi_load(mm_daemon_sd_info *sd)
{
    sd->ops = (void *)&mm_daemon_csi_thread_ops;
}
