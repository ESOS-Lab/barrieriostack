/* linux/arch/arm/mach-xxxx/mdm_hsic_pm.c
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/usb.h>
#include <linux/pm_runtime.h>
#include <plat/gpio-cfg.h>
#include <linux/mdm_hsic_pm.h>
#include <linux/suspend.h>
#include <linux/wakelock.h>
#include <mach/subsystem_restart.h>
#include <linux/msm_charm.h>
//#include "mdm_private.h"
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/ehci_def.h>
#include <linux/of_gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/platform_data/samsung-usbphy.h>
#include <linux/usb/samsung_usb_phy.h>

#ifdef CONFIG_CPU_FREQ_TETHERING
#include <linux/kernel.h>
#include <linux/netdevice.h>
//#include <mach/mdm2.h>
#include <linux/usb/android_composite.h>
#endif

#define EXTERNAL_MODEM "external_modem"
#undef EHCI_REG_DUMP
#define DEFAULT_RAW_WAKE_TIME (0*HZ)

#define HOST_READY_RETRY_CNT 2

BLOCKING_NOTIFIER_HEAD(mdm_reset_notifier_list);

extern unsigned int lpcharge;

/**
 * TODO:
 * pm notifier register
 *
 * think the way to use notifier to register or unregister device
 *
 * disconnect also can be notified
 *
 * block request under kernel power off seq.
 *
 * in suspend function if busy has set, return
 *
 */

/**
 * struct mdm_hsic_pm_data - hsic pm platform driver private data
 *	provide data and information for pm state transition
 *
 * @name:		name of this pm driver
 * @udev:		usb driver for resuming device from device request
 * @intf_cnt:		count of registered interface driver
 * @block_request:	block and ignore requested resume interrupt
 * @state_busy:		it is not determined to use, it can be replaced to
 *			rpm status check
 * @pm_notifier:	notifier block to control block_request variable
 *			block_reqeust set to true at PM_SUSPEND_PREPARE and
 *			release at PM_POST_RESUME
 *
 */
struct mdm_hsic_pm_data {
	struct list_head list;
	char name[32];

	struct usb_device *udev;
	int intf_cnt;

	/* control variables */
	struct notifier_block pm_notifier;
#ifdef CONFIG_CPU_FREQ_TETHERING
	struct notifier_block netdev_notifier;
	struct notifier_block usb_composite_notifier;
#endif

	bool block_request;
	bool state_busy;
	atomic_t pmlock_cnt;
	bool shutdown;

	/* gpio-s and irq */
	int gpio_host_ready;
	int gpio_device_ready;
	int gpio_host_wake;
	int irq;
	int dev_rdy_irq;

	/* wakelock for L0 - L2 */
	struct wake_lock l2_wake;

	/* wakelock for boot */
	struct wake_lock boot_wake;

	/* wakelock for fast dormancy */
	struct wake_lock fd_wake;
	long fd_wake_time; /* wake time for raw packet in jiffies */

	/* workqueue, work for delayed autosuspend */
	struct workqueue_struct *wq;
	struct delayed_work auto_rpm_start_work;
	struct delayed_work auto_rpm_restart_work;
	struct delayed_work request_resume_work;
	struct delayed_work fast_dormancy_work;

	struct mdm_hsic_pm_platform_data *mdm_pdata;

	/* QMICM mode value */
	bool qmicm_mode;

	struct notifier_block phy_nfb;
};

/* indicate wakeup from lpa state */
bool lpa_handling;

/* indicate receive hallo_packet_rx */
int hello_packet_rx;

/* qcom,ap2mdm-hostrdy-gpio */
int pmdata_gpio_host_ready;

#ifdef EHCI_REG_DUMP
struct exynos5430_ehci_regs {
	/* EHCI Capability Register */
	unsigned hc_cap_base;	/* 0x00 */
	unsigned hc_sparams;
	unsigned hc_cparams;
	unsigned reserved0;

	/* EHCI Operational Register */
	unsigned usb_cmd;	/* 0x10 */
	unsigned usb_sts;
	unsigned usb_intr;
	unsigned fr_index;
	unsigned ctrl_ds_segment;
	unsigned periodic_list_base;
	unsigned async_list_addr;
	unsigned reserved1[9];

	/* EHCI Auxiliary Power Well Register */
	unsigned config_flag;	/* 0x50 */
	unsigned port_sc0;
	unsigned port_sc1;
	unsigned reserved2[13];

	/* EHCI Specific Register */
	unsigned insnreg00;	/* 0x90 */
	unsigned insnreg01;
	unsigned insnreg02;
	unsigned insnreg03;
	unsigned insnreg04;
	unsigned insnreg05;
	unsigned insnreg06;
	unsigned insnreg07;
};
/* for EHCI register dump */
static struct exynos5430_ehci_regs __iomem *ehci_reg;

struct exynos5430_phy_regs {
	/* PHY Control Register */
	unsigned host_phy_ctrl0;	/* 0x00 */
	unsigned host_phy_tune0;
	unsigned reserved0[2];

	unsigned hsic_phy_ctrl1;	/* 0x10 */
	unsigned hsic_phy_tune1;
	unsigned reserved1[6];

	unsigned host_ehci_ctrl;	/* 0x30 */
	unsigned host_ohci_ctrl;
};

struct s5p_ehci_hcd_stub {
	struct device *dev;
	struct usb_hcd *hcd;
	struct clk *clk;
	int power_on;
};
/* for PHY register dump */
static struct exynos5430_phy_regs __iomem *phy_reg;
/* for PMU register dump */
static int __iomem *pmu_reg;
/* for USB phy clk register dump */
static int __iomem *clk_reg;

