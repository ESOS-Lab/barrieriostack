/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Register interface file for Exynos Scaler driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "scaler.h"
#include "scaler-regs.h"

#define COEF(val_l, val_h) ((((val_h) & 0x1FF) << 16) | ((val_l) & 0x1FF))

/* Scaling coefficient value */
static const __u32 sc_coef_8t[7][16][4] = {
	{ /* 8:8  or zoom-in */
		{COEF( 0,   0), COEF(  0,   0), COEF(128,   0), COEF(  0,  0)},
		{COEF( 0,   1), COEF( -2,   7), COEF(127,  -6), COEF(  2, -1)},
		{COEF( 0,   1), COEF( -5,  16), COEF(125, -12), COEF(  4, -1)},
		{COEF( 0,   2), COEF( -8,  25), COEF(120, -15), COEF(  5, -1)},
		{COEF(-1,   3), COEF(-10,  35), COEF(114, -18), COEF(  6, -1)},
		{COEF(-1,   4), COEF(-13,  46), COEF(107, -20), COEF(  6, -1)},
		{COEF(-1,   5), COEF(-16,  57), COEF( 99, -21), COEF(  7, -2)},
		{COEF(-1,   5), COEF(-18,  68), COEF( 89, -20), COEF(  6, -1)},
		{COEF(-1,   6), COEF(-20,  79), COEF( 79, -20), COEF(  6, -1)},
		{COEF(-1,   6), COEF(-20,  89), COEF( 68, -18), COEF(  5, -1)},
		{COEF(-2,   7), COEF(-21,  99), COEF( 57, -16), COEF(  5, -1)},
		{COEF(-1,   6), COEF(-20, 107), COEF( 46, -13), COEF(  4, -1)},
		{COEF(-1,   6), COEF(-18, 114), COEF( 35, -10), COEF(  3, -1)},
		{COEF(-1,   5), COEF(-15, 120), COEF( 25,  -8), COEF(  2,  0)},
		{COEF(-1,   4), COEF(-12, 125), COEF( 16,  -5), COEF(  1,  0)},
		{COEF(-1,   2), COEF( -6, 127), COEF(  7,  -2), COEF(  1,  0)}
	}, { /* 8:7 Zoom-out */
		{COEF( 0,   3), COEF( -8,  13), COEF(111,  14), COEF( -8,  3)},
		{COEF(-1,   3), COEF(-10,  21), COEF(112,   7), COEF( -6,  2)},
		{COEF(-1,   4), COEF(-12,  28), COEF(110,   1), COEF( -4,  2)},
		{COEF(-1,   4), COEF(-13,  36), COEF(106,  -3), COEF( -2,  1)},
		{COEF(-1,   4), COEF(-15,  44), COEF(103,  -7), COEF( -1,  1)},
		{COEF(-1,   4), COEF(-16,  53), COEF( 97, -11), COEF(  1,  1)},
		{COEF(-1,   4), COEF(-16,  61), COEF( 91, -13), COEF(  2,  0)},
		{COEF(-1,   4), COEF(-17,  69), COEF( 85, -15), COEF(  3,  0)},
		{COEF( 0,   3), COEF(-16,  77), COEF( 77, -16), COEF(  3,  0)},
		{COEF( 0,   3), COEF(-15,  85), COEF( 69, -17), COEF(  4, -1)},
		{COEF( 0,   2), COEF(-13,  91), COEF( 61, -16), COEF(  4, -1)},
		{COEF( 1,   1), COEF(-11,  97), COEF( 53, -16), COEF(  4, -1)},
		{COEF( 1,  -1), COEF( -7, 103), COEF( 44, -15), COEF(  4, -1)},
		{COEF( 1,  -2), COEF( -3, 106), COEF( 36, -13), COEF(  4, -1)},
		{COEF( 2,  -4), COEF(  1, 110), COEF( 28, -12), COEF(  4, -1)},
		{COEF( 2,  -6), COEF(  7, 112), COEF( 21, -10), COEF(  3, -1)}
	}, { /* 8:6 Zoom-out */
		{COEF( 0,   2), COEF(-11,  25), COEF( 96,  25), COEF(-11,  2)},
		{COEF( 0,   2), COEF(-12,  31), COEF( 96,  19), COEF(-10,  2)},
		{COEF( 0,   2), COEF(-12,  37), COEF( 94,  14), COEF( -9,  2)},
		{COEF( 0,   1), COEF(-12,  43), COEF( 92,  10), COEF( -8,  2)},
		{COEF( 0,   1), COEF(-12,  49), COEF( 90,   5), COEF( -7,  2)},
		{COEF( 1,   0), COEF(-12,  55), COEF( 86,   1), COEF( -5,  2)},
		{COEF( 1,  -1), COEF(-11,  61), COEF( 82,  -2), COEF( -4,  2)},
		{COEF( 1,  -1), COEF( -9,  67), COEF( 77,  -5), COEF( -3,  1)},
		{COEF( 1,  -2), COEF( -7,  72), COEF( 72,  -7), COEF( -2,  1)},
		{COEF( 1,  -3), COEF( -5,  77), COEF( 67,  -9), COEF( -1,  1)},
		{COEF( 2,  -4), COEF( -2,  82), COEF( 61, -11), COEF( -1,  1)},
		{COEF( 2,  -5), COEF(  1,  86), COEF( 55, -12), COEF(  0,  1)},
		{COEF( 2,  -7), COEF(  5,  90), COEF( 49, -12), COEF(  1,  0)},
		{COEF( 2,  -8), COEF( 10,  92), COEF( 43, -12), COEF(  1,  0)},
		{COEF( 2,  -9), COEF( 14,  94), COEF( 37, -12), COEF(  2,  0)},
		{COEF( 2, -10), COEF( 19,  96), COEF( 31, -12), COEF(  2,  0)}
	}, { /* 8:5 Zoom-out */
		{COEF( 0,  -1), COEF( -8,  33), COEF( 80,  33), COEF( -8, -1)},
		{COEF( 1,  -2), COEF( -7,  37), COEF( 80,  28), COEF( -8, -1)},
		{COEF( 1,  -2), COEF( -7,  41), COEF( 79,  24), COEF( -8,  0)},
		{COEF( 1,  -3), COEF( -6,  46), COEF( 78,  20), COEF( -8,  0)},
		{COEF( 1,  -3), COEF( -4,  50), COEF( 76,  16), COEF( -8,  0)},
		{COEF( 1,  -4), COEF( -3,  54), COEF( 74,  13), COEF( -7,  0)},
		{COEF( 1,  -5), COEF( -1,  58), COEF( 71,  10), COEF( -7,  1)},
		{COEF( 1,  -5), COEF(  1,  62), COEF( 68,   6), COEF( -6,  1)},
		{COEF( 1,  -6), COEF(  4,  65), COEF( 65,   4), COEF( -6,  1)},
		{COEF( 1,  -6), COEF(  6,  68), COEF( 62,   1), COEF( -5,  1)},
		{COEF( 1,  -7), COEF( 10,  71), COEF( 58,  -1), COEF( -5,  1)},
		{COEF( 0,  -7), COEF( 13,  74), COEF( 54,  -3), COEF( -4,  1)},
		{COEF( 0,  -8), COEF( 16,  76), COEF( 50,  -4), COEF( -3,  1)},
		{COEF( 0,  -8), COEF( 20,  78), COEF( 46,  -6), COEF( -3,  1)},
		{COEF( 0,  -8), COEF( 24,  79), COEF( 41,  -7), COEF( -2,  1)},
		{COEF(-1,  -8), COEF( 28,  80), COEF( 37,  -7), COEF( -2,  1)}
	}, { /* 8:4 Zoom-out */
		{COEF( 0,  -3), COEF(  0,  35), COEF( 64,  35), COEF(  0, -3)},
		{COEF( 0,  -3), COEF(  1,  38), COEF( 64,  32), COEF( -1, -3)},
		{COEF( 0,  -3), COEF(  2,  41), COEF( 63,  29), COEF( -2, -2)},
		{COEF( 0,  -4), COEF(  4,  43), COEF( 63,  27), COEF( -3, -2)},
		{COEF( 0,  -4), COEF(  6,  46), COEF( 61,  24), COEF( -3, -2)},
		{COEF( 0,  -4), COEF(  7,  49), COEF( 60,  21), COEF( -3, -2)},
		{COEF(-1,  -4), COEF(  9,  51), COEF( 59,  19), COEF( -4, -1)},
		{COEF(-1,  -4), COEF( 12,  53), COEF( 57,  16), COEF( -4, -1)},
		{COEF(-1,  -4), COEF( 14,  55), COEF( 55,  14), COEF( -4, -1)},
		{COEF(-1,  -4), COEF( 16,  57), COEF( 53,  12), COEF( -4, -1)},
		{COEF(-1,  -4), COEF( 19,  59), COEF( 51,   9), COEF( -4, -1)},
		{COEF(-2,  -3), COEF( 21,  60), COEF( 49,   7), COEF( -4,  0)},
		{COEF(-2,  -3), COEF( 24,  61), COEF( 46,   6), COEF( -4,  0)},
		{COEF(-2,  -3), COEF( 27,  63), COEF( 43,   4), COEF( -4,  0)},
		{COEF(-2,  -2), COEF( 29,  63), COEF( 41,   2), COEF( -3,  0)},
		{COEF(-3,  -1), COEF( 32,  64), COEF( 38,   1), COEF( -3,  0)}
	}, { /* 8:3 Zoom-out */
		{COEF( 0,  -1), COEF(  8,  33), COEF( 48,  33), COEF(  8, -1)},
		{COEF(-1,  -1), COEF(  9,  35), COEF( 49,  31), COEF(  7, -1)},
		{COEF(-1,  -1), COEF( 10,  36), COEF( 49,  30), COEF(  6, -1)},
		{COEF(-1,  -1), COEF( 12,  38), COEF( 48,  28), COEF(  5, -1)},
		{COEF(-1,   0), COEF( 13,  39), COEF( 48,  26), COEF(  4, -1)},
		{COEF(-1,   0), COEF( 15,  41), COEF( 47,  24), COEF(  3, -1)},
		{COEF(-1,   0), COEF( 16,  42), COEF( 47,  23), COEF(  2, -1)},
		{COEF(-1,   1), COEF( 18,  43), COEF( 45,  21), COEF(  2, -1)},
		{COEF(-1,   1), COEF( 19,  45), COEF( 45,  19), COEF(  1, -1)},
		{COEF(-1,   2), COEF( 21,  45), COEF( 43,  18), COEF(  1, -1)},
		{COEF(-1,   2), COEF( 23,  47), COEF( 42,  16), COEF(  0, -1)},
		{COEF(-1,   3), COEF( 24,  47), COEF( 41,  15), COEF(  0, -1)},
		{COEF(-1,   4), COEF( 26,  48), COEF( 39,  13), COEF(  0, -1)},
		{COEF(-1,   5), COEF( 28,  48), COEF( 38,  12), COEF( -1, -1)},
		{COEF(-1,   6), COEF( 30,  49), COEF( 36,  10), COEF( -1, -1)},
		{COEF(-1,   7), COEF( 31,  49), COEF( 35,   9), COEF( -1, -1)}
	}, { /* 8:2 Zoom-out */
		{COEF( 0,   2), COEF( 13,  30), COEF( 38,  30), COEF( 13,  2)},
		{COEF( 0,   3), COEF( 14,  30), COEF( 38,  29), COEF( 12,  2)},
		{COEF( 0,   3), COEF( 15,  31), COEF( 38,  28), COEF( 11,  2)},
		{COEF( 0,   4), COEF( 16,  32), COEF( 38,  26), COEF( 10,  2)},
		{COEF( 0,   4), COEF( 17,  33), COEF( 37,  26), COEF( 10,  1)},
		{COEF( 0,   5), COEF( 18,  34), COEF( 37,  24), COEF(  9,  1)},
		{COEF( 0,   5), COEF( 19,  34), COEF( 37,  24), COEF(  8,  1)},
		{COEF( 1,   6), COEF( 20,  35), COEF( 36,  22), COEF(  7,  1)},
		{COEF( 1,   6), COEF( 21,  36), COEF( 36,  21), COEF(  6,  1)},
		{COEF( 1,   7), COEF( 22,  36), COEF( 35,  20), COEF(  6,  1)},
		{COEF( 1,   8), COEF( 24,  37), COEF( 34,  19), COEF(  5,  0)},
		{COEF( 1,   9), COEF( 24,  37), COEF( 34,  18), COEF(  5,  0)},
		{COEF( 1,  10), COEF( 26,  37), COEF( 33,  17), COEF(  4,  0)},
		{COEF( 2,  10), COEF( 26,  38), COEF( 32,  16), COEF(  4,  0)},
		{COEF( 2,  11), COEF( 28,  38), COEF( 31,  15), COEF(  3,  0)},
		{COEF( 2,  12), COEF( 29,  38), COEF( 30,  14), COEF(  3,  0)}
	}
};

