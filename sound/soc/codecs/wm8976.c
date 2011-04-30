/*
 * wm8976.c  --  WM8976 ALSA Soc Audio driver
 *
 * Copyright 2006 Wolfson Microelectronics PLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "wm8976.h"

#define AUDIO_NAME "wm8976"
#define WM8976_VERSION "0.4"


struct snd_soc_codec_device soc_codec_dev_wm8976;
struct snd_soc_codec *g_codec;

/*
 * wm8976 register cache
 * We can't read the WM8976 register space when we are
 * using 2 wire for device control, so we cache them instead.
 */
static const u16 wm8976_reg[WM8976_CACHEREGNUM] = {
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0050, 0x0000, 0x0140, 0x0000,
    0x0000, 0x0000, 0x0000, 0x00ff,
    0x00ff, 0x0000, 0x0100, 0x01ff,             //r15 bit 8 set 1 zengsiling
    0x00ff, 0x0000, 0x012c, 0x002c,
    0x002c, 0x002c, 0x002c, 0x0000,
    0x0032, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0038, 0x000b, 0x0032, 0x0000,
    0x0008, 0x000c, 0x0093, 0x00e9,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0033, 0x0010, 0x0010, 0x0100,
    0x0100, 0x0002, 0x0001, 0x0001,
    0x0032, 0x0032, 0x0039, 0x0039,
    0x0001, 0x0001,
};

/*
 * read wm8976 register cache
 */
static inline unsigned int wm8976_read_reg_cache(struct snd_soc_codec  *codec,
	unsigned int reg)
{
	u16 *cache = codec->reg_cache;
	if (reg == WM8976_RESET)
		return 0;
	if (reg >= WM8976_CACHEREGNUM)
		return -1;
	return cache[reg];
}

/*
 * write wm8976 register cache
 */
static inline void wm8976_write_reg_cache(struct snd_soc_codec  *codec,
	u16 reg, unsigned int value)
{
	u16 *cache = codec->reg_cache;
	if (reg >= WM8976_CACHEREGNUM)
		return;
	cache[reg] = value;
}

/*
 * write to the WM8976 register space
 */
int wm8976_write(struct snd_soc_codec  *codec, unsigned int reg,
	unsigned int value)
{
	u8 data[2];

	/* data is
	 *   D15..D9 WM8976 register offset
	 *   D8...D0 register data
	 */
	data[0] = (reg << 1) | ((value >> 8) & 0x0001);
	data[1] = value & 0x00ff;

	wm8976_write_reg_cache (codec, reg, value);
	if (codec->hw_write(codec->control_data, data, 2) == 2)
		return 0;
	else
		return -1;
}

EXPORT_SYMBOL_GPL(wm8976_write);
#define wm8976_reset(c)	wm8976_write(c, WM8976_RESET, 0)

static const char *wm8976_companding[] = { "Off", "NC", "u-law", "A-law" };
//static const char *wm8976_deemp[] = { "None", "32kHz", "44.1kHz", "48kHz" };
static const char *wm8976_eqmode[] = { "Capture", "Playback" };
static const char *wm8976_bw[] = {"Narrow", "Wide" };
static const char *wm8976_eq1[] = {"80Hz", "105Hz", "135Hz", "175Hz" };
static const char *wm8976_eq2[] = {"230Hz", "300Hz", "385Hz", "500Hz" };
static const char *wm8976_eq3[] = {"650Hz", "850Hz", "1.1kHz", "1.4kHz" };
static const char *wm8976_eq4[] = {"1.8kHz", "2.4kHz", "3.2kHz", "4.1kHz" };
static const char *wm8976_eq5[] = {"5.3kHz", "6.9kHz", "9kHz", "11.7kHz" };
static const char *wm8976_alc[] = {"ALC", "Limiter" };

static const struct soc_enum wm8976_enum[] = {
	SOC_ENUM_SINGLE(WM8976_COMP, 1, 4, wm8976_companding),	/* adc */
	SOC_ENUM_SINGLE(WM8976_COMP, 3, 4, wm8976_companding),	/* dac */

	SOC_ENUM_SINGLE(WM8976_EQ1,  8, 2, wm8976_eqmode),
	SOC_ENUM_SINGLE(WM8976_EQ1,  5, 4, wm8976_eq1),

	SOC_ENUM_SINGLE(WM8976_EQ2,  8, 2, wm8976_bw),
	SOC_ENUM_SINGLE(WM8976_EQ2,  5, 4, wm8976_eq2),

	SOC_ENUM_SINGLE(WM8976_EQ3,  8, 2, wm8976_bw),
	SOC_ENUM_SINGLE(WM8976_EQ3,  5, 4, wm8976_eq3),

	SOC_ENUM_SINGLE(WM8976_EQ4,  8, 2, wm8976_bw),
	SOC_ENUM_SINGLE(WM8976_EQ4,  5, 4, wm8976_eq4),

	SOC_ENUM_SINGLE(WM8976_EQ5,  8, 2, wm8976_bw),
	SOC_ENUM_SINGLE(WM8976_EQ5,  5, 4, wm8976_eq5),
	
	SOC_ENUM_SINGLE(WM8976_ALC3,  8, 2, wm8976_alc),
};