#define pr_reg(s, r) printk(KERN_DEBUG "hcd reg(%s):\t 0x%08x\n", s, r)
static void print_ehci_regs(struct exynos5430_ehci_regs *base)
{
	pr_info("------- EHCI reg dump -------\n");
	pr_reg("HCCPBASE", base->hc_cap_base);
	pr_reg("HCSPARAMS", base->hc_sparams);
	pr_reg("HCCPARAMS", base->hc_cparams);

	pr_reg("USBCMD", base->usb_cmd);
	pr_reg("USBSTS", base->usb_sts);
	pr_reg("USBINTR", base->usb_intr);
	pr_reg("FRINDEX", base->fr_index);
	pr_reg("CTRLDSSEGMENT", base->ctrl_ds_segment);
	pr_reg("PERIODICLISTBASE", base->periodic_list_base);
	pr_reg("ASYNCLISTADDR", base->async_list_addr);

	pr_reg("CONFIGFLAG", base->config_flag);
	pr_reg("PORTSC_0", base->port_sc0);
	pr_reg("PORTSC_1", base->port_sc1);

	pr_reg("INSNREG00", base->insnreg00);
	pr_reg("INSNREG01", base->insnreg01);
	pr_reg("INSNREG02", base->insnreg02);
	pr_reg("INSNREG03", base->insnreg03);
	pr_reg("INSNREG04", base->insnreg04);
	pr_reg("INSNREG05", base->insnreg05);
	pr_reg("INSNREG06", base->insnreg06);
	pr_reg("INSNREG07", base->insnreg07);
	pr_info("-----------------------------\n");
}

static void print_phy_regs(struct exynos5430_phy_regs *base)
{
	pr_info("------- PHY reg dump -------\n");
	pr_reg("HOSTPHYCTRL0", base->host_phy_ctrl0);
	pr_reg("HOSTPHYTUNE0", base->host_phy_tune0);

	pr_reg("HSICPHYCTRL1", base->hsic_phy_ctrl1);
	pr_reg("HSICPHYTUNE1", base->hsic_phy_tune1);

	pr_reg("HOSTEHCICTRL", base->host_ehci_ctrl);
	pr_reg("HOSTOHCICTRL", base->host_ohci_ctrl);
	pr_info("-----------------------------\n");
}

static void print_pmu_regs(int __iomem *pmu_reg)
{
	pr_info("------- PMU reg dump -------\n");
	pr_reg("USBDEV_PHY_CONTROL", *pmu_reg);
	pr_info("-----------------------------\n");
}

static void print_clk_regs(int __iomem *clk_reg)
{
	pr_info("------- USB phy clk reg dump -------\n");
	pr_reg("CLK_MUX_SEL_FSYS2", *clk_reg);
	pr_info("-----------------------------\n");
}

static void debug_ehci_reg_dump(void)
{
	print_ehci_regs(ehci_reg);
	print_phy_regs(phy_reg);
	print_pmu_regs(pmu_reg);
	print_clk_regs(clk_reg);
}
#else
static void debug_ehci_reg_dump(void) {}
#endif

/**
 * hsic pm device list for multiple modem support
 */
static LIST_HEAD(hsic_pm_dev_list);

static void print_pm_dev_info(struct mdm_hsic_pm_data *pm_data)
{
	pr_info("pm device\n\tname = %s\n"
		"\tudev = 0x%p\n"
		"\tintf_cnt = %d\n"
		"\tblock_request = %s\n",
		pm_data->name,
		pm_data->udev,
		pm_data->intf_cnt,
		pm_data->block_request ? "true" : "false");
}

static struct mdm_hsic_pm_data *get_pm_data_by_dev_name(const char *name)
{
	struct mdm_hsic_pm_data *pm_data;

	if (list_empty(&hsic_pm_dev_list)) {
		pr_err("%s:there's no dev on pm dev list\n", __func__);
		return NULL;
	};

	/* get device from list */
	list_for_each_entry(pm_data, &hsic_pm_dev_list, list) {
		if (!strncmp(pm_data->name, name, strlen(name)))
			return pm_data;
	}

	return NULL;
}

/* do not call in irq context */
int pm_dev_runtime_get_enabled(struct usb_device *udev)
{
	int spin = 50;

	while (spin--) {
		pr_debug("%s: rpm status: %d\n", __func__,
						udev->dev.power.runtime_status);
		if (udev->dev.power.runtime_status == RPM_ACTIVE ||
			udev->dev.power.runtime_status == RPM_SUSPENDED) {
			usb_mark_last_busy(udev);
			break;
		}
		msleep(20);
	}
	if (spin <= 0) {
		pr_err("%s: rpm status %d, return -EAGAIN\n", __func__,
						udev->dev.power.runtime_status);
		return -EAGAIN;
	}
	usb_mark_last_busy(udev);

	return 0;
}

/* do not call in irq context */
int pm_dev_wait_lpa_wake(void)
{
	int spin = 50;

	while (lpa_handling && spin--) {
		pr_debug("%s: lpa wake wait loop\n", __func__);
		msleep(20);
	}

	if (lpa_handling) {
		pr_err("%s: in lpa wakeup, return EAGAIN\n", __func__);
		return -EAGAIN;
	}

	return 0;
}

void set_shutdown(void)
{
	struct mdm_hsic_pm_data *pm_data =
		get_pm_data_by_dev_name("15510000.mdmpm_pdata");

	pm_data->shutdown = true;
}

void notify_modem_fatal(void)
{
	struct mdm_hsic_pm_data *pm_data =
				get_pm_data_by_dev_name("15510000.mdmpm_pdata");

	pr_info("%s or shutdown\n", __func__);
	//print_mdm_gpio_state();

	if (!pm_data || !pm_data->intf_cnt || !pm_data->udev)
		return;

	pm_data->shutdown = true;

	/* crash from sleep, ehci is in waking up, so do not control ehci */
	if (!pm_data->block_request) {
		struct device *dev, *hdev;
		hdev = pm_data->udev->bus->root_hub->dev.parent;
		dev = &pm_data->udev->dev;

		pm_runtime_get_noresume(dev);
		pm_runtime_dont_use_autosuspend(dev);
		/* if it's in going suspend, give settle time before wake up */
		msleep(100);
		wake_up_all(&dev->power.wait_queue);
		pm_runtime_resume(dev);
		pm_runtime_get_noresume(dev);

		blocking_notifier_call_chain(&mdm_reset_notifier_list, 0, 0);
	}
}

