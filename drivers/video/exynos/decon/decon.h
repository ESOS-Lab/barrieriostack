/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.

 * Alternatively, this program is free software in case of open source projec;
 * you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 */


#ifndef ___SAMSUNG_DECON_H__
#define ___SAMSUNG_DECON_H__

#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>
#include <media/exynos_mc.h>
#include <mach/devfreq.h>
#include <mach/bts.h>

#include "regs-decon.h"
#include "decon_common.h"
#include "./panels/decon_lcd.h"
#include "dsim.h"

#if defined(CONFIG_EXYNOS_DECON_MDNIE)
#include "./panels/mdnie.h"
#endif

extern struct ion_device *ion_exynos;
extern struct decon_device *decon_int_drvdata;
extern struct decon_device *decon_ext_drvdata;
extern int decon_log_level;

#if defined(CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ) || defined(CONFIG_ARM_EXYNOS7420_BUS_DEVFREQ)
#define CONFIG_DECON_DEVFREQ
#endif

#define MAX_DECON_WIN		(7)
#define MAX_DECON_EXT_WIN	(5)

#define DRIVER_NAME		"decon"
#define MAX_NAME_SIZE		32

#define MAX_DECON_PADS		9
#define DECON_PAD_WB		6

#define MAX_BUF_PLANE_CNT	3
#define DECON_ENTER_LPD_CNT	3
#define MIN_BLK_MODE_WIDTH	144
#define MIN_BLK_MODE_HEIGHT	10

#define DECON_ENABLE		1
#define DECON_DISABLE		0
#define STATE_IDLE		0
#define STATE_DONE		1
#define DECON_BACKGROUND	0
#define VSYNC_TIMEOUT_MSEC	200
#define MAX_BW_PER_WINDOW	(2560 * 1600 * 4 * 60)
#define LCD_DEFAULT_BPP		24
#define WB_DEFAULT_BPP         32

#define DECON_TZPC_OFFSET	3
#define DECON_ODMA_WB		8
#define MAX_DMA_TYPE		9
#define MAX_FRM_DONE_WAIT	10
#define DISP_UTIL		70
#define DECON_INT_UTIL		65
#ifndef NO_CNT_TH
/* BTS driver defines the Threshold */
#define	NO_CNT_TH		10
#endif

#ifdef CONFIG_LCD_ALPM
#define ALPM_TIMEOUT 3
#endif

#define DECON_UNDERRUN_THRESHOLD	300

#ifdef CONFIG_FB_WINDOW_UPDATE
#define DECON_WIN_UPDATE_IDX	(7)
#define decon_win_update_dbg(fmt, ...)					\
	do {								\
		if (decon_log_level >= 7)				\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)
#else
#define decon_win_update_dbg(fmt, ...) while (0)
#endif

