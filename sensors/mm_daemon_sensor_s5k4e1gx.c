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

#define S5K4E1GX_MAX_FPS 30
#define S5K4E1GX_TOTAL_STEPS 36
#define SW_DAMPING_STEP 10
#define DAMPING_THRESHOLD 5

#define MSB(word) (word & 0xFF00) >> 8
#define LSB(word) (word & 0x00FF)

struct s5k4e1gx_pdata {
    uint8_t mode;
    uint16_t line;
    uint16_t gain;
};

static uint16_t s5k4e1gx_act_pos_tbl[] = {
    0, 40, 80, 120, 140, 160, 172, 184, 196, 208, 220, 232, 244,
    256, 268, 280, 292, 304, 316, 328, 340, 352, 364, 376, 388,
    400, 412, 424, 436, 448, 460, 472, 484, 496, 508, 520, 532,
};

static struct msm_camera_i2c_reg_array s5k4e1gx_groupon_settings[] = {
    {0x0104, 0x01, 0},
};

static struct msm_camera_i2c_reg_array s5k4e1gx_groupoff_settings[] = {
    {0x0104, 0x00, 0},
};

static struct msm_camera_i2c_reg_array s5k4e1gx_init_settings[] = {
    {0x302B, 0x00, 0},
    {0x30BC, 0xA0, 0},
    {0x30BE, 0x08, 0},
    {0x0305, 0x06, 0},
    {0x0306, 0x00, 0},
    {0x0307, 0x50, 0},
    {0x30B5, 0x00, 0},
    {0x30F1, 0xB0, 0},
    {0x0101, 0x00, 0},
    {0x034C, 0x05, 0},
    {0x034D, 0x18, 0},
    {0x034E, 0x03, 0},
    {0x034F, 0xD4, 0},
    {0x0381, 0x01, 0},
    {0x0383, 0x01, 0},
    {0x0385, 0x01, 0},
    {0x0387, 0x03, 0},
    {0x30A9, 0x02, 0},
    {0x300E, 0xEB, 0},
    {0x0340, 0x03, 0},
    {0x0341, 0xE0, 0},
    {0x0342, 0x0A, 0},
    {0x0343, 0xB2, 0},
    {0x3110, 0x10, 0},
    {0x3117, 0x0C, 0},
    {0x3119, 0x0F, 0},
    {0x311A, 0xFA, 0},
    {0x0104, 0x01, 0},
    {0x0204, 0x00, 0},
    {0x0205, 0x20, 0},
    {0x0202, 0x03, 0},
    {0x0203, 0x1F, 0},
    {0x0104, 0x00, 0},
    {0x3110, 0x10, 0},
};

static struct msm_camera_i2c_reg_array s5k4e1gx_prev_settings[] = {
    {0x034C, 0x05, 0},/* x_output size msb */
    {0x034D, 0x18, 0},/* x_output size lsb */
    {0x034E, 0x03, 0},/* y_output size msb */
    {0x034F, 0xD4, 0},/* y_output size lsb */
    {0x0381, 0x01, 0},/* x_even_inc */
    {0x0383, 0x01, 0},/* x_odd_inc */
    {0x0385, 0x01, 0},/* y_even_inc */
    {0x0387, 0x03, 0},/* y_odd_inc 03(10b AVG) */
    {0x30A9, 0x02, 0},/* Horizontal Binning On */
    {0x300E, 0xEB, 0},/* Vertical Binning On */
    {0x0105, 0x01, 0},/* mask corrupted frame */
    {0x0340, 0x03, 0},/* Frame Length */
    {0x0341, 0xE0, 0},
    {0x300F, 0x82, 0},/* CDS Test */
    {0x3013, 0xC0, 0},/* rst_offset1 */
    {0x3017, 0xA4, 0},/* rmb_init */
    {0x301B, 0x88, 0},/* comp_bias */
    {0x3110, 0x10, 0},/* PCLK inv */
    {0x3117, 0x0C, 0},/* PCLK delay */
    {0x3119, 0x0F, 0},/* V H Sync Strength */
    {0x311A, 0xFA, 0},/* Data PCLK Strength */
};

static struct msm_camera_i2c_reg_array s5k4e1gx_snap_settings[] = {
    {0x0100, 0x00, 0},
    {0x034C, 0x0A, 0},
    {0x034D, 0x30, 0},
    {0x034E, 0x07, 0},
    {0x034F, 0xA8, 0},
    {0x0381, 0x01, 0},
    {0x0383, 0x01, 0},
    {0x0385, 0x01, 0},
    {0x0387, 0x01, 0},
    {0x30A9, 0x03, 0},
    {0x300E, 0xE8, 0},
    {0x0105, 0x01, 0},
    {0x0340, 0x07, 0},
    {0x0341, 0xB4, 0},
    {0x300F, 0x82, 0},
    {0x3013, 0xC0, 0},
    {0x3017, 0xA4, 0},
    {0x301B, 0x88, 0},
    {0x3110, 0x10, 0},
    {0x3117, 0x0C, 0},
    {0x3119, 0x0F, 0},
    {0x311A, 0xFA, 0},
};

