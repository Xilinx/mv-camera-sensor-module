/*
 * imx547.c - imx547 sensor driver
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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include "imx547_mode_tbls.h"

#define IMX547_K_FACTOR 1000LL
#define IMX547_M_FACTOR 1000000LL
#define IMX547_G_FACTOR 1000000000LL
#define IMX547_T_FACTOR 1000000000000LL

#define IMX547_MIN_GAIN             (0)
#define IMX547_MAX_GAIN             (480)
#define IMX547_DEF_GAIN             (0)

#define IMX547_MIN_BLACK_LEVEL          (0)
#define IMX547_MAX_BLACK_LEVEL_10BIT    (1023)
#define IMX547_MAX_BLACK_LEVEL_12BIT    (4095)
#define IMX547_DEF_BLACK_LEVEL_10BIT    (60)
#define IMX547_DEF_BLACK_LEVEL_12BIT    (240)

#define IMX547_MIN_EXPOSURE_TIME    (14)
#define IMX547_MAX_EXPOSURE_TIME    (660000)
#define IMX547_DEF_EXPOSURE_TIME    (1000)

// 10bit maximum 122.2 fps
#define IMX547_MAX_FRAME_INTERVAL_10BIT_NUMERATOR   (5)
#define IMX547_MAX_FRAME_INTERVAL_10BIT_DENOMINATOR (611)
// 12bit maximum 82.4 fps
#define IMX547_MAX_FRAME_INTERVAL_12BIT_NUMERATOR   (5)
#define IMX547_MAX_FRAME_INTERVAL_12BIT_DENOMINATOR (412)
#define IMX547_MIN_FRAME_RATE       (2)
#define IMX547_DEF_FRAME_RATE       (60)

#define IMX547_MIN_SHS_LENGTH_10BIT 54
#define IMX547_MIN_SHS_LENGTH_12BIT 40

#define IMX547_DEFAULT_LINE_TIME_10BIT 3700 // [ns]
#define IMX547_DEFAULT_LINE_TIME_12BIT 5500 // [ns]

#define IMX547_INCK 74250000LL

static const struct of_device_id imx547_of_match[] = {
    { .compatible = "framos,imx547" },
    { }
};
MODULE_DEVICE_TABLE(of, imx547_of_match);

static const struct regmap_config imx547_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
    .cache_type = REGCACHE_RBTREE,
};

/*
 * imx547 test pattern related structure
 */
enum {
    TEST_PATTERN_NO_PATTERN = 0,
    TEST_PATTERN_SEQUENCE_PATTERN_1,
    TEST_PATTERN_SEQUENCE_PATTERN_2,
    TEST_PATTERN_GRADIATION_PATTERN,
};

static const char * const tp_qmenu[] = {
    "No Pattern",
    "Sequence Pattern 1",
    "Sequence Pattern 2",
    "Gradiation Pattern",
};

/*
 * struct imx547_ctrls - imx547 ctrl structure
 * @handler: V4L2 ctrl handler structure
 * @exposure: Pointer to exposure ctrl structure
 * @gain: Pointer to gain ctrl structure
 * @test_pattern: Pointer to test pattern ctrl structure
 * @black_level: Pointer to black level ctrl structure
 */
struct imx547_ctrls {
    struct v4l2_ctrl_handler handler;
    struct v4l2_ctrl *exposure;
    struct v4l2_ctrl *gain;
    struct v4l2_ctrl *test_pattern;
    struct v4l2_ctrl *black_level;
};

/*
 * struct stim547 - imx547 device structure
 * @sd: V4L2 subdevice structure
 * @pad: Media pad structure
 * @client: Pointer to I2C client
 * @ctrls: imx547 control structure
 * @format: V4L2 media bus frame format structure
 * @frame_interval: V4L2 frame interval structure
 * @regmap: Pointer to regmap structure
 * @gt_trx_reset_gpio: Pointer to GT TRX wizard reset gpio
 * @pipe_reset_gpio: Pointer to input pipe reset gpio
 * @lock: Mutex structure
 * @frame_length: Frame length
 * @line_time: Line time in nanoseconds
 */
struct stimx547 {
    struct v4l2_subdev sd;
    struct media_pad pad;
    struct i2c_client *client;
    struct imx547_ctrls ctrls;
    struct v4l2_mbus_framefmt format;
    struct v4l2_fract frame_interval;
    struct regmap *regmap;
    struct gpio_desc *gt_trx_reset_gpio;
    struct gpio_desc *pipe_reset_gpio;
    struct mutex lock; /* mutex lock for operations */
    u64 frame_length;
    u32 line_time;
};

/*
 * Function declaration
 */
