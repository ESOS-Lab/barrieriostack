/****************************************************************************** 
 ** \file bbd_engine.c  Engine to control the BBD bridge
 *
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

#include "../bbd_internal.h"
#include "bbd_engine.h"
#include "bbd_bridge_c.h"
#include "../utils/stream_codec_c.h"
#include "rpc_codec_c.h"
#include "../utils/bbd_utils.h"
#include "../bbd_ifc.h"

#include <linux/workqueue.h>

void BbdEngine_callback(struct sITransportLayerCb *p, struct BbdEngine *eng);
void BbdEngine_SetUp(struct BbdEngine *p);

ssize_t bbd_send_packet(unsigned char *pbuff, size_t size);

//----------------------------------------------------------------------------
//
//      BBD Engine
//
//----------------------------------------------------------------------------

bool BbdEngine_OnControlMessageToSend(void* p, const char* pcMsg)
{
    ssize_t len = strlen(pcMsg) + 1;
    ssize_t res = bbd_on_read(BBD_MINOR_CONTROL, (unsigned char*)pcMsg, len);
    return len == res;
}

// The synchronization with the remote side is complete, you can start using the
// RpcEngine to communicate with the remote side
void BbdEngine_OnRemoteSyncComplete(void* p)
{
    BbdEngine_OnControlMessageToSend(p, BBD_CTRL_PL_REMOTE_SYNC_COMPLETE);
}

// GetStats() is needed when the PL is pushed out of the TL into the BBD.
bool BbdEngine_GetStats(void* p, struct stTransportLayerStats *rStats)
{
    return false;
}

/* Could also use the jiq, but I don't see how to delay the
 * scheduling of it.  So use a fully-featured work queue.
 */
static struct workqueue_struct *workq; /*  = 0; ** Linux code standard: must assume static is cleared to zero */
static struct delayed_work      bbd_tick_work;

static void BbdEngine_Tick(struct work_struct *work)
{
    printk(KERN_INFO"%s()\n", __func__);
    gpbbd_dev->bbd_engine.tickWorking = false;
    bbd_on_read(BBD_MINOR_CONTROL, BBD_CTRL_TICK, sizeof(BBD_CTRL_TICK));
}

static void BbdEngine_StartWork(struct BbdEngine* p)
{
    if (p && p->constructed && !workq) {
//        printk(KERN_INFO "BBD:%s()/%d\n", __FUNCTION__, __LINE__);
        p->tickWorking = false;
        workq = create_singlethread_workqueue("BbdTick");
        INIT_DELAYED_WORK(&bbd_tick_work, BbdEngine_Tick);
    }
}

static void BbdEngine_StopWork(struct BbdEngine* p)
{
//    int                      canceled_work = cancel_delayed_work(&bbd_tick_work);
    const char              *workq_message = "No";
//    struct workqueue_struct *prev_workq    = workq;
//    printk(KERN_INFO "BBD:%s()/%d cancel_delayed_work(%p) %s\n",
//                    __FUNCTION__, __LINE__, &bbd_tick_work,
//                                   (canceled_work) ? "previously canceled" : "uncertain");

    cancel_delayed_work(&bbd_tick_work);

    if (workq) {
        flush_workqueue(workq);
        workq_message = "Destroyed";
        destroy_workqueue(workq);
        workq = 0;
    }
//    printk(KERN_INFO "BBD:%s()/%d %s workq %p\n",
//                    __FUNCTION__, __LINE__, workq_message, prev_workq);
}

void BbdEngine_Close(struct BbdEngine* p)
{
    if (p && p->constructed && p->open) {
        FUNC();
        BbdEngine_Lock(__LINE__);	//should lock BBD here. Otherwise it may crash while handling packet.
        BbdBridge_dtor(&p->bridge);
        BbdEngine_Unlock();
        BbdEngine_StopWork(p);
		
		p->open = false;
    }
}

extern int spi_suspending(void);
struct BbdEngine* BbdEngine_Open(struct BbdEngine* p)
{
    //SPI suspending means system going to sleep or shutdown.
    //We don't want to open BBD in these case.
    if (spi_suspending())
        return NULL;
    if (p && p->constructed && !cmpxchg(&p->open, false, true)) {
        FUNC();
        BbdEngine_StartWork(p);
        BbdBridge_BbdBridge(&p->bridge,  &p->callback);
        BbdEngine_SetUp(p);
    }
    return p;
}

struct BbdEngine* BbdEngine_BbdEngine(struct BbdEngine* p)
{
    if (p && !p->constructed) {
        FUNC();
        mutex_init(&p->lock);
        workq = 0;
        p->constructed = true;
    }
    return p;
}

void BbdEngine_dtor(struct BbdEngine* p)
{
    if (p && p->constructed) {
        BbdEngine_Close(p);
        p->constructed = false;
    }
}

#define       LOCK_FEATURE
#undef  DEBUG_LOCK_FEATURE

