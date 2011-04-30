/* sound/soc/s3c24xx/s5p-spdif.h
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

#ifndef __SND_SOC_S5P_SPDIF_H
#define __SND_SOC_S5P_SPDIF_H	__FILE__

/**
 * struct s5p_spdif_info - S5P S/PDIF Controller information
 * @lock: spin lock.
 * @dev: The parent device passed to use from the probe.
 * @regs: The pointer to the device register block.
 * @clk_rate: Current clock rate to avoid duplicate clock set.
 * @int_clk: The pointer to internal clock to use.
 * @ext_clk: The pointer to external clock to use.
 * @save_clkcon: Backup clkcon reg. in suspend.
 * @save_con: Backup con reg. in suspend.
 * @save_cstas: Backup cstas reg. in suspend.
 * @dma_playback: DMA information for playback channel.
 */
struct s5p_spdif_info {
	spinlock_t	lock;
	struct device	*dev;
	void __iomem	*regs;
	u32		clk_rate;
	int		use_int_clk;

	struct clk	*int_rclk;
	struct clk	*int_sclk;

	u32		saved_clkcon;
	u32		saved_con;
	u32		saved_cstas;

	struct s3c_dma_params	*dma_playback;
};

extern struct snd_soc_dai s5p_spdif_dai;

/* Registers */
#define CLKCON				(0x00)
#define CON				(0x04)
#define BSTAS				(0x08)
#define CSTAS				(0x0C)
#define DATABUF				(0x10)
#define DCNT				(0x14)
#define BSTAS_S				(0x18)
#define DCNT_S				(0x1C)

#define CLKCTL_MCLK_INT			(0x0<<2)
#define CLKCTL_MCLK_EXT			(0x1<<2)
#define CLKCTL_PWR_OFF			(0x0<<0)
#define CLKCTL_PWR_ON			(0x1<<0)

#define CON_FIFO_TH_SHIFT		(19)
#define CON_FIFO_TH_MASK		(0x7<<19)
#define CON_FIFO_TRMODE_SHIFT		(17)
#define CON_FIFO_TRMODE_MASK		(0x3<<17)
#define CON_FIFO_INT_PEND_CLR		(0x1<<16)
#define CON_FIFO_INT_ENB		(0x1<<15)
#define CON_FIFO_INT_DIS		(0x0<<15)
#define CON_USERDATA_USERBIT		(0x0<<12)
#define CON_USERDATA_23RDBIT		(0x1<<12)

#define CON_SW_RESET			(0x1<<5)

#define CON_MCLKDIV_MASK		(0x3<<3)
#define CON_MCLKDIV_256FS		(0x0<<3)
#define CON_MCLKDIV_384FS		(0x1<<3)
#define CON_MCLKDIV_512FS		(0x2<<3)

#define CON_PCM_MASK			(0x3<<1)
#define CON_PCM_16BIT			(0x0<<1)
#define CON_PCM_20BIT			(0x1<<1)
#define CON_PCM_24BIT			(0x2<<1)

#define CON_STREAM_DATA			(0x0<<0)
#define CON_PCM_DATA			(0x1<<0)

#define CSTAS_CLOCK_ACCURACY_MASK	(0x3<<28)
#define CSTAS_CLOCK_ACCURACY_LEV1	(0x2<<28)
#define CSTAS_CLOCK_ACCURACY_LEV2	(0x0<<28)
#define CSTAS_CLOCK_ACCURACY_LEV3	(0x1<<28)

#define CSTAS_SAMP_FREQ_MASK		(0xF<<24)
#define CSTAS_SAMP_FREQ_44		(0x0<<24)
#define CSTAS_SAMP_FREQ_48		(0x2<<24)
#define CSTAS_SAMP_FREQ_32		(0x3<<24)
#define CSTAS_SAMP_FREQ_96		(0xA<<24)

#define CSTAS_CATEGORY_MASK		(0xFF<<8)
#define CSTAS_CATEGORY_CODE_CDP		(0x01<<8)

#define CSTAS_SET_COPYRIGHT		(0x0<<2)
#define CSTAS_NO_COPYRIGHT		(0x1<<2)

#define S5P_SPDIF_CLKSRC_INT		(0)
#define S5P_SPDIF_CLKSRC_EXT		(1)

#define S5P_SPDIF_MCLK_FS		(0)

#endif	/* __SND_SOC_S5P_SPDIF_H */
