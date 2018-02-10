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

/* MHI Includes */
#include "mhi_uci.h"

#define TRB_MAX_DATA_SIZE 0x1000

UCI_DBG_LEVEL mhi_uci_msg_lvl = UCI_DBG_ERROR;
UCI_DBG_LEVEL mhi_uci_ipc_log_lvl = UCI_DBG_VERBOSE;
void *mhi_uci_ipc_log;
#define MHI_UCI_IPC_LOG_PAGES (100)

module_param(mhi_uci_msg_lvl , uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_uci_msg_lvl, "uci dbg lvl");
module_param(mhi_uci_ipc_log_lvl , uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_uci_ipc_log_lvl, "uci dbg lvl");


int mhi_uci_dump = 0;
module_param(mhi_uci_dump, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_uci_dump, "uci dbg dump");

enum {
	MSM_MHI_CTL_DEBUG = 1U << 0,
	MSM_MHI_CTL_DUMP_BUFFER = 1U << 1,
};

void mhi_uci_dump_log(int log_lvl, const char *prefix_str,
		size_t len, const void *buf)
{
	const u8 *ptr = buf;
	int i, linelen, remaining = len;
	unsigned char linebuf[32 * 3 + 2 + 32 + 1];
	int rowsize = 16;
	int groupsize = 1;


	if (!(mhi_uci_dump & MSM_MHI_CTL_DUMP_BUFFER))
		return ;

	len = len < 16 ? len : 16;

	for (i = 0; i < len; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;
		hex_dump_to_buffer(ptr + i, linelen, rowsize, groupsize,
				linebuf, sizeof(linebuf), false);

		mhi_uci_log(log_lvl, "%s%s\n",prefix_str,linebuf);
	}
}

static ssize_t mhi_uci_client_read(struct file *file, char __user *buf,
		size_t count, loff_t *offp);
static ssize_t mhi_uci_client_write(struct file *file,
		const char __user *buf, size_t count, loff_t *offp);
static int mhi_uci_client_open(struct inode *mhi_inode, struct file*);
static int mhi_uci_client_release(struct inode *mhi_inode,
		struct file *file_handle);
static unsigned int mhi_uci_client_poll(struct file *file, poll_table *wait);
static long mhi_uci_ctl_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg);

mhi_uci_ctxt_t mhi_uci_ctxt;

static const struct file_operations mhi_uci_client_fops = {
read: mhi_uci_client_read,
      write : mhi_uci_client_write,
      open : mhi_uci_client_open,
      release : mhi_uci_client_release,
      poll : mhi_uci_client_poll,
      unlocked_ioctl : mhi_uci_ctl_ioctl,
};

static struct platform_driver mhi_uci_driver =
{
	.driver =
	{
		.name = "mhi_uci",
		.owner = THIS_MODULE,
	},
	.probe = mhi_uci_probe,
	.remove = mhi_uci_remove,
};

//module_platform_driver(mhi_uci_driver);
static long mhi_uci_ctl_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret_val = 0;
	u32 set_val;
	uci_client *uci_handle = NULL;
	uci_handle = file->private_data;
	if (NULL == uci_handle) {
		mhi_uci_log(UCI_DBG_VERBOSE,
				"Invalid handle for client.\n");
		return -ENODEV;
	}
	mhi_uci_log(UCI_DBG_VERBOSE,
			"Attempting to dtr cmd 0x%x arg 0x%lx for chan %d\n",
			cmd, arg, uci_handle->out_chan);

	switch(cmd) {
		case TIOCMGET:
			mhi_uci_log(UCI_DBG_VERBOSE,
					"Returning 0x%x mask\n", uci_handle->local_tiocm);
			return uci_handle->local_tiocm;
			break;
		case TIOCMSET:
			if (0 != copy_from_user(&set_val, (void *)arg, sizeof(set_val)))
				return -ENOMEM;
			mhi_uci_log(UCI_DBG_VERBOSE,
					"Attempting to set cmd 0x%x arg 0x%x for chan %d\n",
					cmd, set_val, uci_handle->out_chan);
			ret_val = mhi_uci_tiocm_set(uci_handle, set_val, ~set_val);
			break;
	}
	return ret_val;
}