bool BbdEngine_Lock(int from)
{
#ifdef LOCK_FEATURE
#ifdef DEBUG_LOCK_FEATURE
    FUNI(from);
    if (gpbbd_dev && gpbbd_dev->bbd_engine.constructed) {
        if (mutex_is_locked(&gpbbd_dev->bbd_engine.lock))
            FUNS("locked!");
    }
    else
        FUNS("uninit");
#endif
    if (gpbbd_dev && gpbbd_dev->bbd_engine.constructed) {
        mutex_lock(&gpbbd_dev->bbd_engine.lock);
#ifdef DEBUG_LOCK_FEATURE
            FUNS("locked{...");
#endif
        return true;
    }
    return false;
#else
    return (gpbbd_dev && gpbbd_dev->bbd_engine.constructed);
#endif
}

void BbdEngine_Unlock(void)
{
#ifdef LOCK_FEATURE
    if (gpbbd_dev) {//  && gpbbd_dev->bbd_engine.constructed)
#ifdef DEBUG_LOCK_FEATURE
        FUNS("...} unlocked");
#endif
        mutex_unlock(&gpbbd_dev->bbd_engine.lock);
    }
#endif
}

ssize_t BbdEngine_SendSensorData(struct BbdEngine* p, unsigned char *pbuff, size_t size)
{
    if (BbdBridge_SendSensorData(&p->bridge, pbuff, size))
        return size;
    return -EINVAL;
}

bool BbdEngine_AddRpc(unsigned short usRpcId, struct sStreamCodec *rRpcStream);

/* send sensor data to the SHMD */
extern ssize_t bbd_sensor_write_internal(const char *sensorData, size_t size);


bool SensorRpc_ProcessRpc(void* p, unsigned short usRpcId,
                        struct sStreamDecoder *str)
{
    if (usRpcId == RPC_DEFINITION(IRpcSensorResponse, Data))
    {
        unsigned short size       = StreamDecoder_GetU16(str);
        unsigned char* sensorData = StreamDecoder_GetBuffer(str, size);
	ssize_t result = bbd_sensor_write_internal(sensorData , size);
        return ((short) result == size);
    }
    return false;
}

void BbdEngine_SetUp(struct BbdEngine *p)
{
    p->m_rpcSensor.cb      = SensorRpc_ProcessRpc;
    p->m_rpcSensor.data    = p;
    p->m_rpcSensor.m_pNext = 0;

    BbdBridge_RegisterRpcDecoder(&p->bridge,
                (gpbbd_dev->shmd) ? &p->m_rpcSensor : 0);
    BbdEngine_callback(&p->callback, p);
}

/*------------------------------------------------------------------------------
 *
 *      BbdEngineCb{}
 *
 *------------------------------------------------------------------------------
 */

static struct BbdEngine *GetEngine(struct sITransportLayerCb *pp)
{
    return (pp) ? ((struct BbdEngine *) pp->m_callbackData) : 0;
}


void BbdTransportCB_OnTimerSet(struct sITransportLayerCb *pp, long lTimerMs)
{
    if (gpbbd_dev->bbd_engine.tickWorking) {
        cancel_delayed_work(&bbd_tick_work); 
        gpbbd_dev->bbd_engine.tickWorking = false;
    }
    if (lTimerMs > 0) {
        unsigned long delay_jiffies = msecs_to_jiffies(lTimerMs);
        gpbbd_dev->jiffies_to_wake = jiffies + delay_jiffies;
        FUNI(lTimerMs);
        gpbbd_dev->bbd_engine.tickWorking = true;
        queue_delayed_work(workq, &bbd_tick_work, delay_jiffies);
    }
    else {
        FUNC();
        gpbbd_dev->jiffies_to_wake = 0;
    }
}

/*------------------------------------------------------------------------------
 *
 *      BbdEngine_OnTimer()
 *
 *   Call here periodically to see if the timer should expire.
 *
 *   To avoid a separate thread, send nonsense message "BBD:tick" to
 *   waken the LHD via bbd_control().  Then bbd_read() will call
 *   BbdEngine_OnTimer().
 *
 *------------------------------------------------------------------------------
 */

unsigned long BbdEngine_OnTimer(void)
{
    struct bbd_device *p = gpbbd_dev;
    unsigned long result = 0;
    if (!workq || mutex_is_locked(&p->bbd_engine.lock))
        return 0;

    if (BbdEngine_Lock(__LINE__)) {
        if (p->jiffies_to_wake)
        {
            if (p->jiffies_to_wake > jiffies) {
                result = p->jiffies_to_wake - jiffies;
            }
            else {
                p->jiffies_to_wake = 0;
                FUNC();
                TransportLayer_Tick(&p->bbd_engine.bridge.m_otTL);
            }
        }
        BbdEngine_Unlock();
    }
    return result;
}

unsigned long BbdTransportCB_OnTimeMsRequest(struct sITransportLayerCb *pp)
{
    return jiffies_to_msecs(jiffies);
}


/*------------------------------------------------------------------------------
 *
 *      Callbacks
 *
 * Connect BBD output to the real:
 *
 *  - TTY - bbd_tty.c
 *  - SSI-SPI - TODO: add code here to connect to the SPI driver:
 *
 *------------------------------------------------------------------------------
 */

