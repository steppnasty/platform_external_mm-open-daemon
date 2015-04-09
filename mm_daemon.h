/*
   Copyright (C) 2014-2015 Brian Stepp 
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

#ifndef MM_DAEMON_H
#define MM_DAEMON_H

#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <linux/videodev2.h>
#include <linux/ion.h>
#include <linux/un.h>
#include <media/msm_camera.h>

#include <cutils/sockets.h>
#include <utils/Log.h>

#include <QCamera_Intf.h>

#define STATS_BUFFER_MAX 3
#define MM_DAEMON_STATS_AEC_LEN 512

struct mm_daemon_obj;

typedef struct {
    uint32_t operation_mode;
    uint32_t stats_comp;
    uint32_t hfr_mode;
    uint32_t cfg;
    uint32_t module_cfg;
    uint32_t realign_buf;
    uint32_t chroma_up;
    uint32_t stats_cfg;
} mm_daemon_vfe_op;

typedef enum {
    STATE_STOPPED,
    STATE_POLL,
    STATE_MAX
} mm_daemon_state_type_t;

typedef enum {
    SOCKET_STATE_DISCONNECTED,
    SOCKET_STATE_CONNECTED,
    SOCKET_STATE_MAX
} mm_daemon_socket_state_t;

typedef struct mm_daemon_socket {
    pthread_t pid;
    int sock_fd;
    struct mm_daemon_obj *daemon_obj;
    mm_daemon_socket_state_t state;
} mm_daemon_socket_t;

typedef struct {
    int fd;
    void *vaddr;
    struct ion_handle *handle;
    uint32_t len;
} mm_daemon_stats_buf_data;

typedef struct {
    int buf_cnt;
    int allocated;
    int stats_cnt;
    mm_daemon_stats_buf_data buf_data[STATS_BUFFER_MAX];
} mm_daemon_stats_buf_info;

typedef struct mm_daemon_obj {
    int32_t server_fd;
    int32_t cfg_fd;
    int32_t vfe_fd;
    uint32_t inst_handle;
    int ion_fd;
    mm_daemon_state_type_t state;
    mm_daemon_state_type_t cfg_state;
    mm_daemon_socket_t sock_obj;
    pthread_mutex_t mutex;
    pthread_t cfg_pid;
    int32_t cfg_pfds[2];
    int vpe_enabled;
    int exp_level;
    uint32_t curr_gain;
    mm_daemon_stats_buf_info stats_buf[MSM_STATS_TYPE_MAX];
    int sd_revision[14];
} mm_daemon_obj_t;

int mm_daemon_config_start(mm_daemon_obj_t *mm_obj);
int mm_daemon_config_stop(mm_daemon_obj_t *mm_obj);

extern int mm_daemon_config_exp_gain(mm_daemon_obj_t *mm_obj, uint16_t gain,
        uint32_t line);
#endif // MM_DAEMON_H
