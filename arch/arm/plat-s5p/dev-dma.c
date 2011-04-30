/*
 * Copyright (C) 2010 Samsung Electronics Co. Ltd.
 *	Jaswinder Singh <jassi.brar@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <mach/irqs.h>

#include <mach/map.h>
#include <mach/dma.h>

#include <plat/s3c-pl330-pdata.h>

static struct resource s5pc11x_mdma_resource[] = {
	[0] = {
		.start  = S5PC110_PA_MDMA,
		.end    = S5PC110_PA_MDMA + SZ_4K,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_MDMA,
		.end	= IRQ_MDMA,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource s5pc11x_pdma0_resource[] = {
	[0] = {
		.start  = S5PC110_PA_PDMA0,
		.end    = S5PC110_PA_PDMA0 + SZ_4K,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_PDMA0,
		.end	= IRQ_PDMA0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource s5pc11x_pdma1_resource[] = {
	[0] = {
		.start  = S5PC110_PA_PDMA1,
		.end    = S5PC110_PA_PDMA1 + SZ_4K,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_PDMA1,
		.end	= IRQ_PDMA1,
		.flags	= IORESOURCE_IRQ,
	},
};

struct s3c_pl330_platdata s5pc11x_mdma_pdata = {
	.peri = {
		/* The DMAC can have max 8 channel so there
		 * can be 8 M<->M requests served at any time.
		 */
		[0] = DMACH_MTOM_0,
		[1] = DMACH_MTOM_1,
		[2] = DMACH_MTOM_2,
		[3] = DMACH_MTOM_3,
		[4] = DMACH_MTOM_4,
		[5] = DMACH_MTOM_5,
		[6] = DMACH_MTOM_6,
		[7] = DMACH_MTOM_7,
		[8] = DMACH_MAX,
		[9] = DMACH_MAX,
		[10] = DMACH_MAX,
		[11] = DMACH_MAX,
		[12] = DMACH_MAX,
		[13] = DMACH_MAX,
		[14] = DMACH_MAX,
		[15] = DMACH_MAX,
		[16] = DMACH_MAX,
		[17] = DMACH_MAX,
		[18] = DMACH_MAX,
		[19] = DMACH_MAX,
		[20] = DMACH_MAX,
		[21] = DMACH_MAX,
		[22] = DMACH_MAX,
		[23] = DMACH_MAX,
		[24] = DMACH_MAX,
		[25] = DMACH_MAX,
		[26] = DMACH_MAX,
		[27] = DMACH_MAX,
		[28] = DMACH_MAX,
		[29] = DMACH_MAX,
		[30] = DMACH_MAX,
		[31] = DMACH_MAX,
	},
};

struct s3c_pl330_platdata s5pc11x_pdma1_pdata = {
	.peri = {
		[0] = DMACH_UART0_RX,
		[1] = DMACH_UART0_TX,
		[2] = DMACH_UART1_RX,
		[3] = DMACH_UART1_TX,
		[4] = DMACH_UART2_RX,
		[5] = DMACH_UART2_TX,
		[6] = DMACH_UART3_RX,
		[7] = DMACH_UART3_TX,
		[8] = DMACH_MTOM_8,
		[9] = DMACH_I2S0_IN,
		[10] = DMACH_I2S0_OUT,
		[11] = DMACH_I2S0_OUT_S,
		[12] = DMACH_I2S1_IN,
		[13] = DMACH_I2S1_OUT,
		[14] = DMACH_I2S2_IN,
		[15] = DMACH_I2S2_OUT,
		[16] = DMACH_SPI0_RX,
		[17] = DMACH_SPI0_TX,
		[18] = DMACH_SPI1_RX,
		[19] = DMACH_SPI1_TX,
		[20] = DMACH_MTOM_9,
		[21] = DMACH_MTOM_10,
		[22] = DMACH_PCM0_RX,
		[23] = DMACH_PCM0_TX,
		[24] = DMACH_PCM1_RX,
		[25] = DMACH_PCM1_TX,
		[26] = DMACH_MSM_REQ0,
		[27] = DMACH_MSM_REQ1,
		[28] = DMACH_MSM_REQ2,
		[29] = DMACH_MSM_REQ3,
		[30] = DMACH_PCM2_RX,
		[31] = DMACH_PCM2_TX,
	},
};

struct s3c_pl330_platdata s5pc11x_pdma0_pdata = {
	.peri = {
		[0] = DMACH_UART0_RX,
		[1] = DMACH_UART0_TX,
		[2] = DMACH_UART1_RX,
		[3] = DMACH_UART1_TX,
		[4] = DMACH_UART2_RX,
		[5] = DMACH_UART2_TX,
		[6] = DMACH_UART3_RX,
		[7] = DMACH_UART3_TX,
		[8] = DMACH_MTOM_11,
		[9] = DMACH_I2S0_IN,
		[10] = DMACH_I2S0_OUT,
		[11] = DMACH_I2S0_OUT_S,
		[12] = DMACH_I2S1_IN,
		[13] = DMACH_I2S1_OUT,
		[14] = DMACH_MTOM_12,
		[15] = DMACH_MTOM_13,
		[16] = DMACH_SPI0_RX,
		[17] = DMACH_SPI0_TX,
		[18] = DMACH_SPI1_RX,
		[19] = DMACH_SPI1_TX,
		[20] = DMACH_MTOM_14,
		[21] = DMACH_MTOM_15,
		[22] = DMACH_AC97_MICIN,
		[23] = DMACH_AC97_PCMIN,
		[24] = DMACH_AC97_PCMOUT,
		[25] = DMACH_MTOM_16,
		[26] = DMACH_PWM,
		[27] = DMACH_SPDIF_OUT,
		[28] = DMACH_MTOM_17,
		[29] = DMACH_MTOM_18,
		[30] = DMACH_MAX,
		[31] = DMACH_MAX,
	},
};

static u64 dma_dmamask = DMA_BIT_MASK(32);

struct platform_device s5pc11x_device_mdma = {
	.name		= "s3c-pl330",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s5pc11x_mdma_resource),
	.resource	= s5pc11x_mdma_resource,
	.dev		= {
		.dma_mask = &dma_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &s5pc11x_mdma_pdata,
	},
};
EXPORT_SYMBOL(s5pc11x_device_mdma);

struct platform_device s5pc11x_device_pdma0 = {
	.name		= "s3c-pl330",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(s5pc11x_pdma0_resource),
	.resource	= s5pc11x_pdma0_resource,
	.dev		= {
		.dma_mask = &dma_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &s5pc11x_pdma0_pdata,
	},
};
EXPORT_SYMBOL(s5pc11x_device_pdma0);

struct platform_device s5pc11x_device_pdma1 = {
	.name		= "s3c-pl330",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(s5pc11x_pdma1_resource),
	.resource	= s5pc11x_pdma1_resource,
	.dev		= {
		.dma_mask = &dma_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &s5pc11x_pdma1_pdata,
	},
};
EXPORT_SYMBOL(s5pc11x_device_pdma1);