static const __u32 sc_coef_4t[7][16][2] = {
	{ /* 8:8  or zoom-in */
		{COEF( 0,   0), COEF(128,  0)},
		{COEF( 0,   5), COEF(127, -4)},
		{COEF(-1,  11), COEF(124, -6)},
		{COEF(-1,  19), COEF(118, -8)},
		{COEF(-2,  27), COEF(111, -8)},
		{COEF(-3,  37), COEF(102, -8)},
		{COEF(-4,  48), COEF( 92, -8)},
		{COEF(-5,  59), COEF( 81, -7)},
		{COEF(-6,  70), COEF( 70, -6)},
		{COEF(-7,  81), COEF( 59, -5)},
		{COEF(-8,  92), COEF( 48, -4)},
		{COEF(-8, 102), COEF( 37, -3)},
		{COEF(-8, 111), COEF( 27, -2)},
		{COEF(-8, 118), COEF( 19, -1)},
		{COEF(-6, 124), COEF( 11, -1)},
		{COEF(-4, 127), COEF(  5,  0)}
	}, { /* 8:7 Zoom-out  */
		{COEF( 0,   8), COEF(112,  8)},
		{COEF(-1,  14), COEF(111,  4)},
		{COEF(-2,  20), COEF(109,  1)},
		{COEF(-2,  27), COEF(105, -2)},
		{COEF(-3,  34), COEF(100, -3)},
		{COEF(-3,  43), COEF( 93, -5)},
		{COEF(-4,  51), COEF( 86, -5)},
		{COEF(-4,  60), COEF( 77, -5)},
		{COEF(-5,  69), COEF( 69, -5)},
		{COEF(-5,  77), COEF( 60, -4)},
		{COEF(-5,  86), COEF( 51, -4)},
		{COEF(-5,  93), COEF( 43, -3)},
		{COEF(-3, 100), COEF( 34, -3)},
		{COEF(-2, 105), COEF( 27, -2)},
		{COEF( 1, 109), COEF( 20, -2)},
		{COEF( 4, 111), COEF( 14, -1)}
	}, { /* 8:6 Zoom-out  */
		{COEF( 0,  16), COEF( 96, 16)},
		{COEF(-2,  21), COEF( 97, 12)},
		{COEF(-2,  26), COEF( 96,  8)},
		{COEF(-2,  32), COEF( 93,  5)},
		{COEF(-2,  39), COEF( 89,  2)},
		{COEF(-2,  46), COEF( 84,  0)},
		{COEF(-3,  53), COEF( 79, -1)},
		{COEF(-2,  59), COEF( 73, -2)},
		{COEF(-2,  66), COEF( 66, -2)},
		{COEF(-2,  73), COEF( 59, -2)},
		{COEF(-1,  79), COEF( 53, -3)},
		{COEF( 0,  84), COEF( 46, -2)},
		{COEF( 2,  89), COEF( 39, -2)},
		{COEF( 5,  93), COEF( 32, -2)},
		{COEF( 8,  96), COEF( 26, -2)},
		{COEF(12,  97), COEF( 21, -2)}
	}, { /* 8:5 Zoom-out  */
		{COEF( 0,  22), COEF( 84, 22)},
		{COEF(-1,  26), COEF( 85, 18)},
		{COEF(-1,  31), COEF( 84, 14)},
		{COEF(-1,  36), COEF( 82, 11)},
		{COEF(-1,  42), COEF( 79,  8)},
		{COEF(-1,  47), COEF( 76,  6)},
		{COEF( 0,  52), COEF( 72,  4)},
		{COEF( 0,  58), COEF( 68,  2)},
		{COEF( 1,  63), COEF( 63,  1)},
		{COEF( 2,  68), COEF( 58,  0)},
		{COEF( 4,  72), COEF( 52,  0)},
		{COEF( 6,  76), COEF( 47, -1)},
		{COEF( 8,  79), COEF( 42, -1)},
		{COEF(11,  82), COEF( 36, -1)},
		{COEF(14,  84), COEF( 31, -1)},
		{COEF(18,  85), COEF( 26, -1)}
	}, { /* 8:4 Zoom-out  */
		{COEF( 0,  26), COEF( 76, 26)},
		{COEF( 0,  30), COEF( 76, 22)},
		{COEF( 0,  34), COEF( 75, 19)},
		{COEF( 1,  38), COEF( 73, 16)},
		{COEF( 1,  43), COEF( 71, 13)},
		{COEF( 2,  47), COEF( 69, 10)},
		{COEF( 3,  51), COEF( 66,  8)},
		{COEF( 4,  55), COEF( 63,  6)},
		{COEF( 5,  59), COEF( 59,  5)},
		{COEF( 6,  63), COEF( 55,  4)},
		{COEF( 8,  66), COEF( 51,  3)},
		{COEF(10,  69), COEF( 47,  2)},
		{COEF(13,  71), COEF( 43,  1)},
		{COEF(16,  73), COEF( 38,  1)},
		{COEF(19,  75), COEF( 34,  0)},
		{COEF(22,  76), COEF( 30,  0)}
	}, { /* 8:3 Zoom-out */
		{COEF( 0,  29), COEF( 70, 29)},
		{COEF( 2,  32), COEF( 68, 26)},
		{COEF( 2,  36), COEF( 67, 23)},
		{COEF( 3,  39), COEF( 66, 20)},
		{COEF( 3,  43), COEF( 65, 17)},
		{COEF( 4,  46), COEF( 63, 15)},
		{COEF( 5,  50), COEF( 61, 12)},
		{COEF( 7,  53), COEF( 58, 10)},
		{COEF( 8,  56), COEF( 56,  8)},
		{COEF(10,  58), COEF( 53,  7)},
		{COEF(12,  61), COEF( 50,  5)},
		{COEF(15,  63), COEF( 46,  4)},
		{COEF(17,  65), COEF( 43,  3)},
		{COEF(20,  66), COEF( 39,  3)},
		{COEF(23,  67), COEF( 36,  2)},
		{COEF(26,  68), COEF( 32,  2)}
	}, { /* 8:2 Zoom-out  */
		{COEF( 0,  32), COEF( 64, 32)},
		{COEF( 3,  34), COEF( 63, 28)},
		{COEF( 4,  37), COEF( 62, 25)},
		{COEF( 4,  40), COEF( 62, 22)},
		{COEF( 5,  43), COEF( 61, 19)},
		{COEF( 6,  46), COEF( 59, 17)},
		{COEF( 7,  48), COEF( 58, 15)},
		{COEF( 9,  51), COEF( 55, 13)},
		{COEF(11,  53), COEF( 53, 11)},
		{COEF(13,  55), COEF( 51,  9)},
		{COEF(15,  58), COEF( 48,  7)},
		{COEF(17,  59), COEF( 46,  6)},
		{COEF(19,  61), COEF( 43,  5)},
		{COEF(22,  62), COEF( 40,  4)},
		{COEF(25,  62), COEF( 37,  4)},
		{COEF(28,  63), COEF( 34,  3)}
	},
};