static const struct snd_kcontrol_new wm8976_snd_controls[] = 
{

	SOC_SINGLE("Digital Loopback Switch", WM8976_COMP, 0, 1, 0),

	SOC_ENUM("DAC Companding", wm8976_enum[1]),
	SOC_ENUM("ADC Companding", wm8976_enum[0]),

	SOC_SINGLE("High Pass Filter Switch", WM8976_ADC, 8, 1, 0),	
	SOC_SINGLE("High Pass Cut Off", WM8976_ADC, 4, 7, 0),	
	SOC_DOUBLE("ADC Inversion Switch", WM8976_ADC, 0, 1, 1, 0),
	SOC_SINGLE("Capture Volume", WM8976_ADCVOL,  0, 255, 0),
	SOC_SINGLE("Capture Boost(+20dB)", WM8976_ADCBOOST, 8, 1, 0),
	SOC_SINGLE("Capture PGA ZC Switch", WM8976_INPPGA,  7, 1, 0),
	SOC_SINGLE("Capture PGA Volume", WM8976_INPPGA,  0, 63, 0),

        SOC_SINGLE("ALC Enable Switch", WM8976_ALC1,  8, 1, 0),
        SOC_SINGLE("ALC Capture Max Gain", WM8976_ALC1,  3, 7, 0),
        SOC_SINGLE("ALC Capture Min Gain", WM8976_ALC1,  0, 7, 0),
        SOC_SINGLE("ALC Capture ZC Switch", WM8976_ALC2,  8, 1, 0),
        SOC_SINGLE("ALC Capture Hold", WM8976_ALC2,  4, 7, 0),
        SOC_SINGLE("ALC Capture Target", WM8976_ALC2,  0, 15, 0),
        SOC_ENUM("ALC Capture Mode", wm8976_enum[12]),
        SOC_SINGLE("ALC Capture Decay", WM8976_ALC3,  4, 15, 0),
        SOC_SINGLE("ALC Capture Attack", WM8976_ALC3,  0, 15, 0),
        SOC_SINGLE("ALC Capture Noise Gate Switch", WM8976_NGATE,  3, 1, 0),
        SOC_SINGLE("ALC Capture Noise Gate Threshold", WM8976_NGATE,  0, 7, 0),

	SOC_ENUM("Eq-3D Mode Switch", wm8976_enum[2]),	
	SOC_ENUM("Eq1 Cut-Off Frequency", wm8976_enum[3]),	
	SOC_SINGLE("Eq1 Volume", WM8976_EQ1,  0, 31, 1),	
	
	SOC_ENUM("Eq2 BandWidth Switch", wm8976_enum[4]),	
	SOC_ENUM("Eq2 Centre Frequency", wm8976_enum[5]),	
	SOC_SINGLE("Eq2 Volume", WM8976_EQ2,  0, 31, 1),

	SOC_ENUM("Eq3 BandWidth Switch", wm8976_enum[6]),	
	SOC_ENUM("Eq3 Centre Frequency", wm8976_enum[7]),	
	SOC_SINGLE("Eq3 Volume", WM8976_EQ3,  0, 31, 1),

	SOC_ENUM("Eq4 BandWidth Switch", wm8976_enum[8]),	
	SOC_ENUM("Eq4 Centre Frequency", wm8976_enum[9]),	
	SOC_SINGLE("Eq4 Volume", WM8976_EQ4,  0, 31, 1),
	
	SOC_ENUM("Eq5 BandWidth Switch", wm8976_enum[10]),	
	SOC_ENUM("Eq5 Centre Frequency", wm8976_enum[11]),	
	SOC_SINGLE("Eq5 Volume", WM8976_EQ5,  0, 31, 1),
	SOC_DOUBLE_R("PCM Playback Volume", WM8976_DACVOLL, WM8976_DACVOLR, 0, 127, 0),

	SOC_DOUBLE_R("Headphone Playback Switch", WM8976_HPVOLL,  WM8976_HPVOLR, 6, 1, 1),
	SOC_DOUBLE_R("Headphone Playback Volume", WM8976_HPVOLL,  WM8976_HPVOLR, 0, 50, 0),

	SOC_DOUBLE_R("Speaker Playback Switch", WM8976_SPKVOLL,  WM8976_SPKVOLR, 6, 1, 1),
	SOC_DOUBLE_R("Speaker Playback Volume", WM8976_SPKVOLL,  WM8976_SPKVOLR, 0, 58, 0),

};

