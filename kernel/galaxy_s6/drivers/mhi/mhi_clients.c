/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include "mhi_rmnet.h"
#include "mhi_shim.h"
#include "mhi_macros.h"
#include "mhi.h"


inline MHI_RMNET_STATUS mhi_rmnet_open_channel(
			mhi_rmnet_client_handle * mhi_rmnet_handle,
			MHI_RMNET_HW_CLIENT_CHANNEL chan_id,
			void *user_data,
			mhi_rmnet_client_info *cbs)
{
	return (MHI_RMNET_STATUS)mhi_open_channel(
			(mhi_client_handle **)mhi_rmnet_handle,
			chan_id,
			0, (mhi_client_info_t *)cbs, user_data);
}

/* MHI RMnet API Implementation */
inline MHI_RMNET_STATUS mhi_rmnet_queue_buffer(
		mhi_rmnet_client_handle mhi_rmnet_handle,
		uintptr_t buf, size_t len)
{
	return (MHI_RMNET_STATUS)mhi_queue_xfer(
					(mhi_client_handle *)mhi_rmnet_handle,
					buf, len, 0, 0);
}
inline void mhi_rmnet_close_channel(mhi_rmnet_client_handle mhi_rmnet_handle)
{
	mhi_close_channel((mhi_client_handle *)mhi_rmnet_handle);
}
inline void mhi_rmnet_mask_irq(mhi_rmnet_client_handle mhi_rmnet_handle)
{
	mhi_mask_irq((mhi_client_handle *)mhi_rmnet_handle);
}
inline void mhi_rmnet_unmask_irq(mhi_rmnet_client_handle mhi_rmnet_handle)
{
	mhi_unmask_irq((mhi_client_handle *)mhi_rmnet_handle);
}
inline mhi_rmnet_result *mhi_rmnet_poll(
				mhi_rmnet_client_handle mhi_rmnet_handle)
{
	return (mhi_rmnet_result *)mhi_poll(
				(mhi_client_handle *)mhi_rmnet_handle);
}
inline MHI_RMNET_STATUS mhi_rmnet_reset_channel(mhi_rmnet_client_handle
							mhi_rmnet_handle)
{
	return (MHI_RMNET_STATUS)MHI_STATUS_SUCCESS;
}

inline uint32_t mhi_rmnet_get_max_buffers(mhi_rmnet_client_handle
							mhi_rmnet_handle)
{
	return MAX_NR_TRBS_PER_HARD_CHAN - 1;
}
uint32_t mhi_rmnet_get_epid(mhi_rmnet_client_handle mhi_rmnet_handle)
{
	return MHI_EPID;
}

/* MHI SHIM API implementation */
inline int mhi_shim_get_free_buf_count(mhi_shim_client_handle client_handle)
{
	return get_free_trbs((mhi_client_handle *)client_handle);
}

inline void mhi_shim_poll_inbound(mhi_shim_client_handle client_handle,
					uintptr_t *buf, ssize_t *buf_size)
{
	mhi_poll_inbound((mhi_client_handle *)client_handle, buf, buf_size);
}

inline void mhi_shim_close_channel(mhi_shim_client_handle client_handle)
{
	mhi_close_channel((mhi_client_handle *)client_handle);
}
MHI_SHIM_STATUS mhi_shim_queue_xfer(mhi_shim_client_handle client_handle,
			uintptr_t buf, size_t buf_len, u32 chain, u32 eob)
{
	return (MHI_SHIM_STATUS)mhi_queue_xfer(
			(mhi_client_handle *)client_handle,
			buf, buf_len, chain, eob);
}
MHI_SHIM_STATUS mhi_shim_recycle_buffer(mhi_shim_client_handle client_handle)
{
	return (MHI_SHIM_STATUS)mhi_client_recycle_trb(
				(mhi_client_handle *)client_handle);
}
MHI_SHIM_STATUS mhi_shim_open_channel(mhi_shim_client_handle *client_handle,
		MHI_SHIM_CLIENT_CHANNEL chan, s32 device_index,
		mhi_shim_client_info_t *cbs, void *user_data)
{
	return (MHI_SHIM_STATUS)mhi_open_channel(
			(mhi_client_handle **)client_handle,
			(MHI_CLIENT_CHANNEL)chan, device_index,
			(mhi_client_info_t *)cbs, user_data);
}

