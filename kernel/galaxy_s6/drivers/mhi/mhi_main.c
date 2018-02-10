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
#include "mhi.h"
#include "mhi_hwio.h"
#include "mhi_macros.h"

extern struct pci_driver mhi_pcie_driver;

int mhi_init_pcie_device(mhi_pcie_dev_info *mhi_pcie_dev)
{
	int ret_val = 0;
	long int sleep_time = 100000;
	struct pci_dev *pcie_device =
			(struct pci_dev *)mhi_pcie_dev->pcie_device;
	/* Enable the device */
	do {
		ret_val = pci_enable_device(mhi_pcie_dev->pcie_device);
		if (0 != ret_val) {
			mhi_log(MHI_MSG_ERROR,
				"Failed to enable pcie device ret_val %d\n",
				ret_val);
			mhi_log(MHI_MSG_ERROR,
				"Sleeping for ~ %li uS, and retrying.\n",
				sleep_time);
			usleep_range(sleep_time,sleep_time);
		}
	} while (ret_val != 0);

	mhi_log(MHI_MSG_INFO, "Successfully enabled pcie device.\n");

	mhi_pcie_dev->core.bar0_base =
		ioremap_nocache(pci_resource_start(pcie_device, 0),
			pci_resource_len(pcie_device, 0));
	mhi_pcie_dev->core.bar0_end = mhi_pcie_dev->core.bar0_base +
		pci_resource_len(pcie_device, 0);
	mhi_pcie_dev->core.bar2_base =
		ioremap_nocache(pci_resource_start(pcie_device, 2),
			pci_resource_len(pcie_device, 2));
	mhi_pcie_dev->core.bar2_end = mhi_pcie_dev->core.bar2_base +
		pci_resource_len(pcie_device, 2);

	if (0 == mhi_pcie_dev->core.bar0_base) {
		mhi_log(MHI_MSG_ERROR,
			"Failed to register for pcie resources\n");
		goto mhi_pcie_read_ep_config_err;
	}

	mhi_log(MHI_MSG_INFO, "Device BAR0 address is at 0x%p\n",
			mhi_pcie_dev->core.bar0_base);
	ret_val = pci_request_region(pcie_device, 0, mhi_pcie_driver.name);
	if (ret_val)
		mhi_log(MHI_MSG_ERROR, "Could not request BAR0 region\n");

	mhi_pcie_dev->core.manufact_id = pcie_device->vendor;
	mhi_pcie_dev->core.dev_id = pcie_device->device;

	if (mhi_pcie_dev->core.manufact_id != MHI_PCIE_VENDOR_ID ||
			mhi_pcie_dev->core.dev_id != MHI_PCIE_DEVICE_ID) {
		mhi_log(MHI_MSG_ERROR, "Incorrect device/manufacturer ID\n");
		goto mhi_device_list_error;
	}
	/* We need to ensure that the link is stable before we kick off MHI */
	return 0;
mhi_device_list_error:
	pci_disable_device(pcie_device);
mhi_pcie_read_ep_config_err:
	return -EIO;
}

static void mhi_move_interrupts(mhi_device_ctxt *mhi_dev_ctxt, u32 cpu)
{
	irq_set_affinity(250, get_cpu_mask(cpu));
}

int mhi_cpu_notifier_cb(struct notifier_block *nfb, unsigned long action,
			void *hcpu)
{
	u64 cpu = (u64)hcpu;
	mhi_device_ctxt *mhi_dev_ctxt = container_of(nfb,
						mhi_device_ctxt,
						mhi_cpu_notifier);
	if (NULL == mhi_dev_ctxt)
		return NOTIFY_BAD;

	switch(action) {
		case CPU_ONLINE:
			cpu = 3;
			while(cpu > 0) {
				if (cpu_online(cpu)) {
					mhi_move_interrupts(mhi_dev_ctxt, cpu);
					break;
				}
				cpu--;
			}
			break;

		case CPU_DEAD:
			cpu = 3;
			while(cpu > 0) {
				if (cpu_online(cpu)) {
					mhi_move_interrupts(mhi_dev_ctxt, cpu);
					break;
				}
				cpu--;
			}
			break;

		default:
			break;
	}
	return NOTIFY_OK;
}

int mhi_init_gpios(mhi_pcie_dev_info *mhi_pcie_dev)
{
	int ret_val = 0;
	struct device *dev = &mhi_pcie_dev->pcie_device->dev;
	struct device_node *np;
	np = dev->of_node;
	mhi_log(MHI_MSG_VERBOSE,
			"Attempting to grab DEVICE_WAKE gpio\n");
	if (!of_find_property(np, "mhi-device-wake-gpio", NULL))
		mhi_log(MHI_MSG_CRITICAL,
			"Could not find device wake gpio prop, in dt.\n");
	else
		mhi_log(MHI_MSG_VERBOSE, "Found gpio as a property in DT\n");
	ret_val = of_get_named_gpio(np, "mhi-device-wake-gpio", 0);
	switch (ret_val) {
	case -EPROBE_DEFER:
		mhi_log(MHI_MSG_VERBOSE, "DT is not ready\n");
		return ret_val;
		break;
	case 0:
		mhi_log(MHI_MSG_CRITICAL,
			"Could not get gpio from device tree!\n");
		return -EIO;
		break;
	default:
		mhi_pcie_dev->core.device_wake_gpio = ret_val;
		mhi_log(MHI_MSG_CRITICAL,
			"Got DEVICE_WAKE GPIO nr 0x%x from device tree\n",
			mhi_pcie_dev->core.device_wake_gpio);
		break;
	}

	ret_val = gpio_request(mhi_pcie_dev->core.device_wake_gpio, "mhi");
	if (ret_val) {
		mhi_log(MHI_MSG_VERBOSE,
			"Could not obtain device WAKE gpio\n");
	}
	mhi_log(MHI_MSG_VERBOSE,
		"Attempting to set output direction to DEVICE_WAKE gpio\n");
	/* This GPIO must never sleep as it can be set in timer ctxt */
	gpio_set_value_cansleep(mhi_pcie_dev->core.device_wake_gpio, 0);
	if (ret_val)
		mhi_log(MHI_MSG_VERBOSE,
		"Could not set GPIO to not sleep!\n");

	ret_val = gpio_direction_output(mhi_pcie_dev->core.device_wake_gpio, 1);

	if (ret_val) {
		mhi_log(MHI_MSG_VERBOSE,
			"Failed to set output direction of DEVICE_WAKE gpio\n");
		goto mhi_gpio_dir_err;
	}
	return 0;

mhi_gpio_dir_err:
	gpio_free(mhi_pcie_dev->core.device_wake_gpio);
	return -EIO;
}
MHI_STATUS mhi_open_channel(mhi_client_handle **client_handle,
		MHI_CLIENT_CHANNEL chan, s32 device_index,
		mhi_client_info_t *client_info, void *UserData)
{
	MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	mhi_control_seg *mhi_ctrl_seg = NULL;

	if (!VALID_CHAN_NR(chan)) {
		ret_val = MHI_STATUS_INVALID_CHAN_ERR;
		goto error_handle;
	}
	if (NULL == client_handle || device_index < 0 ||
			device_index >= mhi_devices.nr_of_devices) {
		ret_val = MHI_STATUS_ERROR;
		goto error_handle;
	}
	mhi_log(MHI_MSG_INFO,
			"Opened channel 0x%x for client\n", chan);

	atomic_inc(&mhi_devices.device_list[device_index].ref_count);

	*client_handle = kmalloc(sizeof(mhi_client_handle), GFP_KERNEL);
	if (NULL == *client_handle) {
		ret_val = MHI_STATUS_ALLOC_ERROR;
		goto error_handle;
	}
	memset(*client_handle, 0, sizeof(mhi_client_handle));
	(*client_handle)->chan = chan;
	(*client_handle)->mhi_dev_ctxt =
		mhi_devices.device_list[device_index].mhi_ctxt;
	mhi_ctrl_seg = (*client_handle)->mhi_dev_ctxt->mhi_ctrl_seg;

	(*client_handle)->mhi_dev_ctxt->client_handle_list[chan] =
		*client_handle;
	if (NULL != client_info)
		(*client_handle)->client_info = *client_info;

	(*client_handle)->user_data = UserData;
	(*client_handle)->event_ring_index =
		mhi_ctrl_seg->mhi_cc_list[chan].mhi_event_ring_index;
	init_completion(&(*client_handle)->chan_close_complete);
	(*client_handle)->msi_vec =
		mhi_ctrl_seg->mhi_ec_list[
		(*client_handle)->event_ring_index].mhi_msi_vector;
	if (NULL != client_info && client_info->cb_mod != 0)
		(*client_handle)->cb_mod = client_info->cb_mod;
	else
		(*client_handle)->cb_mod = 1;

	if (MHI_CLIENT_IP_HW_0_OUT  == chan)
		(*client_handle)->intmod_t = 10;
	if (MHI_CLIENT_IP_HW_0_IN  == chan)
		(*client_handle)->intmod_t = 10;
	mhi_log(MHI_MSG_VERBOSE,
			"Successfuly started chan 0x%x\n", chan);

	mhi_log(MHI_MSG_INFO, "Move interrupts to CPU3\n");
	mhi_move_interrupts((*client_handle)->mhi_dev_ctxt, 3);
error_handle:
	return ret_val;
}

