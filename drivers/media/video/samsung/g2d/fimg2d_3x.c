/* linux/drivers/media/video/samsung/g2d/fimg2d_3x.c
 *
 * Copyright  2008 Samsung Electronics Co, Ltd. All Rights Reserved. 
 *		      http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file implements sec-g2d driver.
 */

#include <linux/init.h>

#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <asm/uaccess.h>
#include <linux/errno.h> /* error codes */
#include <asm/div64.h>
#include <linux/tty.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>

#include <linux/version.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/signal.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/semaphore.h>

#include <asm/io.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/cacheflush.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>

#include <mach/hardware.h>
#include <mach/map.h>
#include <mach/pd.h>
#include <plat/media.h>
#include <plat/cpu.h>

#ifdef CONFIG_CPU_FREQ_S5PV210
#include <mach/cpu-freq-v210.h>
#endif // CONFIG_CPU_FREQ_S5PV210

#include "fimg2d_regs.h"
#include "fimg2d_3x.h"

#define G2D_CLK_GATING
//#define G2D_DEBUG
//#define G2D_CHECK_HW_VERSION
//#define G2D_CHECK_TIME
#define G2D_USE_SPIN_LOCK_FOR_POWER

#ifdef G2D_CHECK_HW_VERSION
	extern int hw_version_check();
	static int g_hw_version = 0;
#endif

static int               g_g2d_irq_num = NO_IRQ;
static struct resource * g_g2d_mem;
static void   __iomem  * g_g2d_base;

static wait_queue_head_t g_g2d_waitq;
static int               g_in_use = 0;
static int               g_num_of_g2d_object = 0;

#ifdef	G2D_CLK_GATING 
	static struct clk * g_g2d_clock;
	static int          g_flag_clk_enable = 0;	
	static struct timer_list  g_g2d_domain_timer;

	#ifdef G2D_USE_SPIN_LOCK_FOR_POWER
		static DEFINE_SPINLOCK(g_g2d_clk_spinlock);
	#endif
#endif

static DEFINE_MUTEX(g_g2d_rot_mutex);

static u32 g_g2d_reserved_phys_addr = 0;
static u32 g_g2d_reserved_size = 0;

static u32 g_g2d_src_phys_addr = 0;
static u32 g_g2d_src_virt_addr = 0;
static u32 g_g2d_src_size      = 0;

static u32 g_g2d_dst_phys_addr = 0;
static u32 g_g2d_dst_virt_addr = 0;
static u32 g_g2d_dst_size      = 0;

#ifdef G2D_CHECK_TIME

#include <linux/time.h>

#define   NUM_OF_STEP (10)
char *           g_time_watch_name[NUM_OF_STEP];
struct timeval   g_time_watch[NUM_OF_STEP];
int              g_time_watch_index = 0;

long sec_g2d_get_time_dur(struct timeval * time_before, struct timeval * time_after)
{
	long sec  = 0;
	long msec = 0;

	//timeval_to_ns
	if (time_before->tv_usec <= time_after->tv_usec)
	{
		msec = time_after->tv_usec - time_before->tv_usec;
		sec  = time_after->tv_sec  - time_before->tv_sec;
	}
	else // if (time_before->tv_usec > time_after->tv_usec)
	{
		msec = 1000000 + time_after->tv_usec - time_before->tv_usec;
		sec  = time_after->tv_sec - time_before->tv_sec -1;
	}

	return ((1000000 * sec) + msec);

}

void sec_g2d_check_time(char * name, int flag_print)
{
	do_gettimeofday(&g_time_watch[ g_time_watch_index]);

	g_time_watch_name[g_time_watch_index] = name;
	g_time_watch_index++;

	if(flag_print == 1)
	{
		int i = 0;
		long time_watch_dur;
		long total_dur;
		
		// 1 has no meaning..
		if(g_time_watch_index <= 0)
		{
			g_time_watch_index = 0;
			return;
		}

		total_dur = sec_g2d_get_time_dur(&g_time_watch[0], &g_time_watch[g_time_watch_index - 1]);

		for(i = 1; i < g_time_watch_index; i++)
		{			
			time_watch_dur = sec_g2d_get_time_dur(&g_time_watch[i-1], &g_time_watch[i]);

			printk("%10s : %5ld msec(%2ld.%3ld %%)\t",
				g_time_watch_name[i],
				time_watch_dur,
				((time_watch_dur * 100) / total_dur),
				((time_watch_dur * 100) % total_dur));
		}

		printk("\n");
		g_time_watch_index = 0;
	}
}
 
#endif // G2D_CHECK_TIME

u32 sec_g2d_check_fifo_stat_wait(void)
{
	int cnt = 50;
	// 1 = The graphics engine finishes the execution of command.
	// 0 = in the middle of rendering process.
	while((!(__raw_readl(g_g2d_base + FIFO_STAT_REG) & 0x1)) && (cnt > 0)){
		cnt--;
		msleep_interruptible(2);
	}

	if(cnt <= 0){
		__raw_writel(1, g_g2d_base +FIFO_STAT_REG);
		return -1;
	}

	return 0;
}

static inline unsigned int get_stride_from_color_space(u32 color_space)
{
	unsigned int stride = 2;

	switch (color_space)
	{
		// use default:
		/*
		case G2D_RGB_565:
			stride = 2;
			break;
		*/

		case G2D_RGBA_8888:
		case G2D_ARGB_8888:
		case G2D_BGRA_8888:
		case G2D_ABGR_8888:

		case G2D_RGBX_8888:
		case G2D_XRGB_8888:
		case G2D_BGRX_8888:
		case G2D_XBGR_8888:
			stride = 4;
			break;

		// use default:
		/*
		case G2D_RGBA_5551:
		case G2D_ARGB_1555:
		case G2D_BGRA_5551:
		case G2D_ABGR_1555:

		case G2D_RGBX_5551:
		case G2D_XRGB_1555:
		case G2D_BGRX_5551:
		case G2D_XBGR_1555:

		case G2D_RGBA_4444:
		case G2D_ARGB_4444:
		case G2D_BGRA_4444:
		case G2D_ABGR_4444:

		case G2D_RGBX_4444:
		case G2D_XRGB_4444:
		case G2D_BGRX_4444:
		case G2D_XBGR_4444:
			stride = 2;
			break;
		*/

		case G2D_PACKED_RGB_888:
		case G2D_PACKED_BGR_888:
			stride = 3;
			break;

		default:
			stride = 2;
			break;
			
	}
	return stride;
}