void request_autopm_lock(int status)
{
	struct mdm_hsic_pm_data *pm_data =
					get_pm_data_by_dev_name("15510000.mdmpm_pdata");
	int spin = 5;

	if (!pm_data || !pm_data->udev)
		return;

	pr_debug("%s: set runtime pm lock : %d\n", __func__, status);

	if (status) {
		if (!atomic_read(&pm_data->pmlock_cnt)) {
			atomic_inc(&pm_data->pmlock_cnt);
			pr_info("get lock\n");

			do {
				if (!pm_dev_runtime_get_enabled(pm_data->udev))
					break;
			} while (spin--);

#if 0
			if (spin <= 0)
				mdm_force_fatal();
#endif

			pm_runtime_get(&pm_data->udev->dev);
			pm_runtime_forbid(&pm_data->udev->dev);
		} else
			atomic_inc(&pm_data->pmlock_cnt);
	} else {
		if (!atomic_read(&pm_data->pmlock_cnt))
			pr_info("unbalanced release\n");
		else if (atomic_dec_and_test(&pm_data->pmlock_cnt)) {
			pr_info("release lock\n");
			pm_runtime_allow(&pm_data->udev->dev);
			pm_runtime_put(&pm_data->udev->dev);
		}
		/* initailize hello_packet_rx */
		hello_packet_rx = 0;
	}
}

void request_active_lock_set(const char *name)
{
	struct mdm_hsic_pm_data *pm_data = get_pm_data_by_dev_name(name);
	pr_info("%s\n", __func__);
	if (pm_data)
		wake_lock(&pm_data->l2_wake);
}

void request_active_lock_release(const char *name)
{
	struct mdm_hsic_pm_data *pm_data = get_pm_data_by_dev_name(name);
	pr_info("%s\n", __func__);
	if (pm_data)
		wake_unlock(&pm_data->l2_wake);
}

void request_boot_lock_set(const char *name)
{
	struct mdm_hsic_pm_data *pm_data = get_pm_data_by_dev_name(name);
	pr_info("%s\n", __func__);
	if (pm_data)
		wake_lock(&pm_data->boot_wake);
}

void request_boot_lock_release(const char *name)
{
	struct mdm_hsic_pm_data *pm_data = get_pm_data_by_dev_name(name);
	pr_info("%s\n", __func__);
	if (pm_data)
		wake_unlock(&pm_data->boot_wake);
}

bool check_request_blocked(const char *name)
{
	struct mdm_hsic_pm_data *pm_data = get_pm_data_by_dev_name(name);
	if (!pm_data)
		return false;

	return pm_data->block_request;
}

static void set_host_stat(struct mdm_hsic_pm_data *pm_data, enum pwr_stat status)
{
	/* crash during kernel suspend/resume, do not control host ready pin */
	/* and it has to be controlled when host driver initialized again */
	if (pm_data->block_request && pm_data->shutdown)
		return;

	if (pm_data->gpio_host_ready) {
		pr_info("dev rdy val = %d\n",
				gpio_get_value(pm_data->gpio_device_ready));
		pr_info("%s:set host port power status to [%d]\n",
							__func__, status);

		/*
		 * need get some delay for MDM9x15 suspend
		 * if L3 drive goes out to modem in suspending
		 * modem goes to unstable PM state. now 10 ms is enough
		 */
		if(status == POWER_OFF)
			mdelay(10);

		gpio_set_value(pm_data->gpio_host_ready, status);
	}
}

#define DEV_POWER_WAIT_SPIN	200
#define DEV_POWER_WAIT_MS	10
int wait_dev_pwr_stat(struct mdm_hsic_pm_data *pm_data, enum pwr_stat status)
{
	/* in shutdown(including modem fatal) do not need to wait dev ready */
	if (pm_data->shutdown)
		return 0;

	pr_info("%s:[%s]...\n", __func__, status ? "PWR ON" : "PWR OFF");

	if (pm_data->gpio_device_ready >= 0) {
		int spin;
		for (spin = 0; spin < DEV_POWER_WAIT_SPIN ; spin++) {
			if (gpio_get_value(pm_data->gpio_device_ready) ==
								status)
				break;
			else
				mdelay(DEV_POWER_WAIT_MS);
		}
	}

	if (gpio_get_value(pm_data->gpio_device_ready) == status)
		pr_info(" done\n");
	else {
		//subsystem_restart(EXTERNAL_MODEM);
		pr_info("%s: host ready val : %08X\n", __func__,
				gpio_get_value(pm_data->gpio_host_ready));
		pr_info("%s: dev  ready val : %08X\n", __func__,
				gpio_get_value(pm_data->gpio_device_ready));
		return -ETIMEDOUT;
	}
	return 0;
}