void mhi_close_channel(mhi_client_handle *mhi_handle)
{
	u32 index = 0;
	u32 chan = 0;
	if (NULL == mhi_handle)
		return;
	chan = mhi_handle->chan;
	mhi_log(MHI_MSG_INFO, "Client attempting to close chan 0x%x\n", chan);
	index = mhi_handle->device_index;
	mhi_log(MHI_MSG_INFO, "Chan 0x%x confirmed closed.\n", chan);
	mhi_handle->mhi_dev_ctxt->client_handle_list[mhi_handle->chan] = NULL;
	atomic_dec(&(mhi_devices.device_list[index].ref_count));
	kfree(mhi_handle);
}

void ring_ev_db(mhi_device_ctxt *mhi_dev_ctxt, u32 event_ring_index)
{
	mhi_ring *event_ctxt = NULL;
	u64 db_value = 0;
	event_ctxt =
		&mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index];
	db_value = mhi_v2p_addr(mhi_dev_ctxt->mhi_ctrl_seg_info,
			(uintptr_t)event_ctxt->wp);
	MHI_WRITE_DB(mhi_dev_ctxt, mhi_dev_ctxt->event_db_addr,
			event_ring_index, db_value);
}
/**
 * @brief Add elements to event ring for the device to use
 *
 * @param mhi device context
 *
 * @return MHI_STATUS
 */
MHI_STATUS mhi_add_elements_to_event_rings(mhi_device_ctxt *mhi_dev_ctxt,
		STATE_TRANSITION new_state)
{
	MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	MHI_EVENT_RING_STATE event_ring_state = MHI_EVENT_RING_UINIT;
	switch (new_state) {
	case STATE_TRANSITION_READY:
		MHI_GET_EVENT_RING_INFO(EVENT_RING_STATE_FIELD,
				mhi_dev_ctxt->ev_ring_props[PRIMARY_EVENT_RING],
				event_ring_state);
		if (MHI_EVENT_RING_UINIT == event_ring_state) {
		ret_val = mhi_init_event_ring(mhi_dev_ctxt,
			   EV_EL_PER_RING,
			   mhi_dev_ctxt->alloced_ev_rings[PRIMARY_EVENT_RING]);
		if (MHI_STATUS_SUCCESS != ret_val) {

			mhi_log(MHI_MSG_ERROR,
				"Failed to add ev el on event ring\n");
			return MHI_STATUS_ERROR;
			}
			MHI_SET_EVENT_RING_INFO(EVENT_RING_STATE_FIELD,
				mhi_dev_ctxt->ev_ring_props[PRIMARY_EVENT_RING],
				MHI_EVENT_RING_INIT);
		}
		mhi_log(MHI_MSG_ERROR,
			"Event ring initialized ringing, EV DB to resume\n");
		ring_ev_db(mhi_dev_ctxt,
			mhi_dev_ctxt->alloced_ev_rings[PRIMARY_EVENT_RING]);
		break;
	case STATE_TRANSITION_AMSS:
		MHI_GET_EVENT_RING_INFO(EVENT_RING_STATE_FIELD,
				mhi_dev_ctxt->ev_ring_props[IPA_OUT_EV_RING],
				event_ring_state);
		if (MHI_EVENT_RING_UINIT == event_ring_state) {
		ret_val = mhi_init_event_ring(mhi_dev_ctxt,
			 EV_EL_PER_RING,
			 mhi_dev_ctxt->alloced_ev_rings[IPA_OUT_EV_RING]);
		if (MHI_STATUS_SUCCESS != ret_val) {

			mhi_log(MHI_MSG_ERROR,
				"Failed to add ev el on event ring\n");
			return MHI_STATUS_ERROR;
		}
		ret_val = mhi_init_event_ring(mhi_dev_ctxt,
			  EV_EL_PER_RING,
			  mhi_dev_ctxt->alloced_ev_rings[IPA_IN_EV_RING]);
		if (MHI_STATUS_SUCCESS != ret_val) {
			mhi_log(MHI_MSG_ERROR,
				"Failed to add ev el on event ring\n");
			return MHI_STATUS_ERROR;
		}
		MHI_SET_EVENT_RING_INFO(EVENT_RING_STATE_FIELD,
				mhi_dev_ctxt->ev_ring_props[IPA_OUT_EV_RING],
				MHI_EVENT_RING_INIT);
		MHI_SET_EVENT_RING_INFO(EVENT_RING_STATE_FIELD,
				mhi_dev_ctxt->ev_ring_props[IPA_IN_EV_RING],
				MHI_EVENT_RING_INIT);
		}
		ring_ev_db(mhi_dev_ctxt,
			mhi_dev_ctxt->alloced_ev_rings[SOFTWARE_EV_RING]);
		ring_ev_db(mhi_dev_ctxt,
			mhi_dev_ctxt->alloced_ev_rings[IPA_OUT_EV_RING]);
		ring_ev_db(mhi_dev_ctxt,
			mhi_dev_ctxt->alloced_ev_rings[IPA_IN_EV_RING]);
	break;
	default:
		mhi_log(MHI_MSG_ERROR,
			"Unrecognized event stage, %d\n", new_state);
		ret_val = MHI_STATUS_ERROR;
		break;
	}
	return ret_val;
}
/**
 * @brief Add available TRBs to an IN channel
 *
 * @param device mhi device context
 * @param chan mhi channel number
 *
 * @return MHI_STATUS
 */

/**
 * @brief Function for sending data on an outbound channel.
 * This function only sends on TRE's worth of
 * data and may chain the TRE as specified by the caller.
 *
 * @param device [IN ] Pointer to mhi context used to send the TRE
 * @param chan [IN ] Channel number to send the TRE on
 * @param buf [IN ] Physical address of buffer to be linked to descriptor
 * @param buf_len [IN ] Length of buffer, which will be populated in the TRE
 * @param chain [IN ] Specification on whether this TRE should be chained
 *
 * @return MHI_STATUS
 */
