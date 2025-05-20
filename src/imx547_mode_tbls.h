/*
 * imx547_mode_tbls.h - imx547 sensor mode tables
 *
 * Copyright (c) 2022. FRAMOS.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __IMX547_TABLES__
#define __IMX547_TABLES__

/**
 * Image sensor registers as described in the IMX547 register map
 */

#define STANDBY             0x3000
#define XMSTA               0x3010

#define INCKSEL_ST0         0x3014
#define INCKSEL_ST1         0x3015
#define INCKSEL_ST2         0x3016
#define INCKSEL_ST3         0x3018
#define INCKSEL_ST4         0x3019
#define INCKSEL_ST5         0x301B
#define REGHOLD             0x3034
#define HVMODE              0x303C
#define VOPB_VBLK_HWID_LOW  0x30D0
#define VOPB_VBLK_HWID_HIGH 0x30D1
#define FINFO_HWIDTH_LOW    0x30D2
#define FINFO_HWIDTH_HIGH   0x30D3

#define VMAX_LOW            0x30D4
#define VMAX_MID            0x30D5
#define VMAX_HIGH           0x30D6
#define HMAX_LOW            0x30D8
#define HMAX_HIGH           0x30D9
#define FREQ                0x30DC
#define GMRWT               0x30E2
#define GMTWT               0x30E3
#define GAINDLY             0x30E5
#define GSDLY               0x30E6


#define ADBIT               0x3200
#define HREVERSE_VREVERSE   0x3204

#define INCKSEL_N0          0x321C
#define INCKSEL_N1          0x321D
#define INCKSEL_N2          0x321E
#define INCKSEL_N3          0x321F

#define INCKSEL_S0          0x3220
#define INCKSEL_S1          0x3221
#define INCKSEL_S2          0x3222
#define INCKSEL_S3          0x3223

#define INCKSEL_D0          0x3224
#define INCKSEL_D1          0x3225
#define INCKSEL_D2          0x3226
#define INCKSEL_D3          0x3227

#define SLVS_EN             0x322B
#define LLBLANK_LOW         0x323C
#define LLBLANK_HIGH        0x323D
#define VINT_EN             0x323E

#define SHS_LOW             0x3240
#define SHS_MID             0x3241
#define SHS_HIGH            0x3242

#define TRIGMODE            0x3400
#define ODBIT               0x3430
#define SYNCSEL             0x343C
#define STBSLVS             0x3444

#define GAIN_RTS            0x3502
#define GAIN_LOW            0x3514
#define GAIN_HIGH           0x3515
#define BLKLEVEL_LOW        0x35B4
#define BLKLEVEL_HIGH       0x35B5

#define LANESEL             0x3904
#define IDLECODE1_LOW       0x3934 
#define IDLECODE1_HIGH      0x3935 
#define IDLECODE2_LOW       0x3936 
#define IDLECODE2_HIGH      0x3937 
#define IDLECODE3_LOW       0x3938 
#define IDLECODE3_HIGH      0x3939 
#define IDLECODE4_LOW       0x393A 
#define IDLECODE4_HIGH      0x393B 

#define CRC_ECC_MODE        0x3A00

/**
 * Resolutions of implemented frame modes
 */

#define IMX547_DEFAULT_WIDTH        2472
#define IMX547_DEFAULT_HEIGHT       2064

/**
 * Special values for the write table function
 */
#define IMX547_TABLE_WAIT_MS 0
#define IMX547_TABLE_END     1
#define IMX547_WAIT_MS       10

#define IMX547_MIN_FRAME_DELTA  144

#define IMX547_TO_LOW_BYTE(x) (x & 0xFF)
#define IMX547_TO_MID_BYTE(x) (x >> 8)

/*
 * imx547 I2C operation related structure
 */
struct reg_8 {
    u16 addr;
    u8 val;
};

typedef struct reg_8 imx547_reg;

/**
 * Tables for the write table function
 */

static const imx547_reg imx547_stop[] = {

    {STANDBY,              0x01},
    {IMX547_TABLE_WAIT_MS, IMX547_WAIT_MS},
    {XMSTA,                0x01},

    {IMX547_TABLE_WAIT_MS, 30},
    {IMX547_TABLE_END,     0x00}
};

