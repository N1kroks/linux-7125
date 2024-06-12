// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2024 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct nt36675_tianma {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *supply;
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline struct nt36675_tianma *to_nt36675_tianma(struct drm_panel *panel)
{
	return container_of(panel, struct nt36675_tianma, panel);
}

static void nt36675_tianma_reset(struct nt36675_tianma *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int nt36675_tianma_on(struct nt36675_tianma *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x3b, 0x03, 0x1e, 0x0a, 0x04, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x00);

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x00b8);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x24);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x27);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x07, 0x01);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_VSYNC_TIMING, 0x25);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x23);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x0a, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0x0b, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0x0c, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0x0d, 0x2a);
	mipi_dsi_dcs_write_seq(dsi, 0x10, 0x50);
	mipi_dsi_dcs_write_seq(dsi, 0x11, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x12, 0x95);
	mipi_dsi_dcs_write_seq(dsi, 0x15, 0x68);
	mipi_dsi_dcs_write_seq(dsi, 0x16, 0x0b);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_PARTIAL_ROWS, 0xff);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_PARTIAL_COLUMNS, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0x32, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0x33, 0xfe);
	mipi_dsi_dcs_write_seq(dsi, 0x34, 0xfd);
	mipi_dsi_dcs_write_seq(dsi, 0x35, 0xfa);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_ADDRESS_MODE, 0xf6);
	mipi_dsi_dcs_write_seq(dsi, 0x37, 0xf2);
	mipi_dsi_dcs_write_seq(dsi, 0x38, 0xf0);
	mipi_dsi_dcs_write_seq(dsi, 0x39, 0xee);

	ret = mipi_dsi_dcs_set_pixel_format(dsi, 0xec);
	if (ret < 0) {
		dev_err(dev, "Failed to set pixel format: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, 0x3b, 0xea);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_3D_CONTROL, 0xe8);
	mipi_dsi_dcs_write_seq(dsi, 0x3f, 0xe7);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_VSYNC_TIMING, 0xe6);
	mipi_dsi_dcs_write_seq(dsi, 0x41, 0xe5);
	mipi_dsi_dcs_write_seq(dsi, 0xa0, 0x11);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(80);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}
	usleep_range(5000, 6000);

	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x27);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x3f, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x43, 0x08);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x10);

	return 0;
}

static int nt36675_tianma_off(struct nt36675_tianma *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x10);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	usleep_range(10000, 11000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(140);

	return 0;
}

static int nt36675_tianma_prepare(struct drm_panel *panel)
{
	struct nt36675_tianma *ctx = to_nt36675_tianma(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_enable(ctx->supply);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulator: %d\n", ret);
		return ret;
	}

	nt36675_tianma_reset(ctx);

	ret = nt36675_tianma_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		regulator_disable(ctx->supply);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int nt36675_tianma_unprepare(struct drm_panel *panel)
{
	struct nt36675_tianma *ctx = to_nt36675_tianma(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = nt36675_tianma_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_disable(ctx->supply);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode nt36675_tianma_mode = {
	.clock = (1080 + 20 + 4 + 22) * (2400 + 10 + 2 + 30) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 20,
	.hsync_end = 1080 + 20 + 4,
	.htotal = 1080 + 20 + 4 + 22,
	.vdisplay = 2400,
	.vsync_start = 2400 + 10,
	.vsync_end = 2400 + 10 + 2,
	.vtotal = 2400 + 10 + 2 + 30,
	.width_mm = 83,
	.height_mm = 147,
};

static int nt36675_tianma_get_modes(struct drm_panel *panel,
				    struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &nt36675_tianma_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs nt36675_tianma_panel_funcs = {
	.prepare = nt36675_tianma_prepare,
	.unprepare = nt36675_tianma_unprepare,
	.get_modes = nt36675_tianma_get_modes,
};

static int nt36675_tianma_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct nt36675_tianma *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->supply = devm_regulator_get(dev, "vddio");
	if (IS_ERR(ctx->supply))
		return dev_err_probe(dev, PTR_ERR(ctx->supply),
				     "Failed to get vddio regulator\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &nt36675_tianma_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void nt36675_tianma_remove(struct mipi_dsi_device *dsi)
{
	struct nt36675_tianma *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id nt36675_tianma_of_match[] = {
	{ .compatible = "mdss,nt36675-tianma" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nt36675_tianma_of_match);

static struct mipi_dsi_driver nt36675_tianma_driver = {
	.probe = nt36675_tianma_probe,
	.remove = nt36675_tianma_remove,
	.driver = {
		.name = "panel-nt36675-tianma",
		.of_match_table = nt36675_tianma_of_match,
	},
};
module_mipi_dsi_driver(nt36675_tianma_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for nt36675 video mode dsi tianma panel");
MODULE_LICENSE("GPL");