MHI_STATUS mhi_queue_xfer(mhi_client_handle *client_handle,
		uintptr_t buf, size_t buf_len, u32 chain, u32 eob)
{
	mhi_xfer_pkt *pkt_loc;
	MHI_STATUS ret_val;
	MHI_CLIENT_CHANNEL chan;
	mhi_device_ctxt *mhi_dev_ctxt;
	unsigned long flags;
	int ret;
	if (NULL == client_handle || !VALID_CHAN_NR(client_handle->chan) ||
			0 == buf || chain >= MHI_TRE_CHAIN_LIMIT || 0 == buf_len) {
		if(client_handle == NULL)
			mhi_log(MHI_MSG_CRITICAL, "Client handle is NULL\n");

		mhi_log(MHI_MSG_CRITICAL, "Bad input args buf : %p, buf_len : %ld \n", (void *) buf, buf_len);
		return MHI_STATUS_ERROR;
	}
	ret = VALID_BUF(buf, buf_len);
	if(!ret) {
		pr_err("MHI ASSERT buf : %llx, size %ld\n",(dma_addr_t) buf, buf_len);
	}
	MHI_ASSERT(ret,
		"Client buffer is of invalid length");
	mhi_dev_ctxt = client_handle->mhi_dev_ctxt;
	chan = client_handle->chan;


	/* Bump up the vote for pending data */
	read_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);

	atomic_inc(&mhi_dev_ctxt->flags.data_pending);
	mhi_dev_ctxt->counters.m1_m0++;
	if (mhi_dev_ctxt->flags.link_up)
		mhi_assert_device_wake(mhi_dev_ctxt);
	read_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);


	pkt_loc = mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp;
	pkt_loc->data_tx_pkt.buffer_ptr = buf;

	if (likely(0 != client_handle->intmod_t))
		MHI_TRB_SET_INFO(TX_TRB_BEI, pkt_loc, 1);
	else
		MHI_TRB_SET_INFO(TX_TRB_BEI, pkt_loc, 0);

	MHI_TRB_SET_INFO(TX_TRB_IEOT, pkt_loc, 1);
	MHI_TRB_SET_INFO(TX_TRB_CHAIN, pkt_loc, chain);
	MHI_TRB_SET_INFO(TX_TRB_IEOB, pkt_loc, eob);
	MHI_TRB_SET_INFO(TX_TRB_TYPE, pkt_loc, MHI_PKT_TYPE_TRANSFER);
	MHI_TX_TRB_SET_LEN(TX_TRB_LEN, pkt_loc, buf_len);

	/* Add the TRB to the correct transfer ring */
	ret_val = ctxt_add_element(&mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
				(void *)&pkt_loc);
	if (unlikely(MHI_STATUS_SUCCESS != ret_val)) {
		mhi_log(MHI_MSG_INFO, "Failed to insert trb in xfer ring\n");
		goto error;
	}

	if (chan % 2 == 0) {
		atomic_inc(&mhi_dev_ctxt->counters.outbound_acks);
		mhi_log(MHI_MSG_VERBOSE,
				"Queued outbound pkt. Pending Acks %d\n",
				atomic_read(&mhi_dev_ctxt->counters.outbound_acks));
	}

	mhi_notify_device(mhi_dev_ctxt, chan);
	atomic_dec(&mhi_dev_ctxt->flags.data_pending);
	return MHI_STATUS_SUCCESS;
error:
	atomic_dec(&mhi_dev_ctxt->flags.data_pending);
	return ret_val;
}

MHI_STATUS mhi_notify_device(mhi_device_ctxt *mhi_dev_ctxt, u32 chan)
{
	unsigned long flags = 0;
	u64 db_value;
	mhi_chan_ctxt *chan_ctxt;
	chan_ctxt = &mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan];
	spin_lock_irqsave(&mhi_dev_ctxt->db_write_lock[chan], flags);
	if (likely(((MHI_STATE_M0 == mhi_dev_ctxt->mhi_state) ||
					(MHI_STATE_M1 == mhi_dev_ctxt->mhi_state)) &&
				(chan_ctxt->mhi_chan_state != MHI_CHAN_STATE_ERROR) &&
				!mhi_dev_ctxt->flags.pending_M3)) {

		mhi_dev_ctxt->mhi_chan_db_order[chan]++;
		db_value = mhi_v2p_addr(mhi_dev_ctxt->mhi_ctrl_seg_info,
				(uintptr_t)mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp);
		if (IS_HARDWARE_CHANNEL(chan) && (chan % 2)) {
			if ((mhi_dev_ctxt->mhi_chan_cntr[chan].pkts_xferd %
						MHI_XFER_DB_INTERVAL) == 0) {
				MHI_WRITE_DB(mhi_dev_ctxt,
						mhi_dev_ctxt->channel_db_addr,
						chan, db_value);
			}
		} else if(VALID_CHAN_NR(chan)){
			MHI_WRITE_DB(mhi_dev_ctxt,
					mhi_dev_ctxt->channel_db_addr,
					chan, db_value);
		}
	} else {
		mhi_log(MHI_MSG_VERBOSE,
			"Triggering wakeup due to pending data MHI state %d, Chan state %d, Pending M3 %d\n",
			mhi_dev_ctxt->mhi_state, chan_ctxt->mhi_chan_state, mhi_dev_ctxt->flags.pending_M3);
		if (mhi_dev_ctxt->flags.pending_M3 ||
				mhi_dev_ctxt->mhi_state == MHI_STATE_M3) {
			mhi_wake_dev_from_m3(mhi_dev_ctxt);
		}
	}
	spin_unlock_irqrestore(&mhi_dev_ctxt->db_write_lock[chan], flags);
	/* If there are no clients still sending we can trigger our
	 * inactivity timer */
	return MHI_STATUS_SUCCESS;
}

MHI_STATUS mhi_wake_dev_from_m3(mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 r;
	if (!atomic_cmpxchg(&mhi_dev_ctxt->flags.m0_work_enabled, 0, 1)) {
		mhi_log(MHI_MSG_INFO,
				"Initiating M0 work...\n");
		if (atomic_read(&mhi_dev_ctxt->flags.pending_resume)) {
			mhi_log(MHI_MSG_INFO,
			"Resume is pending, quitting ...\n");
			atomic_set(&mhi_dev_ctxt->flags.m0_work_enabled, 0);
			__pm_stay_awake(&mhi_dev_ctxt->wake_lock);
			__pm_relax(&mhi_dev_ctxt->wake_lock);
			return MHI_STATUS_SUCCESS;
		}
		r = queue_work(mhi_dev_ctxt->work_queue,
				&mhi_dev_ctxt->m0_work);
		if (!r)
			mhi_log(MHI_MSG_CRITICAL,
					"Failed to start M0 work.\n");
	} else {
		mhi_log(MHI_MSG_VERBOSE,
				"M0 work pending.\n");
	}
	return MHI_STATUS_SUCCESS;
}
/**
 * @brief Function used to send a command TRE to the mhi device.
 *
 * @param device [IN ] Specify the mhi dev context to which to send the command
 * @param cmd [IN ] Enum specifying which command to send to device
 * @param chan [in ] Channel number for which this command is intended,
 * not applicable for all commands
 *
 * @return MHI_STATUS
 */
