/* linux/arch/arm/plat-s5p/include/plat/fimc_is.h
 *
 * Copyright (C) 2011 Samsung Electronics, Co. Ltd
 *
 * Exynos 4 series FIMC-IS slave device support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef EXYNOS_FIMC_IS_H_
#define EXYNOS_FIMC_IS_H_ __FILE__

#define FIMC_IS_MAKE_QOS_IDX_NM(__LV) __LV ## _IDX
#define FIMC_IS_DECLARE_QOS_ENUM(__TYPE) enum FIMC_IS_DVFS_ ## __TYPE ## _LV_IDX

#include <linux/videodev2.h>
#if defined(CONFIG_ARCH_EXYNOS4)
#include <media/s5p_fimc.h>
#endif

#define FIMC_IS_DEV_NAME			"exynos-fimc-is"
#include <linux/platform_device.h>

/* FIMC-IS DVFS LEVEL enum (INT, MIF, I2C) */
enum FIMC_IS_INT_LV {
	FIMC_IS_INT_L0,
	FIMC_IS_INT_L1,
	FIMC_IS_INT_L1_1,
	FIMC_IS_INT_L1_2,
	FIMC_IS_INT_L1_3,
};

enum FIMC_IS_MIF_LV {
	FIMC_IS_MIF_L0,
	FIMC_IS_MIF_L1,
	FIMC_IS_MIF_L2,
	FIMC_IS_MIF_L3,
	FIMC_IS_MIF_L4,
};

/*
 * On some soc, It needs to notify change of INT clock to F/W.
 * Because I2C clock can be take affect from other clock change(like INT)
 */
enum FIMC_IS_I2C_LV {
	FIMC_IS_I2C_L0,
	FIMC_IS_I2C_L1,
	FIMC_IS_I2C_L1_1,
	FIMC_IS_I2C_L1_3,
};

/* FIMC-IS DVFS SCENARIO enum */
enum FIMC_IS_SCENARIO_ID {
	FIMC_IS_SN_DEFAULT,
	FIMC_IS_SN_FRONT_PREVIEW,
	FIMC_IS_SN_FRONT_CAPTURE,
	FIMC_IS_SN_FRONT_CAMCORDING,
	FIMC_IS_SN_FRONT_VT1,
	FIMC_IS_SN_FRONT_VT2,
	FIMC_IS_SN_REAR_PREVIEW_FHD,
	FIMC_IS_SN_REAR_PREVIEW_FHD_BNS_OFF,
	FIMC_IS_SN_REAR_PREVIEW_WHD,
	FIMC_IS_SN_REAR_PREVIEW_UHD,
	FIMC_IS_SN_REAR_CAPTURE,
	FIMC_IS_SN_REAR_CAMCORDING_FHD,
	FIMC_IS_SN_REAR_CAMCORDING_FHD_BNS_OFF,
	FIMC_IS_SN_REAR_CAMCORDING_WHD,
	FIMC_IS_SN_REAR_CAMCORDING_UHD,
	FIMC_IS_SN_DUAL_PREVIEW,
	FIMC_IS_SN_DUAL_CAPTURE,
	FIMC_IS_SN_DUAL_CAMCORDING,
	FIMC_IS_SN_HIGH_SPEED_FPS,
	FIMC_IS_SN_DIS_ENABLE,
	FIMC_IS_SN_MAX,
	FIMC_IS_SN_END,
};

enum FIMC_IS_DVFS_QOS_TYPE {
	FIMC_IS_DVFS_CPU_MIN,
	FIMC_IS_DVFS_CPU_MAX,
	FIMC_IS_DVFS_INT,
	FIMC_IS_DVFS_MIF,
	FIMC_IS_DVFS_I2C,
	FIMC_IS_DVFS_CAM,
	FIMC_IS_DVFS_DISP,
	FIMC_IS_DVFS_PWM,
	FIMC_IS_DVFS_END,
};

#define SET_QOS_WITH_CPU(t, s, i, m, _i, c, d, p, cmin, cmax)	\
	(t)[s][FIMC_IS_DVFS_INT]	= i;	\
	(t)[s][FIMC_IS_DVFS_MIF]	= m;	\
	(t)[s][FIMC_IS_DVFS_I2C]	= _i;	\
	(t)[s][FIMC_IS_DVFS_CAM]	= c;	\
	(t)[s][FIMC_IS_DVFS_DISP]	= d;	\
	(t)[s][FIMC_IS_DVFS_PWM]	= p;	\
	(t)[s][FIMC_IS_DVFS_CPU_MIN]	= cmin;	\
	(t)[s][FIMC_IS_DVFS_CPU_MAX]	= cmax;

#define SET_QOS(t, s, i, m, _i, c, d, p)	\
	(t)[s][FIMC_IS_DVFS_INT]	= i;	\
	(t)[s][FIMC_IS_DVFS_MIF]	= m;	\
	(t)[s][FIMC_IS_DVFS_I2C]	= _i;	\
	(t)[s][FIMC_IS_DVFS_CAM]	= c;	\
	(t)[s][FIMC_IS_DVFS_DISP]	= d;	\
	(t)[s][FIMC_IS_DVFS_PWM]	= p;	\
	(t)[s][FIMC_IS_DVFS_CPU_MIN]	= 0;	\
	(t)[s][FIMC_IS_DVFS_CPU_MAX]	= 0;