/* add non dapm controls */
static int wm8976_add_controls(struct snd_soc_codec *codec)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(wm8976_snd_controls); i++) {
		err = snd_ctl_add(codec->card, snd_soc_cnew(&wm8976_snd_controls[i],codec, NULL));
		if (err < 0)
			return err;
	}

	return 0;
}

/* Left Output Mixer */
static const struct snd_kcontrol_new wm8976_left_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left Playback Switch", WM8976_OUTPUT, 6, 1, 0),
	SOC_DAPM_SINGLE("Right Playback Switch", WM8976_MIXL, 0, 1, 0),
	SOC_DAPM_SINGLE("Bypass Playback Switch", WM8976_MIXL, 1, 1, 0),
	SOC_DAPM_SINGLE("Left Aux Switch", WM8976_MIXL, 5, 1, 0),
};

/* Right Output Mixer */
static const struct snd_kcontrol_new wm8976_right_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left Playback Switch", WM8976_OUTPUT, 5, 1, 0),
	SOC_DAPM_SINGLE("Right Playback Switch", WM8976_MIXR, 0, 1, 0),
	SOC_DAPM_SINGLE("Right Aux Switch", WM8976_MIXR, 5, 1, 0),
};

/* Out4 Mixer */
static const struct snd_kcontrol_new wm8976_out4_mixer_controls[] = {
	SOC_DAPM_SINGLE("VMID", WM8976_MONOMIX, 6, 1, 0),
	SOC_DAPM_SINGLE("Out4 LeftMixer Switch", WM8976_MONOMIX, 4, 1, 0),
	SOC_DAPM_SINGLE("Out4 LeftDac Switch", WM8976_MONOMIX, 3, 1, 0),
	SOC_DAPM_SINGLE("Out4 RightMixer Switch", WM8976_MONOMIX, 1, 1, 0),	
	SOC_DAPM_SINGLE("Out4 RightDac Switch", WM8976_MONOMIX, 0, 1, 0),
};

