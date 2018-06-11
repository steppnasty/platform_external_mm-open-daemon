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

#ifndef MM_DAEMON_H
#define MM_DAEMON_H

#include <dlfcn.h>
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
#include <media/msmb_generic_buf_mgr.h>
#include <media/msmb_isp.h>
#include <media/msm_cam_sensor.h>

#include <cutils/sockets.h>
#include <utils/Log.h>

#include "common.h"

#define STATS_BUFFER_MAX 4
#define MM_DAEMON_STATS_AEC_LEN 512

struct mm_daemon_obj;
struct mm_daemon_cfg;

typedef enum {
    STATE_INIT = 1,
    STATE_POLL,
    STATE_LOCKED,
    STATE_BUSY,
    STATE_STOPPED,
    STATE_MAX
} mm_daemon_thread_state;

typedef enum {
    MM_CSIPHY = 1,
    MM_CSID,
    MM_CSIC,
    MM_CCI,
    MM_ISPIF,
    MM_VFE,
    MM_AXI,
    MM_VPE,
    MM_SNSR,
    MM_ACT,
    MM_EEPROM,
    MM_CPP,
    MM_LED,
    MM_STROBE,
    MM_BUF,
    MM_CFG,
    MM_SOCK,
} mm_daemon_subdev_type;

struct mm_dameon_thread_ops;
typedef struct {
    char *devpath;
    int32_t pfds[2];
    int32_t cb_pfd;
    void *data;
    void *obj;
    uint8_t sid;
    uint8_t type;
    pthread_t pid;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    mm_daemon_thread_state state;
    struct mm_daemon_thread_ops *ops;
} mm_daemon_thread_info;

enum mm_daemon_thread_type {
    SOCK_DEV,
    SNSR_DEV,
    CSI_DEV,
    VFE_DEV,
    LED_DEV,
    ACT_DEV,
    BUF_DEV,
    MAX_DEV,
};

typedef struct {
    char devpath[32];
    void *handle;
    void *data;
    void *ops;
    uint8_t type;
    uint8_t found;
} mm_daemon_sd_info;

typedef struct {
    uint8_t num_cameras;
    uint8_t num_sensors;
    uint8_t num_csi;
    mm_daemon_sd_info camera_sd[MSM_MAX_CAMERA_SENSORS];
    mm_daemon_sd_info sensor_sd[MSM_MAX_CAMERA_SENSORS];
    mm_daemon_sd_info csi[MSM_MAX_CAMERA_SENSORS];
    mm_daemon_sd_info act[MSM_MAX_CAMERA_SENSORS];
    mm_daemon_sd_info msm_sd;
    mm_daemon_sd_info vfe_sd;
    mm_daemon_sd_info led;
    mm_daemon_sd_info buf;
    struct mm_daemon_obj *mm_obj;
} mm_daemon_sd_obj_t;

typedef struct {
    int fd;
    uint8_t mapped;
    void *vaddr;
    struct ion_handle *handle;
    uint32_t len;
} mm_daemon_buf_data;

struct mm_daemon_stat_ops {
    int (*start)(struct mm_daemon_cfg *cfg_obj);
    void (*proc)(struct mm_daemon_cfg *cfg_obj, uint32_t buf_idx);
};

