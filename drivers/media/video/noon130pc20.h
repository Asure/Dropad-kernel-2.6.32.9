/* linux/drivers/media/video/noon130pc20.h
 *
 * Driver for NOON130PC20 (UXGA camera) from Samsung Electronics
 * 
 * 1.3Mp CMOS Image Sensor SoC with an Embedded Image Processor
 *
 * Copyright (C) 2009, Jinsung Yang <jsgood.yang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define NOON130PC20_COMPLETE
#ifndef __NOON130PC20_H__
#define __NOON130PC20_H__

struct noon130pc20_reg {
	unsigned char addr;
	unsigned char val;
};

struct noon130pc20_regset_type {
	unsigned char *regset;
	int len;
};

/*
 * Macro
 */
#define REGSET_LENGTH(x)	(sizeof(x)/sizeof(noon130pc20_reg))

/*
 * User defined commands
 */
/* S/W defined features for tune */
#define REG_DELAY	0xFF00	/* in ms */
#define REG_CMD		0xFFFF	/* Followed by command */

/* Following order should not be changed */
enum image_size_noon130pc20 {
	/* This SoC supports upto SXGA (1280*1024) */
#if 0
	QQVGA,	/* 160*120*/
	QCIF,	/* 176*144 */
	QVGA,	/* 320*240 */
	CIF,	/* 352*288 */
#endif
//	QVGA,	/* 320*240 */
	VGA,	/* 640*480 */
	SXGA,	/* 1280*1024 */
#if 0
	SVGA,	/* 800*600 */
	HD720P,	/* 1280*720 */
	UXGA,	/* 1600*1200 */
#endif
};

/*
 * Following values describe controls of camera
 * in user aspect and must be match with index of noon130pc20_regset[]
 * These values indicates each controls and should be used
 * to control each control
 */
enum noon130pc20_control {
	NOON130PC20_INIT,
	NOON130PC20_EV,
	NOON130PC20_AWB,
	NOON130PC20_MWB,
	NOON130PC20_EFFECT,
	NOON130PC20_CONTRAST,
	NOON130PC20_SATURATION,
	NOON130PC20_SHARPNESS,
};

#define NOON130PC20_REGSET(x)	{	\
	.regset = x,			\
	.len = sizeof(x)/sizeof(noon130pc20_reg),}

/*
 * User tuned register setting values
 */