/* Out3 Mixer */
static const struct snd_kcontrol_new wm8976_out3_mixer_controls[] = {
	SOC_DAPM_SINGLE("VMID", WM8976_OUT3MIX, 6, 1, 0),
	SOC_DAPM_SINGLE("Out3 Out4Mixer Switch", WM8976_OUT3MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("Out3 BypassADC Switch", WM8976_OUT3MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("Out3 LeftMixer Switch", WM8976_OUT3MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("Out3 LeftDac Switch", WM8976_OUT3MIX, 0, 1, 0),
};

static const struct snd_kcontrol_new wm8976_boost_controls[] = {
	SOC_DAPM_SINGLE("Mic PGA Switch", WM8976_INPPGA,  6, 1, 1),
	SOC_DAPM_SINGLE("AuxL Volume", WM8976_ADCBOOST, 0, 7, 0),
	SOC_DAPM_SINGLE("L2 Volume", WM8976_ADCBOOST, 4, 7, 0),
};

static const struct snd_kcontrol_new wm8976_micpga_controls[] = {
	SOC_DAPM_SINGLE("MICP Switch", WM8976_INPUT, 0, 1, 0),
	SOC_DAPM_SINGLE("MICN Switch", WM8976_INPUT, 1, 1, 0),
	SOC_DAPM_SINGLE("L2 Switch", WM8976_INPUT, 2, 1, 0),
};

static const struct snd_soc_dapm_widget wm8976_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("MICN"),
	SND_SOC_DAPM_INPUT("MICP"),
	SND_SOC_DAPM_INPUT("AUXL"),
	SND_SOC_DAPM_INPUT("AUXR"),
	SND_SOC_DAPM_INPUT("L2"),
	
	SND_SOC_DAPM_MICBIAS("Mic Bias", WM8976_POWER1, 4, 0),

	
	SND_SOC_DAPM_MIXER("Left Mixer", WM8976_POWER3, 2, 0,
	&wm8976_left_mixer_controls[0], ARRAY_SIZE(wm8976_left_mixer_controls)),
	SND_SOC_DAPM_PGA("Left Out 1", WM8976_POWER2, 7, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left Out 2", WM8976_POWER3, 6, 0, NULL, 0),
	SND_SOC_DAPM_DAC("Left DAC", "Left HiFi Playback", WM8976_POWER3, 0, 0),

	SND_SOC_DAPM_MIXER("Right Mixer", WM8976_POWER3, 3, 0,
	&wm8976_right_mixer_controls[0], ARRAY_SIZE(wm8976_right_mixer_controls)),
	SND_SOC_DAPM_PGA("Right Out 1", WM8976_POWER2, 8, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Out 2", WM8976_POWER3, 5, 0, NULL, 0),
	SND_SOC_DAPM_DAC("Right DAC", "Right HiFi Playback", WM8976_POWER3, 1, 0),

	SND_SOC_DAPM_ADC("ADC", "HiFi Capture", WM8976_POWER2, 0, 0),

//	SND_SOC_DAPM_PGA("Mic PGA", WM8976_POWER2, 2, 0,
//	&wm8976_micpga_controls[0],ARRAY_SIZE(wm8976_micpga_controls)),	
	SND_SOC_DAPM_MIXER("Mic PGA", WM8976_POWER2, 2, 0,
	&wm8976_micpga_controls[0],ARRAY_SIZE(wm8976_micpga_controls)),	

	SND_SOC_DAPM_MIXER("Boost Mixer", WM8976_POWER2, 4, 0,
	&wm8976_boost_controls[0], ARRAY_SIZE(wm8976_boost_controls)),
	
	SND_SOC_DAPM_OUTPUT("LOUT1"),
	SND_SOC_DAPM_OUTPUT("ROUT1"),
	SND_SOC_DAPM_OUTPUT("LOUT2"),
	SND_SOC_DAPM_OUTPUT("ROUT2"),
	
	SND_SOC_DAPM_MIXER("Out3 Mixer", WM8976_POWER1, 6, 0,
	&wm8976_out3_mixer_controls[0], ARRAY_SIZE(wm8976_out3_mixer_controls)),	
	SND_SOC_DAPM_PGA("Out 3", WM8976_POWER1, 7, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("OUT3"),
	
	SND_SOC_DAPM_MIXER("Out4 Mixer", WM8976_POWER1, 7, 0,
	&wm8976_out4_mixer_controls[0], ARRAY_SIZE(wm8976_out4_mixer_controls)),
	SND_SOC_DAPM_PGA("Out 4", WM8976_POWER3, 8, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("OUT4"),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* left mixer */
	{"Left Mixer", "Left Playback Switch", "Left DAC"},
	{"Left Mixer", "Right Playback Switch", "Right DAC"},
	{"Left Mixer", "Bypass Playback Switch", "Boost Mixer"},
	{"Left Mixer", "Left Aux Switch", "AUXL"},

	/* right mixer */
	{"Right Mixer", "Right Playback Switch", "Right DAC"},
	{"Right Mixer", "Left Playback Switch", "Left DAC"},
	{"Right Mixer", "Right Aux Switch", "AUXR"},	
	/* left out */
	{"Left Out 1", NULL, "Left Mixer"},
	{"Left Out 2", NULL, "Left Mixer"},
	{"LOUT1", NULL, "Left Out 1"},
	{"LOUT2", NULL, "Left Out 2"},
	/* right out */
	{"Right Out 1", NULL, "Right Mixer"},
	{"Right Out 2", NULL, "Right Mixer"},
	{"ROUT1", NULL, "Right Out 1"},
	{"ROUT2", NULL, "Right Out 2"},

	/* Microphone PGA */
	{"Mic PGA", "MICN Switch", "MICN"},
	{"Mic PGA", "MICP Switch", "MICP"},
	{"Mic PGA", "L2 Switch", "L2" },
	
	/* Boost Mixer */
	{"Boost Mixer", "Mic PGA Switch", "Mic PGA"},
	{"Boost Mixer", "AuxL Volume", "AUXL"},
	{"Boost Mixer", "L2 Volume", "L2"},
	
	{"ADC", NULL, "Boost Mixer"},

	/* out 3 */
	{"Out3 Mixer", "VMID", "Out4 Mixer"},
	{"Out3 Mixer", "Out3 Out4Mixer Switch", "Out4 Mixer"},
	{"Out3 Mixer", "Out3 BypassADC Switch", "ADC"},
	{"Out3 Mixer", "Out3 LeftMixer Switch", "Left Mixer"},
	{"Out3 Mixer", "Out3 LeftDac Switch", "Left DAC"},
	{"Out 3", NULL, "Out3 Mixer"},
	{"OUT3", NULL, "Out 3"},
	
	/* out 4 */
	{"Out4 Mixer", "VMID", "Out3 Mixer"},
	{"Out4 Mixer", "Out4 LeftMixer Switch", "Left Mixer"},
	{"Out4 Mixer", "Out4 LeftDac Switch", "Left DAC"},
	{"Out4 Mixer", "Out4 RightMixer Switch", "Right Mixer"},	
	{"Out4 Mixer", "Out4 RightDac Switch", "Right DAC"},
	{"Out 4", NULL, "Out4 Mixer"},
	{"OUT4", NULL, "Out 4"},
};

static int wm8976_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, wm8976_dapm_widgets,
				  ARRAY_SIZE(wm8976_dapm_widgets));

	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	snd_soc_dapm_new_widgets(codec);
	return 0;
}

struct _pll_div {
	unsigned int pre:4; /* prescale - 1 */
	unsigned int n:4;
	unsigned int k;
};

static struct _pll_div pll_div;

/* The size in bits of the pll divide multiplied by 10
 * to allow rounding later */
#define FIXED_PLL_SIZE ((1 << 24) * 10)

static void pll_factors(unsigned int target, unsigned int source)
{
	unsigned long long Kpart;
	unsigned int K, Ndiv, Nmod;

	Ndiv = target / source;
	if (Ndiv < 6) {
		source >>= 1;
		pll_div.pre = 1;
		Ndiv = target / source;
	} else
		pll_div.pre = 0;

	if ((Ndiv < 6) || (Ndiv > 12))
		printk(KERN_WARNING"WM8976 N value outwith recommended range! N = %d\n",Ndiv);

	pll_div.n = Ndiv;
	Nmod = target % source;
	Kpart = FIXED_PLL_SIZE * (long long)Nmod;

	do_div(Kpart, source);

	K = Kpart & 0xFFFFFFFF;

	/* Check if we need to round */
	if ((K % 10) >= 5)
		K += 5;

	/* Move down to proper range now rounding is done */
	K /= 10;

	pll_div.k = K;
}

static int wm8976_set_dai_pll(struct snd_soc_dai *codec_dai,
		int pll_id, unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg;

	if(freq_in == 0 || freq_out == 0) {
		reg = wm8976_read_reg_cache(codec, WM8976_POWER1);
		wm8976_write(codec, WM8976_POWER1, reg & 0x1df);
		return 0;
	}

	pll_factors(freq_out * 8, freq_in);

	wm8976_write(codec, WM8976_PLLN, (pll_div.pre << 4) | pll_div.n);
	wm8976_write(codec, WM8976_PLLK1, pll_div.k >> 18);
	wm8976_write(codec, WM8976_PLLK1, (pll_div.k >> 9) && 0x1ff);
	wm8976_write(codec, WM8976_PLLK1, pll_div.k && 0x1ff);
	reg = wm8976_read_reg_cache(codec, WM8976_POWER1);
	wm8976_write(codec, WM8976_POWER1, reg | 0x020);
	
	
	return 0;
}

static int wm8976_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int wm8976_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = wm8976_read_reg_cache(codec, WM8976_IFACE) & 0x7;
	u16 clk = wm8976_read_reg_cache(codec, WM8976_CLOCK) & 0xfffe;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) 
	{
	case SND_SOC_DAIFMT_CBM_CFM:
		clk |= 0x0001;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK)
	{
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x0010;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface |= 0x0000;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0008;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0018;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x0180;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0100;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0080;
		break;
	default:
		return -EINVAL;
	}

	wm8976_write(codec, WM8976_IFACE, iface);
	wm8976_write(codec, WM8976_CLOCK, clk);

	return 0;
}

static int wm8976_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->codec;
	u16 iface = wm8976_read_reg_cache(codec, WM8976_IFACE) & 0xff9f;
	u16 adn = wm8976_read_reg_cache(codec, WM8976_ADD) & 0x1f1;

	/* bit size */
	switch (params_format(params))
	{
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x0020;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= 0x0040;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface |= 0x0060;
		break;
	}

	/* filter coefficient */
	switch (params_rate(params))
	{
	//case SNDRV_PCM_RATE_8000:
	case 8000:
		adn |= 0x5 << 1;
		break;
	//case SNDRV_PCM_RATE_11025:
	case 11025:
		adn |= 0x4 << 1;
		break;
	//case SNDRV_PCM_RATE_16000:
	case 16000:
		adn |= 0x3 << 1;
		break;
	//case SNDRV_PCM_RATE_22050:
	case 22050:
		adn |= 0x2 << 1;
		break;
	//case SNDRV_PCM_RATE_32000:
	case 32000:
		adn |= 0x1 << 1;
		break;
	case 44100:
	case 48000:
		//adn |= 0x0 << 1;
		break;
	}

	/* set iface */
	wm8976_write(codec, WM8976_IFACE, iface);
	wm8976_write(codec, WM8976_ADD, adn);
	return 0;
}

static int wm8976_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
		int div_id, int div)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg;
	
	switch (div_id) {
	case WM8976_MCLKDIV:
		reg = wm8976_read_reg_cache(codec, WM8976_CLOCK) & 0x11f;
		wm8976_write(codec, WM8976_CLOCK, reg | div);
		break;
	case WM8976_BCLKDIV:
		reg = wm8976_read_reg_cache(codec, WM8976_CLOCK) & 0x1c7;
		wm8976_write(codec, WM8976_CLOCK, reg | div);
		break;
	case WM8976_OPCLKDIV:
		reg = wm8976_read_reg_cache(codec, WM8976_GPIO) & 0x1cf;
		wm8976_write(codec, WM8976_GPIO, reg | div);
		break;
	case WM8976_DACOSR:
		reg = wm8976_read_reg_cache(codec, WM8976_DAC) & 0x1f7;
		wm8976_write(codec, WM8976_DAC, reg | div);
		break;
	case WM8976_ADCOSR:
		reg = wm8976_read_reg_cache(codec, WM8976_ADC) & 0x1f7;
		wm8976_write(codec, WM8976_ADC, reg | div);
		break;
	case WM8976_MCLKSEL:
		reg = wm8976_read_reg_cache(codec, WM8976_CLOCK) & 0x0ff;
		wm8976_write(codec, WM8976_CLOCK, reg | div);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int wm8976_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = wm8976_read_reg_cache(codec, WM8976_DAC) & 0xffbf;

	if(mute)
		wm8976_write(codec, WM8976_DAC, mute_reg | 0x40);
	else
		wm8976_write(codec, WM8976_DAC, mute_reg);

	return 0;
}