static struct msm_camera_i2c_reg_array s5k4e1gx_stop_settings[] = {
    {0x0100, 0x00, 0},
};

static struct reg_settings_t s5k4e1gx_act_init_settings[] = {
    {0x0, 0x00},
};

static int s5k4e1gx_stream_start(mm_sensor_cfg_t *cfg)
{
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;
    return cfg->ops->i2c_write(cfg->mm_snsr, 0x0100, 0x01, dt);
}

static int s5k4e1gx_stream_stop(mm_sensor_cfg_t *cfg)
{
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;
    return cfg->ops->i2c_write(cfg->mm_snsr, 0x0100, 0x00, dt);
}

static int s5k4e1gx_exp_gain(mm_sensor_cfg_t *cfg, uint16_t gain)
{
    int rc = 0;
    struct s5k4e1gx_pdata *pdata = (struct s5k4e1gx_pdata *)cfg->pdata;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;
    uint8_t min_legal_gain = 0x20;
    uint16_t max_legal_gain = 0x200;
    uint16_t min_line;
    uint16_t max_line;
    uint32_t line;
    uint32_t ll_ratio;
    uint32_t fl_lines;
    uint32_t ll_pck = 2738;
    uint32_t offset = 12;

    max_line = cfg->data->attr[pdata->mode]->h;
    fl_lines = max_line + cfg->data->attr[pdata->mode]->blk_l;

    if (gain < 0x30 && pdata->line > 0x40) {
        line = (pdata->line/2);
    } else if ((gain > 0xa0) && (pdata->line < max_line)) {
        if ((pdata->line * 2) > max_line)
            line = cfg->data->attr[pdata->mode]->h;
        else
            line = pdata->line * 2;
    } else
        line = pdata->line;

    if (gain > max_legal_gain)
        gain = max_legal_gain;
    else if (gain < min_legal_gain)
        gain = min_legal_gain;

    line = (line * 0x400);

    if (fl_lines < (line / 0x400))
        ll_ratio = (line / (fl_lines - offset));
    else
        ll_ratio = 0x400;
    ll_pck = ll_pck * ll_ratio;
    line = line / ll_ratio;

    if (line == pdata->line && gain == pdata->gain)
        return rc;

    {
        struct msm_camera_i2c_reg_array exp_settings[] = {
            s5k4e1gx_groupon_settings[0],
            {0x0204, MSB(gain), 0},
            {0x0205, LSB(gain), 0},
            {0x0342, MSB(ll_pck / 0x400), 0},
            {0x0343, LSB(ll_pck / 0x400), 0},
            {0x0202, MSB(line), 0},
            {0x0203, LSB(line), 0},
            s5k4e1gx_groupoff_settings[0],
        };

        rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
                &exp_settings[0], ARRAY_SIZE(exp_settings), dt);
    }
    pdata->line = line;
    pdata->gain = gain;

    return rc;
}

static void s5k4e1gx_get_damping_params(uint16_t dest_step_pos,
        uint16_t curr_step_pos, int sign_dir,
        struct damping_params_t *damping_params)
{
    uint16_t damping_code_step;
    uint16_t target_dist;
    uint16_t dest_lens_pos = s5k4e1gx_act_pos_tbl[dest_step_pos];
    uint16_t curr_lens_pos = s5k4e1gx_act_pos_tbl[curr_step_pos];
    uint16_t time_wait_per_step;
    uint32_t sw_damping_step_dynamic;
    uint32_t sw_damping_time_wait;
    uint32_t hw_params;
    int32_t num_steps;
    int32_t time_wait;

    num_steps = sign_dir * (dest_step_pos - curr_step_pos);
    target_dist = sign_dir * (dest_lens_pos - curr_lens_pos);

    if (num_steps > 2) {
        sw_damping_step_dynamic = 4;
        sw_damping_time_wait = 4;
    } else {
        sw_damping_step_dynamic = 2;
        sw_damping_time_wait = 2;
    }

    if (sign_dir < 0 && (target_dist >= s5k4e1gx_act_pos_tbl[DAMPING_THRESHOLD])) {
        sw_damping_step_dynamic = SW_DAMPING_STEP;
        sw_damping_time_wait = 1;
        time_wait = 1000000 / S5K4E1GX_MAX_FPS - SW_DAMPING_STEP *
                sw_damping_time_wait * 1000;
    } else
        time_wait = 1000000 / S5K4E1GX_MAX_FPS;

