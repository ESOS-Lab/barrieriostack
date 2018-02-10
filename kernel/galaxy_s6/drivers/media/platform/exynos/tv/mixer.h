/*
 * Samsung TV Mixer driver
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *
 * Tomasz Stanislawski, <t.stanislaws@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundiation. either version 2 of the License,
 * or (at your option) any later version
 */

#ifndef SAMSUNG_MIXER_H
#define SAMSUNG_MIXER_H

#ifdef CONFIG_VIDEO_EXYNOS_MIXER_DEBUG
	#define DEBUG
#endif

#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/videodev2_exynos_media.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>
#include <media/exynos_mc.h>
#include <mach/exynos-tv.h>

#include "regs-mixer.h"

#if	defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ) ||   \
	defined(CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ) ||	\
	defined(CONFIG_ARM_EXYNOS5422_BUS_DEVFREQ)
#define CONFIG_TV_USE_BUS_DEVFREQ
#endif

/** maximum number of output interfaces */
#define MXR_MAX_OUTPUTS 2

/** There are 2 mixers after EXYNOS5250 */
#define MXR_SUB_MIXER0		0
#define MXR_SUB_MIXER1		1
/** maximum number of sub-mixers */
#if defined(CONFIG_ARCH_EXYNOS4)
#define MXR_MAX_SUB_MIXERS	1
#else
#define MXR_MAX_SUB_MIXERS	2
#endif

/** each sub-mixer supports 1 video layer and 2 graphic layers */
#define MXR_LAYER_VIDEO		0
#define MXR_LAYER_GRP0		1
#define MXR_LAYER_GRP1		2

/** maximum number of input interfaces (layers) */
#define MXR_MAX_LAYERS 3
#define MXR_DRIVER_NAME "s5p-mixer"
/** maximal number of planes for every layer */
#define MXR_MAX_PLANES	2

#define MXR_ENABLE 1
#define MXR_DISABLE 0

/** mixer pad definitions */
#define MXR_PAD_SINK_GSCALER	0
#define MXR_PAD_SINK_GRP0	1
#define MXR_PAD_SINK_GRP1	2
#define MXR_PAD_SOURCE_GSCALER	3
#define MXR_PAD_SOURCE_GRP0	4
#define MXR_PAD_SOURCE_GRP1	5
#define MXR_PADS_NUM		6

/* HDMI and HPD state definitions */
#define HPD_LOW		0
#define HPD_HIGH	1
#define HDMI_STOP	(0 << 1)
#define HDMI_STREAMING	(1 << 1)

/** description of a macroblock for packed formats */
struct mxr_block {
	/** vertical number of pixels in macroblock */
	unsigned int width;
	/** horizontal number of pixels in macroblock */
	unsigned int height;
	/** size of block in bytes */
	unsigned int size;
};

/** description of supported format */
struct mxr_format {
	/** format name/mnemonic */
	const char *name;
	/** fourcc identifier */
	u32 fourcc;
	/** colorspace identifier */
	enum v4l2_colorspace colorspace;
	/** number of planes in image data */
	int num_planes;
	/** description of block for each plane */
	struct mxr_block plane[MXR_MAX_PLANES];
	/** number of subframes in image data */
	int num_subframes;
	/** specifies to which subframe belong given plane */
	int plane2subframe[MXR_MAX_PLANES];
	/** internal code, driver dependant */
	unsigned long cookie;
};

/** description of crop configuration for image */
struct mxr_crop {
	/** width of layer in pixels */
	unsigned int full_width;
	/** height of layer in pixels */
	unsigned int full_height;
	/** horizontal offset of first pixel to be displayed */
	unsigned int x_offset;
	/** vertical offset of first pixel to be displayed */
	unsigned int y_offset;
	/** width of displayed data in pixels */
	unsigned int width;
	/** height of displayed data in pixels */
	unsigned int height;
	/** indicate which fields are present in buffer */
	unsigned int field;
};

/** stages of geometry operations */
enum mxr_geometry_stage {
	MXR_GEOMETRY_SINK,
	MXR_GEOMETRY_COMPOSE,
	MXR_GEOMETRY_CROP,
	MXR_GEOMETRY_SOURCE,
};