void wm8976_set_mute(int mute)
{
    	u16 mute_reg = wm8976_read_reg_cache(g_codec, WM8976_DAC) & 0xffbf;
    	u16 mute_reg1 = wm8976_read_reg_cache(g_codec, WM8976_HPVOLL) & 0xffbf;
    	u16 mute_reg2 = wm8976_read_reg_cache(g_codec, WM8976_HPVOLR) & 0xffbf;
    	u16 mute_reg3 = wm8976_read_reg_cache(g_codec, WM8976_SPKVOLL) & 0xffbf;
    	u16 mute_reg4 = wm8976_read_reg_cache(g_codec, WM8976_SPKVOLR) & 0xffbf;
	if(mute)
        {
            wm8976_write(g_codec, WM8976_DAC, mute_reg | 0x40);
            wm8976_write(g_codec, WM8976_HPVOLL, mute_reg1 | 0x40);
            wm8976_write(g_codec, WM8976_HPVOLR, mute_reg2 | 0x40);
            wm8976_write(g_codec, WM8976_SPKVOLL, mute_reg3 | 0x40);
            wm8976_write(g_codec, WM8976_SPKVOLR, mute_reg4 | 0x40);
        }
	else
        {
            wm8976_write(g_codec, WM8976_DAC, mute_reg);
            wm8976_write(g_codec, WM8976_HPVOLL, mute_reg1);
            wm8976_write(g_codec, WM8976_HPVOLR, mute_reg2);
            wm8976_write(g_codec, WM8976_SPKVOLL, mute_reg3);
            wm8976_write(g_codec, WM8976_SPKVOLR, mute_reg4);
        }

}
EXPORT_SYMBOL_GPL(wm8976_set_mute);



