// SPDX-License-Identifier: GPL-2.0
/*
 * rpi-simple-soundcard.c -- ALSA SoC Raspberry Pi soundcard.
 *
 * Copyright (C) 2018 Raspberry Pi.
 *
 * Authors: Tim Gover <tim.gover@raspberrypi.org>
 *
 * Based on code:
 * hifiberry_amp.c, hifiberry_dac.c, rpi-dac.c
 * by Florian Meier <florian.meier@koalo.de>
 *
 * googlevoicehat-soundcard.c
 * by Peter Malkin <petermalkin@google.com>
 *
 * adau1977-adc.c
 * by Andrey Grodzovsky <andrey2805@gmail.com>
 *
 * merus-amp.c
 * by Ariel Muszkat <ariel.muszkat@gmail.com>
 *		Jorgen Kragh Jakobsen <jorgen.kraghjakobsen@infineon.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

/* Parameters for generic RPI functions */
struct snd_rpi_simple_drvdata {
	struct snd_soc_dai_link *dai;
	const char* card_name;
	unsigned int fixed_bclk_ratio;
};

static int snd_rpi_simple_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_rpi_simple_drvdata *drvdata =
		snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

	if (drvdata->fixed_bclk_ratio > 0)
		return snd_soc_dai_set_bclk_ratio(cpu_dai,
				drvdata->fixed_bclk_ratio);

	return 0;
}

static int snd_rpi_simple_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_rpi_simple_drvdata *drvdata;
	unsigned int sample_bits;

	drvdata = snd_soc_card_get_drvdata(rtd->card);

	if (drvdata->fixed_bclk_ratio > 0)
		return 0; // BCLK is configured in .init

	/* The simple drivers just set the bclk_ratio to sample_bits * 2 so
	 * hard-code this for now. More complex drivers could just replace
	 * the hw_params routine.
	 */
	sample_bits = snd_pcm_format_physical_width(params_format(params));
	return snd_soc_dai_set_bclk_ratio(cpu_dai, sample_bits * 2);
}

static struct snd_soc_ops snd_rpi_simple_ops = {
	.hw_params = snd_rpi_simple_hw_params,
};

enum adau1977_clk_id {
	ADAU1977_SYSCLK,
};

enum adau1977_sysclk_src {
	ADAU1977_SYSCLK_SRC_MCLK,
	ADAU1977_SYSCLK_SRC_LRCLK,
};

static int adau1977_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);

	ret = snd_soc_dai_set_tdm_slot(codec_dai, 0, 0, 0, 0);
	if (ret < 0)
		return ret;

	return snd_soc_component_set_sysclk(codec_dai->component,
			ADAU1977_SYSCLK, ADAU1977_SYSCLK_SRC_MCLK,
			11289600, SND_SOC_CLOCK_IN);
}

