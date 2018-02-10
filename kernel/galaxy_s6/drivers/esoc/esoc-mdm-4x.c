/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/sysmon.h>
#include "esoc.h"

#if defined(CONFIG_MDM_HSIC_PM)
#include <linux/mdm_hsic_pm.h>
static const char rmnet_pm_dev[] = "15510000.mdmpm_pdata";
#endif
#ifdef CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH
#include <linux/sec_debug.h>
#endif

#define MDM_PBLRDY_CNT			20
#define INVALID_GPIO			(-1)
#define MDM_GPIO(mdm, i)		(mdm->gpios[i])
#define MDM9x25_LABEL			"MDM9x25"
#define MDM9x25_HSIC			"HSIC"
#define MDM9x35_LABEL			"MDM9x35"
#define MDM9x35_PCIE			"PCIe"
#define MDM9x35_DUAL_LINK		"HSIC+PCIe"
#define MDM9x35_HSIC			"HSIC"
#define MDM2AP_STATUS_TIMEOUT_MS	120000L
#define MDM_MODEM_TIMEOUT		3000
#define DEF_RAMDUMP_TIMEOUT		120000
#define DEF_RAMDUMP_DELAY		2000
#define RD_BUF_SIZE			100
#define SFR_MAX_RETRIES			10
#define SFR_RETRY_INTERVAL		1000

extern unsigned int lpcharge;

/* qcom,ap2mdm-hostrdy-gpio */
extern int pmdata_gpio_host_ready;

extern int exynos_pcie_dump_link_down_status(int ch_num);

int exynos_pcie_disable_irq(int ch_num);

enum mdm_gpio {
	AP2MDM_WAKEUP = 0,
	AP2MDM_STATUS,
	AP2MDM_SOFT_RESET,
	AP2MDM_VDD_MIN,
	AP2MDM_CHNLRDY,
	AP2MDM_ERRFATAL,
	AP2MDM_VDDMIN,
	AP2MDM_PMIC_PWR_EN,
	AP2MDM_HSIC_READY,
	MDM2AP_WAKEUP,
	MDM2AP_ERRFATAL,
	MDM2AP_PBLRDY,
	MDM2AP_STATUS,
	MDM2AP_VDDMIN,
	MDM_LINK_DETECT,
	NUM_GPIOS,
};

enum gpio_update_config {
	GPIO_UPDATE_BOOTING_CONFIG = 1,
	GPIO_UPDATE_RUNNING_CONFIG,
};

enum irq_mask {
	IRQ_ERRFATAL = 0x1,
	IRQ_STATUS = 0x2,
	IRQ_PBLRDY = 0x4,
};

struct mdm_ctrl {
	unsigned gpios[NUM_GPIOS];
	spinlock_t status_lock;
	struct workqueue_struct *mdm_queue;
	struct delayed_work mdm2ap_status_check_work;
	struct work_struct mdm_status_work;
	struct work_struct restart_reason_work;
	struct completion debug_done;
	struct device *dev;
	struct gpiomux_setting *mdm2ap_status_gpio_run_cfg;
#if defined(U5_TEMP)
	struct gpiomux_setting mdm2ap_status_old_config;
#endif
	int mdm2ap_status_valid_old_config;
	int soft_reset_inverted;
	int errfatal_irq;
	int status_irq;
	int pblrdy_irq;
	int debug;
	int init;
	bool debug_fail;
	unsigned int dump_timeout_ms;
	unsigned int ramdump_delay_ms;
	int sysmon_subsys_id;
	struct esoc_clink *esoc;
	bool get_restart_reason;
	bool cp_crash_occurrence;
	unsigned long irq_mask;
	bool ready;
	bool dual_interface;
	u32 status;
};

struct mdm_ops {
	struct esoc_clink_ops *clink_ops;
	int (*config_hw)(struct mdm_ctrl *mdm,
				const struct esoc_clink_ops const *ops,
						struct platform_device *pdev);
};

static struct gpio_map {
	const char *name;
	int index;
} gpio_map[] = {
	{"qcom,mdm2ap-errfatal-gpio",   MDM2AP_ERRFATAL},
	{"qcom,ap2mdm-errfatal-gpio",   AP2MDM_ERRFATAL},
	{"qcom,mdm2ap-status-gpio",     MDM2AP_STATUS},
	{"qcom,ap2mdm-status-gpio",     AP2MDM_STATUS},
	{"qcom,mdm2ap-pblrdy-gpio",     MDM2AP_PBLRDY},
//	{"qcom,ap2mdm-wakeup-gpio",     AP2MDM_WAKEUP},
	{"qcom,ap2mdm-soft-reset-gpio",     AP2MDM_SOFT_RESET},
//	{"qcom,ap2mdm-chnlrdy-gpio",    AP2MDM_CHNLRDY},
//	{"qcom,mdm2ap-wakeup-gpio",     MDM2AP_WAKEUP},
	{"qcom,ap2mdm-vddmin-gpio",     AP2MDM_VDDMIN},
	{"qcom,mdm2ap-vddmin-gpio",     MDM2AP_VDDMIN},
//	{"qcom,ap2mdm-pmic-pwr-en-gpio", AP2MDM_PMIC_PWR_EN},
//	{"qcom,mdm-link-detect-gpio", MDM_LINK_DETECT},
	{"qcom,ap2mdm-hsic-ready-gpio",     AP2MDM_HSIC_READY},
};

/* Required gpios */
static const int required_gpios[] = {
	MDM2AP_ERRFATAL,
	AP2MDM_ERRFATAL,
	MDM2AP_STATUS,
	AP2MDM_STATUS,
	AP2MDM_SOFT_RESET
};

struct mdm_ctrl *g_mdm = NULL;

