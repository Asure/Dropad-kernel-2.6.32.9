/* sound/soc/s3c24xx/s5p-spdif.c
 *
 * ALSA SoC Audio Layer - S5P S/PDIF Controller driver
 *
 * Copyright (c) 2010 Samsung Electronics Co. Ltd
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/clk.h>

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <plat/audio.h>
#include <mach/dma.h>

#include "s3c-dma.h"
#include "s5p-spdif.h"

static struct s3c2410_dma_client s5p_spdif_dma_client_out = {
	.name		= "S/PDIF Stereo out",
};
static struct s3c_dma_params s5p_spdif_stereo_out;
static struct s5p_spdif_info s5p_spdif;
struct snd_soc_dai s5p_spdif_dai;
EXPORT_SYMBOL_GPL(s5p_spdif_dai);

static inline struct s5p_spdif_info *to_info(struct snd_soc_dai *cpu_dai)
{
	return cpu_dai->private_data;
}

static void s5p_spdif_snd_txctrl(struct s5p_spdif_info *spdif, int on)
{
	void __iomem *regs = spdif->regs;
	u32 clkcon;

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	clkcon = readl(regs + CLKCON);

	if (on)
		clkcon |= CLKCTL_PWR_ON;
	else
		clkcon |= CLKCTL_PWR_OFF;

	writel(clkcon, regs + CLKCON);
}

static int s5p_spdif_set_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct s5p_spdif_info *spdif = to_info(cpu_dai);
	void __iomem *regs = spdif->regs;
	u32 clkcon;

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	clkcon = readl(regs + CLKCON);
	switch (clk_id) {
	case S5P_SPDIF_CLKSRC_INT:
		clkcon |= CLKCTL_MCLK_INT;
		spdif->use_int_clk = 1;
		break;
	case S5P_SPDIF_CLKSRC_EXT:
		clkcon |= CLKCTL_MCLK_EXT;
		spdif->use_int_clk = 0;
		break;
	default:
		return -EINVAL;
	}

	writel(clkcon, regs + CLKCON);

	return 0;
}

static int s5p_spdif_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rt = substream->private_data;
	struct s5p_spdif_info *spdif = to_info(rt->dai->cpu_dai);
	unsigned long flags;

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		return -EINVAL;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		spin_lock_irqsave(&spdif->lock, flags);
		s5p_spdif_snd_txctrl(spdif, 1);
		spin_unlock_irqrestore(&spdif->lock, flags);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		spin_lock_irqsave(&spdif->lock, flags);
		s5p_spdif_snd_txctrl(spdif, 0);
		spin_unlock_irqrestore(&spdif->lock, flags);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s5p_spdif_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *socdai)
{
	struct snd_soc_pcm_runtime *rt = substream->private_data;
	struct snd_soc_dai_link *dai = rt->dai;
	struct s5p_spdif_info *spdif = to_info(dai->cpu_dai);
	struct s3c_dma_params *dma_data;
	void __iomem *regs = spdif->regs;
	int rfs;
	u32 con, cstas;
	unsigned long flags, freq;

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = spdif->dma_playback;
	else {
		printk(KERN_ERR "capture is not supported\n");
		return -EINVAL;
	}
	//snd_soc_dai_set_dma_data(dai->cpu_dai, substream, dma_data);
	dai->cpu_dai->dma_data = dma_data;

	spin_lock_irqsave(&spdif->lock, flags);

	con = readl(regs + CON);
	cstas = readl(regs + CSTAS);

	con &= ~(CON_FIFO_TH_MASK);
	con |= (0x7 << CON_FIFO_TH_SHIFT);
	con |= CON_USERDATA_23RDBIT;
	con |= CON_PCM_DATA;

	/* S3C DMA(PL330) only support 1 byte,2 bytes and 4 bytes for
	 * data transfer, and S5P S/PDIF TX FIFO max data bits are 24.
	 * So, S/PDIF only can use signed 16 bit data format.
	 */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		con |= CON_PCM_16BIT;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 44100:
	case 32000:
	case 48000:
	case 96000:
		rfs = 512;
		con |= CON_MCLKDIV_512FS;
		break;
	default:
		return -EINVAL;
	}

	if (spdif->use_int_clk) {
		freq = params_rate(params) * rfs;
		if (spdif->clk_rate != freq) {
			clk_set_rate(spdif->int_rclk, freq);
			spdif->clk_rate = freq;
		}
	}

	/* Set Clock Accuracy Level I: (+/-)50ppm */
	cstas &= ~(CSTAS_CLOCK_ACCURACY_MASK);
	cstas |= CSTAS_CLOCK_ACCURACY_LEV1;

	cstas &= ~(CSTAS_SAMP_FREQ_MASK);
	switch (params_rate(params)) {
	case 44100:
		cstas |= CSTAS_SAMP_FREQ_44;
		break;
	case 48000:
		cstas |= CSTAS_SAMP_FREQ_48;
		break;
	case 32000:
		cstas |= CSTAS_SAMP_FREQ_32;
		break;
	case 96000:
		cstas |= CSTAS_SAMP_FREQ_96;
		break;
	default:
		return -EINVAL;
	}

	cstas &= ~(CSTAS_CATEGORY_MASK);
	cstas |= CSTAS_CATEGORY_CODE_CDP;
	cstas |= CSTAS_NO_COPYRIGHT;

	writel(con, regs + CON);
	writel(cstas, regs + CSTAS);

	spin_unlock_irqrestore(&spdif->lock, flags);

	return 0;
}

