/*
 * Copyright 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation (the "GPL").
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *  
 * A copy of the GPL is available at 
 * http://www.broadcom.com/licenses/GPLv2.php, or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The BBD (Broadcom Bridge Driver)
 *
 * tabstop = 8
 */

#ifndef __BBD_H__
#define __BBD_H__

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))

//hoi changed
//#define BBD_MAX_DATA_SIZE	 896  /* max data size for transition  */
#define BBD_MAX_DATA_SIZE	 4096  /* max data size for transition  */

#define BBD_MAX_FREED_NUM	 10    /* max number of freed list      */

/** the status of downloading firmware **/
enum {
	BBD_FW_COMPLETED,
	BBD_FW_DOWNLOADING,
	BBD_FW_FAILED
};

/** callback for incoming data from 477x to senser hub driver **/
typedef int (*bbd_on_packet)(void *ssh_data, unsigned char *buf, size_t size);
typedef int (*bbd_on_packet_alarm)(void *ssh_data);
typedef int (*bbd_on_control)(void *ssh_data, char *str_ctrl);
typedef int (*bbd_on_mcu_ready)(void *ssh_data, bool ready);

typedef struct {
	bbd_on_packet		on_packet;
	bbd_on_packet_alarm	on_packet_alarm;
	bbd_on_control		on_control;
	bbd_on_mcu_ready	on_mcu_ready;
}bbd_callbacks;

extern void	bbd_register(void* ext_data, bbd_callbacks *pcallbacks);
extern ssize_t	bbd_send_packet(unsigned char *buf, size_t size);
extern ssize_t	bbd_pull_packet(unsigned char *pbuff, size_t len, unsigned int timeout_ms);
extern int	bbd_control(char *str_ctrl);
extern int	bbd_mcu_reset(void);

#endif /* __BBD_H__ */
