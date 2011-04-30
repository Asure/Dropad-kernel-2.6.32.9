/*
 *  smdkv2xx_spdif.c
 *
 *  Copyright (c) 2010 Samsung Electronics Co. Ltd
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */


#include <linux/platform_device.h>
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "s3c-dma.h"
#include "s5p-spdif.h"
#include "../codecs/spdif_transciever.h"

static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return -ENOENT;
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_disable(fout_epll);
	clk_set_rate(fout_epll, rate);
	clk_enable(fout_epll);

out:
	clk_put(fout_epll);

	return 0;
}

static int mango_spdif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret = 0;
	unsigned long pll_out;

	dev_dbg(cpu_dai->dev, "%s:%d\n", __func__, __LINE__);

	switch (params_rate(params)) {
	case 44100:
		pll_out = 45158400;
		break;
	case 32000:
	case 48000:
	case 96000:
		pll_out = 49152000;
		break;
	default:
		return -EINVAL;
	}
	set_epll_rate(pll_out);

//	printk("SPDIF Audio: 16Bits Stereo %uHz\n", params_rate(params));

	/* S/PDIF use internal clock source */
	ret = snd_soc_dai_set_sysclk(cpu_dai, S5P_SPDIF_CLKSRC_INT,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops mango_spdif_ops = {
	.hw_params = mango_spdif_hw_params,
};

static struct snd_soc_dai_link mango_dai[] = {
	{
		.name = "S/PDIF PCM Playback",
		.stream_name = "Playback",
		.cpu_dai = &s5p_spdif_dai,
		.codec_dai = &dit_stub_dai,
		.ops = &mango_spdif_ops,
	},
};

static struct snd_soc_card mango = {
	.name = "mango",
	.platform = &s3c24xx_soc_platform,
	.dai_link = mango_dai,
	.num_links = ARRAY_SIZE(mango_dai),
};

static struct snd_soc_device mango_snd_devdata = {
	.card = &mango,
	.codec_dev = &soc_codec_dev_spdif_dit,
};

static struct platform_device *mango_snd_device;
static struct platform_device *mango_spdif_dummy_codec;

static int __init mango_spdif_audio_init(void)
{
	int ret = 0;

	/* Bring up S/PDIF dummy codec device */
	mango_spdif_dummy_codec = platform_device_alloc("spdif-dit", -1);
	if (!mango_spdif_dummy_codec)
		return -ENOMEM;

	ret = platform_device_add(mango_spdif_dummy_codec);
	if (ret)
		goto err2;

	mango_snd_device = platform_device_alloc("soc-audio", 1);
	if (!mango_snd_device) {
		ret = -ENOMEM;
		goto err2;
	}

	platform_set_drvdata(mango_snd_device, &mango_snd_devdata);
	mango_snd_devdata.dev = &mango_snd_device->dev;
	ret = platform_device_add(mango_snd_device);
	if (ret)
		goto err1;

	return 0;

err1:
	platform_device_put(mango_snd_device);
err2:
	platform_device_put(mango_spdif_dummy_codec);
	return ret;
}
module_init(mango_spdif_audio_init);

MODULE_AUTHOR("Seungwhan Youn, <sw.youn@samsung.com>");
MODULE_DESCRIPTION("ALSA SoC SMDK S/PDIF");
MODULE_LICENSE("GPL");