MHI_STATUS mhi_send_cmd(mhi_device_ctxt *mhi_dev_ctxt,
		MHI_COMMAND cmd, u32 chan)
{
	u64 db_value = 0;
	mhi_cmd_pkt *cmd_pkt = NULL;
	MHI_CHAN_STATE from_state = MHI_CHAN_STATE_DISABLED;
	MHI_CHAN_STATE to_state = MHI_CHAN_STATE_DISABLED;
	MHI_PKT_TYPE ring_el_type = MHI_PKT_TYPE_NOOP_CMD;
	struct mutex *cmd_mutex = NULL;
	struct mutex *chan_mutex = NULL;

	if (chan >= MHI_MAX_CHANNELS ||
			cmd >= MHI_COMMAND_MAX_NR || NULL == mhi_dev_ctxt) {
		mhi_log(MHI_MSG_ERROR,
				"Invalid channel id, received id: 0x%x", chan);
		return MHI_STATUS_ERROR;
	}
	mhi_assert_device_wake(mhi_dev_ctxt);
	/*If there is a cmd pending a device confirmation, do not send anymore
	  for this channel */
	if (MHI_CMD_PENDING == mhi_dev_ctxt->mhi_chan_pend_cmd_ack[chan])
		return MHI_STATUS_CMD_PENDING;

	from_state =
		mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan].mhi_chan_state;

	switch (cmd) {
		case MHI_COMMAND_NOOP:
			{
				ring_el_type = MHI_PKT_TYPE_NOOP_CMD;
				break;
			}
		case MHI_COMMAND_RESET_CHAN:
			{
				to_state = MHI_CHAN_STATE_DISABLED;
				ring_el_type = MHI_PKT_TYPE_RESET_CHAN_CMD;
				break;
			}
		case MHI_COMMAND_START_CHAN:
			{
				switch (from_state) {
					case MHI_CHAN_STATE_ENABLED:
					case MHI_CHAN_STATE_STOP:
						to_state = MHI_CHAN_STATE_RUNNING;
						break;
					default:
						mhi_log(MHI_MSG_ERROR,
								"Invalid state transition for "
								"cmd 0x%x, from_state 0x%x\n",
								cmd, from_state);
						goto error_general;
				}
				ring_el_type = MHI_PKT_TYPE_START_CHAN_CMD;
				break;
			}
		case MHI_COMMAND_STOP_CHAN:
			{
				switch (from_state) {
					case MHI_CHAN_STATE_RUNNING:
					case MHI_CHAN_STATE_SUSPENDED:
						to_state = MHI_CHAN_STATE_STOP;
						break;
					default:
						mhi_log(MHI_MSG_ERROR,
								"Invalid state transition for "
								"cmd 0x%x, from_state 0x%x\n",
								cmd, from_state);
						goto error_general;
				}
				ring_el_type = MHI_PKT_TYPE_STOP_CHAN_CMD;
				break;
			}
		default:
			mhi_log(MHI_MSG_ERROR, "Bad command received\n");
	}

	cmd_mutex = &mhi_dev_ctxt->mhi_cmd_mutex_list[PRIMARY_CMD_RING];
	mutex_lock(cmd_mutex);

	if (MHI_STATUS_SUCCESS !=
			ctxt_add_element(mhi_dev_ctxt->mhi_local_cmd_ctxt,
				(void *)&cmd_pkt)) {
		mhi_log(MHI_MSG_ERROR, "Failed to insert element\n");
		goto error_general;
	}
	chan_mutex = &mhi_dev_ctxt->mhi_chan_mutex[chan];
	if (MHI_COMMAND_NOOP != cmd) {
		mutex_lock(chan_mutex);
		MHI_TRB_SET_INFO(CMD_TRB_TYPE, cmd_pkt, ring_el_type);
		MHI_TRB_SET_INFO(CMD_TRB_CHID, cmd_pkt, chan);
		mutex_unlock(chan_mutex);
	}
	db_value = mhi_v2p_addr(mhi_dev_ctxt->mhi_ctrl_seg_info,
			(uintptr_t)mhi_dev_ctxt->mhi_local_cmd_ctxt->wp);
	mhi_dev_ctxt->mhi_chan_pend_cmd_ack[chan] = MHI_CMD_PENDING;

	if (MHI_STATE_M0 == mhi_dev_ctxt->mhi_state ||
			MHI_STATE_M1 == mhi_dev_ctxt->mhi_state) {
		mhi_dev_ctxt->cmd_ring_order++;
		MHI_WRITE_DB(mhi_dev_ctxt, mhi_dev_ctxt->cmd_db_addr, 0, db_value);
	}
	mhi_log(MHI_MSG_INFO, "Sent command 0x%x for chan 0x%x\n", cmd, chan);
	mutex_unlock(&mhi_dev_ctxt->mhi_cmd_mutex_list[PRIMARY_CMD_RING]);

	return MHI_STATUS_SUCCESS;

error_general:
	mutex_unlock(&mhi_dev_ctxt->mhi_cmd_mutex_list[PRIMARY_CMD_RING]);
	return MHI_STATUS_ERROR;
}

/**
 * @brief Thread which handles inbound data for MHI clients.
 * This thread will invoke thecallback for the mhi clients to
 * inform thme of data availability.
 *
 * The thread monitors the MHI state variable to know if it should
 * continue processing, * or stop.
 *
 * @param ctxt void pointer to a device context
 */
MHI_STATUS parse_xfer_event(mhi_device_ctxt *ctxt, mhi_event_pkt *event)
{
	mhi_device_ctxt *mhi_dev_ctxt = (mhi_device_ctxt *)ctxt;
	mhi_result *result = NULL;
	u32 chan = MHI_MAX_CHANNELS;
	u16 xfer_len;
	uintptr_t phy_ev_trb_loc;
	mhi_xfer_pkt *local_ev_trb_loc;
	mhi_client_handle *client_handle;
	mhi_xfer_pkt *local_trb_loc;
	mhi_chan_ctxt *chan_ctxt;
	u32 nr_trb_to_parse;
	u32 i = 0;

	switch (MHI_EV_READ_CODE(EV_TRB_CODE, event)) {
	case MHI_EVENT_CC_EOB:
		mhi_log(MHI_MSG_VERBOSE, "IEOB condition detected\n");
	case MHI_EVENT_CC_OVERFLOW:
		mhi_log(MHI_MSG_VERBOSE, "Overflow condition detected\n");
	case MHI_EVENT_CC_EOT:
	{
		void *trb_data_loc;
		u32 ieot_flag;
		MHI_STATUS ret_val;
		mhi_ring *local_chan_ctxt;

		chan = MHI_EV_READ_CHID(EV_CHID, event);
		local_chan_ctxt =
			&mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
		phy_ev_trb_loc = MHI_EV_READ_PTR(EV_PTR, event);

		if (unlikely(!VALID_CHAN_NR(chan))) {
			mhi_log(MHI_MSG_ERROR, "Bad ring id.\n");
			break;
		}
		chan_ctxt = &mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan];
		ret_val = validate_xfer_el_addr(chan_ctxt,
						phy_ev_trb_loc);

		if (unlikely(MHI_STATUS_SUCCESS != ret_val)) {
			mhi_log(MHI_MSG_ERROR, "Bad event trb ptr.\n");
			break;
		}

		/* Get the TRB this event points to*/
		local_ev_trb_loc =
			(mhi_xfer_pkt *)mhi_p2v_addr(
					mhi_dev_ctxt->mhi_ctrl_seg_info,
							phy_ev_trb_loc);
		local_trb_loc = (mhi_xfer_pkt *)local_chan_ctxt->rp;

		ret_val = get_nr_enclosed_el(local_chan_ctxt,
				      local_trb_loc,
				      local_ev_trb_loc,
				      &nr_trb_to_parse);
		if (unlikely(MHI_STATUS_SUCCESS != ret_val)) {
			mhi_log(MHI_MSG_CRITICAL,
				"Failed to get nr available trbs ret: %d.\n",
				ret_val);
			return MHI_STATUS_ERROR;
		}
		do {
			u64 phy_buf_loc;
			MHI_TRB_GET_INFO(TX_TRB_IEOT, local_trb_loc, ieot_flag);
			phy_buf_loc = local_trb_loc->data_tx_pkt.buffer_ptr;
			trb_data_loc = (void *)(uintptr_t)phy_buf_loc;
			if (chan % 2)
				xfer_len = MHI_EV_READ_LEN(EV_LEN, event);
			else
				xfer_len = MHI_TX_TRB_GET_LEN(TX_TRB_LEN,
							local_trb_loc);

			if (!VALID_BUF(trb_data_loc, xfer_len)) {
				mhi_log(MHI_MSG_CRITICAL,
					"Bad buffer ptr: %p.\n",
					trb_data_loc);
				return MHI_STATUS_ERROR;
			}

			client_handle = mhi_dev_ctxt->client_handle_list[chan];
			if (NULL != client_handle) {
				client_handle->pkt_count++;
				result = &client_handle->result;
				result->payload_buf = trb_data_loc;
				result->bytes_xferd = xfer_len;
				result->user_data = client_handle->user_data;
			}
			if (chan % 2) {
				parse_inbound(mhi_dev_ctxt, chan,
						local_ev_trb_loc, xfer_len);
			} else {
				parse_outbound(mhi_dev_ctxt, chan,
						local_ev_trb_loc, xfer_len);
			}
			mhi_dev_ctxt->mhi_chan_cntr[chan].pkts_xferd++;
			if (local_trb_loc ==
				(mhi_xfer_pkt *)local_chan_ctxt->rp) {
				mhi_log(MHI_MSG_CRITICAL,
					"Done. Processed until: %p.\n",
					trb_data_loc);
				break;
			} else {
				local_trb_loc =
					(mhi_xfer_pkt *)local_chan_ctxt->rp;
			}
			i++;
		} while (i <= nr_trb_to_parse);
		break;
	} /* CC_EOT */
	case MHI_EVENT_CC_OOB:
	case MHI_EVENT_CC_DB_MODE:
	{
		mhi_ring *chan_ctxt = NULL;
		u64 db_value = 0;
		unsigned long flags = 0;
		mhi_dev_ctxt->uldl_enabled = 1;
		chan = MHI_EV_READ_CHID(EV_CHID, event);
		mhi_dev_ctxt->db_mode[chan] = 1;
		chan_ctxt =
			&mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
		mhi_log(MHI_MSG_INFO, "OOB Detected chan %d.\n", chan);
		if (chan_ctxt->wp != chan_ctxt->rp) {
			spin_lock_irqsave(&mhi_dev_ctxt->db_write_lock[chan], flags);
			db_value = mhi_v2p_addr(mhi_dev_ctxt->mhi_ctrl_seg_info,
							(uintptr_t)chan_ctxt->wp);
			MHI_WRITE_DB(mhi_dev_ctxt, mhi_dev_ctxt->channel_db_addr, chan,
				db_value);
			spin_unlock_irqrestore(&mhi_dev_ctxt->db_write_lock[chan], flags);
		}
		client_handle = mhi_dev_ctxt->client_handle_list[chan];
		if (NULL != client_handle && result != NULL) {
				result->transaction_status = MHI_STATUS_DEVICE_NOT_READY;
			}
		break;
	}
	default:
		{
			mhi_log(MHI_MSG_ERROR,
				"Unknown TX completion.\n");
			break;
		}
	} /*switch(MHI_EV_READ_CODE(EV_TRB_CODE,event)) */
	return 0;
}

