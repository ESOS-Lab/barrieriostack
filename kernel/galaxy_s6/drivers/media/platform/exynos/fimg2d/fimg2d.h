/* linux/drivers/media/video/exynos/fimg2d/fimg2d.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __FIMG2D_H
#define __FIMG2D_H __FILE__

#ifdef __KERNEL__

#include <linux/clk.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/pm_qos.h>
#include <linux/mm_types.h>
#include <linux/exynos_iovmm.h>
#include <linux/compat.h>

#define FIMG2D_MINOR		(240)

#if defined(CONFIG_EXYNOS7_IOMMU) || defined(CONFIG_EXYNOS_IOMMU_V6)
#define FIMG2D_IOVMM_PAGETABLE
#define FIMG2D_IOVA_START		(0x10000000)
#define FIMG2D_IOVA_PLANE_SIZE		(0x10000000)
#define FIMG2D_IOVA_MAX_PLANES		(2)
#endif

enum fimg2d_ip_version {
	IP_VER_G2D_4P,
	IP_VER_G2D_5G,
	IP_VER_G2D_5A,
	IP_VER_G2D_5AR,
	IP_VER_G2D_5R,
	IP_VER_G2D_5V,
	IP_VER_G2D_5H,
	IP_VER_G2D_5AR2,
	IP_VER_G2D_5HP,
	IP_VER_G2D_7I,
};

struct fimg2d_platdata {
	int ip_ver;
	int hw_ver;
	const char *parent_clkname;
	const char *clkname;
	const char *gate_clkname;
	const char *gate_clkname2;
	unsigned long clkrate;
	int  cpu_min;
	int  kfc_min;
	int  mif_min;
	int  int_min;
};

#define to_fimg2d_plat(d)	(to_platform_device(d)->dev.platform_data)
#define ip_is_g2d_7i()		(fimg2d_ip_version_is() == IP_VER_G2D_7I)
#define ip_is_g2d_5a()		(fimg2d_ip_version_is() == IP_VER_G2D_5A)
#define ip_is_g2d_5ar()		(fimg2d_ip_version_is() == IP_VER_G2D_5AR)
#define ip_is_g2d_5ar2()	(fimg2d_ip_version_is() == IP_VER_G2D_5AR2)
#define ip_is_g2d_5r()		(fimg2d_ip_version_is() == IP_VER_G2D_5R)
#define ip_is_g2d_5v()		(fimg2d_ip_version_is() == IP_VER_G2D_5V)
#define ip_is_g2d_5h()		(fimg2d_ip_version_is() == IP_VER_G2D_5H)
#define ip_is_g2d_5hp()		(fimg2d_ip_version_is() == IP_VER_G2D_5HP)
#define ip_is_g2d_5g()		(fimg2d_ip_version_is() == IP_VER_G2D_5G)
#define ip_is_g2d_4p()		(fimg2d_ip_version_is() == IP_VER_G2D_4P)

#ifdef DEBUG
/*
 * g2d_debug value:
 *	0: no print
 *	1: err
 *	2: info
 *	3: perf
 *	4: oneline (simple)
 *	5: debug
 */
extern int g2d_debug;

enum debug_level {
	DBG_NO,
	DBG_ERR,
	DBG_INFO,
	DBG_PERF,
	DBG_ONELINE,
	DBG_DEBUG,
};