static int mdm_hsic_pm_phy_notify(struct notifier_block *nfb,
						unsigned long event, void *arg)
{
	struct mdm_hsic_pm_data *pm_data =
		container_of(nfb, struct mdm_hsic_pm_data, phy_nfb);
	int ret = 0;

	/* in shutdown(including modem fatal) do not need to wait dev ready */
	if (pm_data->shutdown)
		return 0;

	switch (event) {
	case STATE_HSIC_RESUME:
		set_host_stat(pm_data, POWER_ON);
		ret = wait_dev_pwr_stat(pm_data, POWER_ON);
		break;
	case STATE_HSIC_SUSPEND:
	case STATE_HSIC_LPA_ENTER:
		set_host_stat(pm_data, POWER_OFF);
		ret = wait_dev_pwr_stat(pm_data, POWER_OFF);
		if (ret) {
			set_host_stat(pm_data, POWER_ON);
			/* pm_runtime_resume(pm_data->udev->dev.parent->parent); */
			return ret;
		}
		break;
	case STATE_HSIC_LPA_WAKE:
		lpa_handling = true;
		pr_info("%s: set lpa handling to true\n", __func__);
		request_active_lock_set("15510000.mdmpm_pdata");
		pr_info("set hsic lpa wake\n");
		break;
	case STATE_HSIC_LPA_PHY_INIT:
		pr_info("set hsic lpa phy init\n");
		break;
	case STATE_HSIC_LPA_CHECK:
#if 0
		if (lpcharge)
			return 0;
		else
#endif
			if (!get_pm_data_by_dev_name("15510000.mdmpm_pdata"))
				return 1;
			else
				return 0;
	case STATE_HSIC_LPA_ENABLE:
#if 0
		if (lpcharge)
			return 0;
		else
#endif
		return pm_data->shutdown;
	default:
		pr_info("unknown lpa state\n");
		break;
	}

	return NOTIFY_DONE;
}



/**
 * check suspended state for L3 drive
 * if not, L3 blocked and stay at L2 / L0 state
 */
int check_udev_suspend_allowed(const char *name)
{
	struct mdm_hsic_pm_data *pm_data = get_pm_data_by_dev_name(name);
	struct device *dev;

	if (!pm_data) {
		pr_err("%s:no pm device(%s) exist\n", __func__, name);
		return -ENODEV;
	}
	if (!pm_data->intf_cnt || !pm_data->udev)
		return -ENODEV;

	dev = &pm_data->udev->dev;

	pr_info("%s:state_busy = %d, suspended = %d(rpmstat = %d:dpth:%d),"
		" suspended_child = %d\n", __func__, pm_data->state_busy,
		pm_runtime_suspended(dev), dev->power.runtime_status,
		dev->power.disable_depth, pm_children_suspended(dev));

	if (pm_data->state_busy)
		return -EBUSY;

	return pm_runtime_suspended(dev) && pm_children_suspended(dev);
}


bool mdm_check_main_connect(const char *name)
{
	/* find pm device from list by name */
	struct mdm_hsic_pm_data *pm_data = get_pm_data_by_dev_name(name);

	if (!pm_data) {
		pr_err("%s:no pm device(%s)\n", __func__, name);
		return false;
	}

	print_pm_dev_info(pm_data);

	if (pm_data->intf_cnt >= 1)
		return true;
	else
		return false;
}

#define PM_START_DELAY_MS 3000
int register_udev_to_pm_dev(const char *name, struct usb_device *udev)
{
	/* find pm device from list by name */
	struct mdm_hsic_pm_data *pm_data = get_pm_data_by_dev_name(name);

	if (!pm_data) {
		pr_err("%s:no pm device(%s) exist for udev(0x%p)\n",
					__func__, name, udev);
		return -ENODEV;
	}

	print_pm_dev_info(pm_data);

	if (!pm_data->intf_cnt) {
		pr_info("%s: registering new udev(0x%p) to %s\n", __func__,
							udev, pm_data->name);
		pm_data->udev = udev;
		atomic_set(&pm_data->pmlock_cnt, 0);
		usb_disable_autosuspend(udev);
		pm_data->shutdown = false;
#ifdef CONFIG_SIM_DETECT
		get_sim_state_at_boot();
#endif
	} else if (pm_data->udev && pm_data->udev != udev) {
		pr_err("%s:udev mismatching: pm_data->udev(0x%p), udev(0x%p)\n",
		__func__, pm_data->udev, udev);
		return -EINVAL;
	}

	pm_data->intf_cnt++;
	pr_info("%s:udev(0x%p) successfully registerd to %s, intf count = %d\n",
			__func__, udev, pm_data->name, pm_data->intf_cnt);

	queue_delayed_work(pm_data->wq, &pm_data->auto_rpm_start_work,
					msecs_to_jiffies(PM_START_DELAY_MS));
	return 0;
}

int set_qmicm_mode(const char *name)
{
	/* find pm device from list by name */
	struct mdm_hsic_pm_data *pm_data = get_pm_data_by_dev_name(name);

	if (!pm_data) {
		pr_err("%s:no pm device(%s) exist\n", __func__, name);
		return -ENODEV;
	}

	pm_data->qmicm_mode = true;
	pr_info("%s: set QMICM mode\n", __func__);

	return 0;
}

int get_qmicm_mode(const char *name)
{
	/* find pm device from list by name */
	struct mdm_hsic_pm_data *pm_data = get_pm_data_by_dev_name(name);

	if (!pm_data) {
		pr_err("%s:no pm device(%s) exist\n", __func__, name);
		return -ENODEV;
	}

	return pm_data->qmicm_mode;
}

/* force fatal for debug when HSIC disconnect */
//extern void mdm_force_fatal(void);

void unregister_udev_from_pm_dev(const char *name, struct usb_device *udev)
{
	/* find pm device from list by name */
	struct mdm_hsic_pm_data *pm_data = get_pm_data_by_dev_name(name);
	struct device *hdev;

	pr_info("%s\n", __func__);

	if (!pm_data) {
		pr_err("%s:no pm device(%s) exist for udev(0x%p)\n",
					__func__, name, udev);
		return;
	}

	if (!pm_data->shutdown) {
		hdev = udev->bus->root_hub->dev.parent;
		pm_runtime_forbid(hdev); /*ehci*/
		debug_ehci_reg_dump();
	}

	if (pm_data->udev && pm_data->udev != udev) {
		pr_err("%s:udev mismatching: pm_data->udev(0x%p), udev(0x%p)\n",
		__func__, pm_data->udev, udev);
		return;
	}

	pm_data->intf_cnt--;
	pr_info("%s:udev(0x%p) unregistered from %s, intf count = %d\n",
			__func__, udev, pm_data->name, pm_data->intf_cnt);

	if (!pm_data->intf_cnt) {
		pr_info("%s: all intf device unregistered from %s\n",
						__func__, pm_data->name);
		pm_data->udev = NULL;
#if  0
		/* force fatal for debug when HSIC disconnect */
		if (!pm_data->shutdown) {
			call_mdm_start_ssr();
		}
#endif
	}
}