int mhi_uci_tiocm_set(uci_client *client_ctxt, u32 set, u32 clear)
{
	u8 status_set = 0;
	u8 status_clear = 0;
	u8 old_status = 0;

	mutex_lock(&client_ctxt->uci_ctxt->ctrl_mutex);

	status_set |= (set & TIOCM_DTR) ? TIOCM_DTR : 0;
	status_clear |= (clear & TIOCM_DTR) ? TIOCM_DTR: 0;
	old_status = client_ctxt->local_tiocm;
	client_ctxt->local_tiocm |= status_set;
	client_ctxt->local_tiocm &= ~status_clear;

	mhi_uci_log(UCI_DBG_VERBOSE,
			"Old TIOCM0x%x for chan %d, Current TIOCM 0x%x\n",
			old_status, client_ctxt->out_chan, client_ctxt->local_tiocm);
	mutex_unlock(&client_ctxt->uci_ctxt->ctrl_mutex);

	if (client_ctxt->local_tiocm != old_status) {
		mhi_uci_log(UCI_DBG_VERBOSE,
				"Setting TIOCM to 0x%x for chan %d\n",
				client_ctxt->local_tiocm, client_ctxt->out_chan);
		return mhi_uci_send_status_cmd(client_ctxt);
	}
	return 0;

}
static unsigned int mhi_uci_client_poll(struct file *file, poll_table *wait)
{
	u32 mask = 0;
	uci_client *uci_handle = NULL;
	uci_handle = file->private_data;
	if (NULL == uci_handle)
		return -ENODEV;
	poll_wait(file, &uci_handle->read_wait_queue, wait);
	poll_wait(file, &uci_handle->write_wait_queue, wait);
	if (atomic_read(&uci_handle->avail_pkts) > 0) {
		mhi_uci_log(UCI_DBG_VERBOSE,
				"Client can read chan %d\n", uci_handle->in_chan);
		mask |= POLLIN | POLLRDNORM;
	}
	if (get_free_trbs(uci_handle->outbound_handle)) {
		mhi_uci_log(UCI_DBG_VERBOSE,
				"Client can write chan %d\n", uci_handle->out_chan);
		mask |= POLLOUT | POLLWRNORM;
	}

	mhi_uci_log(UCI_DBG_VERBOSE,
			"Client attempted to poll chan %d, returning mask 0x%x\n",
			uci_handle->in_chan, mask);
	return mask;
}
static int mhi_uci_client_open(struct inode *mhi_inode,
		struct file *file_handle)
{
	uci_client *uci_client_handle = NULL;
	int ret_val = 0;
	uci_client_handle =
		&mhi_uci_ctxt.client_handle_list[iminor(mhi_inode)];

	mhi_uci_log(UCI_DBG_VERBOSE,
			"Client opened device node 0x%x, ref count 0x%x\n",
			iminor(mhi_inode), atomic_read(&uci_client_handle->ref_count));

	if (NULL == uci_client_handle) {
		ret_val = -ENOMEM;
		goto open_exit;
	}

	if (1 == atomic_add_return(1, &uci_client_handle->ref_count)) {
		if (!uci_client_handle->dev_node_enabled) {
			ret_val = -EPERM;
			goto handle_alloc_err;
		}

		uci_client_handle->uci_ctxt = &mhi_uci_ctxt;
		ret_val = mhi_open_channel(&uci_client_handle->outbound_handle,
				uci_client_handle->out_chan,
				0,
				&mhi_uci_ctxt.client_info,
				(void *)(long)uci_client_handle->out_chan);

		if (MHI_STATUS_SUCCESS != ret_val) {
			mhi_uci_log(UCI_DBG_ERROR,
					"Failed open outbound chan %d ret 0x%x\n",
					iminor(mhi_inode), ret_val);
		}
		/* If this channel was never opened before */
	}
	file_handle->private_data = uci_client_handle;
	return ret_val;

handle_alloc_err:
	atomic_dec(&uci_client_handle->ref_count);
open_exit:
	return ret_val;
}

static int mhi_uci_client_release(struct inode *mhi_inode,
		struct file *file_handle)
{
	uci_client *client_handle = file_handle->private_data;
	u32 retry_cnt = 100;

	if (NULL == client_handle)
		return -EINVAL;
	if (0 == atomic_sub_return(1, &client_handle->ref_count)) {
		mhi_uci_log(UCI_DBG_ERROR,
				"Last client left, closing channel 0x%x\n",
				iminor(mhi_inode));
		while ((--retry_cnt > 0) &&
				(0 != atomic_read(&client_handle->out_pkt_pend_ack))) {
			mhi_uci_log(UCI_DBG_CRITICAL,
					"Still waiting on %d acks!, chan %d\n",
					atomic_read(&client_handle->out_pkt_pend_ack),
					iminor(mhi_inode));
			msleep(10);
		}
		if (0 == atomic_read(&client_handle->out_pkt_pend_ack)) {
			mhi_close_channel(client_handle->outbound_handle);
		} else {
			mhi_uci_log(UCI_DBG_CRITICAL,
					"Did not receive all outbound pkt acks!\n");
			return -EIO;
		}
	} else {
		mhi_uci_log(UCI_DBG_ERROR,
				"Client close chan %d, ref count 0x%x\n",
				iminor(mhi_inode), atomic_read(&client_handle->ref_count));
	}
	return 0;
}

