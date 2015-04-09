/*
   Copyright (C) 2014-2015 Brian Stepp 
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

#define LOG_TAG "mm-daemon-cfg"

#include <sys/types.h>
#include <media/msm_camera.h>
#include <media/msm_isp.h>
#include "mm_daemon.h"

static int mm_daemon_config_vfe_cmd(mm_daemon_obj_t *mm_obj, int cmd_type, int cmd,
        int length, void *value, unsigned int ioctl_cmd)
{
    int rc = 0;
    struct msm_vfe_cfg_cmd cfgcmd;
    struct msm_isp_cmd vfecmd;

    memset(&cfgcmd, 0, sizeof(cfgcmd));
    memset(&vfecmd, 0, sizeof(vfecmd));
    vfecmd.id = cmd;
    vfecmd.length = length;
    vfecmd.value = value;
    cfgcmd.cmd_type = cmd_type;
    cfgcmd.length = sizeof(vfecmd);
    cfgcmd.value = (void *)&vfecmd;
    rc = ioctl(mm_obj->cfg_fd, ioctl_cmd, &cfgcmd);

    return rc;
}

static int mm_daemon_config_vfe_roll_off(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    unsigned int ioctl_cmd;
    uint32_t *rocfg;
    uint32_t *p;

    rocfg = (uint32_t *)malloc(1784);
    p = rocfg;
    *p++ = 0x18c5028;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x511d5200;
    *p++ = 0x4d58437f;
    *p++ = 0x49914ae2;
    *p++ = 0x45d03d9d;
    *p++ = 0x42ba4404;
    *p++ = 0x40863958;
    *p++ = 0x3e683f53;
    *p++ = 0x3c0a34c6;
    *p++ = 0x3b603b9e;
    *p++ = 0x3a3932c1;
    *p++ = 0x39dc3984;
    *p++ = 0x39a33189;
    *p++ = 0x39a038c6;
    *p++ = 0x39e83219;
    *p++ = 0x39ac3972;
    *p++ = 0x3afb3297;
    *p++ = 0x3af23a3c;
    *p++ = 0x3d073437;
    *p++ = 0x3dda3cda;
    *p++ = 0x40d737c6;
    *p++ = 0x42f24291;
    *p++ = 0x46313cef;
    *p++ = 0x4ab64998;
    *p++ = 0x4f214403;
    *p++ = 0x55555533;
    *p++ = 0x58c94d45;
    *p++ = 0xdb7ee8ba;
    *p++ = 0xdebdeaf8;
    *p++ = 0xf089e8e5;
    *p++ = 0xeb51f013;
    *p++ = 0xf4c0f3d5;
    *p++ = 0xf5f2f407;
    *p++ = 0xf519f791;
    *p++ = 0xf5d9f77a;
    *p++ = 0xf96bf7ca;
    *p++ = 0xfb2efcb2;
    *p++ = 0xfc2ffcb1;
    *p++ = 0xfc5d0035;
    *p++ = 0xfb19fd35;
    *p++ = 0x001cfe6b;
    *p++ = 0x0042012c;
    *p++ = 0x014401ac;
    *p++ = 0xfe6afe2d;
    *p++ = 0x009c0186;
    *p++ = 0x01e9ff64;
    *p++ = 0x0507021f;
    *p++ = 0x024e0442;
    *p++ = 0x06060449;
    *p++ = 0x0892060d;
    *p++ = 0x07ff08ad;
    *p++ = 0x0a940d4c;
    *p++ = 0x0b0107d1;
    *p++ = 0x114011a3;
    *p++ = 0x148511b1;
    *p++ = 0x1d2e211d;
    *p++ = 0x1e951eae;
    *p++ = 0x301e2ec2;
    *p++ = 0x37982911;
    *p++ = 0xe5e6e9c7;
    *p++ = 0xe7baee2d;
    *p++ = 0xf154f1c4;
    *p++ = 0xed8cf33a;
    *p++ = 0xf08ef1bd;
    *p++ = 0xf4b7f56c;
    *p++ = 0xf7a1f656;
    *p++ = 0xf829f897;
    *p++ = 0xfa0df905;
    *p++ = 0xfa51ff06;
    *p++ = 0xfbe0fcba;
    *p++ = 0xfd44fc27;
    *p++ = 0xfba1fd3d;
    *p++ = 0xfc3900fe;
    *p++ = 0xfdf4fded;
    *p++ = 0x00f20181;
    *p++ = 0x0189009f;
    *p++ = 0x039f00c5;
    *p++ = 0x010301c2;
    *p++ = 0x0357011a;
    *p++ = 0x03df03d1;
    *p++ = 0x06d80595;
    *p++ = 0x060906fc;
    *p++ = 0x071705da;
    *p++ = 0x0bb90903;
    *p++ = 0x0d9109df;
    *p++ = 0x0da00f6c;
    *p++ = 0x0ed70d50;
    *p++ = 0x19e61a8d;
    *p++ = 0x18be1734;
    *p++ = 0x24c328ed;
    *p++ = 0x2d2f2446;
    *p++ = 0xebe6ee58;
    *p++ = 0xe8f2eed4;
    *p++ = 0xeffdef6d;
    *p++ = 0xf29bf290;
    *p++ = 0xf311f473;
    *p++ = 0xf389f8ff;
    *p++ = 0xf7d5f7e9;
    *p++ = 0xf95bf86c;
    *p++ = 0xf81ef808;
    *p++ = 0xf7c9fda2;
    *p++ = 0xfacbf971;
    *p++ = 0xfa71fbe9;
    *p++ = 0xfa7ffada;
    *p++ = 0xfd82fda6;
    *p++ = 0xfe410013;
    *p++ = 0xffb50152;
    *p++ = 0x023d01b1;
    *p++ = 0x036c017b;
    *p++ = 0x036d01c4;
    *p++ = 0x045f03a2;
    *p++ = 0x06c505ce;
    *p++ = 0x07c8057e;
    *p++ = 0x06840727;
    *p++ = 0x07bd0530;
    *p++ = 0x09630b87;
    *p++ = 0x0b2409da;
    *p++ = 0x0d720c51;
    *p++ = 0x0fbc0a38;
    *p++ = 0x12a2137b;
    *p++ = 0x13e2137e;
    *p++ = 0x242123bd;
    *p++ = 0x25481bd4;
    *p++ = 0xed7eef87;
    *p++ = 0xee19f4a1;
    *p++ = 0xefadf175;
    *p++ = 0xf155f14a;
    *p++ = 0xf5f3f3ef;
    *p++ = 0xf52dfa59;
    *p++ = 0xf603f6fc;
    *p++ = 0xf74af985;
    *p++ = 0xf711f522;
    *p++ = 0xf755f9e7;
    *p++ = 0xf918f7ee;
    *p++ = 0xf9e6fa0d;
    *p++ = 0xfa46fc4c;
    *p++ = 0xfc0ffe2b;
    *p++ = 0xff5cfe9b;
    *p++ = 0x000201cd;
    *p++ = 0x014e01d8;
    *p++ = 0x0197024b;
    *p++ = 0x04fb032c;
    *p++ = 0x06f0030e;
    *p++ = 0x07590785;
    *p++ = 0x08a706e6;
    *p++ = 0x083f0a00;
    *p++ = 0x09b50828;
    *p++ = 0x0a1c094a;
    *p++ = 0x0a78089e;
    *p++ = 0x0bb40f9f;
    *p++ = 0x0d170952;
    *p++ = 0x122a1068;
    *p++ = 0x11710ea7;
    *p++ = 0x1d30204c;
    *p++ = 0x20de1a1a;
    *p++ = 0xed7ff09d;
    *p++ = 0xec74f51c;
    *p++ = 0xf1edf1cd;
    *p++ = 0xf395f2e7;
    *p++ = 0xf3d3f444;
    *p++ = 0xf5ccf9c1;
    *p++ = 0xf54af348;
    *p++ = 0xf51ff714;
    *p++ = 0xf73ff6f0;
    *p++ = 0xf68ef8ed;
    *p++ = 0xf87af6a7;
    *p++ = 0xf914fc0f;
    *p++ = 0xfbc6fcd1;
    *p++ = 0xfc3afe00;
    *p++ = 0xfea7ffbd;
    *p++ = 0xffea00f4;
    *p++ = 0x01510113;
    *p++ = 0x017301f9;
    *p++ = 0x0586056d;
    *p++ = 0x063e035f;
    *p++ = 0x072005f2;
    *p++ = 0x08e60651;
    *p++ = 0x0af20af0;
    *p++ = 0x0a960773;
    *p++ = 0x0a1c0bf0;
    *p++ = 0x0c0c09c4;
    *p++ = 0x0cc30c66;
    *p++ = 0x0c5a0aa1;
    *p++ = 0x0f9b139e;
    *p++ = 0x11fe0d96;
    *p++ = 0x1d571973;
    *p++ = 0x1c7d184c;
    *p++ = 0xeca9f0a0;
    *p++ = 0xecfdf559;
    *p++ = 0xf310f036;
    *p++ = 0xf292f55f;
    *p++ = 0xf4aff40b;
    *p++ = 0xf53cf7e4;
    *p++ = 0xf50bf536;
    *p++ = 0xf4f9f617;
    *p++ = 0xf4fcf484;
    *p++ = 0xf59cf8d1;
    *p++ = 0xf932f940;
    *p++ = 0xf872fb8b;
    *p++ = 0xfba7fc5e;
    *p++ = 0xfcbbfed7;
    *p++ = 0xff12006f;
    *p++ = 0xff2b01a6;
    *p++ = 0x02d802c8;
    *p++ = 0x01a500ad;
    *p++ = 0x041e02b8;
    *p++ = 0x057302ce;
    *p++ = 0x07f10627;
    *p++ = 0x090806d5;
    *p++ = 0x0b600c27;
    *p++ = 0x0b96082f;
    *p++ = 0x0b490c64;
    *p++ = 0x0a9209b8;
    *p++ = 0x0ba50f30;
    *p++ = 0x0e600b23;
    *p++ = 0x11c60fd6;
    *p++ = 0x10d50b40;
    *p++ = 0x19791ba5;
    *p++ = 0x19e115b6;
    *p++ = 0xeaffefb4;
    *p++ = 0xec27f260;
    *p++ = 0xf36bf172;
    *p++ = 0xf2abf66e;
    *p++ = 0xf4ddf53e;
    *p++ = 0xf437f7a3;
    *p++ = 0xf3c3f2bd;
    *p++ = 0xf583f6c4;
    *p++ = 0xf599f4bb;
    *p++ = 0xf5a6f7ee;
    *p++ = 0xf9f2f9f7;
    *p++ = 0xf745fbbb;
    *p++ = 0xfc9ffcde;
    *p++ = 0xfcf1fdaa;
    *p++ = 0xfeaeff64;
    *p++ = 0xfedc0081;
    *p++ = 0x023b02a3;
    *p++ = 0x015b0170;
    *p++ = 0x04bf0377;
    *p++ = 0x059c03d3;
    *p++ = 0x07a5074d;
    *p++ = 0x084005d8;
    *p++ = 0x0b9b0b05;
    *p++ = 0x0bea08ab;
    *p++ = 0x0ad10d57;
    *p++ = 0x0c5d088e;
    *p++ = 0x0d010edd;
    *p++ = 0x0b1f0b1b;
    *p++ = 0x101110c3;
    *p++ = 0x11e80cdf;
    *p++ = 0x1c2e1a51;
    *p++ = 0x1cd11547;
    *p++ = 0xeaebef12;
    *p++ = 0xeae5f46b;
    *p++ = 0xf319f022;
    *p++ = 0xf2e9f32a;
    *p++ = 0xf5eef47e;
    *p++ = 0xf4def8ad;
    *p++ = 0xf496f3eb;
    *p++ = 0xf50af736;
    *p++ = 0xf5f8f538;
    *p++ = 0xf5f2f871;
    *p++ = 0xf8d9f9bd;
    *p++ = 0xf742faf3;
    *p++ = 0xfc26fd34;
    *p++ = 0xfbeafd32;
    *p++ = 0xffafff64;
    *p++ = 0xfe740103;
    *p++ = 0x01ea013d;
    *p++ = 0x025600e2;
    *p++ = 0x05770565;
    *p++ = 0x05b60420;
    *p++ = 0x080f07d6;
    *p++ = 0x083a05dd;
    *p++ = 0x0a800b1e;
    *p++ = 0x0b40083d;
    *p++ = 0x0cf30d6e;
    *p++ = 0x0b8708d8;
    *p++ = 0x0b190d17;
    *p++ = 0x0b8a0a7c;
    *p++ = 0x122d1345;
    *p++ = 0x11620ea3;
    *p++ = 0x1bb01b84;
    *p++ = 0x1cac14b8;
    *p++ = 0xeb3beff1;
    *p++ = 0xeb74f1c1;
    *p++ = 0xf213f00a;
    *p++ = 0xf1bbf3d7;
    *p++ = 0xf4e1f446;
    *p++ = 0xf463f883;
    *p++ = 0xf62af46b;
    *p++ = 0xf728f8c5;
    *p++ = 0xf69bf5d9;
    *p++ = 0xf586f928;
    *p++ = 0xf935f9b2;
    *p++ = 0xf76cfa97;
    *p++ = 0xfbcefcb5;
    *p++ = 0xfac6fdbb;
    *p++ = 0xff80ff9a;
    *p++ = 0xfdfcfefd;
    *p++ = 0x02990227;
    *p++ = 0x01910189;
    *p++ = 0x04c104c4;
    *p++ = 0x06090401;
    *p++ = 0x098a082a;
    *p++ = 0x07ee05d0;
    *p++ = 0x0b1d0b8f;
    *p++ = 0x0ba5098f;
    *p++ = 0x0b6c0c5d;
    *p++ = 0x099907a5;
    *p++ = 0x0b5a0e4d;
    *p++ = 0x0b4b0894;
    *p++ = 0x110413aa;
    *p++ = 0x12da0f87;
    *p++ = 0x1e221ea9;
    *p++ = 0x1fb41628;
    *p++ = 0xeb54ee32;
    *p++ = 0xea63f0ad;
    *p++ = 0xef33f076;
    *p++ = 0xf068f38e;
    *p++ = 0xf3dcf42b;
    *p++ = 0xf296f627;
    *p++ = 0xf83bf5ba;
    *p++ = 0xf6cffa1d;
    *p++ = 0xf853f7fd;
    *p++ = 0xf827f9e3;
    *p++ = 0xf941f8e9;
    *p++ = 0xf7a7fb4f;
    *p++ = 0xfc36fc90;
    *p++ = 0xfb09fcbc;
    *p++ = 0xffb3feb8;
    *p++ = 0xfe04feac;
    *p++ = 0x03300357;
    *p++ = 0x010b01e5;
    *p++ = 0x05f504ed;
    *p++ = 0x05920357;
    *p++ = 0x078c0861;
    *p++ = 0x07d80677;
    *p++ = 0x09fe0a56;
    *p++ = 0x08fe05d5;
    *p++ = 0x09c80c02;
    *p++ = 0x09ee0950;
    *p++ = 0x0d3d0e21;
    *p++ = 0x0b7d0a81;
    *p++ = 0x1224162a;
    *p++ = 0x11ce0f86;
    *p++ = 0x24e62058;
    *p++ = 0x259a1ad3;
    *p++ = 0xe811eae6;
    *p++ = 0xe707ec55;
    *p++ = 0xecd8ee22;
    *p++ = 0xeec1f2a7;
    *p++ = 0xf2eef096;
    *p++ = 0xf17cf584;
    *p++ = 0xf841f795;
    *p++ = 0xf79bf9ba;
    *p++ = 0xfaa7f9f0;
    *p++ = 0xf92dfb29;
    *p++ = 0xfbe6fc02;
    *p++ = 0xf954fad1;
    *p++ = 0xfbecfbc8;
    *p++ = 0xfb46fe0d;
    *p++ = 0xffa7ff67;
    *p++ = 0xfdca0018;
    *p++ = 0x01f50283;
    *p++ = 0x016c002d;
    *p++ = 0x072e04dc;
    *p++ = 0x05210305;
    *p++ = 0x059b076d;
    *p++ = 0x041f04b8;
    *p++ = 0x074108a1;
    *p++ = 0x06d00423;
    *p++ = 0x0a1f0a79;
    *p++ = 0x08ed06de;
    *p++ = 0x0ea011ba;
    *p++ = 0x0ee10ca5;
    *p++ = 0x17f91933;
    *p++ = 0x171913e1;
    *p++ = 0x27ed24d7;
    *p++ = 0x25481d5d;
    *p++ = 0xe0e3e823;
    *p++ = 0xe23fe9fb;
    *p++ = 0xeb86ea52;
    *p++ = 0xe8c6eeb9;
    *p++ = 0xf27ef047;
    *p++ = 0xf051f4f7;
    *p++ = 0xf55af690;
    *p++ = 0xf65df7ef;
    *p++ = 0xfb01f93a;
    *p++ = 0xf949fab7;
    *p++ = 0xfcd6fc67;
    *p++ = 0xf96efb44;
    *p++ = 0xfd2aff0a;
    *p++ = 0xfc9effc6;
    *p++ = 0x0039fee4;
    *p++ = 0xfccbfe06;
    *p++ = 0x02520129;
    *p++ = 0x0154002b;
    *p++ = 0x0365045a;
    *p++ = 0x02ab01a8;
    *p++ = 0x064c060f;
    *p++ = 0x03ce0438;
    *p++ = 0x08870937;
    *p++ = 0x084f05b1;
    *p++ = 0x0c270d4b;
    *p++ = 0x09f809ad;
    *p++ = 0x14811796;
    *p++ = 0x144c0e37;
    *p++ = 0x1ed11d47;
    *p++ = 0x1c5a1a15;
    *p++ = 0x29ac2c53;
    *p++ = 0x2cd52048;
    *p++ = 0xe0a7e223;
    *p++ = 0xdea3e7d6;
    *p++ = 0xe180e515;
    *p++ = 0xe5dee730;
    *p++ = 0xedb0e940;
    *p++ = 0xed3ef14c;
    *p++ = 0xf3c4f449;
    *p++ = 0xf345f80d;
    *p++ = 0xf962f876;
    *p++ = 0xf806f7ef;
    *p++ = 0xfc7efc92;
    *p++ = 0xf8b4fcb6;
    *p++ = 0xfe55fd90;
    *p++ = 0xfd40fd9e;
    *p++ = 0xff68002b;
    *p++ = 0xfba5fe9a;
    *p++ = 0x01b802bd;
    *p++ = 0x018e0032;
    *p++ = 0x04e9066a;
    *p++ = 0x0347022a;
    *p++ = 0x087e040b;
    *p++ = 0x055c0491;
    *p++ = 0x09f50c5e;
    *p++ = 0x0a350910;
    *p++ = 0x12e7126f;
    *p++ = 0x0ec60d87;
    *p++ = 0x162a1cf0;
    *p++ = 0x177e12ea;
    *p++ = 0x2614240a;
    *p++ = 0x23e5204b;
    *p++ = 0x34bf2f0c;
    *p++ = 0x30e62327;

    ioctl_cmd = MSM_CAM_IOCTL_CONFIG_VFE;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_MESH_ROLL_OFF_CFG, 1784, rocfg,
            ioctl_cmd);
    free(rocfg);
    return rc;
}

static int mm_daemon_config_vfe_fov(mm_daemon_obj_t *mm_obj, int update)
{
    int rc = 0;
    uint32_t *fov_cfg;
    uint32_t *p;
    int cmd;

    fov_cfg = (uint32_t *)malloc(8);
    p = fov_cfg;
    *p++ = 0x50b;
    *p++ = 0x203ca;

    if (update)
        cmd = VFE_CMD_FOV_UPDATE;
    else
        cmd = VFE_CMD_FOV_CFG;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, cmd,
            8, fov_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(fov_cfg);
    return rc;
}

static int mm_daemon_config_vfe_main_scaler(mm_daemon_obj_t *mm_obj, int update)
{
    int rc = 0;
    uint32_t *ms_cfg;
    uint32_t *p;
    int cmd;

    ms_cfg = (uint32_t *)malloc(28);
    p = ms_cfg;
    *p++ = 0x3;
    *p++ = 0x280050c;
    *p++ = 0x3204cc;
    *p++ = 0x0;
    *p++ = 0x1e003c9;
    *p++ = 0x3204cc;
    *p = 0x0;

    if (update)
        cmd = VFE_CMD_MAIN_SCALER_UPDATE;
    else
        cmd = VFE_CMD_MAIN_SCALER_CFG;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, cmd,
            28, ms_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(ms_cfg);
    return rc;
}

static int mm_daemon_config_vfe_s2y(mm_daemon_obj_t *mm_obj, int update)
{
    int rc = 0;
    uint32_t *s2y_cfg;
    uint32_t *p;
    int cmd;
    
    s2y_cfg = (uint32_t *)malloc(20);
    p = s2y_cfg;
    *p++ = 0x00000003;
    *p++ = 0x02800280;
    *p++ = 0x00310000;
    *p++ = 0x01e001e0;
    *p = 0x00310000;

    if (update)
        cmd = VFE_CMD_S2Y_UPDATE;
    else
        cmd = VFE_CMD_S2Y_CFG;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, cmd,
            20, s2y_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(s2y_cfg);
    return rc;
}

static int mm_daemon_config_vfe_s2cbcr(mm_daemon_obj_t *mm_obj, int update)
{
    int rc = 0;
    uint32_t *s2cbcr_cfg;
    uint32_t *p;
    int cmd;

    s2cbcr_cfg = (uint32_t *)malloc(20);
    p = s2cbcr_cfg;
    *p++ = 0x00000003;
    *p++ = 0x01400280;
    *p++ = 0x00320000;
    *p++ = 0x00f001e0;
    *p = 0x00320000;

    if (update)
        cmd = VFE_CMD_S2CbCr_UPDATE;
    else
        cmd = VFE_CMD_S2CbCr_CFG;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, cmd,
            20, s2cbcr_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(s2cbcr_cfg);
    return rc;
}

static int mm_daemon_config_vfe_axi(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    unsigned int ioctl_cmd;
    uint32_t *axiout;
    uint32_t *p;

    axiout = (uint32_t *)malloc(240);
    p = axiout + 1;
    *p++ = 0x3fff;
    *p++ = 0x2aaa771;
    *p++ = 0x1;
    *p++ = 0x1a03;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x12f;
    *p++ = 0x2701df;
    *p++ = 0x501df2;
    *p++ = 0x00;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x1c8012f;
    *p++ = 0x2701df;
    *p++ = 0x501df2;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x1300097;
    *p++ = 0x2700ef;
    *p++ = 0x500ef2;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x2f80097;
    *p++ = 0x2700ef;
    *p++ = 0x500ef2;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x40000;		/* Output0 ch0 & ch1 */
    *p++ = 0x0;			/* Output0 ch2 */
    *p++ = mm_obj->inst_handle;	/* Output0 inst_handle */
    *p++ = 0x0;			/* Output1 ch0 & ch1 */
    *p++ = 0x0;			/* Output1 ch2 */
    *p++ = mm_obj->inst_handle;	/* Output1 inst_handle */
    *p++ = 0x50001;		/* Output2 ch0 & ch1 */
    *p++ = 0x0;			/* Output2 ch2 */
    *p = mm_obj->inst_handle;	/* Output2 inst_handle */
    ioctl_cmd = MSM_CAM_IOCTL_AXI_CONFIG;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_AXI_CFG_PRIM, 0, 240, (void *)axiout,
            ioctl_cmd);
    free(axiout);
    return rc;
}

