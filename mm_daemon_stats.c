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

#include "mm_daemon_stats.h"

static int mm_daemon_stats_proc_awb(mm_daemon_stats_t *mm_stats)
{
    int i;
    uint32_t stat = 0;
    uint8_t work_buf[MAX_AWB_STATS_DATA_SIZE];

    if (!mm_stats->done_work.ready) {
        memset(mm_stats->done_work.buf, 0, MAX_AWB_STATS_DATA_SIZE);
        memcpy(&work_buf[0], mm_stats->work_buf, MAX_AWB_STATS_DATA_SIZE);
        for (i = 0; i < MAX_AWB_STATS_DATA_SIZE; i++)
            stat += work_buf[i];
        mm_stats->done_work.val = (stat / i);
        mm_stats->done_work.len = i;
        memcpy(mm_stats->done_work.buf, &work_buf[0], i);
        mm_stats->done_work.ready = 1;
    }
    return 0;
}

static int mm_daemon_stats_proc_aec(mm_daemon_stats_t *mm_stats,
        uint32_t curr_gain)
{
    int i;
    uint32_t stat = 0;
    int32_t gain, val;
    uint16_t target_ae = 80;
    uint16_t last_valid_stat = 0;
    int8_t adj = 0;
    uint8_t work_buf[MAX_AE_STATS_DATA_SIZE];

    if (!mm_stats->done_work.ready) {
        memset(mm_stats->done_work.buf, 0, MAX_AE_STATS_DATA_SIZE);
        memcpy(&work_buf[0], mm_stats->work_buf, MAX_AE_STATS_DATA_SIZE);
        for (i = 0; i < MAX_AE_STATS_DATA_SIZE; i++) {
            if (work_buf[i])
                last_valid_stat = i + 1;
            stat += work_buf[i];
        }
        if (last_valid_stat)
            val = (stat / last_valid_stat);
        else
            val = (stat / i);

        adj = (target_ae - val);
        if (adj > 0) {
            if (adj < 10)
                adj = 0;
            else if (adj < 20)
                adj = 1;
            else if (adj < 30)
                adj = 5;
            else
                adj = 10;
        } else {
            if (adj > -10)
                adj = 0;
            else if (adj > -20)
                adj = -1;
            else if (adj > -30)
                adj = -5;
            else
                adj = -10;
        }
        gain = curr_gain + adj;
        if (gain > 512)
            gain = 512;
        if (gain > 0 && gain <= 512)
            mm_stats->done_work.val = gain;
        else
            mm_stats->done_work.val = curr_gain;
        
        mm_stats->done_work.len = last_valid_stat;
        memcpy(mm_stats->done_work.buf, &work_buf[0], i);
        mm_stats->done_work.ready = 1;
    }
    return 0;
}

static int mm_daemon_stats_proc(mm_daemon_stats_t *mm_stats, uint32_t val)
{
    int rc = 0;

    switch (mm_stats->type) {
        case MSM_ISP_STATS_AEC:
            rc = mm_daemon_stats_proc_aec(mm_stats, val);
            break;
        case MSM_ISP_STATS_AWB:
            rc = mm_daemon_stats_proc_awb(mm_stats);
            break;
        default:
            break;
    }

    return rc;
}

static int mm_daemon_stats_pipe_cmd(mm_daemon_stats_t *mm_stats)
{
    int rc = 0;
    ssize_t read_len;
    mm_daemon_pipe_evt_t pipe_cmd;

    read_len = read(mm_stats->pfds[0], &pipe_cmd, sizeof(pipe_cmd));

    switch (pipe_cmd.cmd) {
        case STATS_CMD_PROC:
            rc = mm_daemon_stats_proc(mm_stats, pipe_cmd.val);
            break;
        case STATS_CMD_SHUTDOWN:
            rc = -1;
            break;
        default:
            break;
    }

    return rc;
}

static void *mm_daemon_stat_thread(void *data)
{
    struct pollfd pfd;
    mm_daemon_stats_t *mm_stats = (mm_daemon_stats_t *)data;

    mm_stats->work_buf = calloc(1, mm_daemon_stats_size(mm_stats->type));
    mm_stats->done_work.buf = calloc(1, mm_daemon_stats_size(mm_stats->type));

    pipe(mm_stats->pfds);
    pfd.fd = mm_stats->pfds[0];
    do {
        pfd.events = POLLIN|POLLRDNORM;
        mm_stats->state = STATE_POLL;
        if (poll(&pfd, 1, -1) > 0) {
            mm_stats->state = STATE_BUSY;
            if ((pfd.revents & POLLIN) &&
                    (pfd.revents & POLLRDNORM)) {
                if ((mm_daemon_stats_pipe_cmd(mm_stats)) < 0) {
                    mm_stats->state = STATE_STOPPED;
                    break;
                }
            } else
                usleep(1000);
        } else {
            usleep(100);
            continue;
        }
    } while (mm_stats->state != STATE_STOPPED);

    if (mm_stats->work_buf)
        free(mm_stats->work_buf);
    if (mm_stats->done_work.buf)
        free(mm_stats->done_work.buf);
    return NULL;
}

int mm_daemon_stats_open(mm_daemon_cfg_t *cfg_obj, uint32_t stats_type)
{
    mm_daemon_stats_t *mm_stats = NULL;
    mm_daemon_stats_buf_info *stat;

    cfg_obj->stats_buf[stats_type] = (mm_daemon_stats_buf_info *)
            calloc(1, sizeof(mm_daemon_stats_buf_info));
    stat = cfg_obj->stats_buf[stats_type];

    if (!stat)
        return -1;
    pthread_mutex_init(&(stat->stat_lock), NULL);
    mm_stats = (mm_daemon_stats_t *)calloc(1, sizeof(mm_daemon_stats_t));
    if (mm_stats == NULL)
        return -1;

    stat->mm_stats = (void *)mm_stats;

    mm_stats->stats_buf = stat;
    mm_stats->type = stats_type;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&mm_stats->pid, &attr, mm_daemon_stat_thread, (void *)mm_stats);
    pthread_attr_destroy(&attr);

    return 0;
}

int mm_daemon_stats_close(mm_daemon_cfg_t *cfg_obj, uint32_t stats_type)
{
    int ret = -1;
    void *rc;
    uint32_t type;
    mm_daemon_stats_buf_info *stat = NULL;
    mm_daemon_stats_t *mm_stats = NULL;

    stat = cfg_obj->stats_buf[stats_type];
    if (!stat)
        goto stats_close;
    mm_stats = (mm_daemon_stats_t *)stat->mm_stats;
    if (!mm_stats)
        goto stat_free;

    pthread_join(mm_stats->pid, &rc);
    ret = (int)rc;
    type = mm_stats->type;
    free(mm_stats);
    stat->mm_stats = NULL;

stat_free:
    pthread_mutex_destroy(&(stat->stat_lock));
    free(cfg_obj->stats_buf[stats_type]);
    cfg_obj->stats_buf[stats_type] = NULL;
stats_close:
    return ret;
}