    time_wait_per_step = (int16_t)(time_wait / target_dist);

    if (time_wait_per_step >= 800)
        hw_params = 0x5;
    else if (time_wait_per_step >= 400)
        hw_params = 0x4;
    else if (time_wait_per_step >= 200)
        hw_params = 0x3;
    else if (time_wait_per_step >= 100)
        hw_params = 0x2;
    else if (time_wait_per_step >= 50)
        hw_params = 0x1;
    else if (time_wait >= 17600)
        hw_params = 0x0D;
    else if (time_wait >= 8800)
        hw_params = 0x0C;
    else if (time_wait >= 4400)
        hw_params = 0x0B;
    else if (time_wait >= 2200)
        hw_params = 0x0A;
    else
        hw_params = 0x09;

    damping_code_step = (uint16_t)target_dist / sw_damping_step_dynamic;
    if ((target_dist % sw_damping_step_dynamic) != 0)
        damping_code_step = damping_code_step + 1;

    damping_params->damping_step = damping_code_step;
    damping_params->damping_delay = sw_damping_time_wait * 1000;
    damping_params->hw_params = hw_params;
}

static int s5k4e1gx_init(mm_sensor_cfg_t *cfg)
{
    int rc = -1;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;

    cfg->pdata = calloc(1, sizeof(struct s5k4e1gx_pdata));
    if (!cfg->pdata)
        return -1;

    rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
            &s5k4e1gx_init_settings[0],
            ARRAY_SIZE(s5k4e1gx_init_settings), dt);
    return rc;
}

static int s5k4e1gx_deinit(mm_sensor_cfg_t *cfg)
{
    if (cfg->pdata) {
        free(cfg->pdata);
        cfg->pdata = NULL;
    }
    return 0;
}

static int s5k4e1gx_preview(mm_sensor_cfg_t *cfg)
{
    int rc = -1;
    struct s5k4e1gx_pdata *pdata = (struct s5k4e1gx_pdata *)cfg->pdata;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;

    pdata->mode = PREVIEW;

    s5k4e1gx_stream_stop(cfg);
    rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
            &s5k4e1gx_prev_settings[0],
            ARRAY_SIZE(s5k4e1gx_prev_settings), dt);
    s5k4e1gx_stream_start(cfg);
    pdata->line = 980;
    return rc;
}

static int s5k4e1gx_snapshot(mm_sensor_cfg_t *cfg)
{
    int rc = -1;
    struct s5k4e1gx_pdata *pdata = (struct s5k4e1gx_pdata *)cfg->pdata;
    enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;

    pdata->mode = SNAPSHOT;
    s5k4e1gx_stream_stop(cfg);
    rc = cfg->ops->i2c_write_array(cfg->mm_snsr,
            &s5k4e1gx_snap_settings[0],
            ARRAY_SIZE(s5k4e1gx_snap_settings), dt);
    s5k4e1gx_stream_start(cfg);
    pdata->line = 1960;
    return rc;
}

