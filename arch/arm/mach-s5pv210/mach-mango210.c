/* linux/arch/arm/mach-s5pv210/mach-smdkv210.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>
#include <linux/pwm_backlight.h>
#include <linux/spi/spi.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-mem.h>
#include <mach/regs-gpio.h>
#include <mach/gpio-bank.h>
#include <mach/ts.h>
#include <mach/adc.h>

#include <media/noon130pc20_platform.h>

#include <plat/regs-serial.h>
#include <plat/s5pv210.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/fb.h>
#include <plat/gpio-cfg.h>
#include <plat/iic.h>
#include <plat/spi.h>
#include <plat/fimc.h>
#include <plat/csis.h>
#include <plat/mfc.h>
#include <plat/sdhci.h>
#include <plat/regs-otg.h>
#include <plat/clock.h>
#include <mach/gpio-bank-b.h>
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#include <plat/media.h>
#endif

#if defined(CONFIG_PM)
#include <plat/pm.h>
#endif

#include <linux/mango_keys.h>
#include <linux/input.h>

#include <linux/i2c-gpio.h>

#include <linux/vd5376.h>

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define S5PV210_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define S5PV210_ULCON_DEFAULT	S3C2410_LCON_CS8

#define S5PV210_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

extern void s5pv210_reserve_bootmem(void);
extern void s3c_sdhci_set_platdata(void);

static struct s3c2410_uartcfg mango210_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
};


#ifdef CONFIG_TOUCHSCREEN_S3C
static struct s3c_ts_mach_info s3c_ts_platform __initdata = {
	.delay                  = 50000,
	.presc                  = 49,
	.oversampling_shift     = 2,
	.resol_bit              = 12,
	.s3c_adc_con            = ADC_TYPE_2,
};
#endif

#ifdef CONFIG_S5PV210_ADC
static struct s3c_adc_mach_info s3c_adc_platform __initdata = {
	/* s5pc100 supports 12-bit resolution */
	.delay  = 10000,
	.presc  = 49,
	.resolution = 12,
};
#endif

#ifdef CONFIG_KEYBOARD_MANGO
static struct mango_keys_button mango_gpio_keys_table[] = {
	{
		.code			= KEY_BACK,
		.gpio			= S5PV210_GPH0(1),
		.active_low		= 1,
		.desc			= "GPH0",
		.type			= EV_KEY,
		.wakeup			= 1,
//		.debounce_interval	= 5,
		.irq			= IRQ_EINT1,
		.config			= (0xf << 4),
		.pull			= S3C_GPIO_PULL_NONE,
		.active_low		= 1,
		.led_gpio		= S5PV210_GPJ3(7),
		.led_desc		= "GPJ3",
		.led_config		= (0x1 << (4 * 7)),
		.led_active_low		= 1,
	},
	{
		.code			= KEY_MENU,
		.gpio			= S5PV210_GPH0(2),
		.active_low		= 1,
		.desc			= "GPH0",
		.type			= EV_KEY,
		.wakeup			= 1,
//		.debounce_interval	= 5,
		.irq			= IRQ_EINT2,
		.config			= (0xf<<8),
		.pull			= S3C_GPIO_PULL_NONE,
		.active_low		= 1,
		.led_gpio		= S5PV210_GPJ4(0),
		.led_desc		= "GPJ4",
		.led_config		= (0x1 << (4 * 0)),
		.led_active_low		= 1,
	},
};

static struct mango_keys_platform_data mango_gpio_keys_data = {
	.buttons	= mango_gpio_keys_table,
	.nbuttons	= ARRAY_SIZE(mango_gpio_keys_table),
	.rep		= 0,
};

static struct platform_device mango_device_gpiokeys = {
	.name	= "mango-keys",
	.dev	= {
		.platform_data	= &mango_gpio_keys_data,
	},
};
#endif

#ifdef CONFIG_BATTERY_MANGO_DUMMY
static struct platform_device mango_battery = {
	.name = "dummy-battery",
};
#endif

#ifdef CONFIG_CRZBOYS_MEM_DBG
static struct platform_device memory_debugger = {
	.name = "memory-debugger",
};
#endif