static unsigned char noon130pc20_init_reg[][2] = {
	//--- Page 0: Calibration
	{0x03,0x00},
	{0x01,0x01},	// Power Sleep
	{0x01,0x03},	// Soft Reset, Power Sleep
	{0x01,0x01},	// Power Sleep

	//--- Page 3: Auto Exposure
	{0x03,0x03}, 	
	{0x10,0x0c},	// AG, Exposure Time

	//--- Page 4: White Balance
	{0x03,0x04},
	{0x10,0x69},	// Update Speed(110), R/B far gaint(10), Adaptive Step

	//--- Page 0: Calibration
	{0x03,0x00},	
	{0x02,0x0e},	// PLL off
	{0x10,0x19},	// Sub-Sampling (VGA), 1 frame skip, VSYNC type 2
	{0x11,0x90},	// Change Image Size, Skip bad 1frame, x flip, y flip
	{0x12,0x04},	// PCLK faling edge
	{0x13,0x00},	// WINROWH
	{0x14,0x02},	// WINROWL
	{0x15,0x00},	// WINCOLH
	{0x16,0x05},	// WINCOLL
	{0x17,0x04},	// WINHGTH
	{0x18,0x00},	// WINHGTL
	{0x19,0x05},	// WINWIDH
	{0x1a,0x00},	// WINWIDL
	{0x1b,0x00},	// blank time(HIGH)
	{0x1c,0xc8},	// blank time(LOW)
	{0x1d,0x00},	// VSYNCH
	{0x1e,0x14},	// VSYNCL //by pjlee
	{0x1f,0x09},	// clipping VSYNC -> SYNCTL
	{0x20,0xa8},	// Black Level Calibration
	{0x21,0xc1},	// BLC OFF ;ablcoff
	{0x22,0x03},	// RED color range
	{0x23,0x03},	// Green color range
	{0x24,0x03},	// Blue color range
	{0x40,0x37},	// Pixel Bias
	{0x41,0x77},	// ADC, ASP Bias
	{0x42,0x57},	// Main, Bus Bias
	{0x43,0xa8},	// Clamp enable
	{0x4b,0x41},	// Automatic Gain
	{0x71,0x00},	// Reserved Area
	{0x72,0x22},	// Reserved Area
	{0x80,0x3c},	// Reserved Area
	{0x85,0x16},	// Reserved Area
	{0x87,0x16},	// Reserved Area
	{0x8b,0x07},	// Reserved Area
	{0x91,0x5f},	// Reserved Area
	{0x92,0x60},	// Reserved Area
	{0xa1,0x04},	// VSYNC Type2
	{0xa2,0x07},	// VSYNC Type2
	{0xa3,0x0f},	// VSYNC Type2

	//--- Page 1: Image Effect, Format
	{0x03,0x01},	// 
	{0x10,0x03},	// ITU601, VYUY		-> pjlee 
	{0x11,0x03},	// YUV, raw data
	{0x12,0x30},	// Auto bright, Y offset
	{0x13,0x01},	// YUV Range
	{0x19,0x00},	// Offset of luminance
	{0x1a,0x00},	// Offset of luminance at dark
	{0x1b,0x00},	// U chrominance
	{0x1c,0x00},	// V chrominance
	{0x1d,0x80},	// U constant
	{0x1e,0x80},	// V constant
	{0x1f,0x00},	// Solarization
	{0x20,0x0f},	// Saturation (Auto)
	{0x21,0x94},	// U saturation
	{0x22,0x94},	// V saturation
	{0x33,0x38},	// Threshold (high)
	{0x35,0x20},	// Threshold (low)
	{0x36,0x3f},	// Sign of CMC
	{0x38,0x6f},	// coefficient11
	{0x39,0x2f},	// coefficient12
	{0x3a,0x00},	// coefficient13
	{0x3b,0x1a},	// coefficient21
	{0x3c,0x75},	// coefficient22
	{0x3d,0x1a},	// coefficient23
	{0x3e,0x0e},	// coefficient31
	{0x3f,0x38},	// coefficient32
	{0x40,0x86},	// coefficient33
	{0x41,0x00},	// CMC11
	{0x42,0x0f},	// CMC12
	{0x43,0x8f},	// CMC13
	{0x44,0x03},	// CMC21
	{0x45,0x00},	// CMC22
	{0x46,0x84},	// CMC23
	{0x47,0x97},	// CMC31
	{0x48,0xa2},	// CMC32
	{0x49,0x3a},	// CMC33
	{0x60,0x01},	// Gamma Enable
	{0x61,0x05},	// Gamma corrected 0 code
	{0x62,0x13},	// Gamma corrected 16 code
	{0x63,0x1e},	// Gamma corrected 32 code
	{0x64,0x30},	// Gamma corrected 64 code
	{0x65,0x4f},	// Gamma corrected 128 code
	{0x66,0x69},	// Gamma corrected 192 code
	{0x67,0x80},	// Gamma corrected 256 code
	{0x68,0x93},	// Gamma corrected 320 code
	{0x69,0xa5},	// Gamma corrected 384 code
	{0x6a,0xb5},	// Gamma corrected 448 code
	{0x6b,0xc3},	// Gamma corrected 512 code
	{0x6c,0xcf},	// Gamma corrected 576 code
	{0x6d,0xd9},	// Gamma corrected 640 code
	{0x6e,0xe2},	// Gamma corrected 704 code	
	{0x6f,0xea},	// Gamma corrected 768 code
	{0x70,0xf0},	// Gamma corrected 832 code
	{0x71,0xf6},	// Gamma corrected 896 code
	{0x72,0xfb},	// Gamma corrected 960 code
	{0x73,0xff},	// Gamma corrected 1023 code
	{0x90,0x92},	// Enable Edge
	{0x91,0x9e},	// Filter size
	{0x92,0x86},	// Edge data : 41->21
	{0x93,0x86},	// Edge data : 41->21
	{0x94,0x41},	// Edga value
	{0x95,0x20},	// previous edge
	{0x96,0x18},	// calculate edge
	{0x97,0x75},
	{0x99,0x28},	// AG
	{0x9a,0x04},
	{0x9b,0x12},
	{0x9c,0x10},
	{0x9d,0x10},
	{0xa0,0x0d},	// Noise reduction
	{0xa1,0x2e},	// Z-LPF : ON
	{0xa3,0x54},	// AG low pass filter
	{0xa4,0x08},	// exposure time low pass filter
	{0xa5,0x48},	// internal gain low pass filter
	{0xaa,0x02},
	{0xb1,0x00},	// Green Color
	{0xb2,0x00},
	{0xc0,0xc3},
	{0xc1,0x44},
	{0xc3,0x30},
	{0xc4,0x10},
	{0xc6,0x08},
	{0xc8,0x00},
	{0xc9,0x00},
	{0xd0,0x01},	// Shading Correction
	{0xd3,0x90},
	{0xd4,0x4e},
	{0xd5,0x48},
	{0xd6,0x88},
	{0xd7,0x80},
	{0xe0,0x01},	// False Color Correction
	{0xe1,0x18},
	{0xe3,0xe9},
	{0xe4,0x88},
	{0xe5,0xf0},

	//--- Page 2: Image Scaling
	{0x03,0x02},
	{0x51,0x50},	// horizintal line
	{0x52,0x42},
	{0x60,0xf7},	// Image statistics
	{0x61,0x87},
	{0x64,0x44},
	{0x85,0x14},

	//--- Page 3: Auto Exposure
	{0x03,0x03},
	{0x11,0xc1},	// Exposure Time
	{0x15,0x57},
	{0x16,0x0b},
	{0x17,0x05},
	{0x18,0x02},
	{0x19,0x71},
	{0x1a,0x5c},
	{0x1b,0x44},
	{0x1c,0x4c},
	{0x1d,0x34},
	{0x1e,0x20},
	{0x26,0x95},
	{0x33,0x01},
	{0x34,0x86},
	{0x35,0xa0},
	{0x36,0x00},
	{0x37,0x50},
	{0x38,0x05},
	{0x39,0xb8},
	{0x3a,0xd8},
	{0x3b,0x75},
	{0x3c,0x30},
	{0x3d,0x61},
	{0x3e,0xa8},
	{0x3f,0x09},
	{0x40,0x27},
	{0x41,0x02},
	{0x42,0xfe},
	{0x43,0x99},
	{0x48,0x89},
	{0x49,0x34},
	{0x4c,0x03},
	{0x4d,0xe8},
	{0x50,0x16},
	{0x51,0x15},
	{0x52,0x50},
	{0x53,0x15},
	{0x54,0x15},
	{0x55,0x3b},
	{0x56,0x60},
	{0x57,0x60},
	{0x58,0x40},
	{0x59,0x28},
	{0x5a,0x21},
	{0x5b,0x1e},
	{0x5c,0x1b},
	{0x5d,0x19},
	{0x5e,0x1a},
	{0x60,0x44},
	{0x61,0x88},
	{0x65,0x75},

	//--- Page 4: Auto White Balance
	{0x03,0x04},
	{0x11,0x2e},
	{0x12,0x32},
	{0x13,0x00},
	{0x14,0x0d},
	{0x19,0xff},
	{0x1f,0x58},
	{0x20,0x12},
	{0x21,0x6c},
	{0x22,0x88},
	{0x23,0xe3},
	{0x24,0xaa},
	{0x25,0x88},
	{0x26,0x88},
	{0x27,0x66},
	{0x2a,0x77},
	{0x2b,0x66},
	{0x2c,0x0a},
	{0x2d,0x08},
	{0x2e,0x04},
	{0x2f,0x88},
	{0x30,0x50},
	{0x31,0xa1},
	{0x32,0xa4},
	{0x33,0xc8},
	{0x34,0x0a},
	{0x35,0x03},
	{0x36,0x43},
	{0x37,0x44},
	{0x38,0x44},
	{0x39,0x44},
	{0x3a,0x0a},
	{0x3b,0x44},
	{0x40,0x34},
	{0x41,0x20},
	{0x42,0x32},
	{0x43,0x48},
	{0x44,0x18},
	{0x45,0x50},
	{0x46,0x1e},
	{0x47,0x48},
	{0x48,0x34},
	{0x49,0x2f},
	{0x4a,0x1e},
	{0x4b,0x0d},
	{0x4c,0x0d},
	{0x4d,0x04},
	{0x4e,0x60},
	{0x4f,0x58},
	{0x50,0x78},
	{0x51,0x46},
	{0x52,0x3d},
	{0x53,0x36},
	{0x54,0x2b},
	{0x55,0x28},
	{0x56,0x24},
	{0x57,0x22},
	{0x58,0x22},
	{0x59,0x20},
	{0x5a,0x20},
	{0x5b,0x45},
	{0x5c,0x55},
	{0x5d,0x3c},
	{0x5e,0x30},
	{0x5f,0x20},
	{0x60,0x7e},
	{0x61,0x80},
	{0x62,0x10},
	{0x63,0x01},
	{0x64,0x26},
	{0x65,0x36},
	{0x10,0xe9},


	{0x03, 0x03},
	{0x10, 0x8c},
	{0x03, 0x00},
	{0x01, 0x00},
};

