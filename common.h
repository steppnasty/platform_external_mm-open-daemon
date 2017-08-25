/*
   Copyright (C) 2016-2017 Brian Stepp 
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
#define ARRAY_SIZE(a)     (sizeof(a) / sizeof(*a))
#define ROUND(a)((a >= 0) ? (long)(a + 0.5) : (long)(a - 0.5))

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

struct mm_sensor_data {
    struct mm_sensor_stream_attr *attr[STREAM_TYPE_MAX];
    void *csi_params;
    cam_capability_t *cap;
    uint32_t vfe_module_cfg;
    uint32_t vfe_clk_rate;
    uint16_t vfe_cfg_off;
    uint16_t vfe_dmux_cfg;
    uint8_t uses_sensor_ctrls;
    uint8_t stats_enable;
    uint8_t csi_dev;
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

#define MSM_CAMERA_SUBDEV_CSIC 15
#define VIDIOC_MSM_CSIC_IO_CFG  _IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct csic_cfg_data)
#define VIDIOC_MSM_CSIC_RELEASE _IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct v4l2_subdev)

#endif