static inline u32 get_color_mode_from_color_space(u32 color_space)
{
	switch (color_space)
	{
		case G2D_RGB_565:        return (G2D_CHL_ORDER_XRGB | G2D_FMT_RGB_565);
		
		case G2D_RGBA_8888:      return (G2D_CHL_ORDER_RGBX | G2D_FMT_ARGB_8888);
		case G2D_ARGB_8888:      return (G2D_CHL_ORDER_XRGB | G2D_FMT_ARGB_8888);
		case G2D_BGRA_8888:      return (G2D_CHL_ORDER_BGRX | G2D_FMT_ARGB_8888);
		case G2D_ABGR_8888:      return (G2D_CHL_ORDER_XBGR | G2D_FMT_ARGB_8888);

		case G2D_RGBX_8888:      return (G2D_CHL_ORDER_RGBX | G2D_FMT_XRGB_8888);
		case G2D_XRGB_8888:      return (G2D_CHL_ORDER_XRGB | G2D_FMT_XRGB_8888);
		case G2D_BGRX_8888:      return (G2D_CHL_ORDER_BGRX | G2D_FMT_XRGB_8888);
		case G2D_XBGR_8888:      return (G2D_CHL_ORDER_XBGR | G2D_FMT_XRGB_8888);

		case G2D_RGBA_5551:      return (G2D_CHL_ORDER_RGBX | G2D_FMT_ARGB_1555);
		case G2D_ARGB_1555:      return (G2D_CHL_ORDER_XRGB | G2D_FMT_ARGB_1555); 
		case G2D_BGRA_5551:      return (G2D_CHL_ORDER_BGRX | G2D_FMT_ARGB_1555);
		case G2D_ABGR_1555:      return (G2D_CHL_ORDER_XBGR | G2D_FMT_ARGB_1555);

		case G2D_RGBX_5551:      return (G2D_CHL_ORDER_RGBX | G2D_FMT_XRGB_1555);
		case G2D_XRGB_1555:      return (G2D_CHL_ORDER_XRGB | G2D_FMT_XRGB_1555);
		case G2D_BGRX_5551:      return (G2D_CHL_ORDER_BGRX | G2D_FMT_XRGB_1555);
		case G2D_XBGR_1555:      return (G2D_CHL_ORDER_XBGR | G2D_FMT_XRGB_1555);

		case G2D_RGBA_4444:      return (G2D_CHL_ORDER_RGBX | G2D_FMT_ARGB_4444);
		case G2D_ARGB_4444:      return (G2D_CHL_ORDER_XRGB | G2D_FMT_ARGB_4444);
		case G2D_BGRA_4444:      return (G2D_CHL_ORDER_BGRX | G2D_FMT_ARGB_4444);
		case G2D_ABGR_4444:      return (G2D_CHL_ORDER_XBGR | G2D_FMT_ARGB_4444);

		case G2D_RGBX_4444:      return (G2D_CHL_ORDER_RGBX | G2D_FMT_XRGB_4444);
		case G2D_XRGB_4444:      return (G2D_CHL_ORDER_XRGB | G2D_FMT_XRGB_4444);
		case G2D_BGRX_4444:      return (G2D_CHL_ORDER_BGRX | G2D_FMT_XRGB_4444);
		case G2D_XBGR_4444:      return (G2D_CHL_ORDER_XBGR | G2D_FMT_XRGB_4444);
		
		case G2D_PACKED_RGB_888: return (G2D_CHL_ORDER_XRGB | G2D_FMT_PACKED_RGB_888);
		case G2D_PACKED_BGR_888: return (G2D_CHL_ORDER_XBGR | G2D_FMT_PACKED_RGB_888);

		default:
			printk("fimg2d: %s::unmatched color_space(%d) \n", __func__, color_space);
			return 0;
	}
}

u32 get_color_format_from_color_space(u32 color_space)
{
	switch (color_space)
	{
		case G2D_RGB_565:	
			return G2D_FMT_RGB_565;

		case G2D_RGBA_8888:
		case G2D_ARGB_8888:
		case G2D_BGRA_8888:
		case G2D_ABGR_8888:
			return G2D_FMT_ARGB_8888;

		case G2D_RGBX_8888:
		case G2D_XRGB_8888:
		case G2D_BGRX_8888:
		case G2D_XBGR_8888:		
			return G2D_FMT_XRGB_8888;

		case G2D_RGBA_5551:
		case G2D_ARGB_1555:
		case G2D_BGRA_5551:
		case G2D_ABGR_1555:
			return G2D_FMT_ARGB_1555;

		case G2D_RGBX_5551:
		case G2D_XRGB_1555:
		case G2D_BGRX_5551:
		case G2D_XBGR_1555:
			return G2D_FMT_XRGB_1555;

		case G2D_RGBA_4444:
		case G2D_ARGB_4444:
		case G2D_BGRA_4444:
		case G2D_ABGR_4444:		
			return G2D_FMT_ARGB_4444;

		case G2D_RGBX_4444:
		case G2D_XRGB_4444:
		case G2D_BGRX_4444:
		case G2D_XBGR_4444:		
			return G2D_FMT_XRGB_4444;

		case G2D_PACKED_RGB_888:
		case G2D_PACKED_BGR_888:
			return G2D_FMT_PACKED_RGB_888;

		default:
			printk("fimg2d: %s::unmatched color_space(%d) \n", __func__, color_space);
			return 0;
	}
}

static void get_rot_config(unsigned int rotate_value, u32 *rot, u32 *src_dir, u32 *dst_dir)
{
	switch(rotate_value)
	{
		case G2D_ROT_90: 
			*rot = 1;   	// rotation = 1, src_y_dir == dst_y_dir, src_x_dir == dst_x_dir 
			*src_dir = 0;		
			*dst_dir = 0;		
			break;

		case G2D_ROT_270: 
			*rot = 1;   	// rotation = 1, src_y_dir != dst_y_dir, src_x_dir != dst_x_dir
			*src_dir = 0;
			*dst_dir = 0x3;
			break;			

		case G2D_ROT_180: 
			*rot = 0;    	// rotation = 0, src_y_dir != dst_y_dir, src_x_dir != dst_x_dir
			*src_dir = 0;
			*dst_dir = 0x3;
			break;

		case G2D_ROT_X_FLIP: 
			*rot = 0;    	// rotation = 0, src_y_dir != dst_y_dir
			*src_dir = 0;
			*dst_dir = 0x2;
			break;

		case G2D_ROT_Y_FLIP: 
			*rot = 0;    	// rotation = 0, src_x_dir != dst_y_dir
			*src_dir = 0;
			*dst_dir = 0x1;
			break;

		default :
			*rot = 0;   	// rotation = 0;
			*src_dir = 0;
			*dst_dir = 0;
			break;
	}
	
	return ;
}

static int sec_g2d_check_params(g2d_params *params)
{
	g2d_rect * src_rect = params->src_rect;
	g2d_rect * dst_rect = params->dst_rect;
	g2d_flag * flag     = params->flag;

	/* source */
	if (0 == src_rect->h || 0 == src_rect->w)
		return -1;
	if (   8000 < src_rect->x+src_rect->w
	    || 8000 < src_rect->y+src_rect->h)
	    return -1;

	/* destination */
	if (0 == dst_rect->h || 0 == dst_rect->w)
		return -1;

	if (   8000 < dst_rect->x+dst_rect->w
	    || 8000 < dst_rect->y+dst_rect->h)
	    return -1;


	if (   src_rect->color_format >= G2D_MAX_COLOR_SPACE
	    || dst_rect->color_format >= G2D_MAX_COLOR_SPACE)
	    return -1;

	if (flag->alpha_val > G2D_ALPHA_BLENDING_OPAQUE)
		return -1; 

	return 0;
}

