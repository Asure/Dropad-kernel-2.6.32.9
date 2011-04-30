/* sound/soc/s3c24xx/s3c-ac97.c
 *
 * ALSA SoC Audio Layer - S3C AC97 Controller driver
 * 	Evolved from s3c2443-ac97.c
 *
 * Copyright (c) 2010 Samsung Electronics Co. Ltd
 * 	Author: Jaswinder Singh <jassi.brar@samsung.com>
 * 	Credits: Graeme Gregory, Sean Choi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <sound/soc.h>

#include <plat/regs-ac97.h>
#include <plat/audio.h>
#include <mach/dma.h>
#include <mach/pd.h>

#include "s3c-dma.h"
#include "s3c-ac97.h"

#define AC_CMD_ADDR(x) (x << 16)
#define AC_CMD_DATA(x) (x & 0xffff)

struct s3c_ac97_info {
	unsigned           state;
	struct clk         *ac97_clk;
	void __iomem	   *regs;
	struct mutex       lock;
	struct completion  done;
};
static struct s3c_ac97_info s3c_ac97;

static struct s3c2410_dma_client s3c_dma_client_out = {
	.name = "AC97 PCMOut"
};

static struct s3c2410_dma_client s3c_dma_client_in = {
	.name = "AC97 PCMIn"
};

static struct s3c2410_dma_client s3c_dma_client_micin = {
	.name = "AC97 MicIn"
};

static struct s3c_dma_params s3c_ac97_pcm_out = {
	.client		= &s3c_dma_client_out,
	.dma_size	= 4,
};

static struct s3c_dma_params s3c_ac97_pcm_in = {
	.client		= &s3c_dma_client_in,
	.dma_size	= 4,
};

static struct s3c_dma_params s3c_ac97_mic_in = {
	.client		= &s3c_dma_client_micin,
	.dma_size	= 4,
};

/* For AC97 Clock/Power Gating */
static char *ac97_pd_name = "ac97_pd";
static int tx_clk_enabled = 0;
static int rx_clk_enabled = 0;
static int audio_clk_gated = 0; /* At first, clock & pcm_pd is enabled in probe() */
static int suspended_by_pm = 0;

void s3c_ac97_set_clk_enabled(struct snd_soc_dai *dai, bool state)
{
	pr_debug("..entering %s \n", __func__);

	if (s3c_ac97.ac97_clk == NULL) return;

	if (state) {
		if (!audio_clk_gated) {
			pr_debug("already audio clock is enabled! \n");
			return;
		}

		s5pv210_pd_enable(ac97_pd_name);

		clk_enable(s3c_ac97.ac97_clk);
		audio_clk_gated = 0;
	}
	else {
		if (audio_clk_gated) {
			pr_debug("already audio clock is gated! \n");
			return;
		}
		clk_disable(s3c_ac97.ac97_clk);

		s5pv210_pd_disable(ac97_pd_name);
		audio_clk_gated = 1;
	}
}

static void s3c_ac97_do_suspend(struct snd_soc_dai *dai)
{
	if (!audio_clk_gated) {		/* Clk/Pwr is alive? */
		/* No register backup required */

		s3c_ac97_set_clk_enabled(dai, 0);	/* Gating Clk/Pwr */

		pr_debug("Registers stored and suspend.\n");
	}

	return;
}

static void s3c_ac97_do_resume(struct snd_soc_dai *dai)
{
	if (audio_clk_gated) {		/* Clk/Pwr is gated? */
		s3c_ac97_set_clk_enabled(dai, 1);	/* Enable Clk/Pwr */

		/* No register restore required */

		pr_debug("Resume and registers restored.\n");
	}
}

static void s3c_ac97_activate(struct snd_ac97 *ac97)
{
	u32 ac_glbctrl, stat;

	s3c_ac97_do_resume(0);

	stat = readl(s3c_ac97.regs + S3C_AC97_GLBSTAT) & 0x7;
	if (stat == S3C_AC97_GLBSTAT_MAINSTATE_ACTIVE)
		return; /* Return if already active */

	INIT_COMPLETION(s3c_ac97.done);

	ac_glbctrl = readl(s3c_ac97.regs + S3C_AC97_GLBCTRL);
	ac_glbctrl = S3C_AC97_GLBCTRL_ACLINKON;
	writel(ac_glbctrl, s3c_ac97.regs + S3C_AC97_GLBCTRL);
	msleep(1);

	ac_glbctrl |= S3C_AC97_GLBCTRL_TRANSFERDATAENABLE;
	writel(ac_glbctrl, s3c_ac97.regs + S3C_AC97_GLBCTRL);
	msleep(1);

	ac_glbctrl = readl(s3c_ac97.regs + S3C_AC97_GLBCTRL);
	ac_glbctrl |= S3C_AC97_GLBCTRL_CODECREADYIE;
	writel(ac_glbctrl, s3c_ac97.regs + S3C_AC97_GLBCTRL);

	if (!wait_for_completion_timeout(&s3c_ac97.done, HZ))
		printk(KERN_ERR "AC97: Unable to activate!");
}