static int imx547_set_gain(struct stimx547 *priv, int val);
static int imx547_set_exposure(struct stimx547 *priv, int val);
static int imx547_set_test_pattern(struct stimx547 *priv, int val);
static int imx547_set_black_level(struct stimx547 *priv, int val);
static int imx547_set_frame_interval(struct stimx547 *priv);
static int imx547_calculate_line_time(struct stimx547 *priv);

static inline void msleep_range(unsigned int delay_base)
{
    usleep_range(delay_base * 1000, delay_base * 1000 + 500);
}

/*
 * v4l2_ctrl and v4l2_subdev related operations
 */
static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
    return &container_of(ctrl->handler,
                 struct stimx547, ctrls.handler)->sd;
}

static inline struct stimx547 *to_imx547(struct v4l2_subdev *sd)
{
    return container_of(sd, struct stimx547, sd);
}

/*
 * Writing a register table
 *
 * @priv: Pointer to device
 * @table: Table containing register values (with optional delays)
 *
 * This is used to write register table into sensor's reg map.
 *
 * Return: 0 on success, errors otherwise
 */
static int imx547_write_table(struct stimx547 *priv, const struct reg_8 table[])
{
    struct regmap *regmap = priv->regmap;
    int err = 0;
    const struct reg_8 *next;
    u8 val;

    int range_start = -1;
    int range_count = 0;
    u8 range_vals[16];
    int max_range_vals = ARRAY_SIZE(range_vals);

    for (next = table;; next++) {
        if ((next->addr != range_start + range_count) ||
            (next->addr == IMX547_TABLE_END) ||
            (next->addr == IMX547_TABLE_WAIT_MS) ||
            (range_count == max_range_vals)) {
            if (range_count == 1)
                err = regmap_write(regmap,
                           range_start, range_vals[0]);
            else if (range_count > 1)
                err = regmap_bulk_write(regmap, range_start,
                            &range_vals[0],
                            range_count);
            else
                err = 0;

            if (err)
                return err;

            range_start = -1;
            range_count = 0;

            /* Handle special address values */
            if (next->addr == IMX547_TABLE_END)
                break;

            if (next->addr == IMX547_TABLE_WAIT_MS) {
                msleep_range(next->val);
                continue;
            }
        }

        val = next->val;

        if (range_start == -1)
            range_start = next->addr;

        range_vals[range_count++] = val;
    }
    return 0;
}

static inline int imx547_write_reg(struct stimx547 *priv, u16 addr, u8 val)
{
    int err;

    err = regmap_write(priv->regmap, addr, val);
    if (err)
        dev_err(&priv->client->dev,
            "%s : i2c write failed, %x = %x\n", __func__,
            addr, val);
    else
        dev_dbg(&priv->client->dev,
            "%s : addr 0x%x, val=0x%x\n", __func__,
            addr, val);
    return err;
}

/**
 * Read a multibyte register.
 *
 * Uses a bulk read where possible.
 *
 * @priv: Pointer to device structure
 * @addr: Address of the LSB register.  Other registers must be
 *        consecutive, least-to-most significant.
 * @val: Pointer to store the register value (cpu endianness)
 * @nbytes: Number of bytes to read (range: [1..3]).
 *          Other bytes are zet to 0.
 *
 * Return: 0 on success, errors otherwise
 */
static int imx547_read_mbreg(struct stimx547 *priv, u16 addr, u32 *val,
                 size_t nbytes)
{
    __le32 val_le = 0;
    int err;

    err = regmap_bulk_read(priv->regmap, addr, &val_le, nbytes);
    if (err) {
        dev_err(&priv->client->dev,
            "%s : i2c bulk read failed, %x (%zu bytes)\n",
            __func__, addr, nbytes);
    } else {
        *val = le32_to_cpu(val_le);
        dev_dbg(&priv->client->dev,
            "%s : addr 0x%x, val=0x%x (%zu bytes)\n",
            __func__, addr, *val, nbytes);
    }

    return err;
}

/**
 * Write a multibyte register.
 *
 * Uses a bulk write where possible.
 *
 * @priv: Pointer to device structure
 * @addr: Address of the LSB register.  Other registers must be
 *        consecutive, least-to-most significant.
 * @val: Value to be written to the register (cpu endianness)
 * @nbytes: Number of bytes to write (range: [1..3])
 */
static int imx547_write_mbreg(struct stimx547 *priv, u16 addr, u32 val,
                  size_t nbytes)
{
    __le32 val_le = cpu_to_le32(val);
    int err;

    err = regmap_bulk_write(priv->regmap, addr, &val_le, nbytes);
    if (err)
        dev_err(&priv->client->dev,
            "%s : i2c bulk write failed, %x = %x (%zu bytes)\n",
            __func__, addr, val, nbytes);
    else
        dev_dbg(&priv->client->dev,
            "%s : addr 0x%x, val=0x%x (%zu bytes)\n",
            __func__, addr, val, nbytes);
    return err;
}