#ifdef CONFIG_MOUSE_VD5376_FINGER
static struct vd5376_platform_data vd5376_table[] = {
	{
		.btn_gpio = S5PV210_GPH0(3),
		.btn_desc = "GPH0",
		.btn_irq = IRQ_EINT3,
		.btn_config = (0xf << 12),
		.btn_active_low = 1,
		.pd_gpio = S5PV210_GPH0(5),
		.pd_desc = "GPH0",
		.gpio = S5PV210_GPH0(4),
		.desc = "GPH0",
		.irq = IRQ_EINT4,
		.config = (0xf << 16),
		.active_low = 1,
		.delay = 20,
	},
	{
		.btn_gpio = S5PV210_GPH2(3),
		.btn_desc = "GPH2",
		.btn_irq = IRQ_EINT(19),
		.btn_config = (0xf << 12),
		.btn_active_low = 1,
		.pd_gpio = S5PV210_GPH2(4),
		.pd_desc = "GPH2",
		.gpio = S5PV210_GPH2(5),
		.desc = "GPH2",
		.irq = IRQ_EINT(21),
		.config = (0xf << 20),
		.active_low = 1,
		.delay = 20,
	},
};

struct vd5376_mouse_platform_data vd5376_mouse_data = {
	.data = vd5376_table,
	.ndatas	= ARRAY_SIZE(vd5376_table),
	.scan_ms = 50,
};

static struct platform_device vd5376_mouse_device = {
	.name	= "vd5376-mouse",
	.dev	= {
		.platform_data	= &vd5376_mouse_data,
	},
};
#endif

#ifdef CONFIG_I2C_GPIO
static struct i2c_gpio_platform_data i2c_gpio_platdata = {
	.sda_pin = S5PV210_GPD1(0),
	.scl_pin = S5PV210_GPD1(1),
	.udelay = 10,
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0
};

static struct platform_device mango_i2c0_gpio = {
	.name	= "i2c-gpio",
	.id	= 0,
	.dev.platform_data = &i2c_gpio_platdata,
};

static struct i2c_gpio_platform_data i2c2_gpio_platdata = {
	.sda_pin = S5PV210_GPD1(4),
	.scl_pin = S5PV210_GPD1(5),
	.udelay = 10,
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0
};

static struct platform_device mango_i2c2_gpio = {
	.name	= "i2c-gpio",
	.id	= 2,
	.dev.platform_data = &i2c2_gpio_platdata,
};
#endif

#ifdef CONFIG_VIDEO_FIMC
/*
 * External camera reset
 * Because the most of cameras take i2c bus signal, so that
 * you have to reset at the boot time for other i2c slave devices.
 * This function also called at fimc_init_camera()
 * Do optimization for cameras on your platform.
*/
static int mango210_cam0_power(int onoff)
{
	int err;

	/* Camera A */
	err = gpio_request(S5PV210_GPJ4(2), "GPJ4");
	if (err)
		printk(KERN_ERR "#### failed to request GPJ4 for CAM_2V8\n");

	s3c_gpio_setpull(S5PV210_GPJ4(2), S3C_GPIO_PULL_NONE);
	gpio_direction_output(S5PV210_GPJ4(2), 0);
	gpio_direction_output(S5PV210_GPJ4(2), 1);
	gpio_free(S5PV210_GPJ4(2));

	return 0;
}

/* External camera module setting */
/* 2 ITU Cameras */
#if defined(CONFIG_VIDEO_NOON130PC20)
static struct noon130pc20_platform_data noon130pc20_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_YUYV,
	.freq = 48000000,
	.is_mipi = 0,
};

static struct i2c_board_info  noon130pc20_i2c_info = {
	I2C_BOARD_INFO("NOON130PC20", 0x20),
	.platform_data = &noon130pc20_plat,
};

static struct s3c_platform_camera noon130pc20 = {
	.id		= CAMERA_PAR_A,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.i2c_busnum	= 0,
	.info		= &noon130pc20_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_YUYV,
	.srclk_name	= "mout_epll",
	.clk_name	= "sclk_cam0",
	.clk_rate	= 48000000,
	.line_length	= 640,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= mango210_cam0_power,
};
#endif

/* Interface setting */
static struct s3c_platform_fimc fimc_plat = {
	.srclk_name	= "mout_mpll",
	.clk_name	= "fimc",
	.clk_rate	= 166000000,

	.default_cam	= CAMERA_PAR_A,
	.camera		= {
#if defined(CONFIG_VIDEO_NOON130PC20)
		&noon130pc20,
#endif
	},
	.hw_ver		= 0x43,
};
#endif

