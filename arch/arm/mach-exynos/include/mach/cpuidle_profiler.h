/*
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CPUIDLE_PROFILE_H
#define CPUIDLE_PROFILE_H __FILE__

#include <linux/ktime.h>
#include <asm/cputype.h>

enum cpuidle_profile_state {
	CPUIDLE_PROFILE_C1,
	CPUIDLE_PROFILE_C2,
	CPUIDLE_PROFILE_CPD,
	NUM_STATE,
};

#define NUM_CLUSTER	2

struct cpuidle_profile_info {
	unsigned int entry_count[NUM_STATE];
	unsigned int early_wakeup_count[NUM_STATE];
	bool entered;
	unsigned int cur_state;

	ktime_t entry_time;
	unsigned long long residency[NUM_STATE];
};

extern void cpuidle_profile_start(unsigned int state, unsigned int cpuid);
extern void cpuidle_profile_finish(unsigned int state, unsigned int cpuid,
					bool early_wakeup);

#endif /* CPUIDLE_PROFILE_H */