/* CSC(Color Space Conversion) coefficient value */
static struct sc_csc_tab sc_no_csc = {
	{ 0x200, 0x000, 0x000, 0x000, 0x200, 0x000, 0x000, 0x000, 0x200 },
};

static struct sc_csc_tab sc_y2r = {
	/* (0,1) 601 Narrow */
	{ 0x254, 0x000, 0x331, 0x254, 0xF38, 0xE60, 0x254, 0x409, 0x000 },
	/* (0,1) 601 Wide */
	{ 0x200, 0x000, 0x2BE, 0x200, 0xF54, 0xE9B, 0x200, 0x377, 0x000 },
	/* (0,1) 709 Narrow */
	{ 0x254, 0x000, 0x331, 0x254, 0xF38, 0xE60, 0x254, 0x409, 0x000 },
	/* (0,1) 709 Wide */
	{ 0x200, 0x000, 0x314, 0x200, 0xFA2, 0xF15, 0x200, 0x3A2, 0x000 },
};

static struct sc_csc_tab sc_r2y = {
	/* (1,0) 601 Narrow */
	{ 0x084, 0x102, 0x032, 0xFB4, 0xF6B, 0x0E1, 0x0E1, 0xF44, 0xFDC },
	/* (1,0) 601 Wide  */
	{ 0x099, 0x12D, 0x03A, 0xFA8, 0xF52, 0x106, 0x106, 0xF25, 0xFD6 },
	/* (1,0) 709 Narrow */
	{ 0x05E, 0x13A, 0x020, 0xFCC, 0xF53, 0x0E1, 0x0E1, 0xF34, 0xFEC },
	/* (1,0) 709 Wide */
	{ 0x06D, 0x16E, 0x025, 0xFC4, 0xF36, 0x106, 0x106, 0xF12, 0xFE8 },
};

