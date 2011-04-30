/*
 *  mango210_wm8960.c
 *
 *  Copyright (c) 2010 Crz-Tech Co. Ltd
 *  Author: Pyeongjeong Lee <leepjung@crz-tech.com>
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

#include "../codecs/wm8960.h"
#include "s3c-dma.h"
#include "s3c64xx-i2s.h"

#include <linux/io.h>
#include <mach/map.h>
#include <mach/regs-clock.h>

#define I2S_NUM 0

extern struct snd_soc_dai i2s_sec_fifo_dai;
extern struct snd_soc_platform s3c_dma_wrapper;
extern const struct snd_kcontrol_new s5p_idma_control;

static int set_epll_rate(unsigned long rate);

static int mango210_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int rclk, psr=1;
	int bfs, rfs, ret;
	unsigned long epll_out_rate;
	unsigned int  value = 0;
	unsigned int bit_per_sample, sample_rate;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		bit_per_sample = 24;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		bit_per_sample = 16;
		break;
	default:
		return -EINVAL;
	}


	/* For resample */
	value = params_rate(params);
	if(value != 44100) {
		printk("%s: sample rate %d --> 44100\n", __func__, value);
		value = 44100;
	}
	sample_rate = value;

	/* The Fvco for WM8960 PLLs must fall within [90,100]MHz.
	 * This criterion can't be met if we request PLL output
	 * as {8000x256, 64000x256, 11025x256}Hz.
	 * As a wayout, we rather change rfs to a minimum value that
	 * results in (params_rate(params) * rfs), and itself, acceptable
	 * to both - the CODEC and the CPU.
	 */
	switch (value) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = value * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

//	printk("IIS Audio: %uBits Stereo %uHz\n", bit_per_sample, sample_rate);

	epll_out_rate = rclk * psr;

	/* Set EPLL clock rate */
	ret = set_epll_rate(epll_out_rate);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	/* We use MUX for basic ops in SoC-Master mode */
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_MUX,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_PRESCALER, psr-1);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_RCLK, rfs);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * MANGO210 WM8960 DAI operations.
 */
static struct snd_soc_ops mango210_ops = {
	.hw_params = mango210_hw_params,
};

/* MANGO210 Playback widgets */
static const struct snd_soc_dapm_widget wm8960_dapm_widgets_pbk[] = {
	SND_SOC_DAPM_HP("Speaker-L/R", NULL),
	SND_SOC_DAPM_HP("HP-L/R", NULL),
};

/* MANGO210 Capture widgets */
static const struct snd_soc_dapm_widget wm8960_dapm_widgets_cpt[] = {
	SND_SOC_DAPM_MIC("MicIn", NULL),
};

/* SMDK-PAIFTX connections */
static const struct snd_soc_dapm_route audio_map_tx[] = {
	/* MicIn feeds LINPUT1 */
	{"LINPUT1", NULL, "MicIn"},
};

/* SMDK-PAIFRX connections */
static const struct snd_soc_dapm_route audio_map_rx[] = {
	{"Speaker-L/R", NULL, "SPK_LP"},
	{"Speaker-L/R", NULL, "SPK_LN"},
	{"Speaker-L/R", NULL, "SPK_RP"},
	{"Speaker-L/R", NULL, "SPK_RN"},
	{"HP-L/R", NULL, "HP_L"},
	{"HP-L/R", NULL, "HP_R"},
};