static void mdm_debug_gpio_show(struct mdm_ctrl *mdm)
{
	struct device *dev = mdm->dev;

	dev_dbg(dev, "%s: MDM2AP_ERRFATAL gpio = %d\n",
			__func__, MDM_GPIO(mdm, MDM2AP_ERRFATAL));
	dev_dbg(dev, "%s: AP2MDM_ERRFATAL gpio = %d\n",
			__func__, MDM_GPIO(mdm, AP2MDM_ERRFATAL));
	dev_dbg(dev, "%s: MDM2AP_STATUS gpio = %d\n",
			__func__, MDM_GPIO(mdm, MDM2AP_STATUS));
	dev_dbg(dev, "%s: AP2MDM_STATUS gpio = %d\n",
			__func__, MDM_GPIO(mdm, AP2MDM_STATUS));
	dev_dbg(dev, "%s: AP2MDM_SOFT_RESET gpio = %d\n",
			__func__, MDM_GPIO(mdm, AP2MDM_SOFT_RESET));
	dev_dbg(dev, "%s: MDM2AP_WAKEUP gpio = %d\n",
			__func__, MDM_GPIO(mdm, MDM2AP_WAKEUP));
	dev_dbg(dev, "%s: AP2MDM_WAKEUP gpio = %d\n",
			 __func__, MDM_GPIO(mdm, AP2MDM_WAKEUP));
	dev_dbg(dev, "%s: AP2MDM_PMIC_PWR_EN gpio = %d\n",
			 __func__, MDM_GPIO(mdm, AP2MDM_PMIC_PWR_EN));
	dev_dbg(dev, "%s: MDM2AP_PBLRDY gpio = %d\n",
			 __func__, MDM_GPIO(mdm, MDM2AP_PBLRDY));
	dev_dbg(dev, "%s: AP2MDM_VDDMIN gpio = %d\n",
			 __func__, MDM_GPIO(mdm, AP2MDM_VDDMIN));
	dev_dbg(dev, "%s: MDM2AP_VDDMIN gpio = %d\n",
			 __func__, MDM_GPIO(mdm, MDM2AP_VDDMIN));
}

static void mdm_fatal_debug_gpio_show(struct mdm_ctrl *mdm)
{
	struct device *dev = mdm->dev;

	dev_err(dev, "%s: MDM2AP_ERRFATAL %d\n",
			__func__, gpio_get_value(MDM_GPIO(mdm, MDM2AP_ERRFATAL)));
	dev_err(dev, "%s: AP2MDM_ERRFATAL %d\n",
			__func__, gpio_get_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL)));
	dev_err(dev, "%s: MDM2AP_STATUS %d\n",
			__func__, gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)));
	dev_err(dev, "%s: AP2MDM_STATUS %d\n",
			__func__, gpio_get_value(MDM_GPIO(mdm, AP2MDM_STATUS)));
}

static void mdm_enable_irqs(struct mdm_ctrl *mdm)
{
	if (!mdm)
		return;
	if (mdm->irq_mask & IRQ_ERRFATAL) {
		enable_irq(mdm->errfatal_irq);
		irq_set_irq_wake(mdm->errfatal_irq, 1);
		mdm->irq_mask &= ~IRQ_ERRFATAL;
	}
	if (mdm->irq_mask & IRQ_STATUS) {
		enable_irq(mdm->status_irq);
		irq_set_irq_wake(mdm->status_irq, 1);
		mdm->irq_mask &= ~IRQ_STATUS;
	}
	if (mdm->irq_mask & IRQ_PBLRDY) {
		enable_irq(mdm->pblrdy_irq);
		mdm->irq_mask &= ~IRQ_PBLRDY;
	}
}

static void mdm_disable_irqs(struct mdm_ctrl *mdm)
{
	if (!mdm)
		return;
	if (!(mdm->irq_mask & IRQ_ERRFATAL)) {
		irq_set_irq_wake(mdm->errfatal_irq, 0);
		disable_irq_nosync(mdm->errfatal_irq);
		mdm->irq_mask |= IRQ_ERRFATAL;
	}
	if (!(mdm->irq_mask & IRQ_STATUS)) {
		irq_set_irq_wake(mdm->status_irq, 0);
		disable_irq_nosync(mdm->status_irq);
		mdm->irq_mask |= IRQ_STATUS;
	}
	if (!(mdm->irq_mask & IRQ_PBLRDY)) {
		disable_irq_nosync(mdm->pblrdy_irq);
		mdm->irq_mask |= IRQ_PBLRDY;
	}
}

static void mdm_deconfigure_ipc(struct mdm_ctrl *mdm)
{
	int i;

	for (i = 0; i < NUM_GPIOS; ++i) {
		if (gpio_is_valid(MDM_GPIO(mdm, i)))
			gpio_free(MDM_GPIO(mdm, i));
	}
	if (mdm->mdm_queue) {
		destroy_workqueue(mdm->mdm_queue);
		mdm->mdm_queue = NULL;
	}
}

static void mdm_update_gpio_configs(struct mdm_ctrl *mdm,
				enum gpio_update_config gpio_config)
{
#if defined(U5_TEMP)
	struct device *dev = mdm->dev;
	/* Some gpio configuration may need updating after modem bootup.*/
	switch (gpio_config) {
	case GPIO_UPDATE_RUNNING_CONFIG:
		if (mdm->mdm2ap_status_gpio_run_cfg) {
			if (msm_gpiomux_write(MDM_GPIO(mdm, MDM2AP_STATUS),
				GPIOMUX_ACTIVE,
				mdm->mdm2ap_status_gpio_run_cfg,
				&mdm->mdm2ap_status_old_config))
				dev_err(dev, "switch to run failed\n");
			else
				mdm->mdm2ap_status_valid_old_config = 1;
		}
		break;
	case GPIO_UPDATE_BOOTING_CONFIG:
		if (mdm->mdm2ap_status_valid_old_config) {
			msm_gpiomux_write(MDM_GPIO(mdm, MDM2AP_STATUS),
					GPIOMUX_ACTIVE,
					&mdm->mdm2ap_status_old_config,
					NULL);
			mdm->mdm2ap_status_valid_old_config = 0;
		}
		break;
	default:
		dev_err(dev, "%s: called with no config\n", __func__);
		break;
	}
#endif
}