static unsigned short s3c_ac97_read(struct snd_ac97 *ac97,
	unsigned short reg)
{
	u32 ac_glbctrl, ac_codec_cmd;
	u32 stat, addr, data;

	mutex_lock(&s3c_ac97.lock);

	s3c_ac97_activate(ac97);

	INIT_COMPLETION(s3c_ac97.done);

	ac_codec_cmd = readl(s3c_ac97.regs + S3C_AC97_CODEC_CMD);
	ac_codec_cmd = S3C_AC97_CODEC_CMD_READ | AC_CMD_ADDR(reg);
	writel(ac_codec_cmd, s3c_ac97.regs + S3C_AC97_CODEC_CMD);

	udelay(50);

	ac_glbctrl = readl(s3c_ac97.regs + S3C_AC97_GLBCTRL);
	ac_glbctrl |= S3C_AC97_GLBCTRL_CODECREADYIE;
	writel(ac_glbctrl, s3c_ac97.regs + S3C_AC97_GLBCTRL);

	if (!wait_for_completion_timeout(&s3c_ac97.done, HZ))
		printk(KERN_ERR "AC97: Unable to read!");

	stat = readl(s3c_ac97.regs + S3C_AC97_STAT);
	addr = (stat >> 16) & 0x7f;
	data = (stat & 0xffff);

	if (addr != reg)
		printk(KERN_ERR "s3c-ac97: req addr = %02x, rep addr = %02x\n", reg, addr);

	mutex_unlock(&s3c_ac97.lock);

	return (unsigned short)data;
}

static void s3c_ac97_write(struct snd_ac97 *ac97, unsigned short reg,
	unsigned short val)
{
	u32 ac_glbctrl, ac_codec_cmd;

	mutex_lock(&s3c_ac97.lock);

	s3c_ac97_activate(ac97);

	INIT_COMPLETION(s3c_ac97.done);

	ac_codec_cmd = readl(s3c_ac97.regs + S3C_AC97_CODEC_CMD);
	ac_codec_cmd = AC_CMD_ADDR(reg) | AC_CMD_DATA(val);
	writel(ac_codec_cmd, s3c_ac97.regs + S3C_AC97_CODEC_CMD);

	udelay(50);

	ac_glbctrl = readl(s3c_ac97.regs + S3C_AC97_GLBCTRL);
	ac_glbctrl |= S3C_AC97_GLBCTRL_CODECREADYIE;
	writel(ac_glbctrl, s3c_ac97.regs + S3C_AC97_GLBCTRL);

	if (!wait_for_completion_timeout(&s3c_ac97.done, HZ))
		printk(KERN_ERR "AC97: Unable to write!");

	ac_codec_cmd = readl(s3c_ac97.regs + S3C_AC97_CODEC_CMD);
	ac_codec_cmd |= S3C_AC97_CODEC_CMD_READ;
	writel(ac_codec_cmd, s3c_ac97.regs + S3C_AC97_CODEC_CMD);

	mutex_unlock(&s3c_ac97.lock);
}

static void s3c_ac97_cold_reset(struct snd_ac97 *ac97)
{
	writel(S3C_AC97_GLBCTRL_COLDRESET,
			s3c_ac97.regs + S3C_AC97_GLBCTRL);
	msleep(1);

	writel(0, s3c_ac97.regs + S3C_AC97_GLBCTRL);
	msleep(1);
}

static void s3c_ac97_warm_reset(struct snd_ac97 *ac97)
{
	u32 stat;

	stat = readl(s3c_ac97.regs + S3C_AC97_GLBSTAT) & 0x7;
	if (stat == S3C_AC97_GLBSTAT_MAINSTATE_ACTIVE)
		return; /* Return if already active */

	writel(S3C_AC97_GLBCTRL_WARMRESET, s3c_ac97.regs + S3C_AC97_GLBCTRL);
	msleep(1);

	writel(0, s3c_ac97.regs + S3C_AC97_GLBCTRL);
	msleep(1);

	s3c_ac97_activate(ac97);
}