static struct sc_csc_tab *sc_csc_list[] = {
	[0] = &sc_no_csc,
	[1] = &sc_y2r,
	[2] = &sc_r2y,
};

static struct sc_bl_op_val sc_bl_op_tbl[] = {
	/* Sc,	 Sa,	Dc,	Da */
	{ZERO,	 ZERO,	ZERO,	ZERO},		/* CLEAR */
	{ ONE,	 ONE,	ZERO,	ZERO},		/* SRC */
	{ZERO,	 ZERO,	ONE,	ONE},		/* DST */
	{ ONE,	 ONE,	INV_SA,	INV_SA},	/* SRC_OVER */
	{INV_DA, ONE,	ONE,	INV_SA},	/* DST_OVER */
	{DST_A,	 DST_A,	ZERO,	ZERO},		/* SRC_IN */
	{ZERO,	 ZERO,	SRC_A,	SRC_A},		/* DST_IN */
	{INV_DA, INV_DA, ZERO,	ZERO},		/* SRC_OUT */
	{ZERO,	 ZERO,	INV_SA,	INV_SA},	/* DST_OUT */
	{DST_A,	 ZERO,	INV_SA,	ONE},		/* SRC_ATOP */
	{INV_DA, ONE,	SRC_A,	ZERO},		/* DST_ATOP */
	{INV_DA, ONE,	INV_SA,	ONE},		/* XOR: need to WA */
	{INV_DA, ONE,	INV_SA,	INV_SA},	/* DARKEN */
	{INV_DA, ONE,	INV_SA,	INV_SA},	/* LIGHTEN */
	{INV_DA, ONE,	INV_SA,	INV_SA},	/* MULTIPLY */
	{ONE,	 ONE,	INV_SC,	INV_SA},	/* SCREEN */
	{ONE,	 ONE,	ONE,	ONE},		/* ADD */
};

