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

#ifndef __BBD_DEBUG_H__ // {
#define __BBD_DEBUG_H__

#define xBBD_DEBUG	1

bool bbd_debug(void);

#ifdef BBD_DEBUG   // }
#ifdef TDD
#define dprint(fmt, args...) \
		if(db) printf("BBD:(%s): " fmt, __func__, ##args)
#else

#ifndef KERN_INFO
#define KERN_INFO "<i>"
#endif

#ifdef __cplusplus
extern "C" {
#endif
int printk(const char *fmt, ...);
#ifdef __cplusplus
}
#endif

#define dprint(fmt, args...) \
		if(bbd_debug()) printk(KERN_INFO "BBD::%s()/%d " fmt, \
                                __func__, __LINE__, ##args)
#endif

#define FUNC()     if (bbd_debug()) printk(KERN_INFO "BBD:%s()/%d\n",           __FUNCTION__,       __LINE__)
#define FUNS(s)    if (bbd_debug()) printk(KERN_INFO "BBD:%s(\"%s\")/%d\n",     __FUNCTION__, s,    __LINE__)
#define FUNI(i)    if (bbd_debug()) printk(KERN_INFO "BBD:%s(%lu)/%d\n",        __FUNCTION__, (unsigned long)   i, __LINE__)
#define FUNSI(s,i) if (bbd_debug()) printk(KERN_INFO "BBD:%s(\"%s\",%lu)/%d\n", __FUNCTION__, s, (unsigned long) i, __LINE__)

#else   // } else {

#define dprint(format, args...)
#define FUNC()     
#define FUNS(s)    
#define FUNI(i)   
#define FUNSI(s,i) 

#endif  // } BBD_DEBUG


void bbd_log_hex(const char*          pIntroduction,
                 const unsigned char* pData,
                 unsigned long        ulDataLen);


#endif // } __BBD_DEBUG_H__