u32 sec_g2d_set_src_img(g2d_rect * src_rect, g2d_rect * dst_rect, g2d_flag * flag)
{
	u32 data    = 0;
	u32 blt_cmd = 0;

	// set  source to one color
	if(src_rect == NULL)
	{
		// select source
		__raw_writel(G2D_SRC_SELECT_R_USE_FG_COLOR, g_g2d_base + SRC_SELECT_REG);

		// this assume RGBA_8888 byte order
		switch(dst_rect->color_format)
		{
			case G2D_RGB_565 :
				data  = ((flag->color_val & 0xF8000000) >> 16); // R
				data |= ((flag->color_val & 0x00FC0000) >> 13); // G
				data |= ((flag->color_val & 0x0000F800) >> 11); // B
				break;

			case G2D_ARGB_8888:
			case G2D_XRGB_8888:
				data  = ((flag->color_val & 0xFF000000) >> 8);  // R
				data |= ((flag->color_val & 0x00FF0000) >> 8);  // G
				data |= ((flag->color_val & 0x0000FF00) >> 8);  // B
				data |= ((flag->color_val & 0x000000FF) << 24); // A
				break;

			case G2D_BGRA_8888:
			case G2D_BGRX_8888:
				data  = ((flag->color_val & 0xFF000000) >> 16); // R
				data |= ((flag->color_val & 0x00FF0000));       // G
				data |= ((flag->color_val & 0x0000FF00) << 16); // B
				data |= ((flag->color_val & 0x000000FF));       // A
				break;

			case G2D_ABGR_8888:
			case G2D_XBGR_8888:
				data  = ((flag->color_val & 0xFF000000) >> 24); // R
				data |= ((flag->color_val & 0x00FF0000) >> 8);  // G
				data |= ((flag->color_val & 0x0000FF00) << 8);  // B
				data |= ((flag->color_val & 0x000000FF) << 24); // A
				break;

			case G2D_RGBA_8888:
			case G2D_RGBX_8888:
			default :
				data = flag->color_val;
				break;
		}
		// foreground color
		__raw_writel(data, g_g2d_base + FG_COLOR_REG);

		// sw5771.park : these setting doesn't need..
		// set stride
		//__raw_writel(0, g_g2d_base + SRC_STRIDE_REG);
		// set color mode
		//data = get_color_mode_from_color_space(G2D_RGBX_8888);
		//__raw_writel(data, g_g2d_base + SRC_COLOR_MODE_REG);	   
	}
	else
	{
		// select source
		__raw_writel(G2D_SRC_SELECT_R_NORMAL, g_g2d_base + SRC_SELECT_REG);

		// set base address of source image
		__raw_writel(src_rect->phys_addr,   g_g2d_base + SRC_BASE_ADDR_REG);

		// set stride
		data = get_stride_from_color_space(src_rect->color_format);
		__raw_writel(src_rect->full_w * data, g_g2d_base + SRC_STRIDE_REG);

		// set color mode
		data = get_color_mode_from_color_space(src_rect->color_format);
		__raw_writel(data, g_g2d_base + SRC_COLOR_MODE_REG);

		// set coordinate of source image
		data = (src_rect->y << 16) | (src_rect->x);
		__raw_writel(data, g_g2d_base + SRC_LEFT_TOP_REG);

		data =  ((src_rect->y + src_rect->h) << 16) | (src_rect->x + src_rect->w);
		__raw_writel(data, g_g2d_base + SRC_RIGHT_BOTTOM_REG);

	}

	return blt_cmd;
}

u32 sec_g2d_set_dst_img(g2d_rect * rect)
{
	u32 data    = 0;
	u32 blt_cmd = 0;
	
	// select destination
	__raw_writel(G2D_DST_SELECT_R_NORMAL, g_g2d_base + DST_SELECT_REG);

	// set base address of destination image
	__raw_writel(rect->phys_addr,   g_g2d_base + DST_BASE_ADDR_REG);

	// set stride
	data = get_stride_from_color_space(rect->color_format);
	__raw_writel(rect->full_w * data, g_g2d_base + DST_STRIDE_REG);

	// set color mode
	data = get_color_mode_from_color_space(rect->color_format);
	__raw_writel(data, g_g2d_base + DST_COLOR_MODE_REG);

	// set coordinate of destination image
	data = (rect->y << 16) | (rect->x);
	__raw_writel(data, g_g2d_base + DST_LEFT_TOP_REG);

	data =  ((rect->y + rect->h) << 16) | (rect->x + rect->w);
	__raw_writel(data, g_g2d_base + DST_RIGHT_BOTTOM_REG);

	return blt_cmd;
}

u32 sec_g2d_set_rotation(g2d_flag * flag)
{
	u32 blt_cmd = 0;
	u32 rot=0, src_dir=0, dst_dir=0;

	get_rot_config(flag->rotate_val, &rot, &src_dir, &dst_dir);

	__raw_writel(rot,     g_g2d_base + ROTATE_REG);
	__raw_writel(src_dir, g_g2d_base + SRC_MSK_DIRECT_REG);
	__raw_writel(dst_dir, g_g2d_base + DST_PAT_DIRECT_REG);

	return blt_cmd;
}

u32 sec_g2d_set_clip_win(g2d_rect * rect)
{
	u32 blt_cmd = 0;

	//if(rect->x < rect->x + rect->w && rect->y < rect->y + rect->h)
	if(rect->w != 0 && rect->h != 0)
	{
		blt_cmd	|= G2D_BLT_CMD_R_CW_ENABLE;
		__raw_writel((rect->y << 16) | (rect->x), g_g2d_base + CW_LEFT_TOP_REG);
		__raw_writel(((rect->y + rect->h) << 16) | (rect->x + rect->w), g_g2d_base + CW_RIGHT_BOTTOM_REG);
	}

	return blt_cmd;
}

u32 sec_g2d_set_color_key(g2d_flag * flag)
{
	u32 blt_cmd = 0;
//	u32 data    = 0;

	// Transparent Selection
	switch(flag->blue_screen_mode)
	{
		// if(BlueS color == source color)
		//    reserve dst color
		case G2D_BLUE_SCREEN_TRANSPARENT :
			__raw_writel(flag->color_key_val, g_g2d_base + BS_COLOR_REG);

			blt_cmd |= G2D_BLT_CMD_R_TRANSPARENT_MODE_TRANS;
			break;
		// if(BlueS color == source color)
		//     change pixel with BG color
		case G2D_BLUE_SCREEN_WITH_COLOR :
			__raw_writel(flag->color_key_val, g_g2d_base + BS_COLOR_REG);
			__raw_writel(flag->color_val,     g_g2d_base + BG_COLOR_REG);

			blt_cmd |= G2D_BLT_CMD_R_TRANSPARENT_MODE_BLUESCR;
			break;

		case G2D_BLUE_SCREEN_NONE :
		default:
			blt_cmd |= G2D_BLT_CMD_R_TRANSPARENT_MODE_OPAQUE;
			break;
	}

	blt_cmd |= G2D_BLT_CMD_R_COLOR_KEY_DISABLE;

	return blt_cmd;
}