/* TODO: liam need to make this lower power with dapm */
static int wm8976_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	u16 pwr_reg = wm8976_read_reg_cache(codec,WM8976_POWER1) & 0x0fc;

	wm8976_write(codec, WM8976_INPUT, 0x03);
	switch (level) {
	case SND_SOC_BIAS_ON:
		/*set vmid to 75k for*/
		wm8976_write(codec, WM8976_POWER1, pwr_reg|0x01D);
		break;
	case SND_SOC_BIAS_STANDBY:
                /* set vmid to 5k for quick power up */
	     	wm8976_write(codec, WM8976_POWER1, pwr_reg|0x001F);
		break;
	case SND_SOC_BIAS_PREPARE:
		/* set vmid to 300k */
		wm8976_write(codec, WM8976_POWER1, pwr_reg|0x001E);
		break;
	case SND_SOC_BIAS_OFF:
		//wm8976_write(codec, WM8976_POWER1, 0x0);
		//wm8976_write(codec, WM8976_POWER2, 0x0);
		//wm8976_write(codec, WM8976_POWER3, 0x0);
		break;
	}
	codec->bias_level = level;
	return 0;
}

#define WM8976_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000)

#define WM8976_FORMATS \
	(SNDRV_PCM_FORMAT_S16_LE | SNDRV_PCM_FORMAT_S20_3LE | \
	SNDRV_PCM_FORMAT_S24_3LE | SNDRV_PCM_FORMAT_S24_LE)