int sc_hwset_src_image_format(struct sc_dev *sc, const struct sc_fmt *fmt)
{
	writel(fmt->cfg_val, sc->regs + SCALER_SRC_CFG);
	return 0;
}

int sc_hwset_dst_image_format(struct sc_dev *sc, const struct sc_fmt *fmt)
{
	writel(fmt->cfg_val, sc->regs + SCALER_DST_CFG);

	/*
	 * When output format is RGB,
	 * CSC_Y_OFFSET_DST_EN should be 0
	 * to avoid color distortion
	 */
	if (fmt->is_rgb) {
		writel(readl(sc->regs + SCALER_CFG) &
					~SCALER_CFG_CSC_Y_OFFSET_DST,
				sc->regs + SCALER_CFG);
	}

	return 0;
}

void sc_hwset_pre_multi_format(struct sc_dev *sc)
{
	unsigned long cfg = readl(sc->regs + SCALER_SRC_CFG);

	if ((cfg & SCALER_CFG_FMT_MASK) == SCALER_CFG_FMT_ARGB8888) {
		cfg &= ~SCALER_CFG_FMT_MASK;
		cfg |= SCALER_CFG_FMT_P_ARGB8888;
		writel(cfg, sc->regs + SCALER_SRC_CFG);
	} else {
		dev_err(sc->dev, "only support src pre-multiplied argb888\n");
	}

	cfg = readl(sc->regs + SCALER_DST_CFG);
	if ((cfg & SCALER_CFG_FMT_MASK) == SCALER_CFG_FMT_ARGB8888) {
		cfg &= ~SCALER_CFG_FMT_MASK;
		cfg |= SCALER_CFG_FMT_P_ARGB8888;
		writel(cfg, sc->regs + SCALER_DST_CFG);
	} else {
		dev_err(sc->dev, "only support dst pre-multiplied argb888\n");
	}
}

void get_blend_value(unsigned int *cfg, u32 val, bool pre_multi)
{
	unsigned int tmp;

	*cfg &= ~(SCALER_SEL_INV_MASK | SCALER_SEL_MASK |
			SCALER_OP_SEL_INV_MASK | SCALER_OP_SEL_MASK);

	if (val == 0xff) {
		*cfg |= (1 << SCALER_SEL_INV_SHIFT);
	} else {
		if (pre_multi)
			*cfg |= (1 << SCALER_SEL_SHIFT);
		else
			*cfg |= (2 << SCALER_SEL_SHIFT);
		tmp = val & 0xf;
		*cfg |= (tmp << SCALER_OP_SEL_SHIFT);
	}

	if (val >= BL_INV_BIT_OFFSET)
		*cfg |= (1 << SCALER_OP_SEL_INV_SHIFT);
}