static void s5p_spdif_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rt = substream->private_data;
	struct s5p_spdif_info *spdif = to_info(rt->dai->cpu_dai);
	void __iomem *regs = spdif->regs;

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	writel(CON_SW_RESET, regs + CON);
	cpu_relax();

	writel(CLKCTL_PWR_OFF, regs + CLKCON);
}

#ifdef CONFIG_PM
static int s5p_spdif_suspend(struct snd_soc_dai *cpu_dai)
{
	struct s5p_spdif_info *spdif = to_info(cpu_dai);
	u32 con = spdif->saved_con;

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	spdif->saved_clkcon = readl(spdif->regs	+ CLKCON);
	spdif->saved_con = readl(spdif->regs + CON);
	spdif->saved_cstas = readl(spdif->regs + CSTAS);

	writel(con | CON_SW_RESET, spdif->regs + CON);
	cpu_relax();

	return 0;
}

static int s5p_spdif_resume(struct snd_soc_dai *cpu_dai)
{
	struct s5p_spdif_info *spdif = to_info(cpu_dai);

	dev_dbg(spdif->dev, "Entered %s\n", __func__);

	writel(spdif->saved_clkcon, spdif->regs	+ CLKCON);
	writel(spdif->saved_con, spdif->regs + CON);
	writel(spdif->saved_cstas, spdif->regs + CSTAS);

	return 0;
}
#else
#define s5p_spdif_suspend NULL
#define s5p_spdif_resume NULL
#endif

static struct snd_soc_dai_ops s5p_spdif_dai_ops = {
	.set_sysclk	= s5p_spdif_set_sysclk,
	.trigger	= s5p_spdif_trigger,
	.hw_params	= s5p_spdif_hw_params,
	.shutdown	= s5p_spdif_shutdown,
};

#define S5P_SPDIF_RATES	(SNDRV_PCM_RATE_32000 |		\
			SNDRV_PCM_RATE_44100 |		\
			SNDRV_PCM_RATE_48000 |		\
			SNDRV_PCM_RATE_96000)
#define S5P_SPDIF_FORMATS	SNDRV_PCM_FMTBIT_S16_LE

