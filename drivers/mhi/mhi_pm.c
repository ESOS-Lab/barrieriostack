/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "msm_mhi.h"
#include "mhi_sys.h"
#include "mhi.h"
#include "mhi_hwio.h"


extern int exynos_pcie_pm_suspend(int ch_num);
extern int exynos_pcie_pm_resume(int ch_num);

extern int exynos_pcie_save_config(struct pci_dev *dev);
extern int exynos_pcie_restore_config(struct pci_dev *dev);

/* Write only sysfs attributes */
static DEVICE_ATTR(MHI_M3, S_IWUSR, NULL, sysfs_init_M3);
static DEVICE_ATTR(MHI_M0, S_IWUSR, NULL, sysfs_init_M0);
static DEVICE_ATTR(MHI_M1, S_IWUSR, NULL, sysfs_init_M1);

/* Read only sysfs attributes */
static DEVICE_ATTR(MHI_STATE, S_IRUSR, sysfs_get_mhi_state, NULL);

static struct attribute *mhi_attributes[] = {
	&dev_attr_MHI_M3.attr,
	&dev_attr_MHI_M0.attr,
	&dev_attr_MHI_M1.attr,
	&dev_attr_MHI_STATE.attr,
	NULL,
};

static struct attribute_group mhi_attribute_group = {
	.attrs = mhi_attributes,
};

int mhi_suspend(struct pci_dev *pcie_dev, pm_message_t state)
{
	int r = 0;
	mhi_device_ctxt *mhi_dev_ctxt =
		*(mhi_device_ctxt **)((pcie_dev->dev).platform_data);
	mhi_log(MHI_MSG_ERROR, "[%s]\n", __func__);
	if (NULL == mhi_dev_ctxt)
		return -EINVAL;
	mhi_log(MHI_MSG_INFO, "Entered, sys state %d, MHI state %d\n",
			state.event, mhi_dev_ctxt->mhi_state);
	atomic_set(&mhi_dev_ctxt->flags.pending_resume, 1);
	r = cancel_work_sync(&mhi_dev_ctxt->m0_work);
	if (r) {
		atomic_set(&mhi_dev_ctxt->flags.m0_work_enabled, 0);
		mhi_log(MHI_MSG_INFO, "M0 work cancelled\n");
	}

	r = mhi_initiate_m3(mhi_dev_ctxt);
	if (!r)
		return r;

	atomic_set(&mhi_dev_ctxt->flags.pending_resume, 0);
	mhi_log(MHI_MSG_ERROR, "Failing suspend sequence ret: %d\n",
						r);
	return r;
}

int mhi_resume(struct pci_dev *pcie_dev)
{
	int r = 0;
	mhi_device_ctxt *mhi_dev_ctxt =
			*(mhi_device_ctxt **)((pcie_dev->dev).platform_data);
	mhi_log(MHI_MSG_ERROR, "[%s]\n", __func__);
	r = mhi_initiate_m0(mhi_dev_ctxt);
	if (r)
		goto exit;
	r = wait_event_interruptible_timeout(*mhi_dev_ctxt->M0_event,
			mhi_dev_ctxt->mhi_state == MHI_STATE_M0 ||
			mhi_dev_ctxt->mhi_state == MHI_STATE_M1,
			msecs_to_jiffies(MHI_MAX_SUSPEND_TIMEOUT));
	switch(r) {
		case 0:
			mhi_log(MHI_MSG_CRITICAL | MHI_DBG_POWER,
					"Timeout: No M0 event after %d ms\n",
					MHI_MAX_SUSPEND_TIMEOUT);
			mhi_dev_ctxt->counters.m0_event_timeouts++;
			r = -ETIME;
			break;
		case -ERESTARTSYS:
			mhi_log(MHI_MSG_CRITICAL | MHI_DBG_POWER,
					"Going Down...\n");
			break;
		default:
			mhi_log(MHI_MSG_INFO | MHI_DBG_POWER,
					"Wait complete state: %d\n", mhi_dev_ctxt->mhi_state);
			r = 0;
	}
exit:
	atomic_set(&mhi_dev_ctxt->flags.pending_resume, 0);
	return r;
}

enum hrtimer_restart mhi_initiate_M1(struct hrtimer *timer)
{
	int ret_val = 0;
	unsigned long flags;
	ktime_t curr_time, timer_inc;
	mhi_device_ctxt *mhi_dev_ctxt = container_of(timer,
			mhi_device_ctxt,
			m1_timer);
	write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);

	/* We will allow M1 if no data is pending, the current
	 * state is M0 and no M3 transition is pending */
	if ((0 == atomic_read(&mhi_dev_ctxt->flags.data_pending)) &&
			(MHI_STATE_M1 == mhi_dev_ctxt->mhi_state ||
			 MHI_STATE_M0 == mhi_dev_ctxt->mhi_state) &&
			(0 == mhi_dev_ctxt->flags.pending_M3) &&
			mhi_dev_ctxt->flags.mhi_initialized &&
			(0 == atomic_read(&mhi_dev_ctxt->counters.outbound_acks))) {
		mhi_dev_ctxt->mhi_state = MHI_STATE_M1;
		ret_val = mhi_deassert_device_wake(mhi_dev_ctxt);
		mhi_dev_ctxt->counters.m0_m1++;
		if (ret_val)
			mhi_log(MHI_MSG_ERROR | MHI_DBG_POWER,
					"Could not set DEVICE WAKE GPIO LOW\n");
	}
	write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);
	if (mhi_dev_ctxt->mhi_state == MHI_STATE_M0 ||
			mhi_dev_ctxt->mhi_state == MHI_STATE_M1 ||
			mhi_dev_ctxt->mhi_state == MHI_STATE_READY) {
		curr_time = ktime_get();
		timer_inc = ktime_set(0, MHI_M1_ENTRY_DELAY_MS * 1E6L);
		hrtimer_forward(timer, curr_time, timer_inc);
		return HRTIMER_RESTART;
	}
	return HRTIMER_NORESTART;
}

