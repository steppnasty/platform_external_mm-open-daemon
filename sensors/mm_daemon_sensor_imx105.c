/*
   Copyright (C) 2017 Brian Stepp 
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

#define TOTAL_STEPS_NEAR_TO_FAR 42
#define IMX105_OFFSET 5
#define MSB(word) (word & 0xFF00) >> 8
#define LSB(word) (word & 0x00FF)

struct imx105_pdata {
    uint8_t mode;
    uint16_t line;
    uint16_t dgain;
    uint16_t again;
};

static uint16_t imx105_step_position_table[TOTAL_STEPS_NEAR_TO_FAR+1];
static uint16_t imx105_nl_region_boundary1 = 3;
static uint16_t imx105_nl_region_boundary2 = 5;
static uint16_t imx105_nl_region_code_per_step1 = 48;
static uint16_t imx105_nl_region_code_per_step2 = 12;
static uint16_t imx105_l_region_code_per_step = 16;
static uint16_t imx105_damping_threshold = 10;
static uint16_t imx105_sw_damping_time_wait = 1;

static struct msm_camera_i2c_reg_array imx105_init_settings[] = {
    {0x0100, 0x00, 0},
    {0x0104, 0x01, 0},
    {0x0305, 0x01, 0},
    {0x0307, 0x1C, 0},
    {0x303C, 0x4B, 0},
    {0x3031, 0x10, 0},
    {0x3064, 0x12, 0},
    {0x3087, 0x57, 0},
    {0x308A, 0x35, 0},
    {0x3091, 0x41, 0},
    {0x3098, 0x03, 0},
    {0x3099, 0xC0, 0},
    {0x309A, 0xA3, 0},
    {0x309D, 0x94, 0},
    {0x30AB, 0x01, 0},
    {0x30AD, 0x00, 0},
    {0x30B1, 0x03, 0},
    {0x30C4, 0x17, 0},
    {0x30F3, 0x03, 0},
    {0x3116, 0x31, 0},
    {0x3117, 0x38, 0},
    {0x3138, 0x28, 0},
    {0x3137, 0x14, 0},
    {0x3139, 0x2E, 0},
    {0x314D, 0x2A, 0},
    {0x328F, 0x01, 0},
    {0x3343, 0x04, 0},
    {0x3032, 0x40, 0},
    {0x0104, 0x00, 0},
};

static struct msm_camera_i2c_reg_array imx105_vcm_tbl[] = {
    {0x0104, 0x01, 0},
    {0x3408, 0x02, 0},
    {0x340A, 0x01, 0},
    {0x340C, 0x01, 0},
    {0x3081, 0x4C, 0},
    {0x3400, 0x01, 0},
    {0x3401, 0x08, 0},
    {0x3404, 0x17, 0},
    {0x3405, 0x00, 0},
    {0x0104, 0x00, 0},
};

static struct msm_camera_i2c_reg_array imx105_prev_tbl[] = {
    {0x0100, 0x00, 0},
    {0x0104, 0x01, 0},
    {0x0340, 0x04, 0},
    {0x0341, 0xF2, 0},
    {0x0342, 0x0D, 0},
    {0x0343, 0xD0, 0},
    {0x0346, 0x00, 0},
    {0x0347, 0x24, 0},
    {0x034A, 0x09, 0},
    {0x034B, 0xC3, 0},
    {0x034C, 0x06, 0},
    {0x034D, 0x68, 0},
    {0x034E, 0x04, 0},
    {0x034F, 0xD0, 0},
    {0x0381, 0x01, 0},
    {0x0383, 0x03, 0},
    {0x0385, 0x01, 0},
    {0x0387, 0x03, 0},
    {0x3033, 0x00, 0},
    {0x3048, 0x01, 0},
    {0x304C, 0x6F, 0},
    {0x304D, 0x03, 0},
    {0x306A, 0xD2, 0},
    {0x309B, 0x28, 0},
    {0x309C, 0x34, 0},
    {0x309E, 0x00, 0},
    {0x30AA, 0x03, 0},
    {0x30D5, 0x09, 0},
    {0x30D6, 0x01, 0},
    {0x30D7, 0x01, 0},
    {0x30DE, 0x02, 0},
    {0x3102, 0x08, 0},
    {0x3103, 0x22, 0},
    {0x3104, 0x20, 0},
    {0x3105, 0x00, 0},
    {0x3106, 0x87, 0},
    {0x3107, 0x00, 0},
    {0x315C, 0xA5, 0},
    {0x315D, 0xA4, 0},
    {0x316E, 0xA6, 0},
    {0x316F, 0xA5, 0},
    {0x3318, 0x72, 0},
    {0x0202, 0x04, 0},
    {0x0203, 0xED, 0},
    {0x0104, 0x00, 0},
    {0x0100, 0x01, 0},
};

static struct msm_camera_i2c_reg_array imx105_snap_tbl[] = {
    {0x0340, 0x09, 0},
    {0x0341, 0xE6, 0},
    {0x0342, 0x0D, 0},
    {0x0343, 0xD0, 0},
    {0x0344, 0x00, 0},
    {0x0345, 0x04, 0},
    {0x0346, 0x00, 0},
    {0x0347, 0x24, 0},
    {0x0348, 0x0C, 0},
    {0x0349, 0xD3, 0},
    {0x034A, 0x09, 0},
    {0x034B, 0xC3, 0},
    {0x034C, 0x0C, 0},
    {0x034D, 0xD0, 0},
    {0x034E, 0x09, 0},
    {0x034F, 0xA0, 0},
    {0x0381, 0x01, 0},
    {0x0383, 0x01, 0},
    {0x0385, 0x01, 0},
    {0x0387, 0x01, 0},
    {0x3033, 0x00, 0},
    {0x3048, 0x00, 0},
    {0x304C, 0x6F, 0},
    {0x304D, 0x03, 0},
    {0x306A, 0xD2, 0},
    {0x309B, 0x20, 0},
    {0x309C, 0x34, 0},
    {0x309E, 0x00, 0},
    {0x30AA, 0x03, 0},
    {0x30D5, 0x00, 0},
    {0x30D6, 0x85, 0},
    {0x30D7, 0x2A, 0},
    {0x30DE, 0x00, 0},
    {0x3102, 0x08, 0},
    {0x3103, 0x22, 0},
    {0x3104, 0x20, 0},
    {0x3105, 0x00, 0},
    {0x3106, 0x87, 0},
    {0x3107, 0x00, 0},
    {0x315C, 0xA5, 0},
    {0x315D, 0xA4, 0},
    {0x316E, 0xA6, 0},
    {0x316F, 0xA5, 0},
    {0x3318, 0x62, 0},
    {0x0202, 0x09, 0},
    {0x0203, 0xE1, 0},
};

static struct msm_camera_i2c_reg_array imx105_stop_settings[] = {
    {0x0100, 0x00, 0},
};

static int imx105_stream_start(mm_sensor_cfg_t *cfg)
{
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;
    return cfg->ops->i2c_write(cfg->mm_snsr, 0x0100, 0x01, dt);
}

static int imx105_stream_stop(mm_sensor_cfg_t *cfg)
{
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;
    return cfg->ops->i2c_write(cfg->mm_snsr, 0x0100, 0x00, dt);
}

static void imx105_exp_gain(mm_sensor_cfg_t *cfg, uint16_t again)
{
    struct imx105_pdata *pdata = (struct imx105_pdata *)cfg->pdata;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;
    uint16_t max_legal_again = 0xE0;
    uint16_t max_legal_dgain = 0x200;
    uint16_t dgain;
    uint32_t line, fl_lines;

    line = pdata->line;
    dgain = pdata->dgain;
    fl_lines = cfg->data->attr[pdata->mode]->h +
            cfg->data->attr[pdata->mode]->blk_l;

    if (line > (fl_lines - IMX105_OFFSET))
        fl_lines = line + IMX105_OFFSET;

    if (again > max_legal_again)
        again = max_legal_again;
    if (dgain > max_legal_dgain)
        dgain = max_legal_dgain;

    {
        struct msm_camera_i2c_reg_array exp_settings[] = {
            {0x0104, 0x01, 0},
            {0x0204, MSB(again), 0},
            {0x0205, LSB(again), 0},
            {0x020E, MSB(dgain), 0},
            {0x020F, LSB(dgain), 0},
            {0x0210, MSB(dgain), 0},
            {0x0211, LSB(dgain), 0},
            {0x0212, MSB(dgain), 0},
            {0x0213, LSB(dgain), 0},
            {0x0214, MSB(dgain), 0},
            {0x0215, LSB(dgain), 0},
            {0x0340, MSB(fl_lines), 0},
            {0x0341, LSB(fl_lines), 0},
            {0x0202, MSB(line), 0},
            {0x0203, LSB(line), 0},
            {0x0104, 0x00, 0},
        };

        cfg->ops->i2c_write_array(cfg->mm_snsr,
                &exp_settings[0], ARRAY_SIZE(exp_settings), dt);
    }
    pdata->line = line;
    pdata->again = again;
    pdata->dgain = dgain;
}

static int imx105_af_init(mm_sensor_cfg_t *cfg)
{
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;
    int rc = 0;
    uint8_t i;

    imx105_step_position_table[0] = 0;
    for (i = 1; i <= TOTAL_STEPS_NEAR_TO_FAR; i++) {
        if (i <= imx105_nl_region_boundary1)
            imx105_step_position_table[i] =
                    imx105_step_position_table[i-1] +
                    imx105_nl_region_code_per_step1;
        else if (i <= imx105_nl_region_boundary2)
            imx105_step_position_table[i] =
                    imx105_step_position_table[i-1] +
                    imx105_nl_region_code_per_step2;
        else
            imx105_step_position_table[i] =
                    imx105_step_position_table[i-1] +
                    imx105_l_region_code_per_step;
        if (imx105_step_position_table[i] > 1023)
            imx105_step_position_table[i] = 1023;
    }
    rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
            &imx105_vcm_tbl[0],
            ARRAY_SIZE(imx105_vcm_tbl), dt);
    return rc;
}

static int imx105_init(mm_sensor_cfg_t *cfg)
{
    int rc = -1;
    struct imx105_pdata *pdata;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;

    cfg->pdata = calloc(1, sizeof(struct imx105_pdata));
    if (!cfg->pdata)
        return -1;

    if (cfg->ops->i2c_write_array(cfg->mm_snsr,
            &imx105_init_settings[0],
            ARRAY_SIZE(imx105_init_settings), dt) == 0)
        rc = imx105_af_init(cfg);

    return rc;
}

static int imx105_deinit(mm_sensor_cfg_t *cfg)
{
    if (cfg->pdata) {
        free(cfg->pdata);
        cfg->pdata = NULL;
    }
    return 0;
}

static int imx105_preview(mm_sensor_cfg_t *cfg)
{
    struct imx105_pdata *pdata = (struct imx105_pdata *)cfg->pdata;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;
    int rc = -1;

    pdata->mode = PREVIEW;
    pdata->line = 2600;
    pdata->dgain = 0x100;

    rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
            &imx105_prev_tbl[0],
            ARRAY_SIZE(imx105_prev_tbl), dt);

    imx105_exp_gain(cfg, 0x02);
    return rc;
}

static int imx105_snapshot(mm_sensor_cfg_t *cfg)
{
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;
    int rc = -1;

    rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
            &imx105_snap_tbl[0],
            ARRAY_SIZE(imx105_snap_tbl), dt);
    return rc;
}

static cam_capability_t imx105_capabilities = {
    //TODO: get this from msm_sensor_init_params
    .position = CAM_POSITION_BACK,

    .supported_iso_modes_cnt = 7,
    .supported_iso_modes = {
            CAM_ISO_MODE_AUTO,
            CAM_ISO_MODE_DEBLUR,
            CAM_ISO_MODE_100,
            CAM_ISO_MODE_200,
            CAM_ISO_MODE_400,
            CAM_ISO_MODE_800,
            CAM_ISO_MODE_1600,
    },

    .supported_flash_modes_cnt = 4,
    .supported_flash_modes = {
        CAM_FLASH_MODE_OFF,
        CAM_FLASH_MODE_AUTO,
        CAM_FLASH_MODE_ON,
        CAM_FLASH_MODE_TORCH,
    },

    .supported_aec_modes_cnt = 2,
    .supported_aec_modes = {
        CAM_AEC_MODE_FRAME_AVERAGE,
        CAM_AEC_MODE_CENTER_WEIGHTED,
    },

    .fps_ranges_tbl_cnt = 1,
    .fps_ranges_tbl[0] = {9.0, 30},

    .supported_white_balances_cnt = 4,
    .supported_white_balances = {
        CAM_WB_MODE_AUTO,
        CAM_WB_MODE_INCANDESCENT,
        CAM_WB_MODE_FLUORESCENT,
        CAM_WB_MODE_DAYLIGHT,
    },

    .supported_focus_modes_cnt = 1,
    .supported_focus_modes = {CAM_FOCUS_MODE_FIXED},

    //TODO: get this from sensor_init_params
    .sensor_mount_angle = 90,

    .focal_length = 3.69,
    .hor_view_angle = 63.1,
    .ver_view_angle = 63.1,

    .preview_sizes_tbl_cnt = 6,
    .preview_sizes_tbl = {
        {1290, 1088},
        {1280, 720},
        {720, 480},
        {640, 480},
        {352, 288},
        {320, 240},
    },

    .video_sizes_tbl_cnt = 6,
    .video_sizes_tbl = {
        {1920, 1088},
        {1280, 720},
        {720, 480},
        {640, 480},
        {352, 288},
        {320, 240},
    },

    .picture_sizes_tbl_cnt = 5,
    .picture_sizes_tbl = {
        {3264, 2448},
        {2592, 1952},
        {2592, 1552},
        {2592, 1456},
        {2048, 1536},
    },

    .supported_preview_fmt_cnt = 1,
    .supported_preview_fmts = {CAM_FORMAT_YUV_420_NV21},

    .histogram_supported = 1,

    .scale_picture_sizes_cnt = 3,
    .scale_picture_sizes = {
        {640, 480},
        {512, 384},
        {384, 288},
    },
};

static struct mm_sensor_stream_attr imx105_attr_preview = {
    .ro_cfg = 0x1386633,
    .h = 1232,
    .w = 1640,
    .blk_l = 12,
    .blk_p = 1896,
};

static struct mm_sensor_stream_attr imx105_attr_snapshot = {
    .h = 2464,
    .w = 3280,
    .blk_l = 70,
    .blk_p = 256,
};

/* MIPI CSI Controller config */
static struct msm_camera_csic_params imx105_csic_params = {
    .data_format = CSIC_10BIT,
    .lane_cnt = 2,
    .lane_assign = 0xE4,
    .settle_cnt = 0x14,
    .dpcm_scheme = 0,
};

static struct mm_sensor_data imx105_data = {
    .attr[PREVIEW] = &imx105_attr_preview,
    .attr[SNAPSHOT] = &imx105_attr_snapshot,
    .csi_params = (void *)&imx105_csic_params,
    .csi_dev = 0,
    .cap = &imx105_capabilities,
    .vfe_module_cfg = 0x1F27C17,
    .vfe_clk_rate = 266667000,
    .vfe_cfg_off = 0x0200,
    .vfe_dmux_cfg = 0xC9AC,
    .uses_sensor_ctrls = 0,
    .stats_enable = 1,
};

static struct mm_sensor_regs imx105_stop_regs = {
    .regs = &imx105_stop_settings[0],
    .size = ARRAY_SIZE(imx105_stop_settings),
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct mm_sensor_ops imx105_ops = {
    .init = imx105_init,
    .deinit = imx105_deinit,
    .prev = imx105_preview,
    .snap = imx105_snapshot,
    .exp_gain = imx105_exp_gain,
};

mm_sensor_cfg_t sensor_cfg_obj = {
    .stop_regs = &imx105_stop_regs,
    .ops = &imx105_ops,
    .data = &imx105_data,
};