static const imx547_reg imx547_10bit_mode[] = {

    {HMAX_LOW,      IMX547_TO_LOW_BYTE(274)}, 
    {HMAX_HIGH,     IMX547_TO_MID_BYTE(274)}, 
    {VMAX_LOW,      IMX547_TO_LOW_BYTE(2216)}, 
    {VMAX_MID,      IMX547_TO_MID_BYTE(2216)}, 

    {GMRWT,     0x08},
    {GMTWT,     0x32},
    {GAINDLY,   0x02},
    {GSDLY,     0x08},

    {ADBIT,     0x05},
    {ODBIT,     0x00},

    {0x35A4,    0x1C},
    {0x35A8,    0x1C},
    {0x35EC,    0x1C},

    {0x362C,    0x1C},
    {0x362E,    0xEB},
    {0x362F,    0x1F},
    {0x3654,    0x1C},
    {0x3656,    0xEB},
    {0x3657,    0x1F},
    {0x367C,    0x1C},
    {0x367E,    0xEB},
    {0x367F,    0x1F},
    {0x36E8,    0x11},

    {0x4056,    0x0F},
    {0x4096,    0x0F},

    {0x4460,    0x6C},

    {0x45E6,    0x53},
    {0x45F0,    0x90},
    {0x45F2,    0x8A},
    {0x45F8,    0x8E},
    {0x45FA,    0x90},


    {0x4604,    0x8E},
    {0x4606,    0x90},
    {0x460C,    0x8A},
    {0x460E,    0xBB},
    {0x4614,    0x90},
    {0x4616,    0x8A},
    {0x4634,    0x4A},
    {0x4636,    0x90},
    {0x463C,    0x4C},
    {0x463E,    0x92},
    {0x4644,    0x4E},
    {0x4646,    0x94},
    {0x464C,    0x47},
    {0x464E,    0x4D},
    {0x4654,    0x49},
    {0x4656,    0x50},
    {0x465C,    0x4B},
    {0x465E,    0x52},
    {0x466A,    0x9E},
    {0x4670,    0x98},
    {0x4676,    0x96},
    {0x4678,    0xBA},
    {0x4698,    0x93},
    {0x469A,    0xB9},

    {0x4728,    0xD4},
    {0x4729,    0x0E},
    {0x472E,    0x05},
    {0x472F,    0x04},
    {0x4730,    0x04},
    {0x4731,    0x04},

    {0x4900,    0x64},
    {0x4908,    0x6E},

    {IMX547_TABLE_WAIT_MS, IMX547_WAIT_MS},
    {IMX547_TABLE_END,     0x00}
};

static const imx547_reg imx547_12bit_mode[] = {

    {HMAX_LOW,      IMX547_TO_LOW_BYTE(408)}, 
    {HMAX_HIGH,     IMX547_TO_MID_BYTE(408)}, 
    {VMAX_LOW,      IMX547_TO_LOW_BYTE(2208)}, 
    {VMAX_MID,      IMX547_TO_MID_BYTE(2208)}, 

    {GMRWT,     0x06},
    {GMTWT,     0x24},
    {GAINDLY,   0x02},
    {GSDLY,     0x10},

    {ADBIT,     0x15},
    {ODBIT,     0x01},

    {0x35A4,    0x08},
    {0x35A8,    0x08},
    {0x35EC,    0x08},

    {0x362C,    0x64},
    {0x362E,    0x00},
    {0x362F,    0x00},
    {0x3654,    0x64},
    {0x3656,    0x20},
    {0x3657,    0x00},
    {0x367C,    0x64},
    {0x367E,    0x00},
    {0x367F,    0x00},
    {0x36E8,    0x13},

    {0x4056,    0x23},
    {0x4096,    0x23},

    {0x4460,    0x6E},

    {0x45E6,    0x3F},
    {0x45F0,    0x95},
    {0x45F2,    0x8F},
    {0x45F8,    0x93},
    {0x45FA,    0x95},


    {0x4604,    0x93},
    {0x4606,    0x95},
    {0x460C,    0x8F},
    {0x460E,    0xC0},
    {0x4614,    0x95},
    {0x4616,    0x8F},
    {0x4634,    0x36},
    {0x4636,    0x95},
    {0x463C,    0x38},
    {0x463E,    0x97},
    {0x4644,    0x3A},
    {0x4646,    0x99},
    {0x464C,    0x33},
    {0x464E,    0x39},
    {0x4654,    0x35},
    {0x4656,    0x3C},
    {0x465C,    0x37},
    {0x465E,    0x3E},
    {0x466A,    0xA3},
    {0x4670,    0x9D},
    {0x4676,    0x9B},
    {0x4678,    0xBF},
    {0x4698,    0x98},
    {0x469A,    0xBE},

    {0x4728,    0xFB},
    {0x4729,    0x07},
    {0x472E,    0x06},
    {0x472F,    0x06},
    {0x4730,    0x06},
    {0x4731,    0x06},

    {0x4900,    0x6C},
    {0x4908,    0x68},

    {IMX547_TABLE_WAIT_MS, IMX547_WAIT_MS},
    {IMX547_TABLE_END,     0x00}
};

