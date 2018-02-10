/*
 *  Copyright (C) 2013, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */


#include	"sec_filter.h"
#include	"tcp_track.h"
#include	"url_parser.h"


static  dev_t               url_ver;
static  struct  cdev        url_cdev;
static  struct  class       *url_class  = NULL;

static  DECLARE_WAIT_QUEUE_HEAD(user_noti_Q);
static  tcp_Info_Manager    *getting_TrackInfo      = NULL;
static  tcp_Info_Manager    *notifying_TrackInfo    = NULL;
static  tcp_Info_Manager    *notified_TrackInfo     = NULL;
static  tcp_Info_Manager    *rejected_TrackInfo     = NULL;
static  char                *exceptionURL           = NULL;
static  atomic_t            totalReference          = ATOMIC_INIT(0);
static  unsigned long       resetTime               = 0;
static  int                 closingTime             = 60;

char    *errorMsg   = NULL;
int     errMsgSize  = 0;
int     filterMode  = FILTER_MODE_OFF;



//  sec_filter driver open function
// When url device file is opend, ths is called.
int sec_url_filter_open(struct inode *inode, struct file *filp)
{
    atomic_inc(&totalReference);    //atomic increase of reference count
    return 0;
}

//  sec_filter driver release function
//  When url device file is closed, this is called
int sec_url_filter_release( struct inode *inode, struct file *filp)
{
    if (atomic_dec_and_test(&totalReference))                   // atomic decrease and check it is zero or not
    {                                                           // if so
        if (filterMode != FILTER_MODE_OFF)                      // Check filter mode is on or off and if it is not off it means app. is not finished properly
        {
            resetTime   = get_jiffies_64()+closingTime*HZ;      // Get current time and add some time to wait for revival of app.
            filterMode  = FILTER_MODE_CLOSING;                  // Change the status of filter mode
        }
        wake_up_interruptible(&user_noti_Q);                    // Wake up the sleeping process
        clean_Managers();

    }
    printk(KERN_INFO "SEC_URL_FILTER : FILE HANDLE IS %d %d\r\n", atomic_read(&totalReference), filterMode);
    return  0;
}