u32 sec_g2d_set_pattern(g2d_rect * rect, g2d_flag * flag)
{
	u32 data    = 0;
	u32 blt_cmd = 0;

	if(rect == NULL)
		data = G2D_THIRD_OP_REG_NONE;
	else
	{		
		// Third Operand Selection
		switch(flag->third_op_mode)
		{	
			case G2D_THIRD_OP_PATTERN :
			{
				// set base address of pattern image
				__raw_writel(rect->phys_addr, g_g2d_base + PAT_BASE_ADDR_REG);

				// set size of pattern image
				data =   ((rect->y + rect->h) << 16)
				       |  (rect->x + rect->w);
				__raw_writel(data, g_g2d_base + PAT_SIZE_REG);

				// set stride
				data = get_stride_from_color_space(rect->color_format);
				__raw_writel(rect->full_w * data, g_g2d_base + PAT_STRIDE_REG);

				// set color mode
				data = get_color_mode_from_color_space(rect->color_format);
				__raw_writel(data, g_g2d_base + PAT_COLOR_MODE_REG);

				// sw5771.park : need & should check..
				// set offset (x, y)		
				//data =    ((params->h - params->y) << 16)
				//        | (params->w  - params->x);
				data =   (rect->y << 16) | rect->x;
				__raw_writel(data, g_g2d_base + PAT_OFFSET_REG);

				data = G2D_THIRD_OP_REG_PATTERN;
				break;
			}
			case G2D_THIRD_OP_FG :
				data = G2D_THIRD_OP_REG_FG_COLOR;
				break;
			case G2D_THIRD_OP_BG :
				data = G2D_THIRD_OP_REG_BG_COLOR;
				break;
			case G2D_THIRD_OP_NONE :
			default:
				data = G2D_THIRD_OP_REG_NONE;
				break;
		}
	}
	__raw_writel(data, g_g2d_base + THIRD_OPERAND_REG);
	
	// Raster Operation Register

	// sw5771.park :  this should be check..
	// __raw_writel((0xCC << 8) | (0xCC << 0), g_g2d_base + ROP4_REG);
	if(flag->third_op_mode == G2D_THIRD_OP_NONE)
	{
		data = ((G2D_ROP_REG_SRC << 8) | G2D_ROP_REG_SRC);
	}
	else
	{
		switch(flag->rop_mode)
		{	
			case G2D_ROP_DST:
				data = ((G2D_ROP_REG_DST << 8) | G2D_ROP_REG_DST);
				break;
			case G2D_ROP_SRC_AND_DST:
				data = ((G2D_ROP_REG_SRC_AND_DST << 8) | G2D_ROP_REG_SRC_AND_DST);
				break;
			case G2D_ROP_SRC_OR_DST:
				data = ((G2D_ROP_REG_SRC_OR_DST << 8) | G2D_ROP_REG_SRC_OR_DST);
				break;
			case G2D_ROP_3RD_OPRND:
				data = ((G2D_ROP_REG_3RD_OPRND << 8) | G2D_ROP_REG_3RD_OPRND);
				break;
			case G2D_ROP_SRC_AND_3RD_OPRND:
				data = ((G2D_ROP_REG_SRC_AND_3RD_OPRND << 8) | G2D_ROP_REG_SRC_AND_3RD_OPRND);
				break;
			case G2D_ROP_SRC_OR_3RD_OPRND:
				data = ((G2D_ROP_REG_SRC_OR_3RD_OPRND << 8) | G2D_ROP_REG_SRC_OR_3RD_OPRND);
				break;
			case G2D_ROP_SRC_XOR_3RD_OPRND:
				data = ((G2D_ROP_REG_SRC_XOR_3RD_OPRND << 8) | G2D_ROP_REG_SRC_XOR_3RD_OPRND);
				break;
			case G2D_ROP_DST_OR_3RD:
				data = ((G2D_ROP_REG_DST_OR_3RD_OPRND << 8) | G2D_ROP_REG_DST_OR_3RD_OPRND);
				break;
			case G2D_ROP_SRC:
			default:
				data = ((G2D_ROP_REG_SRC << 8) | G2D_ROP_REG_SRC);
				break;
		}
	}
	__raw_writel(data, g_g2d_base + ROP4_REG);

	// Mask Operation
	if(rect && flag->mask_mode == TRUE)
	{
		// sw5771.park : need & should check..
		// sw5771.park : FIMG-2D V3.0 supports only 1bpp mask image format !!
		//if(((unsigned char)(params->mask_val>>8)) != ((unsigned char)params->mask_val) )	

		data = get_stride_from_color_space(rect->color_format);

		__raw_writel(rect->phys_addr,     g_g2d_base + MASK_BASE_ADDR_REG);
		__raw_writel(rect->full_w * data, g_g2d_base + MASK_STRIDE_REG);

		blt_cmd |= G2D_BLT_CMD_R_MASK_ENABLE;
	}

	return blt_cmd;
}

u32 sec_g2d_set_alpha(g2d_rect * src_rect, g2d_rect * dst_rect, g2d_flag * flag)
{
	u32 blt_cmd = 0;
//	u32 data    = 0;

	// Alpha Value
	if(flag->alpha_val <= G2D_ALPHA_VALUE_MAX) // < 255
	{
		// blt_cmd |= G2D_BLT_CMD_R_ALPHA_BLEND_ALPHA_BLEND | G2D_BLT_CMD_R_SRC_NON_PRE_BLEND_CONSTANT_ALPHA;
		blt_cmd |= G2D_BLT_CMD_R_ALPHA_BLEND_ALPHA_BLEND;
		__raw_writel((flag->alpha_val & 0xff), g_g2d_base + ALPHA_REG);
	}
	/*
	else if (fading)
	{
	
		blt_cmd |= G2D_BLT_CMD_R_ALPHA_BLEND_FADE;
		__raw_writel((flag->alpha_val & 0xff) << G2D_BLT_CMD_R_ALPHA_BLEND_FAD_OFFSET, g_g2d_base + ALPHA_REG);) {
	}
	*/
	else //if(alphaValue == G2D_ALPHA_BLENDING_OPAQUE)
	{
		blt_cmd |= G2D_BLT_CMD_R_ALPHA_BLEND_NONE;
	}


	return blt_cmd;
}

void sec_g2d_set_bitblt_cmd(g2d_rect * src_rect, g2d_rect * dst_rect, u32 blt_cmd)
{
	if(src_rect)
	{
		if (   (src_rect->w  != dst_rect->w)
		    || (src_rect->h  != dst_rect->h))
		{
			blt_cmd |= G2D_BLT_CMD_R_STRETCH_ENABLE;
		}

	}

	__raw_writel(blt_cmd, g_g2d_base + BITBLT_COMMAND_REG);
}


static int sec_g2d_init_regs(g2d_params *params)
{
	u32 blt_cmd = 0;

	g2d_rect * src_rect = params->src_rect;
	g2d_rect * dst_rect = params->dst_rect;
	g2d_flag * flag     = params->flag;

	/* source image */	
	blt_cmd |= sec_g2d_set_src_img(src_rect, dst_rect, flag);

	/* destination image */		
	blt_cmd |= sec_g2d_set_dst_img(dst_rect);

	/* rotation */
	blt_cmd |= sec_g2d_set_rotation(flag);

	/* clipping */
	// blt_cmd |= sec_g2d_set_clip_win(dst_rect);

	/* color key */
	blt_cmd |= sec_g2d_set_color_key(flag);

	/* pattern */	
	blt_cmd |= sec_g2d_set_pattern(src_rect, flag);

	/* rop & alpha blending */
	blt_cmd |= sec_g2d_set_alpha(src_rect, dst_rect, flag);

	/* command */
	sec_g2d_set_bitblt_cmd(src_rect, dst_rect, blt_cmd);

	// print arg(FOR DEBUGGING).
	//sec_g2d_print_param_n_cmd(params, blt_cmd);

	return 0;
}

