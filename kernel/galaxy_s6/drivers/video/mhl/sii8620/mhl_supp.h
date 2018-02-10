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

#if !defined(MHL_SUPP_H)
#define MHL_SUPP_H

/* APIs exported from mhl_supp.c */

int si_mhl_tx_get_num_block_reqs(void);
int si_mhl_tx_initialize(struct mhl_dev_context *dev_context);
void si_mhl_tx_initialize_block_transport(struct mhl_dev_context *dev_context);
void process_cbus_abort(struct mhl_dev_context *dev_context);
void si_mhl_tx_drive_states(struct mhl_dev_context *dev_context);
void si_mhl_tx_push_block_transactions(struct mhl_dev_context *dev_context);
void initiate_bist_test(struct mhl_dev_context *dev_context);
void si_mhl_tx_process_events(struct mhl_dev_context *dev_context);
uint8_t si_mhl_tx_set_preferred_pixel_format(struct mhl_dev_context
					     *dev_context, uint8_t clkMode);
void si_mhl_tx_process_write_burst_data(struct mhl_dev_context *dev_context);
void si_mhl_tx_msc_command_done(struct mhl_dev_context *dev_context,
				uint8_t data1);
void si_mhl_tx_notify_downstream_hpd_change(struct mhl_dev_context *dev_context,
					    uint8_t downstream_hpd);
void si_mhl_tx_got_mhl_status(struct mhl_dev_context *dev_context,
			      struct mhl_device_status *write_stat);
void si_mhl_tx_got_mhl_intr(struct mhl_dev_context *dev_context,
			    uint8_t intr_0, uint8_t intr_1);
enum scratch_pad_status si_mhl_tx_request_write_burst(
	struct mhl_dev_context *dev_context, uint8_t reg_offset,
	uint8_t length, uint8_t *data);
bool si_mhl_tx_send_msc_msg(struct mhl_dev_context *dev_context,
			uint8_t command, uint8_t cmdData);
bool si_mhl_tx_rcp_send(struct mhl_dev_context *dev_context,
			uint8_t rcpKeyCode);
bool si_mhl_tx_rcpk_send(struct mhl_dev_context *dev_context,
			uint8_t rcp_key_code);
bool si_mhl_tx_rcpe_send(struct mhl_dev_context *dev_context,
			 uint8_t rcpe_error_code);
bool si_mhl_tx_rbp_send(struct mhl_dev_context *dev_context,
			uint8_t rbpButtonCode);
bool si_mhl_tx_rbpk_send(struct mhl_dev_context *dev_context,
			 uint8_t rbp_button_code);
bool si_mhl_tx_rbpe_send(struct mhl_dev_context *dev_context,
			uint8_t rbpe_error_code);
bool si_mhl_tx_ucp_send(struct mhl_dev_context *dev_context,
			uint8_t ucp_key_code);
bool si_mhl_tx_ucpk_send(struct mhl_dev_context *dev_context,
			uint8_t ucp_key_code);
bool si_mhl_tx_ucpe_send(struct mhl_dev_context *dev_context,
			uint8_t ucpe_error_code);
bool si_mhl_tx_rap_send(struct mhl_dev_context *dev_context,
			uint8_t rap_action_code);
struct cbus_req *peek_next_cbus_transaction(struct mhl_dev_context
			*dev_context);
bool si_mhl_tx_send_3d_req_or_feat_req(struct mhl_dev_context *dev_context);

enum bist_cmd_status {
	BIST_STATUS_NO_ERROR,
	BIST_STATUS_NOT_IN_OCBUS,
	BIST_STATUS_NOT_IN_ECBUS,
	BIST_STATUS_INVALID_SETUP,
	BIST_STATUS_INVALID_TRIGGER,
	BIST_STATUS_DUT_NOT_READY,
	BIST_STATUS_NO_RESOURCES,
	BIST_STATUS_TEST_IP,
	BIST_STATUS_NO_TEST_IP
};
enum bist_cmd_status si_mhl_tx_bist_ready(struct mhl_dev_context *dev_context,
					  uint8_t *bist_ready_status);

#define BIST_TRIGGER_OPERAND_eCBUS_TX_BIST		0x01
#define BIST_TRIGGER_OPERAND_eCBUS_RX_BIST		0x02
#define BIST_TRIGGER_OPERAND_SELECT_eCBUS_S		0x00
#define BIST_TRIGGER_OPERAND_SELECT_eCBUS_D		0x80
#define BIST_TRIGGER_OPERAND_AVLINK_TX_BIST		0x10
#define BIST_TRIGGER_OPERAND_AVLINK_RX_BIST		0x20
#define BIST_TRIGGER_OPERAND_IMPEDANCE_BIST		0x40
#define BIST_TRIGGER_OPERAND_VALID_MASK			0x7B

#define BIST_READY_OPERAND_eCBUS_READY			0x01
#define BIST_READY_OPERAND_AVLINK_READY			0x02
#define BIST_READY_OPERAND_TERM_READY			0x04
#define BIST_READY_OPERAND_eCBUS_ERROR			0x10
#define BIST_READY_OPERAND_AVLINK_ERROR			0x20
#define BIST_READY_OPERAND_TERM_ERROR			0x40

enum bist_cmd_status si_mhl_tx_bist_setup(struct mhl_dev_context *dev_context,
					  struct bist_setup_info *setup);
enum bist_cmd_status si_mhl_tx_bist_trigger(struct mhl_dev_context *dev_context,
					    uint8_t trigger_operand);
enum bist_cmd_status si_mhl_tx_bist_stop(struct mhl_dev_context *dev_context);
enum bist_cmd_status si_mhl_tx_bist_request_stat(struct mhl_dev_context
						 *dev_context,
						 uint8_t request_operand);
int si_mhl_tx_shutdown(struct mhl_dev_context *dev_context);
int si_mhl_tx_ecbus_started(struct mhl_dev_context *dev_context);
void si_mhl_tx_check_av_link_status(struct mhl_dev_context *dev_context);
void si_mhl_tx_send_blk_rcv_buf_info(struct mhl_dev_context *dev_context);
void si_mhl_tx_initialize_block_transport(struct mhl_dev_context *dev_context);

#endif /* #if !defined(MHL_SUPP_H) */
