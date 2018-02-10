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
#include "utils/bbd_utils.h"

#include <linux/kernel.h>       /* printk()             */
#include <linux/slab.h>         /* kmalloc()            */

#undef  DEBUG_MEMORY

bool bbd_debug(void)
{
    return ((gpbbd_dev != NULL) && gpbbd_dev->db);
}

struct BbdEngine *bbd_engine(void)
{
    return &gpbbd_dev->bbd_engine;
}

static int nAlloc;      /* Linux guarantees statics are cleared to 0 */

int bbd_alloc_count(void)
{
    return nAlloc;
}

void* bbd_alloc(size_t size)
{
    void* p = kmalloc(size, GFP_KERNEL);
    if (!p) {
	    printk("%s failed to alloc %lu\n", __func__, size);
	    return p;
    }

    memset(p, 0, size);
    ++nAlloc;
#ifdef  DEBUG_MEMORY
    dprint("bbd_alloc(%u) #%d = %p\n", size, nAlloc, p);
#endif
    return p;
}

void bbd_free(void* p)
{
#ifdef  DEBUG_MEMORY
    dprint("bbd_free() #%d = %p\n", nAlloc, p);
#endif
    --nAlloc;
    kfree(p);
}

/*---------------------------------------------------------------
 *
 *      Manage the queue of freed buffers
 *      Allow max 10 buffers freed
 *
 *---------------------------------------------------------------
 */

#define BBD_FREED_BUFFER_THRESHOLD 10

#ifdef DEBUG_MEMORY
#define MSG_INIT_FREED char* msg = "freed"
#define MSG_SET_CACHED msg = "cached"
#else
#define MSG_INIT_FREED
#define MSG_SET_CACHED
#endif

void bbd_qitem_cache_add(struct bbd_qitem *qi)
{
        struct bbd_device *p = gpbbd_dev;
        MSG_INIT_FREED;

	mutex_lock(&p->qlock);
        if (p->qcount < BBD_FREED_BUFFER_THRESHOLD) {
            ++p->qcount;
            qi->next = p->qfree;
            p->qfree = qi;
            MSG_SET_CACHED;
        }
        else {
            bbd_free(qi);
        }
	mutex_unlock(&p->qlock);

#ifdef  DEBUG_MEMORY
        dprint(KERN_INFO"%s(%p) %d %s\n", __func__, qi, p->qcount, msg);
#endif
}

struct bbd_qitem *bbd_qitem_cache_get(size_t req_size)
{
    struct bbd_qitem **q_prev = 0;
    struct bbd_qitem  *q      = 0;

    mutex_lock(&gpbbd_dev->qlock);
    q_prev = &gpbbd_dev->qfree;
    q      = *q_prev;
    while (q)
    {
        if (req_size <= q->len)
        {
            *q_prev = q->next;
            --gpbbd_dev->qcount;
            mutex_unlock(&gpbbd_dev->qlock);
            q->next = 0;
            q->lenUsed = req_size;
#ifdef  DEBUG_MEMORY
            dprint(KERN_INFO"%s(%u) reuse %p\n", __func__, req_size, q);
#endif
            return q;
        }
        q_prev = &q->next;
        q = q->next;
    }
    mutex_unlock(&gpbbd_dev->qlock);
    return 0;
}

int bbd_qitem_cache_uninit(void)
{
        int freed = 0;
	mutex_lock(&gpbbd_dev->qlock);
        freed = bbd_queue_uninit(gpbbd_dev->qfree);
	mutex_unlock(&gpbbd_dev->qlock);
        return freed;
}

/*---------------------------------------------------------------
 *
 *      Common queue and device code.
 *
 *---------------------------------------------------------------
 */

static void bbd_base_init_common(struct bbd_buffer *p)
{
	mutex_init(&p->lock);
	p->count     = 0;
	p->tout_cnt  = 0;
	init_waitqueue_head(&p->poll_inq);
	init_waitqueue_head(&p->comp_inq);
}

void bbd_base_init_vars(struct bbd_device *dev, struct bbd_base *p, int* result)
{
        if (*result)
                return;

        memset(p, 0, sizeof(*p));
	/* p->esw_ready = false; */
	/* p->rxb.buff_size = 0;    ** done by memset() */

	/* allocate memory for rx packet from LHD */
	p->rxb.pbuff = bbd_alloc(BBD_MAX_DATA_SIZE);
	if (!p->rxb.pbuff) {
		printk(KERN_ERR "%s():failed to allocate mem for rxbuff\n",
					__func__);
		*result = -ENOMEM;
	}
        /* printk("%s(%p,%p)\n", __func__, dev, p); */

        bbd_base_init_common((struct bbd_buffer *) &p->txq);
        bbd_base_init_common(                      &p->rxb);
}

int bbd_queue_uninit(struct bbd_qitem *q)
{
        int freed = 0;
        while (q)
        {
            struct bbd_qitem* pq_next = q->next;
            q->next = 0;
            bbd_free(q);
            ++freed;
            q = pq_next;
        }
        return freed;
}