/* SXGA */
static unsigned char noon130pc20_sxga_reg[][2] =
{
	{0x03,0x00},
	{0x10,0x00},
};

/* VGA */
static unsigned char noon130pc20_vga_reg[][2] =
{
	{0x03,0x00},
	{0x10,0x10},
};

#define NOON130PC20_INIT_REGS	(sizeof(noon130pc20_init_reg) / sizeof(noon130pc20_init_reg[0]))
#define NOON130PC20_SXGA_REGS	(sizeof(noon130pc20_sxga_reg) / sizeof(noon130pc20_sxga_reg[0]))
#define NOON130PC20_VGA_REGS	(sizeof(noon130pc20_vga_reg) / sizeof(noon130pc20_vga_reg[0]))

/*
 * EV bias
 */

static const struct noon130pc20_reg noon130pc20_ev_m6[] = {
};

static const struct noon130pc20_reg noon130pc20_ev_m5[] = {
};

static const struct noon130pc20_reg noon130pc20_ev_m4[] = {
};

static const struct noon130pc20_reg noon130pc20_ev_m3[] = {
};

static const struct noon130pc20_reg noon130pc20_ev_m2[] = {
};

static const struct noon130pc20_reg noon130pc20_ev_m1[] = {
};

static const struct noon130pc20_reg noon130pc20_ev_default[] = {
};