MHI_STATUS recycle_trb_and_ring(mhi_device_ctxt *mhi_dev_ctxt,
		mhi_ring *ring,
		MHI_RING_TYPE ring_type,
		u32 ring_index)
{
	MHI_STATUS ret_val = MHI_STATUS_ERROR;
	u64 db_value = 0;
	void *removed_element = NULL;
	void *added_element = NULL;

	/* TODO This will not cover us for ring_index out of
	 * bounds for cmd or event channels */
	if (NULL == mhi_dev_ctxt || NULL == ring ||
			ring_type > (MHI_RING_TYPE_MAX - 1) ||
			ring_index > (MHI_MAX_CHANNELS - 1)) {

		mhi_log(MHI_MSG_ERROR, "Bad input params\n");
		return ret_val;
	}
	ret_val = ctxt_del_element(ring, &removed_element);
	if (MHI_STATUS_SUCCESS != ret_val) {
		mhi_log(MHI_MSG_ERROR, "Could not remove element from ring\n");
		return MHI_STATUS_ERROR;
	}
	ret_val = ctxt_add_element(ring, &added_element);
	if (MHI_STATUS_SUCCESS != ret_val)
		mhi_log(MHI_MSG_ERROR, "Could not add element to ring\n");
	db_value = mhi_v2p_addr(mhi_dev_ctxt->mhi_ctrl_seg_info,
			(uintptr_t)ring->wp);
	if (MHI_STATUS_SUCCESS != ret_val)
		return ret_val;
	if (MHI_RING_TYPE_XFER_RING == ring_type) {
		mhi_xfer_pkt *removed_xfer_pkt =
			(mhi_xfer_pkt *)removed_element;
		mhi_xfer_pkt *added_xfer_pkt =
			(mhi_xfer_pkt *)added_element;
		added_xfer_pkt->data_tx_pkt =
			*(mhi_tx_pkt *)removed_xfer_pkt;
		if ((IS_HARDWARE_CHANNEL(ring_index)) &&
				(mhi_dev_ctxt->counters.m0_m3 > 0)) {
			unsigned long flags = 0;
			mhi_chan_ctxt *chan_ctxt;
			chan_ctxt = &mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[ring_index];
			mhi_log(MHI_MSG_VERBOSE,
					"Updating chan ctxt ring %d\n", ring_index);
			spin_lock_irqsave(&mhi_dev_ctxt->db_write_lock[ring_index], flags);
			mhi_dev_ctxt->mhi_chan_db_order[ring_index] = 1;
			db_value = mhi_v2p_addr(mhi_dev_ctxt->mhi_ctrl_seg_info,
					(uintptr_t)mhi_dev_ctxt->mhi_local_chan_ctxt[ring_index].wp);
			chan_ctxt->mhi_trb_write_ptr = db_value;
			spin_unlock_irqrestore(&mhi_dev_ctxt->db_write_lock[ring_index], flags);
		}
	} else if (MHI_RING_TYPE_EVENT_RING == ring_type &&
			mhi_dev_ctxt->counters.m0_m3 > 0 &&
			IS_HARDWARE_CHANNEL(ring_index)) {
		spinlock_t *lock = NULL;
		unsigned long flags = 0;
		mhi_log(MHI_MSG_VERBOSE, "Updating ev ctxt ring %d\n", ring_index);
		lock = &mhi_dev_ctxt->mhi_ev_spinlock_list[ring_index];
		spin_lock_irqsave(lock, flags);
		mhi_dev_ctxt->mhi_ev_db_order[ring_index] = 1;
		mhi_dev_ctxt->mhi_ctrl_seg->mhi_ec_list[ring_index].mhi_event_write_ptr = db_value;
		mhi_dev_ctxt->ev_counter[ring_index]++;
		spin_unlock_irqrestore(lock, flags);
	}
	atomic_inc(&mhi_dev_ctxt->flags.data_pending);
	/* Asserting Device Wake here, will imediately wake mdm */
	if ((MHI_STATE_M0 == mhi_dev_ctxt->mhi_state ||
				MHI_STATE_M1 == mhi_dev_ctxt->mhi_state) &&
			mhi_dev_ctxt->flags.link_up) {
		switch (ring_type) {
		case MHI_RING_TYPE_CMD_RING:
		{
			struct mutex *cmd_mutex = NULL;
			cmd_mutex =
				&mhi_dev_ctxt->mhi_cmd_mutex_list[PRIMARY_CMD_RING];
			mutex_lock(cmd_mutex);
			mhi_dev_ctxt->cmd_ring_order = 1;
			MHI_WRITE_DB(mhi_dev_ctxt, mhi_dev_ctxt->cmd_db_addr,
					ring_index, db_value);
			mutex_unlock(cmd_mutex);
			break;
		}
		case MHI_RING_TYPE_EVENT_RING:
		{
			spinlock_t *lock = NULL;
			unsigned long flags = 0;
			lock = &mhi_dev_ctxt->mhi_ev_spinlock_list[ring_index];
			spin_lock_irqsave(lock, flags);
			mhi_dev_ctxt->mhi_ev_db_order[ring_index] = 1;
			db_value = mhi_v2p_addr(mhi_dev_ctxt->mhi_ctrl_seg_info,
					(uintptr_t)ring->wp);
			if ((mhi_dev_ctxt->ev_counter[ring_index] %
						MHI_EV_DB_INTERVAL) == 0) {
				MHI_WRITE_DB(mhi_dev_ctxt, mhi_dev_ctxt->event_db_addr,
						ring_index, db_value);
			}
			spin_unlock_irqrestore(lock, flags);
			break;
		}
		case MHI_RING_TYPE_XFER_RING:
		{
			unsigned long flags = 0;
			spin_lock_irqsave(&mhi_dev_ctxt->db_write_lock[ring_index], flags);
			mhi_dev_ctxt->mhi_chan_db_order[ring_index] = 1;
			MHI_WRITE_DB(mhi_dev_ctxt, mhi_dev_ctxt->channel_db_addr,
					ring_index, db_value);
			spin_unlock_irqrestore(&mhi_dev_ctxt->db_write_lock[ring_index],
						flags);
			break;
		}
		default:
			mhi_log(MHI_MSG_ERROR, "Bad ring type\n");
		}
	} else {
		mhi_log(MHI_MSG_ERROR,
			"Cannot ring DB state %d link %d, ring type %d, index %d db 0x%lx\n",
					mhi_dev_ctxt->mhi_state,
					mhi_dev_ctxt->flags.link_up,
					ring_type,
					ring_index,
					(uintptr_t)db_value);
		mhi_dev_ctxt->counters.failed_recycle[ring_type]++;
	}
	atomic_dec(&mhi_dev_ctxt->flags.data_pending);
	return ret_val;
}
MHI_STATUS mhi_change_chan_state(mhi_device_ctxt *mhi_dev_ctxt, u32 chan_id,
		MHI_CHAN_STATE new_state)
{
	struct mutex *chan_mutex;
	if (chan_id > (MHI_MAX_CHANNELS - 1) || NULL == mhi_dev_ctxt ||
			new_state > MHI_CHAN_STATE_LIMIT) {
		mhi_log(MHI_MSG_ERROR, "Bad input parameters\n");
		return MHI_STATUS_ERROR;
	}

	chan_mutex = &mhi_dev_ctxt->mhi_chan_mutex[chan_id];
	mutex_lock(chan_mutex);

	/* Set the new state of the channel context */
	mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan_id].mhi_chan_state =
		MHI_CHAN_STATE_ENABLED;
	mutex_unlock(chan_mutex);
	return MHI_STATUS_SUCCESS;
}