int mhi_init_pm_sysfs(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &mhi_attribute_group);
}

ssize_t sysfs_init_M3(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int r = 0;
	mhi_device_ctxt *mhi_dev_ctxt = mhi_devices.device_list[0].mhi_ctxt;
	r = mhi_initiate_m3(mhi_dev_ctxt);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL | MHI_DBG_POWER,
				"Failed to suspend %d\n", r);
		return r;
	}
	return count;
}

ssize_t sysfs_get_mhi_state(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return 0;
}

ssize_t sysfs_init_M0(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	mhi_device_ctxt *mhi_dev_ctxt = mhi_devices.device_list[0].mhi_ctxt;
	if (MHI_STATUS_SUCCESS != mhi_turn_on_pcie_link(mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_CRITICAL | MHI_DBG_POWER,
				"Failed to resume link\n");
		return count;
	}
	mhi_initiate_m0(mhi_dev_ctxt);
	mhi_log(MHI_MSG_CRITICAL | MHI_DBG_POWER,
			"Current mhi_state = 0x%x\n",
			mhi_dev_ctxt->mhi_state);

	return count;
}
ssize_t sysfs_init_M1(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	panic("MHI PANIC");
	return count;
}
MHI_STATUS mhi_turn_off_pcie_link(mhi_device_ctxt *mhi_dev_ctxt)
{
	int r;
	struct pci_dev *pcie_dev;
	MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	mhi_log(MHI_MSG_INFO, "Entered...\n");
	pcie_dev = mhi_dev_ctxt->dev_info->pcie_device;
	mutex_lock(&mhi_dev_ctxt->mhi_link_state);
	if (0 == mhi_dev_ctxt->flags.link_up) {
		mhi_log(MHI_MSG_CRITICAL | MHI_DBG_POWER,
				"Link already marked as down, nothing to do\n");
		goto exit;
	}



	/* Save pcie config space */
	r = exynos_pcie_save_config(mhi_dev_ctxt->dev_info->pcie_device);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL | MHI_DBG_POWER,
				"Failed to save pcie config space: %x\n", r);
	}

	r = pci_set_power_state(mhi_dev_ctxt->dev_info->pcie_device, PCI_D3hot);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL | MHI_DBG_POWER,
				"Failed to set pcie power state to D3 hotret: %x\n", r);
		ret_val = MHI_STATUS_ERROR;
		goto exit;
	}


	r = exynos_pcie_pm_suspend(0);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL | MHI_DBG_POWER,
				"Failed to suspend PCIe RC driver ret 0x%x\n", r);
		ret_val = MHI_STATUS_ERROR;
		goto exit;
	}
	// TODO MODIFY Temporary delay for MDM pcie down stability
	mdelay(10);

	mhi_dev_ctxt->flags.link_up = 0;
exit:
	mutex_unlock(&mhi_dev_ctxt->mhi_link_state);
	mhi_log(MHI_MSG_INFO, "Exited...\n");
	return ret_val;
}
MHI_STATUS mhi_turn_on_pcie_link(mhi_device_ctxt *mhi_dev_ctxt)
{
	int r = 0;
	struct pci_dev *pcie_dev;
	MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	pcie_dev = mhi_dev_ctxt->dev_info->pcie_device;

	mutex_lock(&mhi_dev_ctxt->mhi_link_state);
	mhi_log(MHI_MSG_INFO, "Entered...\n");
	if (mhi_dev_ctxt->flags.link_up)
		goto exit;

	r = exynos_pcie_pm_resume(0);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL | MHI_DBG_POWER,
				"Failed to resume PCIe RC driver ret %d\n", r);
		ret_val = MHI_STATUS_ERROR;
		goto exit;
	}

	atomic_dec(&mhi_dev_ctxt->flags.mhi_link_off);

	r = pci_set_power_state(mhi_dev_ctxt->dev_info->pcie_device,
			PCI_D0);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL | MHI_DBG_POWER,
				"Failed to load stored state ret %d\n", r);
		ret_val = MHI_STATUS_ERROR;
		goto exit;
	}

	/* Restore pcie config space */
	r = exynos_pcie_restore_config(mhi_dev_ctxt->dev_info->pcie_device);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL | MHI_DBG_POWER,
				"Failed to save pcie config space: %x\n", r);
		ret_val = MHI_STATUS_ERROR;
		goto exit;
	}

	mhi_dev_ctxt->flags.link_up = 1;
exit:
	mutex_unlock(&mhi_dev_ctxt->mhi_link_state);
	mhi_log(MHI_MSG_INFO, "Exited...\n");
	return ret_val;
}