static void mdm_hsic_rpm_check(struct work_struct *work)
{
	struct mdm_hsic_pm_data *pm_data =
			container_of(work, struct mdm_hsic_pm_data,
					request_resume_work.work);
	struct device *dev;

	if (pm_data->shutdown)
		return;

	pr_info("%s\n", __func__);

	if (!pm_data->udev)
		return;

	if (lpa_handling) {
		pr_info("ignore resume req, lpa handling\n");
		return;
	}

	dev = &pm_data->udev->dev;

	if (pm_runtime_resume(dev) < 0)
		queue_delayed_work(pm_data->wq, &pm_data->request_resume_work,
							msecs_to_jiffies(20));

	if (pm_runtime_suspended(dev))
		queue_delayed_work(pm_data->wq, &pm_data->request_resume_work,
							msecs_to_jiffies(20));
};

static void mdm_hsic_rpm_start(struct work_struct *work)
{
	struct mdm_hsic_pm_data *pm_data =
			container_of(work, struct mdm_hsic_pm_data,
					auto_rpm_start_work.work);
	struct usb_device *udev = pm_data->udev;
	struct device *dev, *pdev, *hdev;

	pr_info("%s\n", __func__);

	if (!pm_data->intf_cnt || !pm_data->udev)
		return;

	dev = &pm_data->udev->dev;
	pdev = dev->parent;
	pm_runtime_set_autosuspend_delay(dev, 500);
	hdev = udev->bus->root_hub->dev.parent;
	pr_info("EHCI runtime %s, %s\n", dev_driver_string(hdev),
			dev_name(hdev));

	pm_runtime_allow(dev);	/* usb 1-2 */
	pm_runtime_allow(pdev);	/* usb 1 */
	pm_runtime_allow(hdev);	/* ehci */

	pm_data->shutdown = false;
}

static void mdm_hsic_rpm_restart(struct work_struct *work)
{
	struct mdm_hsic_pm_data *pm_data =
			container_of(work, struct mdm_hsic_pm_data,
					auto_rpm_restart_work.work);
	struct device *dev;

	pr_info("%s\n", __func__);

	if (!pm_data->intf_cnt || !pm_data->udev)
		return;

	dev = &pm_data->udev->dev;
	pm_runtime_set_autosuspend_delay(dev, 500);
}

static void fast_dormancy_func(struct work_struct *work)
{
	struct mdm_hsic_pm_data *pm_data =
			container_of(work, struct mdm_hsic_pm_data,
					fast_dormancy_work.work);
	pr_debug("%s\n", __func__);

	if (!pm_data || !pm_data->fd_wake_time)
		return;

	wake_lock_timeout(&pm_data->fd_wake, pm_data->fd_wake_time);
};

void fast_dormancy_wakelock(const char *name)
{
	struct mdm_hsic_pm_data *pm_data = get_pm_data_by_dev_name(name);

	if (!pm_data || !pm_data->fd_wake_time)
		return;

	queue_delayed_work(pm_data->wq, &pm_data->fast_dormancy_work, 0);
}

static ssize_t show_waketime(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mdm_hsic_pm_data *pm_data = platform_get_drvdata(pdev);
	char *p = buf;
	unsigned int msec;

	if (!pm_data)
		return 0;

	msec = jiffies_to_msecs(pm_data->fd_wake_time);
	p += sprintf(p, "%u\n", msec);

	return p - buf;
}

static ssize_t store_waketime(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mdm_hsic_pm_data *pm_data = platform_get_drvdata(pdev);
	unsigned long msec;
	int r;

	if (!pm_data)
		return count;

	r = strict_strtoul(buf, 10, &msec);
	if (r)
		return count;

	pm_data->fd_wake_time = msecs_to_jiffies(msec);

	return count;
}
static DEVICE_ATTR(waketime, 0660, show_waketime, store_waketime);

static ssize_t store_runtime(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mdm_hsic_pm_data *pm_data = platform_get_drvdata(pdev);
	int value;

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;

	if (!pm_data || !pm_data->intf_cnt || !pm_data->udev)
		return -ENXIO;

	if (value == 1) {
		pr_info("%s: request runtime resume\n", __func__);
		if (pm_request_resume(&pm_data->udev->dev) < 0)
			pr_err("%s: unable to add pm work for rpm\n", __func__);
	}

	return count;
}
static DEVICE_ATTR(runtime, 0664, NULL, store_runtime);

static struct attribute *mdm_hsic_attrs[] = {
	&dev_attr_waketime.attr,
	&dev_attr_runtime.attr,
	NULL
};

static struct attribute_group mdm_hsic_attrgroup = {
	.attrs = mdm_hsic_attrs,
};

static int mdm_reset_notify_main(struct notifier_block *this,
				unsigned long event, void *ptr) {
	pr_info("%s\n", __func__);

	return NOTIFY_DONE;
};

static struct notifier_block mdm_reset_main_block = {
	.notifier_call = mdm_reset_notify_main,
};