static int mm_daemon_config_vfe_chroma_en(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *chroma_en;
    uint32_t *p;

    chroma_en = (uint32_t *)malloc(36);
    p = chroma_en;
    *p++ = 0x0000004d;
    *p++ = 0x00000096;
    *p++ = 0x0000001d;
    *p++ = 0x0;
    *p++ = 0x00800080;
    *p++ = 0x0fa90fa9;
    *p++ = 0x00800080;
    *p++ = 0x0fd70fd7;
    *p = 0x00800080;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_CHROMA_EN_CFG,
            36, chroma_en, MSM_CAM_IOCTL_CONFIG_VFE);
    free(chroma_en);
    return rc;
}

static int mm_daemon_config_vfe_color_cor(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *color_cfg;
    uint32_t *p;

    color_cfg = (uint32_t *)malloc(52);
    p = color_cfg;
    *p++ = 0xc2;
    *p++ = 0xff3;
    *p++ = 0xfcc;
    *p++ = 0xf95;
    *p++ = 0xef;
    *p++ = 0xffd;
    *p++ = 0xf7e;
    *p++ = 0xf;
    *p++ = 0xf4;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p = 0x0;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_COLOR_COR_CFG,
            52, color_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(color_cfg);
    return rc;
}

static int mm_daemon_config_vfe_asf(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *asf_cfg;
    uint32_t *p;
    int cmd;

    asf_cfg = (uint32_t *)malloc(48);
    p = asf_cfg;
    *p++ = 0x0000604d;
    *p++ = 0x0008080d;
    *p++ = 0xd828d828;
    *p++ = 0x3c000000;
    *p++ = 0x00408038;
    *p++ = 0x3c000000;
    *p++ = 0x00438008;
    *p++ = 0x02000000;
    *p++ = 0x00608008;
    *p++ = 0x0;
    *p++ = 0x0;
    *p = 0x0;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_ASF_CFG,
            48, asf_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(asf_cfg);
    return rc;
}