static ssize_t mhi_uci_client_read(struct file *file, char __user *buf,
		size_t uspace_buf_size, loff_t *bytes_pending)
{
	uci_client *uci_handle = NULL;
	uintptr_t phy_buf = 0;
	mhi_client_handle *client_handle = NULL;
	int ret_val = 0;
	size_t buf_size = 0;
	struct mutex *mutex;
	u32 chan = 0;
	ssize_t phy_buf_size = 0;
	ssize_t bytes_copied = 0;
	void *pkt_loc = NULL;
	u32 addr_offset = 0;

	if (NULL == file || NULL == buf ||
			0 == uspace_buf_size || NULL == file->private_data)
		return -EINVAL;

	mhi_uci_log(UCI_DBG_VERBOSE,
			"Client attempted read on chan %d\n", chan);
	uci_handle = file->private_data;
	client_handle = uci_handle->inbound_handle;
	mutex = &mhi_uci_ctxt.client_chan_lock[uci_handle->in_chan];
	chan = uci_handle->in_chan;
	mutex_lock(mutex);
	buf_size = mhi_uci_ctxt.channel_attributes[chan].max_packet_size;

	do {
		mhi_poll_inbound(client_handle,
				&phy_buf,
				&phy_buf_size);
		mhi_uci_log(UCI_DBG_VERBOSE,
				"Obtained pkt of size 0x%lx at addr 0x%lx, chan %d\n",
				phy_buf_size, (uintptr_t)phy_buf, chan);
		if ((0 == phy_buf || 0 == phy_buf_size) &&
				(atomic_read(&uci_handle->avail_pkts) <= 0)) {
			/* If nothing was copied yet, wait for data */
			mhi_uci_log(UCI_DBG_VERBOSE,
					"No data avail_pkts %d, chan %d\n",
					atomic_read(&uci_handle->avail_pkts),
					chan);
			wait_event_interruptible(
					uci_handle->read_wait_queue,
					(atomic_read(&uci_handle->avail_pkts) > 0));
		} else if ((atomic_read(&uci_handle->avail_pkts) > 0) &&
				0 == phy_buf && 0 == phy_buf_size &&
				uci_handle->mhi_status == -ENETRESET) {
			mhi_uci_log(UCI_DBG_VERBOSE,
					"Detected pending reset, reporting to client\n");
			atomic_dec(&uci_handle->avail_pkts);
			uci_handle->mhi_status = 0;
			mutex_unlock(mutex);
			return -ENETRESET;
		} else if (atomic_read(&uci_handle->avail_pkts) &&
				phy_buf != 0 && phy_buf_size != 0) {
			mhi_uci_log(UCI_DBG_VERBOSE,
					"Got packet: avail pkts %d phy_adr 0x%lx, chan %d\n",
					atomic_read(&uci_handle->avail_pkts),
					phy_buf,
					chan);
			break;
		} else {
			mhi_uci_log(UCI_DBG_CRITICAL,
					"error: avail pkts %d phy_adr 0x%lx, chan %d\n",
					atomic_read(&uci_handle->avail_pkts),
					phy_buf,
					chan);
			return -EIO;
		}
	} while (!phy_buf);

	pkt_loc = phys_to_virt((dma_addr_t)(uintptr_t)phy_buf);
	mhi_uci_log(UCI_DBG_VERBOSE, "Mapped DMA for client avail_pkts:%d virt_adr 0x%p, chan %d\n",
			atomic_read(&uci_handle->avail_pkts),
			pkt_loc,
			chan);
	if (*bytes_pending == 0) {
		*bytes_pending = phy_buf_size;
		dma_unmap_single(&(mhi_uci_ctxt.mhi_uci_dev->dev),
				(dma_addr_t)phy_buf,
				buf_size,
				DMA_BIDIRECTIONAL);
	}
	if (uspace_buf_size >= *bytes_pending) {
		addr_offset = phy_buf_size - *bytes_pending;
		if (0 != copy_to_user(buf,
					(void *)(uintptr_t)pkt_loc + addr_offset,
					*bytes_pending)) {
			ret_val = -EIO;
			goto error;
		}

		bytes_copied = *bytes_pending;
		*bytes_pending = 0;
		mhi_uci_log(UCI_DBG_VERBOSE,
				"Copied 0x%lx of 0x%x, chan %d\n",
				bytes_copied,
				(u32)*bytes_pending,
				chan);
	} else {
		addr_offset = phy_buf_size - *bytes_pending;
		if (0 != copy_to_user(buf,
					(void *)(uintptr_t)pkt_loc + addr_offset,
					uspace_buf_size)) {
			ret_val = -EIO;
			goto error;
		}
		bytes_copied = uspace_buf_size;
		*bytes_pending -= uspace_buf_size;
		mhi_uci_log(UCI_DBG_VERBOSE,
				"Copied 0x%lx of 0x%x,chan %d\n",
				bytes_copied,
				(u32)*bytes_pending,
				chan);
	}

	mhi_uci_dump_log(UCI_DBG_ERROR, "uci_read: ", bytes_copied, buf);

	/* We finished with this buffer, map it back */
	if (*bytes_pending == 0) {
		memset(pkt_loc, 0, buf_size);
		dma_map_single(&(mhi_uci_ctxt.mhi_uci_dev->dev), pkt_loc,
				buf_size,
				DMA_BIDIRECTIONAL);
		mhi_uci_log(UCI_DBG_VERBOSE,
				"Decrementing avail pkts avail 0x%x\n",
				atomic_read(&uci_handle->avail_pkts));
		atomic_dec(&uci_handle->avail_pkts);
		ret_val = mhi_client_recycle_trb(client_handle);
		if (MHI_STATUS_SUCCESS != ret_val) {
			mhi_uci_log(UCI_DBG_ERROR,
					"Failed to recycle element\n");
			ret_val = -EIO;
			goto error;
		}
	}
	mhi_uci_log(UCI_DBG_INFO,
			"Returning 0x%lx bytes, 0x%x bytes left\n",
			bytes_copied, (u32)*bytes_pending);
	mutex_unlock(mutex);
	return bytes_copied;
error:
	mutex_unlock(mutex);
	mhi_uci_log(UCI_DBG_VERBOSE,
			"Returning %d bytes\n", ret_val);
	return ret_val;
}

