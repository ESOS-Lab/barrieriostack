/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/semaphore.h>
#include <linux/time.h>
#include <net/sock.h>
#include <net/net_namespace.h>

#include "connection.h"
#include "common.h"

/* Define the initial state of the Data Available Semaphore */
#define SEM_NO_DATA_AVAILABLE 0

struct connection *connection_new(void)
{
	struct connection *conn;

	conn = kzalloc(sizeof(*conn), GFP_KERNEL);
	if (conn == NULL) {
		MCDRV_DBG_ERROR(mc_kapi, "Allocation failure");
		return NULL;
	}
	conn->sequence_magic = mcapi_unique_id();
	mutex_init(&conn->data_lock);
	sema_init(&conn->data_available_sem, SEM_NO_DATA_AVAILABLE);

	mcapi_insert_connection(conn);
	return conn;
}

void connection_cleanup(struct connection *conn)
{
	if (!conn)
		return;

	kfree_skb(conn->skb);

	mcapi_remove_connection(conn->sequence_magic);
	kfree(conn);
}

bool connection_connect(struct connection *conn, pid_t dest)
{
	/* Nothing to connect */
	conn->peer_pid = dest;
	return true;
}

size_t connection_read_data_msg(struct connection *conn, void *buffer,
				uint32_t len)
{
	size_t ret = -1;
	MCDRV_DBG_VERBOSE(mc_kapi,
			  "reading connection data %u, connection data left %u",
			  len, conn->data_len);
	/* trying to read more than the left data */
	if (len > conn->data_len) {
		ret = conn->data_len;
		memcpy(buffer, conn->data_start, conn->data_len);
		conn->data_len = 0;
	} else {
		ret = len;
		memcpy(buffer, conn->data_start, len);
		conn->data_len -= len;
		conn->data_start += len;
	}

	if (conn->data_len == 0) {
		conn->data_start = NULL;
		kfree_skb(conn->skb);
		conn->skb = NULL;
	}
	MCDRV_DBG_VERBOSE(mc_kapi, "read %zu",  ret);
	return ret;
}

size_t connection_read_datablock(struct connection *conn, void *buffer,
				 uint32_t len)
{
        /*##################################
        #mobicore_security_team_add
        ##################################*/
        return connection_read_data(conn, buffer, len, 1000);
        /* default timeout is -1, which may cause infinite waiting,
        so that phone cannot boot. */   
        /*##################################*/
}

size_t connection_read_data(struct connection *conn, void *buffer, uint32_t len,
			    int32_t timeout)
{
	size_t ret = 0;

	MCDRV_ASSERT(buffer != NULL);
	MCDRV_ASSERT(conn->socket_descriptor != NULL);

	MCDRV_DBG_VERBOSE(mc_kapi, "read data len = %u for PID = %u",
			  len, conn->sequence_magic);
	do {
		/*
		 * Wait until data is available or timeout
		 * msecs_to_jiffies(-1) -> wait forever for the sem
		 */
		if (down_timeout(&(conn->data_available_sem),
				 msecs_to_jiffies(timeout))) {
			MCDRV_DBG_VERBOSE(mc_kapi,
					  "Timeout reading the data sem");
			ret = -2;
			break;
		}

    /*##################################
    #mobicore_security_team_add
    ##################################*/
    /*if (mutex_lock_interruptible(&(conn->data_lock))) {
            MCDRV_DBG_ERROR(mc_kapi,
                            "interrupted reading the data sem");
            ret = -1;
            break;
    }*/
    mutex_lock(&(conn->data_lock));         
    /*##################################*/

		/* Have data, use it */
		if (conn->data_len > 0)
			ret = connection_read_data_msg(conn, buffer, len);

		mutex_unlock(&(conn->data_lock));

		/* There is still some data left */
		if (conn->data_len > 0)
			up(&conn->data_available_sem);

	} while (0);

	return ret;
}

size_t connection_write_data(struct connection *conn, void *buffer,
			     uint32_t len)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh;
	int ret = 0;

	MCDRV_DBG_VERBOSE(mc_kapi, "buffer length %u from pid %u\n",
			  len,  conn->sequence_magic);
	do {
		skb = nlmsg_new(NLMSG_SPACE(len), GFP_KERNEL);
		if (!skb) {
			ret = -1;
			break;
		}

		nlh = nlmsg_put(skb, 0, conn->sequence_magic, 2,
				NLMSG_LENGTH(len), NLM_F_REQUEST);
		if (!nlh) {
			ret = -1;
			kfree_skb(skb);
			break;
		}
		memcpy(NLMSG_DATA(nlh), buffer, len);

		/* netlink_unicast frees skb */
		netlink_unicast(conn->socket_descriptor, skb,
				conn->peer_pid, MSG_DONTWAIT);
		ret = len;
	} while (0);

	return ret;
}

int connection_process(struct connection *conn, struct sk_buff *skb)
{
	int ret = 0;
	do {
	  /*##################################
	  #mobicore_security_team_add
	  ##################################*/
    /*if (mutex_lock_interruptible(&(conn->data_lock))) {
            MCDRV_DBG_ERROR(mc_kapi,
                            "Interrupted getting data semaphore!");
            ret = -1;
            break;
    }*/
    mutex_lock(&(conn->data_lock));
    /*##################################*/

		kfree_skb(conn->skb);

		/* Get a reference to the incoming skb */
		conn->skb = skb_get(skb);
		if (conn->skb) {
			conn->data_msg = nlmsg_hdr(conn->skb);
			conn->data_len = NLMSG_PAYLOAD(conn->data_msg, 0);
			conn->data_start = NLMSG_DATA(conn->data_msg);
			up(&(conn->data_available_sem));
		}
		mutex_unlock(&(conn->data_lock));
		ret = 0;
	} while (0);
	return ret;
}