#if defined(CONFIG_SPI_CNTRLR_0) || defined(CONFIG_SPI_CNTRLR_1) || defined(CONFIG_SPI_CNTRLR_2)
static void s3c_cs_suspend(int pin, pm_message_t pm)
{
        /* Whatever need to be done */
}

static void s3c_cs_resume(int pin)
{
        /* Whatever need to be done */
}

static void s3c_cs_set(int pin, int lvl)
{
        if(lvl == CS_HIGH)
           s3c_gpio_setpin(pin, 1);
        else
           s3c_gpio_setpin(pin, 0);
}
static void s3c_cs_config(int pin, int mode, int pull)
{
        s3c_gpio_cfgpin(pin, mode);

        if(pull == CS_HIGH){
           s3c_gpio_setpull(pin, S3C_GPIO_PULL_UP);
		   s3c_gpio_setpin(pin, 0);
		}
        else{
           s3c_gpio_setpull(pin, S3C_GPIO_PULL_DOWN);
		   s3c_gpio_setpin(pin, 1);
		}
}
#endif

#if defined(CONFIG_SPI_CNTRLR_0)
static struct s3c_spi_pdata s3c_slv_pdata_0[] __initdata = {
        [0] = { /* Slave-0 */
                .cs_level     = CS_FLOAT,
                .cs_pin       = S5PV210_GPB(1),
                .cs_mode      = S5PV210_GPB_OUTPUT(1),
                .cs_set       = s3c_cs_set,
                .cs_config    = s3c_cs_config,
                .cs_suspend   = s3c_cs_suspend,
                .cs_resume    = s3c_cs_resume,
        },
        #if 0
        [1] = { /* Slave-1 */
                .cs_level     = CS_FLOAT,
                .cs_pin       = S5PV210_GPA1(1),
                .cs_mode      = S5PV210_GPA1_OUTPUT(1),
                .cs_set       = s3c_cs_set,
                .cs_config    = s3c_cs_config,
                .cs_suspend   = s3c_cs_suspend,
                .cs_resume    = s3c_cs_resume,
        },
        #endif
};
#endif

#if defined(CONFIG_SPI_CNTRLR_1)
static struct s3c_spi_pdata s3c_slv_pdata_1[] __initdata = {
        [0] = { /* Slave-0 */
                .cs_level     = CS_FLOAT,
                .cs_pin       = S5PV210_GPB(5),
                .cs_mode      = S5PV210_GPB_OUTPUT(5),
                .cs_set       = s3c_cs_set,
                .cs_config    = s3c_cs_config,
                .cs_suspend   = s3c_cs_suspend,
                .cs_resume    = s3c_cs_resume,
        },
		#if 0
        [1] = { /* Slave-1 */
                .cs_level     = CS_FLOAT,
                .cs_pin       = S5PV210_GPA1(3),
                .cs_mode      = S5PV210_GPA1_OUTPUT(3),
                .cs_set       = s3c_cs_set,
                .cs_config    = s3c_cs_config,
                .cs_suspend   = s3c_cs_suspend,
                .cs_resume    = s3c_cs_resume,
        },
		#endif
};
#endif

static struct spi_board_info s3c_spi_devs[] __initdata = {
#if defined(CONFIG_SPI_CNTRLR_0)
        [0] = {
                .modalias        = "spidev", /* Test Interface */
                .mode            = SPI_MODE_0,  /* CPOL=0, CPHA=0 */
                .max_speed_hz    = 100000,
                /* Connected to SPI-0 as 1st Slave */
                .bus_num         = 0,
                .irq             = IRQ_SPI0,
                .chip_select     = 0,
        },
		#if 0
        [1] = {
                .modalias        = "spidev", /* Test Interface */
                .mode            = SPI_MODE_0,  /* CPOL=0, CPHA=0 */
                .max_speed_hz    = 100000,
                /* Connected to SPI-0 as 2nd Slave */
                .bus_num         = 0,
                .irq             = IRQ_SPI0,
                .chip_select     = 1,
        },
		#endif
#endif

#if defined(CONFIG_SPI_CNTRLR_1)
        [1] = {
                .modalias        = "spidev", /* Test Interface */
                .mode            = SPI_MODE_0,  /* CPOL=0, CPHA=0 */
                .max_speed_hz    = 100000,
                /* Connected to SPI-1 as 1st Slave */
                .bus_num         = 1,
                .irq             = IRQ_SPI1,
                .chip_select     = 0,
        },
		#if 0
        [3] = {
                .modalias        = "spidev", /* Test Interface */
                .mode            = SPI_MODE_0 | SPI_CS_HIGH,    /* CPOL=0, CPHA=0 & CS is Active High */
                .max_speed_hz    = 100000,
                /* Connected to SPI-1 as 3rd Slave */
                .bus_num         = 1,
                .irq             = IRQ_SPI1,
                .chip_select     = 1,
        },
		#endif
#endif
};