/* This function can be called from atomic context. */
static void mdm_toggle_soft_reset(struct mdm_ctrl *mdm)
{
	int soft_reset_direction_assert = 0,
	    soft_reset_direction_de_assert = 1;

	dev_info(mdm->dev, "%s\n", __func__);

	if (mdm->soft_reset_inverted) {
		soft_reset_direction_assert = 1;
		soft_reset_direction_de_assert = 0;
	}

	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_assert);
	/*
	 * Allow PS hold assert to be detected
	 */
	usleep_range(8000, 9000);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_de_assert);
}

static void mdm_do_first_power_on(struct mdm_ctrl *mdm)
{
	int i;
	int pblrdy;
	struct device *dev = mdm->dev;

	dev_info(dev, "Powering on modem for the first time\n");
	mdm_toggle_soft_reset(mdm);
	/* Add a delay to allow PON sequence to complete*/
	msleep(50);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_STATUS), 1);
	for (i = 0; i  < MDM_PBLRDY_CNT; i++) {
		pblrdy = gpio_get_value(MDM_GPIO(mdm, MDM2AP_PBLRDY));
		if (pblrdy)
			break;
		usleep_range(5000, 6000);
	}
	dev_info(dev, "pblrdy i:%d\n", i);

	if (pmdata_gpio_host_ready) {
		dev_info(dev, "Set qcom,ap2mdm-hostrdy-gpio to 1\n");
		gpio_direction_output(pmdata_gpio_host_ready, 1);
		gpio_set_value(pmdata_gpio_host_ready, 1);
	}
	else {
		dev_info(dev, "Fail to set gpio pin qcom,ap2mdm-hostrdy-gpio\n");
	}

	msleep(200);
}

static void mdm_power_down(struct mdm_ctrl *mdm)
{
	struct device *dev = mdm->dev;
	int soft_reset_direction = mdm->soft_reset_inverted ? 1 : 0;
	/* Assert the soft reset line whether mdm2ap_status went low or not */
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction);
	dev_info(dev, "Doing a hard reset\n");
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction);
	/*
	* Currently, there is a debounce timer on the charm PMIC. It is
	* necessary to hold the PMIC RESET low for 400ms
	* for the reset to fully take place. Sleep here to ensure the
	* reset has occured before the function exits.
	*/
	msleep(400);
}

void set_ap2mdm_errfatal(void)
{
	pr_info("[MIF] AP2MDM_ERRFATAL high!!\n");
	if(!g_mdm) {
		pr_err("[MIF] %s, esoc driver is not initialized\n", __func__);
		return;
	}


	gpio_set_value(MDM_GPIO(g_mdm, AP2MDM_ERRFATAL), 1);
	gpio_set_value(MDM_GPIO(g_mdm, AP2MDM_VDDMIN), 1);
}

static int mdm_cmd_exe(enum esoc_cmd cmd, struct esoc_clink *esoc)
{
	int ret;
	unsigned long end_time;
	bool status_down = false;
	struct mdm_ctrl *mdm = get_esoc_clink_data(esoc);
	struct device *dev = mdm->dev;

	switch (cmd) {
	case ESOC_PWR_ON:
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 0);
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_VDDMIN), 0);
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_HSIC_READY), 0);
		mdm_enable_irqs(mdm);
		mdm->init = 1;
		mdm_do_first_power_on(mdm);
		break;
	case ESOC_PWR_OFF:
		exynos_pcie_disable_irq(0);
		mdm_disable_irqs(mdm);
		mdm->debug = 0;
		mdm->ready = false;
		ret = sysmon_send_shutdown(mdm->sysmon_subsys_id);
		device_lock(dev);
		if (ret)
			dev_err(mdm->dev, "Graceful shutdown fail, ret = %d\n",
									ret);
		else {
			dev_info(mdm->dev, "Waiting for status gpio go low\n");
			status_down = false;
			end_time = jiffies + msecs_to_jiffies(10000);
			while (time_before(jiffies, end_time)) {
				if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS))
									== 0) {
					dev_dbg(dev, "Status went low\n");
					status_down = true;
					break;
				}
				msleep(100);
			}
			if (status_down)
				dev_info(dev, "shutdown successful\n");
			else {
				dev_err(mdm->dev, "graceful poff ipc fail\n");
			}
		}
		/*
		 * Force a shutdown of the mdm. This is required in order
		 * to prevent the mdm from immediately powering back on
		 * after the shutdown
		 */
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_STATUS), 0);
		esoc_clink_queue_request(ESOC_REQ_SHUTDOWN, esoc);
		mdm_power_down(mdm);
		mdm_update_gpio_configs(mdm, GPIO_UPDATE_BOOTING_CONFIG);
		device_unlock(dev);
		break;
	case ESOC_RESET:
		mdm_toggle_soft_reset(mdm);
		break;
	case ESOC_PREPARE_DEBUG:
		/*
		 * disable all irqs except request irq (pblrdy)
		 * force a reset of the mdm by signaling
		 * an APQ crash, wait till mdm is ready for ramdumps.
		 */
		mdm->ready = false;
		cancel_delayed_work(&mdm->mdm2ap_status_check_work);
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 1);
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_VDDMIN), 1);
		dev_info(mdm->dev, "set ap2mdm errfatal to force reset\n");
		msleep(mdm->ramdump_delay_ms);
		break;
	case ESOC_EXE_DEBUG:
		mdm->debug = 1;
		mdm_toggle_soft_reset(mdm);
		/*
		 * wait for ramdumps to be collected
		 * then power down the mdm and switch gpios to booting
		 * config
		 */
		wait_for_completion(&mdm->debug_done);
		if (mdm->debug_fail) {
			dev_err(mdm->dev, "unable to collect ramdumps\n");
			mdm->debug = 0;
			return -EIO;
		}
		dev_dbg(mdm->dev, "ramdump collection done\n");
		mdm->debug = 0;
		init_completion(&mdm->debug_done);
		break;
	case ESOC_EXIT_DEBUG:
		/*
		 * Deassert APQ to mdm err fatal
		 * Power on the mdm
		 */
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 0);
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_VDDMIN), 0);
		dev_dbg(mdm->dev, "exiting debug state after power on\n");
		mdm->get_restart_reason = true;
	      break;
	case ESOC_SET_CRASH_OCCURRENCE:
	      mdm->cp_crash_occurrence= true;
	      return mdm->cp_crash_occurrence;
	case ESOC_GET_CRASH_OCCURRENCE:
	      {
	      bool temp_for_reason;
	      temp_for_reason = mdm->cp_crash_occurrence;
	      mdm->cp_crash_occurrence= false;
	      return temp_for_reason;
	      }
	case ESOC_GET_RESTART_REASON:
	      return mdm->get_restart_reason;
	default:
	      return -EINVAL;
	};
	return 0;
}