static const struct noon130pc20_reg noon130pc20_ev_p1[] = {
};

static const struct noon130pc20_reg noon130pc20_ev_p2[] = {
};

static const struct noon130pc20_reg noon130pc20_ev_p3[] = {
};

static const struct noon130pc20_reg noon130pc20_ev_p4[] = {
};

static const struct noon130pc20_reg noon130pc20_ev_p5[] = {
};

static const struct noon130pc20_reg noon130pc20_ev_p6[] = {
};

#ifdef NOON130PC20_COMPLETE
/* Order of this array should be following the querymenu data */
static const unsigned char *noon130pc20_regs_ev_bias[] = {
	(unsigned char *)noon130pc20_ev_m6, (unsigned char *)noon130pc20_ev_m5,
	(unsigned char *)noon130pc20_ev_m4, (unsigned char *)noon130pc20_ev_m3,
	(unsigned char *)noon130pc20_ev_m2, (unsigned char *)noon130pc20_ev_m1,
	(unsigned char *)noon130pc20_ev_default, (unsigned char *)noon130pc20_ev_p1,
	(unsigned char *)noon130pc20_ev_p2, (unsigned char *)noon130pc20_ev_p3,
	(unsigned char *)noon130pc20_ev_p4, (unsigned char *)noon130pc20_ev_p5,
	(unsigned char *)noon130pc20_ev_p6,
};

/*
 * Auto White Balance configure
 */
static const struct noon130pc20_reg noon130pc20_awb_off[] = {
};

static const struct noon130pc20_reg noon130pc20_awb_on[] = {
};

static const unsigned char *noon130pc20_regs_awb_enable[] = {
	(unsigned char *)noon130pc20_awb_off,
	(unsigned char *)noon130pc20_awb_on,
};