//  sec_filter driver write function
ssize_t sec_url_filter_write( struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    int	result = -EIO;
    if ((buf != NULL) && (count >4))
    {                                                                       // ---------------------------------------
        short   version = *(short *)buf;                                    //|Version | Command | Data               |
        int     cmd     = *(int *)(buf+sizeof(short));                      //|---------------------------------------|
        char    *data   = (char *)(buf+sizeof(short)+sizeof(int));          //|2bytes  | 4bytes  |   Various          |
        {                                                                   // ---------------------------------------
            if (version == 0)
            {
                switch(cmd)
                {
                    case SET_FILTER_MODE:                                   // Turn On and Off filtering.
                    {
                        filterMode    = *(int *)data;                       // First 4 bytes of data will be filter mode
                        result      = count;
                        if (filterMode == FILTER_MODE_OFF)                  // If Filter mode is off
                        {
                            wake_up_interruptible(&user_noti_Q);            // Wake up sleeping read process
                            clean_Managers();                               // Clean internal resources
                        }
                        printk(KERN_INFO "SEC_URL_Filter : Mode is %d\n", filterMode);
                    }
                    break;

                    case SET_USER_SELECT:                                   // Receive choice of App. version 2, id 4, choice 2
                    {
                        tcp_TrackInfo           *selectInfo = NULL;
                        int                     id          = *((int *)(data));                             // First 4 bytes of data will be ID
                        int                     choice      = *((int *)(data+sizeof(unsigned int)));        // Next 4 bytes of data will be choice of App. for the ID
                        unsigned int            verdict     = NF_DROP;
                        struct nf_queue_entry   *entry      = NULL;

                        selectInfo	= find_tcp_TrackInfo_withID(notified_TrackInfo, id);                    // Find track info by ID
                        if (selectInfo != NULL)                                                     // There is track info.
                        {
                            result  = count;
                            entry   = (struct nf_queue_entry *)selectInfo->q_entry;

                            if (choice == ALLOW)                                                // The packet of this ID will be accepted
                            {
                                verdict = NF_ACCEPT;                                            // Change verdict as accept
                                free_tcp_TrackInfo(selectInfo);                                 // Free this track info.
                            }
                            else
                            {
                                add_tcp_TrackInfo(rejected_TrackInfo, selectInfo);              // Add this track info into Rejected List.
                                selectInfo->status  = choice;                                   // Set the choice into track info.
                                selectInfo->q_entry = NULL;

                                if ((filterMode%10) == FILTER_MODE_ON_RESPONSE)                 // If filter mode is response mode, packet should be sent in any case
                                {
                                    verdict  = NF_ACCEPT;                                       // The receiving content would be changed
                                }
                            }
                            nf_reinject(entry, verdict);                                        // Re-inject packet with the verdict
                        }
                        else
                        {
                            printk(KERN_ALERT "SEC_URL_FILTER : NO SUCH ID\n");
                        }
                    }
                    break;

                    case    SET_EXCEPTION_URL:                                      // Set Exception URL
                    {
                        int urlLen  = *((int *)(data));                             // First 4 bytes of data will be length of exception URL
                        SEC_FREE(exceptionURL);                                     // Free previous exception URL
                        exceptionURL    = SEC_MALLOC(urlLen+1);                     // Allocate new memory for new URL
                        if (exceptionURL != NULL)                                   // If memory is allocated properly
                        {
                            memcpy(exceptionURL, (data+sizeof(int)), urlLen);       // Copy exception URL
                            exceptionURL[urlLen]    = '\0';
                            result                  = count;
                        }
                    }
                    break;

                    case    SET_ERROR_MSG:                                          // Set Error message for response mode
                    {
                        int msgLen  = *((int *)(data));                             // First 4 bytes of data will be length of response message
                        SEC_FREE(errorMsg);                                         // Free previous response message
                        errMsgSize  = 0;                                            // Set error message size to 0
                        errorMsg    = SEC_MALLOC(msgLen+1);                         // Allocate new memory for new message
                        if (errorMsg != NULL)                                       // If memory is allocated properly
                        {
                            memcpy(errorMsg, (data+sizeof(int)), msgLen);           // Copy response message
                            errMsgSize = msgLen;
                            errorMsg[msgLen]    = '\0';
                            result              = count;
                        }
                    }
                    break;

                    case    SET_CLOSING_TIME:                                       // Set closing time. Closing time is the duration of waiting for revival of a dead App.
		            {
                        int	timedata = *((int *)(data));                            // Data will be waiting duration
                        if ((timedata > 0) && (timedata <60))                       // It should be within 1 min.
                        {
                            closingTime = *(int *)data;                             // Set closing time
                            printk("SEC_URL_FILTER : TIMER IS %d\n", closingTime);
                        }
                        result = count;
                    }
                    break;

                }
            }
        }
    }
    return result;
}

// sec filter read function
// Read fucntion is used to notify URL to App.
ssize_t	sec_url_filter_read( struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    int             result      = -EIO;
    tcp_TrackInfo   *notifyInfo = NULL;
    int             leftLen     = 0;
    int             writeCount  = count;
    int             loopCount   = 0;
    if (buf != NULL)
    {
        while ((filterMode > 0) &&(loopCount<3))                                            // If filter mode is normal mode and there is less meaningless wake up
        {
            unsigned long   flags = 0;

            SEC_spin_lock_irqsave(&notifying_TrackInfo->tcp_info_lock, flags);              // Enter critical section
            notifyInfo = notifying_TrackInfo->head;                                         // Get first track info from notifying list
            SEC_spin_unlock_irqrestore(&notifying_TrackInfo->tcp_info_lock, flags);         // Leave critical section

            if (notifyInfo != NULL)                                                         // If there is track info to notify
            {
                result  =   access_ok(VERIFY_WRITE, (void *)buf, count);	                // Buffer should be verified because it is old buffer
                if (result != 0)                                                            // If buffer is accessible
                {
                    leftLen = notifyInfo->urlLen - notifyInfo->notify_Index;                // Get left length to notify
                    if ( leftLen <= count)                                                  // If buffer is enough, it would be last block
                    {
                        writeCount  = leftLen;
                    }

                    result  = copy_to_user(buf, &(notifyInfo->url[notifyInfo->notify_Index]), writeCount);  // Check the result because it can be failed

                    if (result == 0)                                                                        // Check write is success
                    {
                        result  = writeCount;                                                               // Set proper result
                        notifyInfo->notify_Index += writeCount;                                             // Increase index to notify as many as writing
                        if (notifyInfo->urlLen == notifyInfo->notify_Index)                                 // If Write full data
                        {
                            notifyInfo->status = NOTIFIED;                                                  // Change Status
                            notifyInfo = getFirst_tcp_TrackInfo(notifying_TrackInfo);                       // Remove current track info from notifying list
                            add_tcp_TrackInfo(notified_TrackInfo, notifyInfo);                              // Move current track info to notified list
                        }
                    }
                }
                break;
            }
            else                                                    // If there is nothing to notify
            {
                interruptible_sleep_on(&user_noti_Q);               // Go into sleep
                loopCount++;                                        // Increase wake up count
            }
        }
    }
    return (ssize_t)result;
}