static __devinit int s5p_spdif_probe(struct platform_device *pdev)
{
	int ret;
	struct s5p_spdif_info *spdif;
	struct snd_soc_dai *dai;
	struct resource *mem_res, *dma_res;
	struct s3c_audio_pdata *spdif_pdata;
	struct clk *fout_epll, *mout_epll, *mout_audio, *clk_spdif;

	spdif_pdata = pdev->dev.platform_data;

	dev_dbg(&pdev->dev, "Entered %s\n", __func__);

	dma_res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!dma_res) {
		dev_err(&pdev->dev, "Unable to get dma resource.\n");
		return -ENXIO;
	}

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "Unable to get register resource.\n");
		return -ENXIO;
	}

	if (spdif_pdata && spdif_pdata->cfg_gpio
			&& spdif_pdata->cfg_gpio(pdev)) {
		dev_err(&pdev->dev, "Unable to configure GPIO pins\n");
		return -EINVAL;
	}

	/* Fill-up basic DAI features */
	s5p_spdif_dai.name = "s5p-spdif";
	s5p_spdif_dai.ops = &s5p_spdif_dai_ops;
	s5p_spdif_dai.suspend = s5p_spdif_suspend;
	s5p_spdif_dai.resume = s5p_spdif_resume;
	s5p_spdif_dai.playback.channels_min = 2;
	s5p_spdif_dai.playback.channels_max = 2;
	s5p_spdif_dai.playback.rates = S5P_SPDIF_RATES;
	s5p_spdif_dai.playback.formats = S5P_SPDIF_FORMATS;

	spdif = &s5p_spdif;
	spdif->dev = &pdev->dev;

	spin_lock_init(&spdif->lock);

	/* Audio Clock
	 * fout_epll >> mout_epll >> mout_audio >> sclk_audio(pcm->cclk)
	 */
	fout_epll = clk_get(&pdev->dev, "fout_epll");
	if (IS_ERR(fout_epll)) {
		dev_err(&pdev->dev, "failed to get fout_epll\n");
		ret = PTR_ERR(fout_epll);
		clk_put(fout_epll);
		goto err0;
	}
	clk_enable(fout_epll);

	mout_epll = clk_get(&pdev->dev, "mout_epll");
	if (IS_ERR(mout_epll)) {
		dev_err(&pdev->dev, "failed to get mout_epll\n");
		ret = PTR_ERR(mout_epll);
		clk_put(mout_epll);
		goto err0;
	}
	clk_enable(mout_epll);
	clk_set_parent(mout_epll, fout_epll);

	mout_audio = clk_get(&pdev->dev, "mout_audio");
	if (IS_ERR(mout_audio)) {
		dev_err(&pdev->dev, "failed to get mout_audio\n");
		ret = PTR_ERR(mout_audio);
		clk_put(mout_audio);
		goto err0;
	}
	clk_enable(mout_audio);
	clk_set_parent(mout_audio, mout_epll);

	/* for Clock Gating */
	clk_spdif = clk_get(&pdev->dev, "spdif");
	if (IS_ERR(clk_spdif)) {
		dev_err(&pdev->dev, "failed to get clk_spdif\n");
		ret = PTR_ERR(clk_spdif);
		clk_put(clk_spdif);
		goto err0;
	}
	clk_enable(clk_spdif);

	spdif->int_sclk = clk_get(&pdev->dev, "sclk_spdif");
	if (IS_ERR(spdif->int_sclk)) {
		dev_err(&pdev->dev, "failed to get internal source clock\n");
		ret = -ENOENT;
		goto err0;
	}
	clk_enable(spdif->int_sclk);

	spdif->use_int_clk = 1;

	spdif->int_rclk = clk_get_parent(spdif->int_sclk);
	if (IS_ERR(spdif->int_rclk)) {
		dev_err(&pdev->dev, "failed to get internal root clock\n");
		ret = -ENOENT;
		goto err1;
	}
	clk_enable(spdif->int_rclk);

	/* Request SPDIF Register's memory region */
	if (!request_mem_region(mem_res->start,
				resource_size(mem_res), "s5p-spdif")) {
		dev_err(&pdev->dev, "Unable to request register region\n");
		ret = -EBUSY;
		//goto err3;
		goto err2;
	}

	spdif->regs = ioremap(mem_res->start, 0x100);
	if (spdif->regs == NULL) {
		dev_err(&pdev->dev, "Cannot ioremap registers\n");
		ret = -ENXIO;
		goto err4;
	}

	/* Register cpu dai */
	dai = &s5p_spdif_dai;
	dai->dev = &pdev->dev;
	dai->private_data = spdif;

	ret = snd_soc_register_dai(dai);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to register cpu dai\n");
		goto err5;
	}

	/* S3C DMA(PL330) only support 1 byte,2 bytes and 4 bytes for
	 * data transfer, and S5P S/PDIF TX FIFO max data bits are 24.
	 * So, 16 bit data(2 bytes) is an only way to DMA transfer.
	 */
	s5p_spdif_stereo_out.dma_size = 2;
	s5p_spdif_stereo_out.client = &s5p_spdif_dma_client_out;
	s5p_spdif_stereo_out.dma_addr = mem_res->start + DATABUF;
	s5p_spdif_stereo_out.channel = dma_res->start;

	spdif->dma_playback = &s5p_spdif_stereo_out;

return 0;

err5:
	iounmap(spdif->regs);
err4:
	release_mem_region(mem_res->start, resource_size(mem_res));
err2:
	clk_disable(spdif->int_sclk);
	clk_put(spdif->int_sclk);
err1:
	clk_disable(spdif->int_rclk);
	clk_put(spdif->int_rclk);
err0:
	return ret;
}

static __devexit int s5p_spdif_remove(struct platform_device *pdev)
{
	struct snd_soc_dai *dai = &s5p_spdif_dai;
	struct s5p_spdif_info *spdif = &s5p_spdif;
	struct resource *mem_res;

	snd_soc_unregister_dai(dai);

	iounmap(spdif->regs);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem_res->start, resource_size(mem_res));

	clk_disable(spdif->int_sclk);
	clk_disable(spdif->int_rclk);
	clk_put(spdif->int_sclk);
	clk_put(spdif->int_rclk);

	return 0;
}

static struct platform_driver s5p_spdif_driver = {
	.probe	= s5p_spdif_probe,
	.remove	= s5p_spdif_remove,
	.driver	= {
		.name	= "s5p-spdif",
		.owner	= THIS_MODULE,
	},
};

static int __init s5p_spdif_init(void)
{
	return platform_driver_register(&s5p_spdif_driver);
}
module_init(s5p_spdif_init);

static void __exit s5p_spdif_exit(void)
{
	platform_driver_unregister(&s5p_spdif_driver);
}
module_exit(s5p_spdif_exit);

MODULE_AUTHOR("Seungwhan Youn, <sw.youn@samsung.com>");
MODULE_DESCRIPTION("S5P S/PDIF Controller Driver");
MODULE_LICENSE("GPL");