enum s5p_mixer_rgb {
	MIXER_RGB601_0_255,
	MIXER_RGB601_16_235,
	MIXER_RGB709_0_255,
	MIXER_RGB709_16_235
};

/** description of transformation from source to destination image */
struct mxr_geometry {
	/** cropping for source image */
	struct mxr_crop src;
	/** cropping for destination image */
	struct mxr_crop dst;
	/** layer-dependant description of horizontal scaling */
	unsigned int x_ratio;
	/** layer-dependant description of vertical scaling */
	unsigned int y_ratio;
};

/** instance of a buffer */
struct mxr_buffer {
	/** common v4l buffer stuff -- must be first */
	struct vb2_buffer	vb;
	/** node for layer's lists */
	struct list_head	list;
	struct list_head	wait;
};

struct mxr_layer_update {
	bool			update;
	struct mxr_buffer	*buffer;
	const struct mxr_format	*fmt;
	struct mxr_geometry	geo;
};

struct mxr_update {
	struct work_struct	work;
	struct mxr_device	*mdev;
	struct mxr_layer_update	layers[MXR_MAX_LAYERS];
};

/** TV graphic layer pipeline state */
enum mxr_pipeline_state {
	/** graphic layer is not shown */
	MXR_PIPELINE_IDLE = 0,
	/** state between STREAMON and hardware start */
	MXR_PIPELINE_STREAMING_START,
	/** graphic layer is shown */
	MXR_PIPELINE_STREAMING,
	/** state before STREAMOFF is finished */
	MXR_PIPELINE_STREAMING_FINISH,
};

/** TV graphic layer pipeline structure for streaming media data */
struct mxr_pipeline {
	struct media_pipeline pipe;
	enum mxr_pipeline_state state;

	/** starting point on pipeline */
	struct mxr_layer *layer;
};

/** forward declarations */
struct mxr_device;
struct mxr_layer;

/** callback for layers operation */
struct mxr_layer_ops {
	/* TODO: try to port it to subdev API */
	/** handler for resource release function */
	void (*release)(struct mxr_layer *);
	/** setting buffer to HW */
	void (*buffer_set)(struct mxr_layer *, struct mxr_buffer *);
	/** setting format and geometry in HW */
	void (*format_set)(struct mxr_layer *, const struct mxr_format *fmt,
					       struct mxr_geometry *geo);
	/** streaming stop/start */
	void (*stream_set)(struct mxr_layer *, int);
	/** adjusting geometry */
	void (*fix_geometry)(struct mxr_layer *);
};

enum mxr_layer_type {
	MXR_LAYER_TYPE_VIDEO = 0,
	MXR_LAYER_TYPE_GRP = 1,
};

struct mxr_layer_en {
	int graph0;
	int graph1;
	int graph2;
	int graph3;
};

/** layer instance, a single window and content displayed on output */
struct mxr_layer {
	/** parent mixer device */
	struct mxr_device *mdev;
	/** layer index (unique identifier) */
	int idx;
	/** layer type */
	enum mxr_layer_type type;
	/** minor number of mixer layer as video device */
	int minor;
	/** callbacks for layer methods */
	struct mxr_layer_ops ops;
	/** format array */
	const struct mxr_format **fmt_array;
	/** size of format array */
	unsigned long fmt_array_size;

	/** lock for protection of list and state fields */
	spinlock_t enq_slock;
	/** list for enqueued buffers */
	struct list_head enq_list;

	/** list for buffers waiting on a fence */
	struct list_head fence_wait_list;

	/** buffer currently owned by hardware in temporary registers */
	struct mxr_buffer *update_buf;
	/** buffer currently owned by hardware in shadow registers */
	struct mxr_buffer *shadow_buf;

	/** mutex for protection of fields below */
	struct mutex mutex;
	/** handler for video node */
	struct video_device vfd;
	/** queue for output buffers */
	struct vb2_queue vb_queue;
	/** current image format */
	const struct mxr_format *fmt;
	/** current geometry of image */
	struct mxr_geometry geo;

	/** index of current mixer path : MXR_SUB_MIXERx*/
	int cur_mxr;
	/** source pad of mixer input */
	struct media_pad pad;
	/** pipeline structure for streaming TV graphic layer */
	struct mxr_pipeline pipe;

