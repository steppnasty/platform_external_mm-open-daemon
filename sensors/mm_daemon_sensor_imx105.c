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

#define IMX105_TOTAL_STEPS 42
#define DAMPING_THRESHOLD 10
#define IMX105_OFFSET 5
#define IMX105_MAX_ANALOG_GAIN 224
#define IMX105_MIN_ANALOG_GAIN 0
#define IMX105_MAX_DIGITAL_GAIN 384
#define IMX105_MIN_DIGITAL_GAIN 256
#define MSB(word) (word & 0xFF00) >> 8
#define LSB(word) (word & 0x00FF)

struct imx105_pdata {
    uint8_t mode;
    uint16_t line;
    uint16_t dgain;
    uint16_t again;
};

static uint16_t imx105_act_pos_tbl[] = {
    0, 48, 96, 144, 156, 168, 184, 200, 216, 232, 248, 264, 280, 296, 312,
    328, 344, 360, 376, 392, 408, 424, 440, 456, 472, 488, 504, 520, 536,
    552, 568, 584, 600, 616, 632, 648, 664, 680, 696, 712, 728, 744,
};

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

static struct msm_camera_i2c_reg_array imx105_video_tbl[] = {
    {0x0100, 0x00, 0},
    {0x0104, 0x01, 0},
    {0x0340, 0x04, 0},
    {0x0341, 0xF2, 0},
    {0x0342, 0x0D, 0},
    {0x0343, 0xD0, 0},
    {0x0344, 0x00, 0},
    {0x0345, 0x66, 0},
    {0x0346, 0x01, 0},
    {0x0347, 0x90, 0},
    {0x0348, 0x0C, 0},
    {0x0349, 0x71, 0},
    {0x034A, 0x08, 0},
    {0x034B, 0x57, 0},
    {0x034C, 0x0C, 0},
    {0x034D, 0x0C, 0},
    {0x034E, 0x04, 0},
    {0x034F, 0x84, 0},
    {0x0381, 0x01, 0},
    {0x0383, 0x01, 0},
    {0x0385, 0x02, 0},
    {0x0387, 0x03, 0},
    {0x3033, 0x00, 0},
    {0x3048, 0x00, 0},
    {0x304C, 0x6F, 0},
    {0x304D, 0x03, 0},
    {0x306A, 0xF2, 0},
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
    {0x0202, 0x04, 0},
    {0x0203, 0xED, 0},
    {0x0104, 0x00, 0},
    {0x0100, 0x01, 0},
};

static struct msm_camera_i2c_reg_array imx105_snap_tbl[] = {
    {0x0100, 0x00, 0},
    {0x0104, 0x01, 0},
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
    {0x0104, 0x00, 0},
    {0x0100, 0x01, 0},
};

static struct msm_camera_i2c_reg_array imx105_stop_settings[] = {
    {0x0100, 0x00, 0},
};

static struct msm_camera_i2c_reg_array imx105_act_init_settings[] = {
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

static int imx105_exp_gain(mm_sensor_cfg_t *cfg, uint16_t gain, uint16_t line)
{
    int rc = 0;
    struct imx105_pdata *pdata = (struct imx105_pdata *)cfg->pdata;
    uint16_t max_line_val = 2398;
    uint16_t dgain = IMX105_MIN_DIGITAL_GAIN;
    uint16_t again = gain;
    uint32_t line_val = line;
    uint32_t fl_lines;

    if (again > IMX105_MAX_ANALOG_GAIN) {
        dgain += again - IMX105_MAX_ANALOG_GAIN;
        again = IMX105_MAX_ANALOG_GAIN;
    }
    if (dgain > IMX105_MAX_DIGITAL_GAIN)
        dgain = IMX105_MAX_DIGITAL_GAIN;

    fl_lines = cfg->data->attr[pdata->mode]->h +
            cfg->data->attr[pdata->mode]->blk_l;

    if (line_val > (fl_lines - IMX105_OFFSET))
        fl_lines = line_val + IMX105_OFFSET;

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
            {0x0202, MSB(line_val), 0},
            {0x0203, LSB(line_val), 0},
            {0x0104, 0x00, 0},
        };

        rc = cfg->ops->i2c_write_array(cfg->mm_snsr, exp_settings,
                ARRAY_SIZE(exp_settings), MSM_CAMERA_I2C_BYTE_DATA);
    }
    pdata->line = line_val;
    pdata->again = again;
    pdata->dgain = dgain;
    return rc;
}

static void imx105_get_damping_params(uint16_t dest_step_pos,
        uint16_t curr_step_pos, int sign_dir,
        struct damping_params_t *damping_params)
{
    uint16_t dest_lens_pos = imx105_act_pos_tbl[dest_step_pos];
    uint16_t curr_lens_pos = imx105_act_pos_tbl[curr_step_pos];
    uint16_t damping_code_step;
    uint16_t target_dist;
    uint32_t sw_damping_time_wait;