static void mdm2ap_status_check(struct work_struct *work)
{
	struct mdm_ctrl *mdm =
		container_of(work, struct mdm_ctrl,
					 mdm2ap_status_check_work.work);
	struct device *dev = mdm->dev;
	struct esoc_clink *esoc = mdm->esoc;
	if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)) == 0) {
		dev_dbg(dev, "MDM2AP_STATUS did not go high\n");
		esoc_clink_evt_notify(ESOC_UNEXPECTED_RESET, esoc);
	}
}

static void mdm_status_fn(struct work_struct *work)
{
	struct mdm_ctrl *mdm =
		container_of(work, struct mdm_ctrl, mdm_status_work);
	struct device *dev = mdm->dev;
	int value = gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS));

	dev_info(dev, "%s: status:%d\n", __func__, value);
#if defined(CONFIG_MDM_HSIC_PM)
	request_active_lock_set(rmnet_pm_dev);
#endif
	/* Update gpio configuration to "running" config. */
	mdm_update_gpio_configs(mdm, GPIO_UPDATE_RUNNING_CONFIG);
}

static void mdm_get_restart_reason(struct work_struct *work)
{
	int ret, ntries = 0;
	char sfr_buf[RD_BUF_SIZE];
	struct mdm_ctrl *mdm =
		container_of(work, struct mdm_ctrl, restart_reason_work);
	struct device *dev = mdm->dev;

	do {
		ret = sysmon_get_reason(mdm->sysmon_subsys_id, sfr_buf,
							sizeof(sfr_buf));
		if (!ret) {
			dev_err(dev, "mdm restart reason is %s\n", sfr_buf);
			break;
		}
		msleep(SFR_RETRY_INTERVAL);
	} while (++ntries < SFR_MAX_RETRIES);
	if (ntries == SFR_MAX_RETRIES)
		dev_err(dev, "%s: Error retrieving restart reason: %d\n",
						__func__, ret);
	mdm->get_restart_reason = false;
}

static void mdm_notify(enum esoc_notify notify, struct esoc_clink *esoc)
{
	bool status_down;
	uint64_t timeout;
	uint64_t now;
	struct mdm_ctrl *mdm = get_esoc_clink_data(esoc);
	struct device *dev = mdm->dev;
	int ret;
	int max_spin = 20;

	switch (notify) {
	case ESOC_IMG_XFER_DONE:
		dev_info(dev, "%s ESOC_IMG_XFER_DONE\n", __func__);
		if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)) ==  0)
			schedule_delayed_work(&mdm->mdm2ap_status_check_work,
				msecs_to_jiffies(MDM2AP_STATUS_TIMEOUT_MS));
		break;
	case ESOC_BOOT_DONE:
		esoc_clink_evt_notify(ESOC_RUN_STATE, esoc);
		break;
	case ESOC_IMG_XFER_RETRY:
		mdm->init = 1;
		mdm_toggle_soft_reset(mdm);
		break;
	case ESOC_IMG_XFER_FAIL:
		esoc_clink_evt_notify(ESOC_INVALID_STATE, esoc);
		break;
	case ESOC_BOOT_FAIL:
		esoc_clink_evt_notify(ESOC_INVALID_STATE, esoc);
		break;
	case ESOC_UPGRADE_AVAILABLE:
		break;
	case ESOC_DEBUG_DONE:
		mdm->debug_fail = false;
		mdm_update_gpio_configs(mdm, GPIO_UPDATE_BOOTING_CONFIG);
		complete(&mdm->debug_done);
		break;
	case ESOC_DEBUG_FAIL:
		mdm->debug_fail = true;
		complete(&mdm->debug_done);
		break;
	case ESOC_PRIMARY_CRASH:
		mdm_disable_irqs(mdm);
		status_down = false;
		dev_info(dev, "signal apq err fatal for graceful restart\n");
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 1);
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_VDDMIN), 1);
		timeout = local_clock();
		do_div(timeout, NSEC_PER_MSEC);
		timeout += MDM_MODEM_TIMEOUT;
		do {
			if (gpio_get_value(MDM_GPIO(mdm,
						MDM2AP_STATUS)) == 0) {
				status_down = true;
				break;
			}
			now = local_clock();
			do_div(now, NSEC_PER_MSEC);
		} while (!time_after64(now, timeout));

		if (!status_down) {
			dev_err(mdm->dev, "%s MDM2AP status did not go low\n",
								__func__);
			gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
					      !!mdm->soft_reset_inverted);
			/*
			 * allow PS hold assert to be detected.
			 * pmic requires 6ms for crash reset case.
			 */
			mdelay(6);
			gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
					      !mdm->soft_reset_inverted);
		}
		break;
	case ESOC_PRIMARY_REBOOT:
		exynos_pcie_disable_irq(0);
		mdm_disable_irqs(mdm);
		dev_info(mdm->dev, "Triggering mdm cold reset");
		mdm->ready = 0;
		while (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)) && max_spin--) {
			msleep(100);
			dev_info(mdm->dev, "gpio_get_value(MDM2AP_STATUS) : %d\n",
					gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)));
		}
		gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
				!!mdm->soft_reset_inverted);
		mdelay(300);
		gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
				!mdm->soft_reset_inverted);
		break;
	case ESOC_DIAG_DISABLE:
		dev_info(mdm->dev, "Send diag_disable noti\n");
		ret = sysmon_send_diag_disable_noti(mdm->sysmon_subsys_id);
		if (ret < 0)
			dev_err(mdm->dev, "sending diag_disable noti is failed, ret = %d\n", ret);
		else
			dev_info(mdm->dev, "sending diag_disable noti is succeed.\n");
		break;
	case ESOC_FORCE_CPCRASH:
		dev_err(mdm->dev, "Force CP Crash\n");
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 1);
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_VDDMIN), 1);
		break;
	};
	return;
}