enum FIMC_IS_CLK_GATE {
	FIMC_IS_GATE_3AA1_IP,
	FIMC_IS_GATE_ISP_IP,
	FIMC_IS_GATE_DRC_IP,
	FIMC_IS_GATE_SCC_IP,
	FIMC_IS_GATE_ODC_IP,
	FIMC_IS_GATE_DIS_IP,
	FIMC_IS_GATE_3DNR_IP,
	FIMC_IS_GATE_SCP_IP,
	FIMC_IS_GATE_FD_IP,
	FIMC_IS_GATE_3AA0_IP,
	FIMC_IS_GATE_ISP1_IP,
	FIMC_IS_GATE_TPU_IP,
	FIMC_IS_GATE_VRA_IP,
	FIMC_IS_CLK_GATE_MAX,
};

enum FIMC_IS_GRP {
	FIMC_IS_GRP_3A0,
	FIMC_IS_GRP_3A1,
	FIMC_IS_GRP_ISP,
	FIMC_IS_GRP_DIS,
	FIMC_IS_GRP_MAX,
};

enum FIMC_IS_CLK_GATE_USR_SCENARIO {
	CLK_GATE_NOT_FULL_BYPASS_SN = 1,
	CLK_GATE_FULL_BYPASS_SN,
	CLK_GATE_DIS_SN,
};


/*
 * struct exynos_fimc_is_clk_gate_group
 *	This struct is for host clock gating.
 * 	It decsribes register, masking bit info and other control for each group.
 *	If you uses host clock gating, You must define this struct in exynos_fimc_is_clk_gate_info.
 */
struct exynos_fimc_is_clk_gate_group {
	u32	mask_clk_on_org;	/* masking value in clk on */
	u32	mask_clk_on_mod;	/* masking value in clk on */
	u32	mask_clk_off_self_org;	/* masking value in clk off(not depend) original */
	u32	mask_clk_off_self_mod;	/* masking value in clk off(not depend) final */
	u32	mask_clk_off_depend;	/* masking value in clk off(depend on other grp) */
	u32	mask_cond_for_depend;	/* masking group having dependancy for other */
};

/*
 * struct exynos_fimc_is_clk_gate_info
 * 	This struct is for host clock gating.
 * 	It has exynos_fimc_is_clk_gate_group to control each group's clk gating.
 * 	And it has function pointer to include user scenario masking
 */
struct exynos_fimc_is_clk_gate_info {
	const char *gate_str[FIMC_IS_CLK_GATE_MAX];			/* register adr for gating */
	struct exynos_fimc_is_clk_gate_group groups[FIMC_IS_GRP_MAX];
	/* You must set this function pointer (on/off) */
	int (*clk_on_off)(u32 clk_gate_id, bool is_on);
	/*
	 * if there are specific scenarios for clock gating,
	 * You can define user function.
	 * user_scenario_id will be in
	 */
	int (*user_clk_gate)(u32 group_id,
			bool is_on,
			u32 user_scenario_id,
			unsigned long msk_state,
			struct exynos_fimc_is_clk_gate_info *gate_info);
};

/**
* struct exynos_platform_fimc_is - camera host interface platform data
*
* @isp_info: properties of camera sensor required for host interface setup
*/
struct exynos_platform_fimc_is {
	int	hw_ver;
	bool	clock_on;
	int	(*cfg_gpio)(struct platform_device *pdev, int channel, bool flag_on);
	int	(*clk_cfg)(struct platform_device *pdev);
	int	(*clk_on)(struct platform_device *pdev);
	int	(*clk_off)(struct platform_device *pdev);
	int	(*print_clk)(struct platform_device *pdev);
	int	(*print_cfg)(struct platform_device *pdev, u32 channel);
	int	(*print_pwr)(struct platform_device *pdev);

	/* These fields are to return qos value for dvfs scenario */
	u32	*int_qos_table;
	u32	*mif_qos_table;
	u32	*i2c_qos_table;
	int	(*get_int_qos)(int scenario_id);
	int	(*get_mif_qos)(int scenario_id);
	int	(*get_i2c_qos)(int scenario_id);
	u32	dvfs_data[FIMC_IS_SN_END][FIMC_IS_DVFS_END];

	/* For host clock gating */
	struct exynos_fimc_is_clk_gate_info *gate_info;
#ifdef CONFIG_COMPANION_USE
	u32	companion_spi_channel;
	bool	use_two_spi_line;
#endif
	u32	use_sensor_dynamic_voltage_mode;
};

extern struct device *fimc_is_dev;

extern void exynos_fimc_is_set_platdata(struct exynos_platform_fimc_is *pd);

int fimc_is_set_parent_dt(struct platform_device *pdev, const char *child, const char *parent);
int fimc_is_set_rate_dt(struct platform_device *pdev, const char *conid, unsigned int rate);
int fimc_is_get_rate_dt(struct platform_device *pdev, const char *conid);
int fimc_is_enable_dt(struct platform_device *pdev, const char *conid);
int fimc_is_disable_dt(struct platform_device *pdev, const char *conid);

/* platform specific clock functions */
extern int exynos_fimc_is_cfg_clk(struct platform_device *pdev);
extern int exynos_fimc_is_cfg_cam_clk(struct platform_device *pdev);
extern int exynos_fimc_is_clk_on(struct platform_device *pdev);
extern int exynos_fimc_is_clk_off(struct platform_device *pdev);
extern int exynos_fimc_is_print_clk(struct platform_device *pdev);
extern int exynos_fimc_is_print_cfg(struct platform_device *pdev, u32 channel);
extern int exynos_fimc_is_print_pwr(struct platform_device *pdev);
extern int exynos_fimc_is_set_user_clk_gate(u32 group_id, bool is_on, u32 user_scenario_id,
											unsigned long msk_state,
											struct exynos_fimc_is_clk_gate_info *gate_info);
extern int exynos_fimc_is_clk_gate(u32 clk_gate_id, bool is_on);

#endif /* EXYNOS_FIMC_IS_H_ */