#define decon_err(fmt, ...)							\
	do {									\
		if (decon_log_level >= 3) {					\
			pr_err(pr_fmt(fmt), ##__VA_ARGS__);			\
			exynos_ss_printk(fmt, ##__VA_ARGS__);			\
		}								\
	} while (0)

#define decon_warn(fmt, ...)							\
	do {									\
		if (decon_log_level >= 4) {					\
			pr_warn(pr_fmt(fmt), ##__VA_ARGS__);			\
			exynos_ss_printk(fmt, ##__VA_ARGS__);			\
		}								\
	} while (0)

#define decon_info(fmt, ...)							\
	do {									\
		if (decon_log_level >= 6)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define decon_dbg(fmt, ...)							\
	do {									\
		if (decon_log_level >= 7)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

/*
 * DECON_STATE_ON : disp power on, decon/dsim clock on & lcd on
 * DECON_STATE_LPD_ENT_REQ : disp power on, decon/dsim clock on, lcd on & request for LPD
 * DECON_STATE_LPD_EXIT_REQ : disp power off, decon/dsim clock off, lcd on & request for LPD exit.
 * DECON_STATE_LPD : disp power off, decon/dsim clock off & lcd on
 * DECON_STATE_OFF : disp power off, decon/dsim clock off & lcd off
 */
enum decon_state {
	DECON_STATE_INIT = 0,
	DECON_STATE_ON,
	DECON_STATE_LPD_ENT_REQ,
	DECON_STATE_LPD_EXIT_REQ,
	DECON_STATE_LPD,
	DECON_STATE_OFF
};

enum decon_ip_version {
	IP_VER_DECON_7I = BIT(1),
	IP_VER_DECON_7580 = BIT(2),
};

struct exynos_decon_platdata {
	enum decon_ip_version	ip_ver;
	enum decon_psr_mode	psr_mode;
	enum decon_trig_mode	trig_mode;
	enum decon_dsi_mode	dsi_mode;
	int	max_win;
	int	default_win;
};

struct decon_vsync {
	wait_queue_head_t	wait;
	ktime_t			timestamp;
	bool			active;
	int			irq_refcount;
	struct mutex		irq_lock;
	struct task_struct	*thread;
};

/*
 * @width: The width of display in mm
 * @height: The height of display in mm
 */
struct decon_fb_videomode {
	struct fb_videomode videomode;
	unsigned short width;
	unsigned short height;

	u8 cs_setup_time;
	u8 wr_setup_time;
	u8 wr_act_time;
	u8 wr_hold_time;
	u8 auto_cmd_rate;
	u8 frame_skip:2;
	u8 rs_pol:1;
};

struct decon_fb_pd_win {
	struct decon_fb_videomode win_mode;

	unsigned short		default_bpp;
	unsigned short		max_bpp;
	unsigned short		virtual_x;
	unsigned short		virtual_y;
	unsigned short		width;
	unsigned short		height;
};

struct decon_dma_buf_data {
	struct ion_handle		*ion_handle;
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attachment;
	struct sg_table			*sg_table;
	dma_addr_t			dma_addr;
	struct sync_fence		*fence;
};

struct decon_win_rect {
	int x;
	int y;
	u32 w;
	u32 h;
};

struct decon_resources {
	struct clk *pclk;
	struct clk *aclk;
	struct clk *eclk;
	struct clk *vclk;
	struct clk *dsd;
	struct clk *lh_disp0;
	struct clk *lh_disp1;
	struct clk *aclk_disp;
	struct clk *pclk_disp;
	struct clk *mif_pll;
};

struct decon_rect {
	int	left;
	int	top;
	int	right;
	int	bottom;
};

struct decon_win {
	struct decon_fb_pd_win		windata;
	struct decon_device		*decon;
	struct fb_info			*fbinfo;
	struct media_pad		pad;

	struct decon_fb_videomode	win_mode;
	struct decon_dma_buf_data	dma_buf_data[MAX_BUF_PLANE_CNT];
	int				plane_cnt;
	struct fb_var_screeninfo	prev_var;
	struct fb_fix_screeninfo	prev_fix;

	int	fps;
	int	index;
	int	use;
	int	local;
	unsigned long	state;
	int	vpp_id;
	u32	pseudo_palette[16];
};

struct decon_user_window {
	int x;
	int y;
};

struct s3c_fb_user_plane_alpha {
	int		channel;
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
};

struct s3c_fb_user_chroma {
	int		enabled;
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
};

struct s3c_fb_user_ion_client {
	int	fd[MAX_BUF_PLANE_CNT];
	int	offset;
};

enum decon_pixel_format {
	/* RGB 32bit */
	DECON_PIXEL_FORMAT_ARGB_8888 = 0,
	DECON_PIXEL_FORMAT_ABGR_8888,
	DECON_PIXEL_FORMAT_RGBA_8888,
	DECON_PIXEL_FORMAT_BGRA_8888,
	DECON_PIXEL_FORMAT_XRGB_8888,
	DECON_PIXEL_FORMAT_XBGR_8888,
	DECON_PIXEL_FORMAT_RGBX_8888,
	DECON_PIXEL_FORMAT_BGRX_8888,
	/* RGB 16 bit */
	DECON_PIXEL_FORMAT_RGBA_5551,
	DECON_PIXEL_FORMAT_RGB_565,
	/* YUV422 2P */
	DECON_PIXEL_FORMAT_NV16,
	DECON_PIXEL_FORMAT_NV61,
	/* YUV422 3P */
	DECON_PIXEL_FORMAT_YVU422_3P,
	/* YUV420 2P */
	DECON_PIXEL_FORMAT_NV12,
	DECON_PIXEL_FORMAT_NV21,
	DECON_PIXEL_FORMAT_NV12M,
	DECON_PIXEL_FORMAT_NV21M,
	/* YUV420 3P */
	DECON_PIXEL_FORMAT_YUV420,
	DECON_PIXEL_FORMAT_YVU420,
	DECON_PIXEL_FORMAT_YUV420M,
	DECON_PIXEL_FORMAT_YVU420M,

	DECON_PIXEL_FORMAT_MAX,
};

enum decon_blending {
	DECON_BLENDING_NONE = 0,
	DECON_BLENDING_PREMULT = 1,
	DECON_BLENDING_COVERAGE = 2,
	DECON_BLENDING_MAX = 3,
};

struct exynos_hdmi_data {
	enum {
		EXYNOS_HDMI_STATE_PRESET = 0,
		EXYNOS_HDMI_STATE_ENUM_PRESET,
		EXYNOS_HDMI_STATE_CEC_ADDR,
		EXYNOS_HDMI_STATE_HDCP,
		EXYNOS_HDMI_STATE_AUDIO,
	} state;
	struct	v4l2_dv_timings timings;
	struct	v4l2_enum_dv_timings etimings;
	__u32	cec_addr;
	__u32	audio_info;
	int	hdcp;
};

enum vpp_rotate {
	VPP_ROT_NORMAL = 0x0,
	VPP_ROT_XFLIP,
	VPP_ROT_YFLIP,
	VPP_ROT_180,
	VPP_ROT_90,
	VPP_ROT_90_XFLIP,
	VPP_ROT_90_YFLIP,
	VPP_ROT_270,
};

enum vpp_csc_eq {
	BT_601_NARROW = 0x0,
	BT_601_WIDE,
	BT_709_NARROW,
	BT_709_WIDE,
};

enum vpp_stop_status {
	VPP_STOP_NORMAL = 0x0,
	VPP_STOP_LPD,
	VPP_STOP_ERR,
};

struct vpp_params {
	dma_addr_t addr[MAX_BUF_PLANE_CNT];
	enum vpp_rotate rot;
	enum vpp_csc_eq eq_mode;
};

struct decon_frame {
	int x;
	int y;
	u32 w;
	u32 h;
	u32 f_w;
	u32 f_h;
};

struct decon_win_config {
	enum {
		DECON_WIN_STATE_DISABLED = 0,
		DECON_WIN_STATE_COLOR,
		DECON_WIN_STATE_BUFFER,
		DECON_WIN_STATE_UPDATE,
	} state;

	union {
		__u32 color;
		struct {
			int				fd_idma[3];
			int				fence_fd;
			int				plane_alpha;
			enum decon_blending		blending;
			enum decon_idma_type		idma_type;
			enum decon_pixel_format		format;
			struct vpp_params		vpp_parm;
			/* no read area of IDMA */
			struct decon_win_rect		block_area;
			/* source framebuffer coordinates */
			struct decon_frame		src;
		};
	};

	/* destination OSD coordinates */
	struct decon_frame dst;
	bool protection;
};

struct decon_reg_data {
	struct list_head		list;
	u32				shadowcon;
	u32				wincon[MAX_DECON_WIN];
	u32				win_rgborder[MAX_DECON_WIN];
	u32				winmap[MAX_DECON_WIN];
	u32				vidosd_a[MAX_DECON_WIN];
	u32				vidosd_b[MAX_DECON_WIN];
	u32				vidosd_c[MAX_DECON_WIN];
	u32				vidosd_d[MAX_DECON_WIN];
	u32				vidw_alpha0[MAX_DECON_WIN];
	u32				vidw_alpha1[MAX_DECON_WIN];
	u32				blendeq[MAX_DECON_WIN - 1];
	u32				buf_start[MAX_DECON_WIN];
	struct decon_dma_buf_data	dma_buf_data[MAX_DECON_WIN][MAX_BUF_PLANE_CNT];
	struct decon_dma_buf_data	wb_dma_buf_data;
	unsigned int			bandwidth;
	unsigned int			num_of_window;
	u32				win_overlap_cnt;
	u32				bw;
	u32				disp_bw;
	u32                             int_bw;
	struct decon_rect		overlap_rect;
	u32                             offset_x[MAX_DECON_WIN];
	u32                             offset_y[MAX_DECON_WIN];
	u32                             whole_w[MAX_DECON_WIN];
	u32                             whole_h[MAX_DECON_WIN];
	u32				wb_whole_w;
	u32				wb_whole_h;
	struct decon_win_config		vpp_config[MAX_DECON_WIN];
	struct decon_win_rect		block_rect[MAX_DECON_WIN];
#ifdef CONFIG_FB_WINDOW_UPDATE
	struct decon_win_rect		update_win;
	bool				need_update;
#endif
	bool				protection[MAX_DECON_WIN];
};

struct decon_win_config_data {
	int	fence;
	int	fd_odma;
	struct decon_win_config config[MAX_DECON_WIN + 1];
};

union decon_ioctl_data {
	struct decon_user_window user_window;
	struct s3c_fb_user_plane_alpha user_alpha;
	struct s3c_fb_user_chroma user_chroma;
	struct exynos_hdmi_data hdmi_data;
	struct decon_win_config_data win_data;
	u32 vsync;
};

struct decon_underrun_stat {
	int	prev_bw;
	int	prev_int_bw;
	int	prev_disp_bw;
	int	chmap;
	int	fifo_level;
	int	underrun_cnt;
	unsigned long aclk;
	unsigned long lh_disp0;
	unsigned long mif_pll;
	unsigned long used_windows;
};

#ifdef CONFIG_DECON_EVENT_LOG
/**
 * Display Subsystem event management status.
 *
 * These status labels are used internally by the DECON to indicate the
 * current status of a device with operations.
 */
typedef enum disp_ss_event_type {
	/* Related with FB interface */
	DISP_EVT_BLANK = 0,
	DISP_EVT_UNBLANK,
	DISP_EVT_ACT_VSYNC,
	DISP_EVT_DEACT_VSYNC,
	DISP_EVT_WIN_CONFIG,

	/* Related with interrupt */
	DISP_EVT_TE_INTERRUPT,
	DISP_EVT_UNDERRUN,
	DISP_EVT_FRAMEDONE,

	/* Related with async event */
	DISP_EVT_UPDATE_HANDLER,
	DISP_EVT_DSIM_COMMAND,
	DISP_EVT_TRIG_MASK,
	DISP_EVT_DECON_FRAMEDONE_WAIT,

	/* Related with VPP */
	DISP_EVT_VPP_WINCON,
	DISP_EVT_VPP_FRAMEDONE,
	DISP_EVT_VPP_STOP,
	DISP_EVT_VPP_UPDATE_DONE,
	DISP_EVT_VPP_SHADOW_UPDATE,
	DISP_EVT_VPP_SUSPEND,
	DISP_EVT_VPP_RESUME,

	/* Related with PM */
	DISP_EVT_DECON_SUSPEND,
	DISP_EVT_DECON_RESUME,
	DISP_EVT_ENTER_LPD,
	DISP_EVT_EXIT_LPD,
	DISP_EVT_DSIM_SUSPEND,
	DISP_EVT_DSIM_RESUME,
	DISP_EVT_ENTER_ULPS,
	DISP_EVT_EXIT_ULPS,

	DISP_EVT_LINECNT_ZERO,
	DISP_EVT_TRIG_UNMASK,
	DISP_EVT_TE_WAIT_DONE,
	DISP_EVT_DECON_UPDATE_WAIT_DONE,
	DISP_EVT_PARTIAL_CMD,
	DISP_EVT_DISP_CONF,
	DISP_EVT_GIC_TE_ENABLE,
	DISP_EVT_GIC_TE_DISABLE,
	DISP_EVT_DSIM_FRAMEDONE,
	DISP_EVT_DECON_FRAMEDONE,

	/* write-back events */
	DISP_EVT_WB_SET_BUFFER,
	DISP_EVT_WB_SW_TRIGGER,
	DISP_EVT_WB_TIMELINE_INC,
	DISP_EVT_WB_FRAME_DONE,
	DISP_EVT_WB_SET_FORMAT,

	DISP_EVT_MAX, /* End of EVENT */
} disp_ss_event_t;

/* Related with PM */
struct disp_log_pm {
	u32 pm_status;		/* ACTIVE(1) or SUSPENDED(0) */
	ktime_t elapsed;	/* End time - Start time */
};

/* Related with S3CFB_WIN_CONFIG */
struct decon_update_reg_data {
	u32 bw;
	u32 int_bw;
	u32 disp_bw;
	u32 wincon[MAX_DECON_WIN];
	u32 offset_x[MAX_DECON_WIN];
	u32 offset_y[MAX_DECON_WIN];
	u32 whole_w[MAX_DECON_WIN];
	u32 whole_h[MAX_DECON_WIN];
	u32 vidosd_a[MAX_DECON_WIN];
	u32 vidosd_b[MAX_DECON_WIN];
	struct decon_win_config win_config[MAX_DECON_WIN];
	struct decon_win_rect win;
};

/* Related with MIPI COMMAND read/write */
struct dsim_log_cmd_buf {
	u32 id;
	u8 buf;
};

/* Related with VPP */
struct disp_log_vpp {
	u32 id;
	u32 start_cnt;
	u32 done_cnt;
};

/* Related with frame count */
struct disp_frame {
	u32 timeline;
	u32 timeline_max;
};

struct esd_protect {
	u32 pcd_irq;
	u32 err_irq;
	u32 disp_det_irq;
	u32 pcd_gpio;
	u32 disp_det_gpio;
	struct workqueue_struct *esd_wq;
	struct work_struct esd_work;
	u32	queuework_pending;
};

struct disp_log_decon_frm_done {
	u32 val[9];
};

/**
 * struct disp_ss_log - Display Subsystem Log
 * This struct includes DECON/DSIM/VPP
 */
struct disp_ss_log {
	ktime_t time;
	disp_ss_event_t type;
	union {
		struct disp_log_vpp vpp;
		struct decon_update_reg_data reg;
		struct dsim_log_cmd_buf cmd_buf;
		struct disp_log_pm pm;
		struct disp_log_decon_frm_done decon_sfr;
		struct v4l2_subdev_format fmt;
		struct disp_frame frame;
	} data;
};

struct disp_ss_size_info {
	u32 w_in;
	u32 h_in;
	u32 w_out;
	u32 h_out;
};

struct disp_ss_size_err_info {
	ktime_t time;
	struct disp_ss_size_info info;
};

/* Definitions below are used in the DECON */
#define	DISP_EVENT_LOG_MAX	SZ_2K
#define	DISP_EVENT_PRINT_MAX	256
#define	DISP_EVENT_SIZE_ERR_MAX	16
typedef enum disp_ss_event_log_level_type {
	DISP_EVENT_LEVEL_LOW = 0,
	DISP_EVENT_LEVEL_HIGH,
} disp_ss_log_level_t;

/* APIs below are used in the DECON/DSIM/VPP driver */
#define DISP_SS_EVENT_START() ktime_t start = ktime_get()
void DISP_SS_EVENT_LOG(disp_ss_event_t type, struct v4l2_subdev *sd, ktime_t time);
void DISP_SS_EVENT_LOG_WINCON(struct v4l2_subdev *sd, struct decon_reg_data *regs);
void DISP_SS_EVENT_LOG_S_FMT(struct v4l2_subdev *sd, struct v4l2_subdev_format *fmt);
void DISP_SS_EVENT_LOG_CMD(struct v4l2_subdev *sd, u32 cmd_id, unsigned long data);
void DISP_SS_EVENT_SHOW(struct seq_file *s, struct decon_device *decon);
void DISP_SS_EVENT_LOG_DECON_FRAMEDONE(struct v4l2_subdev *sd,
			struct disp_log_decon_frm_done* decon_sfr);
void DISP_SS_EVENT_LOG_DSIM_FRAMEDONE(struct v4l2_subdev *sd,
			struct disp_log_decon_frm_done* decon_sfr);
void DISP_SS_EVENT_SIZE_ERR_LOG(struct v4l2_subdev *sd, struct disp_ss_size_info *info);
#else /*!*/
#define DISP_SS_EVENT_START(...) do { } while(0)
#define DISP_SS_EVENT_LOG(...) do { } while(0)
#define DISP_SS_EVENT_LOG_WINCON(...) do { } while(0)
#define DISP_SS_EVENT_LOG_S_FMT(...) do { } while(0)
#define DISP_SS_EVENT_LOG_CMD(...) do { } while(0)
#define DISP_SS_EVENT_SHOW(...) do { } while(0)
#define DISP_SS_EVENT_SIZE_ERR_LOG(...) do { } while(0)
#endif

/**
* END of CONFIG_DECON_EVENT_LOG
*/

#define MAX_VPP_LOG	10

struct vpp_drm_log {
	unsigned long long time;
	int decon_id;
	int line;
	bool protection;
};

struct decon_device {
	void __iomem			*regs;
	void __iomem			*sys_regs;
	void __iomem			*eint_pend;
	void __iomem			*eint_mask;
	u32 					eint_pend_mask;
	struct device			*dev;
	struct exynos_decon_platdata	*pdata;
	struct media_pad		pads[MAX_DECON_PADS];
	struct v4l2_subdev		sd;
	struct decon_win		*windows[MAX_DECON_WIN];
	struct decon_resources		res;
	struct v4l2_subdev		*output_sd;
	struct exynos_md		*mdev;

	struct mutex			update_regs_list_lock;
	struct list_head		update_regs_list;
	struct task_struct		*update_regs_thread;
	struct kthread_worker		update_regs_worker;
	struct kthread_work		update_regs_work;
	struct mutex			lpd_lock;
	struct work_struct		lpd_work;
	struct workqueue_struct		*lpd_wq;
	atomic_t			lpd_trig_cnt;
	atomic_t			lpd_block_cnt;

	struct ion_client		*ion_client;
	struct sw_sync_timeline		*timeline;
	int				timeline_max;
	struct sw_sync_timeline		*wb_timeline;
	int				wb_timeline_max;

	struct mutex			output_lock;
	struct mutex			mutex;
	spinlock_t			slock;
	struct decon_vsync		vsync_info;
	enum decon_state		state;
	enum decon_output_type		out_type;
	int				mic_enabled;
	u32				prev_bw;
	u32				prev_disp_bw;
	u32				prev_int_bw;
	u32				prev_frame_has_yuv;
	u32				cur_frame_has_yuv;
	int				default_bw;
	int				cur_overlap_cnt;
	int				id;
	int				n_sink_pad;
	int				n_src_pad;
	union decon_ioctl_data		ioctl_data;
	bool				vpp_used[MAX_VPP_SUBDEV];
	bool				vpp_err_stat[MAX_VPP_SUBDEV];
	u32				vpp_usage_bitmask;
	struct decon_lcd		*lcd_info;
#ifdef CONFIG_FB_WINDOW_UPDATE
	struct decon_win_rect		update_win;
	bool				need_update;
#endif
	struct decon_underrun_stat	underrun_stat;
	void __iomem			*cam_status[2];
	u32				prev_protection_bitmask;
	u32				cur_protection_bitmask;
	struct decon_dma_buf_data	wb_dma_buf_data;
	atomic_t wb_done;

	unsigned int			irq;
#ifdef CONFIG_CPU_IDLE
        struct notifier_block           lpc_nb;
#endif
	struct dentry			*debug_root;
#ifdef CONFIG_DECON_EVENT_LOG
	struct dentry			*debug_event;
	struct disp_ss_log		disp_ss_log[DISP_EVENT_LOG_MAX];
	atomic_t			disp_ss_log_idx;
	disp_ss_log_level_t		disp_ss_log_level;
	u32				disp_ss_size_log_idx;
	struct disp_ss_size_err_info	disp_ss_size_log[DISP_EVENT_SIZE_ERR_MAX];
#endif
	struct pm_qos_request		int_qos;
	struct pm_qos_request		disp_qos;
	int				frame_done_cnt_cur;
	int				frame_done_cnt_target;
	wait_queue_head_t		wait_frmdone;
	ktime_t				trig_mask_timestamp;
	int                             frame_idle;
	int				eint_status;
	struct vpp_drm_log vpp_log[MAX_VPP_LOG];
	int log_cnt;
	struct decon_regs_data win_regs;
	bool	ignore_vsync;
	struct esd_protect esd;
#if defined(CONFIG_EXYNOS_DECON_MDNIE)
	struct mdnie_info *mdnie;
	struct decon_win_config_data winconfig;
#endif
	unsigned int force_fullupdate;
};

static inline struct decon_device *get_decon_drvdata(u32 id)
{
	if (id)
		return decon_ext_drvdata;
	else
		return decon_int_drvdata;
}

/* register access subroutines */
static inline u32 decon_read(u32 id, u32 reg_id)
{
	struct decon_device *decon = get_decon_drvdata(id);
	return readl(decon->regs + reg_id);
}

static inline u32 decon_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = decon_read(id, reg_id);
	val &= (~mask);
	return val;
}

static inline void decon_write(u32 id, u32 reg_id, u32 val)
{
	struct decon_device *decon = get_decon_drvdata(id);
	writel(val, decon->regs + reg_id);
}

static inline void decon_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct decon_device *decon = get_decon_drvdata(id);
	u32 old = decon_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, decon->regs + reg_id);
}

/* common function API */
bool decon_validate_x_alignment(struct decon_device *decon, int x, u32 w,
		u32 bits_per_pixel);
int find_subdev_mipi(struct decon_device *decon);
int find_subdev_hdmi(struct decon_device *decon);
int create_link_mipi(struct decon_device *decon);
int create_link_hdmi(struct decon_device *decon);
int decon_int_remap_eint(struct decon_device *decon);
int decon_int_register_irq(struct platform_device *pdev, struct decon_device *decon);
int decon_ext_register_irq(struct platform_device *pdev, struct decon_device *decon);
irqreturn_t decon_int_irq_handler(int irq, void *dev_data);
irqreturn_t decon_ext_irq_handler(int irq, void *dev_data);
int decon_int_get_clocks(struct decon_device *decon);
int decon_ext_get_clocks(struct decon_device *decon);
void decon_int_set_clocks(struct decon_device *decon);
void decon_ext_set_clocks(struct decon_device *decon);
int decon_int_register_lpd_work(struct decon_device *decon);
int decon_exit_lpd(struct decon_device *decon);
int decon_lpd_block_exit(struct decon_device *decon);
int decon_lcd_off(struct decon_device *decon);
int decon_enable(struct decon_device *decon);
int decon_disable(struct decon_device *decon);
int decon_tui_protection(struct decon_device *decon, bool tui_en);
int decon_wait_for_vsync(struct decon_device *decon, u32 timeout);

/* internal only function API */
int decon_fb_config_eint_for_te(struct platform_device *pdev, struct decon_device *decon);
int decon_int_create_vsync_thread(struct decon_device *decon);
int decon_int_create_psr_thread(struct decon_device *decon);
void decon_int_destroy_vsync_thread(struct decon_device *decon);
void decon_int_destroy_psr_thread(struct decon_device *decon);
int decon_int_set_lcd_config(struct decon_device *decon);
int decon_check_var(struct fb_var_screeninfo *var, struct fb_info *info);
int decon_pan_display(struct fb_var_screeninfo *var,
			      struct fb_info *info);

/* external only function API */
struct decon_lcd *find_porch(struct v4l2_mbus_framefmt mbus_fmt);
int decon_get_hdmi_config(struct decon_device *decon,
               struct exynos_hdmi_data *hdmi_data);
int decon_set_hdmi_config(struct decon_device *decon,
               struct exynos_hdmi_data *hdmi_data);

/* POWER and ClOCK API */
int init_display_decon_clocks(struct device *dev);
int disable_display_decon_clocks(struct device *dev);
void decon_clock_on(struct decon_device *decon);
void decon_clock_off(struct decon_device *decon);
u32 decon_reg_get_cam_status(void __iomem *);
void decon_reg_set_block_mode(u32 id, u32 win_idx, u32 x, u32 y, u32 h, u32 w, u32 enable);
void decon_reg_set_tui_va(u32 id, u32 va);

#define is_rotation(config) (config->vpp_parm.rot >= VPP_ROT_90)

/* LPD related */
static inline void decon_lpd_block(struct decon_device *decon)
{
	if (!decon->id)
		atomic_inc(&decon->lpd_block_cnt);
}

static inline bool decon_is_lpd_blocked(struct decon_device *decon)
{
	return (atomic_read(&decon->lpd_block_cnt) > 0);
}

static inline int decon_get_lpd_block_cnt(struct decon_device *decon)
{
	return (atomic_read(&decon->lpd_block_cnt));
}

static inline void decon_lpd_unblock(struct decon_device *decon)
{
	if (!decon->id) {
		if (decon_is_lpd_blocked(decon))
			atomic_dec(&decon->lpd_block_cnt);
	}
}

static inline void decon_lpd_block_reset(struct decon_device *decon)
{
	if (!decon->id)
		atomic_set(&decon->lpd_block_cnt, 0);
}

static inline void decon_lpd_trig_reset(struct decon_device *decon)
{
	if (!decon->id)
		atomic_set(&decon->lpd_trig_cnt, 0);
}

static inline bool is_cam_not_running(struct decon_device *decon)
{
	return (!((decon_reg_get_cam_status(decon->cam_status[0]) & 0xF) ||
		(decon_reg_get_cam_status(decon->cam_status[1]) & 0xF)));
}
static inline bool decon_lpd_enter_cond(struct decon_device *decon)
{
#if defined(CONFIG_LCD_ALPM) || defined(CONFIG_LCD_HMT)
	struct dsim_device *dsim = NULL;
	dsim = container_of(decon->output_sd, struct dsim_device, sd);
#endif
	return ((atomic_read(&decon->lpd_block_cnt) <= 0) && is_cam_not_running(decon)
#ifdef CONFIG_LCD_ALPM
	&& (!dsim->alpm)
#endif
#ifdef CONFIG_LCD_HMT
	&& (!dsim->priv.hmt_on)
#endif
#if defined(CONFIG_EXYNOS_DECON_MDNIE)
	&& (decon->mdnie->auto_brightness < 6)
#endif
		&& (atomic_inc_return(&decon->lpd_trig_cnt) >= DECON_ENTER_LPD_CNT));

}
static inline bool decon_min_lock_cond(struct decon_device *decon)
{
	return (atomic_read(&decon->lpd_block_cnt) <= 0);
}

static inline bool is_any_pending_frames(struct decon_device *decon)
{
	return ((decon->timeline_max - decon->timeline->value) > 1);
}

/* IOCTL commands */
#define S3CFB_WIN_POSITION		_IOW('F', 203, \
						struct decon_user_window)
#define S3CFB_WIN_SET_PLANE_ALPHA	_IOW('F', 204, \
						struct s3c_fb_user_plane_alpha)
#define S3CFB_WIN_SET_CHROMA		_IOW('F', 205, \
						struct s3c_fb_user_chroma)
#define S3CFB_SET_VSYNC_INT		_IOW('F', 206, __u32)

#define S3CFB_GET_ION_USER_HANDLE	_IOWR('F', 208, \
						struct s3c_fb_user_ion_client)
#define S3CFB_WIN_CONFIG		_IOW('F', 209, \
						struct decon_win_config_data)
#define S3CFB_WIN_PSR_EXIT		_IOW('F', 210, int)

#define EXYNOS_GET_HDMI_CONFIG		_IOW('F', 220, \
						struct exynos_hdmi_data)
#define EXYNOS_SET_HDMI_CONFIG		_IOW('F', 221, \
						struct exynos_hdmi_data)

#define DECON_IOC_LPD_EXIT_LOCK		_IOW('L', 0, u32)
#define DECON_IOC_LPD_UNLOCK		_IOW('L', 1, u32)

#endif /* ___SAMSUNG_DECON_H__ */