// This function is hooking function for sending packet
unsigned int sec_url_filter_hook( unsigned int hook_no,
     struct sk_buff *skb,
     const struct net_device *dev_in,
     const struct net_device *dev_out,
     int (*handler)(struct sk_buff *))
{
    unsigned int    verdict = NF_ACCEPT;
    if (filterMode)                                                                             // If filter is not off
    {
        if (skb != NULL)
        {
            struct iphdr *iph = (struct iphdr*) ip_hdr(skb);                                    // Get IP header
            if (iph != NULL)
            {
                if (iph->protocol == 6)	// TCP case                                             // Check it is TCP
                {
                    int             delFlag     = 0;
                    struct tcphdr   *tcph       = (struct tcphdr *)skb_transport_header(skb);   // Get TCP header
                    tcp_TrackInfo   *rejected   = NULL;

                    if (filterMode == FILTER_MODE_CLOSING)                                      // If it is closing mode
                    {
                    	if (time_before((unsigned long)get_jiffies_64(), resetTime))            // Check it is still waiting time
                    	{
                    	    verdict    = NF_DROP;                                               // If so, drop every TCP packet
                    	}
                    	else
                    	{
                    	    filterMode = FILTER_MODE_OFF;                                       // If it is more than wait time, turn off filter automatically.
                    	}
                        return verdict;
                    }

                    if (tcph!= NULL)                                                            // From TCP header
                    {
                        delFlag = (tcph->fin || tcph->rst);                                     // Check this TCP session will be disconnected or not
                    }

                    rejected    = find_tcp_TrackInfo(rejected_TrackInfo, skb, delFlag);         // If this TCP session would be disconnected, remove this track info from rejected list
                    if (rejected != NULL)                                                       // This is rejected packet
                    {
                        verdict = NF_DROP;                                                      // Set it drop
                    }
                    else
                    {
                        verdict = NF_QUEUE_NR(ALLOWED_ID);                                      // Packet can be marked with ALLOWED_ID
                    }

                    if (delFlag == 1)                                                           // If this TCP session will be disconnected
                    {
                        tcp_TrackInfo   *gettingNode    = NULL;
                        verdict = NF_ACCEPT;                                                    // FIN, RST should go out.
                        free_tcp_TrackInfo(rejected);                                           // Free related track info.
                        gettingNode = find_tcp_TrackInfo(getting_TrackInfo, skb, 1);            // Find previous TCP Track Info
                        free_tcp_TrackInfo(gettingNode);                                        // Free related track info.
                    }
                }
            }
        }
    }
    return verdict;
}