static const imx547_reg imx547_common_settings[] = {

    {FREQ,          0x00},
    {INCKSEL_ST0,   0x0A},
    {INCKSEL_ST1,   0x22},
    {INCKSEL_ST2,   0xB1},
    {INCKSEL_ST3,   0x40},
    {INCKSEL_ST4,   0x04},
    {INCKSEL_ST5,   0x3A},

    {INCKSEL_N0,    0x80},
    {INCKSEL_N1,    0x05},
    {INCKSEL_N2,    0xE0},
    {INCKSEL_N3,    0x00},

    {INCKSEL_S0,    0x80},
    {INCKSEL_S1,    0x05},
    {INCKSEL_S2,    0xE0},
    {INCKSEL_S3,    0x00},

    {INCKSEL_D0,    0x10},
    {INCKSEL_D1,    0x14},
    {INCKSEL_D2,    0x20},
    {INCKSEL_D3,    0xC0},

    {SLVS_EN,       0x02},
    {LLBLANK_LOW,   0x19},
    {VINT_EN,       0x33},
    {CRC_ECC_MODE,  0xD1},
    {VOPB_VBLK_HWID_LOW,    0xA8},
    {VOPB_VBLK_HWID_HIGH,   0x09},
    {FINFO_HWIDTH_LOW,      0xA8},
    {FINFO_HWIDTH_HIGH,     0x09},
    {IDLECODE1_LOW,     0x3C},
    {IDLECODE1_HIGH,    0x01},
    {IDLECODE2_LOW,     0xBC},
    {IDLECODE2_HIGH,    0x01},
    {IDLECODE3_LOW,     0x3C},
    {IDLECODE3_HIGH,    0x01},
    {IDLECODE4_LOW,     0x3C},
    {IDLECODE4_HIGH,    0x01},

    {HVMODE,    0x03},
    {LANESEL,   0x03},

    {GAIN_RTS,  0x09},
    {SYNCSEL,   0xF0}, 

    {0x3004,    0xA8},
    {0x3005,    0x02},

    {0x3233,    0x00},

    {0x3521,    0x3D},
    {0x3535,    0x00},
    {0x3542,    0x27},
    {0x3546,    0x0F},
    {0x354A,    0x20},
    {0x359C,    0x0F},
    {0x359D,    0x02},
    {0x35A5,    0x12},
    {0x35A9,    0x62},
    {0x35CE,    0x0E},
    {0x35ED,    0x12},
    {0x35F0,    0xFB},
    {0x35F1,    0x0B},
    {0x35F2,    0xFB},
    {0x35F3,    0x0B},
    
    {0x3642,    0x10},
    {0x366A,    0x2E},
    {0x3670,    0xC3},
    {0x3672,    0x05},
    {0x3674,    0xB6},
    {0x3675,    0x01},
    {0x3676,    0x05},
    {0x3692,    0x10},
    {0x36F5,    0x0F},
    
    {0x3797,    0x20},

    {0x3E2E,    0x07},
    {0x3E30,    0x4E},
    {0x3E6E,    0x07},
    {0x3E70,    0x35},
    {0x3E96,    0x01},
    {0x3E9E,    0x38},
    {0x3EA0,    0x4C},

    {0x3F3A,    0x04},

    {0x4182,    0x00},
    {0x41A2,    0x03},

    {0x4232,    0x3C},
    {0x4235,    0x22},

    {0x4306,    0x00},
    {0x4307,    0x00},
    {0x4308,    0x00},
    {0x4309,    0x00},
    {0x4310,    0x04},
    {0x4311,    0x04},
    {0x4312,    0x04},
    {0x4313,    0x04},
    {0x431E,    0x16},
    {0x431F,    0x16},
    {0x433C,    0x8A},
    {0x433D,    0x02},
    {0x433E,    0xE8},
    {0x433F,    0x05},
    {0x4340,    0x9E},
    {0x4341,    0x0C},

    {0x446A,    0x4C},
    {0x446E,    0x51},
    {0x4472,    0x57},
    {0x4476,    0x79},
    {0x448A,    0x4C},
    {0x448E,    0x51},
    {0x4492,    0x57},
    {0x4496,    0x79},
    {0x44EC,    0x3F},
    {0x44F0,    0x44},
    {0x44F4,    0x4A},

    {0x4510,    0x3F},
    {0x4514,    0x44},
    {0x4518,    0x4A},
    {0x4576,    0xBE},
    {0x457A,    0xB1},
    {0x4580,    0xBC},
    {0x4584,    0xAF},

    {0x473C,    0x06},
    {0x473D,    0x06},
    {0x473E,    0x06},
    {0x473F,    0x06},
    {0x4749,    0x9F},
    {0x474A,    0x99},
    {0x474B,    0x09},
    {0x4753,    0x90},
    {0x4754,    0x99},
    {0x4755,    0x09},
    {0x4788,    0x04},

    {0x4864,    0xDC},
    {0x4868,    0xDC},
    {0x486C,    0xDC},
    {0x4874,    0xDC},
    {0x4878,    0xDC},
    {0x487C,    0xDC},
    {0x48A4,    0xF4},
    {0x48A8,    0xF4},
    {0x48AC,    0xF4},
    {0x48B4,    0xF4},
    {0x48B8,    0xF4},
    {0x48BC,    0xF4},

    {0x4901,    0x0A},
    {0x4902,    0x01},
    {0x4916,    0x00},
    {0x4917,    0x00},
    {0x4918,    0xFF},
    {0x4919,    0x0F},
    {0x491E,    0xFF},
    {0x491F,    0x0F},
    {0x4920,    0x00},
    {0x4921,    0x00},
    {0x4926,    0xFF},
    {0x4927,    0x0F},
    {0x4928,    0x00},
    {0x4929,    0x00},

    {0x4A34,    0x0A},

    {IMX547_TABLE_WAIT_MS,      IMX547_WAIT_MS},
    {IMX547_TABLE_END,          0x0000}
};

#endif /* __IMX547_TABLES__ */
