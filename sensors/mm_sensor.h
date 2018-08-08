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
#ifndef MM_SENSOR
#define MM_SENSOR

#include <media/msm_cam_sensor.h>
#include <fcntl.h>

struct mm_sensor_cfg;

struct mm_sensor_regs {
    struct msm_camera_i2c_reg_array *regs;
    unsigned int size;
    enum msm_camera_i2c_data_type data_type;
};

struct mm_sensor_ops {
    int (*i2c_read)(void *snsr, uint16_t reg_addr, uint16_t *data,
            enum msm_camera_i2c_data_type data_type);
    int (*i2c_write)(void *snsr, uint16_t reg_addr, uint16_t reg_data,
            enum msm_camera_i2c_data_type data_type);
    int (*i2c_write_array)(void *snsr,
            struct msm_camera_i2c_reg_array *reg_setting, uint16_t size,
            enum msm_camera_i2c_data_type data_type);
    int (*init)(struct mm_sensor_cfg *cfg);
    int (*deinit)(struct mm_sensor_cfg *cfg);
    int (*prev)(struct mm_sensor_cfg *cfg);
    int (*video)(struct mm_sensor_cfg *cfg);
    int (*snap)(struct mm_sensor_cfg *cfg);
    int (*ab)(struct mm_sensor_cfg *cfg, int mode);
    int (*wb)(struct mm_sensor_cfg *cfg, int mode);
    int (*brightness)(struct mm_sensor_cfg *cfg, int mode);
    int (*saturation)(struct mm_sensor_cfg *cfg, int value);
    int (*contrast)(struct mm_sensor_cfg *cfg, int value);
    int (*effect)(struct mm_sensor_cfg *cfg, int mode);
    int (*sharpness)(struct mm_sensor_cfg *cfg, int value);
    int (*exp_gain)(struct mm_sensor_cfg *cfg, uint16_t gain, uint16_t line);
};

typedef struct mm_sensor_cfg {
    struct mm_sensor_regs *init;
    struct mm_sensor_regs *prev;
    struct mm_sensor_regs *video;
    struct mm_sensor_regs *snap;
    struct mm_sensor_regs *stop_regs;
    struct mm_sensor_ops *ops;
    struct mm_sensor_data *data;
    void *pdata;
    void *mm_snsr;
    uint16_t curr_gain;
} mm_sensor_cfg_t;

#endif
