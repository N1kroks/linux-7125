// SPDX-License-Identifier: GPL-2.0-only
/*
 * drv2624 haptics driver
 *
 * Copyright (c) 2016 Texas Instruments Inc.
 * Copyright (c) 2024 Vitalii Skorkin <nikroksm@mail.ru>
 *
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>

#include <dt-bindings/input/ti-drv2624.h>

#define DRV2624_ID							0x00
#define DRV2624_MODE						0x07
#define DRV2624_CONTROL1					0x08
#define DRV2624_GO							0x0c
#define DRV2624_CONTROL2					0x0d
#define DRV2624_RTP_INPUT					0x0e
#define DRV2624_RATED_VOLTAGE				0x1f
#define DRV2624_OVERDRIVE_CLAMP				0x20
#define DRV2624_DRIVE_TIME					0x27
#define DRV2624_OPENLOOP_PERIOD_H			0x2e
#define DRV2624_OPENLOOP_PERIOD_L			0x2f

/* Mode register */
#define DRV2624_PINFUNC_MASK				0x0c
#define DRV2624_PINFUNC_INT					0x02
#define DRV2624_PINFUNC_SHIFT				0x02
#define DRV2624_MODE_MASK					0x03
#define DRV2624_MODE_RTP					0x00
#define DRV2624_MODE_WAVEFORM				0x01
#define DRV2624_MODE_DIAGNOSTIC				0x02
#define DRV2624_MODE_CALIBRATION			0x03

/* Control1 register */
#define DRV2624_ACTUATOR_MASK				0x80
#define DRV2624_ACTUATOR_SHIFT				0x07
#define DRV2624_LOOP_MASK					0x40
#define DRV2624_LOOP_SHIFT					0x06
#define DRV2624_AUTOBRK_OK_MASK				0x10
#define DRV2624_AUTOBRK_OK_ENABLE			0x10
#define DRV2624_AUTO_BRK_INTO_STBY_MASK		(0x01 << 3)
#define DRV2624_STBY_MODE_WITH_AUTO_BRAKE	(0x01 << 3)
#define DRV2624_REMOVE_STBY_MODE            0x00

/* Control2 register */
#define DRV2624_LIB_MASK					0x80
#define DRV2624_LIB_SHIFT					0x07

/* Drive Time register */
#define DRV2624_DRIVE_TIME_MASK				0x1f
#define DRV2624_MINFREQ_SEL_45HZ			0x01
#define DRV2624_MINFREQ_SEL_MASK			0x80
#define DRV2624_MINFREQ_SEL_SHIFT			0x07

struct drv2624_data {
	struct input_dev *input_dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct work_struct work;
	struct gpio_desc *reset_gpio;
	u8 magnitude;
	u32 mode;
	u32 lra_frequency;
	int rated_voltage;
	int overdrive_voltage;
};

/*
 * Rated and Overdriver Voltages:
 * Calculated using the formula r = v * 255 / 5.6
 * where r is what will be written to the register
 * and v is the rated or overdriver voltage of the actuator
 */
static int drv2624_calculate_voltage(unsigned int voltage)
{
	return (voltage * 255 / 5600);
}

static void drv2624_worker(struct work_struct *work)
{
	struct drv2624_data *haptics = container_of(work, struct drv2624_data, work);
	int error;

	error = regmap_write(haptics->regmap,
					DRV2624_RTP_INPUT, haptics->magnitude);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to set magnitude: %d\n", error);
		return;
	}
}

static int drv2624_haptics_play(struct input_dev *input, void *data,
				struct ff_effect *effect)
{
	struct drv2624_data *haptics = input_get_drvdata(input);

	/* Scale u16 magnitude into u8 register value */
	if (effect->u.rumble.strong_magnitude > 0)
		haptics->magnitude = effect->u.rumble.strong_magnitude >> 8;
	else if (effect->u.rumble.weak_magnitude > 0)
		haptics->magnitude = effect->u.rumble.weak_magnitude >> 8;
	else
		haptics->magnitude = 0;

	if (haptics->magnitude > 0x7f)
		haptics->magnitude = 0x7f;

	schedule_work(&haptics->work);

	return 0;
}