/*
 * imx547_common_regs - Function for setting common registers.
 * @priv: Pointer to device structure
 *
 * Return: 0 on success, errors otherwise
 */
static int imx547_common_regs(struct stimx547 *priv)
{
    int err = 0;

    err = imx547_write_table(priv, imx547_common_settings);
    if (err)
        return err;

    dev_dbg(&priv->client->dev, "imx547 : imx547_common_regs !\n");

    return err;
}

/*
 * imx547_set_pixel_format - Function for setting registers for pixel format.
 * @priv: Pointer to device structure
 *
 * Return: 0 on success, errors otherwise
 */
static int imx547_set_pixel_format(struct stimx547 *priv)
{
    int err = 0;

    switch (priv->format.code) {
        case MEDIA_BUS_FMT_SRGGB10_1X10:
        case MEDIA_BUS_FMT_Y10_1X10:
            err = imx547_write_table(priv, imx547_10bit_mode);
            break;
        case MEDIA_BUS_FMT_SRGGB12_1X12:
        case MEDIA_BUS_FMT_Y12_1X12:
            err = imx547_write_table(priv, imx547_12bit_mode);
            break;
        default:
            dev_err(&priv->client->dev, "%s: unknown pixel format\n", __func__);
            return -EINVAL;
    }
    dev_dbg(&priv->client->dev, "imx547 : imx547_set_pixel_format !\n");

    return err;
}

/*
 * imx547_start_stream - Function for starting stream
 * @priv: Pointer to device structure
 *
 * Return: 0 on success, errors otherwise
 */
static int imx547_start_stream(struct stimx547 *priv)
{
    int err = 0;

    err = imx547_write_reg(priv, STANDBY, 0x00);

    /* "Internal regulator stabilization" time */
    usleep_range(1138000, 1140000);

    gpiod_set_value_cansleep(priv->gt_trx_reset_gpio, 1);
    usleep_range(20000, 21000);
    gpiod_set_value_cansleep(priv->gt_trx_reset_gpio, 0);

    err |= imx547_write_reg(priv, XMSTA, 0x00);

    dev_dbg(&priv->client->dev, "imx547 : imx547_start_stream !\n");
    return 0;
}

/*
 * imx547_stop_stream - Function for stoping stream
 * @priv: Pointer to device structure
 *
 * Return: 0 on success, errors otherwise
 */
static int imx547_stop_stream(struct stimx547 *priv)
{
    int err = 0;

    err = imx547_write_reg(priv, STANDBY, 0x01);

    usleep_range(100, 110);
       
    err |= imx547_write_reg(priv, XMSTA, 0x01);


    dev_dbg(&priv->client->dev, "imx547 : imx547_stop_stream !\n");
    return 0;
}


/**
 * imx547_s_ctrl - This is used to set the imx547 V4L2 controls
 * @ctrl: V4L2 control to be set
 *
 * This function is used to set the V4L2 controls for the imx547 sensor.
 *
 * Return: 0 on success, errors otherwise
 */
static int imx547_s_ctrl(struct v4l2_ctrl *ctrl)
{
    struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
    struct stimx547 *imx547 = to_imx547(sd);
    int ret = -EINVAL;

    dev_dbg(&imx547->client->dev,
        "%s : s_ctrl: %s, value: %d\n", __func__,
        ctrl->name, ctrl->val);

    switch (ctrl->id) {
    case V4L2_CID_EXPOSURE:
        dev_dbg(&imx547->client->dev,
            "%s : set V4L2_CID_EXPOSURE\n", __func__);
        ret = imx547_set_exposure(imx547, ctrl->val);
        break;

    case V4L2_CID_GAIN:
        dev_dbg(&imx547->client->dev,
            "%s : set V4L2_CID_GAIN\n", __func__);
        ret = imx547_set_gain(imx547, ctrl->val);
        break;

    case V4L2_CID_TEST_PATTERN:
        dev_dbg(&imx547->client->dev,
            "%s : set V4L2_CID_TEST_PATTERN\n", __func__);
        ret = imx547_set_test_pattern(imx547, ctrl->val);
        break;

    case V4L2_CID_BLACK_LEVEL:
        dev_dbg(&imx547->client->dev,
            "%s : set V4L2_CID_BLACK_LEVEL\n", __func__);
        ret = imx547_set_black_level(imx547, ctrl->val);
        break;

    }

    return ret;
}


/**
 * imx547_get_fmt - Get the pad format
 * @sd: Pointer to V4L2 Sub device structure
 * @cfg: Pointer to sub device pad information structure
 * @fmt: Pointer to pad level media bus format
 *
 * This function is used to get the pad format information.
 *
 * Return: 0 on success
 */