struct snd_soc_dai wm8976_dai = {
	.name = "WM8976 HiFi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8976_RATES,
		.formats = WM8976_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8976_RATES,
		.formats = WM8976_FORMATS,},
	.ops = {
		.hw_params = wm8976_hw_params,

		.set_fmt = wm8976_set_dai_fmt,
		.set_clkdiv = wm8976_set_dai_clkdiv,
		.set_pll = wm8976_set_dai_pll,
		},
/*
	.dai_ops = {
		.digital_mute = wm8976_mute,
		.set_fmt = wm8976_set_dai_fmt,
		.set_clkdiv = wm8976_set_dai_clkdiv,
		.set_pll = wm8976_set_dai_pll,
		.set_sysclk = wm8976_set_dai_sysclk,
 	},
*/
};
EXPORT_SYMBOL_GPL(wm8976_dai);

static int wm8976_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	wm8976_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int wm8976_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;
	int i;
	u8 data[2];
	u16 *cache = codec->reg_cache;

	/* Sync reg_cache with the hardware */
	for (i = 0; i < ARRAY_SIZE(wm8976_reg); i++)
	{
		data[0] = (i << 1) | ((cache[i] >> 8) & 0x0001);
		data[1] = cache[i] & 0x00ff;
		codec->hw_write(codec->control_data, data, 2);
	}

	wm8976_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	wm8976_set_bias_level(codec, codec->suspend_bias_level);
	return 0;
}

/*
 * initialise the WM8976 driver
 * register the mixer and dsp interfaces with the kernel
 */
static int wm8976_init(struct snd_soc_device* socdev)
{
	struct snd_soc_codec *codec = socdev->codec;
	int ret = 0;
	u16 reg;
	printk("wm8976_init\n");
	codec->name = "WM8976";
	codec->owner = THIS_MODULE;
	codec->read = wm8976_read_reg_cache;
	codec->write = wm8976_write;
	codec->set_bias_level = wm8976_set_bias_level;
	codec->dai = &wm8976_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = ARRAY_SIZE(wm8976_reg);
	codec->reg_cache = kmemdup(wm8976_reg, sizeof(wm8976_reg), GFP_KERNEL);

	if (codec->reg_cache == NULL)
		return -ENOMEM;

	wm8976_reset(codec);

	wm8976_set_bias_level(codec, SND_SOC_BIAS_PREPARE);
	codec->bias_level = SND_SOC_BIAS_STANDBY;
	msleep(msecs_to_jiffies(3000));

        wm8976_write(codec, WM8976_IFACE, 0x010);
	wm8976_write(codec, WM8976_CLOCK, 0x000);
	wm8976_write(codec, WM8976_BEEP, 0x010);

        /* set the update bits */
        reg = wm8976_read_reg_cache(codec, WM8976_DACVOLL);
        wm8976_write(codec, WM8976_DACVOLL, reg | 0x0100);
        reg = wm8976_read_reg_cache(codec, WM8976_DACVOLR);
        wm8976_write(codec, WM8976_DACVOLR, reg | 0x0100);
        reg = wm8976_read_reg_cache(codec, WM8976_ADCVOL);
        wm8976_write(codec, WM8976_ADCVOL, reg | 0x01ff);
        /*Reserved a reg*/
        reg = wm8976_read_reg_cache(codec, WM8976_INPPGA);
        wm8976_write(codec, WM8976_INPPGA, reg | 0x01BF);  // 63/63 % PGA  
        reg = wm8976_read_reg_cache(codec, WM8976_HPVOLL);
        wm8976_write(codec, WM8976_HPVOLL, reg | 0x0100);
        reg = wm8976_read_reg_cache(codec, WM8976_HPVOLR);
        wm8976_write(codec, WM8976_HPVOLR, reg | 0x0100);
        reg = wm8976_read_reg_cache(codec, WM8976_SPKVOLL);
        wm8976_write(codec, WM8976_SPKVOLL, reg | 0x0100);
        reg = wm8976_read_reg_cache(codec, WM8976_SPKVOLR);
        wm8976_write(codec, WM8976_SPKVOLR, reg | 0x0100);

#if 0
	wm8976_write(codec, WM8976_GPIO, 0x008);     /* R8*/	
	wm8976_write(codec, WM8976_ADD, 0x001);      /* slow clock enabled */	
	wm8976_write(codec, WM8976_JACK1, 0x050);    /* selected GPIO2 as jack detection input and Enable*/	
	wm8976_write(codec, WM8976_JACK2, 0x021);    /* OUT2_EN_0 and OUT2_EN_1 */
#endif


	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if(ret < 0) {
		printk(KERN_ERR "wm8976: failed to create pcms\n");
		goto pcm_err;
	}

        g_codec=socdev->codec;;
	/* power on device */
	//wm8976_set_bias_level(codec, SND_SOC_BIAS_OFF);
	
	wm8976_write(codec, WM8976_INPUT, 0x03);

	wm8976_add_controls(codec);
	wm8976_add_widgets(codec);

	//ret = snd_soc_register_card(socdev);
	ret = snd_soc_init_card(socdev);
	if (ret < 0) {
	      	printk(KERN_ERR "wm8976: failed to register card\n");
		goto card_err;
    	}
	return ret;

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
pcm_err:
	kfree(codec->reg_cache);
	return ret;
}