static irqreturn_t mdm_errfatal(int irq, void *dev_id)
{
	struct mdm_ctrl *mdm = (struct mdm_ctrl *)dev_id;
	struct esoc_clink *esoc;
	struct device *dev;

	if (!mdm)
		goto no_mdm_irq;
	dev = mdm->dev;
	if (!mdm->ready)
		goto mdm_pwroff_irq;
	esoc = mdm->esoc;
	if(!gpio_get_value(MDM_GPIO(mdm, MDM2AP_ERRFATAL)) || !gpio_get_value(MDM_GPIO(mdm, MDM2AP_VDDMIN))) {
		dev_err(dev, "%s: ignore IRQ errfatal: %d, vddmin: %d\n", __func__,
					gpio_get_value(MDM_GPIO(mdm, MDM2AP_ERRFATAL)),
					gpio_get_value(MDM_GPIO(mdm, MDM2AP_VDDMIN)));
		return IRQ_HANDLED;
	}
	dev_err(dev, "%s: mdm sent errfatal interrupt\n",
					 __func__);
	dev_err(dev, "%s: ep0_timeout: %d\n", __func__, ep0_timeout_cnt);
	exynos_pcie_dump_link_down_status(0);

	/* disable irq ?*/
	esoc_clink_evt_notify(ESOC_ERR_FATAL, esoc);
	mdm_fatal_debug_gpio_show(mdm);
	return IRQ_HANDLED;
mdm_pwroff_irq:
	dev_info(dev, "errfatal irq when in pwroff\n");
no_mdm_irq:
	return IRQ_HANDLED;
}

static irqreturn_t mdm_status_change(int irq, void *dev_id)
{
	int value;
	struct esoc_clink *esoc;
	struct mdm_ctrl *mdm = (struct mdm_ctrl *)dev_id;
	struct device *dev;

	if (!mdm)
		return IRQ_HANDLED;
	dev = mdm->dev;
	esoc = mdm->esoc;
	value = gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS));
	if (value == 0 && mdm->ready) {
		dev_err(dev, "unexpected reset external modem\n");
		esoc_clink_evt_notify(ESOC_UNEXPECTED_RESET, esoc);
	} else if (value == 1) {
		cancel_delayed_work(&mdm->mdm2ap_status_check_work);
		dev_info(dev, "status = 1: mdm is now ready\n");
		mdm->ready = true;
		queue_work(mdm->mdm_queue, &mdm->mdm_status_work);
		if (mdm->get_restart_reason)
			queue_work(mdm->mdm_queue, &mdm->restart_reason_work);
	}
	return IRQ_HANDLED;
}

static irqreturn_t mdm_pblrdy_change(int irq, void *dev_id)
{
	struct mdm_ctrl *mdm;
	struct device *dev;
	struct esoc_clink *esoc;

	mdm = (struct mdm_ctrl *)dev_id;
	if (!mdm)
		return IRQ_HANDLED;
	esoc = mdm->esoc;
	dev = mdm->dev;
	dev_info(dev, "pbl ready %d:\n",
			gpio_get_value(MDM_GPIO(mdm, MDM2AP_PBLRDY)));
	if (mdm->init) {
		mdm->init = 0;
		dev_err(dev, "Signaling request engine for images\n");
		esoc_clink_queue_request(ESOC_REQ_IMG, esoc);
		return IRQ_HANDLED;
	}
	if (mdm->debug)
		esoc_clink_queue_request(ESOC_REQ_DEBUG, esoc);
	return IRQ_HANDLED;
}

static int mdm_get_status(u32 *status, struct esoc_clink *esoc)
{
	struct mdm_ctrl *mdm = get_esoc_clink_data(esoc);

	if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)) == 0)
		*status = 0;
	else
		*status = 1;
	return 0;
}

int mdm_get_fatal_status(void)
{
	if(!g_mdm) {
		pr_err("[MIF] %s, esoc driver is not initialized\n", __func__);
		return 1;
	}

	if(gpio_get_value(MDM_GPIO(g_mdm, MDM2AP_ERRFATAL)))
		return 1;

	if(gpio_get_value(MDM_GPIO(g_mdm, MDM2AP_STATUS)) == 0)
		return 1;

	return 0;
}

EXPORT_SYMBOL(mdm_get_fatal_status);

static void mdm_set_hsic_ready(struct esoc_clink *esoc)
{
	struct mdm_ctrl *mdm = get_esoc_clink_data(esoc);
	gpio_set_value(MDM_GPIO(mdm, AP2MDM_HSIC_READY), 0);
	msleep(10);
	gpio_set_value(MDM_GPIO(mdm, AP2MDM_HSIC_READY), 1);
}