static void sec_g2d_rotate_with_bitblt(g2d_params *params)
{
	// enable interrupt

	// AP team
	__raw_writel(G2D_INTEN_R_CF_ENABLE, g_g2d_base + INTEN_REG);

	__raw_writel(0x7, g_g2d_base + CACHECTL_REG);

	__raw_writel(G2D_BITBLT_R_START, g_g2d_base + BITBLT_START_REG);
}

void sec_g2d_print_param_n_cmd(g2d_params *params, u32 blt_cmd)
{	
#if 0
	g2d_rect * src_rect = params->src_rect;
	g2d_rect * dst_rect = params->dst_rect;
	g2d_flag * flag     = params->flag;
	printk("fimg2d: cmd=0x%x src(fw=%d,fh=%d,x=%d,y=%d,ww=%d,wh=%d,f=%d) dst(fw=%d,fh=%d,x=%d,y=%d,ww=%d,wh=%d,f=%d) cw(%d,%d,%d,%d) alpha(%d/%d) rot=%d\n", 
			blt_cmd, 
			src_rect->full_w,
			src_rect->full_h, 
			src_rect->x,
			src_rect->y,
			src_rect->w,
			src_rect->h,
			src_rect->color_format, 
			dst_rect->full_w,
			dst_rect->full_h, 
			dst_rect->x,
			dst_rect->y,
			dst_rect->w,
			dst_rect->h,
			dst_rect->color_format,
			flag->alpha_mode,
			flag->alpha_val);
#endif
#if 0
	{
		u32		temp[50], name[50];
		int		i = 0;

		name[i] = CONRTOL_REG;
		temp[i++] = __raw_readl(g_g2d_base+CONRTOL_REG);
		name[i] = SOFT_RESET_REG;
		temp[i++] = __raw_readl(g_g2d_base+SOFT_RESET_REG);
		name[i] = INTEN_REG;
		temp[i++] = __raw_readl(g_g2d_base+INTEN_REG);
		name[i] = INTC_PEND_REG;
		temp[i++] = __raw_readl(g_g2d_base+INTC_PEND_REG);
		name[i] = FIFO_STAT_REG;
		temp[i++] = __raw_readl(g_g2d_base+FIFO_STAT_REG);
		name[i] = AXI_ID_MODE_REG;
		temp[i++] = __raw_readl(g_g2d_base+AXI_ID_MODE_REG);
		name[i] = CACHECTL_REG;
		temp[i++] = __raw_readl(g_g2d_base+CACHECTL_REG);

		name[i] = BITBLT_START_REG;
		temp[i++] = __raw_readl(g_g2d_base+BITBLT_START_REG);
		name[i] = BITBLT_COMMAND_REG;
		temp[i++] = __raw_readl(g_g2d_base+BITBLT_COMMAND_REG);

		name[i] = ROTATE_REG;
		temp[i++] = __raw_readl(g_g2d_base+ROTATE_REG);
		name[i] = SRC_MSK_DIRECT_REG;
		temp[i++] = __raw_readl(g_g2d_base+SRC_MSK_DIRECT_REG);
		name[i] = DST_PAT_DIRECT_REG;
		temp[i++] = __raw_readl(g_g2d_base+DST_PAT_DIRECT_REG);

		name[i] = SRC_SELECT_REG;
		temp[i++] = __raw_readl(g_g2d_base+SRC_SELECT_REG);
		name[i] = SRC_BASE_ADDR_REG;
		temp[i++] = __raw_readl(g_g2d_base+SRC_BASE_ADDR_REG);
		name[i] = SRC_STRIDE_REG;
		temp[i++] = __raw_readl(g_g2d_base+SRC_STRIDE_REG);
		name[i] = SRC_COLOR_MODE_REG;
		temp[i++] = __raw_readl(g_g2d_base+SRC_COLOR_MODE_REG);
		name[i] = SRC_LEFT_TOP_REG;
		temp[i++] = __raw_readl(g_g2d_base+SRC_LEFT_TOP_REG);
		name[i] = SRC_RIGHT_BOTTOM_REG;
		temp[i++] = __raw_readl(g_g2d_base+SRC_RIGHT_BOTTOM_REG);

		name[i] = DST_SELECT_REG;
		temp[i++] = __raw_readl(g_g2d_base+DST_SELECT_REG);
		name[i] = DST_BASE_ADDR_REG;
		temp[i++] = __raw_readl(g_g2d_base+DST_BASE_ADDR_REG);
		name[i] = DST_STRIDE_REG;
		temp[i++] = __raw_readl(g_g2d_base+DST_STRIDE_REG);
		name[i] = DST_COLOR_MODE_REG;
		temp[i++] = __raw_readl(g_g2d_base+DST_COLOR_MODE_REG);
		name[i] = DST_LEFT_TOP_REG;
		temp[i++] = __raw_readl(g_g2d_base+DST_LEFT_TOP_REG);
		name[i] = DST_RIGHT_BOTTOM_REG;
		temp[i++] = __raw_readl(g_g2d_base+DST_RIGHT_BOTTOM_REG);

		name[i] = PAT_BASE_ADDR_REG;
		temp[i++] = __raw_readl(g_g2d_base+PAT_BASE_ADDR_REG);
		name[i] = PAT_SIZE_REG;
		temp[i++] = __raw_readl(g_g2d_base+PAT_SIZE_REG);
		name[i] = PAT_COLOR_MODE_REG;
		temp[i++] = __raw_readl(g_g2d_base+PAT_COLOR_MODE_REG);
		name[i] = PAT_OFFSET_REG;
		temp[i++] = __raw_readl(g_g2d_base+PAT_OFFSET_REG);
		name[i] = PAT_STRIDE_REG;
		temp[i++] = __raw_readl(g_g2d_base+PAT_STRIDE_REG);

		name[i] = MASK_BASE_ADDR_REG;
		temp[i++] = __raw_readl(g_g2d_base+MASK_BASE_ADDR_REG);
		name[i] = MASK_STRIDE_REG;
		temp[i++] = __raw_readl(g_g2d_base+MASK_STRIDE_REG);

		name[i] = CW_LEFT_TOP_REG;
		temp[i++] = __raw_readl(g_g2d_base+CW_LEFT_TOP_REG);
		name[i] = CW_RIGHT_BOTTOM_REG;
		temp[i++] = __raw_readl(g_g2d_base+CW_RIGHT_BOTTOM_REG);

		name[i] = THIRD_OPERAND_REG;
		temp[i++] = __raw_readl(g_g2d_base+THIRD_OPERAND_REG);
		name[i] = ROP4_REG;
		temp[i++] = __raw_readl(g_g2d_base+ROP4_REG);
		name[i] = ALPHA_REG;
		temp[i++] = __raw_readl(g_g2d_base+ALPHA_REG);

		name[i] = FG_COLOR_REG;
		temp[i++] = __raw_readl(g_g2d_base+FG_COLOR_REG);
		name[i] = BG_COLOR_REG;
		temp[i++] = __raw_readl(g_g2d_base+BG_COLOR_REG);
		name[i] = BS_COLOR_REG;
		temp[i++] = __raw_readl(g_g2d_base+BS_COLOR_REG);

		name[i] = SRC_COLORKEY_CTRL_REG;
		temp[i++] = __raw_readl(g_g2d_base+SRC_COLORKEY_CTRL_REG);
		name[i] = SRC_COLORKEY_DR_MIN_REG;
		temp[i++] = __raw_readl(g_g2d_base+SRC_COLORKEY_DR_MIN_REG);
		name[i] = SRC_COLORKEY_DR_MAX_REG;
		temp[i++] = __raw_readl(g_g2d_base+SRC_COLORKEY_DR_MAX_REG);
		name[i] = DST_COLORKEY_CTRL_REG;
		temp[i++] = __raw_readl(g_g2d_base+DST_COLORKEY_CTRL_REG);
		name[i] = DST_COLORKEY_DR_MIN_REG;
		temp[i++] = __raw_readl(g_g2d_base+DST_COLORKEY_DR_MIN_REG);
		name[i] = DST_COLORKEY_DR_MAX_REG;
		temp[i++] = __raw_readl(g_g2d_base+DST_COLORKEY_DR_MAX_REG);

		name[i] = 0xFFFF;

		for (i=0; name[i]!=0xFFFF; i++)
		{
			printk("0x%04x:0x%08X ", name[i], temp[i]);
			if (0 == ((i+1) % 4)) printk("\n");
		}
		printk("\n");
	}
#endif

}


