/*
 * Samsung S5P Multi Format Codec V5/V6
 *
 * This file contains definitions of enums and structs used by the codec
 * driver.
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 * Kamil Debski, <k.debski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */

#ifndef S5P_MFC_COMMON_H_
#define S5P_MFC_COMMON_H_

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-fh.h>

#include <media/videobuf2-core.h>

#include <mach/exynos-mfc.h>

/*
 * CONFIG_MFC_USE_BUS_DEVFREQ might be defined in exynos-mfc.h.
 * So pm_qos.h should be checked after exynos-mfc.h
 */
#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
#include <linux/pm_qos.h>
#endif

#define MFC_MAX_BUFFERS		32
#define MFC_MAX_REF_BUFS	2
#define MFC_FRAME_PLANES	2
#define MFC_MAX_PLANES		3
#define MFC_MAX_DPBS		32
#define MFC_INFO_INIT_FD	-1

#define MFC_NUM_CONTEXTS	32
#define MFC_MAX_DRM_CTX		2
/* Interrupt timeout */
#define MFC_INT_TIMEOUT		2000
/* Interrupt short timeout */
#define MFC_INT_SHORT_TIMEOUT	800
/* Busy wait timeout */
#define MFC_BW_TIMEOUT		500
/* Watchdog interval */
#define MFC_WATCHDOG_INTERVAL   1000
/* After how many executions watchdog should assume lock up */
#define MFC_WATCHDOG_CNT        3

#define MFC_NO_INSTANCE_SET	-1

#define MFC_ENC_CAP_PLANE_COUNT	1
#define MFC_ENC_OUT_PLANE_COUNT	2

#define MFC_NAME_LEN		16
#define MFC_FW_NAME		"mfc_fw.bin"

#define STUFF_BYTE		4
#define MFC_WORKQUEUE_LEN	32

#define MFC_BASE_MASK		((1 << 17) - 1)

#define FLAG_LAST_FRAME		0x80000000
#define MFC_MAX_INTERVAL	(2 * USEC_PER_SEC)

/* Maximum number of temporal layers */
#define VIDEO_MAX_TEMPORAL_LAYERS 7

/*
 *  MFC region id for smc
 */
enum {
	FC_MFC_EXYNOS_ID_MFC_SH        = 0,
	FC_MFC_EXYNOS_ID_VIDEO         = 1,
	FC_MFC_EXYNOS_ID_MFC_FW        = 2,
	FC_MFC_EXYNOS_ID_SECTBL        = 3,
	FC_MFC_EXYNOS_ID_G2D_WFD       = 4,
	FC_MFC_EXYNOS_ID_MFC_NFW       = 5,
	FC_MFC_EXYNOS_ID_VIDEO_EXT     = 6,
};

#define SMC_FC_ID_MFC_SH(id)		((id) * 10 + FC_MFC_EXYNOS_ID_MFC_SH)
#define SMC_FC_ID_VIDEO(id)		((id) * 10 + FC_MFC_EXYNOS_ID_VIDEO)
#define SMC_FC_ID_MFC_FW(id)		((id) * 10 + FC_MFC_EXYNOS_ID_MFC_FW)
#define SMC_FC_ID_SECTBL(id)		((id) * 10 + FC_MFC_EXYNOS_ID_SECTBL)
#define SMC_FC_ID_G2D_WFD(id)		((id) * 10 + FC_MFC_EXYNOS_ID_G2D_WFD)
#define SMC_FC_ID_MFC_NFW(id)		((id) * 10 + FC_MFC_EXYNOS_ID_MFC_NFW)
#define SMC_FC_ID_VIDEO_EXT(id)		((id) * 10 + FC_MFC_EXYNOS_ID_VIDEO_EXT)

/**
 * enum s5p_mfc_inst_type - The type of an MFC device node.
 */
enum s5p_mfc_node_type {
	MFCNODE_INVALID = -1,
	MFCNODE_DECODER = 0,
	MFCNODE_ENCODER = 1,
	MFCNODE_DECODER_DRM = 2,
	MFCNODE_ENCODER_DRM = 3,
};

/**
 * enum s5p_mfc_inst_type - The type of an MFC instance.
 */
enum s5p_mfc_inst_type {
	MFCINST_INVALID = 0,
	MFCINST_DECODER = 1,
	MFCINST_ENCODER = 2,
};

/**
 * enum s5p_mfc_inst_state - The state of an MFC instance.
 */
enum s5p_mfc_inst_state {
	MFCINST_FREE = 0,
	MFCINST_INIT = 100,
	MFCINST_GOT_INST,
	MFCINST_HEAD_PARSED,
	MFCINST_BUFS_SET,
	MFCINST_RUNNING,
	MFCINST_FINISHING,
	MFCINST_FINISHED,
	MFCINST_RETURN_INST,
	MFCINST_ERROR,
	MFCINST_ABORT,
	MFCINST_RES_CHANGE_INIT,
	MFCINST_RES_CHANGE_FLUSH,
	MFCINST_RES_CHANGE_END,
	MFCINST_RUNNING_NO_OUTPUT,
	MFCINST_ABORT_INST,
	MFCINST_DPB_FLUSHING,
	MFCINST_VPS_PARSED_ONLY,
};

