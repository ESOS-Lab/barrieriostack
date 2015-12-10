/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include "mhi_sys.h"
MHI_DEBUG_LEVEL mhi_msg_lvl = MHI_MSG_INFO;
MHI_DEBUG_LEVEL mhi_ipc_log_lvl = MHI_MSG_VERBOSE;
MHI_DEBUG_CLASS mhi_msg_class = MHI_DBG_DATA | MHI_DBG_POWER;

module_param(mhi_msg_lvl , uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_msg_lvl, "dbg lvl");
module_param(mhi_ipc_log_lvl, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_ipc_log_lvl, "dbg lvl");

module_param(mhi_msg_class , uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_msg_class, "dbg class");
u32 m3_timer_val_ms = 1000;
module_param(m3_timer_val_ms, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(m3_timer_val_ms, "timer val");

static ssize_t mhi_dbgfs_chan_read(struct file *fp, char __user *buf,
		size_t count, loff_t *offp);
static ssize_t mhi_dbgfs_state_read(struct file *fp, char __user *buf,
		size_t count, loff_t *offp);
static ssize_t mhi_dbgfs_ev_read(struct file *fp, char __user *buf,
		size_t count, loff_t *offp);
static ssize_t mhi_dbgfs_trigger_msi(struct file *fp, const char __user *buf,
		size_t count, loff_t *offp);

static const struct file_operations mhi_dbgfs_chan_fops = {
	.read = mhi_dbgfs_chan_read,
	.write = NULL,
};
static const struct file_operations mhi_dbgfs_ev_fops = {
	.read = mhi_dbgfs_ev_read,
	.write = NULL,
};

static const struct file_operations mhi_dbgfs_state_fops = {
	.read = mhi_dbgfs_state_read,
	.write = NULL,
};
static const struct file_operations mhi_dbgfs_trigger_msi_fops = {
	.read = NULL,
	.write = mhi_dbgfs_trigger_msi,
};
static ssize_t mhi_dbgfs_trigger_msi(struct file *fp, const char __user *buf,
		size_t count, loff_t *offp)
{
	u32 msi_nr = 0;
	void *irq_ctxt = &((mhi_devices.device_list[0]).pcie_device->dev);
	if(copy_from_user(&msi_nr, buf, sizeof(msi_nr)))
		return -ENOMEM;
	irq_cb(msi_nr, irq_ctxt);
	return 0;
}

static char chan_info[0x1000];
int mhi_init_debugfs(mhi_device_ctxt *mhi_dev_ctxt)
{
	struct dentry *mhi_parent_folder;
	struct dentry *mhi_chan_stats;
	struct dentry *mhi_state_stats;
	mhi_parent_folder = debugfs_create_dir("mhi", NULL);
	if (NULL == mhi_parent_folder) {
		mhi_log(MHI_MSG_INFO, "Failed to create debugfs parent dir.\n");
		return -EIO;
	}
	mhi_chan_stats = debugfs_create_file("mhi_chan_stats",
			0444,
			mhi_parent_folder,
			mhi_dev_ctxt,
			&mhi_dbgfs_chan_fops);
	mhi_chan_stats = debugfs_create_file("mhi_ev_stats",
			0444,
			mhi_parent_folder,
			mhi_dev_ctxt,
			&mhi_dbgfs_ev_fops);
	mhi_state_stats = debugfs_create_file("mhi_state_stats",
			0444,
			mhi_parent_folder,
			mhi_dev_ctxt,
			&mhi_dbgfs_state_fops);
	mhi_state_stats = debugfs_create_file("mhi_msi_trigger",
			0444,
			mhi_parent_folder,
			mhi_dev_ctxt,
			&mhi_dbgfs_trigger_msi_fops);
	return 0;
}
static ssize_t mhi_dbgfs_state_read(struct file *fp, char __user *buf,
		size_t count, loff_t *offp)
{
	int amnt_copied = 0;
	mhi_device_ctxt *mhi_dev_ctxt = mhi_devices.device_list[0].mhi_ctxt;
	if (NULL == mhi_dev_ctxt)
		return -EIO;
	msleep(100);
	amnt_copied =
		scnprintf(chan_info,
				sizeof(chan_info),
				"%s %u %s %d %s %d %s %d %s %d %s %d %s %d %s %d %s %d %s %d %s %d, %s, %d\n",
				"Our State:",
				mhi_dev_ctxt->mhi_state,
				"M0->M1:",
				mhi_dev_ctxt->counters.m0_m1,
				"M0<-M1:",
				mhi_dev_ctxt->counters.m1_m0,
				"M1->M2:",
				mhi_dev_ctxt->counters.m1_m2,
				"M0<-M2:",
				mhi_dev_ctxt->counters.m2_m0,
				"M0->M3:",
				mhi_dev_ctxt->counters.m0_m3,
				"M0<-M3:",
				mhi_dev_ctxt->counters.m3_m0,
				"M3_ev_TO:",
				mhi_dev_ctxt->counters.m3_event_timeouts,
				"M0_ev_TO:",
				mhi_dev_ctxt->counters.m0_event_timeouts,
				"MSI_d:",
				mhi_dev_ctxt->counters.msi_disable_cntr,
				"MSI_e:",
				mhi_dev_ctxt->counters.msi_enable_cntr,
				"outstanding_acks:",
				atomic_read(&mhi_dev_ctxt->counters.outbound_acks));
	if (amnt_copied < count)
		return amnt_copied - copy_to_user(buf, chan_info, amnt_copied);
	else
		return -ENOMEM;
}

static ssize_t mhi_dbgfs_chan_read(struct file *fp, char __user *buf,
		size_t count, loff_t *offp)
{
	int amnt_copied = 0;
	mhi_chan_ctxt *chan_ctxt;
	mhi_device_ctxt *mhi_dev_ctxt = mhi_devices.device_list[0].mhi_ctxt;
	uintptr_t v_wp_index;
	uintptr_t v_rp_index;
	if (NULL == mhi_dev_ctxt)
		return -EIO;
	*offp = (u32)(*offp) % MHI_MAX_CHANNELS;
	if (*offp == (MHI_MAX_CHANNELS - 1)) msleep(1000);
	while (!VALID_CHAN_NR(*offp)) {
		*offp += 1;
		*offp = (u32)(*offp) % MHI_MAX_CHANNELS;
	}

	get_element_index(&mhi_dev_ctxt->mhi_local_chan_ctxt[*offp],
			mhi_dev_ctxt->mhi_local_chan_ctxt[*offp].rp,
			&v_rp_index);
	get_element_index(&mhi_dev_ctxt->mhi_local_chan_ctxt[*offp],
			mhi_dev_ctxt->mhi_local_chan_ctxt[*offp].wp,
			&v_wp_index);
	chan_ctxt = &mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[*offp];
	amnt_copied =
		scnprintf(chan_info,
				sizeof(chan_info),
				"%s0x%x %s %d %s 0x%x %s 0x%llx %s %p %s %p %s %lu %s %p %s %lu %s %d %s %d\n",
				"chan:",
				(unsigned int)*offp,
				"pkts from dev:",
				mhi_dev_ctxt->mhi_chan_cntr[*offp].pkts_xferd,
				"state:",
				chan_ctxt->mhi_chan_state,
				"p_base:",
				chan_ctxt->mhi_trb_ring_base_addr,
				"v_base:",
				mhi_dev_ctxt->mhi_local_chan_ctxt[*offp].base,
				"v_wp:",
				mhi_dev_ctxt->mhi_local_chan_ctxt[*offp].wp,
				"index:",
				v_wp_index,
				"v_rp:",
				mhi_dev_ctxt->mhi_local_chan_ctxt[*offp].rp,
				"index:",
				v_rp_index,
				"pkts_queued",
				get_nr_avail_ring_elements(&mhi_dev_ctxt->mhi_local_chan_ctxt[*offp]),
				"/",
				mhi_get_chan_max_buffers(*offp));

	*offp += 1;

	if (amnt_copied < count)
		return amnt_copied -
			copy_to_user(buf, chan_info, amnt_copied);
	else
		return -ENOMEM;
}
static ssize_t mhi_dbgfs_ev_read(struct file *fp, char __user *buf,
		size_t count, loff_t *offp)
{
	int amnt_copied = 0;
	int event_ring_index = 0;
	mhi_event_ctxt *ev_ctxt;
	uintptr_t v_wp_index;
	uintptr_t v_rp_index;
	uintptr_t device_p_rp_index;

	mhi_device_ctxt *mhi_dev_ctxt = mhi_devices.device_list[0].mhi_ctxt;
	if (NULL == mhi_dev_ctxt)
		return -EIO;
	*offp = (u32)(*offp) % EVENT_RINGS_ALLOCATED;
	event_ring_index = mhi_dev_ctxt->alloced_ev_rings[*offp];
	ev_ctxt = &mhi_dev_ctxt->mhi_ctrl_seg->mhi_ec_list[event_ring_index];
	if (*offp == (EVENT_RINGS_ALLOCATED - 1))
	{
		msleep(1000);
	}


	get_element_index(&mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index],
			mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index].rp,
			&v_rp_index);
	get_element_index(&mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index],
			mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index].wp,
			&v_wp_index);
	get_element_index(&mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index],
			mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index].wp,
			&v_wp_index);
	get_element_index(&mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index],
			(void *)mhi_p2v_addr(mhi_dev_ctxt->mhi_ctrl_seg_info,
				ev_ctxt->mhi_event_read_ptr),
			&device_p_rp_index);

	amnt_copied =
		scnprintf(chan_info,
				sizeof(chan_info),
				"%s 0x%08x %s %02x %s 0x%08x %s 0x%08x %s 0x%llx %s %llx %s %lu %s %p %s %p %s %lu %s %p %s %lu\n",
				"Event Context ",
				(unsigned int)event_ring_index,
				"Intmod_T",
				MHI_GET_EV_CTXT(EVENT_CTXT_INTMODT, ev_ctxt),
				"MSI Vector",
				ev_ctxt->mhi_msi_vector,
				"MSI RX Count",
				mhi_dev_ctxt->msi_counter[*offp],
				"p_base:",
				ev_ctxt->mhi_event_ring_base_addr,
				"p_rp:",
				ev_ctxt->mhi_event_read_ptr,
				"index:",
				device_p_rp_index,
				"v_base:",
				mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index].base,
				"v_wp:",
				mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index].wp,
				"index:",
				v_wp_index,
				"v_rp:",
				mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index].rp,
				"index:",
				v_rp_index);

	*offp += 1;
	if (amnt_copied < count)
		return amnt_copied -
			copy_to_user(buf, chan_info, amnt_copied);
	else
		return -ENOMEM;
}
inline uintptr_t mhi_p2v_addr(mhi_meminfo *meminfo, uintptr_t pa)
{
	return meminfo->va_aligned + (pa - meminfo->pa_aligned);
}