static int mm_daemon_config_vfe_white_balance(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *wb_cfg;
    uint32_t *p;

    wb_cfg = (uint32_t *)malloc(4);
    p = wb_cfg;
    *p = 0x33e5a8d;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_WB_CFG,
            4, wb_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(wb_cfg);
    return rc;
}

static int mm_daemon_config_vfe_black_level(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *bl_cfg;
    uint32_t *p;

    bl_cfg = (uint32_t *)malloc(16);
    p = bl_cfg;
    *p++ = 0xf9;
    *p++ = 0xf9;
    *p++ = 0xfe;
    *p = 0xfe;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_BLACK_LEVEL_CFG,
            16, bl_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(bl_cfg);
    return rc;
}

static int mm_daemon_config_vfe_rgb_gamma(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *gamma_cfg;
    uint32_t *p;

    gamma_cfg = (uint32_t *)malloc(132);
    p = gamma_cfg;
    *p++ = 0x0;
    *p++ = 0x0c0d0d00;
    *p++ = 0x0a240b19;
    *p++ = 0x09380a2e;
    *p++ = 0x08490841;
    *p++ = 0x07590851;
    *p++ = 0x06660660;
    *p++ = 0x0572066c;
    *p++ = 0x047d0677;
    *p++ = 0x04860581;
    *p++ = 0x048f058a;
    *p++ = 0x04960393;
    *p++ = 0x039e049a;
    *p++ = 0x04a403a1;
    *p++ = 0x03ab03a8;
    *p++ = 0x03b103ae;
    *p++ = 0x03b602b4;
    *p++ = 0x03bc03b9;
    *p++ = 0x03c102bf;
    *p++ = 0x03c602c4;
    *p++ = 0x02cb02c9;
    *p++ = 0x02d003cd;
    *p++ = 0x02d402d2;
    *p++ = 0x02d903d6;
    *p++ = 0x02dd02db;
    *p++ = 0x02e102df;
    *p++ = 0x02e502e3;
    *p++ = 0x02e902e7;
    *p++ = 0x02ed02eb;
    *p++ = 0x02f102ef;
    *p++ = 0x02f502f3;
    *p++ = 0x02f902f7;
    *p++ = 0x00ff02fb;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_RGB_G_CFG,
            132, gamma_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(gamma_cfg);
    return rc;
}