/**
 * enum s5p_mfc_queue_state - The state of buffer queue.
 */
enum s5p_mfc_queue_state {
	QUEUE_FREE = 0,
	QUEUE_BUFS_REQUESTED,
	QUEUE_BUFS_QUERIED,
	QUEUE_BUFS_MMAPED,
};

enum mfc_dec_wait_state {
	WAIT_NONE = 0,
	WAIT_DECODING,
	WAIT_INITBUF_DONE,
};

/**
 * enum s5p_mfc_check_state - The state for user notification
 */
enum s5p_mfc_check_state {
	MFCSTATE_PROCESSING = 0,
	MFCSTATE_DEC_RES_DETECT,
	MFCSTATE_DEC_TERMINATING,
	MFCSTATE_ENC_NO_OUTPUT,
	MFCSTATE_DEC_S3D_REALLOC,
};

/**
 * enum s5p_mfc_buf_cacheable_mask - The mask for cacheble setting
 */
enum s5p_mfc_buf_cacheable_mask {
	MFCMASK_DST_CACHE = (1 << 0),
	MFCMASK_SRC_CACHE = (1 << 1),
};

enum mfc_buf_usage_type {
	MFCBUF_INVALID = 0,
	MFCBUF_NORMAL,
	MFCBUF_DRM,
};

enum mfc_buf_process_type {
	MFCBUFPROC_DEFAULT 		= 0x0,
	MFCBUFPROC_COPY 		= (1 << 0),
	MFCBUFPROC_SHARE 		= (1 << 1),
	MFCBUFPROC_META 		= (1 << 2),
	MFCBUFPROC_ANBSHARE		= (1 << 3),
	MFCBUFPROC_ANBSHARE_NV12L	= (1 << 4),
};

struct s5p_mfc_ctx;
struct s5p_mfc_extra_buf;
struct s5p_mfc_dev;

#define MFC_DEV_NUM_MAX			2
#define MFC_TRACE_STR_LEN		80
#define MFC_TRACE_COUNT_MAX		1024
struct _mfc_trace {
	unsigned long long time;
	char str[MFC_TRACE_STR_LEN];
};