/*
 * Manual White Balance (presets)
 */
static const struct noon130pc20_reg noon130pc20_wb_tungsten[] = {

};

static const struct noon130pc20_reg noon130pc20_wb_fluorescent[] = {

};

static const struct noon130pc20_reg noon130pc20_wb_sunny[] = {

};

static const struct noon130pc20_reg noon130pc20_wb_cloudy[] = {

};

/* Order of this array should be following the querymenu data */
static const unsigned char *noon130pc20_regs_wb_preset[] = {
	(unsigned char *)noon130pc20_wb_tungsten,
	(unsigned char *)noon130pc20_wb_fluorescent,
	(unsigned char *)noon130pc20_wb_sunny,
	(unsigned char *)noon130pc20_wb_cloudy,
};

/*
 * Color Effect (COLORFX)
 */
static const struct noon130pc20_reg noon130pc20_color_sepia[] = {
};

static const struct noon130pc20_reg noon130pc20_color_aqua[] = {
};

static const struct noon130pc20_reg noon130pc20_color_monochrome[] = {
};

static const struct noon130pc20_reg noon130pc20_color_negative[] = {
};

static const struct noon130pc20_reg noon130pc20_color_sketch[] = {
};

/* Order of this array should be following the querymenu data */
static const unsigned char *noon130pc20_regs_color_effect[] = {
	(unsigned char *)noon130pc20_color_sepia,
	(unsigned char *)noon130pc20_color_aqua,
	(unsigned char *)noon130pc20_color_monochrome,
	(unsigned char *)noon130pc20_color_negative,
	(unsigned char *)noon130pc20_color_sketch,
};

/*
 * Contrast bias
 */
static const struct noon130pc20_reg noon130pc20_contrast_m2[] = {
};

static const struct noon130pc20_reg noon130pc20_contrast_m1[] = {
};

static const struct noon130pc20_reg noon130pc20_contrast_default[] = {
};

static const struct noon130pc20_reg noon130pc20_contrast_p1[] = {
};

static const struct noon130pc20_reg noon130pc20_contrast_p2[] = {
};

static const unsigned char *noon130pc20_regs_contrast_bias[] = {
	(unsigned char *)noon130pc20_contrast_m2,
	(unsigned char *)noon130pc20_contrast_m1,
	(unsigned char *)noon130pc20_contrast_default,
	(unsigned char *)noon130pc20_contrast_p1,
	(unsigned char *)noon130pc20_contrast_p2,
};

/*
 * Saturation bias
 */
static const struct noon130pc20_reg noon130pc20_saturation_m2[] = {
};

static const struct noon130pc20_reg noon130pc20_saturation_m1[] = {
};

static const struct noon130pc20_reg noon130pc20_saturation_default[] = {
};

static const struct noon130pc20_reg noon130pc20_saturation_p1[] = {
};

static const struct noon130pc20_reg noon130pc20_saturation_p2[] = {
};

static const unsigned char *noon130pc20_regs_saturation_bias[] = {
	(unsigned char *)noon130pc20_saturation_m2,
	(unsigned char *)noon130pc20_saturation_m1,
	(unsigned char *)noon130pc20_saturation_default,
	(unsigned char *)noon130pc20_saturation_p1,
	(unsigned char *)noon130pc20_saturation_p2,
};

/*
 * Sharpness bias
 */
static const struct noon130pc20_reg noon130pc20_sharpness_m2[] = {
};

static const struct noon130pc20_reg noon130pc20_sharpness_m1[] = {
};

static const struct noon130pc20_reg noon130pc20_sharpness_default[] = {
};

static const struct noon130pc20_reg noon130pc20_sharpness_p1[] = {
};

static const struct noon130pc20_reg noon130pc20_sharpness_p2[] = {
};

static const unsigned char *noon130pc20_regs_sharpness_bias[] = {
	(unsigned char *)noon130pc20_sharpness_m2,
	(unsigned char *)noon130pc20_sharpness_m1,
	(unsigned char *)noon130pc20_sharpness_default,
	(unsigned char *)noon130pc20_sharpness_p1,
	(unsigned char *)noon130pc20_sharpness_p2,
};
#endif /* NOON130PC20_COMPLETE */

#endif
