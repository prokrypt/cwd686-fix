/*
 * SPDX-License-Identifier: GPL-2.0+
 * Copyright (c) 2021 Clockwork Tech LLC
 * Copyright (c) 2021 Max Fierke <max@maxfierke.com>
 *
 */

#include <drm/drm_modes.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/module.h>

struct cwd686 {
	struct device *dev;
	struct drm_panel panel;
	struct regulator *supply;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;
	struct backlight_device *backlight;
	bool prepared;
	bool enabled;
	enum drm_panel_orientation orientation;
};

/*
 * Tunables for debugging the clipped-edge problem. Defaults match the
 * ClockworkPi vendor driver (480 wide); yatli's tree used hact=485.
 */
static int hact = 480, hfp = 150, hsw = 24, hbp = 40;
static int vact = 1280, vfp = 12, vsw = 6, vbp = 10;
static int clock = 54465;
static bool burst = true, sync_pulse = true;
static int madctl = 0x14;
static int c1[8] = { 0x0C, 0x16, 0x04, 0x0C, 0x10, 0x04 };
static int c1_n = 6;
static int c2[2] = { 0x11, 0x41 };
static int c2_n = 2;
static bool bist;
static bool d1seq;
module_param(hact, int, 0444);
module_param(hfp, int, 0444);
module_param(hsw, int, 0444);
module_param(hbp, int, 0444);
module_param(vact, int, 0444);
module_param(vfp, int, 0444);
module_param(vsw, int, 0444);
module_param(vbp, int, 0444);
module_param(clock, int, 0444);
module_param(burst, bool, 0444);
module_param(sync_pulse, bool, 0444);
module_param(madctl, int, 0444);
module_param_array(c1, int, &c1_n, 0444);
MODULE_PARM_DESC(c1, "TCON 0xC1 bytes, 6 (vendor) or 8 (datasheet layout)");
module_param_array(c2, int, &c2_n, 0444);
MODULE_PARM_DESC(c2, "TCON_2 0xC2 bytes (resolution)");
module_param(bist, bool, 0444);
module_param(d1seq, bool, 0444);
MODULE_PARM_DESC(d1seq, "Use the init table from ClockworkPi's Allwinner D1 driver (icn9707_480x1280.c)");
MODULE_PARM_DESC(bist, "Show the controller's built-in test pattern (0xC0)");

static struct drm_display_mode default_mode;

static void cwd686_build_mode(void)
{
	default_mode = (struct drm_display_mode) {
		.clock = clock,
		.hdisplay = hact,
		.hsync_start = hact + hfp,
		.hsync_end = hact + hfp + hsw,
		.htotal = hact + hfp + hsw + hbp,
		.vdisplay = vact,
		.vsync_start = vact + vfp,
		.vsync_end = vact + vfp + vsw,
		.vtotal = vact + vfp + vsw + vbp,
	};
}

static inline struct cwd686 *panel_to_cwd686(struct drm_panel *panel)
{
	return container_of(panel, struct cwd686, panel);
}

#define dcs_write_seq(seq...)                              \
({                                                              \
	static const u8 d[] = { seq };                          \
	mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));	 \
})

/* Same panel, init table from ClockworkPi's D1 (R-01) sunxi driver icn9707_480x1280.c */
static void cwd686_init_sequence_d1(struct cwd686 *ctx)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	dcs_write_seq(0xF0,0x5A,0x59);
	dcs_write_seq(0xF1,0xA5,0xA6);
	dcs_write_seq(0xB0,0x54,0x32,0x23,0x45,0x44,0x44,0x44,0x44,0x9F,0x00,0x01,0x9F,0x00,0x01);
	dcs_write_seq(0xB1,0x32,0x84,0x02,0x83,0x29,0x06,0x06,0x72,0x06,0x06);
	dcs_write_seq(0xB2,0x73);
	dcs_write_seq(0xB3,0x0B,0x09,0x13,0x11,0x0F,0x0D,0x00,0x00,0x00,0x03,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x05,0x07);
	dcs_write_seq(0xB4,0x0A,0x08,0x12,0x10,0x0E,0x0C,0x00,0x00,0x00,0x03,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x04,0x06);
	dcs_write_seq(0xB6,0x13,0x13);
	dcs_write_seq(0xB8,0xB4,0x43,0x02,0xCC);
	dcs_write_seq(0xB9,0xA5,0x20,0xFF,0xC8);
	dcs_write_seq(0xBA,0x88,0x23);
	dcs_write_seq(0xBD,0x43,0x0E,0x0E,0x50,0x50,0x29,0x10,0x03,0x44,0x03);
	dcs_write_seq(0xC1,0x00,0x0C,0x16,0x04,0x00,0x30,0x10,0x04);
	dcs_write_seq(0xC2,0x21,0x81);
	dcs_write_seq(0xC3,0x02,0x30);
	dcs_write_seq(0xC7,0x25,0x6A);
	dcs_write_seq(0xC8,0x7C,0x68,0x59,0x4E,0x4B,0x3C,0x41,0x2B,0x44,0x43,0x43,0x60,0x4E,0x55,0x47,0x44,0x38,0x27,0x06,0x7C,0x68,0x59,0x4E,0x4B,0x3C,0x41,0x2B,0x44,0x43,0x43,0x60,0x4E,0x55,0x47,0x44,0x38,0x27,0x06);
	dcs_write_seq(0xD4,0x00,0x00,0x00,0x32,0x04,0x51);
	dcs_write_seq(0xF1,0x5A,0x59);
	dcs_write_seq(0xF0,0xA5,0xA6);
	dcs_write_seq(0x36,0x14);
	dcs_write_seq(0x35,0x00);
}

