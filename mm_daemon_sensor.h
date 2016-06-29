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

#ifndef MM_DAEMON_SENSOR_H
#define MM_DAEMON_SENSOR_H

#include "mm_daemon.h"
#include "sensors/mm_sensor.h"

#define DEFAULT_EXP_GAIN 0x20

typedef enum {
    SENSOR_CMD_PREVIEW = 1,
    SENSOR_CMD_SNAPSHOT,
    SENSOR_CMD_GAIN_UPDATE,
    SENSOR_CMD_EXP_GAIN,
    SENSOR_CMD_AB,
    SENSOR_CMD_WB,
    SENSOR_CMD_BRIGHTNESS,
    SENSOR_CMD_SATURATION,
    SENSOR_CMD_CONTRAST,
    SENSOR_CMD_EFFECT,
    SENSOR_CMD_SHARPNESS,
    SENSOR_CMD_POWER_UP,
    SENSOR_CMD_SHUTDOWN,
} mm_daemon_sensor_cmd_t;

typedef enum {
    SENSOR_POWER_OFF,
    SENSOR_POWER_ON,
} mm_daemon_sensor_state_t;

typedef struct mm_daemon_sensor {
    int cam_fd;
    uint16_t lines;
    mm_daemon_cfg_t *cfg_obj;
    mm_daemon_thread_info *info;
    mm_daemon_sensor_state_t sensor_state;
    mm_sensor_cfg_t *cfg;
} mm_daemon_sensor_t;

int mm_daemon_sensor_open(mm_daemon_thread_info *info);
int mm_daemon_sensor_close(mm_daemon_thread_info *info);
#endif