#if defined(CONFIG_HAVE_PWM)
static struct platform_pwm_backlight_data mango_backlight_data = {
	.pwm_id  = 0,
	.max_brightness = 255,
	.dft_brightness = 255,
	.pwm_period_ns  = 78770,
};

static struct platform_device mango_backlight_device = {
	.name      = "pwm-backlight",
	.id        = -1,
	.dev        = {
		.parent = &s3c_device_timer[0].dev,
		.platform_data = &mango_backlight_data,
	},
};
static void __init mango_backlight_register(void)
{
	int ret = platform_device_register(&mango_backlight_device);
	if (ret)
		printk(KERN_ERR "mango: failed to register backlight device: %d\n", ret);
}
#endif

/* I2C0 */
static struct i2c_board_info i2c_devs0[] __initdata = {
#ifdef CONFIG_SND_SOC_WM8960
	{
		I2C_BOARD_INFO("wm8960", 0x1a),
	},
#endif
#ifdef CONFIG_MOUSE_VD5376_FINGER
	{
		I2C_BOARD_INFO("vd5376", 0x53),
		.platform_data = &vd5376_table[0],
	},
#endif
};

/* I2C1 */
static struct i2c_board_info i2c_devs1[] __initdata = {
#ifdef CONFIG_VIDEO_TV20
	{
		I2C_BOARD_INFO("s5p_ddc", (0x74>>1)),
	},
#endif
};
static struct i2c_board_info i2c_devs2[] __initdata = {
#ifdef CONFIG_MOUSE_VD5376_FINGER
	{
		I2C_BOARD_INFO("vd5376", 0x53),
		.platform_data = &vd5376_table[1],
	},
#endif
};


#ifdef CONFIG_SMSC911X
static void __init mango210_smsc911x_set(void)
{
	unsigned int tmp;

	tmp = __raw_readl(S5PV210_MP01CON);
	tmp &= ~(0xf << 4);
	tmp |= (2 << 4);
	__raw_writel(tmp, S5PV210_MP01CON);

	tmp = __raw_readl(S5P_SROM_BW);
	tmp &= ~(0xf << 4);
	tmp |= ((1<<4) | (1<<5));
	__raw_writel(tmp, S5P_SROM_BW);

	tmp = ((0<<28)|(4<<24)|(13<<16)|(1<<12)|(4<<8)|(6<<4)|(0<<0));
	__raw_writel(tmp, (S5P_SROM_BC1));
}
#endif

#if defined(CONFIG_VIDEO_NOON130PC20)
static void __init mango210_camera_set(void)
{
	int err;

	/* Camera A */
	err = gpio_request(S5PV210_GPJ4(1), "GPJ4");
	if (err)
		printk(KERN_ERR "#### failed to request GPJ4 for CAM_2V8\n");

	s3c_gpio_setpull(S5PV210_GPJ4(1), S3C_GPIO_PULL_NONE);
	gpio_direction_output(S5PV210_GPJ4(1), 1);
	gpio_free(S5PV210_GPJ4(1));

}
#endif

#ifdef CONFIG_USB_SUPPORT
static void __init mango210_usb_host_set(void)
{
	int err;

	err = gpio_request(S5PV210_ETC2(7), "ETC2");
	if (err)
		printk(KERN_ERR "#### failed to request ETC2 for USB host\n");

	s3c_gpio_setpull(S5PV210_ETC2(7), S3C_GPIO_PULL_DOWN);
	gpio_free(S5PV210_ETC2(7));
}
#endif

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data pmem_pdata = {
	.name = "pmem",
	.no_allocator = 1,
	.cached = 1,
	.start = 0, // will be set during proving pmem driver.
	.size = 0 // will be set during proving pmem driver.
};