inline uintptr_t mhi_v2p_addr(mhi_meminfo *meminfo, uintptr_t va)
{
	return meminfo->pa_aligned + (va - meminfo->va_aligned);
}
inline void *mhi_get_virt_addr(mhi_meminfo *meminfo)
{
	return (void *)meminfo->va_aligned;
}

inline void mhi_memcpy(void *to, void *from, size_t size)
{
	memcpy(to, from, size);
}

inline u64 mhi_get_memregion_len(mhi_meminfo *meminfo)
{
	return meminfo->size;
}

MHI_STATUS mhi_mallocmemregion(mhi_meminfo *meminfo, size_t size)
{
	meminfo->va_unaligned = (uintptr_t)dma_alloc_coherent(
			&((mhi_devices.device_list[0]).pcie_device->dev),
			size,
			(dma_addr_t *)&(meminfo->pa_unaligned),
			GFP_DMA);
	meminfo->va_aligned = meminfo->va_unaligned;
	meminfo->pa_aligned = meminfo->pa_unaligned;
	meminfo->size = size;
	if ((meminfo->pa_unaligned + size) >= MHI_DATA_SEG_WINDOW_END_ADDR)
		return MHI_STATUS_ERROR;

	if (0 == meminfo->va_unaligned)
		return MHI_STATUS_ERROR;
	mb();
	return MHI_STATUS_SUCCESS;
}