/* Fail if any of the required gpios is absent. */
static int mdm_dt_parse_gpios(struct mdm_ctrl *mdm)
{
	int i, val, rc = 0;
	struct device_node *node = mdm->dev->of_node;
//	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;

	for (i = 0; i < NUM_GPIOS; i++)
		mdm->gpios[i] = INVALID_GPIO;

	for (i = 0; i < ARRAY_SIZE(gpio_map); i++) {
		val = of_get_named_gpio(node, gpio_map[i].name, 0);
		if (val >= 0) {
			MDM_GPIO(mdm, gpio_map[i].index) = val;
			pr_info("[MIF] %s, gpio : %s\n", __func__, gpio_map[i].name);
		}
	}
#if 0
	/* These two are special because they can be inverted. */
	val = of_get_named_gpio_flags(node, "qcom,ap2mdm-soft-reset-gpio",
						0, &flags);
	if (val >= 0) {
		MDM_GPIO(mdm, AP2MDM_SOFT_RESET) = val;
		if (flags & OF_GPIO_ACTIVE_LOW)
			mdm->soft_reset_inverted = 1;
	}
#endif
	/* Verify that the required gpios have valid values */
	for (i = 0; i < ARRAY_SIZE(required_gpios); i++) {
		if (MDM_GPIO(mdm, required_gpios[i]) == INVALID_GPIO) {
			rc = -ENXIO;
			break;
		}
	}
	mdm_debug_gpio_show(mdm);
	return rc;
}

static int mdm_configure_ipc(struct mdm_ctrl *mdm, struct platform_device *pdev)
{
	int ret = -1;
	int irq;
	struct device *dev = mdm->dev;
	struct device_node *node = pdev->dev.of_node;

	ret = of_property_read_u32(node, "qcom,ramdump-timeout-ms",
						&mdm->dump_timeout_ms);
	if (ret)
		mdm->dump_timeout_ms = DEF_RAMDUMP_TIMEOUT;
	ret = of_property_read_u32(node, "qcom,ramdump-delay-ms",
						&mdm->ramdump_delay_ms);
	if (ret)
		mdm->ramdump_delay_ms = DEF_RAMDUMP_DELAY;
	/* Multilple gpio_request calls are allowed */
	if (gpio_request(MDM_GPIO(mdm, AP2MDM_STATUS), "AP2MDM_STATUS"))
		dev_err(dev, "Failed to configure AP2MDM_STATUS gpio\n");
	/* Multilple gpio_request calls are allowed */
	if (gpio_request(MDM_GPIO(mdm, AP2MDM_ERRFATAL), "AP2MDM_ERRFATAL"))
		dev_err(dev, "%s Failed to configure AP2MDM_ERRFATAL gpio\n",
			   __func__);
	if (gpio_request(MDM_GPIO(mdm, AP2MDM_VDDMIN), "AP2MDM_VDDMIN"))
		dev_err(dev, "%s Failed to configure AP2MDM_VDDMIN gpio\n",
			   __func__);
	if (gpio_request(MDM_GPIO(mdm, MDM2AP_VDDMIN), "MDM2AP_VDDMIN"))
		dev_err(dev, "%s Failed to configure MDM2AP_VDDMIN gpio\n",
				__func__);
	if (gpio_request(MDM_GPIO(mdm, MDM2AP_STATUS), "MDM2AP_STATUS")) {
		dev_err(dev, "%s Failed to configure MDM2AP_STATUS gpio\n",
			   __func__);
		goto fatal_err;
	}
	if (gpio_request(MDM_GPIO(mdm, MDM2AP_ERRFATAL), "MDM2AP_ERRFATAL")) {
		dev_err(dev, "%s Failed to configure MDM2AP_ERRFATAL gpio\n",
			   __func__);
		goto fatal_err;
	}
	if (gpio_is_valid(MDM_GPIO(mdm, MDM2AP_PBLRDY))) {
		if (gpio_request(MDM_GPIO(mdm, MDM2AP_PBLRDY),
						"MDM2AP_PBLRDY")) {
			dev_err(dev, "Cannot configure MDM2AP_PBLRDY gpio\n");
			goto fatal_err;
		}
	}
	if (gpio_is_valid(MDM_GPIO(mdm, AP2MDM_SOFT_RESET))) {
		if (gpio_request(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
					 "AP2MDM_SOFT_RESET")) {
			dev_err(dev, "Cannot config AP2MDM_SOFT_RESET gpio\n");
			goto fatal_err;
		}
	}
	if (gpio_is_valid(MDM_GPIO(mdm, AP2MDM_WAKEUP))) {
		if (gpio_request(MDM_GPIO(mdm, AP2MDM_WAKEUP),
					"AP2MDM_WAKEUP")) {
			dev_err(dev, "Cannot configure AP2MDM_WAKEUP gpio\n");
			goto fatal_err;
		}
	}
	if (gpio_is_valid(MDM_GPIO(mdm, AP2MDM_CHNLRDY))) {
		if (gpio_request(MDM_GPIO(mdm, AP2MDM_CHNLRDY),
						"AP2MDM_CHNLRDY")) {
			dev_err(dev, "Cannot configure AP2MDM_CHNLRDY gpio\n");
			goto fatal_err;
		}
	}
	if (gpio_is_valid(MDM_GPIO(mdm, AP2MDM_HSIC_READY))) {
		if (gpio_request(MDM_GPIO(mdm, AP2MDM_HSIC_READY),
					 "AP2MDM_HSIC_READY")) {
			dev_err(dev, "Cannot config AP2MDM_HSIC_READY gpio\n");
			goto fatal_err;
		}
	}

	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_HSIC_READY), 0);

	ret = of_property_read_u32(node, "qcom,sysmon-subsys-id",
						&mdm->sysmon_subsys_id);
	if (ret < 0)
		dev_info(dev, "sysmon_subsys_id not set.\n");

	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_STATUS), 0);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 0);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_VDDMIN), 0);
