# arch/arm/plat-s5p/Makefile
#
# Copyright (c) 2009 Samsung Electronics Co., Ltd.
# 		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y				:=
obj-m				:=
obj-n				:= dummy.o
obj-				:=

# Core files

obj-y				+= dev-uart.o
obj-y				+= cpu.o
obj-y				+= clock.o
obj-y				+= irq.o
obj-y				+= devs.o
obj-y				+= bootmem.o
obj-y				+= setup-i2c0.o
ifndef CONFIG_S5P_HIGH_RES_TIMERS
obj-$(CONFIG_SYSTIMER_S5P)	+= systimer-s5p.o
else
ifdef CONFIG_HRT_RTC
obj-y				+= hr-time-rtc.o
else
obj-y				+= hr-time.o
endif
endif
obj-$(CONFIG_PM)		+= pm.o
obj-$(CONFIG_PM)		+= irq-pm.o
obj-$(CONFIG_PM)		+= sleep.o
# Device files:
obj-$(CONFIG_OLD_DMA_PL330)	+= dma-pl330.o
obj-$(CONFIG_NEW_DMA_PL330)	+= s3c-pl330.o dev-dma.o

obj-$(CONFIG_S5P_SETUP_I2C1)	+= setup-i2c1.o
obj-$(CONFIG_S5P_SETUP_I2C2)	+= setup-i2c2.o

obj-$(CONFIG_S5P_DEV_MFC)	+= dev-mfc.o

obj-$(CONFIG_S5P_SETUP_MFC)	+= setup-mfc.o

obj-$(CONFIG_SND_S3C24XX_SOC)	+= dev-audio.o

obj-$(CONFIG_TOUCHSCREEN_S3C)   += dev-ts.o