static struct snd_soc_device *wm8976_socdev;

#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)

static int wm8976_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct snd_soc_device *socdev = wm8976_socdev;
	struct snd_soc_codec *codec = socdev->codec;
	int ret;

	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = wm8976_init(socdev);
	if (ret < 0)
		dev_err(&i2c->dev, "failed to initialise WM8976\n");
	return ret;
}

static int wm8976_i2c_remove(struct i2c_client *client)
{
	struct snd_soc_codec *codec = i2c_get_clientdata(client);
	kfree(codec->reg_cache);
	return 0;
}

static const struct i2c_device_id wm8976_i2c_id[] = {
	{ "wm8976", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8976_i2c_id);

static struct i2c_driver wm8976_i2c_driver = {
	.driver = {
		.name = "WM8976 I2C Codec",
		.owner = THIS_MODULE,
	},
	.probe    = wm8976_i2c_probe,
	.remove   = wm8976_i2c_remove,
	.id_table = wm8976_i2c_id,
};

static int wm8976_add_i2c_device(struct platform_device *pdev,
				 const struct wm8976_setup_data *setup)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int ret;

	ret = i2c_add_driver(&wm8976_i2c_driver);
	if (ret != 0) {
		dev_err(&pdev->dev, "can't add i2c driver\n");
		return ret;
	}

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = setup->i2c_address;
	strlcpy(info.type, "wm8976", I2C_NAME_SIZE);
	
	adapter = i2c_get_adapter(setup->i2c_bus);
	if (!adapter) {
		dev_err(&pdev->dev, "can't get i2c adapter %d\n",setup->i2c_bus);
		goto err_driver;
	}

	client = i2c_new_device(adapter, &info);
	i2c_put_adapter(adapter);
	if (!client) {
		dev_err(&pdev->dev, "t can't add i2c device at 0x%x\n",	(unsigned int)info.addr);
		goto err_driver;
	}

	return 0;

err_driver:
	i2c_del_driver(&wm8976_i2c_driver);
	return -ENODEV;
}

#endif

static int wm8976_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct wm8976_setup_data *setup;
	struct snd_soc_codec *codec;
	int ret = 0;

	pr_info("WM8976 Audio Codec %s", WM8976_VERSION);

	setup = socdev->codec_data;
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;

	socdev->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	wm8976_socdev = socdev;
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
	if (setup->i2c_address) 
	{
		codec->hw_write = (hw_write_t)i2c_master_send;
		ret = wm8976_add_i2c_device(pdev, setup);
		if (ret != 0)
			printk(KERN_ERR "can't add i2c driver");
	}
#else
	/* Add other interfaces here */
#endif
	return ret;
}

/* power down chip */
static int wm8976_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	if (codec->control_data)
		wm8976_set_bias_level(codec, SND_SOC_BIAS_OFF);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
	i2c_unregister_device(codec->control_data);
	i2c_del_driver(&wm8976_i2c_driver);
#endif
	kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_wm8976 = {
	.probe = 	wm8976_probe,
	.remove = 	wm8976_remove,
	.suspend = 	wm8976_suspend,
	.resume =	wm8976_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_wm8976);


static int __init wm8976_modinit(void)
{
	return snd_soc_register_dai(&wm8976_dai);
}
module_init(wm8976_modinit);

static void __exit wm8976_exit(void)
{
	snd_soc_unregister_dai(&wm8976_dai);
}
module_exit(wm8976_exit);

MODULE_DESCRIPTION("ASoC WM8976 driver");
MODULE_AUTHOR("Jiang jianjun");
MODULE_LICENSE("GPL");