static int mm_daemon_config_vfe_la_update(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *la_cfg;
    uint32_t *p;

    la_cfg = (uint32_t *)malloc(132);
    p = la_cfg;
    *p = 0x1;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_LA_UPDATE,
            132, la_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(la_cfg);
    return rc;
}

static int mm_daemon_config_vfe_camif(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *camif_cfg;
    uint32_t *p;

    camif_cfg = (uint32_t *)malloc(32);
    p = camif_cfg;
    *p++ = 0x100;
    *p++ = 0x00;
    *p++ = 0x3d40518;
    *p++ = 0x517;
    *p++ = 0x3d3;
    *p++ = 0xffffffff;
    *p++ = 0x00;
    *p++ = 0x3fff3fff;
    
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_CAMIF_CFG,
            32, camif_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(camif_cfg);
    return rc;
}

static int mm_daemon_config_vfe_demux(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *demux_cfg;
    uint32_t *p;

    demux_cfg = (uint32_t *)malloc(20);
    p = demux_cfg;
    *p++ = 0x1;
    *p++ = 0x800080;
    *p++ = 0x800080;
    *p++ = 0x9c;
    *p++ = 0xca;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_DEMUX_CFG,
            20, demux_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(demux_cfg);
    return rc;
}

static int mm_daemon_config_vfe_out_clamp(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *clamp_cfg;
    uint32_t *p;

    clamp_cfg = (uint32_t *)malloc(8);
    p = clamp_cfg;
    *p++ = 0xffffff;
    *p = 0x0;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_OUT_CLAMP_CFG,
            8, clamp_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(clamp_cfg);
    return rc;
}