static int drv2624_open(struct input_dev *input)
{
	struct drv2624_data *haptics = input_get_drvdata(input);
	int error;

	error = regmap_update_bits(haptics->regmap,
			     DRV2624_MODE, DRV2624_MODE_MASK, DRV2624_MODE_RTP);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to set mode: %d\n", error);
		return error;
	}

	error = regmap_write(haptics->regmap,
					DRV2624_RTP_INPUT, 0x0);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to set magnitude: %d\n", error);
		return error;
	}

	error = regmap_write(haptics->regmap, DRV2624_GO, 1);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write GO register: %d\n",
			error);
		return error;
	}

	return error;
}

static void drv2624_close(struct input_dev *input)
{
	struct drv2624_data *haptics = input_get_drvdata(input);
	int error;
	unsigned int cal_buf;

	cancel_work_sync(&haptics->work);

	error = regmap_write(haptics->regmap, DRV2624_GO, 0);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write GO register: %d\n",
			error);
		return;
	}

	do {
		usleep_range(15000, 15500);
		error = regmap_read(haptics->regmap, DRV2624_GO, &cal_buf);
		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to read GO register: %d\n",
				error);
			return;
		}
	} while (cal_buf == 0);
}

static int drv2624_init(struct drv2624_data *haptics)
{
	int error;
	unsigned int drive_time;
	unsigned short open_loop_period;
	unsigned int cal_buf;

	error = regmap_update_bits(haptics->regmap, DRV2624_MODE,
		DRV2624_PINFUNC_MASK, DRV2624_PINFUNC_INT << DRV2624_PINFUNC_SHIFT);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write DRV2624_MODE register: %d\n",
			error);
		return error;
	}

	error = regmap_update_bits(haptics->regmap, DRV2624_CONTROL1,
		DRV2624_ACTUATOR_MASK | DRV2624_LOOP_MASK | DRV2624_AUTOBRK_OK_MASK,
		(haptics->mode << DRV2624_ACTUATOR_SHIFT) |
		(1 << DRV2624_LOOP_SHIFT) | DRV2624_AUTOBRK_OK_ENABLE);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write DRV2624_CONTROL1 register: %d\n",
			error);
		return error;
	}

	if(haptics->mode == DRV2624_ERM_MODE) {
		error = regmap_update_bits(haptics->regmap, DRV2624_CONTROL2,
			DRV2624_LIB_MASK, 0x01);
		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to write DRV2624_CONTROL2 register: %d\n",
				error);
			return error;
		}
	}

	error = regmap_write(haptics->regmap,
			     DRV2624_RATED_VOLTAGE, haptics->rated_voltage);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write DRV2624_RATED_VOLTAGE register: %d\n",
			error);
		return error;
	}

	error = regmap_write(haptics->regmap,
			     DRV2624_OVERDRIVE_CLAMP, haptics->overdrive_voltage);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write DRV2624_OVERDRIVE_CLAMP register: %d\n",
			error);
		return error;
	}

	if(haptics->mode == DRV2624_LRA_MODE) {
		drive_time = 5 * (1000 - haptics->lra_frequency) / haptics->lra_frequency;
		open_loop_period = (unsigned short)(1000000000 / (24619 * haptics->lra_frequency));

		if(haptics->lra_frequency < 125)
			drive_time |= (DRV2624_MINFREQ_SEL_45HZ << DRV2624_MINFREQ_SEL_SHIFT);

		error = regmap_update_bits(haptics->regmap,
					DRV2624_DRIVE_TIME, DRV2624_DRIVE_TIME_MASK |
					DRV2624_MINFREQ_SEL_MASK, drive_time);
		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to write DRV2624_DRIVE_TIME register: %d\n",
				error);
			return error;
		}

		error = regmap_update_bits(haptics->regmap, DRV2624_OPENLOOP_PERIOD_H,
					0x03, (open_loop_period & 0x0300) >> 8);
		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to write DRV2624_OPENLOOP_PERIOD_H register: %d\n",
				error);
			return error;
		}

		error = regmap_write(haptics->regmap, DRV2624_OPENLOOP_PERIOD_L,
					open_loop_period & 0x00ff);
		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to write DRV2624_OPENLOOP_PERIOD_L register: %d\n",
				error);
			return error;
		}
	}

	error = regmap_update_bits(haptics->regmap,
			     DRV2624_MODE, DRV2624_MODE_MASK, DRV2624_MODE_CALIBRATION);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to set mode: %d\n", error);
		return error;
	}

	error = regmap_write(haptics->regmap, DRV2624_GO, 1);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write GO register: %d\n",
			error);
		return error;
	}

	do {
		usleep_range(15000, 15500);
		error = regmap_read(haptics->regmap, DRV2624_GO, &cal_buf);
		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to read GO register: %d\n",
				error);
			return error;
		}
	} while (cal_buf == 1);

	return 0;
}