void sc_hwset_blend(struct sc_dev *sc, enum sc_blend_op bl_op, bool pre_multi,
		unsigned char g_alpha)
{
	unsigned int cfg = readl(sc->regs + SCALER_CFG);
	int idx = bl_op - 1;

	cfg |= SCALER_CFG_BLEND_EN;
	writel(cfg, sc->regs + SCALER_CFG);

	cfg = readl(sc->regs + SCALER_SRC_BLEND_COLOR);
	get_blend_value(&cfg, sc_bl_op_tbl[idx].src_color, pre_multi);
	if (g_alpha < 0xff)
		cfg |= (SRC_GA << SCALER_OP_SEL_SHIFT);
	writel(cfg, sc->regs + SCALER_SRC_BLEND_COLOR);
	sc_dbg("src_blend_color is 0x%x, %d\n", cfg, pre_multi);

	cfg = readl(sc->regs + SCALER_SRC_BLEND_ALPHA);
	get_blend_value(&cfg, sc_bl_op_tbl[idx].src_alpha, 1);
	if (g_alpha < 0xff)
		cfg |= (SRC_GA << SCALER_OP_SEL_SHIFT) | (g_alpha << 0);
	writel(cfg, sc->regs + SCALER_SRC_BLEND_ALPHA);
	sc_dbg("src_blend_alpha is 0x%x\n", cfg);

	cfg = readl(sc->regs + SCALER_DST_BLEND_COLOR);
	get_blend_value(&cfg, sc_bl_op_tbl[idx].dst_color, pre_multi);
	if (g_alpha < 0xff)
		cfg |= ((INV_SAGA & 0xf) << SCALER_OP_SEL_SHIFT);
	writel(cfg, sc->regs + SCALER_DST_BLEND_COLOR);
	sc_dbg("dst_blend_color is 0x%x\n", cfg);

	cfg = readl(sc->regs + SCALER_DST_BLEND_ALPHA);
	get_blend_value(&cfg, sc_bl_op_tbl[idx].dst_alpha, 1);
	if (g_alpha < 0xff)
		cfg |= ((INV_SAGA & 0xf) << SCALER_OP_SEL_SHIFT);
	writel(cfg, sc->regs + SCALER_DST_BLEND_ALPHA);
	sc_dbg("dst_blend_alpha is 0x%x\n", cfg);

	/*
	 * If dst format is non-premultiplied format
	 * and blending operation is enabled,
	 * result image should be divided by alpha value
	 * because the result is always pre-multiplied.
	 */
	if (!pre_multi) {
		cfg = readl(sc->regs + SCALER_CFG);
		cfg |= SCALER_CFG_BL_DIV_ALPHA_EN;
		writel(cfg, sc->regs + SCALER_CFG);
	}
}

void sc_hwset_color_fill(struct sc_dev *sc, unsigned int val)
{
	unsigned int cfg = readl(sc->regs + SCALER_CFG);

	cfg |= SCALER_CFG_FILL_EN;
	writel(cfg, sc->regs + SCALER_CFG);

	cfg = readl(sc->regs + SCALER_FILL_COLOR);
	cfg = val;
	writel(cfg, sc->regs + SCALER_FILL_COLOR);
	sc_dbg("color filled is 0x%08x\n", val);
}

void sc_hwset_dith(struct sc_dev *sc, unsigned int val)
{
	unsigned int cfg = readl(sc->regs + SCALER_DITH_CFG);

	cfg &= ~(SCALER_DITH_R_MASK | SCALER_DITH_G_MASK | SCALER_DITH_B_MASK);
	cfg |= val;
	writel(cfg, sc->regs + SCALER_DITH_CFG);
}

void sc_hwset_csc_coef(struct sc_dev *sc, enum sc_csc_idx idx,
			struct sc_csc *csc)
{
	unsigned int i, j, tmp;
	unsigned long cfg;
	int *csc_eq_val;

	if (idx == NO_CSC) {
		csc_eq_val = sc_csc_list[idx]->narrow_601;
	} else {
		if (csc->csc_eq == SC_CSC_601) {
			if (csc->csc_range == SC_CSC_NARROW)
				csc_eq_val = sc_csc_list[idx]->narrow_601;
			else
				csc_eq_val = sc_csc_list[idx]->wide_601;
		} else {
			if (csc->csc_range == SC_CSC_NARROW)
				csc_eq_val = sc_csc_list[idx]->narrow_709;
			else
				csc_eq_val = sc_csc_list[idx]->wide_709;
		}
	}

	tmp = SCALER_CSC_COEF22 - SCALER_CSC_COEF00;

	for (i = 0, j = 0; i < 9; i++, j += 4) {
		cfg = readl(sc->regs + SCALER_CSC_COEF00 + j);
		cfg &= ~SCALER_CSC_COEF_MASK;
		cfg |= csc_eq_val[i];
		writel(cfg, sc->regs + SCALER_CSC_COEF00 + j);
		sc_dbg("csc value %d - %d\n", i, csc_eq_val[i]);
	}

	/* set CSC_Y_OFFSET_EN */
	cfg = readl(sc->regs + SCALER_CFG);
	if (idx == CSC_Y2R) {
		if (csc->csc_range == SC_CSC_WIDE)
			cfg &= ~SCALER_CFG_CSC_Y_OFFSET_SRC;
		else
			cfg |= SCALER_CFG_CSC_Y_OFFSET_SRC;
	} else if (idx == CSC_R2Y) {
		if (csc->csc_range == SC_CSC_WIDE)
			cfg |= SCALER_CFG_CSC_Y_OFFSET_DST;
		else
			cfg &= ~SCALER_CFG_CSC_Y_OFFSET_DST;
	}
	writel(cfg, sc->regs + SCALER_CFG);
}

static const __u32 sc_scaling_ratio[] = {
	1048576,	/* 0: 8:8 scaing or zoom-in */
	1198372,	/* 1: 8:7 zoom-out */
	1398101,	/* 2: 8:6 zoom-out */
	1677721,	/* 3: 8:5 zoom-out */
	2097152,	/* 4: 8:4 zoom-out */
	2796202,	/* 5: 8:3 zoom-out */
	/* higher ratio -> 6: 8:2 zoom-out */
};