static ssize_t mhi_uci_client_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *offp)
{
	uci_client *uci_handle = NULL;
	int ret_val = 0;
	int wait_ret = 0;
	u32 chan = 0xFFFFFFFF;

	mhi_uci_dump_log(UCI_DBG_ERROR, "uci_write: ", count, buf);

	if (NULL == file || NULL == buf ||
			0 == count || NULL == file->private_data)
		return -EINVAL;
	else
		uci_handle = (uci_client *)file->private_data;
	if (atomic_read(&mhi_uci_ctxt.mhi_disabled))
	{
		mhi_uci_log(UCI_DBG_VERBOSE,
				"Client attempted to write while MHI is disabled.\n");
		return -EIO;
	}
	chan = uci_handle->out_chan;
	mutex_lock(&uci_handle->uci_ctxt->client_chan_lock[chan]);
	while (ret_val == 0) {
		ret_val = mhi_uci_send_packet(&uci_handle->outbound_handle,
				(void *)buf, count, 1);
		if (0 == ret_val) {
			mhi_uci_log(UCI_DBG_VERBOSE,
					"No descriptors available, did we poll, chan %d?\n",
					chan);
			wait_ret = wait_event_interruptible_timeout(uci_handle->write_wait_queue,
					get_free_trbs(uci_handle->outbound_handle) > 0,
					msecs_to_jiffies(3*60*1000));
			if(wait_ret == 0) {
				mhi_uci_log(UCI_DBG_ERROR,
					"mhi uci write timeout %d?\n", chan);
				panic("CP Crash - mhi uci write timeout");
			}
		}
	}
	mutex_unlock(&uci_handle->uci_ctxt->client_chan_lock[chan]);
	return ret_val;
}

int mhi_uci_init(void)
{
	mhi_uci_ipc_log = ipc_log_context_create(MHI_UCI_IPC_LOG_PAGES, "mhi-uci");
	if (mhi_uci_ipc_log == NULL) {
		mhi_uci_log(UCI_DBG_WARNING, "Failed to create IPC logging context\n");
	}
	return platform_driver_register(&mhi_uci_driver);
}
int mhi_uci_remove(struct platform_device* dev)
{
	platform_driver_unregister(&mhi_uci_driver);
	return 0;
}