void mhi_freememregion(mhi_meminfo *meminfo)
{
	mb();
	dma_free_coherent(meminfo->dev,
			meminfo->size,
			(dma_addr_t *)&meminfo->pa_unaligned,
			GFP_DMA);

	meminfo->va_aligned = 0;
	meminfo->pa_aligned = 0;
	meminfo->va_unaligned = 0;
	meminfo->pa_unaligned = 0;
	return;
}
void print_ring(mhi_ring *local_chan_ctxt, u32 ring_id)
{
	u32 i = 0;
	mhi_log(MHI_MSG_VERBOSE,
			"Chan %d, ACK_RP 0x%p RP 0x%p WP 0x%p BASE 0x%p:\n",
			ring_id,
			local_chan_ctxt->ack_rp,
			local_chan_ctxt->rp,
			local_chan_ctxt->wp,
			local_chan_ctxt->base);
	for (i = 0; i < MAX_NR_TRBS_PER_SOFT_CHAN; ++i) {
		mhi_log(MHI_MSG_VERBOSE, "0x%x: TRB: 0x%p ",
				i, &((mhi_tx_pkt *)local_chan_ctxt->base)[i]);
		mhi_log(MHI_MSG_VERBOSE,
				"Buff Ptr = 0x%llx, Buf Len: 0x%x, Buf Flags: 0x%x\n",
				((mhi_tx_pkt *)(local_chan_ctxt->base))[i].buffer_ptr,
				((mhi_tx_pkt *)(local_chan_ctxt->base))[i].buf_len,
				((mhi_tx_pkt *)(local_chan_ctxt->base))[i].info);
	}

}