static int mm_daemon_config_vfe_frame_skip(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *skip_cfg;
    uint32_t *p;

    skip_cfg = (uint32_t *)malloc(32);
    p = skip_cfg;
    *p++ = 0x1f;
    *p++ = 0x1f;
    *p++ = 0xffffffff;
    *p++ = 0xffffffff;
    *p++ = 0x1f;
    *p++ = 0x1f;
    *p++ = 0xffffffff;
    *p++ = 0xffffffff;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_FRAME_SKIP_CFG,
            32, skip_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(skip_cfg);
    return rc;
}

static int mm_daemon_config_vfe_chroma_subs(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *chroma_cfg;
    uint32_t *p;

    chroma_cfg = (uint32_t *)malloc(12);
    p = chroma_cfg;
    *p++ = 0x30;
    *p++ = 0x0;
    *p = 0x0;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_CHROMA_SUBS_CFG,
            12, chroma_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(chroma_cfg);
    return rc;
}

static int mm_daemon_config_vfe_sk_enhance(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *sk_cfg;
    uint32_t *p;

    sk_cfg = (uint32_t *)malloc(136);
    p = sk_cfg;
    *p++ = 0x110a28;
    *p++ = 0x461128;
    *p++ = 0x414628;
    *p++ = 0x2d4128;
    *p++ = 0xa2d28;
    *p++ = 0xfdece2;
    *p++ = 0xe7fde2;
    *p++ = 0xc9e7e2;
    *p++ = 0xa6c9e2;
    *p++ = 0xeca6e2;
    *p++ = 0x400;
    *p++ = 0x400;
    *p++ = 0x400;
    *p++ = 0x400;
    *p++ = 0x400;
    *p++ = 0x400;
    *p++ = 0x4000000;
    *p++ = 0x4000000;
    *p++ = 0x4000000;
    *p++ = 0x4000000;
    *p++ = 0x4000000;
    *p++ = 0x4000000;
    *p++ = 0xa00000;
    *p++ = 0xa00000;
    *p++ = 0xa00000;
    *p++ = 0xa00000;
    *p++ = 0xa00000;
    *p++ = 0xa00000;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p = 0x0;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_SK_ENHAN_CFG,
            136, sk_cfg, MSM_CAM_IOCTL_CONFIG_VFE);
    free(sk_cfg);
    return rc;
}

static int mm_daemon_config_vfe_op_mode(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    mm_daemon_vfe_op vfeop;

    memset(&vfeop, 0, sizeof(vfeop));
    vfeop.operation_mode = VFE_OUTPUTS_PREVIEW_AND_VIDEO;
    vfeop.cfg = 529;
    vfeop.module_cfg = 0x1e27c17;
    vfeop.stats_cfg = 1;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_OPERATION_CFG,
            sizeof(vfeop), &vfeop, MSM_CAM_IOCTL_CONFIG_VFE);
    return rc;
}

static int mm_daemon_config_vfe_stats_reqbuf(mm_daemon_obj_t *mm_obj,
        int stats_type, int num_buf)
{
    int rc = 0;
    struct msm_stats_reqbuf reqbuf;

    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.num_buf = num_buf;
    reqbuf.stats_type = stats_type;
    rc = ioctl(mm_obj->cfg_fd, MSM_CAM_IOCTL_STATS_REQBUF, &reqbuf);
    if (rc == 0)
        mm_obj->stats_buf[stats_type].buf_cnt = num_buf;
    return rc;
}