static int imx547_get_fmt(struct v4l2_subdev *sd,
              struct v4l2_subdev_state *sd_state,
              struct v4l2_subdev_format *fmt)
{
    struct stimx547 *imx547 = to_imx547(sd);

    mutex_lock(&imx547->lock);
    fmt->format = imx547->format;
    mutex_unlock(&imx547->lock);
    return 0;
}

/**
 * imx547_set_fmt - This is used to set the pad format
 * @sd: Pointer to V4L2 Sub device structure
 * @cfg: Pointer to sub device pad information structure
 * @format: Pointer to pad level media bus format
 *
 * This function is used to set the pad format.
 *
 * Return: 0 on success
 */
static int imx547_set_fmt(struct v4l2_subdev *sd,
              struct v4l2_subdev_state *sd_state,
              struct v4l2_subdev_format *format)
{
    struct stimx547 *imx547 = to_imx547(sd);
    int err = 0;

    mutex_lock(&imx547->lock);
    imx547->format = format->format;

    switch (imx547->format.code) {
        case MEDIA_BUS_FMT_SRGGB10_1X10:
        case MEDIA_BUS_FMT_Y10_1X10:
            imx547->line_time = IMX547_DEFAULT_LINE_TIME_10BIT;
            break;
        case MEDIA_BUS_FMT_SRGGB12_1X12:
        case MEDIA_BUS_FMT_Y12_1X12:
            imx547->line_time = IMX547_DEFAULT_LINE_TIME_12BIT;
            break;
        default:
            dev_err(&imx547->client->dev, "%s: Cannot find line time\n", __func__);
            return -EINVAL;
    }

    mutex_unlock(&imx547->lock);

    return err;
}


/**
 * imx547_g_frame_interval - Get the frame interval
 * @sd: Pointer to V4L2 Sub device structure
 * @fi: Pointer to V4l2 Sub device frame interval structure
 *
 * This function is used to get the frame interval.
 *
 * Return: 0 on success
 */
static int imx547_g_frame_interval(struct v4l2_subdev *sd,
                   struct v4l2_subdev_frame_interval *fi)
{
    struct stimx547 *imx547 = to_imx547(sd);

    fi->interval = imx547->frame_interval;
    dev_dbg(&imx547->client->dev, "%s frame rate = %d / %d\n", __func__, imx547->frame_interval.numerator, imx547->frame_interval.denominator);

    return 0;
}

/**
 * imx547_s_frame_interval - Set the frame interval
 * @sd: Pointer to V4L2 Sub device structure
 * @fi: Pointer to V4l2 Sub device frame interval structure
 *
 * This function is used to set the frame interval.
 *
 * Return: 0 on success
 */
static int imx547_s_frame_interval(struct v4l2_subdev *sd,
                   struct v4l2_subdev_frame_interval *fi)
{
    struct stimx547 *imx547 = to_imx547(sd);
    struct v4l2_ctrl *ctrl = imx547->ctrls.exposure;
    int min, max, def;
    int ret;
    u32 min_reg_shs;

    mutex_lock(&imx547->lock);
    imx547->frame_interval = fi->interval;
    ret = imx547_set_frame_interval(imx547);
    if (!ret) {
        /*
         * exposure time range is decided by frame interval
         * need to update it after frame interval changes
         */

        switch (imx547->format.code) {
            case MEDIA_BUS_FMT_SRGGB10_1X10:
            case MEDIA_BUS_FMT_Y10_1X10:
                min_reg_shs = IMX547_MIN_SHS_LENGTH_10BIT;
                break;
            case MEDIA_BUS_FMT_SRGGB12_1X12:
            case MEDIA_BUS_FMT_Y12_1X12:
                min_reg_shs = IMX547_MIN_SHS_LENGTH_12BIT;
                break;
            default:
                dev_err(&imx547->client->dev, "%s: Cannot find minimum SHS\n", __func__);
                return -EINVAL;
        }

        min = IMX547_MIN_EXPOSURE_TIME;
        max = (imx547->frame_length - min_reg_shs) * imx547->line_time / IMX547_K_FACTOR;
        def = max;
        if (__v4l2_ctrl_modify_range(ctrl, min, max, 1, def)) {
            dev_err(&imx547->client->dev,
                "Exposure ctrl range update failed\n");
            goto unlock;
        }

        /* update exposure time accordingly */
        imx547_set_exposure(imx547, ctrl->val);

        dev_dbg(&imx547->client->dev, "set frame interval to %llu us\n", fi->interval.numerator * IMX547_M_FACTOR / fi->interval.denominator);
    }

unlock:
    mutex_unlock(&imx547->lock);

    return ret;
}

/**
 * imx547_s_stream - It is used to start/stop the streaming.
 * @sd: V4L2 Sub device
 * @on: Flag (True / False)
 *
 * This function controls the start or stop of streaming for the
 * imx547 sensor.
 *
 * Return: 0 on success, errors otherwise
 */