#ifdef CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH
	if (!sec_debug_is_enabled_for_ssr())
		gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET), 0);
#endif

	if (gpio_is_valid(MDM_GPIO(mdm, AP2MDM_CHNLRDY)))
		gpio_direction_output(MDM_GPIO(mdm, AP2MDM_CHNLRDY), 0);

	gpio_direction_input(MDM_GPIO(mdm, MDM2AP_STATUS));
	gpio_direction_input(MDM_GPIO(mdm, MDM2AP_VDDMIN));
	gpio_direction_input(MDM_GPIO(mdm, MDM2AP_ERRFATAL));

	// ERR_FATAL irq.
	irq = gpio_to_irq(MDM_GPIO(mdm, MDM2AP_ERRFATAL));
	if (irq < 0) {
		pr_err("%s: bad MDM2AP_ERRFATAL IRQ resource, err = %d\n",
			   __func__, irq);
		goto errfatal_err;
	}
#if 0
	irq = platform_get_irq_byname(pdev, "err_fatal_irq");
	if (irq < 0) {
		dev_err(dev, "bad MDM2AP_ERRFATAL IRQ resource\n");
		goto errfatal_err;
	}
#endif
	ret = request_irq(irq, mdm_errfatal,
			IRQF_TRIGGER_RISING , "mdm errfatal", mdm);

	if (ret < 0) {
		dev_err(dev, "%s: MDM2AP_ERRFATAL IRQ#%d request failed,\n",
					__func__, irq);
		goto errfatal_err;
	}
	mdm->errfatal_irq = irq;

errfatal_err:
	 /* status irq */
	irq = gpio_to_irq(MDM_GPIO(mdm, MDM2AP_STATUS));
	/* irq = platform_get_irq_byname(pdev, "status_irq"); */
	if (irq < 0) {
		dev_err(dev, "%s: bad MDM2AP_STATUS IRQ resource, err = %d\n",
				__func__, irq);
		goto status_err;
	}
	ret = request_threaded_irq(irq, NULL, mdm_status_change,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"mdm status", mdm);
	if (ret < 0) {
		dev_err(dev, "%s: MDM2AP_STATUS IRQ#%d request failed, err=%d",
			 __func__, irq, ret);
		goto status_err;
	}
	mdm->status_irq = irq;
status_err:
	if (gpio_is_valid(MDM_GPIO(mdm, MDM2AP_PBLRDY))) {
		irq = gpio_to_irq(MDM_GPIO(mdm, MDM2AP_PBLRDY));
		/* irq =  platform_get_irq_byname(pdev, "plbrdy_irq"); */
		if (irq < 0) {
			dev_err(dev, "%s: MDM2AP_PBLRDY IRQ request failed\n",
				 __func__);
			goto pblrdy_err;
		}

		ret = request_threaded_irq(irq, NULL, mdm_pblrdy_change,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"mdm pbl ready", mdm);
		if (ret < 0) {
			dev_err(dev, "MDM2AP_PBL IRQ#%d request failed %d\n",
								irq, ret);
			goto pblrdy_err;
		}
		mdm->pblrdy_irq = irq;
	}
	mdm_disable_irqs(mdm);
pblrdy_err:
	return 0;
fatal_err:
	mdm_deconfigure_ipc(mdm);
	return ret;

}

static int mdm9x25_setup_hw(struct mdm_ctrl *mdm,
					struct esoc_clink_ops const *ops,
					struct platform_device *pdev)
{
	int ret;
	struct esoc_clink *esoc;

	pr_info("[MIF] %s\n", __func__);
	mdm->dev = &pdev->dev;
	esoc = devm_kzalloc(mdm->dev, sizeof(*esoc), GFP_KERNEL);
	if (IS_ERR(esoc)) {
		dev_err(mdm->dev, "cannot allocate esoc device\n");
		return PTR_ERR(esoc);
	}
	mdm->mdm_queue = alloc_workqueue("mdm_queue", 0, 0);
	if (!mdm->mdm_queue) {
		dev_err(mdm->dev, "could not create mdm_queue\n");
		return -ENOMEM;
	}
	mdm->irq_mask = 0;
	mdm->ready = false;
	ret = mdm_dt_parse_gpios(mdm);
	if (ret)
		return ret;
	dev_err(mdm->dev, "parsing gpio done\n");
	ret = mdm_configure_ipc(mdm, pdev);
	if (ret)
		return ret;
	dev_err(mdm->dev, "ipc configure done\n");
	esoc->name = MDM9x25_LABEL;
	esoc->link_name = MDM9x25_HSIC;
	esoc->clink_ops = ops;
	esoc->parent = mdm->dev;
	esoc->owner = THIS_MODULE;
	esoc->np = pdev->dev.of_node;
	set_esoc_clink_data(esoc, mdm);
	ret = esoc_clink_register(esoc);
	if (ret) {
		dev_err(mdm->dev, "esoc registration failed\n");
		return ret;
	}
	dev_dbg(mdm->dev, "esoc registration done\n");
	init_completion(&mdm->debug_done);
	INIT_WORK(&mdm->mdm_status_work, mdm_status_fn);
	INIT_WORK(&mdm->restart_reason_work, mdm_get_restart_reason);
	INIT_DELAYED_WORK(&mdm->mdm2ap_status_check_work, mdm2ap_status_check);
	mdm->get_restart_reason = false;
	mdm->cp_crash_occurrence= false;
	mdm->debug_fail = false;
	mdm->esoc = esoc;
	mdm->init = 0;

	return 0;
}

static int mdm9x35_setup_hw(struct mdm_ctrl *mdm,
					struct esoc_clink_ops const *ops,
					struct platform_device *pdev)
{
	int ret;
	struct device_node *node;
	struct esoc_clink *esoc;