static unsigned int sc_get_scale_filter(unsigned int ratio)
{
	unsigned int filter;

	for (filter = 0; filter < ARRAY_SIZE(sc_scaling_ratio); filter++)
		if (ratio <= sc_scaling_ratio[filter])
			return filter;

	return filter;
}

void sc_hwset_polyphase_hcoef(struct sc_dev *sc,
				unsigned int yratio, unsigned int cratio)
{
	unsigned int phase;
	unsigned int yfilter = sc_get_scale_filter(yratio);
	unsigned int cfilter = sc_get_scale_filter(cratio);

	BUG_ON(yfilter >= ARRAY_SIZE(sc_coef_8t));
	BUG_ON(cfilter >= ARRAY_SIZE(sc_coef_8t));

	BUILD_BUG_ON(ARRAY_SIZE(sc_coef_8t[yfilter]) < 9);
	BUILD_BUG_ON(ARRAY_SIZE(sc_coef_8t[cfilter]) < 9);

	/* reset value of the coefficient registers are the 8:8 table */
	if (IS_ENABLED(CONFIG_SCALER_NO_SOFTRST) || (yfilter != 0)) {
		for (phase = 0; phase < 9; phase++) {
			writel(sc_coef_8t[yfilter][phase][3],
				sc->regs + SCALER_YHCOEF + phase * 16);
			writel(sc_coef_8t[yfilter][phase][2],
				sc->regs + SCALER_YHCOEF + phase * 16 + 4);
			writel(sc_coef_8t[yfilter][phase][1],
				sc->regs + SCALER_YHCOEF + phase * 16 + 8);
			writel(sc_coef_8t[yfilter][phase][0],
				sc->regs + SCALER_YHCOEF + phase * 16 + 12);
		}
	}

	if (IS_ENABLED(CONFIG_SCALER_NO_SOFTRST) || (cfilter != 0)) {
		for (phase = 0; phase < 9; phase++) {
			writel(sc_coef_8t[cfilter][phase][3],
				sc->regs + SCALER_CHCOEF + phase * 16);
			writel(sc_coef_8t[cfilter][phase][2],
				sc->regs + SCALER_CHCOEF + phase * 16 + 4);
			writel(sc_coef_8t[cfilter][phase][1],
				sc->regs + SCALER_CHCOEF + phase * 16 + 8);
			writel(sc_coef_8t[cfilter][phase][0],
				sc->regs + SCALER_CHCOEF + phase * 16 + 12);
		}
	}
}

void sc_hwset_polyphase_vcoef(struct sc_dev *sc,
				unsigned int yratio, unsigned int cratio)
{
	unsigned int phase;
	unsigned int yfilter = sc_get_scale_filter(yratio);
	unsigned int cfilter = sc_get_scale_filter(cratio);

	BUG_ON(yfilter >= ARRAY_SIZE(sc_coef_4t));
	BUG_ON(cfilter >= ARRAY_SIZE(sc_coef_4t));

	BUILD_BUG_ON(ARRAY_SIZE(sc_coef_4t[yfilter]) < 9);
	BUILD_BUG_ON(ARRAY_SIZE(sc_coef_4t[cfilter]) < 9);

	/* reset value of the coefficient registers are the 8:8 table */
	if (IS_ENABLED(CONFIG_SCALER_NO_SOFTRST) || (yfilter != 0)) {
		for (phase = 0; phase < 9; phase++) {
			writel(sc_coef_4t[yfilter][phase][1],
				sc->regs + SCALER_YVCOEF + phase * 8);
			writel(sc_coef_4t[yfilter][phase][0],
				sc->regs + SCALER_YVCOEF + phase * 8 + 4);
		}
	}

	if (IS_ENABLED(CONFIG_SCALER_NO_SOFTRST) || (cfilter != 0)) {
		for (phase = 0; phase < 9; phase++) {
			writel(sc_coef_4t[cfilter][phase][1],
				sc->regs + SCALER_CVCOEF + phase * 8);
			writel(sc_coef_4t[cfilter][phase][0],
				sc->regs + SCALER_CVCOEF + phase * 8 + 4);
		}
	}
}

void sc_hwset_src_imgsize(struct sc_dev *sc, struct sc_frame *frame)
{
	unsigned long cfg = 0;

	cfg &= ~(SCALER_SRC_CSPAN_MASK | SCALER_SRC_YSPAN_MASK);
	cfg |= frame->width;

	/*
	 * TODO: C width should be half of Y width
	 * but, how to get the diffferent c width from user
	 * like AYV12 format
	 */
	if (frame->sc_fmt->num_comp == 2)
		cfg |= (frame->width << frame->sc_fmt->cspan) << 16;
	if (frame->sc_fmt->num_comp == 3) {
		if (sc_fmt_is_ayv12(frame->sc_fmt->pixelformat))
			cfg |= ALIGN(frame->width >> 1, 16) << 16;
		else if (frame->sc_fmt->cspan) /* YUV444 */
			cfg |= frame->width << 16;
		else
			cfg |= (frame->width >> 1) << 16;
	}

	writel(cfg, sc->regs + SCALER_SRC_SPAN);
}

void sc_hwset_intsrc_imgsize(struct sc_dev *sc, int num_comp, __u32 width)
{
	unsigned long cfg = 0;

	cfg &= ~(SCALER_SRC_CSPAN_MASK | SCALER_SRC_YSPAN_MASK);
	cfg |= width;

	/*
	 * TODO: C width should be half of Y width
	 * but, how to get the diffferent c width from user
	 * like AYV12 format
	 */
	if (num_comp == 2)
		cfg |= width << 16;
	if (num_comp == 3)
		cfg |= (width >> 1) << 16;

	writel(cfg, sc->regs + SCALER_SRC_SPAN);
}