static int imx547_s_stream(struct v4l2_subdev *sd, int on)
{
    struct stimx547 *imx547 = to_imx547(sd);
    int ret = 0;

    mutex_lock(&imx547->lock);

    if (on) {

        /* load common registers */
        ret = imx547_common_regs(imx547);
        if (ret)
            goto fail;

         /* load pixel format registers */
        ret = imx547_set_pixel_format(imx547);
        if (ret)
            goto fail;

        /* calculate line time */
        ret = imx547_calculate_line_time(imx547);
        if (ret)
            goto fail;

        /* update frame interval */
        ret = imx547_set_frame_interval(imx547);
        if (ret)
            goto fail;

        /* update exposure time */
        ret = imx547_set_exposure(imx547, imx547->ctrls.exposure->val);
        if (ret)
            goto fail;

        /* start stream */
        ret = imx547_start_stream(imx547);
        if (ret)
            goto fail;
    } else {
        /* stop stream */
        ret = imx547_stop_stream(imx547);
        if (ret)
            goto fail;
    }

    mutex_unlock(&imx547->lock);
    dev_dbg(&imx547->client->dev, "%s : Done\n", __func__);
    return 0;

fail:
    mutex_unlock(&imx547->lock);
    dev_err(&imx547->client->dev, "s_stream failed\n");
    return ret;
}


/*
 * imx547_set_gain - Function called when setting gain
 * @priv: Pointer to device structure
 * @val: Value of gain
 *
 * Set the gain based on input value.
 * The caller should hold the mutex lock imx547->lock if necessary
 *
 * Return: 0 on success
 */
static int imx547_set_gain(struct stimx547 *priv, int val)
{
    
    int err;

    err = imx547_write_mbreg(priv, GAIN_LOW, val, 2);
    if (err){
        dev_dbg(&priv->client->dev, "%s: GAIN control error\n", __func__);
        return err;
    }

    /* update gain */
    priv->ctrls.gain->val = val;

    dev_dbg(&priv->client->dev, "%s:  gain val [%d]\n",  __func__, val);

    return 0;
}

/*
 * imx547_set_black_level - Function called when setting black level
 * @priv: Pointer to device structure
 * @val: Value of black level
 *
 * Set the black level based on input value.
 * The caller should hold the mutex lock imx547->lock if necessary
 *
 * Return: 0 on success
 */
static int imx547_set_black_level(struct stimx547 *priv, int val)
{
    
    int err;

    err = imx547_write_mbreg(priv, BLKLEVEL_LOW, val, 2);
    if (err){
        dev_dbg(&priv->client->dev, "%s: BLKLEVEL control error\n", __func__);
        return err;
    }

    /* update black level */
    priv->ctrls.black_level->val = val;

    dev_dbg(&priv->client->dev, "%s:  black level val [%d]\n",  __func__, val);

    return 0;
}

/*
 * imx547_set_exposure - Function called when setting exposure time
 * @priv: Pointer to device structure
 * @val: Variable for exposure time, in the unit of micro-second
 *
 * Set exposure time based on input value.
 * The caller should hold the mutex lock imx547->lock if necessary
 *
 * Return: 0 on success
 */
static int imx547_set_exposure(struct stimx547 *priv, int val)
{
    
    int err = 0;
    u32 integration_time_line;
    u32 reg_shs, min_reg_shs;

    dev_dbg(&priv->client->dev, "%s: integration time: %d [us]\n", __func__, val);

    /* Check value with internal range */
    if (val > priv->ctrls.exposure->maximum) {
        val = priv->ctrls.exposure->maximum;
    }
    else if (val < priv->ctrls.exposure->minimum) {
        val = priv->ctrls.exposure->minimum;
    }

    integration_time_line = (val * IMX547_K_FACTOR) / priv->line_time ;

    reg_shs = priv->frame_length - integration_time_line;
    
    switch (priv->format.code) {
        case MEDIA_BUS_FMT_SRGGB10_1X10:
        case MEDIA_BUS_FMT_Y10_1X10:
            min_reg_shs = IMX547_MIN_SHS_LENGTH_10BIT;
            break;
        case MEDIA_BUS_FMT_SRGGB12_1X12:
        case MEDIA_BUS_FMT_Y12_1X12:
            min_reg_shs = IMX547_MIN_SHS_LENGTH_12BIT;
            break;
        default:
            dev_err(&priv->client->dev, "%s: Cannot find minimum SHS\n", __func__);
            return -EINVAL;
    }

    if (reg_shs < min_reg_shs)
        reg_shs = min_reg_shs;
    else if (reg_shs > (priv->frame_length - 1))
        reg_shs = priv->frame_length - 1;

    err = imx547_write_mbreg(priv, SHS_LOW, reg_shs, 3);
    if (err) {
        dev_err(&priv->client->dev, "%s: failed to set exposure\n", __func__);
        return err;
    }

    /* update exposure time */
    priv->ctrls.exposure->val = val;

    dev_dbg(&priv->client->dev,
     "%s: set integration time: %d [us], coarse1:%d [line], shs: %d [line], frame length: %llu [line]\n",
     __func__, val, integration_time_line, reg_shs, priv->frame_length);

    return err;
}

