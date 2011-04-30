/* linux/arch/arm/plat-s5p/include/plat/s3c-pl330-pdata.h
 *
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

#ifndef __S3C_PL330_PDATA_H
#define __S3C_PL330_PDATA_H

/*
 * The platforms just need to provide this info
 * to the S3C DMA API driver for PL330.
 */
struct s3c_pl330_platdata {
	enum dma_ch peri[32];
};

#endif /* __S3C_PL330_PDATA_H */