#define MFC_TRACE_DEV(fmt, args...)							\
	do {											\
		int cpu = raw_smp_processor_id();						\
		int cnt;									\
		cnt = atomic_inc_return(&dev->trace_ref) & (MFC_TRACE_COUNT_MAX - 1);	\
		dev->mfc_trace[cnt].time = cpu_clock(cpu);					\
		snprintf(dev->mfc_trace[cnt].str, MFC_TRACE_STR_LEN,			\
				"[d:%d] " fmt, dev->id, ##args);				\
	} while(0)

#define MFC_TRACE_CTX(fmt, args...)							\
	do {											\
		int cpu = raw_smp_processor_id();						\
		int cnt;									\
		cnt = atomic_inc_return(&dev->trace_ref) & (MFC_TRACE_COUNT_MAX - 1);	\
		dev->mfc_trace[cnt].time = cpu_clock(cpu);					\
		snprintf(dev->mfc_trace[cnt].str, MFC_TRACE_STR_LEN,			\
				"[d:%d, c:%d] " fmt, ctx->dev->id, ctx->num, ##args);		\
	} while(0)

/**
 * struct s5p_mfc_buf - MFC buffer
 *
 */
struct s5p_mfc_buf {
	struct vb2_buffer vb;
	struct list_head list;
	union {
		dma_addr_t raw[3];
		dma_addr_t stream;
	} planes;
	int used;
	int already;
	int consumed;
};

#define vb_to_mfc_buf(x)	\
	container_of(x, struct s5p_mfc_buf, vb)

struct s5p_mfc_pm {
	struct clk	*clock;
	atomic_t	power;
	struct device	*device;
	spinlock_t	clklock;

	int clock_on_steps;
	int clock_off_steps;
	enum mfc_buf_usage_type base_type;
};

struct s5p_mfc_fw {
	const struct firmware	*info;
	int			state;
	int			date;
};

struct s5p_mfc_buf_align {
	unsigned int mfc_base_align;
};

struct s5p_mfc_buf_size_v5 {
	unsigned int h264_ctx_buf;
	unsigned int non_h264_ctx_buf;
	unsigned int desc_buf;
	unsigned int shared_buf;
};

struct s5p_mfc_buf_size_v6 {
	unsigned int dev_ctx;
	unsigned int h264_dec_ctx;
	unsigned int other_dec_ctx;
	unsigned int h264_enc_ctx;
	unsigned int hevc_enc_ctx;
	unsigned int other_enc_ctx;
};

struct s5p_mfc_buf_size {
	size_t firmware_code;
	unsigned int cpb_buf;
	void *buf;
};

struct s5p_mfc_variant {
	struct s5p_mfc_buf_size *buf_size;
	struct s5p_mfc_buf_align *buf_align;
	int	num_entities;
};

/**
 * struct s5p_mfc_extra_buf - represents internal used buffer
 * @alloc:		allocation-specific contexts for each buffer
 *			(videobuf2 allocator)
 * @ofs:		offset of each buffer, will be used for MFC
 * @virt:		kernel virtual address, only valid when the
 *			buffer accessed by driver
 * @dma:		DMA address, only valid when kernel DMA API used
 */
struct s5p_mfc_extra_buf {
	void		*alloc;
	unsigned long	ofs;
	void		*virt;
	dma_addr_t	dma;
};

#define OTO_BUF_FW		(1 << 0)
#define OTO_BUF_COMMON_CTX	(1 << 1)
/**
 * struct s5p_mfc_dev - The struct containing driver internal parameters.
 */
struct s5p_mfc_dev {
	struct v4l2_device	v4l2_dev;
	struct video_device	*vfd_dec;
	struct video_device	*vfd_enc;
	struct video_device	*vfd_dec_drm;
	struct video_device	*vfd_enc_drm;
	struct device		*device;
#ifdef CONFIG_ION_EXYNOS
	struct ion_client	*mfc_ion_client;
#endif

	void __iomem		*regs_base;
	int			irq;
	struct resource		*mfc_mem;

	struct s5p_mfc_pm	pm;
	struct s5p_mfc_fw	fw;
	struct s5p_mfc_variant	*variant;
	struct s5p_mfc_platdata	*pdata;

	int num_inst;
	spinlock_t irqlock;
	spinlock_t condlock;

	struct mutex mfc_mutex;

	int int_cond;
	int int_type;
	unsigned int int_err;
	wait_queue_head_t queue;

	size_t port_a;
	size_t port_b;

	unsigned long hw_lock;

	/*
	struct clk *clock1;
	struct clk *clock2;
	*/

	/* For 6.x, Added for SYS_INIT context buffer */
	struct s5p_mfc_extra_buf ctx_buf;
	struct s5p_mfc_extra_buf dis_shm_buf;
	struct s5p_mfc_extra_buf ctx_buf_drm;
	struct s5p_mfc_extra_buf dis_shm_buf_drm;

	struct s5p_mfc_ctx *ctx[MFC_NUM_CONTEXTS];
	int curr_ctx;
	int preempt_ctx;
	unsigned long ctx_work_bits;

	atomic_t watchdog_cnt;
	atomic_t watchdog_run;
	struct timer_list watchdog_timer;
	struct workqueue_struct *watchdog_wq;
	struct work_struct watchdog_work;

	struct vb2_alloc_ctx **alloc_ctx;

	unsigned long clk_state;

	/* for DRM */
	int curr_ctx_drm;
	int fw_status;
	int num_drm_inst;
	struct s5p_mfc_extra_buf drm_info;
	struct s5p_mfc_extra_buf fw_info;
	struct s5p_mfc_extra_buf drm_fw_info;
	struct vb2_alloc_ctx *alloc_ctx_fw;
	struct vb2_alloc_ctx *alloc_ctx_drm_fw;
	struct vb2_alloc_ctx *alloc_ctx_sh;
	struct vb2_alloc_ctx *alloc_ctx_drm;

	struct workqueue_struct *sched_wq;
	struct work_struct sched_work;

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	struct list_head qos_queue;
	atomic_t qos_req_cur;
	atomic_t *qos_req_cnt;
	struct pm_qos_request qos_req_int;
	struct pm_qos_request qos_req_mif;
	struct pm_qos_request qos_req_cluster1;
	struct pm_qos_request qos_req_cluster0;

	/* for direct clock control */
	int min_rate;
	int curr_rate;
#endif
	int id;
	atomic_t clk_ref;
	int fw_size;
	int drm_fw_status;
	int is_support_smc;
	int sys_init_status;
	int wakeup_status;

	atomic_t trace_ref;
	struct _mfc_trace *mfc_trace;

	struct mutex curr_rate_lock;
	int has_enc_ctx;
};

/**
 *
 */
struct s5p_mfc_h264_enc_params {
	enum v4l2_mpeg_video_h264_profile profile;
	u8 level;
	u8 interlace;
	enum v4l2_mpeg_video_h264_loop_filter_mode loop_filter_mode;
	s8 loop_filter_alpha;
	s8 loop_filter_beta;
	enum v4l2_mpeg_video_h264_entropy_mode entropy_mode;
	u8 num_ref_pic_4p;
	u8 _8x8_transform;
	u32 rc_framerate;
	u8 rc_frame_qp;
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_mb_dark;
	u8 rc_mb_smooth;
	u8 rc_mb_static;
	u8 rc_mb_activity;
	u8 rc_p_frame_qp;
	u8 rc_b_frame_qp;
	u8 ar_vui;
	enum v4l2_mpeg_video_h264_vui_sar_idc ar_vui_idc;
	u16 ext_sar_width;
	u16 ext_sar_height;
	u8 open_gop;
	u16 open_gop_size;
	u8 hier_qp;
	enum v4l2_mpeg_video_h264_hierarchical_coding_type hier_qp_type;
	u8 hier_qp_layer;
	u8 hier_qp_layer_qp[7];
	u32 hier_qp_layer_bit[7];
	u8 sei_gen_enable;
	u8 sei_fp_curr_frame_0;
	enum v4l2_mpeg_video_h264_sei_fp_arrangement_type sei_fp_arrangement_type;
	u32 fmo_enable;
	u32 fmo_slice_map_type;
	u32 fmo_slice_num_grp;
	u32 fmo_run_length[4];
	u32 fmo_sg_dir;
	u32 fmo_sg_rate;
	u32 aso_enable;
	u32 aso_slice_order[8];

	u32 prepend_sps_pps_to_idr;
};

/**
 *
 */
struct s5p_mfc_mpeg4_enc_params {
	/* MPEG4 Only */
	enum v4l2_mpeg_video_mpeg4_profile profile;
	u8 level;
	u8 quarter_pixel; /* MFC5.x */
	u16 vop_time_res;
	u16 vop_frm_delta;
	u8 rc_b_frame_qp;
	/* Common for MPEG4, H263 */
	u32 rc_framerate;
	u8 rc_frame_qp;
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_p_frame_qp;
};

/**
 *
 */
struct s5p_mfc_vp8_enc_params {
	/* VP8 Only */
	u32 rc_framerate;
	u8 vp8_version;
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_frame_qp;
	u8 rc_p_frame_qp;
	u8 vp8_numberofpartitions;
	u8 vp8_filterlevel;
	u8 vp8_filtersharpness;
	u8 vp8_goldenframesel;
	u8 vp8_gfrefreshperiod;
	u8 hierarchy_qp_enable;
	u8 hier_qp_layer_qp[3];
	u32 hier_qp_layer_bit[3];
	u8 num_refs_for_p;
	u8 intra_4x4mode_disable;
	u8 num_temporal_layer;
};

/**
 *
 */
struct s5p_mfc_hevc_enc_params {
	u8 level;
	u8 tier_flag;
	/* HEVC Only */
	u32 rc_framerate;
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_lcu_dark;
	u8 rc_lcu_smooth;
	u8 rc_lcu_static;
	u8 rc_lcu_activity;
	u8 rc_frame_qp;
	u8 rc_p_frame_qp;
	u8 rc_b_frame_qp;
	u8 max_partition_depth;
	u8 num_refs_for_p;
	u8 refreshtype;
	u8 refreshperiod;
	s8 croma_qp_offset_cr;
	s8 croma_qp_offset_cb;
	s8 lf_beta_offset_div2;
	s8 lf_tc_offset_div2;
	u8 loopfilter_disable;
	u8 loopfilter_across;
	u8 nal_control_length_filed;
	u8 nal_control_user_ref;
	u8 nal_control_store_ref;
	u8 const_intra_period_enable;
	u8 lossless_cu_enable;
	u8 wavefront_enable;
	u8 longterm_ref_enable;
	u8 hier_qp;
	u8 hier_qp_type;
	u8 hier_qp_layer;
	u8 hier_qp_layer_qp;
	u8 hier_qp_layer_bit;
	u8 sign_data_hiding;
	u8 general_pb_enable;
	u8 temporal_id_enable;
	u8 strong_intra_smooth;
	u8 intra_pu_split_disable;
	u8 tmv_prediction_disable;
	u8 max_num_merge_mv;
	u8 eco_mode_enable;
	u8 encoding_nostartcode_enable;
	u8 size_of_length_field;
	u8 user_ref;
	u8 store_ref;
};

/**
 *
 */
struct s5p_mfc_enc_params {
	u16 width;
	u16 height;

	u16 gop_size;
	enum v4l2_mpeg_video_multi_slice_mode slice_mode;
	u16 slice_mb;
	u32 slice_bit;
	u16 intra_refresh_mb;
	u8 pad;
	u8 pad_luma;
	u8 pad_cb;
	u8 pad_cr;
	u8 rc_frame;
	u32 rc_bitrate;
	u16 rc_reaction_coeff;
	u8 frame_tag;

	u8 num_b_frame;		/* H.264/MPEG4 */
	u8 rc_mb;		/* H.264: MFCv5, MPEG4/H.263: MFCv6 */
	u16 vbv_buf_size;
	enum v4l2_mpeg_video_header_mode seq_hdr_mode;
	enum v4l2_mpeg_mfc51_video_frame_skip_mode frame_skip_mode;
	u8 fixed_target_bit;

	u16 rc_frame_delta;	/* MFC6.1 Only */

	union {
		struct s5p_mfc_h264_enc_params h264;
		struct s5p_mfc_mpeg4_enc_params mpeg4;
		struct s5p_mfc_vp8_enc_params vp8;
		struct s5p_mfc_hevc_enc_params hevc;
	} codec;
};

enum s5p_mfc_ctrl_type {
	MFC_CTRL_TYPE_GET_SRC	= 0x1,
	MFC_CTRL_TYPE_GET_DST	= 0x2,
	MFC_CTRL_TYPE_SET	= 0x4,
};

#define	MFC_CTRL_TYPE_GET	(MFC_CTRL_TYPE_GET_SRC | MFC_CTRL_TYPE_GET_DST)
#define	MFC_CTRL_TYPE_SRC	(MFC_CTRL_TYPE_SET | MFC_CTRL_TYPE_GET_SRC)
#define	MFC_CTRL_TYPE_DST	(MFC_CTRL_TYPE_GET_DST)

enum s5p_mfc_ctrl_mode {
	MFC_CTRL_MODE_NONE	= 0x0,
	MFC_CTRL_MODE_SFR	= 0x1,
	MFC_CTRL_MODE_SHM	= 0x2,
	MFC_CTRL_MODE_CST	= 0x4,
};

struct s5p_mfc_buf_ctrl;

struct s5p_mfc_ctrl_cfg {
	enum s5p_mfc_ctrl_type type;
	unsigned int id;
	unsigned int is_volatile;	/* only for MFC_CTRL_TYPE_SET */
	unsigned int mode;
	unsigned int addr;
	unsigned int mask;
	unsigned int shft;
	unsigned int flag_mode;		/* only for MFC_CTRL_TYPE_SET */
	unsigned int flag_addr;		/* only for MFC_CTRL_TYPE_SET */
	unsigned int flag_shft;		/* only for MFC_CTRL_TYPE_SET */
	int (*read_cst) (struct s5p_mfc_ctx *ctx,
			struct s5p_mfc_buf_ctrl *buf_ctrl);
	void (*write_cst) (struct s5p_mfc_ctx *ctx,
			struct s5p_mfc_buf_ctrl *buf_ctrl);
};

struct s5p_mfc_ctx_ctrl {
	struct list_head list;
	enum s5p_mfc_ctrl_type type;
	unsigned int id;
	unsigned int addr;
	int has_new;
	int val;
};

struct s5p_mfc_buf_ctrl {
	struct list_head list;
	unsigned int id;
	enum s5p_mfc_ctrl_type type;
	int has_new;
	int val;
	unsigned int old_val;		/* only for MFC_CTRL_TYPE_SET */
	unsigned int is_volatile;	/* only for MFC_CTRL_TYPE_SET */
	unsigned int updated;
	unsigned int mode;
	unsigned int addr;
	unsigned int mask;
	unsigned int shft;
	unsigned int flag_mode;		/* only for MFC_CTRL_TYPE_SET */
	unsigned int flag_addr;		/* only for MFC_CTRL_TYPE_SET */
	unsigned int flag_shft;		/* only for MFC_CTRL_TYPE_SET */
	int (*read_cst) (struct s5p_mfc_ctx *ctx,
			struct s5p_mfc_buf_ctrl *buf_ctrl);
	void (*write_cst) (struct s5p_mfc_ctx *ctx,
			struct s5p_mfc_buf_ctrl *buf_ctrl);
};

#define call_bop(b, op, args...)	\
	(b->op ? b->op(args) : 0)

struct s5p_mfc_codec_ops {
	/* initialization routines */
	int (*alloc_ctx_buf) (struct s5p_mfc_ctx *ctx);
	int (*alloc_desc_buf) (struct s5p_mfc_ctx *ctx);
	int (*get_init_arg) (struct s5p_mfc_ctx *ctx, void *arg);
	int (*pre_seq_start) (struct s5p_mfc_ctx *ctx);
	int (*post_seq_start) (struct s5p_mfc_ctx *ctx);
	int (*set_init_arg) (struct s5p_mfc_ctx *ctx, void *arg);
	int (*set_codec_bufs) (struct s5p_mfc_ctx *ctx);
	int (*set_dpbs) (struct s5p_mfc_ctx *ctx);		/* decoder */
	/* execution routines */
	int (*get_exe_arg) (struct s5p_mfc_ctx *ctx, void *arg);
	int (*pre_frame_start) (struct s5p_mfc_ctx *ctx);
	int (*post_frame_start) (struct s5p_mfc_ctx *ctx);
	int (*multi_data_frame) (struct s5p_mfc_ctx *ctx);
	int (*set_exe_arg) (struct s5p_mfc_ctx *ctx, void *arg);
	/* configuration routines */
	int (*get_codec_cfg) (struct s5p_mfc_ctx *ctx, unsigned int type,
			int *value);
	int (*set_codec_cfg) (struct s5p_mfc_ctx *ctx, unsigned int type,
			int *value);
	/* controls per buffer */
	int (*init_ctx_ctrls) (struct s5p_mfc_ctx *ctx);
	int (*cleanup_ctx_ctrls) (struct s5p_mfc_ctx *ctx);
	int (*init_buf_ctrls) (struct s5p_mfc_ctx *ctx,
			enum s5p_mfc_ctrl_type type, unsigned int index);
	int (*cleanup_buf_ctrls) (struct s5p_mfc_ctx *ctx,
			enum s5p_mfc_ctrl_type type, unsigned int index);
	int (*to_buf_ctrls) (struct s5p_mfc_ctx *ctx, struct list_head *head);
	int (*to_ctx_ctrls) (struct s5p_mfc_ctx *ctx, struct list_head *head);
	int (*set_buf_ctrls_val) (struct s5p_mfc_ctx *ctx,
			struct list_head *head);
	int (*get_buf_ctrls_val) (struct s5p_mfc_ctx *ctx,
			struct list_head *head);
	int (*recover_buf_ctrls_val) (struct s5p_mfc_ctx *ctx,
			struct list_head *head);
	int (*get_buf_update_val) (struct s5p_mfc_ctx *ctx,
			struct list_head *head, unsigned int id, int value);
};

#define call_cop(c, op, args...)				\
	(((c)->c_ops->op) ?					\
		((c)->c_ops->op(args)) : 0)

struct stored_dpb_info {
	int fd[MFC_MAX_PLANES];
};

struct dec_dpb_ref_info {
	int index;
	struct stored_dpb_info dpb[MFC_MAX_DPBS];
};

struct temporal_layer_info {
	unsigned int temporal_layer_count;
	unsigned int temporal_layer_bitrate[VIDEO_MAX_TEMPORAL_LAYERS];
};

struct mfc_user_shared_handle {
	int fd;
	struct ion_handle *ion_handle;
	void *virt;
};

struct s5p_mfc_raw_info {
	int num_planes;
	int stride[3];
	int plane_size[3];
};

#define MFC_TIME_INDEX		8
struct mfc_timestamp {
	struct list_head list;
	struct timeval timestamp;
	int index;
	int interval;
};

struct s5p_mfc_dec {
	int total_dpb_count;

	struct list_head dpb_queue;
	unsigned int dpb_queue_cnt;

	size_t src_buf_size;

	int loop_filter_mpeg4;
	int display_delay;
	int immediate_display;
	int is_packedpb;
	int slice_enable;
	int mv_count;
	int idr_decoding;
	int is_interlaced;
	int is_dts_mode;

	int crc_enable;
	int crc_luma0;
	int crc_chroma0;
	int crc_luma1;
	int crc_chroma1;

	struct s5p_mfc_extra_buf dsc;
	unsigned long consumed;
	unsigned long remained_size;
	unsigned long dpb_status;
	unsigned int dpb_flush;

	enum v4l2_memory dst_memtype;
	int sei_parse;
	int stored_tag;
	dma_addr_t y_addr_for_pb;

	int internal_dpb;
	int cr_left, cr_right, cr_top, cr_bot;

	/* For 6.x */
	int remained;

	/* For 7.x */
	int is_dual_dpb;
	int tiled_buf_cnt;
	struct s5p_mfc_raw_info tiled_ref;

	/* For dynamic DPB */
	int is_dynamic_dpb;
	unsigned int dynamic_set;
	unsigned int dynamic_used;
	struct list_head ref_queue;
	unsigned int ref_queue_cnt;
	struct dec_dpb_ref_info *ref_info;
	int assigned_fd[MFC_MAX_DPBS];
	struct mfc_user_shared_handle sh_handle;

	int dynamic_ref_filled;
};

struct s5p_mfc_enc {
	struct s5p_mfc_enc_params params;

	size_t dst_buf_size;

	int frame_count;
	enum v4l2_mpeg_mfc51_video_frame_type frame_type;
	enum v4l2_mpeg_mfc51_video_force_frame_type force_frame_type;

	struct list_head ref_queue;
	unsigned int ref_queue_cnt;

	/* For 6.x */
	size_t luma_dpb_size;
	size_t chroma_dpb_size;
	size_t me_buffer_size;
	size_t tmv_buffer_size;

	unsigned int slice_mode;
	union {
		unsigned int mb;
		unsigned int bits;
	} slice_size;
	unsigned int in_slice;

	int stored_tag;
	struct mfc_user_shared_handle sh_handle;
};

/**
 * struct s5p_mfc_ctx - This struct contains the instance context
 */
struct s5p_mfc_ctx {
	struct s5p_mfc_dev *dev;
	struct v4l2_fh fh;
	int num;

	int int_cond;
	int int_type;
	unsigned int int_err;
	wait_queue_head_t queue;

	struct s5p_mfc_fmt *src_fmt;
	struct s5p_mfc_fmt *dst_fmt;

	struct vb2_queue vq_src;
	struct vb2_queue vq_dst;

	struct list_head src_queue;
	struct list_head dst_queue;

	unsigned int src_queue_cnt;
	unsigned int dst_queue_cnt;

	enum s5p_mfc_inst_type type;
	enum s5p_mfc_inst_state state;
	int inst_no;

	int img_width;
	int img_height;
	int buf_width;
	int buf_height;
	int dpb_count;
	int buf_stride;

	struct s5p_mfc_raw_info raw_buf;
	int mv_size;

	/* Buffers */
	void *port_a_buf;
	size_t port_a_phys;
	size_t port_a_size;

	void *port_b_buf;
	size_t port_b_phys;
	size_t port_b_size;

	enum s5p_mfc_queue_state capture_state;
	enum s5p_mfc_queue_state output_state;

	struct list_head ctrls;

	struct list_head src_ctrls[MFC_MAX_BUFFERS];
	struct list_head dst_ctrls[MFC_MAX_BUFFERS];

	unsigned long src_ctrls_avail;
	unsigned long dst_ctrls_avail;

	unsigned int sequence;

	/* Control values */
	int codec_mode;
	__u32 pix_format;
	int cacheable;

	/* Extra Buffers */
	unsigned int ctx_buf_size;
	struct s5p_mfc_extra_buf ctx;
	struct s5p_mfc_extra_buf shm;

	struct s5p_mfc_dec *dec_priv;
	struct s5p_mfc_enc *enc_priv;

	struct s5p_mfc_codec_ops *c_ops;

	/* For 6.x */
	size_t scratch_buf_size;

	/* for DRM */
	int is_drm;

	int is_dpb_realloc;
	enum mfc_dec_wait_state wait_state;

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	int qos_req_step;
	struct list_head qos_list;
#endif
	int qos_ratio;
	int framerate;
	int last_framerate;
	int avg_framerate;
	int frame_count;
	struct timeval last_timestamp;
	int qp_min_change;
	int qp_max_change;

	int is_max_fps;
	int buf_process_type;

	struct mfc_timestamp ts_array[MFC_TIME_INDEX];
	struct list_head ts_list;
	int ts_count;
	int ts_is_full;
};

#define fh_to_mfc_ctx(x)	\
	container_of(x, struct s5p_mfc_ctx, fh)

#define MFC_FMT_DEC	0
#define MFC_FMT_ENC	1
#define MFC_FMT_RAW	2

static inline unsigned int mfc_linear_buf_size(unsigned int version)
{
	unsigned int size = 0;

	switch (version) {
	case 0x51:
	case 0x61:
	case 0x65:
		size = 0;
		break;
	case 0x72:
	case 0x723:
	case 0x80:
	case 0x90:
		size = 256;
		break;
	default:
		size = 0;
		break;
	}

	return size;
}

static inline unsigned int mfc_version(struct s5p_mfc_dev *dev)
{
	unsigned int version = 0;

	switch (dev->pdata->ip_ver) {
	case IP_VER_MFC_4P_0:
	case IP_VER_MFC_4P_1:
	case IP_VER_MFC_4P_2:
		version = 0x51;
		break;
	case IP_VER_MFC_5G_0:
		version = 0x61;
		break;
	case IP_VER_MFC_5G_1:
	case IP_VER_MFC_5A_0:
	case IP_VER_MFC_5A_1:
		version = 0x65;
		break;
	case IP_VER_MFC_6A_0:
	case IP_VER_MFC_6A_1:
		version = 0x72;
		break;
	case IP_VER_MFC_6A_2:
		version = 0x723;
		break;
	case IP_VER_MFC_7A_0:
		version = 0x80;
		break;
	case IP_VER_MFC_8I_0:
		version = 0x90;
		break;
	case IP_VER_MFC_6I_0:
		version = 0x78;
		break;
	}

	return version;
}
#define MFC_VER_MAJOR(dev)	((mfc_version(dev) >> 4) & 0xF)
#define MFC_VER_MINOR(dev)	(mfc_version(dev) & 0xF)

#define IS_TWOPORT(dev)		(mfc_version(dev) == 0x51)
#define NUM_OF_PORT(dev)	(IS_TWOPORT(dev) ? 2 : 1)
#define NUM_OF_ALLOC_CTX(dev)	(NUM_OF_PORT(dev) + 1)
/*
 * Version Description
 *
 * IS_MFCv5X : For MFC v5.X only
 * IS_MFCv6X : For MFC v6.X only
 * IS_MFCv7X : For MFC v7.X only
 * IS_MFCv8X : For MFC v8.X only
 * IS_MFCv9X : For MFC v9.X only
 * IS_MFCv78 : For MFC v7.8 only
 * IS_MFCV6 : For MFC v6 architecure
 * IS_MFCV8 : For MFC v8 architecure
 */
#define IS_MFCv5X(dev)		(mfc_version(dev) == 0x51)
#define IS_MFCv6X(dev)		((mfc_version(dev) == 0x61) || \
				 (mfc_version(dev) == 0x65))
#define IS_MFCv7X(dev)		((mfc_version(dev) == 0x72) || \
				 (mfc_version(dev) == 0x723) || \
				 (mfc_version(dev) == 0x78))
