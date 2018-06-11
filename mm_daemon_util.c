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

#include "mm_daemon_util.h"

static void *mm_daemon_util_thread_poll_start(void *data)
{
    mm_daemon_thread_info *info = (mm_daemon_thread_info *)data;
    mm_daemon_pipe_evt_t pipe_cmd;
    struct pollfd pfd;
    ssize_t read_len;
    uint8_t wait;
    int ret = 0;

    if (info->ops->init(info) < 0) {
        mm_daemon_util_pipe_cmd(info->cb_pfd, CFG_CMD_ERR, info->type);
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
            if ((pfd.revents & POLLIN) && (pfd.revents & POLLRDNORM)) {
                read_len = read(info->pfds[0], &pipe_cmd, sizeof(pipe_cmd));
                wait = pipe_cmd.wait;
                if (wait)
                    pthread_mutex_lock(&info->lock);
                ret = info->ops->cmd(info, pipe_cmd.cmd, pipe_cmd.val);
                if (wait) {
                    pthread_cond_signal(&info->cond);
                    pthread_mutex_unlock(&info->lock);
                }
                if (ret < 0)
                    break;
            } else
                usleep(1000);
        } else {
            usleep(100);
            continue;
        }
    } while (mm_daemon_util_set_thread_state(info, 0) == 0);
    if (info->ops->shutdown)
        info->ops->shutdown(info);
error:
    mm_daemon_util_set_thread_state(info, STATE_STOPPED);
    return NULL;
}

static void mm_daemon_util_thread_poll_stop(mm_daemon_thread_info *info)
{
    if (info->state == STATE_LOCKED) {
        pthread_mutex_lock(&info->lock);
        pthread_cond_signal(&info->cond);
        pthread_mutex_unlock(&info->lock);
    }
    mm_daemon_util_pipe_cmd(info->pfds[1], 0, 0);
}

mm_daemon_thread_info *mm_daemon_util_thread_open(mm_daemon_sd_info *sd,
        uint8_t cam_idx, int32_t cb_pfd)
{
    mm_daemon_thread_info *info = NULL;
    struct mm_daemon_thread_ops *ops = (struct mm_daemon_thread_ops *)sd->ops;

    if (!ops)
        return NULL;
    info = (mm_daemon_thread_info *)calloc(1, sizeof(mm_daemon_thread_info));
    if (info == NULL)
        return NULL;
    if (pipe(info->pfds) < 0) {
        free(info);
        return NULL;
    }
    if (ops->start == NULL) {
        if (ops->init == NULL || ops->shutdown == NULL) {
            free(info);
            return NULL;
        }
        ops->start = mm_daemon_util_thread_poll_start;
    }
    if (ops->stop == NULL)
        ops->stop = mm_daemon_util_thread_poll_stop;
    mm_daemon_util_set_thread_state(info, STATE_INIT);
    info->devpath = sd->devpath;
    info->sid = cam_idx;
    info->type = sd->type;
    info->cb_pfd = cb_pfd;
    info->data = sd->data;
    info->ops = ops;
    pthread_mutex_init(&(info->lock), NULL);
    pthread_cond_init(&(info->cond), NULL);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&info->pid, &attr, ops->start, (void *)info);
    pthread_attr_destroy(&attr);
    return info;
}

int mm_daemon_util_thread_close(mm_daemon_thread_info *info)
{
    void *rc;

    if ((info->state == STATE_POLL) && info->ops->stop)
        info->ops->stop(info);
    pthread_join(info->pid, &rc);
    pthread_mutex_destroy(&(info->lock));
    pthread_cond_destroy(&(info->cond));
    free(info);
    return (int)rc;
}

/*===========================================================================
 * FUNCTION   : mm_daemon_util_set_thread_state
 *
 * DESCRIPTION: Ensure state is not stopped before setting new state
 *
 * PARAMETERS :
 *   @info:  ptr to thread info obj
 *   @state: new thread state or 0 if only checking state
 *
 * RETURN     : 0 if successfully set
 *              -1 if stopped.
 *==========================================================================*/
int mm_daemon_util_set_thread_state(mm_daemon_thread_info *info,
        mm_daemon_thread_state state)
{
    if (info->state == STATE_STOPPED)
        return -1;
    if (state)
        info->state = state;
    return 0;
}

/*==========================================================================
 * FUNCTION   : mm_daemon_util_pipe_cmd
 *
 * DESCRIPTION: Sends pipe command to a polling thread
 *
 * PARAMETERS :
 *   @pfd: file descriptor for receiving poll thread
 *   @cmd: command
 *   @val: extra data value
 *==========================================================================*/
void mm_daemon_util_pipe_cmd(int32_t pfd, uint8_t cmd, int32_t val)
{
    mm_daemon_pipe_evt_t pipe_cmd;

    memset(&pipe_cmd, 0, sizeof(pipe_cmd));
    pipe_cmd.cmd = cmd;
    pipe_cmd.val = val;
    write(pfd, &pipe_cmd, sizeof(pipe_cmd));
}

/*==========================================================================
 * FUNCTION   : mm_daemon_util_subdev_cmd
 *
 * DESCRIPTION: Sends pipe command to a subdevice thread
 *
 * PARAMETERS :
 *   @info: pointer to thread info object
 *   @cmd:  subdev command
 *   @val:  extra data value
 *   @wait: set to 1 to wait for polling thread to unlock before continuing
 *==========================================================================*/
void mm_daemon_util_subdev_cmd(mm_daemon_thread_info *info, uint8_t cmd,
        int32_t val, uint8_t wait)
{
    mm_daemon_pipe_evt_t pipe_cmd;

    if (!info)
        return;

    pipe_cmd.wait = wait;
    pipe_cmd.cmd = cmd;
    pipe_cmd.val = val;
    write(info->pfds[1], &pipe_cmd, sizeof(pipe_cmd));
    if (wait) {
        pthread_mutex_lock(&info->lock);
        pthread_cond_wait(&info->cond, &info->lock);
        pthread_mutex_unlock(&info->lock);
    }
}