#undef EXTENSIVE_REINIT

int bbd_base_reinit(int minor)
{
        struct bbd_base *p = gpbbd_dev->bbd_ptr[minor];
	if (!p) return -EFAULT;

        FUNS(p->name);
#ifdef EXTENSIVE_REINIT
        /* bbd_queue_uninit(p->txq.qhead); */
        /* p->txq.qhead     = 0; */
        /* p->txq.qtail     = 0; */
        p->rxb.buff_size = 0;
        /* bbd_base_init_common((struct bbd_buffer *) &p->txq); */
        /* bbd_base_init_common(&p->rxb); */
#endif
    return 0;
}

int bbd_base_uninit(struct bbd_base *p)
{
        int freed = bbd_queue_uninit(p->txq.qhead);
        p->txq.qhead = p->txq.qtail = 0;
	bbd_free(p->rxb.pbuff);
        dprint(KERN_INFO"%s(%s) %d freed\n", __func__, p->name, freed);
        return freed;
}

struct bbd_qitem *bbd_qitem_alloc(size_t req_size)
{
    struct bbd_qitem *qi = bbd_qitem_cache_get(req_size);
    if (!qi)
    {
        int usable_size = BBD_BUFFER_SIZE;
        if ((int) req_size > usable_size)
            usable_size = req_size;
        qi = (struct bbd_qitem *) bbd_alloc(usable_size + QITEM_OVERHEAD);
        if (qi)  {
            qi->lenUsed = req_size;
            qi->len     = usable_size;
            qi->next    = 0;
        }
    }
    return qi;
}

bool bbd_q_add(struct bbd_queue *q, const unsigned char *pbuff, size_t size)
{
        struct bbd_qitem *qi = 0;

        mutex_lock(&q->lock);

        qi = bbd_qitem_alloc(size);
        if (!qi) {
                mutex_unlock(&q->lock);
                return false;
        }
        memcpy(qi->data, pbuff, size);
        if (q->qtail) {
                q->qtail->next = qi;
        }
        else {
                q->qhead = qi;
        }
        q->qtail = qi;
        qi->next = 0;

        ++q->count;

        mutex_unlock(&q->lock);
        return true;
}

struct bbd_qitem *bbd_q_get(struct bbd_queue *q)
{
        struct bbd_qitem  *qi = 0;

        mutex_lock(&q->lock);
        qi = q->qhead;
        if (qi) {
                q->qhead = qi->next;
                if (!q->qhead) q->qtail = 0;
                qi->next = 0;
                --q->count;
        }
        mutex_unlock(&q->lock);

        return qi;
}

/*----------------------------------------------------------------------
 *
 *      bbd_on_read
 *
 *  Common code to fill a buffer to the user.
 *
 *  Note:  this code needs to handle N packets, not just one buffer-full
 *
 *----------------------------------------------------------------------
 */
#define BBD_ON_READ_Q_THRESHOLD 1024
ssize_t bbd_on_read(int minor, const unsigned char *pbuff, size_t size)
{
        struct bbd_base  *p = 0;
	struct bbd_queue *q = 0;

        if (!gpbbd_dev)
		return -EFAULT;

        p = gpbbd_dev->bbd_ptr[minor];

        /* allow only some byte-stuffing */
	if (!p || size > 2*BBD_MAX_DATA_SIZE)
		return -EFAULT;
	q = &p->txq;

	if(q->count < BBD_ON_READ_Q_THRESHOLD) {
                bbd_q_add(q, pbuff, size);
	}
        else {
                // TODO: Add error info. to sysfs
        }

	wake_up(&q->poll_inq);       /* Tell about new data */

	dprint("%s %d #%u\n", p->name, (int) size, q->count);
        BbdEngine_OnTimer();
	return size;
}

/*----------------------------------------------------------------------
 *
 *      bbd_write
 *
 *  Common code to fill a buffer from the user.
 *  Notes:
 *      This code lacks capacity for N packets.
 *      It touches the "rx" buffer, so the naming is skewed.
 *
 *----------------------------------------------------------------------
 */

ssize_t bbd_write(int minor, const char *buf, size_t size, bool fromUser)
{
        struct bbd_base *p = gpbbd_dev->bbd_ptr[minor];
	struct bbd_buffer * prxb = &p->rxb;
        bool userFail = false;
        size_t wr_size = MIN(2*BBD_MAX_DATA_SIZE, size);

        BbdEngine_OnTimer();

	if (!p || size > 2*BBD_MAX_DATA_SIZE) {
                dprint("%s [%d]\n", (p) ? p->name : "no BBD", size);
		return -EFAULT;
        }

	dprint("%s(%s.%s)[%d]\n", __func__, p->name,
                        (fromUser) ? "user" : "internal",
                        (int) size);

        if (wr_size != size) {
                dprint("%s [%d]\n", p->name, size);
		return -EFAULT;
        }

	mutex_lock(&prxb->lock);
        if (fromUser)
            userFail = copy_from_user(prxb->pbuff, buf, wr_size);
        else
            memcpy(prxb->pbuff, buf, wr_size);

        bbd_log_hex(p->name, prxb->pbuff, wr_size);

        if (!userFail) {
            prxb->buff_size = wr_size;
            prxb->count    += wr_size;
        }
	mutex_unlock(&prxb->lock);

	if (userFail)
		return -EINVAL;
	return wr_size;
}