struct mm_sensor_regs s5k4e1gx_prev_regs = {
    .regs = &s5k4e1gx_prev_settings[0],
    .size = ARRAY_SIZE(s5k4e1gx_prev_settings),
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

struct mm_sensor_regs s5k4e1gx_snap_regs = {
    .regs = &s5k4e1gx_snap_settings[0],
    .size = ARRAY_SIZE(s5k4e1gx_snap_settings),
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

struct mm_sensor_regs s5k4e1gx_stop_regs = {
    .regs = &s5k4e1gx_stop_settings[0],
    .size = ARRAY_SIZE(s5k4e1gx_stop_settings),
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

struct mm_sensor_ops s5k4e1gx_ops = {
    .init = s5k4e1gx_init,
    .deinit = s5k4e1gx_deinit,
    .prev = s5k4e1gx_preview,
    .snap = s5k4e1gx_snapshot,
    .exp_gain = s5k4e1gx_exp_gain,
};

static cam_capability_t s5k4e1gx_capabilities = {
    //TODO: get this from msm_sensor_init_params
    .position = CAM_POSITION_BACK,

    .supported_iso_modes_cnt = 6,
    .supported_iso_modes = {
            CAM_ISO_MODE_AUTO,
            CAM_ISO_MODE_DEBLUR,
            CAM_ISO_MODE_100,
            CAM_ISO_MODE_200,
            CAM_ISO_MODE_400,
            CAM_ISO_MODE_800
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

    .supported_focus_algos_cnt = 1,
    .supported_focus_algos = {
        CAM_FOCUS_ALGO_AUTO,
    },

    //TODO: get this from sensor_init_params
    .sensor_mount_angle = 90,

    .focal_length = 3.53,
    .hor_view_angle = 54.8,
    .ver_view_angle = 42.5,

    .preview_sizes_tbl_cnt = 5,
    .preview_sizes_tbl = {
        {1280, 720},
        {800, 480},
        {720, 480},
        {640, 480},
        {176, 144},
    },

    .video_sizes_tbl_cnt = 5,
    .video_sizes_tbl = {
        {1280, 720},
        {800, 480},
        {720, 480},
        {640, 480},
        {176, 144},
    },

    .picture_sizes_tbl_cnt = 7,
    .picture_sizes_tbl = {
        {2592, 1952},
        {2592, 1456},
        {2592, 1520},
        {2592, 1936},
        {2592, 1728},
        {2592, 1552},
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

static struct mm_sensor_stream_attr s5k4e1gx_attr_preview = {
    .ro_cfg = 0x18C5028,
    .h = 980,
    .w = 1304,
    .blk_l = 12,
    .blk_p = 1434,
};

static struct mm_sensor_stream_attr s5k4e1gx_attr_snapshot = {
    .ro_cfg = 0xC4A251,
    .h = 1960,
    .w = 2608,
    .blk_l = 27,
    .blk_p = 130,
};

/* MIPI CSI Controller config */
static struct msm_camera_csic_params s5k4e1gx_csic_params = {
    .data_format = CSIC_10BIT,
    .lane_cnt = 2,
    .lane_assign = 0xE4,
    .settle_cnt = 0x1E,
    .dpcm_scheme = 0,
};

static struct msm_actuator_reg_params_t s5k4e1gx_act_reg_tbl[] = {
    {
        .reg_write_type = MSM_ACTUATOR_WRITE_DAC,
        .hw_mask = 0xF,
        .reg_addr = 0xFFFF,
        .hw_shift = 0,
        .data_shift = 4,
    },
};

static struct region_params_t s5k4e1gx_act_region_params[] = {
    {
        .step_bound[MOVE_NEAR] = 3,
        .step_bound[MOVE_FAR] = 0,
        .code_per_step = 40,
    },
    {
        .step_bound[MOVE_NEAR] = 5,
        .step_bound[MOVE_FAR] = 4,
        .code_per_step = 20,
    },
    {
        .step_bound[MOVE_NEAR] = 36,
        .step_bound[MOVE_FAR] = 6,
        .code_per_step = 12,
    },
};

static struct msm_actuator_set_info_t s5k4e1gx_act_info = {
    .actuator_params = {
        .act_type = ACTUATOR_VCM,
        .reg_tbl_size = ARRAY_SIZE(s5k4e1gx_act_reg_tbl),
        .data_size = 10,
        .init_setting_size = ARRAY_SIZE(s5k4e1gx_act_init_settings),
        .i2c_addr = 0x18,
        .i2c_addr_type = MSM_ACTUATOR_BYTE_ADDR,
        .i2c_data_type = MSM_ACTUATOR_BYTE_DATA,
        .reg_tbl_params = s5k4e1gx_act_reg_tbl,
        .init_settings = s5k4e1gx_act_init_settings,
    },
    .af_tuning_params = {
        .initial_code = 0,
        .region_size = 3,
        .total_steps = S5K4E1GX_TOTAL_STEPS,
        .region_params = s5k4e1gx_act_region_params,
    },
};

static struct mm_daemon_act_snsr_ops s5k4e1gx_act_snsr_ops = {
    .get_damping_params = &s5k4e1gx_get_damping_params,
};

static struct mm_daemon_act_params s5k4e1gx_act_params = {
    .act_info = &s5k4e1gx_act_info,
    .act_snsr_ops = &s5k4e1gx_act_snsr_ops,
};

static struct mm_sensor_data s5k4e1gx_data = {
    .attr[PREVIEW] = &s5k4e1gx_attr_preview,
    .attr[SNAPSHOT] = &s5k4e1gx_attr_snapshot,
    .csi_params = (void *)&s5k4e1gx_csic_params,
    .act_params = (void *)&s5k4e1gx_act_params,
    .cap = &s5k4e1gx_capabilities,
    .gain_min = 0x20,
    .gain_max = 0x200,
    .vfe_module_cfg = 0x1E27C17,
    .vfe_clk_rate = 122880000,
    .vfe_cfg_off = 0x0211,
    .vfe_dmux_cfg = 0x9CCA,
    .stats_enable = 0x7,
    .uses_sensor_ctrls = 0,
    .csi_dev = 0,
    .act_id = 0,
};

mm_sensor_cfg_t sensor_cfg_obj = {
    .prev = &s5k4e1gx_prev_regs,
    .snap = &s5k4e1gx_snap_regs,
    .stop_regs = &s5k4e1gx_stop_regs,
    .ops = &s5k4e1gx_ops,
    .data = &s5k4e1gx_data,
};