    target_dist = sign_dir * (dest_lens_pos - curr_lens_pos);

    if (sign_dir < 0 && (target_dist >= imx105_act_pos_tbl[DAMPING_THRESHOLD])) {
        damping_code_step = target_dist / 4;
        sw_damping_time_wait = 10;
    } else {
        damping_code_step = target_dist / 2;
        sw_damping_time_wait = 4;
    }

    damping_params->damping_step = damping_code_step;
    damping_params->damping_delay = sw_damping_time_wait * 500;
}

static int imx105_init_regs(mm_sensor_cfg_t *cfg)
{
    int rc = -1;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;

    rc = cfg->ops->i2c_write_array(cfg->mm_snsr, imx105_init_settings,
            ARRAY_SIZE(imx105_init_settings), dt);

    if (rc < 0)
        return rc;

    rc = cfg->ops->i2c_write_array(cfg->mm_snsr, imx105_act_init_settings,
            ARRAY_SIZE(imx105_act_init_settings), dt);

    return rc;
}

static int imx105_init_data(mm_sensor_cfg_t *cfg)
{
    cfg->pdata = calloc(1, sizeof(struct imx105_pdata));
    if (!cfg->pdata)
        return -1;

    return 0;
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

    pdata->mode = PREVIEW;

    return cfg->ops->i2c_write_array(cfg->mm_snsr, imx105_prev_tbl,
            ARRAY_SIZE(imx105_prev_tbl), MSM_CAMERA_I2C_BYTE_DATA);
}

static int imx105_video(mm_sensor_cfg_t *cfg)
{
    struct imx105_pdata *pdata = (struct imx105_pdata *)cfg->pdata;

    pdata->mode = VIDEO;

    return cfg->ops->i2c_write_array(cfg->mm_snsr, imx105_video_tbl,
            ARRAY_SIZE(imx105_video_tbl), MSM_CAMERA_I2C_BYTE_DATA);
}

