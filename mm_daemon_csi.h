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

#ifndef MM_DAEMON_CSI_H
#define MM_DAEMON_CSI_H

#include "mm_daemon.h"

typedef enum {
    CSI_CMD_SHUTDOWN,
    CSI_CMD_CFG,
} mm_daemon_csi_cmd_t;

typedef enum {
    CSI_POWER_OFF,
    CSI_POWER_ON,
} mm_daemon_csi_state_t;

typedef struct mm_daemon_csi {
    int fd;
    mm_daemon_csi_state_t state;
    struct msm_camera_csic_params *params;
} mm_daemon_csi_t;
#endif /* MM_DAEMON_CSI_H */