unsigned int sec_g2d_framesize(unsigned int w, unsigned int h, unsigned int colorformat)
{
    unsigned int frame_size = 0;
    unsigned int size       = 0;

    unsigned int stride = get_stride_from_color_space(colorformat);

	switch(stride)
	{
		case 2  :
			size = w * h;
			frame_size = (size << 1);
			break;

		case 4 :
			size = w * h;
			frame_size = (size << 2);
			break;       

		default :
			printk("fimg2d:%s:invalid argu w(%d) h(%d) colorformat(%d)\n",
				__func__, w, h, colorformat);
			break;
	}
    return frame_size;
}

irqreturn_t sec_g2d_irq(int irq, void *dev_id)
{
	#ifdef G2D_DEBUG
		//printk("fimg2d:%s: 0x%x\n", __func__, __raw_readl(g_g2d_base + INTC_PEND_REG));
	#endif

	/*
	if((__raw_readl(g_g2d_base + INTC_PEND_REG) & G2D_INTC_PEND_R_INTP_CMD_FIN) == 0)
	{
		printk("fimg2d:%s: weird interrupt 0x%x\n", __func__, __raw_readl(g_g2d_base + INTC_PEND_REG));
	}
	*/

	// AP team
	__raw_writel(G2D_INTC_PEND_R_INTP_CMD_FIN, g_g2d_base + INTC_PEND_REG);

	g_in_use = 0;
	
	wake_up_interruptible(&g_g2d_waitq);
	
	return IRQ_HANDLED;
}

#ifdef G2D_CLK_GATING
static int sec_g2d_clk_enable(void)
{
	unsigned long spin_flags;
	int ret = -1;

	/*
	#ifdef CONFIG_CPU_FREQ_S5PV210
		s5pv210_set_cpufreq_level(RESTRICT_TABLE);
	#endif // CONFIG_CPU_FREQ_S5PV210
	*/

	#ifdef G2D_USE_SPIN_LOCK_FOR_POWER
		spin_lock_irqsave(&g_g2d_clk_spinlock, spin_flags);
	#endif

	if(g_flag_clk_enable == 0)
	{
		if (s5pv210_pd_enable("g2d_pd") < 0) {
			printk("fimg2d:%s:failed to enable g2d power domain\n", __func__);
			goto sec_g2d_clk_enable_done ;
		}

		// clock gating
		clk_enable(g_g2d_clock);

		g_flag_clk_enable = 1;

		//printk("!!!!!!!!!!!!!! g2d ON\n");
	}

	ret = 0;

sec_g2d_clk_enable_done :

	#ifdef G2D_USE_SPIN_LOCK_FOR_POWER
		spin_unlock_irqrestore(&g_g2d_clk_spinlock, spin_flags);
	#endif

	return 0;
}

static int sec_g2d_clk_disable(int flag_spin_lock)
{
	unsigned long spin_flags;
	int ret = -1;

	#ifdef G2D_USE_SPIN_LOCK_FOR_POWER
	{	
		if(flag_spin_lock == 1)
			spin_lock_irqsave(&g_g2d_clk_spinlock, spin_flags);
	}
	#endif

	if(g_flag_clk_enable == 1)
	{
		if(g_in_use == 0)
		{
			// clock gating
			clk_disable(g_g2d_clock);

			if (s5pv210_pd_disable("g2d_pd") < 0) {
				printk("fimg2d:%s:failed to disable g2d power domain\n", __func__);
				goto sec_g2d_clk_disable_done;
			}

			g_flag_clk_enable = 0;
			//printk("!!!!!!!!!!!!! g2d OFF (jiffies=%u)\n", jiffies);
		}
		else // if(g_in_use == 1)
			mod_timer(&g_g2d_domain_timer, jiffies + HZ);
	}

	ret = 0;

sec_g2d_clk_disable_done :

	#ifdef G2D_USE_SPIN_LOCK_FOR_POWER
	{
		if(flag_spin_lock == 1)
			spin_unlock_irqrestore(&g_g2d_clk_spinlock, spin_flags);
	}
	#endif

	/*
   	#ifdef CONFIG_CPU_FREQ_S5PV210
		s5pv210_set_cpufreq_level(NORMAL_TABLE);
	#endif //CONFIG_CPU_FREQ_S5PV210
	*/

	return 0;
}

static void sec_g2d_domain_timer(void)
{
	sec_g2d_clk_disable(0);
}

#endif // G2D_CLK_GATING

int sec_g2d_open(struct inode *inode, struct file *file)
{

#ifdef G2D_CHECK_HW_VERSION
	if (1 != g_hw_version)
		return 0;
#endif
	
	g_num_of_g2d_object++;
	
	#ifdef G2D_DEBUG
		printk("fimg2d: open ok!\n"); 	
	#endif

	return 0;
}


int sec_g2d_release(struct inode *inode, struct file *file)
{
	#ifdef G2D_CHECK_HW_VERSION
		if (1 != g_hw_version)
			return 0;
	#endif

	g_num_of_g2d_object--;
	
	if(g_num_of_g2d_object == 0)
	{
		g_in_use = 0;
	}
	
	#ifdef G2D_DEBUG
		printk("fimg2d: release ok! \n"); 
	#endif

	return 0;
}


static int sec_g2d_mmap(struct file* filp, struct vm_area_struct *vma) 
{
	unsigned long pageFrameNo=0;
	unsigned long size;

	mutex_lock(&g_g2d_rot_mutex);
	
	size = vma->vm_end - vma->vm_start;

	// page frame number of the address for a source G2D_SFR_SIZE to be stored at. 
	//pageFrameNo = __phys_to_pfn(SEC6400_PA_G2D);
	
	if(g_g2d_reserved_size < size) {
		printk("fimg2d: the size (%ld) mapping is too big!\n", size);
		mutex_unlock(&g_g2d_rot_mutex);
		return -EINVAL;
	}

	// non-cachable..
	//vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); 

	pageFrameNo = __phys_to_pfn(g_g2d_src_phys_addr);
	
	if(remap_pfn_range(vma, vma->vm_start, pageFrameNo, g_g2d_reserved_size, vma->vm_page_prot))
	{
		printk("fimg2d:remap_pfn_range fail\n");

		mutex_unlock(&g_g2d_rot_mutex);
		return -EINVAL;
	}

	mutex_unlock(&g_g2d_rot_mutex);
	
	return 0;
}