/*----------------------------------------------------------------------
 *
 *      bbd_read
 *
 *  Common read code to fill a user buffer from an internal queue.
 *  Notes:
 *      This code lacks capacity for N packets.
 *      It touches the "rx" buffer.
 *
 *----------------------------------------------------------------------
 */

// static int nPollReport = 5;

ssize_t bbd_read(int minor, char __user *buf, size_t size, bool bUser)
{
        struct bbd_base  *p  = gpbbd_dev->bbd_ptr[minor];
	struct bbd_queue *q  = NULL;
	struct bbd_qitem *qi = NULL;

	ssize_t rd_size = 0;

	if (!p)
		return -EFAULT;

        BbdEngine_OnTimer();
	q = &p->txq;

        qi = bbd_q_get(q);
	if (!qi) {
                // if (nPollReport > 0) {
                //     --nPollReport;
                //     FUNS(p->name);
                // }
		return 0;
        }

	rd_size = MIN(qi->lenUsed, size);
	if (size < qi->lenUsed) {
            dprint("%d %s %d<%d\n", __LINE__, p->name, size, qi->lenUsed);
            rd_size = -EFAULT;
            goto err;
        }

        if (bUser) {
            if (copy_to_user(buf, (void *)qi->data, rd_size)) {
                    rd_size = -EFAULT;
                    goto err;
            }
        }
        else {
            memcpy(buf, qi->data, rd_size);
        }


	dprint("%s(%s.%s)[%d]\n", __func__, p->name,
                        (bUser) ? "user" : "internal", (int) rd_size);
        bbd_log_hex(p->name, qi->data, rd_size);

        bbd_qitem_cache_add(qi);
	wake_up(&q->comp_inq);

	return rd_size;

err:
        bbd_qitem_cache_add(qi);
	return rd_size;
}

/*      REVIEW QUESTION:
 *      Can we install bbd_poll() as the fops.poll for all minors?
 *      Is there something inside struct file *filp we can use to get
 *      the minor or the bbd_base*?
 */

unsigned int bbd_poll(int minor, struct file *filp, poll_table * wait)
{
        struct bbd_base *p = gpbbd_dev->bbd_ptr[minor];
	unsigned int mask = 0;

	if (!p)
		return -EFAULT;

        BbdEngine_OnTimer();
	if (p->txq.count > 0) {
                // if (nPollReport > 0) FUNSI(p->name, p->txq.count);
		mask |= POLLIN;          /* no need to sleep - already have data */
		return mask;
	}

	poll_wait(filp, &p->txq.poll_inq, wait);       /* wait until wake up */

	if (p->txq.count > 0) {
                // if (nPollReport > 0) FUNI(p->txq.count);
		mask |= POLLIN;
        }
	return mask;
}

/*      Return the number of packets, or -1 upon error
 */

int bbd_count(int minor)
{
        struct bbd_base *p = gpbbd_dev->bbd_ptr[minor];

	return (p) ? p->txq.count : -1;
}

void bbd_log_hex(const char*          pIntroduction,
                 const unsigned char* pData,
                 unsigned long        ulDataLen)
#if 0
{
    const unsigned char* pDataEnd = pData + ulDataLen;
    if (!bbd_debug() || !pData) return;
    if (!pIntroduction) pIntroduction = "...unknown...";

#if 0
    printk(KERN_INFO"%s(%s) [%d] %s = %02X %02X %02X %02X ...\n", __func__,
                                p->name, (int) rd_size,
                                (bUser) ? "user" : "internal",
                                qi->data[0], qi->data[1],
                                qi->data[2], qi->data[3]);
#endif

    while (pData < pDataEnd)
    {
        char buf[128];
        size_t bufsize = sizeof(buf) - 3;
        size_t lineLen = pDataEnd - pData;
        size_t perLineCount = lineLen;
        if (lineLen > 32) {
                lineLen = 32;
                perLineCount = lineLen;
        }

        snprintf(buf, bufsize, "%s [%u] { ",
                        pIntroduction, lineLen);

        for (; perLineCount > 0; ++pData, --perLineCount)
        {
            size_t len = strlen(buf);
            snprintf(buf+len, bufsize - len, "%02X ", *pData);
        }
        printk(KERN_INFO"%s}\n", buf);
    }
}
#else
{
}
#endif