static void cwd686_init_sequence(struct cwd686 *ctx)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	if (d1seq) {
		cwd686_init_sequence_d1(ctx);
		return;
	}

	dcs_write_seq(0xF0,0x5A,0x5A); // Password1 (Enable Level 2 registers)
	dcs_write_seq(0xF1,0xA5,0xA5); // Password2 (Enable Level 2 registers)
	dcs_write_seq(0xB6,0x0D,0x0D); // PWRCON_VCOM (-0.495V ?, -0.495V ?)
	dcs_write_seq(0xB4,0x0A,0x08,0x12,0x10,0x0E,0x0C,0x00,0x00,0x00,0x03,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x04,0x06); // CGOUTR (set ASG Output signals)
	dcs_write_seq(0xB3,0x0B,0x09,0x13,0x11,0x0F,0x0D,0x00,0x00,0x00,0x03,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x05,0x07); // CGOUTL (set ASG Output signals)
	dcs_write_seq(0xB0,0x54,0x32,0x23,0x45,0x44,0x44,0x44,0x44,0x90,0x01,0x90,0x01); // ???
	dcs_write_seq(0xB1,0x32,0x84,0x02,0x83,0x30,0x01,0x6B,0x01); // ???
	dcs_write_seq(0xB2,0x73); // ???
	dcs_write_seq(0xBD,0x4E,0x0E,0x50,0x50,0x26,0x1D,0x00,0x14,0x42,0x03); // PWRCON_REG
	dcs_write_seq(0xB7,0x01,0x01,0x09,0x11,0x0D,0x55,0x19,0x19,0x21,0x1D,0x00,0x00,0x00,0x00,0x02,0xFF,0x3C); // PWRCON_SEQ
	dcs_write_seq(0xB8,0x23,0x01,0x30,0x34,0x63); // PWRCON_CLK
	dcs_write_seq(0xB9,0xA0,0x22,0x00,0x44); // PWRCON_BAT (Disable abnormal power-off?)
	dcs_write_seq(0xBA,0x12,0x63); // PWRCON_MODE
	{
		u8 d[9] = { 0xC1 };
		int i;

		for (i = 0; i < c1_n; i++)
			d[i + 1] = c1[i];
		mipi_dsi_dcs_write_buffer(dsi, d, c1_n + 1);
	} // TCON (Set VBP, VFP, VSW, HBP, HFP, HSW)
	{
		u8 d[3] = { 0xC2, c2[0], c2[1] };

		mipi_dsi_dcs_write_buffer(dsi, d, 3);
	} // TCON2 (Set resolution)
	dcs_write_seq(0xC3,0x22,0x31,0x04); // TCON3 (Set frame blanking)
	dcs_write_seq(0xC7,0x05,0x23,0x6B,0x49,0x00); // SRCCON
	dcs_write_seq(0xC5,0x00); // ???
	dcs_write_seq(0xD0,0x37,0xFF,0xFF); // ABNO_CTR (Set MIPI abnormal state)
	dcs_write_seq(0xD2,0x63,0x0B,0x08,0x88); // ???
	dcs_write_seq(0xD3,0x01,0x00,0x00,0x01,0x01,0x37,0x25,0x38,0x31,0x06,0x07); // ???
	dcs_write_seq(0xC8,0x7C,0x6A,0x5D,0x53,0x53,0x45,0x4B,0x35,0x4D,0x4A,0x49,0x66,0x53,0x57,0x4A,0x48,0x3B,0x2A,0x06,0x7C,0x6A,0x5D,0x53,0x53,0x45,0x4B,0x35,0x4D,0x4A,0x49,0x66,0x53,0x57,0x4A,0x48,0x3B,0x2A,0x06); // GAMMA2.2
	dcs_write_seq(0xC6,0x00,0x00,0xFF,0x00,0x00,0xFF,0x00,0x00); // SRC_TIM (Set source timing)
	dcs_write_seq(0xF4,0x08,0x77); // ???
	{
		u8 d[2] = { 0x36, madctl };

		mipi_dsi_dcs_write_buffer(dsi, d, 2);
	} // MADCTL (Set display direction)
	if (bist)
		dcs_write_seq(0xC0,0x11,0xFF,0xFF); // BIST on (D0 = BIST_ON)
	dcs_write_seq(0x35,0x00); // TEON (Disable tearing effect ?)
	dcs_write_seq(0xF1,0x5A,0x5A); // Password1 (Enable Level 2 registers), again for some reason ??
	dcs_write_seq(0xF0,0xA5,0xA5); // Password2 (Enable Level 2 registers), again for some reason ??
}