static struct android_pmem_platform_data pmem_gpu1_pdata = {
   .name = "pmem_gpu1",
   .no_allocator = 1,
   .cached = 1,
   .buffered = 1,
   .start = 0,
   .size = 0,
};

static struct android_pmem_platform_data pmem_adsp_pdata = {
   .name = "pmem_adsp",
   .no_allocator = 1,
   .cached = 1,
   .buffered = 1,
   .start = 0,
   .size = 0,
};

static struct platform_device pmem_device = {
   .name = "android_pmem",
   .id = 0,
   .dev = { .platform_data = &pmem_pdata },
};

static struct platform_device pmem_gpu1_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &pmem_gpu1_pdata },
};

static struct platform_device pmem_adsp_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &pmem_adsp_pdata },
};

static void __init android_pmem_set_platdata(void)
{
	pmem_pdata.start = (u32)s3c_get_media_memory_bank(S3C_MDEV_PMEM, 1);
	pmem_pdata.size = (u32)s3c_get_media_memsize_bank(S3C_MDEV_PMEM, 1);

	pmem_gpu1_pdata.start = (u32)s3c_get_media_memory_bank(S3C_MDEV_PMEM_GPU1, 1);
	pmem_gpu1_pdata.size = (u32)s3c_get_media_memsize_bank(S3C_MDEV_PMEM_GPU1, 1);

	pmem_adsp_pdata.start = (u32)s3c_get_media_memory_bank(S3C_MDEV_PMEM_ADSP, 1);
	pmem_adsp_pdata.size = (u32)s3c_get_media_memsize_bank(S3C_MDEV_PMEM_ADSP, 1);
}
#endif

static struct platform_device *mango210_devices[] __initdata = {
#ifdef CONFIG_MTD_NAND
	&s3c_device_nand,
#endif
#ifdef CONFIG_FB_S3C
	&s3c_device_fb,
#endif
#ifdef CONFIG_TOUCHSCREEN_S3C
	&s3c_device_ts,
#endif
#ifdef CONFIG_S5PV210_ADC
	&s3c_device_adc,
#endif
#ifdef CONFIG_S3C2410_WATCHDOG
	&s3c_device_wdt,
#endif
#ifdef CONFIG_RTC_DRV_S3C
	&s5p_device_rtc,
#endif
#ifdef CONFIG_HAVE_PWM
	&s3c_device_timer[0],
	&s3c_device_timer[1],
	&s3c_device_timer[2],
	&s3c_device_timer[3],
#endif
#ifdef CONFIG_SND_S3C24XX_SOC
	&s3c64xx_device_iis0,
#endif
#ifdef CONFIG_SND_S3C_SOC_PCM
	&s3c64xx_device_pcm1,
#endif
#ifdef CONFIG_SND_S5P_SOC_SPDIF
	&s5p_device_spdif,
#endif
#ifdef CONFIG_VIDEO_FIMC
#ifdef CONFIG_S5PV210_SETUP_FIMC0
	&s3c_device_fimc0,	// Main Camera
#endif
#ifdef CONFIG_S5PV210_SETUP_FIMC1
	&s3c_device_fimc1,	// MFC, Cam Hdmi Output
#endif
#ifdef CONFIG_S5PV210_SETUP_FIMC2
	&s3c_device_fimc2,	// Layerout Hdmit Output
#endif
	&s3c_device_ipc,
#endif
#ifdef CONFIG_VIDEO_MFC50
	&s3c_device_mfc,
#endif

#ifdef CONFIG_VIDEO_JPEG_V2
	&s3c_device_jpeg,
#endif

#ifdef CONFIG_VIDEO_ROTATOR
	&s5p_device_rotator,
#endif

#ifdef CONFIG_MOUSE_VD5376_FINGER
	&vd5376_mouse_device,
#endif

#ifdef CONFIG_I2C_GPIO
	&mango_i2c0_gpio,
#else
	&s3c_device_i2c0,
#endif
	&s3c_device_i2c1,
#ifdef CONFIG_I2C_GPIO
	&mango_i2c2_gpio,
#else
	&s3c_device_i2c2,
#endif

#ifdef CONFIG_USB
	&s3c_device_usb_ehci,
	&s3c_device_usb_ohci,
#endif
#ifdef CONFIG_USB_GADGET
	&s3c_device_usbgadget,
#endif
#ifdef CONFIG_USB_ANDROID
	&s3c_device_android_usb,
	&s3c_device_usb_mass_storage,
#endif

#ifdef CONFIG_S3C_DEV_HSMMC3
	&s3c_device_hsmmc3,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	&s3c_device_hsmmc1,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	&s3c_device_hsmmc2,
#endif

#ifdef CONFIG_SPI_CNTRLR_0
        &s3c_device_spi0,
#endif
#ifdef CONFIG_SPI_CNTRLR_1
        &s3c_device_spi1,
#endif

#ifdef CONFIG_VIDEO_TV20
	&s5p_device_tvout,
	&s5p_device_cec,
	&s5p_device_hpd,
#endif

#ifdef CONFIG_ANDROID_PMEM
	&pmem_device,
	&pmem_gpu1_device,
	&pmem_adsp_device,
#endif
#ifdef CONFIG_VIDEO_G2D
	&s5p_device_g2d,
#endif
#ifdef CONFIG_SMSC911X
	&s5p_device_smsc911x,
#endif
#ifdef CONFIG_KEYBOARD_MANGO
	&mango_device_gpiokeys,
#endif
#ifdef CONFIG_BATTERY_MANGO_DUMMY
	&mango_battery,
#endif
#ifdef CONFIG_CRZBOYS_MEM_DBG
	&memory_debugger,
#endif
};

