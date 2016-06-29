/*
   Copyright (C) 2016 Brian Stepp 
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
    SNAPSHOT,
    STREAM_TYPE_MAX,
};

struct mm_sensor_attr {
    uint16_t h;
    uint16_t w;
    uint16_t blk_l;
    uint16_t blk_p;
};

struct mm_sensor_data {
    struct mm_sensor_attr *attr[STREAM_TYPE_MAX];
    cam_capability_t *cap;
    uint32_t vfe_module_cfg;
    uint8_t uses_sensor_ctrls;
    uint8_t stats_enable;
};

#endif