static irqreturn_t s3c_ac97_irq(int irq, void *dev_id)
{
	u32 ac_glbctrl, ac_glbstat;

	ac_glbstat = readl(s3c_ac97.regs + S3C_AC97_GLBSTAT);

	if (ac_glbstat & S3C_AC97_GLBSTAT_CODECREADY) {

		ac_glbctrl = readl(s3c_ac97.regs + S3C_AC97_GLBCTRL);
		ac_glbctrl &= ~S3C_AC97_GLBCTRL_CODECREADYIE;
		writel(ac_glbctrl, s3c_ac97.regs + S3C_AC97_GLBCTRL);

		complete(&s3c_ac97.done);
	}

	ac_glbctrl = readl(s3c_ac97.regs + S3C_AC97_GLBCTRL);
	ac_glbctrl |= (1<<30); /* Clear interrupt */
	writel(ac_glbctrl, s3c_ac97.regs + S3C_AC97_GLBCTRL);

	return IRQ_HANDLED;
}

struct snd_ac97_bus_ops soc_ac97_ops = {
	.read       = s3c_ac97_read,
	.write      = s3c_ac97_write,
	.warm_reset = s3c_ac97_warm_reset,
	.reset      = s3c_ac97_cold_reset,
};
EXPORT_SYMBOL_GPL(soc_ac97_ops);

static int s3c_ac97_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	u32 dma_tsfr_size = 0;

	switch (params_channels(params)) {
		case 1:
			dma_tsfr_size = 2;
			break;
		case 2:
			dma_tsfr_size = 4;
			break;
		case 4:
			break;
		case 6:
			break;
		default:
			break;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		s3c_ac97_pcm_out.dma_size = dma_tsfr_size;
		cpu_dai->dma_data = &s3c_ac97_pcm_out;
	} else {
		s3c_ac97_pcm_in.dma_size = dma_tsfr_size;
		cpu_dai->dma_data = &s3c_ac97_pcm_in;
	}
/*
	100701 AC97 porting
	add external audio codec(WM9713) preparation code (from 2.6.29 s3c_ac97_prepare())
	Headphone mixer & Recording path settings
*/

	s3c_ac97_write(0, 0x26, 0x0000);
	s3c_ac97_write(0, 0x0c, 0x0808);
	s3c_ac97_write(0, 0x3c, 0xf803);
	s3c_ac97_write(0, 0x3e, 0xb990);

	s3c_ac97_write(0, 0x02, 0x8080);
	s3c_ac97_write(0, 0x04, 0x0606);
	s3c_ac97_write(0, 0x1c, 0x00aa);

#ifdef CONFIG_SOUND_WM9713_INPUT_STREAM_MIC	/* Input Stream is MIC-IN */
	s3c_ac97_write(0, 0x5c, 0x0002);
	s3c_ac97_write(0, 0x10, 0x0068);
	s3c_ac97_write(0, 0x14, 0xfe00);
#else						/* Input Stream is LINE-IN : SMDKC110 default */
	s3c_ac97_write(0, 0x14, 0xd612);
#endif
	s3c_ac97_write(0, 0x12, 0x0000);	/* Enable audio ADC, Record Gain to default 0dB */

	return 0;
}

static int s3c_ac97_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	u32 ac_glbctrl;

	ac_glbctrl = readl(s3c_ac97.regs + S3C_AC97_GLBCTRL);
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ac_glbctrl &= ~S3C_AC97_GLBCTRL_PCMINTM_MASK;
	else
		ac_glbctrl &= ~S3C_AC97_GLBCTRL_PCMOUTTM_MASK;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			ac_glbctrl |= S3C_AC97_GLBCTRL_PCMINTM_DMA;
		else
			ac_glbctrl |= S3C_AC97_GLBCTRL_PCMOUTTM_DMA;
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	}

	writel(ac_glbctrl, s3c_ac97.regs + S3C_AC97_GLBCTRL);

	return 0;
}

static int s3c_ac97_hw_mic_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params,
				      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return -ENODEV;
	else
		cpu_dai->dma_data = &s3c_ac97_mic_in;

	return 0;
}

static int s3c_ac97_mic_trigger(struct snd_pcm_substream *substream,
				    int cmd, struct snd_soc_dai *dai)
{
	u32 ac_glbctrl;

	ac_glbctrl = readl(s3c_ac97.regs + S3C_AC97_GLBCTRL);
	ac_glbctrl &= ~S3C_AC97_GLBCTRL_MICINTM_MASK;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ac_glbctrl |= S3C_AC97_GLBCTRL_MICINTM_DMA;
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	}

	writel(ac_glbctrl, s3c_ac97.regs + S3C_AC97_GLBCTRL);

	return 0;
}

