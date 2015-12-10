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
 * The BBD TTY (Broadcom Bridge TTY Driver)
 *
 * tabstop = 8
 */

#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include "bbd_internal.h"
#include "transport/bbd_engine.h"
#include "transport/bbd_bridge_c.h"

void BbdEngine_Open(struct BbdEngine *p);
void BbdEngine_Close(struct BbdEngine *p);

/*------------------------------------------------------------------------------
 *
 *      internal static data
 *
 *------------------------------------------------------------------------------
 */

static struct file        *tty;
//static struct work_struct  read_work;
static bool                sent_autobaud;
static atomic_t		bbd_tty_run;
#if 0
static /* const */ unsigned char autobaud[16] =
{
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};
#endif

/*------------------------------------------------------------------------------
 *
 *      Customization
 *
 * 1)  Autobaud sequence
 * 2)  portname, speed, etc.
 * 3)  Stop condition
 *
 *------------------------------------------------------------------------------
 */
 
#define GN3
#define OPT_WORK_QUEUE
#define BBD_USE_AUTOBAUD_not

#ifdef OPT_WORK_QUEUE
static struct workqueue_struct *workq;
#endif
static struct work_struct       bbd_tty_read_work;


#ifdef GN3      /* { */

#define PORT_NAME "/dev/ttyBCM0";

static bool bbd_tty_ioctl(struct file *f, int config, unsigned long param,
                        const char* msg)
{
        return (tty_ioctl(f, config, param) >= 0);
}

#else           /* } else not GN3 { */

#define PORT_NAME "/dev/ttyO1";

static bool bbd_tty_ioctl(struct file *f, int config, unsigned long param,
                        const char* msg)
{
        int result = f->f_op->unlocked_ioctl(f, config, param);
        dprint("%s(%p,%d,0x%lx) %s = %d\n",
                __func__, f, config, param, msg, result);
        return result >= 0;
}

#endif          /* } // GN3 */


/* configure port to use a custom speed.
 *      Set the custom divisor using ioctl(TIOSSERIAL)
 *      Set the speed to 38400 to use the custom speed.
 */

static bool bbd_tty_setcustomspeed(struct file *f, int speed)
{
    bool result = true;

    struct serial_struct ss;
    int closestSpeed = 0;
    if (!bbd_tty_ioctl(f, TIOCGSERIAL, (unsigned long) &ss, "TIOCGSERIAL"))
        result = false;

    ss.flags = (ss.flags & ~ASYNC_SPD_MASK) | ASYNC_SPD_CUST;
    ss.custom_divisor = (ss.baud_base + (speed / 2)) / speed;
    closestSpeed = ss.baud_base / ss.custom_divisor;

    if (    closestSpeed < speed *  98 / 100
         || closestSpeed > speed * 102 / 100) {
            dprint("BBDTTY: Cannot set serial port speed to %d. "
                                       "Closest possible is %d\n",
                            speed, closestSpeed);
    }
    return bbd_tty_ioctl(f, TIOCSSERIAL, (unsigned long) &ss, "TIOCSSERIAL");
}






/*------------------------------------------------------------------------------
 *
 *      bbd_tty_setspeed
 *
 *------------------------------------------------------------------------------
 */