typedef struct {
    int buf_cnt;
    int stats_cnt;
    unsigned int stream_id;
    uint32_t stream_handle;
    uint32_t bufq_handle;
    void *mm_stats;
    mm_daemon_buf_data buf_data[CAM_MAX_NUM_BUFS_PER_STREAM];
    struct mm_daemon_stat_ops ops;
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

typedef struct {
    int fd;
    parm_buffer_t *cfg_buf;
    parm_buffer_t *buf;
    uint8_t mapped;
} mm_daemon_parm_buf_info;

enum mm_daemon_act_focus_state {
    MM_FOCUS_INIT,
    MM_FOCUS_SCANNING,
    MM_FOCUS_SCANNING_DONE,
};

struct mm_daemon_af_metadata {
    cam_autofocus_state_t state;
    uint8_t is_focus_valid;
};

struct mm_daemon_af_info {
    struct mm_daemon_af_metadata meta;
    enum mm_daemon_act_focus_state state;
    uint16_t focus_stat_val;
    uint8_t focus_step_pos;
    uint8_t curr_step_pos;
    uint8_t frm_cnt;
    uint8_t incr_step;
};

struct mm_daemon_ae_metadata {
    uint8_t is_ae_params_valid;
    uint8_t is_prep_snapshot_done_valid;
};

struct mm_daemon_ae_info {
    struct mm_daemon_ae_metadata meta;
    int32_t curr_gain;
    int32_t curr_line;
    uint8_t flash_needed;
    uint8_t stable;
    uint8_t frm_cnt;
};

struct mm_daemon_wb_info {
    cam_wb_mode_type curr_wb;
};

typedef struct mm_daemon_cfg {
    mm_daemon_buf_info *stream_buf[MAX_NUM_STREAM];
    mm_daemon_stats_buf_info *stats_buf[MSM_ISP_STATS_MAX];
    mm_daemon_thread_info *cfg;
    mm_daemon_thread_info *info[MAX_DEV];
    mm_daemon_parm_buf_info parm_buf;
    mm_daemon_cap_buf_info cap_buf;
    struct mm_daemon_af_info af;
    struct mm_daemon_ae_info ae;
    struct mm_daemon_wb_info wb;
    struct mm_sensor_data *sdata;
    int32_t vfe_fd;
    int32_t ion_fd;
    int32_t buf_fd;
    uint32_t current_streams;
    uint16_t enabled_stats;
    uint16_t stat_frames;
    uint8_t num_stats_buf;
    uint8_t session_id;
    uint8_t prep_snapshot;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} mm_daemon_cfg_t;

typedef struct mm_daemon_obj {
    int32_t server_fd;
    mm_daemon_thread_info *cfg;
    mm_daemon_thread_state state;
    int32_t svr_pfds[2];
    unsigned int session_id;
    unsigned int stream_id;
    uint8_t cfg_shutdown;
    uint8_t cap_buf_mapped;
} mm_daemon_obj_t;

typedef enum {
    CFG_CMD_STREAM_START = 1,
    CFG_CMD_STREAM_STOP,
    CFG_CMD_NEW_STREAM,
    CFG_CMD_DEL_STREAM,
    CFG_CMD_PARM,
    CFG_CMD_DO_AUTO_FOCUS,
    CFG_CMD_CANCEL_AUTO_FOCUS,
    CFG_CMD_PREPARE_SNAPSHOT,
    CFG_CMD_VFE_REG,
    CFG_CMD_SHUTDOWN,

    CFG_CMD_MAP_UNMAP_DONE,
    CFG_CMD_SK_PKT_MAP,
    CFG_CMD_SK_PKT_UNMAP,

    CFG_CMD_AF_ACT_POS,

    CFG_CMD_ERR,
} mm_daemon_cfg_cmd_t;

typedef enum {
    SERVER_CMD_MAP_UNMAP_DONE = 1,
    SERVER_CMD_CAP_BUF_MAP,
} mm_daemon_server_cmd_t;

typedef struct {
    uint8_t wait;
    uint8_t cmd;
    uint32_t val;
} mm_daemon_pipe_evt_t;

void mm_daemon_sock_load(mm_daemon_sd_info *sd);
void mm_daemon_csi_load(mm_daemon_sd_info *sd);
void mm_daemon_led_load(mm_daemon_sd_info *sd);
void mm_daemon_act_load(mm_daemon_sd_info *sd);
void mm_daemon_snsr_load(mm_daemon_sd_info *sd, mm_daemon_sd_info *camif,
        mm_daemon_sd_info *act);
mm_daemon_thread_info *mm_daemon_config_open(mm_daemon_sd_obj_t *sd,
        uint8_t session_id, int32_t cb_pfd);
int mm_daemon_config_close(mm_daemon_thread_info *info);
#endif // MM_DAEMON_H