	/** enable per layer blending for each layer */
	int layer_blend_en;
	/** alpha value for per layer blending */
	u32 layer_alpha;
	/** enable per pixel blending */
	int pixel_blend_en;
	/** enable chromakey */
	int chroma_en;
	/** value for chromakey */
	u32 chroma_val;
	/** priority for each layer */
	u8 prio;
};

/** description of mixers output interface */
struct mxr_output {
	/** name of output */
	char name[32];
	/** output subdev */
	struct v4l2_subdev *sd;
	/** cookie used for configuration of registers */
	int cookie;
};

/** specify source of output subdevs */
struct mxr_output_conf {
	/** name of output (connector) */
	char *output_name;
	/** name of module that generates output subdev */
	char *module_name;
	/** cookie need for mixer HW */
	int cookie;
};

struct clk;
struct regulator;

/** auxiliary resources used my mixer */
struct mxr_resources {
	/** interrupt index */
	int irq;
	/** pointer to Mixer registers */
	void __iomem *mxr_regs;
#if defined(CONFIG_ARCH_EXYNOS4)
	/** pointer to Video Processor registers */
	void __iomem *vp_regs;
	/** other resources, should used under mxr_device.mutex */
	struct clk *vp;
#endif
#if defined(CONFIG_CPU_EXYNOS4210)
	struct clk *sclk_dac;
#endif
	struct clk *axi_disp1;
	struct clk *sclk_mixer;
	struct clk *mixer;
	struct clk *sclk_hdmi;
};

/** videobuf2 context of mixer */
struct mxr_vb2 {
	const struct vb2_mem_ops *ops;
	void *(*init)(struct mxr_device *mdev);
	void (*cleanup)(void *alloc_ctx);

	unsigned long (*plane_addr)(struct vb2_buffer *vb, u32 plane_no);

	int (*resume)(void *alloc_ctx);
	void (*suspend)(void *alloc_ctx);

	void (*set_cacheable)(void *alloc_ctx, bool cacheable);
};

/** sub-mixer 0,1 drivers instance */
struct sub_mxr_device {
	/** state of each layer */
	struct mxr_layer *layer[MXR_MAX_LAYERS];

	/** use of each sub mixer */
	int use;
	/** use of local path gscaler to mixer */
	int local;
	/** number of G-Scaler linked to mixer */
	int gsc_num;
	/** for mixer as sub-device */
	struct v4l2_subdev sd;
	/** mixer's pads : 3 sink pad, 3 source pad */
	struct media_pad pads[MXR_PADS_NUM];
	/** format info of mixer's pads */
	struct v4l2_mbus_framefmt mbus_fmt[MXR_PADS_NUM];
	/** crop info of mixer's pads */
	struct v4l2_rect crop[MXR_PADS_NUM];
};

/** drivers instance */
struct mxr_device {
	/** master device */
	struct device *dev;
	/** state of each output */
	struct mxr_output *output[MXR_MAX_OUTPUTS];
	/** number of registered outputs */
	int output_cnt;

	/** platform data of mixer */
	struct s5p_mxr_platdata *pdata;

	/* video resources */

	/** videbuf2 context */
	const struct mxr_vb2 *vb2;
	/** context of allocator */
	void *alloc_ctx;

	/** vsync wait queue */
	wait_queue_head_t vsync_wait;
	ktime_t           vsync_timestamp;

	/** spinlock for protection of registers */
	spinlock_t reg_slock;

	struct workqueue_struct *update_wq;

	/** mutex for protection of fields below */
	struct mutex mutex;
	/** mutex for protection of streamer */
	struct mutex s_mutex;

	/** number of users that do streaming */
	int n_streamer;
	/** number of users that get power and clock */
	int n_power;
	/** index of current output */
	int current_output;
	/** auxiliary resources used my mixer */
	struct mxr_resources res;

	/** media entity link setup flags */
	unsigned long flags;

	/** entity info which transfers media data to mixer subdev */
	enum mxr_data_from mxr_data_from;

	/** count of sub-mixers */
	struct sub_mxr_device sub_mxr[MXR_MAX_SUB_MIXERS];