/*
 * imx547_set_test_pattern - Function called when setting test pattern
 * @priv: Pointer to device structure
 * @val: Variable for test pattern
 *
 * Set to different test patterns based on input value.
 *
 * Return: 0 on success
 */
static int imx547_set_test_pattern(struct stimx547 *priv, int val)
{

    int err;

    if (val) {
        err = imx547_write_reg(priv, 0x3550, 0x07);
        if (err) 
            goto fail;

        err = imx547_write_reg(priv, 0x3551, (u8)(val));
        if (err) 
            goto fail;
    }
    else {
        err = imx547_write_reg(priv, 0x3550, 0x06);
        if (err) 
            goto fail;
    }

    return 0;

fail:
    dev_err(&priv->client->dev, "%s: error setting test pattern\n", __func__);
    return err;
}


/*
 * imx547_set_exposure - Function for calculating 1H time
 * @priv: Pointer to device structure
 *
 * Calculate line time based on HMAX value.
 *
 * Return: 0 on success
 */
static int imx547_calculate_line_time(struct stimx547 *priv)
{
    u32 hmax;
    int err;

    err = imx547_read_mbreg(priv, HMAX_LOW, &hmax, 2);
    if (err) {
        dev_err(&priv->client->dev, "%s: unable to read hmax\n", __func__);
        return err;
    }

    priv->line_time = (hmax*IMX547_G_FACTOR) / (IMX547_INCK);


    dev_dbg(&priv->client->dev, "%s: hmax: %u [inck], line_time: %u [ns]\n",
            __func__, hmax, priv->line_time);

    return 0;
}


/*
 * imx547_set_frame_length - Function called when setting frame length
 * @priv: Pointer to device structure
 *
 * Set frame length.
 *
 * Return: 0 on success
 */
static int imx547_set_frame_length(struct stimx547 *priv)
{
    int err;
    u32 frame_length_32bit;

    frame_length_32bit = (u32)priv->frame_length;

    err = imx547_write_mbreg(priv, VMAX_LOW, frame_length_32bit, 3);
    if (err) {
        dev_err(&priv->client->dev, "%s unable to write vmax\n", __func__);
        return err;
    }

    dev_dbg(&priv->client->dev, "%s : input length = %llu\n", __func__, priv->frame_length);

    return 0;
}

/*
 * imx547_set_frame_interval - Function called when setting frame interval
 * @priv: Pointer to device structure
 *
 * Change frame interval by updating VMAX value
 * The caller should hold the mutex lock imx547->lock if necessary
 *
 * Return: 0 on success
 */
