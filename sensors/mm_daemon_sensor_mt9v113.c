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

#include "mm_sensor.h"
#include "../common.h"

struct mt9v113_pdata {
    uint8_t mode;
    unsigned int pre_mirror_mode;
};

enum mt9v113_brightness_mode {
    BRIGHTNESS_N3,
    BRIGHTNESS_N2,
    BRIGHTNESS_N1,
    BRIGHTNESS_DEFAULT,
    BRIGHTNESS_P1,
    BRIGHTNESS_P2,
    BRIGHTNESS_P3,
    BRIGHTNESS_P4,
    BRIGHTNESS_N4,
};

enum mt9v113_ffc_mode {
    MIRROR,
    REVERSE,
    PORTRAIT_REVERSE,
};

enum mt9v113_saturation_mode {
    SATURATION_X0,
    SATURATION_X05,
    SATURATION_X1,
    SATURATION_X15,
    SATURATION_X2,
};

enum mt9v113_contrast_mode {
    CONTRAST_P2,
    CONTRAST_P1,
    CONTRAST_D,
    CONTRAST_N1,
    CONTRAST_N2,
};

enum mt9v113_effect_mode {
    EFFECT_OFF,
    EFFECT_MONO,
    EFFECT_NEGATIVE,
    EFFECT_SEPIA,
    EFFECT_AQUA,
};

enum mt9v113_sharpness_mode {
    SHARPNESS_X0,
    SHARPNESS_X1,
    SHARPNESS_X2,
    SHARPNESS_X3,
    SHARPNESS_X4,
};

