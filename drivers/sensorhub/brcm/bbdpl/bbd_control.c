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

#include "bbd_internal.h"

ssize_t bbd_ESW_control(char *buf, ssize_t len);

int bbd_tty_open (void);
int bbd_tty_close(void);

struct bcm4773_uart_port;
void bcm4773_mcu_resp_float(bool do_float);
void BbdEngine_SetUp(struct BbdEngine *p);
struct ssp_data;
extern void reset_mcu(struct ssp_data *data);

extern bool ssp_dbg;
extern bool ssp_pkt_dbg;
extern bool ssi_dbg;

/**
 * initialize control-specific variables
 *
 * @pbbd_dev: bbd driver pointer
 *
 * @return: *result 0 = success, others = failure
 */

void bbd_control_init_vars(struct bbd_device *dev, int* result)
{
        struct bbd_control_device *p = 0;

        if (*result)
                return;

	p = bbd_alloc(sizeof(struct bbd_control_device));
        if (!p) {
		*result = -ENOMEM;
		return;
        }

        bbd_base_init_vars(dev, &p->base, result);
        if (*result) {
                bbd_free(p);
                return;
        }
        dev->bbd_ptr[BBD_MINOR_CONTROL] = p;
        p->base.name = "control";
}

struct bbd_control_device *bbd_control_ptr(void)
{
	if (!gpbbd_dev)
		return 0;
	return gpbbd_dev->bbd_ptr[BBD_MINOR_CONTROL];
}

struct bbd_base *bbd_control_ptr_base(void)
{
        struct bbd_control_device *p = bbd_control_ptr();
	if (!p)
		return 0;
	return &p->base;
}

int bbd_control_uninit(void)
{
        struct bbd_control_device* p = bbd_control_ptr();
        int freed = 0;
        if (p) {
                freed = bbd_base_uninit(&p->base);
                bbd_free(p);
                gpbbd_dev->bbd_ptr[BBD_MINOR_CONTROL] = 0;
        }
        return freed;
}

/**
 * sensor hub driver or internal PL sends string control
 *
 * somewhat primitive - no queueing or guarantees at all.
 *
 * @str_ctrl: string control
 *
 * @return: 0 = success, -1 = failure
 */
int bbd_control(char *str)
{
        return bbd_on_read(BBD_MINOR_CONTROL, str, strlen(str) + 1);
}
EXPORT_SYMBOL(bbd_control);

/**
 * sensor hub request reset to 477x
 *
 * @return: 0 = success, -1 = failure
 */
int bbd_mcu_reset(void)
{
	dprint("reset request from sensor hub\n");
	return bbd_control(BBD_CTRL_RESET_REQ);
}
EXPORT_SYMBOL(bbd_mcu_reset);


/**
 * open bbd control driver
 */
int bbd_control_open(struct inode *inode, struct file *filp)
{
	filp->private_data = gpbbd_dev;
	dprint("opened\n");
	return 0;
}

/**
 * close bbd control driver
 */
int bbd_control_release(struct inode *inode, struct file *filp)
{
	if (!gpbbd_dev)
		return -EFAULT;
#if 0
	/* Need to set SSP down because we're closing */
	bbd_ESW_control(ESW_CTRL_NOTREADY, sizeof(ESW_CTRL_NOTREADY));
#endif
        return bbd_base_reinit(BBD_MINOR_CONTROL);
}

/**
 * read signal/data from bbd_list_head to user
 */
ssize_t bbd_control_read(struct file *filp, char __user *buf,
				size_t size, loff_t *ppos)
{
	return bbd_read(BBD_MINOR_CONTROL, buf, size, true);
}


/**
 * send string control directly from lhd to bbd for these purposes:
 * 1)  BBD control
 * 2)  BBD packet layer
 * 3)  SH driver
 */

ssize_t bbd_control_write(struct file *filp,
				const char __user *buf,
				size_t size, loff_t *ppos)
{
	struct bbd_base *pb = bbd_control_ptr_base();
	ssize_t len = bbd_write(BBD_MINOR_CONTROL, buf, size, true);
	if (len < 0)
		return len;
	return bbd_ESW_control(pb->rxb.pbuff, pb->rxb.buff_size);
}

bool esw_ready(void)
{
        struct bbd_control_device *p    =    bbd_control_ptr();
	if (!p)
		return false;

	return p->esw_ready;
}