#define g2d_print(level, fmt, args...)				\
	do {							\
		if (g2d_debug >= level)				\
			pr_info("[%s] " fmt, __func__, ##args);	\
	} while (0)

#define fimg2d_err(fmt, args...)	g2d_print(DBG_ERR, fmt, ##args)
#define fimg2d_info(fmt, args...)	g2d_print(DBG_INFO, fmt, ##args)
#define fimg2d_debug(fmt, args...)	g2d_print(DBG_DEBUG, fmt, ##args)
#else
#define g2d_print(level, fmt, args...)
#define fimg2d_err(fmt, args...)
#define fimg2d_info(fmt, args...)
#define fimg2d_debug(fmt, arg...)
#endif

#endif /* __KERNEL__ */

/* ioctl commands */
#define FIMG2D_IOCTL_MAGIC	'F'
#define FIMG2D_BITBLT_BLIT	_IOWR(FIMG2D_IOCTL_MAGIC, 0, struct fimg2d_blit)
#define FIMG2D_BITBLT_SYNC	_IOW(FIMG2D_IOCTL_MAGIC, 1, int)
#define FIMG2D_BITBLT_VERSION	_IOR(FIMG2D_IOCTL_MAGIC, 2, struct fimg2d_version)
#define FIMG2D_BITBLT_ACTIVATE	_IOW(FIMG2D_IOCTL_MAGIC, 3, enum driver_act)

enum fimg2d_qos_status {
	FIMG2D_QOS_ON = 0,
	FIMG2D_QOS_OFF
};

enum fimg2d_qos_level {
	G2D_LV0 = 0,
	G2D_LV1,
	G2D_LV2,
	G2D_LV3,
	G2D_LV4,
	G2D_LV_END
};

enum fimg2d_ctx_state {
	CTX_READY = 0,
	CTX_ERROR,
};

enum fimg2d_pw_status {
	FIMG2D_PW_ON = 0,
	FIMG2D_PW_OFF
};

enum driver_act {
	DRV_ACT = 0,
	DRV_DEACT
};

struct fimg2d_version {
	unsigned int hw;
	unsigned int sw;
};

/**
 * @BLIT_SYNC: sync mode, to wait for blit done irq
 * @BLIT_ASYNC: async mode, not to wait for blit done irq
 */
enum blit_sync {
	BLIT_SYNC,
	BLIT_ASYNC,
};

/**
 * @ADDR_PHYS: physical address
 * @ADDR_USER: user virtual address (physically Non-contiguous)
 * @ADDR_USER_CONTIG: user virtual address (physically Contiguous)
 * @ADDR_DEVICE: specific device virtual address
 */
enum addr_space {
	ADDR_NONE,
	ADDR_PHYS,
	ADDR_KERN,
	ADDR_USER,
	ADDR_USER_CONTIG,
	ADDR_DEVICE,
};

/**
 * Pixel order complies with little-endian style
 *
 * DO NOT CHANGE THIS ORDER
 */
enum pixel_order {
	AX_RGB = 0,
	RGB_AX,
	AX_BGR,
	BGR_AX,
	ARGB_ORDER_END,

	P1_CRY1CBY0,
	P1_CBY1CRY0,
	P1_Y1CRY0CB,
	P1_Y1CBY0CR,
	P1_ORDER_END,

	P2_CRCB,
	P2_CBCR,
	P2_ORDER_END,
};

/**
 * DO NOT CHANGE THIS ORDER
 */
enum color_format {
	CF_XRGB_8888 = 0,
	CF_ARGB_8888,
	CF_RGB_565,
	CF_XRGB_1555,
	CF_ARGB_1555,
	CF_XRGB_4444,
	CF_ARGB_4444,
	CF_RGB_888,
	CF_YCBCR_444,
	CF_YCBCR_422,
	CF_YCBCR_420,
	CF_A8,
	CF_L8,
	SRC_DST_FORMAT_END,

	CF_MSK_1BIT,
	CF_MSK_4BIT,
	CF_MSK_8BIT,
	CF_MSK_16BIT_565,
	CF_MSK_16BIT_1555,
	CF_MSK_16BIT_4444,
	CF_MSK_32BIT_8888,
	MSK_FORMAT_END,
};

enum rotation {
	ORIGIN,
	ROT_90,	/* clockwise */
	ROT_180,
	ROT_270,
	XFLIP,	/* x-axis flip */
	YFLIP,	/* y-axis flip */
};

/**
 * @NO_REPEAT: no effect
 * @REPEAT_NORMAL: repeat horizontally and vertically
 * @REPEAT_PAD: pad with pad color
 * @REPEAT_REFLECT: reflect horizontally and vertically
 * @REPEAT_CLAMP: pad with edge color of original image
 *
 * DO NOT CHANGE THIS ORDER
 */
enum repeat {
	NO_REPEAT = 0,
	REPEAT_NORMAL,	/* default setting */
	REPEAT_PAD,
	REPEAT_REFLECT, REPEAT_MIRROR = REPEAT_REFLECT,
	REPEAT_CLAMP,
};

enum scaling {
	NO_SCALING,
	SCALING_NEAREST,
	SCALING_BILINEAR,
};

/**
 * premultiplied alpha
 */
enum premultiplied {
	PREMULTIPLIED,
	NON_PREMULTIPLIED,
};

/**
 * @TRANSP: discard bluescreen color
 * @BLUSCR: replace bluescreen color with background color
 */
enum bluescreen {
	OPAQUE,
	TRANSP,
	BLUSCR,
};

/**
 * DO NOT CHANGE THIS ORDER
 */
enum blit_op {
	BLIT_OP_SOLID_FILL = 0,

	BLIT_OP_CLR,
	BLIT_OP_SRC, BLIT_OP_SRC_COPY = BLIT_OP_SRC,
	BLIT_OP_DST,
	BLIT_OP_SRC_OVER,
	BLIT_OP_DST_OVER, BLIT_OP_OVER_REV = BLIT_OP_DST_OVER,
	BLIT_OP_SRC_IN,
	BLIT_OP_DST_IN, BLIT_OP_IN_REV = BLIT_OP_DST_IN,
	BLIT_OP_SRC_OUT,
	BLIT_OP_DST_OUT, BLIT_OP_OUT_REV = BLIT_OP_DST_OUT,
	BLIT_OP_SRC_ATOP,
	BLIT_OP_DST_ATOP, BLIT_OP_ATOP_REV = BLIT_OP_DST_ATOP,
	BLIT_OP_XOR,

	BLIT_OP_ADD,
	BLIT_OP_MULTIPLY,
	BLIT_OP_SCREEN,
	BLIT_OP_DARKEN,
	BLIT_OP_LIGHTEN,

	BLIT_OP_DISJ_SRC_OVER,
	BLIT_OP_DISJ_DST_OVER, BLIT_OP_SATURATE = BLIT_OP_DISJ_DST_OVER,
	BLIT_OP_DISJ_SRC_IN,
	BLIT_OP_DISJ_DST_IN, BLIT_OP_DISJ_IN_REV = BLIT_OP_DISJ_DST_IN,
	BLIT_OP_DISJ_SRC_OUT,
	BLIT_OP_DISJ_DST_OUT, BLIT_OP_DISJ_OUT_REV = BLIT_OP_DISJ_DST_OUT,
	BLIT_OP_DISJ_SRC_ATOP,
	BLIT_OP_DISJ_DST_ATOP, BLIT_OP_DISJ_ATOP_REV = BLIT_OP_DISJ_DST_ATOP,
	BLIT_OP_DISJ_XOR,

	BLIT_OP_CONJ_SRC_OVER,
	BLIT_OP_CONJ_DST_OVER, BLIT_OP_CONJ_OVER_REV = BLIT_OP_CONJ_DST_OVER,
	BLIT_OP_CONJ_SRC_IN,
	BLIT_OP_CONJ_DST_IN, BLIT_OP_CONJ_IN_REV = BLIT_OP_CONJ_DST_IN,
	BLIT_OP_CONJ_SRC_OUT,
	BLIT_OP_CONJ_DST_OUT, BLIT_OP_CONJ_OUT_REV = BLIT_OP_CONJ_DST_OUT,
	BLIT_OP_CONJ_SRC_ATOP,
	BLIT_OP_CONJ_DST_ATOP, BLIT_OP_CONJ_ATOP_REV = BLIT_OP_CONJ_DST_ATOP,
	BLIT_OP_CONJ_XOR,

	/* user select coefficient manually */
	BLIT_OP_USER_COEFF,

	BLIT_OP_USER_SRC_GA,

	/* Add new operation type here */

	/* end of blit operation */
	BLIT_OP_END,
};
#define MAX_FIMG2D_BLIT_OP	((int)BLIT_OP_END)

#ifdef __KERNEL__

/**
 * @TMP: temporary buffer for 2-step blit at a single command
 *
 * DO NOT CHANGE THIS ORDER
 */
enum image_object {
	IMAGE_SRC = 0,
	IMAGE_MSK,
	IMAGE_TMP,
	IMAGE_DST,
	IMAGE_END,
};
#define MAX_IMAGES		IMAGE_END
#define ISRC			IMAGE_SRC
#define IMSK			IMAGE_MSK
#define ITMP			IMAGE_TMP
#define IDST			IMAGE_DST

/**
 * @size: dma size of image
 * @cached: cached dma size of image
 */
struct fimg2d_dma {
	unsigned long addr;
	exynos_iova_t iova;
	off_t offset;
	size_t size;
	size_t cached;
};

struct fimg2d_dma_group {
	struct fimg2d_dma base;
	struct fimg2d_dma plane2;
};

#endif /* __KERNEL__ */

struct fimg2d_qos {
	unsigned int freq_int;
	unsigned int freq_mif;
	unsigned int freq_cpu;
	unsigned int freq_kfc;
};

/**
 * @start: start address or unique id of image
 */
struct fimg2d_addr {
	enum addr_space type;
	unsigned long start;
};

struct fimg2d_rect {
	int x1;
	int y1;
	int x2;	/* x1 + width */
	int y2; /* y1 + height */
};

/**
 * pixels can be different from src, dst or clip rect
 */
struct fimg2d_scale {
	enum scaling mode;

	/* ratio in pixels */
	int src_w, src_h;
	int dst_w, dst_h;
};

struct fimg2d_clip {
	bool enable;
	int x1;
	int y1;
	int x2;	/* x1 + width */
	int y2; /* y1 + height */
};

struct fimg2d_repeat {
	enum repeat mode;
	unsigned long pad_color;
};

/**
 * @bg_color: bg_color is valid only if bluescreen mode is BLUSCR.
 */
struct fimg2d_bluscr {
	enum bluescreen mode;
	unsigned long bs_color;
	unsigned long bg_color;
};

/**
 * @plane2: address info for CbCr in YCbCr 2plane mode
 * @rect: crop/clip rect
 * @need_cacheopr: true if cache coherency is required
 */
struct fimg2d_image {
	int width;
	int height;
	int stride;
	enum pixel_order order;
	enum color_format fmt;
	struct fimg2d_addr addr;
	struct fimg2d_addr plane2;
	struct fimg2d_rect rect;
	bool need_cacheopr;
};

/**
 * @solid_color:
 *         src color instead of src image
 *         color format and order must be ARGB8888(A is MSB).
 * @g_alpha: global(constant) alpha. 0xff is opaque, 0 is transparnet
 * @dither: dithering
 * @rotate: rotation degree in clockwise
 * @premult: alpha premultiplied mode for read & write
 * @scaling: common scaling info for src and mask image.
 * @repeat: repeat type (tile mode)
 * @bluscr: blue screen and transparent mode
 * @clipping: clipping rect within dst rect
 */
struct fimg2d_param {
	unsigned long solid_color;
	unsigned char g_alpha;
	bool dither;
	enum rotation rotate;
	enum premultiplied premult;
	struct fimg2d_scale scaling;
	struct fimg2d_repeat repeat;
	struct fimg2d_bluscr bluscr;
	struct fimg2d_clip clipping;
};

/**
 * @op: blit operation mode
 * @src: set when using src image
 * @msk: set when using mask image
 * @tmp: set when using 2-step blit at a single command
 * @dst: dst must not be null
 *         * tmp image must be the same to dst except memory address
 * @seq_no: user debugging info.
 *          for example, user can set sequence number or pid.
 */
struct fimg2d_blit {
	enum blit_op op;
	struct fimg2d_param param;
	struct fimg2d_image *src;
	struct fimg2d_image *msk;
	struct fimg2d_image *tmp;
	struct fimg2d_image *dst;
	enum blit_sync sync;
	unsigned int seq_no;
	enum fimg2d_qos_level qos_lv;
};

/* compat layer support */
#ifdef CONFIG_COMPAT

#define COMPAT_FIMG2D_BITBLT_BLIT	\
	_IOWR(FIMG2D_IOCTL_MAGIC, 0, struct compat_fimg2d_blit)
#define COMPAT_FIMG2D_BITBLT_VERSION	\
	_IOR(FIMG2D_IOCTL_MAGIC, 2, struct compat_fimg2d_version)

struct compat_fimg2d_version {
	compat_uint_t hw;
	compat_uint_t sw;
};

struct compat_fimg2d_scale {
	enum scaling mode;
	compat_int_t src_w, src_h;
	compat_int_t dst_w, dst_h;
};

struct compat_fimg2d_clip {
	bool enable;
	compat_int_t x1;
	compat_int_t y1;
	compat_int_t x2;
	compat_int_t y2;
};

struct compat_fimg2d_repeat {
	enum repeat mode;
	compat_ulong_t pad_color;
};

struct compat_fimg2d_bluscr {
	enum bluescreen mode;
	compat_ulong_t bs_color;
	compat_ulong_t bg_color;
};

struct compat_fimg2d_param {
	compat_ulong_t solid_color;
	unsigned char g_alpha;
	bool dither;
	enum rotation rotate;
	enum premultiplied premult;
	struct compat_fimg2d_scale scaling;
	struct compat_fimg2d_repeat repeat;
	struct compat_fimg2d_bluscr bluscr;
	struct compat_fimg2d_clip clipping;
};

struct compat_fimg2d_addr {
	enum addr_space type;
	compat_ulong_t start;
};

struct compat_fimg2d_rect {
	compat_int_t x1;
	compat_int_t y1;
	compat_int_t x2;
	compat_int_t y2;
};

struct compat_fimg2d_image {
	compat_int_t width;
	compat_int_t height;
	compat_int_t stride;
	enum pixel_order order;
	enum color_format fmt;
	struct compat_fimg2d_addr addr;
	struct compat_fimg2d_addr plane2;
	struct compat_fimg2d_rect rect;
	bool need_cacheopr;
};

struct compat_fimg2d_blit {
	enum blit_op op;
	struct compat_fimg2d_param param;
	compat_uptr_t src;
	compat_uptr_t msk;
	compat_uptr_t tmp;
	compat_uptr_t dst;
	enum blit_sync sync;
	compat_uint_t seq_no;
	enum fimg2d_qos_level qos_lv;
};

#endif

#ifdef __KERNEL__

enum perf_desc {
	PERF_CACHE = 0,
	PERF_SFR,
	PERF_BLIT,
	PERF_UNMAP,
	PERF_TOTAL,
	PERF_END
};
#define MAX_PERF_DESCS		PERF_END

struct fimg2d_perf {
	unsigned int seq_no;
	struct timeval start;
	struct timeval end;
};

/**
 * @pgd: base address of arm mmu pagetable
 * @ncmd: request count in blit command queue
 * @wait_q: conext wait queue head
*/
struct fimg2d_context {
	struct mm_struct *mm;
	atomic_t ncmd;
	wait_queue_head_t wait_q;
	struct fimg2d_perf perf[MAX_PERF_DESCS];
	void *vma_lock;
	unsigned long state;
};

/**
 * @op: blit operation mode
 * @sync: sync/async blit mode (currently support sync mode only)
 * @image: array of image object.
 *         [0] is for src image
 *         [1] is for mask image
 *         [2] is for temporary buffer
 *             set when using 2-step blit at a single command
 *         [3] is for dst, dst must not be null
 *         * tmp image must be the same to dst except memory address
 * @seq_no: user debugging info.
 *          for example, user can set sequence number or pid.
 * @dma_all: total dma size of src, msk, dst
 * @dma: array of dma info for each src, msk, tmp and dst
 * @ctx: context is created when user open fimg2d device.
 * @node: list head of blit command queue
 */
struct fimg2d_bltcmd {
	struct fimg2d_blit blt;
	struct fimg2d_image image[MAX_IMAGES];
	size_t dma_all;
	struct fimg2d_dma_group dma[MAX_IMAGES];
	struct fimg2d_context *ctx;
	struct list_head node;
};

/**
 * @suspended: in suspend mode
 * @clkon: power status for runtime pm
 * @mem: resource platform device
 * @regs: base address of hardware
 * @dev: pointer to device struct
 * @err: true if hardware is timed out while blitting
 * @irq: irq number
 * @nctx: context count
 * @busy: 1 if hardware is running
 * @bltlock: spinlock for blit
 * @wait_q: blit wait queue head
 * @cmd_q: blit command queue
 * @workqueue: workqueue_struct for kfimg2dd
*/
struct fimg2d_control {
	struct clk *clock;
	struct clk *qe_clock;
	struct device *dev;
	struct resource *mem;
	void __iomem *regs;
	bool boost;

	atomic_t drvact;
	atomic_t suspended;
	atomic_t clkon;

	atomic_t nctx;
	atomic_t busy;
	spinlock_t bltlock;
	spinlock_t qoslock;
	struct mutex drvlock;
	int irq;
	wait_queue_head_t wait_q;
	struct list_head cmd_q;
	struct workqueue_struct *work_q;
	struct pm_qos_request exynos5_g2d_cluster1_qos;
	struct pm_qos_request exynos5_g2d_cluster0_qos;
	struct pm_qos_request exynos5_g2d_mif_qos;
	struct pm_qos_request exynos5_g2d_int_qos;
	struct fimg2d_platdata *pdata;
	struct clk *clk_osc;
	struct clk *clk_parn1;
	struct clk *clk_parn2;
	struct clk *clk_chld1;
	struct clk *clk_chld2;
	struct fimg2d_qos g2d_qos;
	enum fimg2d_qos_level qos_lv;
	enum fimg2d_qos_level pre_qos_lv;

	int (*blit)(struct fimg2d_control *ctrl);
	int (*configure)(struct fimg2d_control *ctrl,
			struct fimg2d_bltcmd *cmd);
	void (*run)(struct fimg2d_control *ctrl);
	void (*stop)(struct fimg2d_control *ctrl);
	void (*dump)(struct fimg2d_control *ctrl);
	void (*finalize)(struct fimg2d_control *ctrl);
};

int fimg2d_register_ops(struct fimg2d_control *ctrl);
int fimg2d_ip_version_is(void);
int bit_per_pixel(struct fimg2d_image *img, int plane);

static inline int width2bytes(int width, int bpp)
{
	switch (bpp) {
	case 32:
	case 24:
	case 16:
	case 8:
		return (width * bpp) >> 3;
	case 1:
		return (width + 7) >> 3;
	case 4:
		return (width + 1) >> 1;
	default:
		return 0;
	}
}

#ifdef BLIT_WORKQUE
#define g2d_lock(x)		do {} while (0)
#define g2d_unlock(x)		do {} while (0)
#define g2d_spin_lock(x, f)	spin_lock_irqsave(x, f)
#define g2d_spin_unlock(x, f)	spin_unlock_irqrestore(x, f)
#else
#define g2d_lock(x)		mutex_lock(x)
#define g2d_unlock(x)		mutex_unlock(x)
#define g2d_spin_lock(x, f)	do { f = 0; } while (0)
#define g2d_spin_unlock(x, f)	do { f = 0; } while (0)
#endif

#endif /* __KERNEL__ */

#endif /* __FIMG2D_H__ */
