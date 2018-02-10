/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#if !defined(SI_MHL_TX_DRV_API_H)
#define SI_MHL_TX_DRV_API_H

/*
 * Structure to hold command details from upper layer to CBUS module
 */
struct cbus_req {
	struct list_head link;
	union {
		struct {
			uint8_t cancel:1; /* this command has been canceled */
			uint8_t resvd:7;
		} flags;
		uint8_t as_uint8;
	} status;
	uint8_t retry_count;
	uint8_t command;	/* VS_CMD or RCP opcode */
	uint8_t reg;
	uint8_t reg_data;
	uint8_t burst_offset;	/* register offset */
	uint8_t length;		/* Only applicable to write burst */
	uint8_t msg_data[16];	/* scratch pad data area. */
	const char *function;
	int line;
	int sequence;
};
struct SI_PACK_THIS_STRUCT tport_hdr_and_burst_id_t {
	/* waste two bytes to save on memory copying when submitting BLOCK
	 * transactions
	 */
	struct SI_PACK_THIS_STRUCT standard_transport_header_t tport_hdr;

	/* sub-payloads start here */
	struct MHL_burst_id_t burst_id;
};
union SI_PACK_THIS_STRUCT emsc_payload_t {
	struct SI_PACK_THIS_STRUCT tport_hdr_and_burst_id_t hdr_and_burst_id;
	uint8_t as_bytes[256];
};

struct SI_PACK_THIS_STRUCT block_req {
	struct list_head link;
	const char *function;
	int line;
	int sequence;
	uint16_t count;		/* (size - 1) see MHL spec section 13.5.7.2 */
	uint8_t space_remaining;
	uint8_t sub_payload_size;
	uint8_t *platform_header;
	union SI_PACK_THIS_STRUCT emsc_payload_t *payload;
};

enum quantization_settings_e {
	qs_auto_select_by_color_space,
	qs_full_range,
	qs_limited_range,
	qs_reserved
};

struct bist_setup_info {
	uint8_t e_cbus_duration;
	uint8_t e_cbus_pattern;
	uint16_t e_cbus_fixed_pat;
	uint8_t avlink_data_rate;
	uint8_t avlink_pattern;
	uint8_t avlink_video_mode;
	uint8_t avlink_duration;
	uint16_t avlink_fixed_pat;
	uint8_t avlink_randomizer;
	uint8_t impedance_mode;
};

struct bist_stat_info {
	uint16_t e_cbus_stat;
	uint16_t avlink_stat;
};

/*
 * The APIs listed below must be implemented by the MHL transmitter
 * hardware support module.
 */

struct drv_hw_context;
struct interrupt_info;

int si_mhl_tx_chip_initialize(struct drv_hw_context *hw_context);
void si_mhl_tx_drv_device_isr(struct drv_hw_context *hw_context,
	struct interrupt_info *intr_info);
void si_mhl_tx_drv_disable_video_path(struct drv_hw_context *hw_context);
void si_mhl_tx_drv_enable_video_path(struct drv_hw_context *hw_context);

void si_mhl_tx_drv_content_on(struct drv_hw_context *hw_context);
void si_mhl_tx_drv_content_off(struct drv_hw_context *hw_context);
uint8_t si_mhl_tx_drv_send_cbus_command(struct drv_hw_context *hw_context,
	struct cbus_req *req);

void mhl_tx_drv_send_block(struct drv_hw_context *hw_context,
	struct block_req *req);
int si_mhl_tx_drv_get_scratch_pad(struct drv_hw_context *hw_context,
	uint8_t start_reg, uint8_t *data, uint8_t length);
void si_mhl_tx_read_devcap_fifo(struct drv_hw_context *hw_context,
	union MHLDevCap_u *dev_cap_buf);
void si_mhl_tx_read_xdevcap_fifo(struct drv_hw_context *hw_context,
	union MHLXDevCap_u *xdev_cap_buf);
int si_mhl_tx_drv_get_aksv(struct drv_hw_context *hw_context, uint8_t *buffer);

int si_mhl_tx_drv_start_avlink_bist(struct drv_hw_context *hw_context,
	struct bist_setup_info *test_info);
int si_mhl_tx_drv_stop_avlink_bist(struct drv_hw_context *hw_context);
int si_mhl_tx_drv_start_ecbus_bist(struct drv_hw_context *hw_context,
	struct bist_setup_info *test_info);
int si_mhl_tx_drv_stop_ecbus_bist(struct drv_hw_context *hw_context,
	struct bist_setup_info *test_info);
int si_mhl_tx_drv_start_impedance_bist(struct drv_hw_context *hw_context,
	struct bist_setup_info *test_info);
void si_mhl_tx_drv_stop_impedance_bist(struct drv_hw_context *hw_context,
	struct bist_setup_info *test_info);
void si_mhl_tx_drv_leave_bist_mode(struct drv_hw_context *hw_context);

void si_mhl_tx_drv_shutdown(struct drv_hw_context *hw_context);

int si_mhl_tx_drv_connection_is_mhl3(struct mhl_dev_context *dev_context);

int si_mhl_tx_drv_get_highest_tmds_link_speed(
	struct mhl_dev_context *dev_context);

uint8_t si_mhl_tx_drv_hawb_xfifo_avail(struct mhl_dev_context *dev_context);

uint8_t si_mhl_tx_drv_get_pending_hawb_write_status(
	struct mhl_dev_context *dev_context);

enum cbus_mode_e {
	CM_NO_CONNECTION,
	CM_oCBUS_PEER_VERSION_PENDING,
	CM_oCBUS_PEER_IS_MHL1_2,
	CM_oCBUS_PEER_IS_MHL3,
	CM_TRANSITIONAL_TO_eCBUS_S,
	CM_TRANSITIONAL_TO_eCBUS_S_CALIBRATED,
	CM_TRANSITIONAL_TO_eCBUS_D,
	CM_TRANSITIONAL_TO_eCBUS_D_CALIBRATED,
	CM_eCBUS_S,
	CM_eCBUS_D
};

enum cbus_mode_e si_mhl_tx_drv_get_cbus_mode(
	struct mhl_dev_context *dev_context);
char *si_mhl_tx_drv_get_cbus_mode_str(enum cbus_mode_e cbus_mode);

uint16_t si_mhl_tx_drv_get_blk_rcv_buf_size(void);

void si_mhl_tx_drv_start_cp(struct mhl_dev_context *dev_context);
void si_mhl_tx_drv_shut_down_HDCP2(struct drv_hw_context *hw_context);

bool si_mhl_tx_drv_support_e_cbus_d(struct drv_hw_context *hw_context);

int si_mhl_tx_drv_switch_cbus_mode(struct drv_hw_context *hw_context,
	enum cbus_mode_e mode_sel);
void si_mhl_tx_drv_free_block_input_buffer(struct mhl_dev_context *dev_context);
int si_mhl_tx_drv_peek_block_input_buffer(struct mhl_dev_context *dev_context,
	uint8_t **buffer, int *length);

enum tdm_vc_num {
	VC_CBUS1,
	VC_E_MSC,
	VC_T_CBUS,
	VC_MAX
};
int si_mhl_tx_drv_set_tdm_slot_allocation(struct drv_hw_context *hw_context,
	uint8_t *vc_slot_counts, bool program);

#endif /* if !defined(SI_MHL_TX_DRV_API_H) */
