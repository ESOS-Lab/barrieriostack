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

#ifndef BBD_UTILS_H_ /* { */
#define BBD_UTILS_H_

#include <linux/types.h>

#include <linux/kernel.h>
#include <linux/string.h>

#if 0
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/list.h>

#include <linux/kernel.h>       /* printk()             */
#include <linux/slab.h>         /* kmalloc()            */
#include <linux/fs.h>           /* everything           */
#include <linux/errno.h>        /* error codes          */
#include <linux/types.h>        /* size_t               */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>        /* O_ACCMODE            */
#include <linux/sysfs.h>
#include <linux/cdev.h>
#include <linux/workqueue.h>  
#include <linux/semaphore.h>

#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/poll.h> 
#endif

#define ASSERT_COMPILE(test_) \
    int __static_assert(int assert[(test_) ? 1 : -1])

#ifndef _DIM
/** Array dimension
*/
#define _DIM(x) ((unsigned int)(sizeof(x)/sizeof(*(x))))
#endif

#if 0   /* { */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
    // #ifndef __BOOL_DEFINED
        typedef int _Bool;
    // #endif
#else
    #include <stdbool.h>
#endif

/**
* Define the Boolean macros only if they are not already defined.
*/
#ifndef __cplusplus
#ifndef __bool_true_false_are_defined
    #define bool _Bool
    #define false 0 
    #define true 1
    #define __bool_true_false_are_defined 1
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* } */

#endif /* } BBD_UTILS_H_ */