#define IS_MFCv8X(dev)		(mfc_version(dev) == 0x80)
#define IS_MFCv9X(dev)		(mfc_version(dev) == 0x90)
#define IS_MFCv78(dev)		(mfc_version(dev) == 0x78)
#define IS_MFCV6(dev)		(IS_MFCv6X(dev) || IS_MFCv7X(dev) ||	\
				IS_MFCv8X(dev) || IS_MFCv9X(dev))
#define IS_MFCV8(dev)		(IS_MFCv8X(dev) || IS_MFCv9X(dev))

/* supported feature macros by F/W version */
#define FW_HAS_BUS_RESET(dev)		(dev->fw.date >= 0x120206)
#define FW_HAS_VER_INFO(dev)		(dev->fw.date >= 0x120328)
#define FW_HAS_INITBUF_TILE_MODE(dev)	(dev->fw.date >= 0x120629)
#define FW_HAS_INITBUF_LOOP_FILTER(dev)	(dev->fw.date >= 0x120831)
#define FW_HAS_SEI_S3D_REALLOC(dev)	(IS_MFCV6(dev) &&	\
					(dev->fw.date >= 0x121205))
#define FW_HAS_ENC_SPSPPS_CTRL(dev)	((IS_MFCV6(dev) &&		\
					(dev->fw.date >= 0x121005)) ||	\
					(IS_MFCv5X(dev) &&		\
					(dev->fw.date >= 0x120823)))
