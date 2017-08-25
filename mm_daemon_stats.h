/*
   Copyright (C) 2014-2017 Brian Stepp 
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

#ifndef MM_DAEMON_STATS_H
#define MM_DAEMON_STATS_H

#include "mm_daemon.h"

#define DAEMON_STATS_BUF_CNT 4

static inline int32_t mm_daemon_stats_size(uint32_t stats_type)
{
    int32_t len;

    switch (stats_type) {
        case MSM_ISP_STATS_AEC:
            len = MAX_AE_STATS_DATA_SIZE;
            break;
        case MSM_ISP_STATS_AWB:
            len = MAX_AWB_STATS_DATA_SIZE;
            break;
        case MSM_ISP_STATS_AF:
            len = MAX_AF_STATS_DATA_SIZE;
            break;
        default:
            len = 0;
            break;
    }
    return len;
}

typedef enum {
    STATS_CMD_PROC,
    STATS_CMD_SHUTDOWN,
} mm_daemon_stats_cmd_t;

typedef struct {
    uint8_t ready;
    int32_t val;
    int32_t len;
    void *buf;
} mm_daemon_stats_done_work;

typedef struct mm_daemon_stats {
    pthread_t pid;
    int32_t pfds[2];
    void *work_buf;
    enum msm_isp_stats_type type;
    mm_daemon_stats_done_work done_work;
    mm_daemon_stats_buf_info *stats_buf;
    mm_daemon_thread_state state;
} mm_daemon_stats_t;

int mm_daemon_stats_open(mm_daemon_cfg_t *cfg_obj, uint32_t stats_type);
int mm_daemon_stats_close(mm_daemon_cfg_t *cfg_obj, uint32_t stats_type);

#endif //MM_DAEMON_STATS_H