static int cwd686_disable(struct drm_panel *panel)
{
	struct cwd686 *ctx = panel_to_cwd686(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (!ctx->enabled)
		return 0;

	backlight_disable(ctx->backlight);

	ctx->enabled = false;

	return 0;
}

static int cwd686_unprepare(struct drm_panel *panel)
{
	struct cwd686 *ctx = panel_to_cwd686(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret) {
		dev_err(ctx->dev, "failed to turn display off (%d)\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret) {
		dev_err(ctx->dev, "failed to enter sleep mode (%d)\n", ret);
		return ret;
	}
	msleep(120);

	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(5);

	ctx->prepared = false;

	return 0;
}

static int cwd686_prepare(struct drm_panel *panel)
{
	struct cwd686 *ctx = panel_to_cwd686(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (ctx->prepared)
		return 0;

	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(10);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(120);

	/* Enabe tearing mode: send TE (tearing effect) at VBLANK */
	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret) {
		dev_err(ctx->dev, "failed to enable vblank TE (%d)\n", ret);
		return ret;
	}
	/* Exit sleep mode and power on */

	cwd686_init_sequence(ctx);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret) {
		dev_err(ctx->dev, "failed to exit sleep mode (%d)\n", ret);
		return ret;
	}
	msleep(120);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret) {
		dev_err(ctx->dev, "failed to turn display on (%d)\n", ret);
		return ret;
	}
	msleep(20);

	ctx->prepared = true;

	return 0;
}

static int cwd686_enable(struct drm_panel *panel)
{
	struct cwd686 *ctx = panel_to_cwd686(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (ctx->enabled)
		return 0;

	backlight_enable(ctx->backlight);

	ctx->enabled = true;

	return 0;
}

static int cwd686_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct cwd686 *ctx = panel_to_cwd686(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(panel->dev, "bad mode or failed to add mode\n");
		return -EINVAL;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	/* set up connector's "panel orientation" property */
	drm_connector_set_panel_orientation(connector, ctx->orientation);

	drm_mode_probed_add(connector, mode);

	return 1; /* Number of modes */
}

static const struct drm_panel_funcs cwd686_drm_funcs = {
	.disable = cwd686_disable,
	.unprepare = cwd686_unprepare,
	.prepare = cwd686_prepare,
	.enable = cwd686_enable,
	.get_modes = cwd686_get_modes,
};

static int cwd686_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct cwd686 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO;
	if (burst)
		dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;
	if (sync_pulse)
		dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
	cwd686_build_mode();
	dev_info(dev, "mode %dx%d h %d/%d/%d v %d/%d/%d clk %d burst %d pulse %d madctl %#x c1[%d] %02x %02x %02x %02x %02x %02x %02x %02x c2 %02x %02x bist %d d1seq %d\n",
		 hact, vact, hfp, hsw, hbp, vfp, vsw, vbp, clock, burst, sync_pulse, madctl,
		 c1_n, c1[0], c1[1], c1[2], c1[3], c1[4], c1[5], c1[6], c1[7], c2[0], c2[1], bist, d1seq);

	ctx->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		ret = PTR_ERR(ctx->reset_gpio);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to request GPIO (%d)\n", ret);
		return ret;
	}

	ctx->backlight = devm_of_find_backlight(dev);
	if (IS_ERR(ctx->backlight)) {
		return PTR_ERR(ctx->backlight);
	}

	ret = of_drm_get_panel_orientation(dev->of_node, &ctx->orientation);
	if (ret) {
		dev_err(dev, "%pOF: failed to get orientation %d\n", dev->of_node, ret);
		return ret;
	}

	drm_panel_init(&ctx->panel, dev, &cwd686_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "mipi_dsi_attach() failed: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void cwd686_remove(struct mipi_dsi_device *dsi)
{
	struct cwd686 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id cwd686_of_match[] = {
	{ .compatible = "cw,cwd686" },
	{ }
};
MODULE_DEVICE_TABLE(of, cwd686_of_match);

static struct mipi_dsi_driver cwd686_driver = {
	.probe = cwd686_probe,
	.remove = cwd686_remove,
	.driver = {
		.name = "panel-cwd686-test",
		.of_match_table = cwd686_of_match,
	},
};
module_mipi_dsi_driver(cwd686_driver);

MODULE_DESCRIPTION("cwd686 MIPI DSI panel, test build with tunable registers");
MODULE_LICENSE("GPL v2");