static int imx547_set_frame_interval(struct stimx547 *priv)
{
    int err;
    u64 frame_length;
    u64 req_frame_rate;
    u32 max_frame_rate;

	dev_dbg(&priv->client->dev, "%s: input frame interval = %d / %d", 
			__func__, priv->frame_interval.numerator, priv->frame_interval.denominator);

    if (priv->frame_interval.numerator == 0 || priv->frame_interval.denominator == 0) {
        priv->frame_interval.denominator = IMX547_DEF_FRAME_RATE;
        priv->frame_interval.numerator = 1;
    }

    req_frame_rate = IMX547_M_FACTOR * priv->frame_interval.denominator / priv->frame_interval.numerator;

    switch (priv->format.code) {
        case MEDIA_BUS_FMT_SRGGB10_1X10:
        case MEDIA_BUS_FMT_Y10_1X10:
            max_frame_rate = IMX547_M_FACTOR * IMX547_MAX_FRAME_INTERVAL_10BIT_DENOMINATOR / IMX547_MAX_FRAME_INTERVAL_10BIT_NUMERATOR;
            break;
        case MEDIA_BUS_FMT_SRGGB12_1X12:
        case MEDIA_BUS_FMT_Y12_1X12:
            max_frame_rate = IMX547_M_FACTOR * IMX547_MAX_FRAME_INTERVAL_12BIT_DENOMINATOR / IMX547_MAX_FRAME_INTERVAL_12BIT_NUMERATOR;
            break;
        default:
            dev_err(&priv->client->dev, "%s: Cannot find maximum frame rate\n", __func__);
            return -EINVAL;
    }

    /* boundary check */
    if (req_frame_rate > max_frame_rate) {
        switch (priv->format.code) {
            case MEDIA_BUS_FMT_SRGGB10_1X10:
            case MEDIA_BUS_FMT_Y10_1X10:
                priv->frame_interval.numerator = IMX547_MAX_FRAME_INTERVAL_10BIT_NUMERATOR;
                priv->frame_interval.denominator = IMX547_MAX_FRAME_INTERVAL_10BIT_DENOMINATOR ;
                break;
            case MEDIA_BUS_FMT_SRGGB12_1X12:
            case MEDIA_BUS_FMT_Y12_1X12:
                priv->frame_interval.numerator = IMX547_MAX_FRAME_INTERVAL_12BIT_NUMERATOR;
                priv->frame_interval.denominator = IMX547_MAX_FRAME_INTERVAL_12BIT_DENOMINATOR ;
                break;
            default:
                dev_err(&priv->client->dev, "%s: Frame interval setting error\n", __func__);
                return -EINVAL;
        }
    } else if (req_frame_rate < (IMX547_MIN_FRAME_RATE * IMX547_M_FACTOR)) {
        priv->frame_interval.numerator = 1;
        priv->frame_interval.denominator = IMX547_MIN_FRAME_RATE;
    }

    req_frame_rate = IMX547_M_FACTOR * priv->frame_interval.denominator / priv->frame_interval.numerator;

    frame_length = (IMX547_M_FACTOR * IMX547_G_FACTOR) / (req_frame_rate * priv->line_time);
    priv->frame_length = frame_length;
    dev_dbg(&priv->client->dev, "%s: req_frame_rate: %llu line time: %d, frame_length:%llu  \n",
			 __func__, req_frame_rate, priv->line_time, frame_length);

    err = imx547_set_frame_length(priv);
    if (err)
        goto fail;

    return 0;

fail:
    dev_err(&priv->client->dev, "%s error = %d\n", __func__, err);
    return err;
}

static const struct v4l2_subdev_pad_ops imx547_pad_ops = {
    .get_fmt = imx547_get_fmt,
    .set_fmt = imx547_set_fmt,
};

static const struct v4l2_subdev_video_ops imx547_video_ops = {
    .g_frame_interval = imx547_g_frame_interval,
    .s_frame_interval = imx547_s_frame_interval,
    .s_stream = imx547_s_stream,
};

static const struct v4l2_subdev_ops imx547_subdev_ops = {
    .pad = &imx547_pad_ops,
    .video = &imx547_video_ops,
};

static const struct v4l2_ctrl_ops imx547_ctrl_ops = {
    .s_ctrl = imx547_s_ctrl,
};