static int sec_g2d_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int                 ret = -1;
	g2d_params *        params     = NULL;	
	struct g2d_dma_info dma_info;
	void *              vaddr;

	#ifdef G2D_CHECK_HW_VERSION
	{	
		if (1 != g_hw_version)
			return 0;
	}
	#endif
	
	switch(cmd)
	{
		#ifdef G2D_CHECK_HW_VERSION
		case G2D_GET_VERSION :
		{
			copy_to_user((int *)arg, &g_hw_version, sizeof(int));	
			return 0;
			break;
		}
		#endif
		case G2D_GET_MEMORY :
		{
			copy_to_user((unsigned int *)arg, &g_g2d_reserved_phys_addr, sizeof(unsigned int));	
			return 0;
			break;
		}
		case G2D_GET_MEMORY_SIZE :
		{
			copy_to_user((unsigned int *)arg, &g_g2d_reserved_size, sizeof(unsigned int));
			return 0;
			break;
		}
		case G2D_DMA_CACHE_INVAL :
		{
			copy_from_user(&dma_info, (struct g2d_dma_info *)arg, sizeof(dma_info));
			vaddr = phys_to_virt(dma_info.addr);
			dmac_inv_range(vaddr, vaddr + dma_info.size);
			//dmac_inv_range(vaddr, PAGE_ALIGN((unsigned long)vaddr + dma_info.size));
			//dmac_inv_range(((unsigned long)vaddr)&PAGE_MASK, PAGE_ALIGN((unsigned long)vaddr + dma_info.size));
			return 0;
			break;
		}
		case G2D_DMA_CACHE_CLEAN :
		{
			copy_from_user(&dma_info, (struct g2d_dma_info *)arg, sizeof(dma_info));
			vaddr = phys_to_virt(dma_info.addr);
			dmac_clean_range(vaddr, vaddr + dma_info.size);
			return 0;
			break;
		}
		case G2D_DMA_CACHE_FLUSH :
		{
			copy_from_user(&dma_info, (struct g2d_dma_info *)arg, sizeof(dma_info));
			vaddr = phys_to_virt(dma_info.addr);
			dmac_flush_range(vaddr, vaddr + dma_info.size);
			return 0;
			break;
		}
		case G2D_SET_MEMORY :
		{
			copy_from_user(&dma_info, (struct g2d_dma_info *)arg, sizeof(dma_info));
			vaddr = phys_to_virt(dma_info.addr);
			memset(vaddr, 0x00000000, dma_info.size);
			return 0;
			break;
		}
		default:
			break;
	}

	mutex_lock(&g_g2d_rot_mutex);

	// sw5771.park (101007)
	// when two process or thread run on ioctl..
	// we need to make g_in_use safe...
	if(file->f_flags & O_NONBLOCK)
	{
		if(g_in_use == 1)
		{        
			if(wait_event_interruptible_timeout(g_g2d_waitq, (g_in_use == 0), msecs_to_jiffies(G2D_TIMEOUT)) == 0)
			{
			    printk(KERN_ERR "fimg2d:%s: waiting for interrupt is timeout 0\n", __FUNCTION__);
			}
		}
	}

	g_in_use = 1;

	#ifdef G2D_CHECK_TIME
		sec_g2d_check_time("# start", 0);
	#endif // G2D_CHECK_TIME

	#ifdef G2D_CLK_GATING
		sec_g2d_clk_enable();
	#endif

	#ifdef G2D_CHECK_TIME
		sec_g2d_check_time("# clk on", 0);
	#endif // G2D_CHECK_TIME

	params = (g2d_params*)arg;

	switch(cmd)
	{
		case G2D_BLIT:
		{
			// initialize
			if(sec_g2d_init_regs(params) < 0)
			{
				printk("fimg2d: failed to sec_g2d_init_regs() \n");
				goto sec_g2d_ioctl_done;
			}

			// bitblit
			sec_g2d_rotate_with_bitblt(params);

			#ifdef G2D_CHECK_TIME
				sec_g2d_check_time("# blit", 0);
			#endif // G2D_CHECK_TIME

			break;
		}
		default :
		{
		
			printk("fimg2d: unmatched command (%d) \n", cmd);
			goto sec_g2d_ioctl_done;
			break;
		}
	}
	
	if(!(file->f_flags & O_NONBLOCK))
	{
		if(g_in_use == 1)
		{        
			if(wait_event_interruptible_timeout(g_g2d_waitq, (g_in_use == 0), msecs_to_jiffies(G2D_TIMEOUT)) == 0)
			{
			    printk(KERN_ERR "fimg2d:%s: waiting for interrupt is timeout\n", __FUNCTION__);
			}
			#ifdef G2D_CHECK_TIME
			    sec_g2d_check_time("# wait", 0);
			#endif // G2D_CHECK_TIME

			g_in_use = 0;
		}
	}
	//else
	// user may use polling

	ret = 0;

sec_g2d_ioctl_done :

	if(ret != 0)
	{
		if(params->src_rect)
		{        
			printk(KERN_ERR "src : %d, %d, %d, %d / %d, %d / %d / 0x%x)\n",
				params->src_rect->x,
				params->src_rect->y, 
				params->src_rect->w,
				params->src_rect->h,
				params->src_rect->full_w,
				params->src_rect->full_h,
				params->src_rect->color_format,
				params->src_rect->phys_addr);
		}
		if(params->dst_rect)
		{        
			printk(KERN_ERR "dst : %d, %d, %d, %d / %d, %d / %d / 0x%x)\n",
				params->dst_rect->x,
				params->dst_rect->y, 
				params->dst_rect->w,
				params->dst_rect->h,
				params->dst_rect->full_w,
				params->dst_rect->full_h,
				params->dst_rect->color_format,
				params->dst_rect->phys_addr);
		}

		if(params->flag)
		{        
			    printk(KERN_ERR "alpha_value : %d \n",
				params->flag->alpha_val);
		}

		//sec_g2d_print_param_n_cmd(params, 0);
	}

	#ifdef G2D_CLK_GATING
	{
		sec_g2d_clk_disable(1);
	}
	#endif

	#ifdef G2D_CHECK_TIME
		sec_g2d_check_time("# clk off", 1);
	#endif // G2D_CHECK_TIME

	mutex_unlock(&g_g2d_rot_mutex);

	return ret;
}

static unsigned int sec_g2d_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	#ifdef G2D_CHECK_HW_VERSION
		if (1 != g_hw_version)
			return 0;
	#endif

	if (g_in_use == 0)
	{	
		mask = POLLOUT | POLLWRNORM;
	}
	else
	{
		poll_wait(file, &g_g2d_waitq, wait);

		if(g_in_use == 0)
		{
			mask = POLLOUT | POLLWRNORM;
			
		}
	}

	return mask;
}

static struct file_operations sec_g2d_fops = {
	.owner 		= THIS_MODULE,
	.open 		= sec_g2d_open,
	.release 	= sec_g2d_release,
	.mmap 		= sec_g2d_mmap,
	.ioctl		= sec_g2d_ioctl,
	.poll		= sec_g2d_poll,
};


static struct miscdevice sec_g2d_dev = {
	.minor		= G2D_MINOR,
	.name		= "sec-g2d",
	.fops		= &sec_g2d_fops,
};