// This function is slow function to avoid blocking in hook function
int sec_url_filter_slow(struct nf_queue_entry *entry, unsigned int queuenum)
{
#ifdef CONFIG_SEC_NET_FILTER
    if (queuenum == ALLOWED_ID)
    {
#endif
    if (entry != NULL)
    {
        if (filterMode)                                                                         //  If filter is not turned off
        {
            struct sk_buff  *skb            = entry->skb;
            char            *request        = NULL;
            tcp_TrackInfo   *gettingNode    = NULL;
            tcp_TrackInfo   *notifyNode     = NULL;

            if (skb != NULL)
            {
                struct iphdr *iph   = (struct iphdr*)ip_hdr(skb);                                           // Get IP header
                if (iph != NULL)
                {
                    if (iph->protocol == 6)                                                                 // If it is TCP
                    {
                        struct tcphdr   *tcph = (struct tcphdr *)skb_transport_header(skb);                 // Get TCP header
                        if (tcph!= NULL)
                        {
                            notifyNode  = find_tcp_TrackInfo(notifying_TrackInfo, skb, 0);
                            if (notifyNode != NULL)                                                         // If this is already being notified to user
                            {
                                nf_reinject(entry, NF_DROP);                                                // Drop this packet
                                return 0;
                            }
                            gettingNode	= find_tcp_TrackInfo(getting_TrackInfo, skb, 1);                    // Find previous TCP Track Info and remove from list
                            if (gettingNode == NULL)                                                        // If there is no previous track info
                            {
                                gettingNode = isURL(skb);                                                   // If this is new URL request then make new TCP track info
                            }
                            if (gettingNode != NULL)                                                        // If this packet need to be analyzed
                            {
                                request	= getPacketData(skb, gettingNode);                                  // Get Packet data. if there is buffered data, it will be appended in front of this packet data
                                if (request != NULL)                                                        // If there is packet data
                                {
                                    getURL(request, gettingNode);                                           // Get URL and update status
                                    kfree(request);                                                         // Free packet data
                                    request	= NULL;
                                    if (gettingNode->status == GOT_FULL_URL)		                        // If we got full URL
                                    {
                                        makeNotifyInfo(gettingNode);                                        // Make notifying information
                                        if ((exceptionURL != NULL) && (gettingNode->url !=NULL))            // Check this is exceptional URL
                                        {
                                            if (strstr(&gettingNode->url[sizeof(URL_Info)], exceptionURL) != NULL)  // This is exception URL
                                            {
                                                free_tcp_TrackInfo(gettingNode);                                    // Free this getting track info
                                                nf_reinject(entry, NF_ACCEPT);                                      // Send this packet because it is exception
                                                return 0;
                                            }
                                        }
                                        gettingNode->q_entry = entry;                                               // Keep this packet into notifying track info
                                        add_tcp_TrackInfo(notifying_TrackInfo, gettingNode);                        // Add this track info into notifying list
                                        wake_up_interruptible(&user_noti_Q);                                        // Wake Up read function to notify it
                                        return 0;
                                    }
                                }
                                add_tcp_TrackInfo_ToHead(getting_TrackInfo, gettingNode);                           // Add this track info into getting list
                            }
                        }
                    }
                }
            }
        }
        nf_reinject(entry, NF_ACCEPT);
    }
#ifdef CONFIG_SEC_NET_FILTER
    }
    else                                                        // If this is not queued by us
    {
        return nfqnl_enqueue_packet(entry,  queuenum);              // Call net-filter queue function
    }
#endif
    return 0;
}


unsigned int sec_url_filter_recv_hook( unsigned int hook_no,
     struct sk_buff *skb,
     const struct net_device *dev_in,
     const struct net_device *dev_out,
     int (*handler)(struct sk_buff *))
{
    unsigned int    verdict = NF_ACCEPT;
    if ((filterMode%10) == FILTER_MODE_ON_RESPONSE)
    {
        if (skb != NULL)
        {
            struct iphdr *iph = (struct iphdr*) ip_hdr(skb);
            if (iph != NULL)
            {
                if (iph->protocol == 6) // TCP case
                {
                    tcp_TrackInfo   *rejected   = NULL;
                    rejected = find_tcp_TrackInfo_Reverse(rejected_TrackInfo, skb); // If this is FIN packet, remove TCP Info.
                    if (rejected != NULL)   // This is Rejected
                    {
                        change_tcp_Data(skb);
                    }
                }
            }
        }
    }
    return verdict;
}

static struct nf_queue_handler sec_url_queue_handler = {
#ifdef _PF_DEFINED
    .name = SEC_MODULE_NAME,
#endif
    .outfn = sec_url_filter_slow
};

static struct nf_hook_ops sec_url_filter = {
    .hook       = sec_url_filter_hook,
    .pf         = PF_INET,
    .hooknum    = NF_INET_LOCAL_OUT,
    .priority   = NF_IP_PRI_FIRST
};


static struct nf_hook_ops sec_url_recv_filter = {
    .hook       = sec_url_filter_recv_hook,
    .pf         = PF_INET,
    .hooknum    = NF_INET_LOCAL_IN,
    .priority   = NF_IP_PRI_FIRST
};