static int imx547_probe(struct i2c_client *client)
{
    struct v4l2_subdev *sd;
    struct stimx547 *imx547;
    int ret;

    /* initialize imx547 */
    imx547 = devm_kzalloc(&client->dev, sizeof(*imx547), GFP_KERNEL);
    if (!imx547)
        return -ENOMEM;

    mutex_init(&imx547->lock);

    /* initialize format */
    imx547->format.width = IMX547_DEFAULT_WIDTH;
    imx547->format.height = IMX547_DEFAULT_HEIGHT;
    imx547->format.field = V4L2_FIELD_NONE;
    imx547->format.code = MEDIA_BUS_FMT_SRGGB12_1X12;
    imx547->format.colorspace = V4L2_COLORSPACE_SRGB;
    imx547->frame_interval.numerator = 1;
    imx547->frame_interval.denominator = IMX547_DEF_FRAME_RATE;
    imx547->frame_length = IMX547_DEFAULT_HEIGHT + IMX547_MIN_FRAME_DELTA;

    /* initialize regmap */
    imx547->regmap = devm_regmap_init_i2c(client, &imx547_regmap_config);
    if (IS_ERR(imx547->regmap)) {
        dev_err(&client->dev,
            "regmap init failed: %ld\n", PTR_ERR(imx547->regmap));
        ret = -ENODEV;
        goto err_regmap;
    }

    /* initialize subdevice */
    imx547->client = client;
    sd = &imx547->sd;
    v4l2_i2c_subdev_init(sd, client, &imx547_subdev_ops);
    sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;

    /* initialize subdev media pad */
    imx547->pad.flags = MEDIA_PAD_FL_SOURCE;
    sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
    ret = media_entity_pads_init(&sd->entity, 1, &imx547->pad);
    if (ret < 0) {
        dev_err(&client->dev,
            "%s : media entity init Failed %d\n", __func__, ret);
        goto err_regmap;
    }

    /* initialize gt trx reset gpio */
    imx547->gt_trx_reset_gpio = gpiod_get_index_optional(&client->dev, "reset",
                                0, GPIOD_OUT_HIGH);
    if (IS_ERR(imx547->gt_trx_reset_gpio)) {
        if (PTR_ERR(imx547->gt_trx_reset_gpio) != -EPROBE_DEFER)
            dev_err(&client->dev, "GT TRX Reset GPIO not setup in DT");
        ret = PTR_ERR(imx547->gt_trx_reset_gpio);
        goto err_me;
    }

    /* initialize pipe reset gpio */
    imx547->pipe_reset_gpio = gpiod_get_index_optional(&client->dev, "reset",
                              1, GPIOD_OUT_HIGH);
    if (IS_ERR(imx547->pipe_reset_gpio)) {
        if (PTR_ERR(imx547->pipe_reset_gpio) != -EPROBE_DEFER)
            dev_err(&client->dev, "GT TRX Reset GPIO not setup in DT");
        ret = PTR_ERR(imx547->pipe_reset_gpio);
        goto err_me;
    }
    gpiod_set_value_cansleep(imx547->pipe_reset_gpio, 0);

    /* initialize controls */
    ret = v4l2_ctrl_handler_init(&imx547->ctrls.handler, 3);
    if (ret < 0) {
        dev_err(&client->dev,
            "%s : ctrl handler init Failed\n", __func__);
        goto err_me;
    }

    imx547->ctrls.handler.lock = &imx547->lock;

    /* add new controls */
    imx547->ctrls.test_pattern = v4l2_ctrl_new_std_menu_items(
        &imx547->ctrls.handler, &imx547_ctrl_ops,
        V4L2_CID_TEST_PATTERN,
        ARRAY_SIZE(tp_qmenu) - 1, 0, 0, tp_qmenu);

    imx547->ctrls.gain = v4l2_ctrl_new_std(
        &imx547->ctrls.handler,
        &imx547_ctrl_ops,
        V4L2_CID_GAIN, IMX547_MIN_GAIN,
        IMX547_MAX_GAIN, 1,
        IMX547_DEF_GAIN);

    imx547->ctrls.exposure = v4l2_ctrl_new_std(
        &imx547->ctrls.handler,
        &imx547_ctrl_ops,
        V4L2_CID_EXPOSURE, IMX547_MIN_EXPOSURE_TIME,
        IMX547_M_FACTOR / IMX547_DEF_FRAME_RATE, 1,
        IMX547_DEF_EXPOSURE_TIME);

    imx547->ctrls.black_level = v4l2_ctrl_new_std(
        &imx547->ctrls.handler,
        &imx547_ctrl_ops,
        V4L2_CID_BLACK_LEVEL, IMX547_MIN_BLACK_LEVEL,
        IMX547_MAX_BLACK_LEVEL_10BIT, 1,
        IMX547_DEF_BLACK_LEVEL_10BIT);

    imx547->sd.ctrl_handler = &imx547->ctrls.handler;
    if (imx547->ctrls.handler.error) {
        ret = imx547->ctrls.handler.error;
        goto err_ctrls;
    }

    /* setup default controls */
    ret = v4l2_ctrl_handler_setup(&imx547->ctrls.handler);
    if (ret) {
        dev_err(&client->dev,
            "Error %d setup default controls\n", ret);
        goto err_ctrls;
    }

    /* register subdevice */
    ret = v4l2_async_register_subdev(sd);
    if (ret < 0) {
        dev_err(&client->dev,
            "%s : v4l2_async_register_subdev failed %d\n",
            __func__, ret);
        goto err_ctrls;
    }

    dev_info(&client->dev, "imx547 : imx547 probe success !\n");
    return 0;

err_ctrls:
    v4l2_ctrl_handler_free(&imx547->ctrls.handler);
err_me:
    media_entity_cleanup(&sd->entity);
err_regmap:
    mutex_destroy(&imx547->lock);
    return ret;
}

static int imx547_remove(struct i2c_client *client)
{
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct stimx547 *imx547 = to_imx547(sd);

    /* stop stream */
    imx547_stop_stream(imx547);

    v4l2_async_unregister_subdev(sd);
    v4l2_ctrl_handler_free(&imx547->ctrls.handler);
    media_entity_cleanup(&sd->entity);
    mutex_destroy(&imx547->lock);
    return 0;
}

static const struct i2c_device_id imx547_id[] = {
    { "imx547", 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, imx547_id);

static struct i2c_driver imx547_i2c_driver = {
    .driver = {
        .name   = "imx547",
        .of_match_table = imx547_of_match,
    },
    .probe_new  = imx547_probe,
    .remove     = imx547_remove,
    .id_table   = imx547_id,
};

module_i2c_driver(imx547_i2c_driver);

MODULE_AUTHOR("FRAMOS GmbH");
MODULE_DESCRIPTION("IMX547 CMOS Image Sensor driver");
MODULE_LICENSE("GPL v2");