MHI_STATUS parse_cmd_event(mhi_device_ctxt *mhi_dev_ctxt, mhi_event_pkt *ev_pkt)
{
	MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	mhi_cmd_pkt *cmd_pkt = NULL;
	uintptr_t phy_trb_loc = 0;
	if (NULL != ev_pkt)
		phy_trb_loc = (uintptr_t)MHI_EV_READ_PTR(EV_PTR,
				ev_pkt);
	else
		return MHI_STATUS_ERROR;

	cmd_pkt = (mhi_cmd_pkt *)mhi_p2v_addr(mhi_dev_ctxt->mhi_ctrl_seg_info,
			phy_trb_loc);
	mhi_log(MHI_MSG_INFO, "Received CMD completion event\n");
	switch (MHI_EV_READ_CODE(EV_TRB_CODE, ev_pkt)) {
		/* Command completion was successful */
	case MHI_EVENT_CC_SUCCESS:
	{
		u32 chan;
		MHI_TRB_GET_INFO(CMD_TRB_CHID, cmd_pkt, chan);
		switch (MHI_TRB_READ_INFO(CMD_TRB_TYPE, cmd_pkt)) {
		case MHI_PKT_TYPE_NOOP_CMD:
			mhi_log(MHI_MSG_INFO, "Processed NOOP cmd event\n");
			break;
		case MHI_PKT_TYPE_RESET_CHAN_CMD:
			if (MHI_STATUS_SUCCESS != reset_chan_cmd(mhi_dev_ctxt,
								cmd_pkt))
				mhi_log(MHI_MSG_INFO,
					"Failed to process reset cmd\n");
		break;
		case MHI_PKT_TYPE_STOP_CHAN_CMD:
		{

			mhi_log(MHI_MSG_INFO, "Processed cmd stop event\n");
			if (MHI_STATUS_SUCCESS != ret_val) {
				mhi_log(MHI_MSG_INFO,
						"Failed to set chan state\n");
				return MHI_STATUS_ERROR;
			}
			break;
		}
		case MHI_PKT_TYPE_START_CHAN_CMD:
		{
			if (MHI_STATUS_SUCCESS != start_chan_cmd(mhi_dev_ctxt,
								cmd_pkt))
				mhi_log(MHI_MSG_INFO,
					"Failed to process reset cmd\n");
			atomic_dec(&mhi_dev_ctxt->start_cmd_pending_ack);
			wake_up_interruptible(mhi_dev_ctxt->chan_start_complete);
			break;
		}
		default:
			mhi_log(MHI_MSG_INFO,
				"Bad cmd type 0x%x\n",
				MHI_TRB_READ_INFO(CMD_TRB_TYPE, cmd_pkt));
			break;
		}
		mhi_log(MHI_MSG_INFO, "CMD completion indicated successful\n");
		mhi_dev_ctxt->mhi_chan_pend_cmd_ack[chan] = MHI_CMD_NOT_PENDING;
		break;
	}
	default:
		mhi_log(MHI_MSG_INFO, "Unhandled mhi completion code\n");
		break;
	}
	mhi_log(MHI_MSG_INFO, "Recycling cmd event\n");
	ctxt_del_element(mhi_dev_ctxt->mhi_local_cmd_ctxt, NULL);
	return MHI_STATUS_SUCCESS;
}

MHI_STATUS reset_chan_cmd(mhi_device_ctxt *mhi_dev_ctxt, mhi_cmd_pkt *cmd_pkt)
{
	u32 chan  = 0;
	MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	mhi_ring *local_chan_ctxt;
	mhi_chan_ctxt *chan_ctxt;
	mhi_client_handle *client_handle = NULL;
	struct mutex *chan_mutex;

	MHI_TRB_GET_INFO(CMD_TRB_CHID, cmd_pkt, chan);

	if (!VALID_CHAN_NR(chan)) {
		mhi_log(MHI_MSG_ERROR,
				"Bad channel number for CCE\n");
		return MHI_STATUS_ERROR;
	}

	chan_mutex = &mhi_dev_ctxt->mhi_chan_mutex[chan];
	mutex_lock(chan_mutex);
	client_handle = mhi_dev_ctxt->client_handle_list[chan];
	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	chan_ctxt = &mhi_dev_ctxt->mhi_ctrl_seg->mhi_cc_list[chan];
	mhi_log(MHI_MSG_INFO, "Processed cmd reset event\n");

	/* Reset the local channel context */
	local_chan_ctxt->rp = local_chan_ctxt->base;
	local_chan_ctxt->wp = local_chan_ctxt->base;
	local_chan_ctxt->ack_rp = local_chan_ctxt->base;

	/* Reset the mhi channel context */
	chan_ctxt->mhi_chan_state = MHI_CHAN_STATE_ENABLED;
	chan_ctxt->mhi_trb_read_ptr = chan_ctxt->mhi_trb_ring_base_addr;
	chan_ctxt->mhi_trb_write_ptr = chan_ctxt->mhi_trb_ring_base_addr;

	mhi_dev_ctxt->mhi_chan_pend_cmd_ack[chan] = MHI_CMD_NOT_PENDING;
	mutex_unlock(chan_mutex);
	mhi_log(MHI_MSG_INFO,
			"Reset complete starting channel back up.\n");
	if (MHI_STATUS_SUCCESS != mhi_send_cmd(mhi_dev_ctxt,
				MHI_COMMAND_START_CHAN,
				chan))
		mhi_log(MHI_MSG_CRITICAL,
				"Failed to restart channel.\n");
	if (NULL != client_handle)
		complete(&client_handle->chan_close_complete);
	return ret_val;
}
MHI_STATUS start_chan_cmd(mhi_device_ctxt *mhi_dev_ctxt, mhi_cmd_pkt *cmd_pkt)
{
	u32 chan;
	MHI_TRB_GET_INFO(CMD_TRB_CHID, cmd_pkt, chan);
	if (!VALID_CHAN_NR(chan)) {
		mhi_log(MHI_MSG_ERROR, "Bad chan: 0x%x\n", chan);
		return MHI_STATUS_ERROR;
	}
	mhi_dev_ctxt->mhi_chan_pend_cmd_ack[chan] =
		MHI_CMD_NOT_PENDING;

	mhi_log(MHI_MSG_INFO, "Processed cmd channel start\n");
	return MHI_STATUS_SUCCESS;
}