static int imx105_snapshot(mm_sensor_cfg_t *cfg)
{
    struct imx105_pdata *pdata = (struct imx105_pdata *)cfg->pdata;

    pdata->mode = SNAPSHOT;

    return cfg->ops->i2c_write_array(cfg->mm_snsr, imx105_snap_tbl,
            ARRAY_SIZE(imx105_snap_tbl), MSM_CAMERA_I2C_BYTE_DATA);
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

    .supported_focus_modes_cnt = 2,
    .supported_focus_modes = {
        CAM_FOCUS_MODE_AUTO,
        CAM_FOCUS_MODE_INFINITY,
    },

    //TODO: get this from sensor_init_params
    .sensor_mount_angle = 90,

    .focal_length = 3.69,
    .hor_view_angle = 63.1,
    .ver_view_angle = 63.1,

    .preview_sizes_tbl_cnt = 8,
    .preview_sizes_tbl = {
        {1920, 1088},
        {1280, 720},
        {960, 544},
        {800, 480},
        {720, 480},
        {640, 480},
        {640, 368},
        {480, 320},
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

    .scale_picture_sizes_cnt = 6,
    .scale_picture_sizes = {
        {512, 288},
        {480, 288},
        {432, 288},
        {512, 384},
        {352, 288},
        {0, 0},
    },
};

static struct mm_sensor_stream_attr imx105_attr_preview = {
    .ro_cfg = 0x1386633,
    .h = 1232,
    .w = 1640,
    .blk_l = 34,
    .blk_p = 1896,
    .vscale = 1,
};

static struct mm_sensor_stream_attr imx105_attr_video = {
    .ro_cfg = 0x14C6060,
    .h = 1156,
    .w = 3084,
    .blk_l = 110,
    .blk_p = 452,
    .vscale = 1.5,
};

static struct mm_sensor_stream_attr imx105_attr_snapshot = {
    .ro_cfg = 0x9CCC66,
    .h = 2464,
    .w = 3280,
    .blk_l = 70,
    .blk_p = 256,
    .vscale = 1,
};

static struct mm_sensor_aec_config imx105_aec_cfg = {
    .gain_min = 0,
    .gain_max = 352,
    .line_min = 190,
    .line_max = 3729,
    .default_gain = 2,
    .default_line = { 2600, 2600, 2597 },
    .line_mult = 10,
    .frame_skip = 1,
    .target = { 5000, 4000, 4000 },
    .flash_threshold = 224,
};

static struct mm_sensor_awb_config imx105_awb_prev_cfg = {
    .dmx_wb1 = { 0x00840084, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    .dmx_wb2 = { 0x00800080, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    .wb = { 0x03EE428D, 0, 0x02EB268D, 0x031A7A8D, 0, 0x04E5968D, 0, 0, 0, 0 },
};

static struct mm_sensor_awb_config imx105_awb_snap_cfg = {
    .dmx_wb1 = { 0x00910091, 0, 0x008D008D, 0x00910091, 0, 0x008D008D, 0, 0, 0, 0 },
    .dmx_wb2 = { 0x01170113, 0, 0x00E200B9, 0x01170113, 0, 0x00CF012D, 0, 0, 0, 0 },
    .wb = { 0x02010080, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/* MIPI CSI Controller config */
static struct msm_camera_csic_params imx105_csic_params = {
    .data_format = CSIC_10BIT,
    .lane_cnt = 2,
    .lane_assign = 0xE4,
    .settle_cnt = 0x14,
    .dpcm_scheme = 0,
};

static struct msm_actuator_reg_params_t imx105_act_reg_tbl[] = {
    {
        .reg_write_type = MSM_ACTUATOR_WRITE_DAC,
        .hw_mask = 0x0,
        .reg_addr = 0x3402,
        .hw_shift = 0,
        .data_shift = 0,
    },
    {
        .reg_write_type = MSM_ACTUATOR_WRITE_DAC,
        .hw_mask = 0x0,
        .reg_addr = 0x3403,
        .hw_shift = 0,
        .data_shift = 0,
    },
};

static struct region_params_t imx105_act_region_params[] = {
    {
        .step_bound[MOVE_NEAR] = 3,
        .step_bound[MOVE_FAR] = 0,
        .code_per_step = 48,
    },
    {
        .step_bound[MOVE_NEAR] = 5,
        .step_bound[MOVE_FAR] = 4,
        .code_per_step = 12,
    },
    {
        .step_bound[MOVE_NEAR] = 42,
        .step_bound[MOVE_FAR] = 6,
        .code_per_step = 16,
    },
};

static struct msm_actuator_set_info_t imx105_act_info = {
    .actuator_params = {
        .act_type = ACTUATOR_VCM,
        .reg_tbl_size = ARRAY_SIZE(imx105_act_reg_tbl),
        .data_size = 10,
        .init_setting_size = 0,
        .i2c_addr = 0x1A << 1,
        .i2c_addr_type = MSM_ACTUATOR_WORD_ADDR,
        .i2c_data_type = MSM_ACTUATOR_BYTE_DATA,
        .reg_tbl_params = imx105_act_reg_tbl,
        .init_settings = NULL,
    },
    .af_tuning_params = {
        .initial_code = 0,
        .region_size = ARRAY_SIZE(imx105_act_region_params),
        .total_steps = IMX105_TOTAL_STEPS,
        .region_params = imx105_act_region_params,
    },
};

static struct mm_daemon_act_snsr_ops imx105_act_snsr_ops = {
    .get_damping_params = &imx105_get_damping_params,
};

static struct mm_daemon_act_params imx105_act_params = {
    .act_info = &imx105_act_info,
    .act_snsr_ops = &imx105_act_snsr_ops,
};

static struct mm_sensor_data imx105_data = {
    .attr = { 
        &imx105_attr_preview,
        &imx105_attr_video,
        &imx105_attr_snapshot,
    },
    .aec_cfg = &imx105_aec_cfg,
    .awb_cfg = {
        &imx105_awb_prev_cfg,
        &imx105_awb_prev_cfg,
        &imx105_awb_snap_cfg,
    },
    .csi_params = (void *)&imx105_csic_params,
    .act_params = (void *)&imx105_act_params,
    .csi_dev = 0,
    .cap = &imx105_capabilities,
    .vfe_module_cfg = 0x1F27C17,
    .vfe_clk_rate = 266667000,
    .vfe_cfg_off = 0x0200,
    .vfe_dmux_cfg = 0xC9AC,
    .stats_enable = 0x7,
    .uses_sensor_ctrls = 0,
    .act_id = 0,
};

static struct mm_sensor_regs imx105_stop_regs = {
    .regs = imx105_stop_settings,
    .size = ARRAY_SIZE(imx105_stop_settings),
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct mm_sensor_ops imx105_ops = {
    .init_regs = imx105_init_regs,
    .init_data = imx105_init_data,
    .deinit = imx105_deinit,
    .prev = imx105_preview,
    .video = imx105_video,
    .snap = imx105_snapshot,
    .exp_gain = imx105_exp_gain,
};

mm_sensor_cfg_t sensor_cfg_obj = {
    .stop_regs = &imx105_stop_regs,
    .ops = &imx105_ops,
    .data = &imx105_data,
};