static void __init mango210_map_io(void)
{
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(mango210_uartcfgs, ARRAY_SIZE(mango210_uartcfgs));
	s5pv210_reserve_bootmem();
#ifdef CONFIG_MTD_NAND
	s3c_device_nand.name = "s5pv210-nand";
#endif
}

#ifdef CONFIG_S3C_SAMSUNG_PMEM
static void __init s3c_pmem_set_platdata(void)
{
	pmem_pdata.start = s3c_get_media_memory_bank(S3C_MDEV_PMEM, 1);
	pmem_pdata.size = s3c_get_media_memsize_bank(S3C_MDEV_PMEM, 1);
}
#endif

#if defined(CONFIG_FB_S3C_LB070WV6) || defined(CONFIG_FB_S3C_LTN101NT05)
static struct s3c_platform_fb mango_fb_data __initdata = {
	.hw_ver	= 0x62,
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,
};
#endif
/* this function are used to detect s5pc110 chip version temporally */

int s5pc110_version ;

void _hw_version_check(void)
{
	void __iomem * phy_address ;
	int temp; 

	phy_address = ioremap (0x40,1);

	temp = __raw_readl(phy_address);


	if (temp == 0xE59F010C)
	{
		s5pc110_version = 0;
	}
	else
	{
		s5pc110_version = 1;
	}
	printk("S5PC110 Hardware version : EVT%d \n",s5pc110_version);
	
	iounmap(phy_address);
}

/* Temporally used
 * return value 0 -> EVT 0
 * value 1 -> evt 1
 */

int hw_version_check(void)
{
	return s5pc110_version ;
}
EXPORT_SYMBOL(hw_version_check);

static void mango210_power_off(void)
{
	/* PS_HOLD --> Output Low */
//	printk(KERN_EMERG "%s : setting GPIO_PDA_PS_HOLD low.\n", __func__);
	/* PS_HOLD output High --> Low  PS_HOLD_CONTROL, R/W, 0xE010_E81C */
//	writel(readl(S5P_PSHOLD_CONTROL) & 0xFFFFFEFF, S5P_PSHOLD_CONTROL);

	while(1);

	printk(KERN_EMERG "%s : should not reach here!\n", __func__);
}