void mhi_poll_inbound(mhi_client_handle *client_handle,
		uintptr_t *buf, ssize_t *buf_size)
{
	mhi_tx_pkt *pending_trb = 0;
	mhi_device_ctxt *mhi_dev_ctxt = NULL;
	u32 chan = 0;
	mhi_ring *local_chan_ctxt;
	struct mutex *chan_mutex = NULL;

	if (NULL == client_handle || NULL == buf || 0 == buf_size ||
			NULL == client_handle->mhi_dev_ctxt)
		return;
	mhi_dev_ctxt = client_handle->mhi_dev_ctxt;
	chan = client_handle->chan;
	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	chan_mutex = &mhi_dev_ctxt->mhi_chan_mutex[chan];
	mutex_lock(chan_mutex);
	if ((local_chan_ctxt->rp != local_chan_ctxt->ack_rp)) {
		pending_trb = (mhi_tx_pkt *)(local_chan_ctxt->ack_rp);
		*buf = (uintptr_t)(pending_trb->buffer_ptr);
		*buf_size = (size_t)MHI_TX_TRB_GET_LEN(TX_TRB_LEN,
				(mhi_xfer_pkt *)pending_trb);
	} else {
		*buf = 0;
		*buf_size = 0;
	}
	mutex_unlock(chan_mutex);
}

MHI_STATUS mhi_client_recycle_trb(mhi_client_handle *client_handle)
{
	unsigned long flags;
	u32 chan = client_handle->chan;
	MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	mhi_device_ctxt *mhi_dev_ctxt = client_handle->mhi_dev_ctxt;
	struct mutex *chan_mutex  = &mhi_dev_ctxt->mhi_chan_mutex[chan];
	mhi_ring *local_ctxt = NULL;
	u64 db_value;
	local_ctxt = &client_handle->mhi_dev_ctxt->mhi_local_chan_ctxt[chan];

	mutex_lock(chan_mutex);
	MHI_TX_TRB_SET_LEN(TX_TRB_LEN,
			(mhi_xfer_pkt *)local_ctxt->ack_rp,
			TRB_MAX_DATA_SIZE);

	*(mhi_xfer_pkt *)local_ctxt->wp =
		*(mhi_xfer_pkt *)local_ctxt->ack_rp;
	ret_val = delete_element(local_ctxt, &local_ctxt->ack_rp,
			&local_ctxt->rp, NULL);
	ret_val = ctxt_add_element(local_ctxt, NULL);
	db_value = mhi_v2p_addr(mhi_dev_ctxt->mhi_ctrl_seg_info,
			(uintptr_t)local_ctxt->wp);
	read_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);
	atomic_inc(&mhi_dev_ctxt->flags.data_pending);
	if (mhi_dev_ctxt->flags.link_up) {
		if (MHI_STATE_M0 == mhi_dev_ctxt->mhi_state ||
				MHI_STATE_M1 == mhi_dev_ctxt->mhi_state) {
			mhi_assert_device_wake(mhi_dev_ctxt);
			MHI_WRITE_DB(mhi_dev_ctxt, mhi_dev_ctxt->channel_db_addr, chan, db_value);
		} else if (mhi_dev_ctxt->flags.pending_M3 ||
				mhi_dev_ctxt->mhi_state == MHI_STATE_M3) {
			mhi_wake_dev_from_m3(mhi_dev_ctxt);
		}
	}
	atomic_dec(&mhi_dev_ctxt->flags.data_pending);
	read_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);
	mhi_dev_ctxt->mhi_chan_cntr[chan].pkts_xferd++;
	mutex_unlock(chan_mutex);
	return ret_val;
}

MHI_STATUS validate_xfer_el_addr(mhi_chan_ctxt *ring, uintptr_t addr)
{
	return (addr < (ring->mhi_trb_ring_base_addr) ||
			addr > (ring->mhi_trb_ring_base_addr)
			+ (ring->mhi_trb_ring_len - 1)) ?
		MHI_STATUS_ERROR : MHI_STATUS_SUCCESS;
}
MHI_STATUS validate_ev_el_addr(mhi_ring *ring, uintptr_t addr)
{
	return (addr < (uintptr_t)(ring->base) ||
			addr > ((uintptr_t)(ring->base)
				+ (ring->len - 1))) ?
		MHI_STATUS_ERROR : MHI_STATUS_SUCCESS;
}

MHI_STATUS validate_ring_el_addr(mhi_ring *ring, uintptr_t addr)
{
	return (addr < (uintptr_t)(ring->base) ||
			addr > ((uintptr_t)(ring->base)
				+ (ring->len - 1))) ?
		MHI_STATUS_ERROR : MHI_STATUS_SUCCESS;
}
MHI_STATUS parse_inbound(mhi_device_ctxt *mhi_dev_ctxt, u32 chan,
		mhi_xfer_pkt *local_ev_trb_loc, u16 xfer_len)
{
	mhi_client_handle *client_handle;
	mhi_ring *local_chan_ctxt;
	mhi_result *result;
	mhi_cb_info cb_info;

	client_handle = mhi_dev_ctxt->client_handle_list[chan];
	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];

	if (unlikely(mhi_dev_ctxt->mhi_local_chan_ctxt[chan].rp ==
				mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp)) {
		mhi_dev_ctxt->mhi_chan_cntr[chan].empty_ring_removal++;
		mhi_wait_for_mdm(mhi_dev_ctxt);
		return mhi_send_cmd(mhi_dev_ctxt,
				MHI_COMMAND_RESET_CHAN,
				chan);
	}

	if (NULL != mhi_dev_ctxt->client_handle_list[chan])
		result = &mhi_dev_ctxt->client_handle_list[chan]->result;

	/* If a client is registered */
	if (unlikely(IS_SOFTWARE_CHANNEL(chan))) {
		MHI_TX_TRB_SET_LEN(TX_TRB_LEN,
				local_ev_trb_loc,
				xfer_len);
		ctxt_del_element(local_chan_ctxt, NULL);
		if (NULL != client_handle->client_info.mhi_client_cb &&
				(0 == (client_handle->pkt_count % client_handle->cb_mod))) {
			cb_info.cb_reason = MHI_CB_XFER_SUCCESS;
			cb_info.result = &client_handle->result;
			cb_info.result->transaction_status =
				MHI_STATUS_SUCCESS;
			client_handle->client_info.mhi_client_cb(&cb_info);
		}
	} else  {
		/* IN Hardware channel with no client
		 * registered, we are done with this TRB*/
		if (likely(NULL != client_handle)) {
			ctxt_del_element(local_chan_ctxt, NULL);
			/* A client is not registred for this IN channel */
		} else  {/* Hardware Channel, no client registerered,
			    drop data */
			recycle_trb_and_ring(mhi_dev_ctxt,
					&mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
					MHI_RING_TYPE_XFER_RING,
					chan);
		}
	}
	return MHI_STATUS_SUCCESS;
}