static int mm_daemon_config_vfe_stats_enqueuebuf(mm_daemon_obj_t *mm_obj,
        int stats_type, int buf_idx)
{
    int rc = 0;
    struct msm_stats_buf_info buf_info;
    struct ion_allocation_data alloc_data;
    struct ion_fd_data fd_data;
    void *base = 0;
    int i;

    memset(&buf_info, 0, sizeof(buf_info));
    if (!mm_obj->stats_buf[stats_type].allocated) {
        if (mm_obj->ion_fd < 0)
            return -1;
        for (i = 0; i < mm_obj->stats_buf[stats_type].buf_cnt; i++) {
            memset(&buf_info, 0, sizeof(buf_info));
            memset(&alloc_data, 0, sizeof(alloc_data));
            memset(&fd_data, 0, sizeof(fd_data));

            alloc_data.len = 4096;
            alloc_data.align = 4096;
            alloc_data.flags = ((0x1 << ION_CP_MM_HEAP_ID) |
                    (0x1 << ION_CAMERA_HEAP_ID));
            alloc_data.heap_mask = alloc_data.flags;
            rc = ioctl(mm_obj->ion_fd, ION_IOC_ALLOC, &alloc_data);
            if (rc < 0) {
                ALOGI("%s: Error allocating stat heap", __FUNCTION__);
                return rc;
            }
            fd_data.handle = alloc_data.handle;
            if (ioctl(mm_obj->ion_fd, ION_IOC_SHARE, &fd_data)) {
                rc = -errno;
                ALOGE("%s: ION_IOC_SHARE failed with error %s",
                        __FUNCTION__, strerror(errno));
                ioctl(mm_obj->ion_fd, ION_IOC_FREE, &alloc_data);
                return rc;
            }
            base = mmap(0, alloc_data.len, PROT_READ|PROT_WRITE, MAP_SHARED,
                    fd_data.fd, 0);
            if (base == MAP_FAILED) {
                struct ion_handle_data handle_data;

                handle_data.handle = alloc_data.handle;
                rc = -errno;
                ALOGE("%s: Failed to map the allocated memory: %s",
                        __FUNCTION__, strerror(errno));
                ioctl(mm_obj->ion_fd, ION_IOC_FREE, &handle_data);
                return rc;
            }
            memset(base, 0, alloc_data.len);
            mm_obj->stats_buf[stats_type].buf_data[i].len = alloc_data.len;
            mm_obj->stats_buf[stats_type].buf_data[i].fd = fd_data.fd;
            mm_obj->stats_buf[stats_type].buf_data[i].vaddr = base;
            mm_obj->stats_buf[stats_type].buf_data[i].handle = alloc_data.handle;
            mm_obj->stats_buf[stats_type].stats_cnt++;
	    buf_info.vaddr = fd_data.handle;
            buf_info.fd = fd_data.fd;
            buf_info.type = stats_type;
            buf_info.buf_idx = i;
            rc = ioctl(mm_obj->cfg_fd, MSM_CAM_IOCTL_STATS_ENQUEUEBUF, &buf_info);
        }
        mm_obj->stats_buf[stats_type].allocated = 1;
    } else {
            buf_info.type = stats_type;
            buf_info.buf_idx = buf_idx;
            rc = ioctl(mm_obj->cfg_fd, MSM_CAM_IOCTL_STATS_ENQUEUEBUF, &buf_info);
    }
    return rc;
}

static int mm_daemon_config_vfe_stats_flush_bufq(mm_daemon_obj_t *mm_obj,
        int stats_type)
{
    int rc = 0;
    struct msm_stats_flush_bufq flush_bufq;

    flush_bufq.stats_type = stats_type;
    rc = ioctl(mm_obj->cfg_fd, MSM_CAM_IOCTL_STATS_FLUSH_BUFQ, &flush_bufq);
    return rc;
}

static void mm_daemon_config_vfe_stats_unregbuf(mm_daemon_obj_t *mm_obj)
{
    struct msm_stats_reqbuf reqbuf;
    struct ion_handle_data handle_data;
    int i, j;

    for (i = 0; i < MSM_STATS_TYPE_MAX; i++) {
        if (mm_obj->stats_buf[i].allocated) {
            reqbuf.num_buf = mm_obj->stats_buf[i].buf_cnt;
            reqbuf.stats_type = i;
            ioctl(mm_obj->cfg_fd, MSM_CAM_IOCTL_STATS_UNREG_BUF, &reqbuf);
            for (j = 0; j < mm_obj->stats_buf[i].stats_cnt; j++) {
                if (munmap(mm_obj->stats_buf[i].buf_data[j].vaddr,
                        mm_obj->stats_buf[i].buf_data[j].len))
                    ALOGE("%s: Failed to unmap memory at %p", __FUNCTION__,
                            mm_obj->stats_buf[i].buf_data[j].vaddr);
                handle_data.handle =
                        mm_obj->stats_buf[i].buf_data[j].handle;
                ioctl(mm_obj->ion_fd, ION_IOC_FREE, &handle_data);
            }
            mm_obj->stats_buf[i].allocated = 0;
            mm_obj->stats_buf[i].stats_cnt = 0;
        }
    }
}
    
static int mm_daemon_config_vfe_stats_ae(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *ae_stats;
    uint32_t *p;
    unsigned int ioctl_cmd;

    ae_stats = (uint32_t *)malloc(8);
    p = ae_stats;
    *p++ = 0x50000000;
    *p = 0xff03b04f;
    ioctl_cmd = MSM_CAM_IOCTL_CONFIG_VFE;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_STATS_AE_START, 8,
            ae_stats, ioctl_cmd);
    free(ae_stats);
    return rc;
}

static int mm_daemon_config_vfe_stats_awb(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *awb_stats;
    uint32_t *p;

    awb_stats = (uint32_t *)malloc(32);
    p = awb_stats;
    *p++ = 0x50000000;
    *p++ = 0xff03b04f;
    *p++ = 0x00000af1;
    *p++ = 0x0098005a;
    *p++ = 0x01010f9d;
    *p++ = 0xf010f010;
    *p++ = 0x4021203d;
    *p = 0x4009d082;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_STATS_AWB_START, 32,
            awb_stats, MSM_CAM_IOCTL_CONFIG_VFE);
    free(awb_stats);
    return rc;
}

static int mm_daemon_config_vfe_stats_af(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *af_stats;
    uint32_t *p;

    af_stats = (uint32_t *)malloc(16);
    p = af_stats;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p = 0x0;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_STATS_AF_START, 16,
            af_stats, MSM_CAM_IOCTL_CONFIG_VFE);
    free(af_stats);
    return rc;
}

static int mm_daemon_config_vfe_stats_rs(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *rs_stats;
    uint32_t *p;

    rs_stats = (uint32_t *)malloc(8);
    p = rs_stats;
    *p++ = 0x0;
    *p = 0x0;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_STATS_RS_START, 8,
            rs_stats, MSM_CAM_IOCTL_CONFIG_VFE);
    free(rs_stats);
    return rc;
}