int mhi_uci_probe(struct platform_device *dev)
{
	u32 i = 0;
	MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	mhi_client_handle **init_handle = NULL;
	uci_client *curr_client = NULL;
	s32 r = 0;

	u32 ret;
	dev->dev.dma_mask = kmalloc(sizeof(*dev->dev.dma_mask), GFP_KERNEL);
	if(!dev->dev.dma_mask)
		printk("%s: dma_mask error\n", __func__);

	ret = dma_set_mask(&dev->dev, DMA_BIT_MASK(32));

	mhi_uci_ctxt.mhi_uci_dev = dev;

	mhi_uci_ctxt.client_info.mhi_client_cb = uci_xfer_cb;
	mhi_uci_ctxt.client_info.cb_mod = 1;

	for (i = 0; i < MHI_MAX_SOFTWARE_CHANNELS; ++i)
		mutex_init(&mhi_uci_ctxt.client_chan_lock[i]);
	mutex_init(&mhi_uci_ctxt.ctrl_mutex);

	ret_val = uci_init_client_attributes(&mhi_uci_ctxt);
	if (MHI_STATUS_SUCCESS != ret_val) {
		mhi_uci_log(UCI_DBG_ERROR,
				"Failed to init client attributes\n");
		return -EIO;
	}
	mhi_uci_ctxt.ctrl_chan_id = MHI_CLIENT_IP_CTRL_1_OUT;
	mhi_uci_log(UCI_DBG_VERBOSE, "Initializing clients\n");

	for (i = 0; i < MHI_SOFTWARE_CLIENT_LIMIT; ++i) {
		curr_client = &mhi_uci_ctxt.client_handle_list[i];
		init_handle = &curr_client->inbound_handle;
		init_waitqueue_head(&curr_client->read_wait_queue);
		init_waitqueue_head(&curr_client->write_wait_queue);
		curr_client->out_chan = i * 2;
		curr_client->in_chan = i * 2 + 1;
		curr_client->client_index = i;

		ret_val = mhi_open_channel(init_handle,
				curr_client->in_chan,
				0,
				&mhi_uci_ctxt.client_info,
				(void *)(long)(curr_client->in_chan));
		if (i != (mhi_uci_ctxt.ctrl_chan_id / 2))
			curr_client->dev_node_enabled = 1;

		if (MHI_STATUS_SUCCESS != ret_val)
			mhi_uci_log(UCI_DBG_ERROR,
					"Failed to open chan %d, ret 0x%x\n", i, ret_val);
		ret_val = mhi_init_inbound(dev, *init_handle, i);
		if (MHI_STATUS_SUCCESS != ret_val)
			mhi_uci_log(UCI_DBG_ERROR,
					"Failed to init inbound 0x%x, ret 0x%x\n", i, ret_val);
		if (curr_client->out_chan == mhi_uci_ctxt.ctrl_chan_id) {
			mhi_uci_log(UCI_DBG_ERROR,
					"Initializing ctrl chan id %d\n",
					curr_client->out_chan);
			ret_val = mhi_open_channel(&curr_client->outbound_handle,
					curr_client->out_chan,
					0,
					&mhi_uci_ctxt.client_info,
					(void *)(long)curr_client->out_chan);
			if (MHI_STATUS_SUCCESS != ret_val)
				mhi_uci_log(UCI_DBG_ERROR,
						"Failed to init outbound 0x%x, ret 0x%x\n", i, ret_val);
			mhi_uci_ctxt.ctrl_handle = curr_client->outbound_handle;
		}
	}
	mhi_uci_log(UCI_DBG_VERBOSE, "Allocating char devices\n");
	r = alloc_chrdev_region(&mhi_uci_ctxt.start_ctrl_nr,
			0, MHI_MAX_SOFTWARE_CHANNELS,
			DEVICE_NAME);

	if (IS_ERR_VALUE(r)) {
		mhi_uci_log(UCI_DBG_ERROR,
				"Failed to alloc char devs, ret 0x%x\n", r);
		goto failed_char_alloc;
	}
	mhi_uci_log(UCI_DBG_VERBOSE, "Creating class\n");
	mhi_uci_ctxt.mhi_uci_class = class_create(THIS_MODULE,
			DEVICE_NAME);
	if (IS_ERR(mhi_uci_ctxt.mhi_uci_class)) {
		mhi_uci_log(UCI_DBG_ERROR,
				"Failed to instantiate class, ret 0x%x\n", r);
		r = -ENOMEM;
		goto failed_class_add;
	}

	mhi_uci_log(UCI_DBG_VERBOSE, "Setting up contexts\n");
	for (i = 0; i < MHI_SOFTWARE_CLIENT_LIMIT; ++i) {

		cdev_init(&mhi_uci_ctxt.cdev[i], &mhi_uci_client_fops);
		mhi_uci_ctxt.cdev[i].owner = THIS_MODULE;
		r = cdev_add(&mhi_uci_ctxt.cdev[i],
				mhi_uci_ctxt.start_ctrl_nr + i , 1);
		if (IS_ERR_VALUE(r)) {
			mhi_uci_log(UCI_DBG_ERROR,
					"Failed to add cdev %d, ret 0x%x\n",
					i, r);
			goto failed_char_add;
		}
		mhi_uci_ctxt.client_handle_list[i].dev =
			device_create(mhi_uci_ctxt.mhi_uci_class, NULL,
					mhi_uci_ctxt.start_ctrl_nr + i,
					NULL, DEVICE_NAME "_pipe_%d", i * 2);

		if (IS_ERR(mhi_uci_ctxt.client_handle_list[i].dev)) {
			mhi_uci_log(UCI_DBG_ERROR,
					"Failed to add cdev %d\n", i);
			goto failed_device_create;
		}
	}
	return 0;

failed_char_add:
failed_device_create:
	while (i-- >= 0) {
		cdev_del(&mhi_uci_ctxt.cdev[i]);
		device_destroy(mhi_uci_ctxt.mhi_uci_class,
				MKDEV(MAJOR(mhi_uci_ctxt.start_ctrl_nr), i * 2));
	};
	class_destroy(mhi_uci_ctxt.mhi_uci_class);
failed_class_add:
	unregister_chrdev_region(MAJOR(mhi_uci_ctxt.start_ctrl_nr),
			MHI_MAX_SOFTWARE_CHANNELS);
failed_char_alloc:
	return r;
}

