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
#ifndef MM_DAEMON_UTIL_H
#define MM_DAEMON_UTIL_H

#include "mm_daemon.h"

struct mm_daemon_thread_ops {
    void *(*start)(void *data);
    int (*init)(mm_daemon_thread_info *info);
    void (*stop)(mm_daemon_thread_info *info);
    void (*shutdown)(mm_daemon_thread_info *info);
    int (*cmd)(mm_daemon_thread_info *info, uint8_t cmd, uint32_t val);
};

mm_daemon_thread_info *mm_daemon_util_thread_open(mm_daemon_sd_info *sd,
        uint8_t cam_idx, int32_t cb_pfd);
int mm_daemon_util_thread_close(mm_daemon_thread_info *info);
int mm_daemon_util_set_thread_state(mm_daemon_thread_info *info,
        mm_daemon_thread_state state);
void mm_daemon_util_pipe_cmd(int32_t pfd, uint8_t cmd, int32_t val);
void mm_daemon_util_subdev_cmd(mm_daemon_thread_info *info, uint8_t cmd,
        int32_t val, uint8_t wait);
#endif /* MM_DAEMON_UTIL_H */