	/** enabled layer number **/
	struct mxr_layer_en layer_en;
	/** frame packing flag **/
	int frame_packing;

	/** RGB Quantization range and Colorimetry */
	enum s5p_mixer_rgb color_range;
	/** TV suspend */
	int blank;
};

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
extern const struct mxr_vb2 mxr_vb2_cma;
#elif defined(CONFIG_VIDEOBUF2_ION)
extern const struct mxr_vb2 mxr_vb2_ion;
#endif

/** transform device structure into mixer device */
static inline struct mxr_device *to_mdev(struct device *dev)
{
	return dev_get_drvdata(dev);
}

/** transform subdev structure into mixer device */
static inline struct mxr_device *sd_to_mdev(struct v4l2_subdev *sd)
{
	struct sub_mxr_device *sub_mxr =
		container_of(sd, struct sub_mxr_device, sd);
	return sub_mxr->layer[MXR_LAYER_GRP0]->mdev;
}

/** transform subdev structure into sub mixer device */
static inline struct sub_mxr_device *sd_to_sub_mxr(struct v4l2_subdev *sd)
{
	return container_of(sd, struct sub_mxr_device, sd);
}

/** transform entity structure into sub mixer device */
static inline struct sub_mxr_device *entity_to_sub_mxr(struct media_entity *me)
{
	struct v4l2_subdev *sd;

	sd = container_of(me, struct v4l2_subdev, entity);
	return container_of(sd, struct sub_mxr_device, sd);
}

/** transform entity structure into sub mixer device */
static inline struct mxr_device *sub_mxr_to_mdev(struct sub_mxr_device *sub_mxr)
{
	int idx;

	if (!strcmp(sub_mxr->sd.name, "s5p-mixer0"))
		idx = MXR_SUB_MIXER0;
	else
		idx = MXR_SUB_MIXER1;

	return container_of(sub_mxr, struct mxr_device, sub_mxr[idx]);
}

/** get current output data, should be called under mdev's mutex */
static inline struct mxr_output *to_output(struct mxr_device *mdev)
{
	return mdev->output[mdev->current_output];
}

/** get current output subdev, should be called under mdev's mutex */
static inline struct v4l2_subdev *to_outsd(struct mxr_device *mdev)
{
	struct mxr_output *out = to_output(mdev);
	return out ? out->sd : NULL;
}

/** forward declaration for mixer platform data */
struct mxr_platform_data;

/** acquiring common video resources */
int mxr_acquire_video(struct mxr_device *mdev,
	struct mxr_output_conf *output_cont, int output_count);

/** releasing common video resources */
void mxr_release_video(struct mxr_device *mdev);

struct mxr_layer *mxr_graph_layer_create(struct mxr_device *mdev, int cur_mxr,
	int idx, int nr);
struct mxr_layer *mxr_vp_layer_create(struct mxr_device *mdev, int cur_mxr,
	int idx, int nr);
struct mxr_layer *mxr_video_layer_create(struct mxr_device *mdev, int cur_mxr,
	int idx);
struct mxr_layer *mxr_base_layer_create(struct mxr_device *mdev,
	int idx, char *name, struct mxr_layer_ops *ops);

const struct mxr_format *find_format_by_fourcc(
	struct mxr_layer *layer, unsigned long fourcc);

void mxr_base_layer_release(struct mxr_layer *layer);
void mxr_layer_release(struct mxr_layer *layer);
void mxr_layer_geo_fix(struct mxr_layer *layer);
void mxr_layer_default_geo(struct mxr_layer *layer);

int mxr_base_layer_register(struct mxr_layer *layer);
void mxr_base_layer_unregister(struct mxr_layer *layer);

unsigned long mxr_get_plane_size(const struct mxr_block *blk,
	unsigned int width, unsigned int height);

/** adds new consumer for mixer's power */
int __must_check mxr_power_get(struct mxr_device *mdev);
/** removes consumer for mixer's power */
void mxr_power_put(struct mxr_device *mdev);
/** returns format of data delivared to current output */
void mxr_get_mbus_fmt(struct mxr_device *mdev,
	struct v4l2_mbus_framefmt *mbus_fmt);

