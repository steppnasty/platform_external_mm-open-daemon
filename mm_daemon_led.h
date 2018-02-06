/*
   Copyright (C) 2018 Brian Stepp 
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

#ifndef MM_DAEMON_LED_H
#define MM_DAEMON_LED_H

#include "mm_daemon.h"

typedef enum {
    LED_CMD_SHUTDOWN,
    LED_CMD_CONTROL,
} mm_daemon_led_cmd_t;

typedef enum {
    LED_INIT,
    LED_OFF,
    LED_ON,
} mm_daemon_led_cfg_t;

typedef struct mm_daemon_led {
    int fd;
    mm_daemon_led_cfg_t state;
} mm_daemon_led_t;

#endif /* MM_DAEMON_LED_H */