static int mango210_wm8960_init_paiftx(struct snd_soc_codec *codec)
{
	int ret;

	/* Add mango210 specific Capture widgets */
	snd_soc_dapm_new_controls(codec, wm8960_dapm_widgets_cpt,
				  ARRAY_SIZE(wm8960_dapm_widgets_cpt));

	/* Set up PAIFTX audio path */
	snd_soc_dapm_add_routes(codec, audio_map_tx, ARRAY_SIZE(audio_map_tx));

	/* Enabling the microphone requires the fitting of a 0R
	 * resistor to connect the line from the microphone jack.
	 */
//	snd_soc_dapm_disable_pin(codec, "MicIn");

	/* signal a DAPM event */
	snd_soc_dapm_sync(codec);

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(&wm8960_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;
	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(&s3c64xx_i2s_dai[I2S_NUM], SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set WM8960 to drive MCLK from its MCLK-pin */
	ret = snd_soc_dai_set_clkdiv(&wm8960_dai, WM8960_SYSCLKSEL,
					WM8960_SYSCLK_MCLK);
	if (ret < 0)
		return ret;

	return 0;
}

static int mango210_wm8960_init_paifrx(struct snd_soc_codec *codec)
{
	int ret;

	/* Add mango210 specific Playback widgets */
	snd_soc_dapm_new_controls(codec, wm8960_dapm_widgets_pbk,
				  ARRAY_SIZE(wm8960_dapm_widgets_pbk));

	/* add iDMA controls */
	ret = snd_soc_add_controls(codec, &s5p_idma_control, 1);
	if (ret < 0)
		return ret;

	/* Set up PAIFRX audio path */
	snd_soc_dapm_add_routes(codec, audio_map_rx, ARRAY_SIZE(audio_map_rx));

	/* signal a DAPM event */
	snd_soc_dapm_sync(codec);

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(&wm8960_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(&s3c64xx_i2s_dai[I2S_NUM], SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set WM8960 to drive MCLK from its MCLK-pin */
	ret = snd_soc_dai_set_clkdiv(&wm8960_dai, WM8960_SYSCLKSEL,
					WM8960_SYSCLK_MCLK);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_dai_link mango210_dai[] = {
{ /* Primary Playback i/f */
	.name = "WM8960 PAIF RX",
	.stream_name = "Playback",
	.cpu_dai = &s3c64xx_i2s_dai[I2S_NUM],
	.codec_dai = &wm8960_dai,
	.init = mango210_wm8960_init_paifrx,
	.ops = &mango210_ops,
},
{ /* Primary Capture i/f */
	.name = "WM8960 PAIF TX",
	.stream_name = "Capture",
	.cpu_dai = &s3c64xx_i2s_dai[I2S_NUM],
	.codec_dai = &wm8960_dai,
	.init = mango210_wm8960_init_paiftx,
	.ops = &mango210_ops,
},
};

static struct snd_soc_card mango210 = {
	.name = "mango",
	.platform = &s3c_dma_wrapper,
	.dai_link = mango210_dai,
	.num_links = ARRAY_SIZE(mango210_dai),
};

static struct snd_soc_device mango210_snd_devdata = {
	.card = &mango210,
	.codec_dev = &soc_codec_dev_wm8960,
};

static struct platform_device *mango210_snd_device;

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

static int __init mango210_audio_init(void)
{
	int ret;
	u32 val;
	u32 reg;

#include <mach/map.h>
#define S3C_VA_AUDSS	S3C_ADDR(0x01600000)	/* Audio SubSystem */
#include <mach/regs-audss.h>
	/* We use I2SCLK for rate generation, so set EPLLout as
	 * the parent of I2SCLK.
	 */
	val = readl(S5P_CLKSRC_AUDSS);
	val &= ~(0x3<<2);
	val |= (1<<0);
	writel(val, S5P_CLKSRC_AUDSS);

	val = readl(S5P_CLKGATE_AUDSS);
	val |= (0x7f<<0);
	writel(val, S5P_CLKGATE_AUDSS);

#ifdef CONFIG_S5P_LPAUDIO
	/* yman.seo CLKOUT is prior to CLK_OUT of SYSCON. XXTI & XUSBXTI work in sleep mode */
	reg = __raw_readl(S5P_OTHERS);
	reg &= ~(0x0003 << 8);
	reg |= 0x0003 << 8;	/* XUSBXTI */
	__raw_writel(reg, S5P_OTHERS);
#else
	/* yman.seo Set XCLK_OUT as 24MHz (XUSBXTI -> 24MHz) */
	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~S5P_CLKOUT_CLKSEL_MASK;
	reg |= S5P_CLKOUT_CLKSEL_XUSBXTI;
	reg &= ~S5P_CLKOUT_DIV_MASK;
	reg |= 0x0001 << S5P_CLKOUT_DIV_SHIFT;	/* DIVVAL = 1, Ratio = 2 = DIVVAL + 1 */
	__raw_writel(reg, S5P_CLK_OUT);

	reg = __raw_readl(S5P_OTHERS);
	reg &= ~(0x0003 << 8);
	reg |= 0x0000 << 8;	/* Clock from SYSCON */
	__raw_writel(reg, S5P_OTHERS);
#endif

	mango210_snd_device = platform_device_alloc("soc-audio", 0);
	if (!mango210_snd_device)
		return -ENOMEM;

	platform_set_drvdata(mango210_snd_device, &mango210_snd_devdata);
	mango210_snd_devdata.dev = &mango210_snd_device->dev;
	ret = platform_device_add(mango210_snd_device);

	if (ret)
		platform_device_put(mango210_snd_device);

	return ret;
}
module_init(mango210_audio_init);

MODULE_AUTHOR("Pyeongjeong Lee, leepjung@crz-tech.com");
MODULE_DESCRIPTION("ALSA SoC MANGO210 WM8960");
MODULE_LICENSE("GPL");