#define FW_HAS_VUI_PARAMS(dev)		(IS_MFCV6(dev) &&		\
					(dev->fw.date >= 0x121214))
#define FW_HAS_ADV_RC_MODE(dev)		(IS_MFCV6(dev) &&		\
					(dev->fw.date >= 0x130329))
#define FW_HAS_I_LIMIT_RC_MODE(dev)	((IS_MFCv7X(dev) &&		\
					(dev->fw.date >= 0x140801)) ||	\
					(IS_MFCv8X(dev) &&		\
					(dev->fw.date >= 0x140808)) ||	\
					(IS_MFCv9X(dev) &&		\
					(dev->fw.date >= 0x141008)))
#define FW_HAS_POC_TYPE_CTRL(dev)	(IS_MFCV6(dev) &&		\
					(dev->fw.date >= 0x130405))
#define FW_HAS_DYNAMIC_DPB(dev)		((IS_MFCv7X(dev) || IS_MFCV8(dev))&&	\
					(dev->fw.date >= 0x131108))
#define FW_HAS_NOT_CODED(dev)		(IS_MFCV8(dev) &&		\
					(dev->fw.date >= 0x140926))
#define FW_HAS_BASE_CHANGE(dev)		((IS_MFCv7X(dev) || IS_MFCV8(dev))&&	\
					(dev->fw.date >= 0x131108))