extern int bbd_tty_send(unsigned char *buf, int count);
extern int bbd_ssi_spi_send(unsigned char *buf, int count);

void BbdTransportCB_OnDataToSend_TTY(struct sITransportLayerCb *pp,
            unsigned char *data, unsigned long len)
{
        bbd_tty_send(data, len);
}

void BbdTransportCB_OnDataToSend_SSI_SPI(struct sITransportLayerCb *pp,
            unsigned char *data, unsigned long len)
{
        bbd_ssi_spi_send(data, len);
}

void BbdTransportCB_OnReliableAck(struct sITransportLayerCb *pp,
                struct sBbdReliableTransaction* trans)
{
        bbd_on_read(BBD_MINOR_RELIABLE, (unsigned char*) trans, sizeof(*trans));
}

bool BbdTransportCB_OnControlMessageToSend(struct sITransportLayerCb *pp,
                char* pcMsg)
{
    return BbdEngine_OnControlMessageToSend(pp, pcMsg);
}

void BbdTransportCB_OnCommunicationError(struct sITransportLayerCb *pp)
{
    /* struct BbdEngine *eng = GetEngine(pp); */
    /* FUNC(); */
    bbd_on_read(BBD_MINOR_CONTROL, 
               BBD_CTRL_PL_ON_COMMUNICATION_ERROR,
        sizeof(BBD_CTRL_PL_ON_COMMUNICATION_ERROR));
}


void BbdTransportCB_OnPacketReceived(struct sITransportLayerCb *pp,
                unsigned char *pucData, unsigned short usSize)
{
    struct BbdEngine *eng = GetEngine(pp);
    FUNI(usSize);
    if (eng)
        BbdBridge_OnPacketReceived(&eng->bridge.m_otTL, pucData, usSize);
}

/* an internal error occured */
void BbdTransportCB_OnException(struct sITransportLayerCb *pp,
        const char filename[], unsigned int uiLine)
{
    // struct BbdEngine *eng = GetEngine(pp);
    char  buf[80] = {0};
    char* prefix = "";
    int  len = strlen(filename);

    FUNSI(filename, uiLine);
    if (len > 30)
    {
        prefix = "...";
        filename += len - 30;
    }
    snprintf(buf, sizeof(buf)-1, BBD_CTRL_PL_ON_EXCEPTION
                "%s%s/%d", prefix, filename, uiLine);
    BbdEngine_OnControlMessageToSend(pp, buf);
}

void BbdTransportCB_OnRemoteSyncComplete(struct sITransportLayerCb *pp)
{
    // struct BbdEngine *eng = GetEngine(pp);
    FUNC();
    BbdTransportCB_OnControlMessageToSend(pp, BBD_CTRL_PL_REMOTE_SYNC_COMPLETE);
    //if (eng)
    //    eng->callback->OnRemoteSyncComplete(eng->callback);
}

#if 0

bool BbdTransportCB_GetStats(struct sITransportLayerCb * pp, struct stTransportLayerStats *rStats)
{
    struct BbdEngine *eng = GetEngine(pp);
    if (eng)
        return eng->callback->GetStats(eng->callback, rStats);
}

#endif

void BbdTransportCB_OnDataToLhd(struct sITransportLayerCb* pp,
                const unsigned char *data, unsigned short size)
{
    // struct BbdEngine *eng = GetEngine(pp);
    FUNI(size);
    bbd_on_read(BBD_MINOR_PACKET, data, size);
}

/*------------------------------------------------------------------------------
 *
 *      Callbacks
 *
 *------------------------------------------------------------------------------
 */

void BbdEngine_callback(struct sITransportLayerCb *p, struct BbdEngine *eng)
{
    p->m_callbackData       = eng;
    p->OnTimerSet           = BbdTransportCB_OnTimerSet;
    p->OnTimeMsRequest      = BbdTransportCB_OnTimeMsRequest;
    p->OnCommunicationError = BbdTransportCB_OnCommunicationError;
    p->OnPacketReceived     = BbdTransportCB_OnPacketReceived;
    p->OnException          = BbdTransportCB_OnException;
    p->OnRemoteSyncComplete = BbdTransportCB_OnRemoteSyncComplete;
    p->OnReliableAck        = BbdTransportCB_OnReliableAck;
    p->OnDataToLhd          = BbdTransportCB_OnDataToLhd;

    p->OnDataToSend         = BbdTransportCB_OnDataToSend_SSI_SPI;
    switch(gpbbd_dev->sio_type)
    {
    case BBD_SERIAL_TTY:
        p->OnDataToSend = BbdTransportCB_OnDataToSend_TTY;
        break;
    case BBD_SERIAL_SPI:
        p->OnDataToSend = BbdTransportCB_OnDataToSend_SSI_SPI;
        break;
    default:
        p->OnDataToSend = BbdTransportCB_OnDataToSend_SSI_SPI;
        break;
    }
}