int sec_g2d_probe(struct platform_device *pdev)
{

	struct resource *res;
	int ret;

	#ifdef G2D_DEBUG
		printk(KERN_ALERT"fimg2d: start probe : name=%s num=%d res[0].start=0x%x res[1].start=0x%x\n",
		       pdev->name, pdev->num_resources, pdev->resource[0].start, pdev->resource[1].start);
	#endif

	#ifdef G2D_CHECK_HW_VERSION
	{
		g_hw_version = hw_version_check();
		if (1 != g_hw_version)
		{
			printk("fimg2d:%s: dummy probe - s5pc110 evt%d has no fimg2d\n", __func__, g_hw_version);
			return 0;
		}
	}
	#endif

	// get the memory region
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(res == NULL) 
	{
		printk(KERN_ERR "fimg2d: failed to get memory region resouce\n");
		return -ENOENT;
	}

	g_g2d_mem = request_mem_region(res->start, res->end - res->start + 1, pdev->name);
	if(g_g2d_mem == NULL) {
		printk(KERN_ERR "fimg2d: failed to reserve memory region\n");
                return -ENOENT;
	}
	
	// ioremap
	g_g2d_base = ioremap(g_g2d_mem->start, g_g2d_mem->end - res->start + 1);
	if(g_g2d_base == NULL) {
		printk(KERN_ERR "fimg2d: failed ioremap\n");
                return -ENOENT;
	}

	// irq
	g_g2d_irq_num = platform_get_irq(pdev, 0);
	if(g_g2d_irq_num <= 0) {
		printk(KERN_ERR "fimg2d: failed to get irq resouce\n");
                return -ENOENT;
	}

	#ifdef G2D_DEBUG
		printk("fimg2d:%s: g_g2d_irq_num=%d\n", __func__, g_g2d_irq_num);
	#endif

	ret = request_irq(g_g2d_irq_num, sec_g2d_irq, IRQF_DISABLED, pdev->name, NULL);
	if (ret) {
		printk("fimg2d: request_irq(g2d) failed.\n");
		return ret;
	}

	// clock for gating
	#ifdef G2D_CLK_GATING
	{
		/*
		g_g2d_clock = clk_get(&pdev->dev, "g2d");
		if(g_g2d_clock == NULL) {
				printk(KERN_ERR "fimg2d: failed to find g2d clock source\n");
				return -ENOENT;
		}
		*/		
		
		struct clk *parent, *sclk;

		parent = clk_get(&pdev->dev, "mout_mpll");
		if (IS_ERR(parent)) {
			printk(KERN_ERR "fimg2d: failed to get parent clock\n");
			return -ENOENT;
		}

		sclk = clk_get(&pdev->dev, "sclk_g2d");
		if (IS_ERR(sclk)) {
			printk(KERN_ERR "fimg2d: failed to get sclk_g2d clock\n");
			return -ENOENT;
		}

		clk_set_parent(sclk, parent);
		//clk_set_rate(sclk, 250 * MHZ);
		clk_set_rate(sclk, 222 * MHZ);

		// clock for gating
		g_g2d_clock = clk_get(&pdev->dev, "g2d");
		if (IS_ERR(g_g2d_clock)) {
			printk(KERN_ERR "fimg2d: failed to get clock clock\n");
			return -ENOENT;
		}
	}
	#endif

	// blocking I/O
	init_waitqueue_head(&g_g2d_waitq);

	// atomic init
	g_in_use = 0;

	// misc register
	ret = misc_register(&sec_g2d_dev);
	if (ret) {
		printk (KERN_ERR "fimg2d: cannot register miscdev on minor=%d (%d)\n",
			G2D_MINOR, ret);
		return ret;
	}

	g_g2d_reserved_phys_addr = s3c_get_media_memory_bank(S3C_MDEV_G2D, 1);
	if (g_g2d_reserved_phys_addr == 0)
	{
		printk(KERN_ERR "fimg2d: failed to s3c_get_media_memory_bank !!! \n");
		return -ENOENT;
	}

	g_g2d_reserved_size = s3c_get_media_memsize_bank(S3C_MDEV_G2D, 1);

	g_g2d_src_phys_addr = g_g2d_reserved_phys_addr;
	g_g2d_src_virt_addr = (u32)phys_to_virt(g_g2d_src_phys_addr);
	g_g2d_src_size      = PAGE_ALIGN(g_g2d_reserved_size >> 1);

	g_g2d_dst_phys_addr = g_g2d_src_phys_addr + g_g2d_src_size;
	g_g2d_dst_virt_addr = g_g2d_src_virt_addr + g_g2d_src_size;
	g_g2d_dst_size      = PAGE_ALIGN(g_g2d_reserved_size - g_g2d_src_size);

	#ifdef G2D_CLK_GATING
	{
		// init domain timer
		init_timer(&g_g2d_domain_timer);
		g_g2d_domain_timer.function = sec_g2d_domain_timer;
	}
	#endif

	#ifdef G2D_DEBUG
		printk(KERN_ALERT"fimg2d: sec_g2d_probe ok!\n");
	#endif

	return 0;  
}

static int sec_g2d_remove(struct platform_device *dev)
{
	#ifdef G2D_DEBUG
		printk(KERN_INFO "fimg2d: sec_g2d_remove called !\n");
	#endif

	#ifdef G2D_CLK_GATING
		del_timer(&g_g2d_domain_timer);
	#endif

	free_irq(g_g2d_irq_num, NULL);
	
	if (g_g2d_mem != NULL) {   
		printk(KERN_INFO "fimg2d: releasing resource\n");
		iounmap(g_g2d_base);
		release_resource(g_g2d_mem);
		kfree(g_g2d_mem);
	}
	
	misc_deregister(&sec_g2d_dev);

	g_in_use = 0;

	#ifdef G2D_CLK_GATING
	{
		sec_g2d_clk_disable(1);

		if (g_g2d_clock)
		{
			clk_put(g_g2d_clock);
			g_g2d_clock = NULL;
		}
	}
	#endif

	#ifdef G2D_DEBUG
		printk(KERN_INFO "fimg2d: sec_g2d_remove ok!\n");
	#endif

	return 0;
}

static int sec_g2d_suspend(struct platform_device *dev, pm_message_t state)
{
	//clk_disable(g_g2d_clock);
	return 0;
}
static int sec_g2d_resume(struct platform_device *pdev)
{
	//clk_enable(g_g2d_clock);
	return 0;
}


static struct platform_driver sec_g2d_driver = {
       .probe          = sec_g2d_probe,
       .remove         = sec_g2d_remove,
       .suspend        = sec_g2d_suspend,
       .resume         = sec_g2d_resume,
       .driver		= {
		.owner	= THIS_MODULE,
		.name	= "s5p-g2d",
	},
};

int __init  sec_g2d_init(void)
{
 	if(platform_driver_register(&sec_g2d_driver)!=0)
  	{
   		printk("fimg2d: platform device register Failed \n");
   		return -1;
  	}

	#ifdef G2D_DEBUG
		printk("fimg2d: init ok!\n");
	#endif

	return 0;
}

void  sec_g2d_exit(void)
{
	platform_driver_unregister(&sec_g2d_driver);

	#ifdef G2D_DEBUG
 		printk("fimg2d: exit ok!\n");
	#endif
}

module_init(sec_g2d_init);
module_exit(sec_g2d_exit);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("SEC G2D Device Driver");
MODULE_LICENSE("GPL");