MHI_STATUS parse_outbound(mhi_device_ctxt *mhi_dev_ctxt, u32 chan,
		mhi_xfer_pkt *local_ev_trb_loc, u16 xfer_len)
{
	mhi_result *result = NULL;
	MHI_STATUS ret_val = 0;
	mhi_client_handle *client_handle = NULL;
	mhi_ring *local_chan_ctxt = NULL;
	mhi_cb_info cb_info;
	local_chan_ctxt = &mhi_dev_ctxt->mhi_local_chan_ctxt[chan];
	client_handle = mhi_dev_ctxt->client_handle_list[chan];

	/* If ring is empty */
	if (mhi_dev_ctxt->mhi_local_chan_ctxt[chan].rp ==
			mhi_dev_ctxt->mhi_local_chan_ctxt[chan].wp) {
		mhi_dev_ctxt->mhi_chan_cntr[chan].empty_ring_removal++;
		mhi_wait_for_mdm(mhi_dev_ctxt);
		return mhi_send_cmd(mhi_dev_ctxt,
				MHI_COMMAND_RESET_CHAN,
				chan);
	}

	if (NULL != client_handle) {
		result = &mhi_dev_ctxt->client_handle_list[chan]->result;

		if (NULL != (&client_handle->client_info.mhi_client_cb) &&
				(0 == (client_handle->pkt_count % client_handle->cb_mod))) {
			cb_info.cb_reason = MHI_CB_XFER_SUCCESS;
			cb_info.result = &client_handle->result;
			cb_info.result->transaction_status =
				MHI_STATUS_SUCCESS;
			client_handle->client_info.mhi_client_cb(&cb_info);
		}
	}
	ret_val = ctxt_del_element(&mhi_dev_ctxt->mhi_local_chan_ctxt[chan],
			NULL);
	atomic_dec(&mhi_dev_ctxt->counters.outbound_acks);
	mhi_log(MHI_MSG_VERBOSE,
			"Processed outbound ack chan %d Pending acks %d.\n",
			chan, atomic_read(&mhi_dev_ctxt->counters.outbound_acks));
	return MHI_STATUS_SUCCESS;
}

MHI_STATUS probe_clients(mhi_device_ctxt *mhi_dev_ctxt,STATE_TRANSITION new_state)
{
	int ret_val = 0;
	if (new_state == STATE_TRANSITION_AMSS) {
		probe_clients(mhi_dev_ctxt, STATE_TRANSITION_SBL);
		mhi_dev_ctxt->mhi_rmnet_dev = platform_device_alloc("mhi_rmnet",-1);
		ret_val = platform_device_add(mhi_dev_ctxt->mhi_rmnet_dev);
		if (ret_val)
			mhi_log(MHI_MSG_CRITICAL, "Failed to add MHI_RmNET device.\n");
	} else if (new_state == STATE_TRANSITION_SBL) {
		if (0 == mhi_dev_ctxt->flags.uci_probed) {
			mhi_dev_ctxt->mhi_uci_dev = platform_device_alloc("mhi_uci",-1);
			ret_val = platform_device_add(mhi_dev_ctxt->mhi_uci_dev);

			if (ret_val)
				mhi_log(MHI_MSG_CRITICAL, "Failed to add MHI_SHIM device.\n");
			mhi_dev_ctxt->flags.uci_probed = 1;
		}
	}
	return ret_val;
}

MHI_STATUS mhi_wait_for_mdm(mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 j = 0;
	while (0xFFFFffff == pcie_read(mhi_dev_ctxt->mmio_addr, MHIREGLEN)
			&& j <= MHI_MAX_LINK_RETRIES) {
		mhi_log(MHI_MSG_CRITICAL,
				"Could not access MDM retry %d\n", j);
		msleep(MHI_LINK_STABILITY_WAIT_MS);
		if (MHI_MAX_LINK_RETRIES == j) {
			mhi_log(MHI_MSG_CRITICAL,
					"Could not access MDM, FAILING!\n");
			return MHI_STATUS_ERROR;
		}
		j++;
	}
	return MHI_STATUS_SUCCESS;
}
int mhi_get_chan_max_buffers(u32 chan)
{
	if (IS_SOFTWARE_CHANNEL(chan))
		return MAX_NR_TRBS_PER_SOFT_CHAN - 1;
	else
		return MAX_NR_TRBS_PER_HARD_CHAN - 1;
}

int mhi_get_max_buffers(mhi_client_handle *client_handle)
{
	return mhi_get_chan_max_buffers(client_handle->chan);
}

int mhi_get_epid(mhi_client_handle *client_handle)
{
	return MHI_EPID;
}
int mhi_assert_device_wake(mhi_device_ctxt *mhi_dev_ctxt)
{
	mhi_log(MHI_MSG_MSI_WAKE_GPIO, "GPIO %d\n",
			mhi_dev_ctxt->dev_props->device_wake_gpio);
	gpio_set_value(mhi_dev_ctxt->dev_props->device_wake_gpio, 1);
	return 0;
}
int mhi_deassert_device_wake(mhi_device_ctxt *mhi_dev_ctxt)
{
	mhi_log(MHI_MSG_MSI_WAKE_GPIO, "GPIO %d\n",
			mhi_dev_ctxt->dev_props->device_wake_gpio);
	if (mhi_dev_ctxt->enable_lpm)
		gpio_set_value(mhi_dev_ctxt->dev_props->device_wake_gpio, 0);
	else
		mhi_log(MHI_MSG_VERBOSE, "LPM Enabled\n");
	return 0;
}

int mhi_set_lpm(mhi_client_handle *client_handle, int enable_lpm)
{
	mhi_log(MHI_MSG_VERBOSE, "LPM Set %d\n", enable_lpm);
	client_handle->mhi_dev_ctxt->enable_lpm = enable_lpm ? 1 : 0;
	return 0;
}
int mhi_set_bus_request(struct mhi_device_ctxt *mhi_dev_ctxt,
					int index)
{
//	mhi_log(MHI_MSG_INFO, "Setting bus request to index %d\n", index);
//	return msm_bus_scale_client_update_request(mhi_dev_ctxt->bus_client,
//							index);
	return 0;
}

void mhi_reg_write_field(void __iomem *io_addr,
                         uintptr_t io_offset,
                         u32 mask, u32 shift, u32 val)
{
        u32 reg_val;
        reg_val = mhi_reg_read(io_addr, io_offset);
        reg_val &= ~mask;
        reg_val = reg_val | (val << shift);
        mhi_reg_write(io_addr, io_offset, reg_val);
}

void mhi_reg_write(void __iomem *io_addr,
                   uintptr_t io_offset, u32 val)
{
        mhi_log(MHI_MSG_VERBOSE, "d.s 0x%p off: 0x%lx 0x%x\n",
                                        io_addr, io_offset, val);
        iowrite32(val, io_addr + io_offset);
        wmb();
}

u32 mhi_reg_read_field(void __iomem *io_addr, uintptr_t io_offset,
                         u32 mask, u32 shift)
{
        return (mhi_reg_read(io_addr, io_offset) & mask) >> shift;
}

u32 mhi_reg_read(void __iomem *io_addr, uintptr_t io_offset)
{
        return ioread32(io_addr + io_offset);
}
