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

#define DEFAULT_EXP_GAIN 0x20

typedef enum {
    SENSOR_CMD_PREVIEW = 1,
    SENSOR_CMD_SNAPSHOT,
    SENSOR_CMD_GAIN_UPDATE,
    SENSOR_CMD_EXP_GAIN,
    SENSOR_CMD_POWER_UP,
    SENSOR_CMD_SHUTDOWN,
} mm_daemon_sensor_cmd_t;

typedef enum {
    SENSOR_POWER_OFF,
    SENSOR_POWER_ON,
} mm_daemon_sensor_state_t;

typedef struct mm_daemon_sensor {
    pthread_t pid;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int cam_fd;
    int32_t pfds[2];
    mm_daemon_cfg_t *cfg_obj;
    uint8_t curr_stream;
    uint16_t curr_gain;
    mm_daemon_sensor_state_t sensor_state;
    mm_daemon_state_type_t state;
} mm_daemon_sensor_t;

mm_daemon_sensor_t *mm_daemon_sensor_open();
int mm_daemon_sensor_close(mm_daemon_sensor_t *mm_snsr);
#endif