int mhi_uci_send_packet(mhi_client_handle **client_handle,
		void *buf, u32 size, u32 is_uspace_buf)
{
	u32 nr_avail_trbs = 0;
	u32 chain = 0;
	u32 i = 0;
	void *data_loc = NULL;
	uintptr_t memcpy_result = 0;
	u32 data_left_to_insert = 0;
	size_t data_to_insert_now = 0;
	u32 data_inserted_so_far = 0;
	dma_addr_t dma_addr = 0;
	u32 eob = 0;
	int ret_val = 0;
	uci_client *uci_handle;
	uci_handle = container_of(client_handle, uci_client, outbound_handle);

	if (NULL == client_handle || NULL == buf || 0 == size || NULL == uci_handle)
		return MHI_STATUS_ERROR;

	nr_avail_trbs = get_free_trbs(*client_handle);

	data_left_to_insert = size;

	if (0 == nr_avail_trbs)
		return 0;

	for (i = 0; i < nr_avail_trbs; ++i) {
		data_to_insert_now = MIN(data_left_to_insert,
				TRB_MAX_DATA_SIZE);

		if (is_uspace_buf) {
			data_loc = kmalloc(data_to_insert_now, GFP_DMA);
			mhi_uci_log(UCI_DBG_INFO,
					"Alloced buffer for chan %d buff 0x%p phys 0x%p\n",
					uci_handle->out_chan, data_loc,
					(void *)virt_to_phys (data_loc));
			if (NULL == data_loc) {
				mhi_uci_log(UCI_DBG_ERROR,
						"Failed to allocate memory 0x%lx\n",
						data_to_insert_now);
				return -ENOMEM;
			}
			memcpy_result = copy_from_user(data_loc,
					buf + data_inserted_so_far,
					data_to_insert_now);

			if (0 != memcpy_result)
				goto error_memcpy;
		} else {
			data_loc = buf;
		}
		dma_addr = dma_map_single(&(mhi_uci_ctxt.mhi_uci_dev->dev), data_loc,
				data_to_insert_now, DMA_TO_DEVICE);
		if (dma_mapping_error(&(mhi_uci_ctxt.mhi_uci_dev->dev), dma_addr)) {
			mhi_uci_log(UCI_DBG_ERROR,
					"Failed to Map DMA 0x%x\n", size);
			goto error_memcpy;
			return -ENOMEM;
		}

		chain = (data_left_to_insert - data_to_insert_now > 0) ? 1 : 0;
		eob = chain;
		mhi_uci_log(UCI_DBG_VERBOSE,
				"At trb i = %d/%d, chain = %d, eob = %d, addr 0x%lx chan %d\n", i,
				nr_avail_trbs, chain, eob, (uintptr_t)dma_addr,
				uci_handle->out_chan);
		ret_val = mhi_queue_xfer(*client_handle, dma_addr,
				data_to_insert_now, chain, eob);
		if (0 != ret_val) {
			goto error_queue;
		} else {
			data_left_to_insert -= data_to_insert_now;
			data_inserted_so_far += data_to_insert_now;
			atomic_inc(&uci_handle->out_pkt_pend_ack);
		}

		if (0 == data_left_to_insert)
			break;
	}
	return data_inserted_so_far;

error_queue:
	dma_unmap_single(&(mhi_uci_ctxt.mhi_uci_dev->dev),
			(dma_addr_t)dma_addr,
			data_to_insert_now,
			DMA_TO_DEVICE);
error_memcpy:
	mhi_uci_log(UCI_DBG_INFO,
			"Freeing for chan %d buffer 0x%p\n",
			uci_handle->out_chan,
			data_loc);
	kfree(data_loc);
	return data_inserted_so_far;
}

/**
 * @brief Statically initialize the channel attributes table.
 *	 This table contains information on the nature of the transfer
 *	 on a particular pipe; information which helps us optimize
 *	 the memory allocation layout
 *
 * @param device [IN/OUT] reference to a mhi context to be populated
 *
 * @return MHI_STATUS
 */
MHI_STATUS uci_init_client_attributes(mhi_uci_ctxt_t *mhi_uci_ctxt)
{
	u32 i = 0;
	u32 nr_trbs = MAX_NR_TRBS_PER_CHAN;
	u32 data_size = TRB_MAX_DATA_SIZE;
	chan_attr *chan_attributes = NULL;
	for (i = 0; i < MHI_MAX_SOFTWARE_CHANNELS; ++i) {
		chan_attributes = &mhi_uci_ctxt->channel_attributes[i];
		chan_attributes->chan_id = i;
		chan_attributes->max_packet_size = data_size;
		chan_attributes->avg_packet_size = data_size;
		chan_attributes->max_nr_packets = nr_trbs;
		chan_attributes->nr_trbs = nr_trbs;
		if (i % 2 == 0)
			chan_attributes->dir = MHI_DIR_OUT;
		else
			chan_attributes->dir = MHI_DIR_IN;
	}
	return MHI_STATUS_SUCCESS;
}