void sc_hwset_dst_imgsize(struct sc_dev *sc, struct sc_frame *frame)
{
	unsigned long cfg = 0;

	cfg &= ~(SCALER_DST_CSPAN_MASK | SCALER_DST_YSPAN_MASK);
	cfg |= frame->width;

	/*
	 * TODO: C width should be half of Y width
	 * but, how to get the diffferent c width from user
	 * like AYV12 format
	 */
	if (frame->sc_fmt->num_comp == 2)
		cfg |= (frame->width << frame->sc_fmt->cspan) << 16;
	if (frame->sc_fmt->num_comp == 3) {
		if (sc_fmt_is_ayv12(frame->sc_fmt->pixelformat))
			cfg |= ALIGN(frame->width >> 1, 16) << 16;
		else if (frame->sc_fmt->cspan) /* YUV444 */
			cfg |= frame->width << 16;
		else
			cfg |= (frame->width >> 1) << 16;
	}

	writel(cfg, sc->regs + SCALER_DST_SPAN);
}

void sc_hwset_src_addr(struct sc_dev *sc, struct sc_addr *addr)
{
	writel(addr->y, sc->regs + SCALER_SRC_Y_BASE);
	writel(addr->cb, sc->regs + SCALER_SRC_CB_BASE);
	writel(addr->cr, sc->regs + SCALER_SRC_CR_BASE);
}

void sc_hwset_dst_addr(struct sc_dev *sc, struct sc_addr *addr)
{
	writel(addr->y, sc->regs + SCALER_DST_Y_BASE);
	writel(addr->cb, sc->regs + SCALER_DST_CB_BASE);
	writel(addr->cr, sc->regs + SCALER_DST_CR_BASE);
}

void sc_hwregs_dump(struct sc_dev *sc)
{
	dev_notice(sc->dev, "Dumping control registers...\n");
	pr_notice("------------------------------------------------\n");

	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x000, 0x044 - 0x000 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x050, 0x058 - 0x050 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x060, 0x134 - 0x060 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x140, 0x214 - 0x140 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x220, 0x240 - 0x220 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x250, 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x260, 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x278, 4, false);
	if (sc->version <= SCALER_VERSION(2, 1, 1))
		print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x280, 0x28C - 0x280 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x290, 0x298 - 0x290 + 4, false);
	if (sc->version <= SCALER_VERSION(2, 1, 1))
		print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x2A8, 0x2A8 - 0x2A0 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x2B0, 0x2C4 - 0x2B0 + 4, false);
	if (sc->version >= SCALER_VERSION(3, 0, 0))
		print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x2D0, 0x2DC - 0x2D0 + 4, false);

	/* shadow registers */
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x1004, 0x1004 - 0x1004 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x1010, 0x1044 - 0x1010 + 4, false);

	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x1050, 0x1058 - 0x1050 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x1060, 0x1134 - 0x1060 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x1140, 0x1214 - 0x1140 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x1220, 0x1240 - 0x1220 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x1250, 4, false);
	if (sc->version <= SCALER_VERSION(2, 1, 1))
		print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x1280, 0x128C - 0x1280 + 4, false);
	print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x1290, 0x1298 - 0x1290 + 4, false);
	if (sc->version >= SCALER_VERSION(3, 0, 0))
		print_hex_dump(KERN_NOTICE, "", DUMP_PREFIX_ADDRESS, 16, 4,
			sc->regs + 0x12D0, 0x12DC - 0x12D0 + 4, false);
	pr_notice("------------------------------------------------\n");
}

/* starts from the second status which is the begining of err status */
const static char *sc_irq_err_status[] = {
	[ 0] = "illigal src color",
	[ 1] = "illigal src Y base",
	[ 2] = "illigal src Cb base",
	[ 3] = "illigal src Cr base",
	[ 4] = "illigal src Y span",
	[ 5] = "illigal src C span",
	[ 6] = "illigal src YH pos",
	[ 7] = "illigal src YV pos",
	[ 8] = "illigal src CH pos",
	[ 9] = "illigal src CV pos",
	[10] = "illigal src width",
	[11] = "illigal src height",
	[12] = "illigal dst color",
	[13] = "illigal dst Y base",
	[14] = "illigal dst Cb base",
	[15] = "illigal dst Cr base",
	[16] = "illigal dst Y span",
	[17] = "illigal dst C span",
	[18] = "illigal dst H pos",
	[19] = "illigal dst V pos",
	[20] = "illigal dst width",
	[21] = "illigal dst height",
	[23] = "illigal scaling ratio",
	[25] = "illigal pre-scaler width/height",
	[31] = "timeout",
};

static void sc_print_irq_err_status(struct sc_dev *sc, u32 status)
{
	unsigned int i = 0;

	status >>= 1; /* ignore the INT_STATUS_FRAME_END */
	if (status) {
		sc_hwregs_dump(sc);

		while (status) {
			if (status & 1)
				dev_err(sc->dev,
					"Scaler reported error %u: %s\n",
					i + 1, sc_irq_err_status[i]);
			i++;
			status >>= 1;
		}
	}
}

u32 sc_hwget_and_clear_irq_status(struct sc_dev *sc)
{
	u32 val = __raw_readl(sc->regs + SCALER_INT_STATUS);
	sc_print_irq_err_status(sc, val);
	__raw_writel(val, sc->regs + SCALER_INT_STATUS);
	return val;
}
