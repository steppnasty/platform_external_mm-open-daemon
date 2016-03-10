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
#include <linux/media.h>
#include <media/msmb_camera.h>
#include <media/msmb_isp.h>

#include <cutils/sockets.h>
#include <utils/Log.h>

#include <cam_intf.h>

#define STATS_BUFFER_MAX 3
#define MM_DAEMON_STATS_AEC_LEN 512
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(*a))

#define DAEMON_STREAM_TYPE_PREVIEW	0x01
#define DAEMON_STREAM_TYPE_POSTVIEW	0x02
#define DAEMON_STREAM_TYPE_SNAPSHOT	0x04
#define DAEMON_STREAM_TYPE_VIDEO	0x08
#define DAEMON_STREAM_TYPE_METADATA	0x10
#define DAEMON_STREAM_TYPE_RAW		0x20
#define DAEMON_STREAM_TYPE_OFFLINE_PROC	0x40

struct mm_daemon_obj;

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

typedef struct {
    int fd;
    uint8_t mapped;
    void *vaddr;
    struct ion_handle *handle;
    uint32_t len;
} mm_daemon_buf_data;

typedef struct {
    int buf_cnt;
    int allocated;
    int stats_cnt;
    mm_daemon_buf_data buf_data[STATS_BUFFER_MAX];
} mm_daemon_stats_buf_info;

typedef struct {
    int fd;
    uint8_t mapped;
    void *vaddr;
} mm_daemon_cap_buf_info;

typedef struct {
    int fd;
    int stream_id;
    uint32_t bufq_handle;
    uint32_t stream_handle[MSM_ISP_STATS_MAX];
    uint32_t output_format;
    uint8_t stream_info_mapped;
    uint8_t num_stream_bufs;
    uint8_t streamon;
    cam_stream_info_t *stream_info;
    mm_daemon_buf_data buf_data[CAM_MAX_NUM_BUFS_PER_STREAM];
} mm_daemon_buf_info;

typedef struct mm_daemon_cfg {
    int32_t vfe_fd;
    int32_t ion_fd;
    mm_daemon_buf_info *stream_buf[MAX_NUM_STREAM];
    mm_daemon_stats_buf_info stats_buf[MSM_ISP_STATS_MAX];
    uint32_t curr_gain;
    uint8_t num_stats_buf;
    uint8_t session_id;
    uint8_t num_streams;
    uint8_t first_frame_rcvd;
    uint8_t exp_level;
    uint8_t stat_frame;
    uint8_t current_streams;
    uint8_t gain_changed;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} mm_daemon_cfg_t;

typedef struct mm_daemon_socket {
    pthread_t pid;
    int sock_fd;
    struct mm_daemon_obj *daemon_obj;
    mm_daemon_cfg_t *cfg_obj;
    struct sockaddr_un sock_addr;
    mm_daemon_socket_state_t state;
    int dev_id;
} mm_daemon_socket_t;

typedef struct {
    int fd;
    parm_buffer_t *buf;
    uint8_t mapped;
} mm_daemon_parm_buf_info;

typedef struct mm_daemon_obj {
    int32_t server_fd;
    mm_daemon_state_type_t state;
    mm_daemon_state_type_t cfg_state;
    pthread_mutex_t mutex;
    pthread_mutex_t lock;
    pthread_mutex_t cfg_lock;
    pthread_cond_t cond;
    pthread_t cfg_pid;
    int32_t cfg_pfds[2];
    int32_t svr_pfds[2];
    mm_daemon_cap_buf_info cap_buf;
    mm_daemon_parm_buf_info parm_buf;
    unsigned int session_id;
    unsigned int stream_id;
    uint8_t cfg_shutdown;
} mm_daemon_obj_t;

typedef enum {
    CFG_CMD_STREAM_START = 1,
    CFG_CMD_STREAM_STOP,
    CFG_CMD_MAP_UNMAP_DONE,
    CFG_CMD_NEW_STREAM,
    CFG_CMD_DEL_STREAM,
    CFG_CMD_SOF_NOTIFY,
    CFG_CMD_SHUTDOWN,
} mm_daemon_cfg_cmd_t;

typedef enum {
    SERVER_CMD_MAP_UNMAP_DONE = 1,
} mm_daemon_server_cmd_t;

typedef struct {
    uint8_t cmd;
    uint32_t val;
} mm_daemon_pipe_evt_t;

int mm_daemon_config_open(mm_daemon_obj_t *mm_obj);
int mm_daemon_config_close(mm_daemon_obj_t *mm_obj);
char *mm_daemon_server_find_subdev(char *dev_name,
        uint32_t group_id, uint32_t sd_type);
#endif // MM_DAEMON_H