static int mdm_hsic_pm_notify_event(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	struct mdm_hsic_pm_data *pm_data =
		container_of(this, struct mdm_hsic_pm_data, pm_notifier);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		/* to catch blocked resume req */
		pm_data->state_busy = false;
		pm_data->block_request = true;
		pr_info("%s: block request\n", __func__);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		pm_data->block_request = false;
		pr_info("%s: unblock request\n", __func__);

		if (pm_data->shutdown) {
			notify_modem_fatal();
			return NOTIFY_DONE;
		}
		/**
		 * cover L2 -> L3 broken and resume req blocked case :
		 * force resume request for the lost request
		 */
		/* pm_request_resume(&pm_data->udev->dev); */
		queue_delayed_work(pm_data->wq, &pm_data->request_resume_work,
							msecs_to_jiffies(20));
		/*pm_runtime_set_autosuspend_delay(&pm_data->udev->dev, 200);*/
		queue_delayed_work(pm_data->wq, &pm_data->auto_rpm_restart_work,
						msecs_to_jiffies(20));

		request_active_lock_set(pm_data->name);

		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static irqreturn_t mdm_device_ready_irq_handler(int irq, void *data)
{
	struct mdm_hsic_pm_data *pm_data = data;

	pr_info("%s, irq: host ready val : %08X\n", __func__, gpio_get_value(pm_data->gpio_host_ready));
	pr_info("%s, irq: dev  ready val : %08X\n", __func__, gpio_get_value(pm_data->gpio_device_ready));

	return IRQ_HANDLED;
}

#define HSIC_RESUME_TRIGGER_LEVEL	1
static irqreturn_t mdm_hsic_irq_handler(int irq, void *data)
{
	int irq_level;
	struct mdm_hsic_pm_data *pm_data = data;

	if (!pm_data || !pm_data->intf_cnt || !pm_data->udev)
		return IRQ_HANDLED;

	if (pm_data->shutdown)
		return IRQ_HANDLED;

	/**
	 * host wake up handler, takes both edge
	 * in rising, isr triggers L2 ->  L0 resume
	 */

	irq_level = gpio_get_value(pm_data->gpio_host_wake);
	pr_info("%s: detect %s edge\n", __func__,
					irq_level ? "Rising" : "Falling");

	if (irq_level != HSIC_RESUME_TRIGGER_LEVEL)
		return IRQ_HANDLED;

	if (pm_data->block_request) {
		pr_info("%s: request blocked by kernel suspending\n", __func__);
		pm_data->state_busy = true;
		/* for blocked request, set wakelock to return at dpm suspend */
		wake_lock(&pm_data->l2_wake);
		return IRQ_HANDLED;
	}
#if 0
	if (pm_request_resume(&pm_data->udev->dev) < 0)
		pr_err("%s: unable to add pm work for rpm\n", __func__);
	/* check runtime pm runs in Active state, after 100ms */
	queue_delayed_work(pm_data->wq, &pm_data->request_resume_work,
							msecs_to_jiffies(200));
#else
	queue_delayed_work(pm_data->wq, &pm_data->request_resume_work, 0);
#endif
	return IRQ_HANDLED;
}

static int parse_dt_gpio_pdata(struct device_node *np,
			struct  mdm_hsic_pm_data *pm_data)
{
	int ret = 0;

	/* host ready gpio */
	pm_data->gpio_host_ready = of_get_named_gpio(np,
				"qcom,ap2mdm-hostrdy-gpio", 0);
	if (!gpio_is_valid(pm_data->gpio_host_ready)) {
		pr_err("host ready: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pm_data->gpio_host_ready, "host_rdy");
	if (ret)
		pr_err("fail to request gpio %s:%d\n", "host_rdy", ret);

	pmdata_gpio_host_ready = pm_data->gpio_host_ready;
#if 0
	gpio_direction_output(pm_data->gpio_host_ready, 1);
	s3c_gpio_cfgpin(pm_data->gpio_host_ready, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(pm_data->gpio_host_ready, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(pm_data->gpio_host_ready,
					S5P_GPIO_DRVSTR_LV4);
	gpio_set_value(pm_data->gpio_host_ready, 1);
#endif
	/* device ready gpio */
	pm_data->gpio_device_ready =
		of_get_named_gpio(np, "qcom,mdm2ap-devicerdy-gpio", 0);
	if (!gpio_is_valid(pm_data->gpio_device_ready)) {
		pr_err("device ready: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pm_data->gpio_device_ready, "device_rdy");
	if (ret)
		pr_err("fail to request gpio %s:%d\n", "RESET_REQ_N", ret);
	gpio_direction_input(pm_data->gpio_device_ready);

	/* host wake gpio */
	pm_data->gpio_host_wake  =
		of_get_named_gpio(np, "qcom,mdm2ap-hostwake-gpio", 0);
	if (!gpio_is_valid(pm_data->gpio_host_wake)) {
		pr_err("host_wake: Invalied gpio pins\n");
		return -EINVAL;
	}
	if (pm_data->gpio_host_wake)
		pm_data->irq = gpio_to_irq(pm_data->gpio_host_wake);

	if (!pm_data->irq) {
		pr_err("fail to get host wake irq\n");
		return -EINVAL;
	}

	if (pm_data->gpio_device_ready >= 0)
		pm_data->dev_rdy_irq = gpio_to_irq(pm_data->gpio_device_ready);

	if (!pm_data->dev_rdy_irq) {
		pr_err("fail to get dev rdy irq\n");
		return -ENXIO;
	}

	return 0;
}

static struct mdm_hsic_pm_data *mdm_pm_parse_dt_pdata(struct device *dev)
{
	struct mdm_hsic_pm_data *pm_data;

	pm_data = kzalloc(sizeof(struct mdm_hsic_pm_data), GFP_KERNEL);
	if (!pm_data) {
		pr_err("pm_data: alloc fail\n");
		return ERR_PTR(-ENOMEM);
	}

	if (parse_dt_gpio_pdata(dev->of_node, pm_data)) {
		pr_err("DT error: failed to parse gpio\n");
		goto error;
	}

	dev->platform_data = pm_data;
	pr_info("DT parse complete!\n");
	return pm_data;
error:
	if (!pm_data) {
		kfree(pm_data);
	}
	return ERR_PTR(-EINVAL);
}

#ifdef EHCI_REG_DUMP
static int ehci_port_reg_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	if (ehci_reg) {
		pr_info("port reg aleady initialized\n");
		return -EBUSY;
	}

	ehci_reg = of_iomap(dev->of_node, 0);
	if (IS_ERR(ehci_reg)) {
		pr_err("fail to get port reg address\n");
		return -EINVAL;
	}
	pr_info("port reg get success (%p)\n", ehci_reg);

	if (phy_reg) {
		pr_info("phy reg aleady initialized\n");
		return -EBUSY;
	}

	phy_reg = of_iomap(dev->of_node, 1);
	if (IS_ERR(phy_reg)) {
		pr_err("fail to get phy reg address\n");
		return -EINVAL;
	}
	pr_info("phy reg get success (%p)\n", phy_reg);

	pmu_reg = of_iomap(dev->of_node, 2);
	if (IS_ERR(pmu_reg)) {
		pr_err("fail to get phy reg address\n");
	return -EINVAL;

	}
	pr_info("pmu reg get success (%p)\n", pmu_reg);

	clk_reg = of_iomap(dev->of_node, 3);
	if (IS_ERR(pmu_reg)) {
		pr_err("fail to get clk reg address\n");
	return -EINVAL;

	}
	pr_info("clk reg get success (%p)\n", pmu_reg);

	return 0;
}
#endif

#if 0
static int mdm_hsic_pm_gpio_init(struct mdm_hsic_pm_data *pm_data,
						struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	/* get gpio from platform data */

	/* host ready gpio */
	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					"AP2MDM_HSIC_ACTIVE");
	if (res)
		pm_data->gpio_host_ready = res->start;

	if (pm_data->gpio_host_ready) {
		ret = gpio_request(pm_data->gpio_host_ready, "host_rdy");
		if (ret < 0)
			return ret;
		gpio_direction_output(pm_data->gpio_host_ready, 1);
		s3c_gpio_cfgpin(pm_data->gpio_host_ready, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(pm_data->gpio_host_ready, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(pm_data->gpio_host_ready,
							S5P_GPIO_DRVSTR_LV4);
		gpio_set_value(pm_data->gpio_host_ready, 1);
	} else
		return -ENXIO;

	/* device ready gpio */
	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					"MDM2AP_DEVICE_PWR_ACTIVE");
	if (res)
		pm_data->gpio_device_ready = res->start;
	if (pm_data->gpio_device_ready) {
		ret = gpio_request(pm_data->gpio_device_ready, "device_rdy");
		if (ret < 0)
			return ret;
		gpio_direction_input(pm_data->gpio_device_ready);
		s3c_gpio_cfgpin(pm_data->gpio_device_ready, S3C_GPIO_INPUT);
	} else
		return -ENXIO;

	if (pm_data->gpio_device_ready)
		pm_data->dev_rdy_irq = gpio_to_irq(pm_data->gpio_device_ready);

	if (!pm_data->dev_rdy_irq) {
		pr_err("fail to get dev rdy irq\n");
		return -ENXIO;
	}

	/* host wake gpio */
	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					"MDM2AP_RESUME_REQ");
	if (res)
		pm_data->gpio_host_wake = res->start;
	if (pm_data->gpio_host_wake) {
		ret = gpio_request(pm_data->gpio_host_wake, "host_wake");
		if (ret < 0)
			return ret;
		gpio_direction_input(pm_data->gpio_host_wake);
		s3c_gpio_cfgpin(pm_data->gpio_host_wake, S3C_GPIO_SFN(0xF));
	} else
		return -ENXIO;

	if (pm_data->gpio_host_wake)
		pm_data->irq = gpio_to_irq(pm_data->gpio_host_wake);

	if (!pm_data->irq) {
		pr_err("fail to get host wake irq\n");
		return -ENXIO;
	}

	return 0;
}
#endif

static void mdm_hsic_pm_gpio_free(struct mdm_hsic_pm_data *pm_data)
{
	if (pm_data->gpio_host_ready)
		gpio_free(pm_data->gpio_host_ready);

	if (pm_data->gpio_device_ready)
		gpio_free(pm_data->gpio_device_ready);

	if (pm_data->gpio_host_wake)
		gpio_free(pm_data->gpio_host_wake);
}

#ifdef CONFIG_CPU_FREQ_TETHERING
static int link_pm_netdev_event(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	struct mdm_hsic_pm_data *pm_data =
		container_of(this, struct mdm_hsic_pm_data, netdev_notifier);
	struct mdm_hsic_pm_platform_data *mdm_pdata = pm_data->mdm_pdata;
	struct net_device *dev = ptr;

	if (!net_eq(dev_net(dev), &init_net))
		return NOTIFY_DONE;

	if (!strncmp(dev->name, "rndis", 5)) {
		switch (event) {
		case NETDEV_UP:
			pr_info("%s: %s UP\n", __func__, dev->name);
			if (mdm_pdata->freq_lock)
				mdm_pdata->freq_lock(mdm_pdata->dev);

			break;
		case NETDEV_DOWN:
			pr_info("%s: %s DOWN\n", __func__, dev->name);
			if (mdm_pdata->freq_unlock)
				mdm_pdata->freq_unlock(mdm_pdata->dev);
			break;
		}
	}
	return NOTIFY_DONE;
}

static int usb_composite_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct mdm_hsic_pm_data *pm_data =
		container_of(this, struct mdm_hsic_pm_data,
				usb_composite_notifier);
	struct mdm_hsic_pm_platform_data *mdm_pdata = pm_data->mdm_pdata;

	switch (event) {
	case 0:
		if (mdm_pdata->freq_unlock)
			mdm_pdata->freq_unlock(mdm_pdata->dev);
		pr_info("%s: USB detached\n", __func__);
		break;
	case 1:
		if (mdm_pdata->freq_lock)
			mdm_pdata->freq_lock(mdm_pdata->dev);
		pr_info("%s: USB attached\n", __func__);
		break;
	}
	pr_info("%s: usb configuration: %s\n", __func__, (char *)ptr);

	return NOTIFY_DONE;
}
#endif

static int mdm_hsic_pm_probe(struct platform_device *pdev)
{
	int ret;
	struct mdm_hsic_pm_data *pm_data = pdev->dev.platform_data;;

	pr_info("%s for %s\n", __func__, pdev->name);

	if (pdev->dev.of_node) {
		pm_data = mdm_pm_parse_dt_pdata(&pdev->dev);
		if (IS_ERR(pm_data)) {
			pr_err("MDM DT parse error!\n");
			goto err_gpio_init_fail;
		}
#ifdef EHCI_REG_DUMP
		ehci_port_reg_init(pdev);
#endif
	} else {
	if (!pm_data) {
			pr_err("MDM Non-DT, incorrect pdata!\n");
			return -EINVAL;
		}
	}

	memcpy(pm_data->name, pdev->name, strlen(pdev->name));
	/* request irq for host wake interrupt */
	ret = request_irq(pm_data->irq, mdm_hsic_irq_handler,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_DISABLED,
		"mdm_hsic_pm", (void *)pm_data);
	if (ret < 0) {
		pr_err("%s: fail to request mdm_hsic_pm irq(%d)\n", __func__, ret);
		goto err_request_irq;
	}

	ret = enable_irq_wake(pm_data->irq);
	if (ret < 0) {
		pr_err("%s: fail to set wake irq(%d)\n", __func__, ret);
		goto err_set_wake_irq;
	}

	ret = request_irq(pm_data->dev_rdy_irq, mdm_device_ready_irq_handler,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_DISABLED,
		"mdm_device_ready", (void *)pm_data);
	if (ret < 0) {
		pr_err("%s: fail to request mdm_device_ready irq(%d)\n", __func__, ret);
		goto err_request_irq;
	}

	pm_data->wq = create_singlethread_workqueue("hsicrpmd");
	if (!pm_data->wq) {
		pr_err("%s: fail to create wq\n", __func__);
		goto err_create_wq;
	}

	if (sysfs_create_group(&pdev->dev.kobj, &mdm_hsic_attrgroup) < 0) {
		pr_err("%s: fail to create sysfs\n", __func__);
		goto err_create_sys_file;
	}

	pm_data->mdm_pdata =
		(struct mdm_hsic_pm_platform_data *)pdev->dev.platform_data;
	INIT_DELAYED_WORK(&pm_data->auto_rpm_start_work, mdm_hsic_rpm_start);
	INIT_DELAYED_WORK(&pm_data->auto_rpm_restart_work,
							mdm_hsic_rpm_restart);
	INIT_DELAYED_WORK(&pm_data->request_resume_work, mdm_hsic_rpm_check);
	INIT_DELAYED_WORK(&pm_data->fast_dormancy_work, fast_dormancy_func);
	/* register notifier call */
	pm_data->pm_notifier.notifier_call = mdm_hsic_pm_notify_event;
	register_pm_notifier(&pm_data->pm_notifier);
	blocking_notifier_chain_register(&mdm_reset_notifier_list,
							&mdm_reset_main_block);

#ifdef CONFIG_CPU_FREQ_TETHERING
	pm_data->netdev_notifier.notifier_call = link_pm_netdev_event;
	register_netdevice_notifier(&pm_data->netdev_notifier);

	pm_data->usb_composite_notifier.notifier_call =
		usb_composite_notifier_event;
	register_usb_composite_notifier(&pm_data->usb_composite_notifier);
#endif
#if defined(CONFIG_OF)
	pm_data->phy_nfb.notifier_call = mdm_hsic_pm_phy_notify;
	phy_register_notifier(&pm_data->phy_nfb);
#endif

	wake_lock_init(&pm_data->l2_wake, WAKE_LOCK_SUSPEND, pm_data->name);
	wake_lock_init(&pm_data->boot_wake, WAKE_LOCK_SUSPEND, "mdm_boot");
	wake_lock_init(&pm_data->fd_wake, WAKE_LOCK_SUSPEND, "fast_dormancy");
	pm_data->fd_wake_time = DEFAULT_RAW_WAKE_TIME;
	pm_data->qmicm_mode = false;

	print_pm_dev_info(pm_data);
	list_add(&pm_data->list, &hsic_pm_dev_list);
	platform_set_drvdata(pdev, pm_data);
	pr_info("%s for %s has done\n", __func__, pdev->name);
	debug_ehci_reg_dump();

	return 0;

err_create_sys_file:
	destroy_workqueue(pm_data->wq);
err_create_wq:
	disable_irq_wake(pm_data->irq);
err_set_wake_irq:
	free_irq(pm_data->irq, (void *)pm_data);
err_request_irq:
err_gpio_init_fail:
	mdm_hsic_pm_gpio_free(pm_data);
	kfree(pm_data);
	return -ENXIO;
}
static struct of_device_id mdm_pm_match_table[] = {
	{.compatible = "qcom,mdm-hsic-pm"},
	{},
};
static struct platform_driver mdm_pm_driver = {
	.probe = mdm_hsic_pm_probe,
	.driver = {
		.name = "mdm_hsic_pm",
		.owner = THIS_MODULE,
		.of_match_table = mdm_pm_match_table,
	},
};

static int __init mdm_hsic_pm_init(void)
{
#if 1
	/* in lpm mode, do not load modem driver */
	if (lpcharge) {
		pr_info("[MIF] %s : lpcharge [%u], skip register\n", __func__, lpcharge);
		return 0;
	}
#endif
	pr_info("[MIF] %s\n", __func__);
	return platform_driver_register(&mdm_pm_driver);
}

static void __exit mdm_hsic_pm_exit(void)
{
	platform_driver_unregister(&mdm_pm_driver);
}

late_initcall(mdm_hsic_pm_init);