	pr_info("[MIF] %s\n", __func__);
	mdm->dev = &pdev->dev;
	node = pdev->dev.of_node;
	esoc = devm_kzalloc(mdm->dev, sizeof(*esoc), GFP_KERNEL);
	if (IS_ERR(esoc)) {
		dev_err(mdm->dev, "cannot allocate esoc device\n");
		return PTR_ERR(esoc);
	}
	mdm->mdm_queue = alloc_workqueue("mdm_queue", 0, 0);
	if (!mdm->mdm_queue) {
		dev_err(mdm->dev, "could not create mdm_queue\n");
		return -ENOMEM;
	}
	mdm->irq_mask = 0;
	mdm->ready = false;
	ret = mdm_dt_parse_gpios(mdm);
	if (ret)
		return ret;
	dev_info(mdm->dev, "parsing gpio done\n");
	ret = mdm_configure_ipc(mdm, pdev);
	if (ret)
		return ret;
	dev_info(mdm->dev, "ipc configure done\n");
	esoc->name = MDM9x35_LABEL;
	mdm->dual_interface = of_property_read_bool(node,
						"qcom,mdm-dual-link");
	/* Check if link gpio is available */
	if (gpio_is_valid(MDM_GPIO(mdm, MDM_LINK_DETECT))) {
		if (mdm->dual_interface) {
			if (gpio_get_value(MDM_GPIO(mdm, MDM_LINK_DETECT)))
				esoc->link_name = MDM9x35_DUAL_LINK;
			else
				esoc->link_name = MDM9x35_PCIE;
		} else {
			if (gpio_get_value(MDM_GPIO(mdm, MDM_LINK_DETECT)))
				esoc->link_name = MDM9x35_HSIC;
			else
				esoc->link_name = MDM9x35_PCIE;
		}
	} else if (mdm->dual_interface)
		esoc->link_name = MDM9x35_DUAL_LINK;
	else
		esoc->link_name = MDM9x35_HSIC;

	esoc->link_name = MDM9x35_DUAL_LINK;

	esoc->clink_ops = ops;
	esoc->parent = mdm->dev;
	esoc->owner = THIS_MODULE;
	esoc->np = pdev->dev.of_node;
	set_esoc_clink_data(esoc, mdm);
	ret = esoc_clink_register(esoc);
	if (ret) {
		dev_err(mdm->dev, "esoc registration failed\n");
		return ret;
	}
	dev_info(mdm->dev, "esoc registration done\n");
	init_completion(&mdm->debug_done);
	INIT_WORK(&mdm->mdm_status_work, mdm_status_fn);
	INIT_WORK(&mdm->restart_reason_work, mdm_get_restart_reason);
	INIT_DELAYED_WORK(&mdm->mdm2ap_status_check_work, mdm2ap_status_check);
	mdm->get_restart_reason = false;
	mdm->cp_crash_occurrence= false;
	mdm->debug_fail = false;
	mdm->esoc = esoc;
	mdm->init = 0;
	g_mdm = mdm;
	return 0;
}

static struct esoc_clink_ops mdm_cops = {
	.cmd_exe = mdm_cmd_exe,
	.get_status = mdm_get_status,
	.set_hsic_ready = mdm_set_hsic_ready,
	.notify = mdm_notify,
};

static struct mdm_ops mdm9x25_ops = {
	.clink_ops = &mdm_cops,
	.config_hw = mdm9x25_setup_hw,
};

static struct mdm_ops mdm9x35_ops = {
	.clink_ops = &mdm_cops,
	.config_hw = mdm9x35_setup_hw,
};

static struct of_device_id mdm_dt_match[] = {
	{ .compatible = "qcom,ext-mdm9x35",
		.data = &mdm9x35_ops, },
	{},
};
MODULE_DEVICE_TABLE(of, mdm_dt_match);

static int __init get_baseband(char *str)
{
	if(!strncasecmp(str, "mdm2", 4)) {
		strcpy(mdm_dt_match->compatible, "qcom,ext-mdm9x35");
		mdm_dt_match->data = &mdm9x35_ops;
	} else if(!strncasecmp(str, "mdm", 3)) {
		strcpy(mdm_dt_match->compatible, "qcom,ext-mdm9x25");
		mdm_dt_match->data = &mdm9x25_ops;
	}
	pr_emerg("%s: %s\n", __func__, mdm_dt_match->compatible);

	return 0;
}
__setup("androidboot.baseband=", get_baseband);

static int mdm_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct mdm_ops *mdm_ops;
	struct device_node *node = pdev->dev.of_node;
	struct mdm_ctrl *mdm;

	pr_info("[MIF] %s\n", __func__);
	match = of_match_node(mdm_dt_match, node);
	if (IS_ERR(match)) {
		pr_info("[MIF] %s\n", __func__);
		return PTR_ERR(match);
	}
	mdm_ops = match->data;
	mdm = devm_kzalloc(&pdev->dev, sizeof(*mdm), GFP_KERNEL);
	if (IS_ERR(mdm))
		return PTR_ERR(mdm);
	return mdm_ops->config_hw(mdm, mdm_ops->clink_ops, pdev);
}

static struct platform_driver mdm_driver = {
	.probe		= mdm_probe,
	.driver = {
		.name	= "ext-mdm",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mdm_dt_match),
	},
};

static int __init mdm_register(void)
{
#if 1
	/* in lpm mode, do not load modem driver */
	if (lpcharge) {
		pr_info("[MIF] %s : lpcharge [%u], skip register\n", __func__, lpcharge);
		return 0;
	}
#endif
	pr_info("[MIF] %s\n", __func__);
	return platform_driver_register(&mdm_driver);
}
module_init(mdm_register);

static void __exit mdm_unregister(void)
{
	platform_driver_unregister(&mdm_driver);
}
module_exit(mdm_unregister);
MODULE_LICENSE("GPL v2");