#define FW_HAS_TEMPORAL_SVC_CH(dev)	((IS_MFCv8X(dev) &&			\
					(dev->fw.date >= 0x140821)) ||		\
					(IS_MFCv9X(dev) &&			\
					(dev->fw.date >= 0x141008)))
#define FW_WAKEUP_AFTER_RISC_ON(dev)	(IS_MFCV8(dev) || IS_MFCv78(dev))

#define FW_NEED_SHARED_MEMORY(dev)	(IS_MFCv5X(dev) || IS_MFCv6X(dev) ||	\
					IS_MFCv7X(dev) || IS_MFCv78(dev))
#if 0		/* Do not use last frame info */
#define FW_HAS_LAST_DISP_INFO(dev)	(IS_MFCv9X(dev) &&			\
 					(dev->fw.date >= 0x141205))
#else
#define FW_HAS_LAST_DISP_INFO(dev)	0
#endif

#define HW_LOCK_CLEAR_MASK		(0xFFFFFFFF)

#define is_h264(ctx)		((ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC) ||\
				(ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC))
#define is_mpeg4vc1(ctx)	((ctx->codec_mode == S5P_FIMV_CODEC_VC1RCV_DEC) ||\
				(ctx->codec_mode == S5P_FIMV_CODEC_VC1_DEC) ||\
				(ctx->codec_mode == S5P_FIMV_CODEC_MPEG4_DEC))