static bool bbd_tty_setspeed(struct file *f, int speed)
{
	mm_segment_t oldfs;
        bool result = true;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	{
		/*  Set speed */
		struct termios settings;

		if (!bbd_tty_ioctl(f, TCGETS, (unsigned long)&settings,
                                        "TCGETS"))
                    result = false;
		settings.c_iflag = 0;
		settings.c_oflag = 0;
		settings.c_lflag = 0;
		settings.c_cflag = CRTSCTS | CLOCAL | CS8 | CREAD;
		settings.c_cc[VMIN ] = 0;
		settings.c_cc[VTIME] = 1;	/* 100ms timeout */
		switch (speed) {
		case 2400:
			settings.c_cflag |= B2400;
			break;
		case 4800:
			settings.c_cflag |= B4800;
			break;
		case 9600:
			settings.c_cflag |= B9600;
			break;
		case 19200:
			settings.c_cflag |= B19200;
			break;
		case 38400:
			settings.c_cflag |= B38400;
			break;
		case 57600:
			settings.c_cflag |= B57600;
			break;
		case 115200:
			settings.c_cflag |= B115200;
			break;
		case 921600:
			settings.c_cflag |= B921600;
			break;
		case 1000000:
			settings.c_cflag |= B1000000;
			break;
		case 1152000:
			settings.c_cflag |= B1152000;
			break;
		case 1500000:
			settings.c_cflag |= B1500000;
			break;
		case 2000000:
			settings.c_cflag |= B2000000;
			break;
		case 2500000:
			settings.c_cflag |= B2500000;
			break;
		case 3000000:
			settings.c_cflag |= B3000000;
			break;
		case 3500000:
			settings.c_cflag |= B3500000;
			break;
		case 4000000:
			settings.c_cflag |= B4000000;
			break;
		default:
                        bbd_tty_setcustomspeed(f, speed);
                        settings.c_cflag |= B38400;
			break;
		}
		if (!bbd_tty_ioctl(f, TCSETS, (unsigned long)&settings,
                                        "TCSETS"))
                    result = false;
	}
	{
		/*  Set low latency */
		struct serial_struct settings;

		if (!bbd_tty_ioctl(f, TIOCGSERIAL, (unsigned long)&settings,
                                "TIOCGSERIAL"))
                    result = false;
		settings.flags |= ASYNC_LOW_LATENCY;
		if (!bbd_tty_ioctl(f, TIOCSSERIAL, (unsigned long)&settings,
                                "TIOCSSERIAL"))
                    result = false;
	}

	set_fs(oldfs);
        return result;
}

/*------------------------------------------------------------------------------
 *
 *      bbd_tty_close
 *
 *------------------------------------------------------------------------------
 */

static inline struct tty_struct *file_tty(struct file *file)
{
	return ((struct tty_file_private *)file->private_data)->tty;
}

int bbd_tty_close(void)
{
	if (!atomic_read(&bbd_tty_run)) {
		dprint("BBDTTY: already closed\n");
		return 0;
	}

	/* This is needed for JTAG download (without LHD) */
        BbdEngine_Close(&gpbbd_dev->bbd_engine);

        sent_autobaud = false;
	atomic_set(&bbd_tty_run, 0);

	dprint("BBDTTY: %s TTY %p  ERR %ld\n", __func__, tty, IS_ERR(tty));

#ifdef OPT_WORK_QUEUE
	if (workq) {
		struct tty_struct *tty_st = file_tty(tty);
		set_bit(TTY_OTHER_CLOSED, &tty_st->flags);
		wake_up_interruptible(&tty_st->read_wait);
		wake_up_interruptible(&tty_st->write_wait);

		cancel_work_sync(&bbd_tty_read_work);
	}
#endif
	if ((tty != 0) && !IS_ERR(tty)) {
		filp_close(tty, 0);             /* REVIEW: should cause read() to exit */
                tty = 0;
        }

#ifdef OPT_WORK_QUEUE
	if (workq) {
                dprint("BBDTTY: %s()/%d\n", __func__, __LINE__);
		destroy_workqueue(workq);
		workq = 0;
	}
#endif
	return 0;
}

/*------------------------------------------------------------------------------
 *
 *      bbd_tty_read
 *
 *------------------------------------------------------------------------------
 */

static int bbd_tty_read(struct file *f, int maxRead, unsigned char *ch)
{
	int result = -1;

	if (!IS_ERR(f)) {
		mm_segment_t oldfs = get_fs();
		set_fs(KERNEL_DS);

		f->f_pos = 0;
		result = f->f_op->read(f, ch, maxRead, &f->f_pos);
		set_fs(oldfs);
	}

	return result;
}

/*------------------------------------------------------------------------------
 *
 *      bbd_tty_write
 *
 *------------------------------------------------------------------------------
 */

static int bbd_tty_write(struct file *f, unsigned char *buf, int count)
{
	int result = -1;
	mm_segment_t oldfs;
        bbd_log_hex(__func__, buf, count);
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	f->f_pos = 0;
	result = f->f_op->write(f, buf, count, &f->f_pos);
	set_fs(oldfs);

	return result;
}