MHI_STATUS mhi_init_inbound(struct platform_device *dev, mhi_client_handle *client_handle,
		MHI_CLIENT_CHANNEL chan)
{

	MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	u32 i = 0;
	dma_addr_t dma_addr = 0;
	chan_attr *chan_attributes = &mhi_uci_ctxt.channel_attributes[chan];
	void *data_loc = NULL;
	size_t buf_size = chan_attributes->max_packet_size;

	if (NULL == client_handle) {
		mhi_uci_log(UCI_DBG_ERROR, "Bad Input data, quitting\n");
		return MHI_STATUS_ERROR;
	}
	for (i = 0; i < (chan_attributes->nr_trbs - 1); ++i) {
		data_loc = kmalloc(buf_size, GFP_DMA);
		mhi_uci_log(UCI_DBG_INFO,
			   "Alloced buffer for chan %d buff 0x%p\n",
			   chan, data_loc);
		dma_addr = dma_map_single(&(dev->dev), data_loc,
				buf_size, DMA_BIDIRECTIONAL);
		mhi_uci_log(UCI_DBG_INFO,
			   "dma_addr %llx\n",
			   dma_addr);
		if (dma_mapping_error(&(dev->dev), dma_addr)) {
			mhi_uci_log(UCI_DBG_ERROR, "Failed to Map DMA\n");
			return -ENOMEM;
		}
		(ret_val = mhi_queue_xfer(client_handle,
					  dma_addr, buf_size, 0, 0));
		if (MHI_STATUS_SUCCESS != ret_val)
			goto error_insert;
	}
	return ret_val;
error_insert:
	mhi_uci_log(UCI_DBG_ERROR,
			"Failed insertion for chan %d\n", chan);

	return MHI_STATUS_ERROR;
}