/* Debug */

#define mxr_err(mdev, fmt, ...)  dev_err(mdev->dev, fmt, ##__VA_ARGS__)
#define mxr_warn(mdev, fmt, ...) dev_warn(mdev->dev, fmt, ##__VA_ARGS__)
#define mxr_info(mdev, fmt, ...) dev_info(mdev->dev, fmt, ##__VA_ARGS__)

#ifdef CONFIG_VIDEO_EXYNOS_MIXER_DEBUG
	#define mxr_dbg(mdev, fmt, ...)  dev_dbg(mdev->dev, fmt, ##__VA_ARGS__)
#else
	#define mxr_dbg(mdev, fmt, ...)  do { (void) mdev; } while (0)
#endif

/* accessing Mixer's and Video Processor's registers */

void mxr_layer_sync(struct mxr_device *mdev, int en);
void mxr_vsync_set_update(struct mxr_device *mdev, int en);
void mxr_reg_hw_pixelasync_reset(struct mxr_device *mdev);
void mxr_reg_sw_reset(struct mxr_device *mdev);
void mxr_vsync_enable_update(struct mxr_device *mdev);
void mxr_vsync_disable_update(struct mxr_device *mdev);
void mxr_reg_reset(struct mxr_device *mdev);
void mxr_reg_set_resolution(struct mxr_device *mdev);
void mxr_reg_set_layer_prio(struct mxr_device *mdev);
void mxr_reg_set_color_range(struct mxr_device *mdev);
void mxr_reg_set_layer_blend(struct mxr_device *mdev, int sub_mxr, int num,
		int en);
void mxr_reg_layer_alpha(struct mxr_device *mdev, int sub_mxr, int num, u32 a);
void mxr_reg_set_pixel_blend(struct mxr_device *mdev, int sub_mxr, int num,
		int en);
void mxr_reg_set_colorkey(struct mxr_device *mdev, int sub_mxr,
		int num, int en);
void mxr_reg_colorkey_val(struct mxr_device *mdev, int sub_mxr, int num, u32 v);
irqreturn_t mxr_irq_handler(int irq, void *dev_data);
void mxr_reg_s_output(struct mxr_device *mdev, int cookie);
void mxr_reg_streamon(struct mxr_device *mdev);
void mxr_reg_streamoff(struct mxr_device *mdev);
int mxr_reg_wait4update(struct mxr_device *mdev);
void mxr_reg_set_mbus_fmt(struct mxr_device *mdev,
	struct v4l2_mbus_framefmt *fmt, u32 dvi_mode);
void mxr_reg_local_path_set(struct mxr_device *mdev);
void mxr_reg_graph_layer_stream(struct mxr_device *mdev, int idx, int en);
void mxr_reg_graph_buffer(struct mxr_device *mdev, int idx, dma_addr_t addr);
void mxr_reg_graph_format(struct mxr_device *mdev, int idx,
	const struct mxr_format *fmt, const struct mxr_geometry *geo);

void mxr_reg_video_layer_stream(struct mxr_device *mdev, int idx, int en);
void mxr_reg_video_geo(struct mxr_device *mdev, int cur_mxr, int idx,
		const struct mxr_geometry *geo);

#if defined(CONFIG_ARCH_EXYNOS4)
void mxr_reg_vp_layer_stream(struct mxr_device *mdev, int en);
void mxr_reg_vp_buffer(struct mxr_device *mdev,
	dma_addr_t luma_addr[2], dma_addr_t chroma_addr[2]);
void mxr_reg_vp_format(struct mxr_device *mdev,
	const struct mxr_format *fmt, const struct mxr_geometry *geo);
#endif
void mxr_reg_dump(struct mxr_device *mdev);
void mxr_debugfs_init(struct mxr_device *mdev);

#if defined(CONFIG_VIDEOBUF2_ION)
#define mxr_buf_sync_prepare	vb2_ion_buf_prepare
#define mxr_buf_sync_finish	vb2_ion_buf_finish
#else
int mxr_buf_sync_prepare(struct vb2_buffer *vb);
int mxr_buf_sync_finish(struct vb2_buffer *vb);
#endif

#endif /* SAMSUNG_MIXER_H */