/*------------------------------------------------------------------------------
 *
 *      bbd_tty_read_func
 *
 *------------------------------------------------------------------------------
 */

static void bbd_tty_read_func(struct work_struct *work)
{
	struct BbdEngine *pEng = 0;

	while (atomic_read(&bbd_tty_run) && tty) {
                char buf[256];
                int  nRead = bbd_tty_read(tty, sizeof(buf), buf);
		if (nRead <= 0)
			continue;
		pEng = bbd_engine();

                bbd_log_hex(__func__, buf, nRead);
		if (pEng && atomic_read(&bbd_tty_run))
			BbdBridge_SetData(&pEng->bridge, buf, nRead);
		else
			dprint("BBDTTY: bbdengine %p run %d, rx ignored\n",
                                pEng, atomic_read(&bbd_tty_run));
	}
	dprint("BBDTTY: exit\n");
}

/*------------------------------------------------------------------------------
 *
 *      bbd_tty_open
 *
 *------------------------------------------------------------------------------
 */

int bbd_tty_open(void)
{
	int   result   = 0;
	char *portname = PORT_NAME;

	if (atomic_read(&bbd_tty_run)) {
		dprint("BBDTTY: already opened\n");
		return 0;
	}

	dprint("BBDTTY: open(%s)\n", portname);

	tty = filp_open(portname, O_RDWR, 0);
	if (IS_ERR(tty)) {
		result = (int)PTR_ERR(tty);
		dprint("serial_2002: open(%s) error = %d\n", 
			portname, result);
	}
	else {
		bool ok = false;
		dprint("BBDTTY: open(%s) OK\n", portname);
		ok = bbd_tty_setspeed(tty, gpbbd_dev->tty_baud);
		dprint("BBDTTY: setspeed(%d) %s\n", gpbbd_dev->tty_baud,
				ok ? "OK" : "FAIL");
                atomic_set(&bbd_tty_run, 1);
#ifdef OPT_WORK_QUEUE
		{
                    int q = 0;
                    workq = create_singlethread_workqueue("BbdTty");
                    //PREPARE_WORK(&bbd_tty_read_work, bbd_tty_read_func);
			INIT_WORK(&bbd_tty_read_work, bbd_tty_read_func);
                    q = queue_work(workq, &bbd_tty_read_work);
                    dprint("BBDTTY: BbdTty worker %s\n",
                            (q) ? "Already running" : "OK");
		}
#else
		{
			INIT_WORK(&bbd_tty_read_work, bbd_tty_read_func);
			schedule_work(&bbd_tty_read_work);
			dprint("BBDTTY: worker OK\n");
		}
#endif
		/* Needed for JTAG download of ESW without LH daemon */
        	BbdEngine_Open(&gpbbd_dev->bbd_engine);
	}
	return result;
}

/*------------------------------------------------------------------------------
 *
 *      bbd_tty_autobaud
 *
 *------------------------------------------------------------------------------
 */

int bbd_tty_autobaud(void)
{
        int res = -1;

	if (!tty || IS_ERR(tty))
		return res;

	if (!atomic_read(&bbd_tty_run))
		return res;

#ifdef BBD_USE_AUTOBAUD
        res = bbd_tty_write(tty, autobaud, sizeof(autobaud));
        sent_autobaud = (res == sizeof(autobaud));
        dprint("BBDTTY: autobaud %d res %d\n", sent_autobaud, res);
#else
        res = 0;
#endif
        return res;
}

/*------------------------------------------------------------------------------
 *
 *      bbd_tty_send
 *
 *------------------------------------------------------------------------------
 */

int bbd_tty_send(unsigned char *buf, int count)
{
        int res = -1;

	if (!tty || IS_ERR(tty))
		return res;

	if (!atomic_read(&bbd_tty_run))
		return res;

#ifdef BBD_USE_AUTOBAUD
        if (!sent_autobaud) {
                res = bbd_tty_autobaud();
        }
#endif

        res = bbd_tty_write(tty, buf, count);
        return res;
}

void __init bbd_tty_init(void)
{
	atomic_set(&bbd_tty_run, 0);
}