#ifdef CONFIG_VIDEO_TV20
void s3c_set_qos()
{
	/* VP QoS */
	__raw_writel(0x00400001, S5P_VA_DMC0 + 0xC8);
	__raw_writel(0x387F0022, S5P_VA_DMC0 + 0xCC);
	/* MIXER QoS */
	__raw_writel(0x00400001, S5P_VA_DMC0 + 0xD0);
	__raw_writel(0x3FFF0062, S5P_VA_DMC0 + 0xD4);
	/* LCD1 QoS */
	__raw_writel(0x00800001, S5P_VA_DMC1 + 0x90);
	__raw_writel(0x3FFF005B, S5P_VA_DMC1 + 0x94);
	/* LCD2 QoS */
	__raw_writel(0x00800001, S5P_VA_DMC1 + 0x98);
	__raw_writel(0x3FFF015B, S5P_VA_DMC1 + 0x9C);
	/* VP QoS */
	__raw_writel(0x00400001, S5P_VA_DMC1 + 0xC8);
	__raw_writel(0x387F002B, S5P_VA_DMC1 + 0xCC);
	/* DRAM Controller QoS */
	__raw_writel((__raw_readl(S5P_VA_DMC0)&~(0xFFF<<16)|(0x100<<16)),
			S5P_VA_DMC0 + 0x0);
	__raw_writel((__raw_readl(S5P_VA_DMC1)&~(0xFFF<<16)|(0x100<<16)),
			S5P_VA_DMC1 + 0x0);
	/* BUS QoS AXI_DSYS Control */
	__raw_writel(0x00000007, S5P_VA_BUS_AXI_DSYS + 0x400);
	__raw_writel(0x00000007, S5P_VA_BUS_AXI_DSYS + 0x420);
	__raw_writel(0x00000030, S5P_VA_BUS_AXI_DSYS + 0x404);
	__raw_writel(0x00000030, S5P_VA_BUS_AXI_DSYS + 0x424);
}
#endif

static void __init mango210_machine_init(void)
{
	/* Find out S5PC110 chip version */
	_hw_version_check();

#ifdef CONFIG_MTD_NAND
//	s3c_device_nand.dev.platform_data = &s5p_nand_data;
#endif

#ifdef CONFIG_SMSC911X
	mango210_smsc911x_set();
#endif

#ifdef CONFIG_ANDROID_PMEM
	android_pmem_set_platdata();
#endif
	/* i2c */
	s3c_i2c0_set_platdata(NULL);
	s3c_i2c1_set_platdata(NULL);
	s3c_i2c2_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));

	/* to support system shut down */
	pm_power_off = mango210_power_off;

#if defined(CONFIG_SPI_CNTRLR_0)
        s3cspi_set_slaves(BUSNUM(0), ARRAY_SIZE(s3c_slv_pdata_0), s3c_slv_pdata_0);
#endif
#if defined(CONFIG_SPI_CNTRLR_1)
        s3cspi_set_slaves(BUSNUM(1), ARRAY_SIZE(s3c_slv_pdata_1), s3c_slv_pdata_1);
#endif
#if defined(CONFIG_SPI_CNTRLR_2)
        s3cspi_set_slaves(BUSNUM(2), ARRAY_SIZE(s3c_slv_pdata_2), s3c_slv_pdata_2);
#endif
        spi_register_board_info(s3c_spi_devs, ARRAY_SIZE(s3c_spi_devs));

#if defined(CONFIG_FB_S3C_LB070WV6) || defined(CONFIG_FB_S3C_LTN101NT05)
	s3cfb_set_platdata(&mango_fb_data);
#endif

#if defined(CONFIG_TOUCHSCREEN_S3C)
	s3c_ts_set_platdata(&s3c_ts_platform);
#endif

#if defined(CONFIG_S5PV210_ADC)
	s3c_adc_set_platdata(&s3c_adc_platform);
#endif

#if defined(CONFIG_PM)
	s3c_pm_init();
#endif

#ifdef CONFIG_VIDEO_FIMC
#ifdef CONFIG_S5PV210_SETUP_FIMC0
	s3c_fimc0_set_platdata(&fimc_plat);
#endif
#ifdef CONFIG_S5PV210_SETUP_FIMC1
	s3c_fimc1_set_platdata(&fimc_plat);
#endif
#ifdef CONFIG_S5PV210_SETUP_FIMC2
	s3c_fimc2_set_platdata(&fimc_plat);
#endif

	mango210_camera_set();
	mango210_cam0_power(1);
#else
#ifdef CONFIG_S5PV210_SETUP_FIMC0
	s3c_fimc0_set_platdata(NULL);
#endif
#ifdef CONFIG_S5PV210_SETUP_FIMC1
	s3c_fimc1_set_platdata(NULL);
#endif
#ifdef CONFIG_S5PV210_SETUP_FIMC2
	s3c_fimc2_set_platdata(NULL);
#endif
#endif

#ifdef CONFIG_SMSC911X
	mango210_smsc911x_set();
#endif

#ifdef CONFIG_VIDEO_MFC50
	/* mfc */
	s3c_mfc_set_platdata(NULL);