static int s3c_ac97_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	s3c_ac97_do_resume(dai);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		tx_clk_enabled = 1;
	}
	else {
		rx_clk_enabled = 1;
	}

	return 0;
}

static void s3c_ac97_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		tx_clk_enabled = 0;
	}
	else {
		rx_clk_enabled = 0;
	}

	if (!tx_clk_enabled && !rx_clk_enabled) {	/* Tx/Rx both off? */
		s3c_ac97_do_suspend(dai);
	}
}

#ifdef CONFIG_PM
static int s3c_ac97_suspend(struct snd_soc_dai *dai)
{
	if (!audio_clk_gated) {		/* Clk/Pwr is alive? */
		suspended_by_pm = 1;
		s3c_ac97_do_suspend(dai);
	}

	return 0;
}

static int s3c_ac97_resume(struct snd_soc_dai *dai)
{
	if (suspended_by_pm) {
		suspended_by_pm = 0;
		s3c_ac97_do_resume(dai);
	}

	return 0;
}

int s3c_ac97_suspend_from_codec(void)
{
	if (!tx_clk_enabled && !rx_clk_enabled) {	/* Tx/Rx both off? */
		s3c_ac97_do_suspend(0);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s3c_ac97_suspend_from_codec);

int s3c_ac97_resume_from_codec(void)
{
	if (audio_clk_gated) {		/* Clk/Pwr is gated? */
		s3c_ac97_do_resume(0);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s3c_ac97_resume_from_codec);

#else
#define s3c_ac97_suspends NULL
#define s3c_ac97_resume  NULL
#endif	/* CONFIG_PM */

static struct snd_soc_dai_ops s3c_ac97_dai_ops = {
	.hw_params	= s3c_ac97_hw_params,
	.trigger	= s3c_ac97_trigger,
	.startup	= s3c_ac97_startup,
	.shutdown	= s3c_ac97_shutdown,
};

static struct snd_soc_dai_ops s3c_ac97_mic_dai_ops = {
	.hw_params	= s3c_ac97_hw_mic_params,
	.trigger	= s3c_ac97_mic_trigger,
	.startup	= s3c_ac97_startup,
	.shutdown	= s3c_ac97_shutdown,
};

struct snd_soc_dai s3c_ac97_dai[] = {
	[S3C_AC97_DAI_PCM] = {
		.name =	"s3c-ac97",
		.id = S3C_AC97_DAI_PCM,
		.ac97_control = 1,
		.playback = {
			.stream_name = "AC97 Playback",
			.channels_min = 1,	/* match Playback channels_min value with ac97_dai(@wm9713.c) */
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
		.capture = {
			.stream_name = "AC97 Capture",
			.channels_min = 1,	/* fix channel_min value to enable Mono recording */
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
		.ops = &s3c_ac97_dai_ops,
		.suspend = s3c_ac97_suspend,
		.resume = s3c_ac97_resume,
	},
	[S3C_AC97_DAI_MIC] = {
		.name = "s3c-ac97-mic",
		.id = S3C_AC97_DAI_MIC,
		.ac97_control = 1,
		.capture = {
			.stream_name = "AC97 Mic Capture",
			.channels_min = 1,
			.channels_max = 1,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
		.ops = &s3c_ac97_mic_dai_ops,
		.suspend = s3c_ac97_suspend,
		.resume = s3c_ac97_resume,
	},
};
EXPORT_SYMBOL_GPL(s3c_ac97_dai);

static __devinit int s3c_ac97_probe(struct platform_device *pdev)
{
	struct resource *mem_res, *dmatx_res, *dmarx_res, *dmamic_res, *irq_res;
	struct s3c_audio_pdata *ac97_pdata;
	int ret;

	ac97_pdata = pdev->dev.platform_data;
	if (!ac97_pdata || !ac97_pdata->cfg_gpio) {
		dev_err(&pdev->dev, "cfg_gpio callback not provided!\n");
		return -EINVAL;
	}

	s5pv210_pd_enable(ac97_pd_name);	/* Enable Power domain */

	/* Check for availability of necessary resource */
	dmatx_res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!dmatx_res) {
		dev_err(&pdev->dev, "Unable to get AC97-TX dma resource\n");
		return -ENXIO;
	}

	dmarx_res = platform_get_resource(pdev, IORESOURCE_DMA, 1);
	if (!dmarx_res) {
		dev_err(&pdev->dev, "Unable to get AC97-RX dma resource\n");
		return -ENXIO;
	}

	dmamic_res = platform_get_resource(pdev, IORESOURCE_DMA, 2);
	if (!dmamic_res) {
		dev_err(&pdev->dev, "Unable to get AC97-MIC dma resource\n");
		return -ENXIO;
	}

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "Unable to get register resource\n");
		return -ENXIO;
	}

	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq_res) {
		dev_err(&pdev->dev, "AC97 IRQ not provided!\n");
		return -ENXIO;
	}

	if (!request_mem_region(mem_res->start,
				resource_size(mem_res), "s3c-ac97")) {
		dev_err(&pdev->dev, "Unable to request register region\n");
		return -EBUSY;
	}

	s3c_ac97_pcm_out.channel = dmatx_res->start;
	s3c_ac97_pcm_out.dma_addr = mem_res->start + S3C_AC97_PCM_DATA;
	s3c_ac97_pcm_in.channel = dmarx_res->start;
	s3c_ac97_pcm_in.dma_addr = mem_res->start + S3C_AC97_PCM_DATA;
	s3c_ac97_mic_in.channel = dmamic_res->start;
	s3c_ac97_mic_in.dma_addr = mem_res->start + S3C_AC97_MIC_DATA;

	init_completion(&s3c_ac97.done);
	mutex_init(&s3c_ac97.lock);

	s3c_ac97.regs = ioremap(mem_res->start, resource_size(mem_res));
	if (s3c_ac97.regs == NULL) {
		dev_err(&pdev->dev, "Unable to ioremap register region\n");
		ret = -ENXIO;
		goto err1;
	}

	s3c_ac97.ac97_clk = clk_get(&pdev->dev, "ac97");
	if (IS_ERR(s3c_ac97.ac97_clk)) {
		dev_err(&pdev->dev, "s3c-ac97 failed to get ac97_clock\n");
		ret = -ENODEV;
		goto err2;
	}
	clk_enable(s3c_ac97.ac97_clk);

	if (ac97_pdata->cfg_gpio(pdev)) {
		dev_err(&pdev->dev, "Unable to configure gpio\n");
		ret = -EINVAL;
		goto err3;
	}

	ret = request_irq(irq_res->start, s3c_ac97_irq,
					IRQF_DISABLED, "AC97", NULL);
	if (ret < 0) {
		printk(KERN_ERR "s3c-ac97: interrupt request failed.\n");
		goto err4;
	}

	s3c_ac97_dai[S3C_AC97_DAI_PCM].dev = &pdev->dev;
	s3c_ac97_dai[S3C_AC97_DAI_MIC].dev = &pdev->dev;

	ret = snd_soc_register_dais(s3c_ac97_dai, ARRAY_SIZE(s3c_ac97_dai));
	if (ret)
		goto err5;

#if 0	/* Leave Power ON-state due to open new pcm
	   (preallocating dma buffer & codec initializing.) */
	pr_debug("%s: Completed... Now disable clock & power...\n", __FUNCTION__);
	s3c_ac97_set_clk_enabled(s3c_ac97_dai[S3C_AC97_DAI_PCM], 0);
#endif
	return 0;

err5:
	free_irq(irq_res->start, NULL);
err4:
err3:
	clk_disable(s3c_ac97.ac97_clk);
	clk_put(s3c_ac97.ac97_clk);
err2:
	iounmap(s3c_ac97.regs);
err1:
	release_mem_region(mem_res->start, resource_size(mem_res));

	return ret;
}

static __devexit int s3c_ac97_remove(struct platform_device *pdev)
{
	struct resource *mem_res, *irq_res;

	snd_soc_unregister_dais(s3c_ac97_dai, ARRAY_SIZE(s3c_ac97_dai));

	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (irq_res)
		free_irq(irq_res->start, NULL);

	clk_disable(s3c_ac97.ac97_clk);
	clk_put(s3c_ac97.ac97_clk);

	iounmap(s3c_ac97.regs);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem_res)
		release_mem_region(mem_res->start, resource_size(mem_res));

	return 0;
}

static struct platform_driver s3c_ac97_driver = {
	.probe  = s3c_ac97_probe,
	.remove = s3c_ac97_remove,
	.driver = {
		.name = "s3c-ac97",
		.owner = THIS_MODULE,
	},
};

static int __init s3c_ac97_init(void)
{
	return platform_driver_register(&s3c_ac97_driver);
}
module_init(s3c_ac97_init);

static void __exit s3c_ac97_exit(void)
{
	platform_driver_unregister(&s3c_ac97_driver);
}
module_exit(s3c_ac97_exit);

MODULE_AUTHOR("Jaswinder Singh, <jassi.brar@samsung.com>");
MODULE_DESCRIPTION("AC97 driver for the Samsung SoC");
MODULE_LICENSE("GPL");
