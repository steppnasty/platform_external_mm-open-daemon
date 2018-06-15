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

#ifndef MM_DAEMON_COMMON_H
#define MM_DAEMON_COMMON_H

#include <cam_intf.h>

#define BIT(nr) (1UL << (nr))
#define SB(type) BIT(CAM_STREAM_TYPE_##type)
#define ARRAY_SIZE(a)     (sizeof(a) / sizeof(*a))
#define ROUND(a)((a >= 0) ? (long)(a + 0.5) : (long)(a - 0.5))

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

enum mm_sensor_stream_type {
    PREVIEW,
    VIDEO,
    SNAPSHOT,
    STREAM_TYPE_MAX,
};

/* per-stream settings */
struct mm_sensor_stream_attr {
    uint32_t ro_cfg;
    uint16_t h;
    uint16_t w;
    uint16_t blk_l;
    uint16_t blk_p;
};

struct mm_sensor_aec_config {
    uint16_t gain_min;
    uint16_t gain_max;
    uint16_t line_min;
    uint16_t line_max;
    uint16_t default_gain;
    uint16_t default_line[STREAM_TYPE_MAX];
    uint16_t line_mult;
    uint16_t frame_skip;
    uint16_t target;
    uint16_t flash_threshold;
};

struct mm_sensor_awb_config {
    uint32_t dmx_wb1[CAM_WB_MODE_MAX];
    uint32_t dmx_wb2[CAM_WB_MODE_MAX];
    uint32_t wb[CAM_WB_MODE_MAX];
};

struct mm_sensor_data {
    struct mm_sensor_stream_attr *attr[STREAM_TYPE_MAX];
    struct mm_sensor_aec_config *aec_cfg;
    struct mm_sensor_awb_config *awb_cfg[STREAM_TYPE_MAX];
    void *csi_params;
    void *act_params;
    cam_capability_t *cap;
    uint32_t vfe_module_cfg;
    uint32_t vfe_clk_rate;
    uint16_t vfe_cfg_off;
    uint16_t vfe_dmux_cfg;
    uint16_t stats_enable;
    uint8_t uses_sensor_ctrls;
    uint8_t csi_dev;
    uint8_t act_id;
};

/* For legacy Camera Serial Interface */
enum msm_camera_csic_data_format {
    CSIC_8BIT,
    CSIC_10BIT,
    CSIC_12BIT,
};

struct msm_camera_csic_params {
    enum msm_camera_csic_data_format data_format;
    uint8_t lane_cnt;
    uint8_t lane_assign;
    uint8_t settle_cnt;
    uint8_t dpcm_scheme;
};

enum csic_cfg_type_t {
    CSIC_INIT,
    CSIC_CFG,
    CSIC_RELEASE,
};

struct csic_cfg_data {
    enum csic_cfg_type_t cfgtype;
    struct msm_camera_csic_params *csic_params;
};

struct mm_daemon_act_snsr_ops {
    void (*get_damping_params)(uint16_t dest_step_pos, uint16_t curr_step_pos,
        int sign_dir, struct damping_params_t *damping_params);
};

struct mm_daemon_act_params {
    struct msm_actuator_set_info_t *act_info;
    struct mm_daemon_act_snsr_ops *act_snsr_ops;
};

#define MSM_CAMERA_SUBDEV_CSIC 15
#define VIDIOC_MSM_CSIC_IO_CFG  _IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct csic_cfg_data)
#define VIDIOC_MSM_CSIC_RELEASE _IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct v4l2_subdev)

#endif
