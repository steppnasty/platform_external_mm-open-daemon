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

#include "mm_daemon_actuator.h"
#include "mm_daemon_util.h"

static int mm_daemon_act_move_focus(mm_daemon_act_t *mm_act, int32_t num_steps)
{
    int rc = 0;
    int8_t dir;
    int8_t sign_dir;
    int16_t dest_step_pos;
    uint32_t total_steps;
    struct msm_actuator_cfg_data cdata;
    struct damping_params_t damping_params;

    if (num_steps < 0) {
        dir = MOVE_FAR;
        num_steps *= -1;
    } else
        dir = MOVE_NEAR;

    total_steps = mm_act->params->act_info->af_tuning_params.total_steps;

    if (num_steps > (int32_t)total_steps)
        num_steps = total_steps;
    else if (num_steps == 0)
        return 0;

    if (dir == MOVE_NEAR)
        sign_dir = MSM_ACTUATOR_MOVE_SIGNED_NEAR;
    else
        sign_dir = MSM_ACTUATOR_MOVE_SIGNED_FAR;

    dest_step_pos = mm_act->curr_step_pos + (sign_dir * num_steps);
    if (dest_step_pos < 0)
        dest_step_pos = 0;
    else if (dest_step_pos > (int32_t)total_steps)
        dest_step_pos = total_steps;

    if (dest_step_pos == mm_act->curr_step_pos)
        return 0;

    mm_act->params->act_snsr_ops->get_damping_params(dest_step_pos,
            mm_act->curr_step_pos, num_steps, sign_dir, &damping_params);

    cdata.cfgtype = CFG_MOVE_FOCUS;
    cdata.cfg.move.dir = dir;
    cdata.cfg.move.sign_dir = sign_dir;
    cdata.cfg.move.dest_step_pos = dest_step_pos;
    cdata.cfg.move.num_steps = num_steps;
    cdata.cfg.move.ringing_params = &damping_params;

    rc = ioctl(mm_act->fd, VIDIOC_MSM_ACTUATOR_CFG, &cdata);
    if (rc == 0)
        mm_act->curr_step_pos = dest_step_pos;

    return rc;
}

static void mm_daemon_act_shutdown(mm_daemon_thread_info *info)
{
    mm_daemon_act_t *mm_act = (mm_daemon_act_t *)info->obj;

    if (mm_act) {
        if (mm_act->fd)
            close(mm_act->fd);
        free(info->obj);
        info->obj = NULL;
    }
}

static int mm_daemon_act_cmd(mm_daemon_thread_info *info)
{
    int rc = 0;
    ssize_t read_len;
    mm_daemon_act_t *mm_act = (mm_daemon_act_t *)info->obj;
    mm_daemon_pipe_evt_t pipe_cmd;

    read_len = read(info->pfds[0], &pipe_cmd, sizeof(pipe_cmd));

    switch (pipe_cmd.cmd) {
    case ACT_CMD_SHUTDOWN:
        rc = -1;
        break;
    case ACT_CMD_MOVE_FOCUS:
        rc = mm_daemon_act_move_focus(mm_act, pipe_cmd.val);
        break;
    case ACT_CMD_DEFAULT_FOCUS:
        rc = mm_daemon_act_move_focus(mm_act, mm_act->curr_step_pos * -1);
        break;
    default:
        break;
    }
    if (rc == 0)
        mm_daemon_util_pipe_cmd(info->cb_pfd, CFG_CMD_AF_ACT_POS,
                mm_act->curr_step_pos);

    return rc;
}

static int mm_daemon_act_init(mm_daemon_thread_info *info)
{
    mm_daemon_act_t *mm_act = NULL;
    struct msm_actuator_cfg_data cdata;

    if (!info->data)
        goto error;

    mm_act = (mm_daemon_act_t *)calloc(1, sizeof(mm_daemon_act_t));
    if (!mm_act)
        goto error;

    mm_act->params = (struct mm_daemon_act_params *)info->data;
    if (!mm_act->params->act_info || !mm_act->params->act_snsr_ops)
        goto param_error;

    mm_act->fd = open(info->devpath, O_RDWR | O_NONBLOCK);
    if (mm_act->fd > 0) {
        cdata.cfgtype = CFG_SET_ACTUATOR_INFO;
        memcpy(&cdata.cfg.set_info, mm_act->params->act_info,
                sizeof(struct msm_actuator_set_info_t));

        if (ioctl(mm_act->fd, VIDIOC_MSM_ACTUATOR_CFG, &cdata) == 0) {
            info->obj = (void *)mm_act;
            return 0;
        }
    }

    if (mm_act->fd)
        close(mm_act->fd);
param_error:
    if (mm_act)
        free(mm_act);
error:
    return -EINVAL;
}

static struct mm_daemon_thread_ops mm_daemon_act_thread_ops = {
    .init = mm_daemon_act_init,
    .shutdown = mm_daemon_act_shutdown,
    .cmd = mm_daemon_act_cmd,
};

void mm_daemon_act_load(mm_daemon_sd_info *sd)
{
    sd->ops = (void *)&mm_daemon_act_thread_ops;
}