static const struct regmap_config drv2624_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.cache_type = REGCACHE_NONE,
};

static int drv2624_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct drv2624_data *haptics;
	u32 voltage;
	int error;

	haptics = devm_kzalloc(dev, sizeof(*haptics), GFP_KERNEL);
	if (!haptics)
		return -ENOMEM;

	error = device_property_read_u32(dev, "mode", &haptics->mode);
	if (error) {
		dev_err(dev, "Can't fetch 'mode' property: %d\n", error);
		return error;
	}

	if (haptics->mode < DRV2624_ERM_MODE ||
	    haptics->mode > DRV2624_LRA_MODE) {
		dev_err(dev, "Vibrator mode is invalid: %i\n", haptics->mode);
		return -EINVAL;
	}

	if(haptics->mode == DRV2624_LRA_MODE) {
		error = device_property_read_u32(dev, "lra-frequency", &haptics->lra_frequency);
		if (error) {
			dev_err(dev, "Can't fetch 'lra-frequency' property: %d\n", error);
			return error;
		}

		if(haptics->lra_frequency < 45 || haptics->lra_frequency > 300) {
			dev_err(dev, "Property 'lra-frequency' is out of range\n");
			return -EINVAL;
		}
	}

	error = device_property_read_u32(dev, "vib-rated-mv", &voltage);
	if (error) {
		dev_err(dev, "Can't fetch 'vib-rated-mv' property: %d\n", error);
		return error;
	}

	haptics->rated_voltage = drv2624_calculate_voltage(voltage);

	error = device_property_read_u32(dev, "vib-overdrive-mv", &voltage);
	if (error) {
		dev_err(dev, "Can't fetch 'vib-overdrive-mv' property: %d\n", error);
		return error;
	}

	haptics->overdrive_voltage = drv2624_calculate_voltage(voltage);

	haptics->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						       GPIOD_OUT_HIGH);
	if (IS_ERR(haptics->reset_gpio))
		return PTR_ERR(haptics->reset_gpio);

	gpiod_set_value(haptics->reset_gpio, 0);
	usleep_range(5000, 5500);
	gpiod_set_value(haptics->reset_gpio, 1);
	usleep_range(5000, 5500);

	haptics->input_dev = devm_input_allocate_device(dev);
	if (!haptics->input_dev) {
		dev_err(dev, "Failed to allocate input device\n");
		return -ENOMEM;
	}

	haptics->input_dev->name = "drv2624_haptics";
	haptics->input_dev->dev.parent = client->dev.parent;
	haptics->input_dev->open = drv2624_open;
	haptics->input_dev->close = drv2624_close;
	input_set_drvdata(haptics->input_dev, haptics);
	input_set_capability(haptics->input_dev, EV_FF, FF_RUMBLE);

	error = input_ff_create_memless(haptics->input_dev, NULL,
					drv2624_haptics_play);
	if (error) {
		dev_err(dev, "input_ff_create() failed: %d\n", error);
		return error;
	}

	INIT_WORK(&haptics->work, drv2624_worker);

	haptics->client = client;
	i2c_set_clientdata(client, haptics);

	haptics->regmap = devm_regmap_init_i2c(client, &drv2624_regmap_config);
	if (IS_ERR(haptics->regmap)) {
		error = PTR_ERR(haptics->regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", error);
		return error;
	}

	error = drv2624_init(haptics);
	if (error) {
		dev_err(dev, "Device init failed: %d\n", error);
		return error;
	}

	error = input_register_device(haptics->input_dev);
	if (error) {
		dev_err(dev, "couldn't register input device: %d\n", error);
		return error;
	}

	return 0;
}

static const struct i2c_device_id drv2624_id[] = {
	{ "drv2624" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, drv2624_id);

static const struct of_device_id drv2624_of_match[] = {
	{ .compatible = "ti,drv2624", },
	{ }
};
MODULE_DEVICE_TABLE(of, drv2624_of_match);

static struct i2c_driver drv2624_driver = {
	.probe		= drv2624_probe,
	.driver		= {
		.name	= "drv2624-haptics",
		.of_match_table = drv2624_of_match,
	},
	.id_table = drv2624_id,
};
module_i2c_driver(drv2624_driver);

MODULE_DESCRIPTION("TI DRV2624 haptics driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vitalii Skorkin <nikroksm@mail.ru>");