#endif

#ifdef CONFIG_VIDEO_TV20
	s3c_set_qos();
#endif

#ifdef CONFIG_S3C_DEV_HSMMC
	s5pv210_default_sdhci0();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	s5pv210_default_sdhci1();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	s5pv210_default_sdhci2();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	s5pv210_default_sdhci3();
#endif
#ifdef CONFIG_S5PV210_SETUP_SDHCI
	s3c_sdhci_set_platdata();
#endif
	platform_add_devices(mango210_devices, ARRAY_SIZE(mango210_devices));
	
#if defined(CONFIG_HAVE_PWM)
	mango_backlight_register();
#endif

#ifdef CONFIG_USB_SUPPORT
	mango210_usb_host_set();
#endif
}

#ifdef CONFIG_USB_SUPPORT
/* Initializes OTG Phy. */
void otg_phy_init(void)
{
	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL)
		|(0x1<<0), S5P_USB_PHY_CONTROL); /*USB PHY0 Enable */
	__raw_writel((__raw_readl(S3C_USBOTG_PHYPWR)
		&~(0x3<<3)&~(0x1<<0))|(0x1<<5), S3C_USBOTG_PHYPWR);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYCLK)
		&~(0x5<<2))|(0x3<<0), S3C_USBOTG_PHYCLK);
	__raw_writel((__raw_readl(S3C_USBOTG_RSTCON)
		&~(0x3<<1))|(0x1<<0), S3C_USBOTG_RSTCON);
	udelay(10);
	__raw_writel(__raw_readl(S3C_USBOTG_RSTCON)
		&~(0x7<<0), S3C_USBOTG_RSTCON);
	udelay(10);
}
EXPORT_SYMBOL(otg_phy_init);

/* USB Control request data struct must be located here for DMA transfer */
//struct usb_ctrlrequest usb_ctrl __attribute__((aligned(8)));

/* OTG PHY Power Off */
void otg_phy_off(void)
{
	__raw_writel(__raw_readl(S3C_USBOTG_PHYPWR)
		|(0x3<<3), S3C_USBOTG_PHYPWR);
	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL)
		&~(1<<0), S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(otg_phy_off);

void usb_host_phy_init(void)
{
	struct clk *otg_clk;

	otg_clk = clk_get(NULL, "usbotg");
	clk_enable(otg_clk);

	if (readl(S5P_USB_PHY_CONTROL) & (0x1<<1))
		return;

	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL)
		|(0x1<<1), S5P_USB_PHY_CONTROL);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYPWR)
		&~(0x1<<7)&~(0x1<<6))|(0x1<<8)|(0x1<<5), S3C_USBOTG_PHYPWR);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYCLK)
		&~(0x1<<7))|(0x3<<0), S3C_USBOTG_PHYCLK);
	__raw_writel((__raw_readl(S3C_USBOTG_RSTCON))
		|(0x1<<4)|(0x1<<3), S3C_USBOTG_RSTCON);
	__raw_writel(__raw_readl(S3C_USBOTG_RSTCON)
		&~(0x1<<4)&~(0x1<<3), S3C_USBOTG_RSTCON);
}
EXPORT_SYMBOL(usb_host_phy_init);

void usb_host_phy_off(void)
{
	__raw_writel(__raw_readl(S3C_USBOTG_PHYPWR)
		|(0x1<<7)|(0x1<<6), S3C_USBOTG_PHYPWR);
	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL)
		&~(1<<1), S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(usb_host_phy_off);
#endif

static void __init mango210_fixup(struct machine_desc *desc,
					struct tag *tags, char **cmdline,
					struct meminfo *mi)
{
	mi->bank[0].start = 0x20000000;
	mi->bank[0].size = 256 * SZ_1M;
	mi->bank[0].node = 0;

	mi->bank[1].start = 0x40000000;
	mi->bank[1].size = 256 * SZ_1M;
	mi->bank[1].node = 0;

	mi->nr_banks = 2;
}

MACHINE_START(MANGO210, "MANGO210")
	/* Maintainer: Kukjin Kim <kgene.kim@samsung.com> */
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.fixup		= mango210_fixup,
	.init_irq	= s5pv210_init_irq,
	.map_io		= mango210_map_io,
	.init_machine	= mango210_machine_init,
	.timer		= &s5p_systimer,
MACHINE_END