#define MFC_UHD_RES		(3840*2160)
#define MFC_HD_RES		(1280*720)
#define is_UHD(ctx)		(((ctx)->img_width * (ctx)->img_height) == MFC_UHD_RES)
#define under_HD(ctx)		(((ctx)->img_width * (ctx)->img_height) <= MFC_HD_RES)
#define not_coded_cond(ctx)	is_mpeg4vc1(ctx)

/* Extra information for Decoder */
#define	DEC_SET_DUAL_DPB		(1 << 0)
#define	DEC_SET_DYNAMIC_DPB		(1 << 1)
#define	DEC_SET_LAST_FRAME_INFO		(1 << 2)
/* Extra information for Encoder */
#define	ENC_SET_RGB_INPUT		(1 << 0)
#define	ENC_SET_SPARE_SIZE		(1 << 1)
#define	ENC_SET_TEMP_SVC_CH		(1 << 2)

#define MFC_QOS_FLAG_NODATA		0xFFFFFFFF

struct s5p_mfc_fmt {
	char *name;
	u32 fourcc;
	u32 codec_mode;
	u32 type;
	u32 num_planes;
};

int get_framerate(struct timeval *to, struct timeval *from);
int get_framerate_by_interval(int interval);
static inline int clear_hw_bit(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int ret = -1;

	if (!atomic_read(&dev->watchdog_run))
		ret = test_and_clear_bit(ctx->num, &dev->hw_lock);

	return ret;
}