static int mm_daemon_config_vfe_stats_cs(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *cs_stats;
    uint32_t *p;

    cs_stats = (uint32_t *)malloc(8);
    p = cs_stats;
    *p++ = 0x0;
    *p = 0x0;
    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_STATS_CS_START, 8,
            cs_stats, MSM_CAM_IOCTL_CONFIG_VFE);
    free(cs_stats);
    return rc;
}

static int mm_daemon_config_vfe_main_scaler_update(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *ms_updt;
    uint32_t *p;

    ms_updt = (uint32_t *)malloc(28);
    p = ms_updt;
    *p++ = 0x3;
    *p++ = 0x280050c;
    *p++ = 0x3204cc;
    *p++ = 0x0;
    *p++ = 0x1e003c9;
    *p++ = 0x3204cc;
    *p = 0x0;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_MAIN_SCALER_UPDATE,
            28, ms_updt, MSM_CAM_IOCTL_CONFIG_VFE);
    free(ms_updt);
    return rc;
}

static int mm_daemon_config_vfe_s2y_update(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *s2y_updt;
    uint32_t *p;

    s2y_updt = (uint32_t *)malloc(20);
    p = s2y_updt;
    *p++ = 0x3;
    *p++ = 0x2800280;
    *p++ = 0x310000;
    *p++ = 0x1e001e0;
    *p = 0x310000;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_S2Y_UPDATE,
            20, s2y_updt, MSM_CAM_IOCTL_CONFIG_VFE);
    free(s2y_updt);
    return rc;
}

static int mm_daemon_config_vfe_asf_update(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *asf_updt;
    uint32_t *p;

    asf_updt = (uint32_t *)malloc(36);
    p = asf_updt;
    *p++ = 0x8005;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x3c000000;
    *p++ = 0x408038;
    *p++ = 0x3c000000;
    *p++ = 0x438008;
    *p++ = 0x0;
    *p = 0x0;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_ASF_UPDATE,
            36, asf_updt, MSM_CAM_IOCTL_CONFIG_VFE);
    free(asf_updt);
    return rc;
}

int mm_daemon_config_vfe_chroma_sup(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *chroma_sup;
    uint32_t *p;

    chroma_sup = (uint32_t *)malloc(12);
    p = chroma_sup;
    *p++ = 0xf0b40a05;
    *p++ = 0x01524466;
    *p = 0x03441e0f;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_CHROMA_SUP_UPDATE,
            12, chroma_sup, MSM_CAM_IOCTL_CONFIG_VFE);
    free(chroma_sup);
    return rc;
}

int mm_daemon_config_vfe_mce(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    uint32_t *mce, *p;

    mce = (uint32_t *)malloc(44);
    p = mce;
    *p++ = 0xebc81e0a;
    *p++ = 0x11220000;
    *p++ = 0x001a7f2d;
    *p++ = 0xebc82814;
    *p++ = 0x86aa8000;
    *p++ = 0x0013f400;
    *p++ = 0xffeb9650;
    *p++ = 0x86aa8000;
    *p = 0x001319f4;

    rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_MCE_UPDATE,
            44, mce, MSM_CAM_IOCTL_CONFIG_VFE);
    free(mce);
    return rc;
}

int mm_daemon_config_exp_gain(mm_daemon_obj_t *mm_obj, uint16_t gain,
        uint32_t line)
{
    int rc = 0;
    struct sensor_cfg_data cdata;

    cdata.cfgtype = CFG_SET_EXP_GAIN;
    cdata.cfg.exp_gain.gain = gain;
    cdata.cfg.exp_gain.line = line;
    rc = ioctl(mm_obj->cfg_fd, MSM_CAM_IOCTL_SENSOR_IO_CFG, &cdata);
    return rc;
}

static int mm_daemon_config_process_stats(mm_daemon_obj_t *mm_obj,
        struct msm_stats_buf *stats_buf,
        enum msm_stats_enum_type stats_type)
{
    int rc = 0;
    int i;
    uint32_t stat = 0;
    unsigned char *stat_ptr;

    switch (stats_type) {
        case MSM_STATS_TYPE_AEC:
            if (stats_buf->buf_idx == 2) {
                stat_ptr = (unsigned char *)mm_obj->stats_buf[stats_type]
                        .buf_data[stats_buf->buf_idx].vaddr;
                for (i = 0; i < MM_DAEMON_STATS_AEC_LEN; i++) {
                    stat += stat_ptr[i];
                }
                mm_obj->exp_level = (stat / MM_DAEMON_STATS_AEC_LEN);
                if (mm_obj->exp_level > 120)
                    mm_daemon_config_exp_gain(mm_obj, mm_obj->curr_gain--, 0x3d4);
                else if (mm_obj->exp_level < 50)
                    mm_daemon_config_exp_gain(mm_obj, mm_obj->curr_gain++, 0x3d4);
                
            }
            mm_daemon_config_vfe_stats_enqueuebuf(mm_obj, stats_type, stats_buf->buf_idx);
            break;
        case MSM_STATS_TYPE_AWB:
            mm_daemon_config_vfe_stats_enqueuebuf(mm_obj, stats_type, stats_buf->buf_idx);
            break;
        default:
            ALOGE("%s: unknown stats type %d", __FUNCTION__, stats_type);
    }
    return rc;
}

static int mm_daemon_config_isp_evt(mm_daemon_obj_t *mm_obj,
        struct msm_isp_event_ctrl *isp_evt)
{
    int rc = 0, i;
    unsigned int ioctl_cmd;
    struct msm_mctl_pp_frame_buffer pp_buffer[3];