static struct file_operations sec_url_filter_fops = {
    .owner      = THIS_MODULE,
    .read       = sec_url_filter_read,
    .write      = sec_url_filter_write,
    .open       = sec_url_filter_open,
    .release    = sec_url_filter_release
};


void    clean_Managers(void)
{
    clean_tcp_TrackInfos(getting_TrackInfo);
    clean_tcp_TrackInfos(notifying_TrackInfo);
    clean_tcp_TrackInfos(notified_TrackInfo);
    clean_tcp_TrackInfos(rejected_TrackInfo);
    errMsgSize  = 0;
    SEC_FREE(exceptionURL);
    SEC_FREE(errorMsg);
}
void    deInit_Managers(void)
{
    SEC_FREE(getting_TrackInfo);
    SEC_FREE(notifying_TrackInfo);
    SEC_FREE(notified_TrackInfo);
    SEC_FREE(rejected_TrackInfo);
    SEC_FREE(exceptionURL);
    SEC_FREE(errorMsg);
}

int	init_Managers(void)
{
    do
    {
        if ((getting_TrackInfo = create_tcp_Info_Manager()) == NULL)    break;
        if ((notifying_TrackInfo = create_tcp_Info_Manager()) == NULL)  break;
        if ((notified_TrackInfo = create_tcp_Info_Manager()) == NULL)   break;
        if ((rejected_TrackInfo = create_tcp_Info_Manager()) == NULL)   break;
        return 0;
    }while(0);
    return -1;
}


int nfilter_init (void)
{
    int     alloc_result            = -1;
    int     add_result              = -1;
    int     add_send_hook           = -1;
    struct	device	*device_result  = NULL;

    cdev_init(&url_cdev,	&sec_url_filter_fops );
    do
    {
        if (init_Managers() < 0) break;
        if ((alloc_result = alloc_chrdev_region(&url_ver, 0, 1, "url"))  < 0 ) break;
        if ((url_class = class_create(THIS_MODULE, "secfilter")) == NULL) break;
        if ((device_result = device_create( url_class, NULL, url_ver, NULL, "url" )) == NULL)   break;
        if ((add_result = cdev_add(&url_cdev, url_ver, 1)) <0) break;
        if ((add_send_hook =nf_register_hook( &sec_url_filter)) <0) break;
        if (nf_register_hook( &sec_url_recv_filter) <0) break;
#ifdef  _NF_CHECK_REGISTER_NULL
        if (nf_register_queue_handler_is_null())
        {
            pr_info("%s:%s SEC_URL_FILTER : queue handler is null!\n", __FILE__, __func__);
#endif
#ifdef  _PF_DEFINED
            nf_register_queue_handler(PF_INET, &sec_url_queue_handler);
#else
            nf_register_queue_handler(&sec_url_queue_handler);
#endif
#ifdef  _NF_CHECK_REGISTER_NULL
        }
#endif
        return 0;
    }while(0);
    deInit_Managers();
    if (add_result == 0) cdev_del( &url_cdev );
    if (device_result != NULL) device_destroy(url_class, url_ver);
    if (url_class != NULL) class_destroy(url_class);
    if (alloc_result == 0) unregister_chrdev_region(url_ver, 1);
    if (add_send_hook == 0) nf_unregister_hook(&sec_url_filter);
    printk(KERN_ALERT "SEC_URL_FILTER : FAIL TO INIT\n");
    return -1;
}

void nfilter_exit(void)
{
    filterMode = FILTER_MODE_OFF;
    cdev_del( &url_cdev );
    device_destroy( url_class, url_ver );
    class_destroy(url_class);
    unregister_chrdev_region( url_ver, 1 );
    nf_unregister_hook(&sec_url_filter);
    nf_unregister_hook(&sec_url_recv_filter);
#ifdef _PF_DEFINED
    nf_unregister_queue_handler(PF_INET, &sec_url_queue_handler);
#else
    nf_unregister_queue_handler();
#endif
    clean_Managers();
    deInit_Managers();
    printk(KERN_ALERT "SEC_URL_FILTER : DEINITED\n");
}
EXPORT_SYMBOL(sec_url_filter_slow);

late_initcall(nfilter_init);
module_exit(nfilter_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("SAMSUNG Electronics");
MODULE_DESCRIPTION("SEC FILTER DEVICE");