static struct msm_camera_i2c_reg_array mt9v113_stop_settings[] = {
    {0x0017, 0x20, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_antibanding_50hz[] = {
    {0x098C, 0xA404, 0},
    {0x0990, 0x00C0, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_antibanding_60hz[] = {
    {0x098C, 0xA404, 0},
    {0x0990, 0x0080, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_wb_auto_0[] = {
    {0x098C, 0xA11F, 0},
    {0x0990, 0x0001, 0},
    {0x098C, 0xA103, 0},
    {0x0990, 0x0005, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_wb_auto_1[] = {
    {0x098C, 0x2306, 0},
    {0x0990, 0x03C0, 0},
    {0x098C, 0x2308, 0},
    {0x0990, 0xFD7C, 0},
    {0x098C, 0x230A, 0},
    {0x0990, 0xFFF7, 0},
    {0x098C, 0x230C, 0},
    {0x0990, 0xFF25, 0},
    {0x098C, 0x230E, 0},
    {0x0990, 0x0384, 0},
    {0x098C, 0x2310, 0},
    {0x0990, 0xFFD6, 0},
    {0x098C, 0x2312, 0},
    {0x0990, 0xFED2, 0},
    {0x098C, 0x2314, 0},
    {0x0990, 0xFCB2, 0},
    {0x098C, 0x2316, 0},
    {0x0990, 0x068E, 0},
    {0x098C, 0x2318, 0},
    {0x0990, 0x001B, 0},
    {0x098C, 0x231A, 0},
    {0x0990, 0x0039, 0},
    {0x098C, 0x231C, 0},
    {0x0990, 0xFF65, 0},
    {0x098C, 0x231E, 0},
    {0x0990, 0x0052, 0},
    {0x098C, 0x2320, 0},
    {0x0990, 0x0012, 0},
    {0x098C, 0x2322, 0},
    {0x0990, 0x0007, 0},
    {0x098C, 0x2324, 0},
    {0x0990, 0xFFCF, 0},
    {0x098C, 0x2326, 0},
    {0x0990, 0x0037, 0},
    {0x098C, 0x2328, 0},
    {0x0990, 0x00DB, 0},
    {0x098C, 0x232A, 0},
    {0x0990, 0x01C8, 0},
    {0x098C, 0x232C, 0},
    {0x0990, 0xFC9F, 0},
    {0x098C, 0x232E, 0},
    {0x0990, 0x0010, 0},
    {0x098C, 0x2330, 0},
    {0x0990, 0xFFF3, 0}
};

static struct msm_camera_i2c_reg_array mt9v113_wb_fluorescent_0[] = {
    {0x098C, 0xA115, 0},
    {0x0990, 0x0000, 0},
    {0x098C, 0xA11F, 0},
    {0x0990, 0x0000, 0},
    {0x098C, 0xA103, 0},
    {0x0990, 0x0005, 0}
};

static struct msm_camera_i2c_reg_array mt9v113_wb_fluorescent_1[] = {
    {0x098C, 0xA353, 0},
    {0x0990, 0x0043, 0},
    {0x098C, 0xA34E, 0},
    {0x0990, 0x00A0, 0},
    {0x098C, 0xA34F, 0},
    {0x0990, 0x0086, 0},
    {0x098C, 0xA350, 0},
    {0x0990, 0x008A, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_wb_incandescent_0[] = {
    {0x098C, 0xA115, 0},
    {0x0990, 0x0000, 0},
    {0x098C, 0xA11F, 0},
    {0x0990, 0x0000, 0},
    {0x098C, 0xA103, 0},
    {0x0990, 0x0005, 0}
};

static struct msm_camera_i2c_reg_array mt9v113_wb_incandescent_1[] = {
    {0x098C, 0xA353, 0},
    {0x0990, 0x000B, 0},
    {0x098C, 0xA34E, 0},
    {0x0990, 0x0090, 0},
    {0x098C, 0xA34F, 0},
    {0x0990, 0x0085, 0},
    {0x098C, 0xA350, 0},
    {0x0990, 0x00A0, 0}
};

static struct msm_camera_i2c_reg_array mt9v113_wb_daylight_0[] = {
    {0x098C, 0xA115, 0},
    {0x0990, 0x0000, 0},
    {0x098C, 0xA11F, 0},
    {0x0990, 0x0000, 0},
    {0x098C, 0xA103, 0},
    {0x0990, 0x0005, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_wb_daylight_1[] = {
    {0x098C, 0xA353, 0},
    {0x0990, 0x007F, 0},
    {0x098C, 0xA34E, 0},
    {0x0990, 0x00A2, 0},
    {0x098C, 0xA34F, 0},
    {0x0990, 0x0085, 0},
    {0x098C, 0xA350, 0},
    {0x0990, 0x0080, 0}
};

static struct msm_camera_i2c_reg_array mt9v113_brightness_n4[] = {
    {0x098C, 0xA24F, 0},
    {0x0990, 0x001E, 0},
    {0x098C, 0xAB1F, 0},
    {0x0990, 0x00CA, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_brightness_n3[] = {
    {0x098C, 0xA24F, 0},
    {0x0990, 0x0025, 0},
    {0x098C, 0xAB1F, 0},
    {0x0990, 0x00C9, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_brightness_n2[] = {
    {0x098C, 0xA24F, 0},
    {0x0990, 0x0028, 0},
    {0x098C, 0xAB1F, 0},
    {0x0990, 0x00C9, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_brightness_n1[] = {
    {0x098C, 0xA24F, 0},
    {0x0990, 0x002E, 0},
    {0x098C, 0xAB1F, 0},
    {0x0990, 0x00C9, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_brightness_d[] = {
    {0x098C, 0xA24F, 0},
    {0x0990, 0x0033, 0},
    {0x098C, 0xAB1F, 0},
    {0x0990, 0x00C9, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_brightness_p1[] = {
    {0x098C, 0xA24F, 0},
    {0x0990, 0x003B, 0},
    {0x098C, 0xAB1F, 0},
    {0x0990, 0x00C8, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_brightness_p2[] = {
    {0x098C, 0xA24F, 0},
    {0x0990, 0x0046, 0},
    {0x098C, 0xAB1F, 0},
    {0x0990, 0x00C8, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_brightness_p3[] = {
    {0x098C, 0xA24F, 0},
    {0x0990, 0x004D, 0},
    {0x098C, 0xAB1F, 0},
    {0x0990, 0x00C8, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_brightness_p4[] = {
    {0x098C, 0xA24F, 0},
    {0x0990, 0x0054, 0},
    {0x098C, 0xAB1F, 0},
    {0x0990, 0x00C8, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_ffc_mirror[] = {
    {0x098C, 0x2717, 0},
    {0x0990, 0x0027, 0},
    {0x098C, 0x272D, 0},
    {0x0990, 0x0027, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_ffc_reverse[] = {
    {0x098C, 0x2717, 0},
    {0x0990, 0x0026, 0},
    {0x098C, 0x272D, 0},
    {0x0990, 0x0026, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_ffc_portrait_reverse[] = {
    {0x098C, 0x2717, 0},
    {0x0990, 0x0025, 0},
    {0x098C, 0x272D, 0},
    {0x0990, 0x0025, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_saturation_x0[] = {
    {0x098C, 0xAB20, 0},
    {0x0990, 0x0010, 0},
    {0x098C, 0xAB24, 0},
    {0x0990, 0x0009, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_saturation_x05[] = {
    {0x098C, 0xAB20, 0},
    {0x0990, 0x0035, 0},
    {0x098C, 0xAB24, 0},
    {0x0990, 0x0025, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_saturation_x1[] = {
    {0x098C, 0xAB20, 0},
    {0x0990, 0x0048, 0},
    {0x098C, 0xAB24, 0},
    {0x0990, 0x0033, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_saturation_x15[] = {
    {0x098C, 0xAB20, 0},
    {0x0990, 0x0063, 0},
    {0x098C, 0xAB24, 0},
    {0x0990, 0x0045, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_saturation_x2[] = {
    {0x098C, 0xAB20, 0},
    {0x0990, 0x0076, 0},
    {0x098C, 0xAB24, 0},
    {0x0990, 0x0053, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_contrast_n2[] = {
    {0x098C, 0xAB3C, 0},
    {0x0990, 0x0000, 0},
    {0x098C, 0xAB3D, 0},
    {0x0990, 0x0023, 0},
    {0x098C, 0xAB3E, 0},
    {0x0990, 0x0045, 0},
    {0x098C, 0xAB3F, 0},
    {0x0990, 0x0064, 0},
    {0x098C, 0xAB40, 0},
    {0x0990, 0x0080, 0},
    {0x098C, 0xAB41, 0},
    {0x0990, 0x0099, 0},
    {0x098C, 0xAB42, 0},
    {0x0990, 0x00B0, 0},
    {0x098C, 0xAB43, 0},
    {0x0990, 0x00C1, 0},
    {0x098C, 0xAB44, 0},
    {0x0990, 0x00CF, 0},
    {0x098C, 0xAB45, 0},
    {0x0990, 0x00D9, 0},
    {0x098C, 0xAB46, 0},
    {0x0990, 0x00E1, 0},
    {0x098C, 0xAB47, 0},
    {0x0990, 0x00E8, 0},
    {0x098C, 0xAB48, 0},
    {0x0990, 0x00EE, 0},
    {0x098C, 0xAB49, 0},
    {0x0990, 0x00F2, 0},
    {0x098C, 0xAB4A, 0},
    {0x0990, 0x00F6, 0},
    {0x098C, 0xAB4B, 0},
    {0x0990, 0x00F9, 0},
    {0x098C, 0xAB4C, 0},
    {0x0990, 0x00FB, 0},
    {0x098C, 0xAB4D, 0},
    {0x0990, 0x00FD, 0},
    {0x098C, 0xAB4E, 0},
    {0x0990, 0x00FF, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_contrast_n1[] = {
    {0x098C, 0xAB3C, 0},
    {0x0990, 0x0000, 0},
    {0x098C, 0xAB3D, 0},
    {0x0990, 0x001B, 0},
    {0x098C, 0xAB3E, 0},
    {0x0990, 0x002E, 0},
    {0x098C, 0xAB3F, 0},
    {0x0990, 0x004C, 0},
    {0x098C, 0xAB40, 0},
    {0x0990, 0x0078, 0},
    {0x098C, 0xAB41, 0},
    {0x0990, 0x0098, 0},
    {0x098C, 0xAB42, 0},
    {0x0990, 0x00B0, 0},
    {0x098C, 0xAB43, 0},
    {0x0990, 0x00C1, 0},
    {0x098C, 0xAB44, 0},
    {0x0990, 0x00CF, 0},
    {0x098C, 0xAB45, 0},
    {0x0990, 0x00D9, 0},
    {0x098C, 0xAB46, 0},
    {0x0990, 0x00E1, 0},
    {0x098C, 0xAB47, 0},
    {0x0990, 0x00E8, 0},
    {0x098C, 0xAB48, 0},
    {0x0990, 0x00EE, 0},
    {0x098C, 0xAB49, 0},
    {0x0990, 0x00F2, 0},
    {0x098C, 0xAB4A, 0},
    {0x0990, 0x00F6, 0},
    {0x098C, 0xAB4B, 0},
    {0x0990, 0x00F9, 0},
    {0x098C, 0xAB4C, 0},
    {0x0990, 0x00FB, 0},
    {0x098C, 0xAB4D, 0},
    {0x0990, 0x00FD, 0},
    {0x098C, 0xAB4E, 0},
    {0x0990, 0x00FF, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_contrast_d[] = {
    {0x098C, 0xAB3C, 0},
    {0x0990, 0x0000, 0},
    {0x098C, 0xAB3D, 0},
    {0x0990, 0x0014, 0},
    {0x098C, 0xAB3E, 0},
    {0x0990, 0x0027, 0},
    {0x098C, 0xAB3F, 0},
    {0x0990, 0x0041, 0},
    {0x098C, 0xAB40, 0},
    {0x0990, 0x0074, 0},
    {0x098C, 0xAB41, 0},
    {0x0990, 0x0093, 0},
    {0x098C, 0xAB42, 0},
    {0x0990, 0x00AD, 0},
    {0x098C, 0xAB43, 0},
    {0x0990, 0x00C1, 0},
    {0x098C, 0xAB44, 0},
    {0x0990, 0x00CA, 0},
    {0x098C, 0xAB45, 0},
    {0x0990, 0x00D4, 0},
    {0x098C, 0xAB46, 0},
    {0x0990, 0x00DC, 0},
    {0x098C, 0xAB47, 0},
    {0x0990, 0x00E4, 0},
    {0x098C, 0xAB48, 0},
    {0x0990, 0x00E9, 0},
    {0x098C, 0xAB49, 0},
    {0x0990, 0x00EE, 0},
    {0x098C, 0xAB4A, 0},
    {0x0990, 0x00F2, 0},
    {0x098C, 0xAB4B, 0},
    {0x0990, 0x00F5, 0},
    {0x098C, 0xAB4C, 0},
    {0x0990, 0x00F8, 0},
    {0x098C, 0xAB4D, 0},
    {0x0990, 0x00FD, 0},
    {0x098C, 0xAB4E, 0},
    {0x0990, 0x00FF, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_contrast_p1[] = {
    {0x098C, 0xAB3C, 0},
    {0x0990, 0x0000, 0},
    {0x098C, 0xAB3D, 0},
    {0x0990, 0x0008, 0},
    {0x098C, 0xAB3E, 0},
    {0x0990, 0x0017, 0},
    {0x098C, 0xAB3F, 0},
    {0x0990, 0x002F, 0},
    {0x098C, 0xAB40, 0},
    {0x0990, 0x0050, 0},
    {0x098C, 0xAB41, 0},
    {0x0990, 0x006D, 0},
    {0x098C, 0xAB42, 0},
    {0x0990, 0x0088, 0},
    {0x098C, 0xAB43, 0},
    {0x0990, 0x009E, 0},
    {0x098C, 0xAB44, 0},
    {0x0990, 0x00AF, 0},
    {0x098C, 0xAB45, 0},
    {0x0990, 0x00BD, 0},
    {0x098C, 0xAB46, 0},
    {0x0990, 0x00C9, 0},
    {0x098C, 0xAB47, 0},
    {0x0990, 0x00D3, 0},
    {0x098C, 0xAB48, 0},
    {0x0990, 0x00DB, 0},
    {0x098C, 0xAB49, 0},
    {0x0990, 0x00E3, 0},
    {0x098C, 0xAB4A, 0},
    {0x0990, 0x00EA, 0},
    {0x098C, 0xAB4B, 0},
    {0x0990, 0x00F0, 0},
    {0x098C, 0xAB4C, 0},
    {0x0990, 0x00F5, 0},
    {0x098C, 0xAB4D, 0},
    {0x0990, 0x00FA, 0},
    {0x098C, 0xAB4E, 0},
    {0x0990, 0x00FF, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_contrast_p2[] = {
    {0x098C, 0xAB3C, 0},
    {0x0990, 0x0000, 0},
    {0x098C, 0xAB3D, 0},
    {0x0990, 0x0006, 0},
    {0x098C, 0xAB3E, 0},
    {0x0990, 0x0012, 0},
    {0x098C, 0xAB3F, 0},
    {0x0990, 0x0027, 0},
    {0x098C, 0xAB40, 0},
    {0x0990, 0x0048, 0},
    {0x098C, 0xAB41, 0},
    {0x0990, 0x0069, 0},
    {0x098C, 0xAB42, 0},
    {0x0990, 0x008A, 0},
    {0x098C, 0xAB43, 0},
    {0x0990, 0x00A4, 0},
    {0x098C, 0xAB44, 0},
    {0x0990, 0x00B7, 0},
    {0x098C, 0xAB45, 0},
    {0x0990, 0x00C6, 0},
    {0x098C, 0xAB46, 0},
    {0x0990, 0x00D1, 0},
    {0x098C, 0xAB47, 0},
    {0x0990, 0x00DB, 0},
    {0x098C, 0xAB48, 0},
    {0x0990, 0x00E2, 0},
    {0x098C, 0xAB49, 0},
    {0x0990, 0x00E9, 0},
    {0x098C, 0xAB4A, 0},
    {0x0990, 0x00EE, 0},
    {0x098C, 0xAB4B, 0},
    {0x0990, 0x00F3, 0},
    {0x098C, 0xAB4C, 0},
    {0x0990, 0x00F7, 0},
    {0x098C, 0xAB4D, 0},
    {0x0990, 0x00FB, 0},
    {0x098C, 0xAB4E, 0},
    {0x0990, 0x00FF, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_effect_off[] = {
    {0x098C, 0x2759, 0},
    {0x0990, 0x6440, 0},
    {0x098C, 0x275B, 0},
    {0x0990, 0x6440, 0},
    {0x098C, 0x2763, 0},
    {0x0990, 0xB023, 0},
    {0x098C, 0xA103, 0},
    {0x0990, 0x0005, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_effect_mono[] = {
    {0x098C, 0x2759, 0},
    {0x0990, 0x6441, 0},
    {0x098C, 0x275B, 0},
    {0x0990, 0x6441, 0},
    {0x098C, 0x2763, 0},
    {0x0990, 0xB023, 0},
    {0x098C, 0xA103, 0},
    {0x0990, 0x0005, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_effect_negative[] = {
    {0x098C, 0x2759, 0},
    {0x0990, 0x6443, 0},
    {0x098C, 0x275B, 0},
    {0x0990, 0x6443, 0},
    {0x098C, 0x2763, 0},
    {0x0990, 0xB023, 0},
    {0x098C, 0xA103, 0},
    {0x0990, 0x0005, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_effect_sepia[] = {
    {0x098C, 0x2759, 0},
    {0x0990, 0x6442, 0},
    {0x098C, 0x275B, 0},
    {0x0990, 0x6442, 0},
    {0x098C, 0x2763, 0},
    {0x0990, 0xB023, 0},
    {0x098C, 0xA103, 0},
    {0x0990, 0x0005, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_effect_aqua[] = {
    {0x098C, 0x2759, 0},
    {0x0990, 0x6442, 0},
    {0x098C, 0x275B, 0},
    {0x0990, 0x6442, 0},
    {0x098C, 0x2763, 0},
    {0x0990, 0x30D0, 0},
    {0x098C, 0xA103, 0},
    {0x0990, 0x0005, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_sharpness_x0[] = {
    {0x098C, 0xAB22, 0},
    {0x0990, 0x0000, 0},
    {0x326C, 0x0400, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_sharpness_x1[] = {
    {0x098C, 0xAB22, 0},
    {0x0990, 0x0001, 0},
    {0x326C, 0x0600, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_sharpness_x2[] = {
    {0x098C, 0xAB22, 0},
    {0x0990, 0x0003, 0},
    {0x326C, 0x0900, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_sharpness_x3[] = {
    {0x098C, 0xAB22, 0},
    {0x0990, 0x0005, 0},
    {0x326C, 0x0B00, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_sharpness_x4[] = {
    {0x098C, 0xAB22, 0},
    {0x0990, 0x0007, 0},
    {0x326C, 0x0FF0, 0},
};

static struct msm_camera_i2c_reg_array mt9v113_snap_settings[] = {
    {0x098C, 0xA103, 0},
    {0x0990, 0x0002, 0},
};

static int mt9v113_set_antibanding(mm_sensor_cfg_t *cfg, int mode)
{
    int rc = -1;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_WORD_DATA;

    switch (mode) {
        case CAM_ANTIBANDING_MODE_50HZ:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_antibanding_50hz[0],
                    ARRAY_SIZE(mt9v113_antibanding_50hz), dt);
            break;
        case CAM_ANTIBANDING_MODE_60HZ:
        case CAM_ANTIBANDING_MODE_AUTO:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_antibanding_60hz[0],
                    ARRAY_SIZE(mt9v113_antibanding_60hz), dt);
            break;
        default:
            break;
    }
    return rc;
}

static int mt9v113_set_wb(mm_sensor_cfg_t *cfg, int wb_mode)
{
    int i, rc;
    uint16_t value;
    struct msm_camera_i2c_reg_setting setting;
    struct msm_camera_i2c_read_config read_config;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_WORD_DATA;

    switch (wb_mode) {
        case CAM_WB_MODE_AUTO:
            cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_wb_auto_0[0], ARRAY_SIZE(mt9v113_wb_auto_0), dt);

            for (i = 0; i < 100; i++) {
                cfg->ops->i2c_write(cfg->mm_snsr, 0x098C, 0xA103, dt);

                cfg->ops->i2c_read(cfg->mm_snsr, 0x0990, &value, dt);
                if (value == 0x0)
                    break;
                usleep(1000);
            }
            if (i == 100)
                return -1;

            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_wb_auto_1[0],
                    ARRAY_SIZE(mt9v113_wb_auto_1), dt);
            break;
        case CAM_WB_MODE_INCANDESCENT:
            cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_wb_incandescent_0[0],
                    ARRAY_SIZE(mt9v113_wb_incandescent_0), dt);

            for (i = 0; i < 100; i++) {
                cfg->ops->i2c_write(cfg->mm_snsr, 0x098C, 0xA103, dt);

                cfg->ops->i2c_read(cfg->mm_snsr, 0x0990, &value, dt);
                if (value == 0x0)
                    break;
                usleep(1000);
            }
            if (i == 100)
                return -1;

            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_wb_incandescent_1[0],
                    ARRAY_SIZE(mt9v113_wb_incandescent_1), dt);
            break;
        case CAM_WB_MODE_FLUORESCENT:
            cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_wb_fluorescent_0[0],
                    ARRAY_SIZE(mt9v113_wb_fluorescent_0), dt);

            for (i = 0; i < 100; i++) {
                cfg->ops->i2c_write(cfg->mm_snsr, 0x098C, 0xA103, dt);

                cfg->ops->i2c_read(cfg->mm_snsr, 0x0990, &value, dt);
                if (value == 0x0)
                    break;
                usleep(1000);
            }
            if (i == 100)
                return -1;

            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_wb_fluorescent_1[0],
                    ARRAY_SIZE(mt9v113_wb_fluorescent_1), dt);
            break;
        case CAM_WB_MODE_DAYLIGHT:
            cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_wb_daylight_0[0],
                    ARRAY_SIZE(mt9v113_wb_daylight_0), dt);

            for (i = 0; i < 100; i++) {
                cfg->ops->i2c_write(cfg->mm_snsr, 0x098C, 0xA103, dt);

                cfg->ops->i2c_read(cfg->mm_snsr, 0x0990, &value, dt);
                if (value == 0x0)
                    break;
                usleep(1000);
            }
            if (i == 100)
                return -1;

            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_wb_daylight_1[0],
                    ARRAY_SIZE(mt9v113_wb_daylight_1), dt);
            break;
        default:
            rc = -1;
            break;
    }
    return rc;
}

static int mt9v113_set_brightness(mm_sensor_cfg_t *cfg, int mode)
{
    int rc = -1;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_WORD_DATA;

    switch (mode) {
        case BRIGHTNESS_N4:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_brightness_n4[0],
                    ARRAY_SIZE(mt9v113_brightness_n4), dt);
            break;
        case BRIGHTNESS_N3:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_brightness_n3[0],
                    ARRAY_SIZE(mt9v113_brightness_n3), dt);
            break;
        case BRIGHTNESS_N2:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_brightness_n2[0],
                    ARRAY_SIZE(mt9v113_brightness_n2), dt);
            break;
        case BRIGHTNESS_N1:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_brightness_n1[0],
                    ARRAY_SIZE(mt9v113_brightness_n1), dt);
            break;
        case BRIGHTNESS_DEFAULT:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_brightness_d[0],
                    ARRAY_SIZE(mt9v113_brightness_d), dt);
            break;
        case BRIGHTNESS_P1:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_brightness_p1[0],
                    ARRAY_SIZE(mt9v113_brightness_p1), dt);
            break;
        case BRIGHTNESS_P2:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_brightness_p2[0],
                    ARRAY_SIZE(mt9v113_brightness_p2), dt);
            break;
        case BRIGHTNESS_P3:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_brightness_p3[0],
                    ARRAY_SIZE(mt9v113_brightness_p3), dt);
            break;
        case BRIGHTNESS_P4:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_brightness_p4[0],
                    ARRAY_SIZE(mt9v113_brightness_p4), dt);
            break;
        default:
            break;
    }

    return rc;
}

static unsigned int pre_mirror_mode;
static int mt9v113_set_ffc(mm_sensor_cfg_t *cfg,
        enum mt9v113_ffc_mode mode)
{
    int i, rc = 0;
    uint16_t value;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_WORD_DATA;

    switch (mode) {
        case MIRROR:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_ffc_mirror[0],
                    ARRAY_SIZE(mt9v113_ffc_mirror), dt);
            break;
        case REVERSE:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_ffc_reverse[0],
                    ARRAY_SIZE(mt9v113_ffc_reverse), dt);
            break;
        case PORTRAIT_REVERSE:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_ffc_portrait_reverse[0],
                    ARRAY_SIZE(mt9v113_ffc_portrait_reverse), dt);
            break;
        default:
            break;
    }
    if (rc < 0)
        return rc;

    if (pre_mirror_mode != mode) {
        cfg->ops->i2c_write(cfg->mm_snsr, 0x098C, 0xA103, dt);
        cfg->ops->i2c_write(cfg->mm_snsr, 0x0990, 0x0006, dt);

        for (i = 0; i < 100; i++) {
            cfg->ops->i2c_write(cfg->mm_snsr, 0x098C, 0xA103, dt);
            cfg->ops->i2c_read(cfg->mm_snsr, 0x0990, &value, dt);
            if (value == 0x0)
                break;
            usleep(1000);
        }
        if (i == 100)
            return -1;
    }
    pre_mirror_mode = mode;

    usleep(20000);

    return rc;
}

static int mt9v113_set_saturation(mm_sensor_cfg_t *cfg, int value)
{
    int rc;
    int mode = value/2;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_WORD_DATA;

    switch (mode) {
        case SATURATION_X0:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_saturation_x0[0],
                    ARRAY_SIZE(mt9v113_saturation_x0), dt);
            break;
        case SATURATION_X05:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_saturation_x05[0],
                    ARRAY_SIZE(mt9v113_saturation_x05), dt);
            break;
        case SATURATION_X1:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_saturation_x1[0],
                    ARRAY_SIZE(mt9v113_saturation_x1), dt);
            break;
        case SATURATION_X15:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_saturation_x15[0],
                    ARRAY_SIZE(mt9v113_saturation_x15), dt);
            break;
        case SATURATION_X2:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_saturation_x2[0],
                    ARRAY_SIZE(mt9v113_saturation_x2), dt);
            break;
        default:
            rc = -1;
            break;
    }

    return rc;
}

static int mt9v113_set_contrast(mm_sensor_cfg_t *cfg, int value)
{
   int rc = -1;
   int mode = value/2;
   enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_WORD_DATA;

   switch (mode) {
       case CONTRAST_N2:
           rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                   &mt9v113_contrast_n2[0],
                   ARRAY_SIZE(mt9v113_contrast_n2), dt);
           break;
       case CONTRAST_N1:
           rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                   &mt9v113_contrast_n1[0],
                   ARRAY_SIZE(mt9v113_contrast_n1), dt);
           break;
       case CONTRAST_D:
           rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                   &mt9v113_contrast_d[0],
                   ARRAY_SIZE(mt9v113_contrast_d), dt);
           break;
       case CONTRAST_P1:
           rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                   &mt9v113_contrast_p1[0],
                   ARRAY_SIZE(mt9v113_contrast_p1), dt);
           break;
       case CONTRAST_P2:
           rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                   &mt9v113_contrast_p2[0],
                   ARRAY_SIZE(mt9v113_contrast_p2), dt);
           break;
       default:
           rc = -1;
           break;
   }

   return rc;
}

static int pre_effect;
static int mt9v113_set_effect(mm_sensor_cfg_t *cfg, int mode)
{
    int i;
    unsigned short reg_value;
    int rc = -1;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_WORD_DATA;

    if (pre_effect == mode)
        return 0;
    switch (mode) {
        case CAM_EFFECT_MODE_OFF:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_effect_off[0],
                    ARRAY_SIZE(mt9v113_effect_off), dt);
            break;
        case CAM_EFFECT_MODE_MONO:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_effect_mono[0],
                    ARRAY_SIZE(mt9v113_effect_mono), dt);
            break;
        case CAM_EFFECT_MODE_NEGATIVE:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_effect_negative[0],
                    ARRAY_SIZE(mt9v113_effect_negative), dt);
            break;
        case CAM_EFFECT_MODE_SEPIA:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_effect_sepia[0],
                    ARRAY_SIZE(mt9v113_effect_sepia), dt);
            break;
        case CAM_EFFECT_MODE_AQUA:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_effect_aqua[0],
                    ARRAY_SIZE(mt9v113_effect_aqua), dt);
            break;
        default:
            break;
    }
    if (rc < 0)
        return rc;

    for (i = 0; i < 100; i++) {
        cfg->ops->i2c_write(cfg->mm_snsr, 0x098C, 0xA103, dt);
        cfg->ops->i2c_read(cfg->mm_snsr, 0x0990, &reg_value, dt);
        if (reg_value == 0)
            break;
        usleep(1000);
    }
    if (i == 100)
        return -1;

    pre_effect = mode;

    return rc;
}

static int mt9v113_set_sharpness(mm_sensor_cfg_t *cfg, int value)
{
    int rc = -1;
    int mode = value/5;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_WORD_DATA;

    switch (mode) {
        case SHARPNESS_X0:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_sharpness_x0[0],
                    ARRAY_SIZE(mt9v113_sharpness_x0), dt);
            break;
        case SHARPNESS_X1:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_sharpness_x1[0],
                    ARRAY_SIZE(mt9v113_sharpness_x1), dt);
            break;
        case SHARPNESS_X2:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_sharpness_x2[0],
                    ARRAY_SIZE(mt9v113_sharpness_x2), dt);
            break;
        case SHARPNESS_X3:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_sharpness_x3[0],
                    ARRAY_SIZE(mt9v113_sharpness_x3), dt);
            break;
        case SHARPNESS_X4:
            rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                    &mt9v113_sharpness_x4[0],
                    ARRAY_SIZE(mt9v113_sharpness_x4), dt);
            break;
        default:
            rc = -1;
            break;
    }

    return rc;
}

static int mt9v113_preview(mm_sensor_cfg_t *cfg)
{
    int i, rc;
    uint16_t value;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_WORD_DATA;

    rc = mt9v113_set_antibanding(cfg, CAM_ANTIBANDING_MODE_AUTO);
    if (rc < 0)
        return rc;

    rc = mt9v113_set_brightness(cfg, BRIGHTNESS_DEFAULT);
    if (rc < 0)
        return rc;

    rc = mt9v113_set_ffc(cfg, MIRROR);
    if (rc < 0)
        return rc;

    rc = mt9v113_set_ffc(cfg, REVERSE);
    if (rc < 0)
        return rc;

    rc = cfg->ops->i2c_write(cfg->mm_snsr, 0x098C, 0xA103, dt);
    if (rc < 0)
        return rc;
    rc = cfg->ops->i2c_write(cfg->mm_snsr, 0x0990, 0x0002, dt);
    if (rc < 0)
        return rc;
    for (i = 0; i < 100; i++) {
        cfg->ops->i2c_write(cfg->mm_snsr, 0x098C, 0xA104, dt);
        cfg->ops->i2c_read(cfg->mm_snsr, 0x0990, &value, dt);
        if (value == 0x3)
            break;
        usleep(1000);
    }
    if (i == 100)
        return -1;
    usleep(150000);

    return rc;
}

static int mt9v113_snapshot(mm_sensor_cfg_t *cfg)
{
    int i;
    uint16_t value;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_WORD_DATA;

    if (cfg->ops->i2c_write(cfg->mm_snsr, 0x098C, 0xA103, dt) < 0)
        return -1;
    if (cfg->ops->i2c_write(cfg->mm_snsr, 0x0990, 0x1, dt) < 0)
        return -1;
    for (i = 0; i < 100; i++) {
        cfg->ops->i2c_write(cfg->mm_snsr, 0x098C, 0xA104, dt);
        cfg->ops->i2c_read(cfg->mm_snsr, 0x0990, &value, dt);
        if (value == 0x3)
            break;
        usleep(1000);
    }
    if (i == 100)
        return -1;

    return 0;
}

static int mt9v113_init(mm_sensor_cfg_t *cfg)
{
    cfg->pdata = calloc(1, sizeof(struct mt9v113_pdata));
    if (!cfg->pdata)
        return -1;
    return 0;
}

static int mt9v113_deinit(mm_sensor_cfg_t *cfg)
{
    if (cfg->pdata)
        free(cfg->pdata);
    return 0;
}

struct mm_sensor_regs mt9v113_snap_regs = {
    .regs = &mt9v113_snap_settings[0],
    .size = ARRAY_SIZE(mt9v113_snap_settings),
};

struct mm_sensor_regs mt9v113_stop_regs = {
    .regs = &mt9v113_stop_settings[0],
    .size = ARRAY_SIZE(mt9v113_stop_settings),
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

struct mm_sensor_ops mt9v113_ops = {
    .init = &mt9v113_init,
    .deinit = &mt9v113_deinit,
    .prev = &mt9v113_preview,
    .snap = &mt9v113_snapshot,
    .ab = &mt9v113_set_antibanding,
    .wb = &mt9v113_set_wb,
    .brightness = &mt9v113_set_brightness,
    .saturation = &mt9v113_set_saturation,
    .contrast = &mt9v113_set_contrast,
    .effect = &mt9v113_set_effect,
    .sharpness = &mt9v113_set_sharpness,
};

static struct mm_sensor_stream_attr mt9v113_attr = {
    .h = 480,
    .w = 640,
    .blk_l = 0,
    .blk_p = 0,
};

static cam_capability_t mt9v113_capabilities = {
    //TODO: get this from msm_sensor_init_params
    .position = CAM_POSITION_FRONT,

    .supported_iso_modes_cnt = 6,
    .supported_iso_modes = {
        CAM_ISO_MODE_AUTO,
        CAM_ISO_MODE_DEBLUR,
        CAM_ISO_MODE_100,
        CAM_ISO_MODE_200,
        CAM_ISO_MODE_400,
        CAM_ISO_MODE_800
    },

    .supported_flash_modes_cnt = 1,
    .supported_flash_modes = {CAM_FLASH_MODE_OFF},

    .zoom_ratio_tbl_cnt = 2,
    .zoom_ratio_tbl = {1, 2},

    .supported_effects_cnt = 5,
    .supported_effects = {
        CAM_EFFECT_MODE_OFF,
        CAM_EFFECT_MODE_MONO,
        CAM_EFFECT_MODE_NEGATIVE,
        CAM_EFFECT_MODE_SEPIA,
        CAM_EFFECT_MODE_AQUA
    },

    .fps_ranges_tbl_cnt = 1,
    .fps_ranges_tbl = {
        {9.0, 31},
    },

    .supported_antibandings_cnt = 3,
    .supported_antibandings = {
        CAM_ANTIBANDING_MODE_60HZ,
        CAM_ANTIBANDING_MODE_50HZ,
        CAM_ANTIBANDING_MODE_AUTO
    },

    .supported_white_balances_cnt = 4,
    .supported_white_balances = {
        CAM_WB_MODE_AUTO,
        CAM_WB_MODE_INCANDESCENT,
        CAM_WB_MODE_FLUORESCENT,
        CAM_WB_MODE_DAYLIGHT
    },

    .supported_focus_modes_cnt = 1,
    .supported_focus_modes = {CAM_FOCUS_MODE_FIXED},

    //TODO: get this from msm_sensor_init_params
    .sensor_mount_angle = 270,

    .focal_length = 1.42,
    .hor_view_angle = 56.3,
    .ver_view_angle = 40.2,

    .preview_sizes_tbl_cnt = 4,
    .preview_sizes_tbl = {
        {640, 480},
        {352, 288},
        {320, 240},
        {176, 144},
    },

    .video_sizes_tbl_cnt = 2,
    .video_sizes_tbl = {
        {640, 480},
        {352, 288},
    },

    .picture_sizes_tbl_cnt = 3,
    .picture_sizes_tbl = {
        {640, 480},
        {640, 384},
        {320, 240},
    },

    .supported_preview_fmt_cnt = 1,
    .supported_preview_fmts = {CAM_FORMAT_YUV_420_NV21},

    .zoom_supported = 1,

    .brightness_ctrl = {0, 6, 3, 1},
    .sharpness_ctrl = {0, 20, 10, 5},
    .contrast_ctrl = {0, 8, 4, 2},
    .saturation_ctrl = {0, 8, 4, 2},

    .qcom_supported_feature_mask = CAM_QCOM_FEATURE_SHARPNESS,

    .scale_picture_sizes_cnt = 4,
    .scale_picture_sizes = {
        {640, 480},
        {512, 384},
        {384, 288},
        {320, 240},
    },
};

/* MIPI CSI Controller config */
static struct msm_camera_csic_params mt9v113_csic_params = {
    .data_format = CSIC_8BIT,
    .lane_cnt = 1,
    .lane_assign = 0xE4,
    .settle_cnt = 0x14,
    .dpcm_scheme = 0,
};

static struct mm_sensor_data mt9v113_data = {
    .attr[PREVIEW] = &mt9v113_attr,
    .attr[SNAPSHOT] = &mt9v113_attr,
    .aec_cfg = NULL,
    .awb_cfg = { NULL, NULL, NULL },
    .csi_params = (void *)&mt9v113_csic_params,
    .csi_dev = 1,
    .cap = &mt9v113_capabilities,
    .vfe_module_cfg = 0x1C00C0C,
    .vfe_clk_rate = 228570000,
    .vfe_cfg_off = 0x0214,
    .uses_sensor_ctrls = 1,
    .stats_enable = 0,
};

mm_sensor_cfg_t sensor_cfg_obj = {
    .snap = &mt9v113_snap_regs,
    .stop_regs = &mt9v113_stop_regs,
    .ops = &mt9v113_ops,
    .data = &mt9v113_data,
};