    switch (isp_evt->isp_data.isp_msg.msg_id) {
        case MSG_ID_RESET_ACK: { /* 0 */
            memset(&pp_buffer, 0, sizeof(pp_buffer));
            mm_daemon_config_exp_gain(mm_obj, 0x34, 0x1e7);
            mm_daemon_config_vfe_roll_off(mm_obj);
            mm_daemon_config_vfe_fov(mm_obj, 0);
            mm_daemon_config_vfe_main_scaler(mm_obj, 0);
            rc = mm_daemon_config_vfe_s2y(mm_obj, 0);
            rc = mm_daemon_config_vfe_s2cbcr(mm_obj, 0);
            rc = mm_daemon_config_vfe_axi(mm_obj);
            if (rc < 0)
                break;
            mm_daemon_config_vfe_chroma_en(mm_obj);
            mm_daemon_config_vfe_color_cor(mm_obj);
            mm_daemon_config_vfe_asf(mm_obj);
            mm_daemon_config_vfe_white_balance(mm_obj);
            mm_daemon_config_vfe_black_level(mm_obj);
            mm_daemon_config_vfe_rgb_gamma(mm_obj);
            mm_daemon_config_vfe_la_update(mm_obj);
            mm_daemon_config_vfe_camif(mm_obj);
            mm_daemon_config_vfe_demux(mm_obj);
            mm_daemon_config_vfe_out_clamp(mm_obj);
            mm_daemon_config_vfe_frame_skip(mm_obj);
            mm_daemon_config_vfe_chroma_subs(mm_obj);
            mm_daemon_config_vfe_sk_enhance(mm_obj);
            rc = mm_daemon_config_vfe_op_mode(mm_obj);
            if (rc < 0)
                break;
            ioctl_cmd = MSM_CAM_IOCTL_CONFIG_VFE;
            mm_daemon_config_vfe_stats_reqbuf(mm_obj, MSM_STATS_TYPE_AEC, 3);
            mm_daemon_config_vfe_stats_reqbuf(mm_obj, MSM_STATS_TYPE_AWB, 3);

            mm_daemon_config_vfe_stats_enqueuebuf(mm_obj, MSM_STATS_TYPE_AEC, 0);
            mm_daemon_config_vfe_stats_enqueuebuf(mm_obj, MSM_STATS_TYPE_AWB, 0);

            mm_daemon_config_vfe_stats_ae(mm_obj);
            mm_daemon_config_vfe_stats_awb(mm_obj);
            ALOGI("starting vfe", __FUNCTION__);
            rc = mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_START, 0,
                    NULL, ioctl_cmd);
            if (rc < 0)
                break;
            break;
        }
        case MSG_ID_START_ACK: /* 1 */
            mm_daemon_config_vfe_fov(mm_obj, 1);
            mm_daemon_config_vfe_main_scaler(mm_obj, 1);
            mm_daemon_config_vfe_s2y(mm_obj, 1);
            mm_daemon_config_vfe_s2cbcr(mm_obj, 1);
            mm_daemon_config_vfe_asf_update(mm_obj);
            mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_UPDATE, 0,
                    NULL, MSM_CAM_IOCTL_CONFIG_VFE);
            break;
        case MSG_ID_UPDATE_ACK: /* 3 */
            break;
        case MSG_ID_STATS_AEC: {/* 9 */
            struct msm_stats_buf *stats_buf = isp_evt->isp_data.isp_msg.data;
            mm_daemon_config_process_stats(mm_obj, stats_buf, MSM_STATS_TYPE_AEC);
            break;
        }
        case MSG_ID_STATS_AWB: {/* 11 */
            struct msm_stats_buf *stats_buf = isp_evt->isp_data.isp_msg.data;
            mm_daemon_config_process_stats(mm_obj, stats_buf, MSM_STATS_TYPE_AWB);
            break;
        }
        case MSG_ID_CAMIF_ERROR: /* 35 */
            ALOGE("Camera interface errors detected", __FUNCTION__);
            break;
        case MSG_ID_SOF_ACK: /* 37 */
            break;
	case MSG_ID_OUTPUT_PRIMARY: { /* 40 */
            mm_daemon_config_vfe_chroma_sup(mm_obj);
            mm_daemon_config_vfe_mce(mm_obj);
            mm_daemon_config_vfe_cmd(mm_obj, CMD_GENERAL, VFE_CMD_UPDATE, 0,
                    NULL, MSM_CAM_IOCTL_CONFIG_VFE);
            break;
        }
        default:
            ALOGI("%s Unknown event %d", __FUNCTION__,
                    isp_evt->isp_data.isp_msg.msg_id);
            break;
    }
    return rc;
}

static int mm_daemon_config_dequeue(mm_daemon_obj_t *mm_obj)
{
    struct v4l2_event ev;
    struct msm_isp_event_ctrl isp_evt;
    uint8_t cam_evt_data[512];
    int rc = 0;

    memset(&ev, 0, sizeof(ev));
    memset(&isp_evt, 0, sizeof(isp_evt));
    memset(&cam_evt_data, 0, 512);
    isp_evt.isp_data.isp_msg.data = (uint32_t *)&cam_evt_data;

    *((uint32_t *)ev.u.data) = (uint32_t *)&isp_evt;
    rc = ioctl(mm_obj->cfg_fd, VIDIOC_DQEVENT, &ev);
    if (rc < 0) {
        ALOGE("%s: Error dequeueing event", __FUNCTION__);
        mm_obj->cfg_state = STATE_STOPPED;
        return rc;
    }

    rc = mm_daemon_config_isp_evt(mm_obj, &isp_evt);

    return rc;
}

static int mm_daemon_config_subscribe(mm_daemon_obj_t *mm_obj, int subscribe)
{
    int rc = 0;
    struct v4l2_event_subscription sub;

    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_PRIVATE_START + MSM_CAM_RESP_STAT_EVT_MSG;
    if (subscribe == 0) {
        rc = ioctl(mm_obj->cfg_fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub);
        if (rc < 0) {
            ALOGE("%s: unsubscribe event rc=%d", __FUNCTION__, rc);
            return rc;
        }
    } else {
        rc = ioctl(mm_obj->cfg_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
        if (rc < 0) {
            ALOGE("%s: subscribe event rc=%d", __FUNCTION__, rc);
            return rc;
        }
    }
    return rc;
}

static void *mm_daemon_config_thread(void *data)
{
    int i, subscribed;
    struct pollfd fds[2];
    mm_daemon_obj_t *mm_obj = (mm_daemon_obj_t *)data;

    subscribed = 1;
    if (mm_daemon_config_subscribe(mm_obj, subscribed) < 0)
        return NULL;

    mm_obj->cfg_state = STATE_POLL;
    do {
        for (i = 0; i < 2; i++) {
            fds[i].fd = mm_obj->cfg_fd;
            fds[i].events = POLLIN|POLLRDNORM|POLLPRI;
        }
        if (poll(fds, 2, -1) > 0) {
            if (fds[0].revents & POLLPRI)
                mm_daemon_config_dequeue(mm_obj);
            else
                usleep(1000);
        } else {
            usleep(100);
            continue;
        }
    } while (mm_obj->cfg_state == STATE_POLL);
    return NULL;
}

int mm_daemon_config_start(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;
    rc = pipe(mm_obj->cfg_pfds);
    if (rc < 0)
        return rc;
    pthread_create(&mm_obj->cfg_pid, NULL, mm_daemon_config_thread, (void *)mm_obj);
    return rc;
}

int mm_daemon_config_stop(mm_daemon_obj_t *mm_obj)
{
    int rc = 0;

    mm_daemon_config_vfe_stats_unregbuf(mm_obj);
    mm_obj->cfg_state = STATE_STOPPED;
    return rc;
}
