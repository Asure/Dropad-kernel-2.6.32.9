/*
 * smdk_wm9713.c  --  SoC audio for SMDK
 *
 * Copyright 2010 Samsung Electronics Co. Ltd.
 * Author: Jaswinder Singh Brar <jassi.brar@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/device.h>
#include <sound/soc.h>

#include "../codecs/wm9713.h"
#include "s3c-dma.h"
#include "s3c-ac97.h"

static struct snd_soc_card smdk;

/*
 Playback (HeadPhone):-
	$ amixer sset 'Headphone' unmute
	$ amixer sset 'Right Headphone Out Mux' 'Headphone'
	$ amixer sset 'Left Headphone Out Mux' 'Headphone'
	$ amixer sset 'Right HP Mixer PCM' unmute
	$ amixer sset 'Left HP Mixer PCM' unmute

 Capture (LineIn):-
	$ amixer sset 'Right Capture Source' 'Line'
	$ amixer sset 'Left Capture Source' 'Line'
*/

static struct snd_soc_dai_link smdk_dai[] = {
{
	.name = "AC97 PCM RX",
	.stream_name = "AC97 PCM Playback",
	.cpu_dai = &s3c_ac97_dai[S3C_AC97_DAI_PCM],
	.codec_dai = &wm9713_dai[WM9713_DAI_AC97_HIFI],
},
{
	.name = "AC97 PCM TX",
	.stream_name = "AC97 PCM Capture",
	.cpu_dai = &s3c_ac97_dai[S3C_AC97_DAI_PCM],
	.codec_dai = &wm9713_dai[WM9713_DAI_AC97_HIFI],
}
};

static struct snd_soc_card smdk = {
	.name = "SMDK",
	.platform = &s3c24xx_soc_platform,
	.dai_link = smdk_dai,
	.num_links = ARRAY_SIZE(smdk_dai),
};

static struct snd_soc_device smdk_snd_ac97_devdata = {
	.card = &smdk,
	.codec_dev = &soc_codec_dev_wm9713,
};

static struct platform_device *smdk_snd_ac97_device;

static int __init smdk_init(void)
{
	int ret;

	smdk_snd_ac97_device = platform_device_alloc("soc-audio", -1);
	if (!smdk_snd_ac97_device)
		return -ENOMEM;

	platform_set_drvdata(smdk_snd_ac97_device,
			     &smdk_snd_ac97_devdata);
	smdk_snd_ac97_devdata.dev = &smdk_snd_ac97_device->dev;

	ret = platform_device_add(smdk_snd_ac97_device);
	if (ret)
		platform_device_put(smdk_snd_ac97_device);

	return ret;
}

static void __exit smdk_exit(void)
{
	platform_device_unregister(smdk_snd_ac97_device);
}

module_init(smdk_init);
module_exit(smdk_exit);

/* Module information */
MODULE_AUTHOR("Jaswinder Singh Brar, jassi.brar@samsung.com");
MODULE_DESCRIPTION("ALSA SoC SMDK+WM9713");
MODULE_LICENSE("GPL");