ssize_t bbd_ESW_control(char *buf, ssize_t len)
{
	ssize_t ret = len;
        struct bbd_control_device *p    =    bbd_control_ptr();
        struct bbd_sensor_device  *ps   =    bbd_sensor_ptr();
        struct BbdEngine          *pEng = &gpbbd_dev->bbd_engine;
        bool bSetup = false;

        printk("%s : %s \n", __func__, buf);

	if (!p || !ps)
		return -EFAULT;

	if (strstr(buf, ESW_CTRL_READY)) {
		p->esw_ready = true;
//		bcm4773_mcu_resp_float(true);
		if (ps->callbacks && ps->callbacks->on_mcu_ready) {
			ret = ps->callbacks->on_mcu_ready(
						ps->priv_data,
						p->esw_ready);
		}
	} else if (strstr(buf, ESW_CTRL_NOTREADY)) {
		p->esw_ready = false;
		ps->base.rxb.buff_size = 0;
//		bcm4773_mcu_resp_float(false);
		if (ps->callbacks && ps->callbacks->on_mcu_ready) {
			ret = ps->callbacks->on_mcu_ready(ps->priv_data,
						 p->esw_ready);
		}
	} else if (strstr(buf, ESW_CTRL_CRASHED)) {
		p->esw_ready     = false;
		ps->base.rxb.buff_size = 0;
//		bcm4773_mcu_resp_float(false);
		if (ps->callbacks && ps->callbacks->on_control)
			ret = ps->callbacks->on_control(ps->priv_data, buf);
	} else if (strstr(buf, BBD_CTRL_DEBUG_ON)) {
                gpbbd_dev->db = true;
		bSetup = true;
	} else if (strstr(buf, BBD_CTRL_DEBUG_OFF)) {
                gpbbd_dev->db = false;
		bSetup = true;
	} else if (strstr(buf, SSP_DEBUG_ON)) {
		ssp_dbg = true;
		ssp_pkt_dbg = true;
	} else if (strstr(buf, SSP_DEBUG_OFF)) {
		ssp_dbg = false;
		ssp_pkt_dbg = false;
	} else if (strstr(buf, SSI_DEBUG_ON)) {
		ssi_dbg = true;
	} else if (strstr(buf, SSI_DEBUG_OFF)) {
		ssi_dbg = false;
	} else if (strstr(buf, BBD_CTRL_SHMD_ON)) {
                gpbbd_dev->shmd = true;
		bSetup = true;
	} else if (strstr(buf, BBD_CTRL_SHMD_OFF)) {
                gpbbd_dev->shmd = false;
		bSetup = true;
	} else if (strstr(buf, BBD_CTRL_SPI)) {
                bbd_tty_close();
                gpbbd_dev->sio_type = BBD_SERIAL_SPI;
		bSetup = true;
	} else if (strstr(buf, BBD_CTRL_TTY)) {
                gpbbd_dev->sio_type = BBD_SERIAL_TTY;
		bSetup = true;
                bbd_tty_open();
	} else if (BbdBridge_SetControlMessage(&pEng->bridge, buf)) {
                return len;
        } else if (ps->callbacks && ps->callbacks->on_control) {
                /* Tell SHMD about the unknown control string */
                ps->callbacks->on_control(ps->priv_data, buf);
	}

	if (bSetup) {
                BbdEngine_SetUp(pEng);
        }
        else            /* No need to send it to the SHMD */
        {
            const char* msg = "NO CB";
            if (ps->callbacks)
                msg = (ps->callbacks->on_mcu_ready) ? "rdy cb" : "no cb";
            dprint("%s() %s RDY %d   DBG %d = %d\n", __func__, msg,
                        p->esw_ready, bbd_debug(), ret);
            /* REVIEW: does LHD care if SHMD received this message? */
            ret = len;
        }

	return ret;
}

/**
 * tx control string to LHD
 * 
 * There are several paths for control strings:
 *
 * SHMD-->BBD_CONTROL-->LHD "need reset"
 * SHMD<--BBD_CONTROL<--LHD (TBD)
 *        BBD_CONTROL<--LHD ESW, BBD, and PL control
 *        BBD_CONTROL-->LHD BBD and PL status and errors
 */
unsigned int bbd_control_poll(struct file *filp, poll_table * wait)
{
    return bbd_poll(BBD_MINOR_CONTROL, filp, wait);
}