SND_SOC_DAILINK_DEFS(adau1977,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC("adau1977.1-0011", "adau1977-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link snd_rpi_adau1977_dai[] = {
	{
	.name           = "adau1977",
	.stream_name    = "ADAU1977",
	.init           = adau1977_init,
	.dai_fmt = SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBM_CFM,
	SND_SOC_DAILINK_REG(adau1977),
	},
};

static struct snd_rpi_simple_drvdata drvdata_adau1977 = {
	.card_name = "snd_rpi_adau1977_adc",
	.dai       = snd_rpi_adau1977_dai,
};

SND_SOC_DAILINK_DEFS(gvchat,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC("voicehat-codec", "voicehat-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link snd_googlevoicehat_soundcard_dai[] = {
{
	.name           = "Google voiceHAT SoundCard",
	.stream_name    = "Google voiceHAT SoundCard HiFi",
	.dai_fmt        =  SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
	SND_SOC_DAILINK_REG(gvchat),
},
};

static struct snd_rpi_simple_drvdata drvdata_googlevoicehat = {
	.card_name = "snd_rpi_googlevoicehat_soundcard",
	.dai       = snd_googlevoicehat_soundcard_dai,
};

SND_SOC_DAILINK_DEFS(hifiberry_dacplusdsp,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC("dacplusdsp-codec", "dacplusdsp-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link snd_hifiberrydacplusdsp_soundcard_dai[] = {
{
	.name           = "Hifiberry DAC+DSP SoundCard",
	.stream_name    = "Hifiberry DAC+DSP SoundCard HiFi",
	.dai_fmt        =  SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS,
	SND_SOC_DAILINK_REG(hifiberry_dacplusdsp),
},
};

static struct snd_rpi_simple_drvdata drvdata_hifiberrydacplusdsp = {
	.card_name = "snd_rpi_hifiberrydacplusdsp_soundcard",
	.dai       = snd_hifiberrydacplusdsp_soundcard_dai,
};

SND_SOC_DAILINK_DEFS(hifiberry_amp,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC("tas5713.1-001b", "tas5713-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link snd_hifiberry_amp_dai[] = {
	{
		.name           = "HifiBerry AMP",
		.stream_name    = "HifiBerry AMP HiFi",
		.dai_fmt        = SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS,
		SND_SOC_DAILINK_REG(hifiberry_amp),
	},
};

static struct snd_rpi_simple_drvdata drvdata_hifiberry_amp = {
	.card_name        = "snd_rpi_hifiberry_amp",
	.dai              = snd_hifiberry_amp_dai,
	.fixed_bclk_ratio = 64,
};

SND_SOC_DAILINK_DEFS(hifiberry_dac,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC("pcm5102a-codec", "pcm5102a-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link snd_hifiberry_dac_dai[] = {
	{
		.name           = "HifiBerry DAC",
		.stream_name    = "HifiBerry DAC HiFi",
		.dai_fmt        = SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS,
		SND_SOC_DAILINK_REG(hifiberry_dac),
	},
};

static struct snd_rpi_simple_drvdata drvdata_hifiberry_dac = {
	.card_name = "snd_rpi_hifiberry_dac",
	.dai       = snd_hifiberry_dac_dai,
};

SND_SOC_DAILINK_DEFS(rpi_dac,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC("pcm1794a-codec", "pcm1794a-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link snd_rpi_dac_dai[] = {
{
	.name		= "RPi-DAC",
	.stream_name	= "RPi-DAC HiFi",
	.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
	SND_SOC_DAILINK_REG(rpi_dac),
},
};

static struct snd_rpi_simple_drvdata drvdata_rpi_dac = {
	.card_name        = "snd_rpi_rpi_dac",
	.dai              = snd_rpi_dac_dai,
	.fixed_bclk_ratio = 64,
};

SND_SOC_DAILINK_DEFS(merus_amp,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC("ma120x0p.1-0020","ma120x0p-amp")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link snd_merus_amp_dai[] = {
	{
		.name           = "MerusAmp",
		.stream_name    = "Merus Audio Amp",
		.dai_fmt        = SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS,
		SND_SOC_DAILINK_REG(merus_amp),
	},
};

static struct snd_rpi_simple_drvdata drvdata_merus_amp = {
	.card_name        = "snd_rpi_merus_amp",
	.dai              = snd_merus_amp_dai,
	.fixed_bclk_ratio = 64,
};

static const struct of_device_id snd_rpi_simple_of_match[] = {
	{ .compatible = "adi,adau1977-adc",
		.data = (void *) &drvdata_adau1977 },
	{ .compatible = "googlevoicehat,googlevoicehat-soundcard",
		.data = (void *) &drvdata_googlevoicehat },
	{ .compatible = "hifiberrydacplusdsp,hifiberrydacplusdsp-soundcard",
		.data = (void *) &drvdata_hifiberrydacplusdsp },
	{ .compatible = "hifiberry,hifiberry-amp",
		.data = (void *) &drvdata_hifiberry_amp },
	{ .compatible = "hifiberry,hifiberry-dac",
		.data = (void *) &drvdata_hifiberry_dac },
	{ .compatible = "rpi,rpi-dac", &drvdata_rpi_dac},
	{ .compatible = "merus,merus-amp",
		.data = (void *) &drvdata_merus_amp },
	{},
};

static struct snd_soc_card snd_rpi_simple = {
	.driver_name  = "RPi-simple",
	.owner        = THIS_MODULE,
	.dai_link     = NULL,
	.num_links    = 1, /* Only a single DAI supported at the moment */
};

static int snd_rpi_simple_probe(struct platform_device *pdev)
{
	int ret = 0;
	const struct of_device_id *of_id;

	snd_rpi_simple.dev = &pdev->dev;
	of_id = of_match_node(snd_rpi_simple_of_match, pdev->dev.of_node);

	if (pdev->dev.of_node && of_id->data) {
		struct device_node *i2s_node;
		struct snd_rpi_simple_drvdata *drvdata =
			(struct snd_rpi_simple_drvdata *) of_id->data;
		struct snd_soc_dai_link *dai = drvdata->dai;

		snd_soc_card_set_drvdata(&snd_rpi_simple, drvdata);

		/* More complex drivers might override individual functions */
		if (!dai->init)
			dai->init = snd_rpi_simple_init;
		if (!dai->ops)
			dai->ops = &snd_rpi_simple_ops;

		snd_rpi_simple.name = drvdata->card_name;

		snd_rpi_simple.dai_link = dai;
		i2s_node = of_parse_phandle(pdev->dev.of_node,
				"i2s-controller", 0);
		if (!i2s_node) {
			pr_err("Failed to find i2s-controller DT node\n");
			return -ENODEV;
		}

		dai->cpus->of_node = i2s_node;
		dai->platforms->of_node = i2s_node;
	}

	ret = devm_snd_soc_register_card(&pdev->dev, &snd_rpi_simple);
	if (ret && ret != -EPROBE_DEFER)
		dev_err(&pdev->dev, "Failed to register card %d\n", ret);

	return ret;
}

static struct platform_driver snd_rpi_simple_driver = {
	.driver = {
		.name   = "snd-rpi-simple",
		.owner  = THIS_MODULE,
		.of_match_table = snd_rpi_simple_of_match,
	},
	.probe          = snd_rpi_simple_probe,
};
MODULE_DEVICE_TABLE(of, snd_rpi_simple_of_match);

module_platform_driver(snd_rpi_simple_driver);

MODULE_AUTHOR("Tim Gover <tim.gover@raspberrypi.org>");
MODULE_DESCRIPTION("ASoC Raspberry Pi simple soundcard driver ");
MODULE_LICENSE("GPL v2");