void uci_xfer_cb(mhi_cb_info *cb_info)
{
	u32 chan_nr;
	uci_client *uci_handle = NULL;
	u32 client_index;
	mhi_result *result;
	u32 i = 0;

	switch (cb_info->cb_reason) {
		case MHI_CB_MHI_ENABLED :
			mhi_uci_log(UCI_DBG_CRITICAL, "Enabling MHI.\n");
			atomic_set(&mhi_uci_ctxt.mhi_disabled, 0);
			break;
		case MHI_CB_MHI_DISABLED :
			if (!atomic_cmpxchg(&mhi_uci_ctxt.mhi_disabled, 0, 1)) {
				for (i = 0; i < MHI_MAX_NR_OF_CLIENTS; ++i) {
					wmb();
					uci_handle =
						&mhi_uci_ctxt.client_handle_list[i];
					if (uci_handle->mhi_status != -ENETRESET) {
						mhi_uci_log(UCI_DBG_CRITICAL,
								"Setting reset for chan %d.\n",
								i * 2);
						uci_handle->mhi_status = -ENETRESET;
						atomic_inc(&uci_handle->avail_pkts);
						wake_up(&uci_handle->read_wait_queue);
						wake_up(&uci_handle->write_wait_queue);
					} else {
						mhi_uci_log(UCI_DBG_CRITICAL,
								"Chan %d state already reset.\n",
								i*2);
					}
				}
			}
			break;
		case MHI_CB_XFER_SUCCESS:
			if (cb_info->result == NULL) {
				mhi_uci_log(UCI_DBG_CRITICAL,
						"Failed to obtain mhi result from CB.\n");
				return;
			}
			result = cb_info->result;
			chan_nr = (long)result->user_data;
			client_index = chan_nr / 2;
			uci_handle =
				&mhi_uci_ctxt.client_handle_list[client_index];
			if (chan_nr % 2) {
				atomic_inc(&uci_handle->avail_pkts);
				mhi_uci_log(UCI_DBG_VERBOSE,
						"Received cb on chan %d, avail pkts: 0x%x\n",
						chan_nr,
						atomic_read(&uci_handle->avail_pkts));
				wake_up(&uci_handle->read_wait_queue);
			} else {
				dma_unmap_single(&(mhi_uci_ctxt.mhi_uci_dev->dev),
						(dma_addr_t)(uintptr_t)result->payload_buf,
						result->bytes_xferd,
						DMA_TO_DEVICE);
				mhi_uci_log(UCI_DBG_INFO,
						"Freeing for chan %d buffer 0x%p phys 0x%p\n",
						chan_nr,
						phys_to_virt((uintptr_t)result->payload_buf),  result->payload_buf);
				kfree(phys_to_virt((dma_addr_t)(uintptr_t)result->payload_buf));
				mhi_uci_log(UCI_DBG_VERBOSE,
						"Received ack on chan %d, pending acks: 0x%x\n",
						chan_nr,
						atomic_read(&uci_handle->out_pkt_pend_ack));
				atomic_dec(&uci_handle->out_pkt_pend_ack);
				if (get_free_trbs(uci_handle->outbound_handle))
					wake_up(&uci_handle->write_wait_queue);
			}
			break;
		default:
			mhi_uci_log(UCI_DBG_VERBOSE,
					"Cannot handle cb reason 0x%x\n",
					cb_info->cb_reason);
	}
}
void process_rs232_state(mhi_result *result)
{
	rs232_ctrl_msg *rs232_pkt = NULL;
	uci_client* client = NULL;
	u32 msg_id;
	MHI_STATUS ret_val = MHI_STATUS_ERROR;
	u32 chan;
	mutex_lock(&mhi_uci_ctxt.ctrl_mutex);
	if (result->transaction_status != MHI_STATUS_SUCCESS) {
		mhi_uci_log(UCI_DBG_ERROR,
				"Non successful transfer code 0x%x\n",
				result->transaction_status);
		goto error_bad_xfer;
	}
	if (result->bytes_xferd != sizeof(rs232_ctrl_msg)) {
		mhi_uci_log(UCI_DBG_ERROR,
				"Buffer is of wrong size is: 0x%ld: expected 0x%ld\n",
				(long)result->bytes_xferd, (long)sizeof(rs232_ctrl_msg));
		goto error_size;
	}
	dma_unmap_single(&(mhi_uci_ctxt.mhi_uci_dev->dev), (dma_addr_t)(uintptr_t)result->payload_buf,
			result->bytes_xferd, DMA_BIDIRECTIONAL);
	rs232_pkt = phys_to_virt((dma_addr_t)(uintptr_t)result->payload_buf);
	MHI_GET_CTRL_DEST_ID(CTRL_DEST_ID, rs232_pkt, chan);
	client = &mhi_uci_ctxt.client_handle_list[chan / 2];


	MHI_GET_CTRL_MSG_ID(CTRL_MSG_ID, rs232_pkt, msg_id);
	client->local_tiocm = 0;
	if (MHI_SERIAL_STATE_ID == msg_id) {
		client->local_tiocm |=
			MHI_GET_STATE_MSG(STATE_MSG_DCD, rs232_pkt) ?
			TIOCM_CD : 0;
		client->local_tiocm |=
			MHI_GET_STATE_MSG(STATE_MSG_DSR, rs232_pkt) ?
			TIOCM_DSR : 0;
		client->local_tiocm |=
			MHI_GET_STATE_MSG(STATE_MSG_RI, rs232_pkt) ?
			TIOCM_RI : 0;
	}
error_bad_xfer:
error_size:
	if(rs232_pkt != NULL){
	        memset(rs232_pkt, 0, sizeof(rs232_ctrl_msg));
	        dma_map_single(&(mhi_uci_ctxt.mhi_uci_dev->dev), rs232_pkt,
		        	sizeof(rs232_ctrl_msg),
		        	DMA_BIDIRECTIONAL);
	}

	if(client != NULL)
 	        ret_val = mhi_client_recycle_trb(client->inbound_handle);
	if (MHI_STATUS_SUCCESS != ret_val){
		mhi_uci_log(UCI_DBG_ERROR,
				"Failed to recycle ctrl msg buffer\n");
	}
	mutex_unlock(&mhi_uci_ctxt.ctrl_mutex);
}
int mhi_uci_send_status_cmd(uci_client *client)
{
	rs232_ctrl_msg *rs232_pkt = NULL;
	uci_client *uci_handle;
	u32 ctrl_chan_id = mhi_uci_ctxt.ctrl_chan_id;

	int ret_val = 0;
	size_t pkt_size = sizeof(rs232_ctrl_msg);
	u32 amount_sent;

	rs232_pkt = kmalloc(sizeof(rs232_ctrl_msg), GFP_DMA);
	mhi_uci_log(UCI_DBG_INFO,
		   "Alloced buffer for chan %d buff 0x%p\n",
		   ctrl_chan_id, rs232_pkt);

	if (NULL == rs232_pkt)
		return -ENOMEM;
	mhi_uci_log(UCI_DBG_VERBOSE,
			"Received request to send msg for chan %d\n",
			client->out_chan);
	memset(rs232_pkt, 0, sizeof(rs232_ctrl_msg));
	rs232_pkt->preamble = CTRL_MAGIC;
	if (client->local_tiocm & TIOCM_DTR) {
		MHI_SET_CTRL_MSG(CTRL_MSG_DTR, rs232_pkt, 1);
	}
	if (client->local_tiocm & TIOCM_RTS) {
		MHI_SET_CTRL_MSG(CTRL_MSG_RTS, rs232_pkt, 1);
	}
	MHI_SET_CTRL_MSG_ID(CTRL_MSG_ID, rs232_pkt, MHI_CTRL_LINE_STATE_ID);
	MHI_SET_CTRL_MSG_SIZE(CTRL_MSG_SIZE, rs232_pkt, sizeof(u32));
	MHI_SET_CTRL_DEST_ID(CTRL_DEST_ID, rs232_pkt, client->out_chan);

	if (MHI_STATUS_SUCCESS != ret_val) {
		mhi_uci_log(UCI_DBG_CRITICAL,
				"Could not open chan %d, for sideband ctrl\n",
				client->out_chan);
		goto error;
	}


	uci_handle = &mhi_uci_ctxt.client_handle_list[ctrl_chan_id/2];

	amount_sent = mhi_uci_send_packet(&uci_handle->outbound_handle, rs232_pkt,
			pkt_size, 0);
	if (pkt_size != amount_sent){
		mhi_uci_log(UCI_DBG_INFO,
				"Failed to send signal on chan %d, ret : %d n",
				client->out_chan, ret_val);
		goto error;
	}
	return ret_val;
error:
	mhi_uci_log(UCI_DBG_INFO,
			"Freeing for chan %d buffer 0x%p\n",
			ctrl_chan_id,
			rs232_pkt);
	kfree(rs232_pkt);
	return ret_val;
}