#ifdef CONFIG_ION_EXYNOS
extern struct ion_device *ion_exynos;
#endif

static inline int is_decoder_node(enum s5p_mfc_node_type node)
{
	if (node == MFCNODE_DECODER || node == MFCNODE_DECODER_DRM)
		return 1;

	return 0;
}

static inline int is_drm_node(enum s5p_mfc_node_type node)
{
	if (node == MFCNODE_DECODER_DRM || node == MFCNODE_ENCODER_DRM)
		return 1;

	return 0;
}

int s5p_mfc_dec_ctx_ready(struct s5p_mfc_ctx *ctx);
int s5p_mfc_enc_ctx_ready(struct s5p_mfc_ctx *ctx);

static inline int s5p_mfc_ctx_ready(struct s5p_mfc_ctx *ctx)
{
	if (ctx->type == MFCINST_DECODER)
		return s5p_mfc_dec_ctx_ready(ctx);
	else if (ctx->type == MFCINST_ENCODER)
		return s5p_mfc_enc_ctx_ready(ctx);

	return 0;
}


#if defined(CONFIG_EXYNOS_MFC_V5)
#include "regs-mfc-v5.h"
#include "s5p_mfc_opr_v5.h"
#include "s5p_mfc_shm.h"
#elif defined(CONFIG_EXYNOS_MFC_V6)
#include "regs-mfc-v6.h"
#include "s5p_mfc_opr_v6.h"
#elif defined(CONFIG_EXYNOS_MFC_V8)
#include "regs-mfc-v8.h"
#include "s5p_mfc_opr_v8.h"
#elif defined(CONFIG_EXYNOS_MFC_V9)
#include "regs-mfc-v9.h"
#include "s5p_mfc_opr_v9.h"
#endif

#endif /* S5P_MFC_COMMON_H_ */
