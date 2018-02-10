/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Author: Hyosang Jung
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>

#include <mach/exynos-pm.h>

static DEFINE_RWLOCK(exynos_pm_notifier_lock);
static RAW_NOTIFIER_HEAD(exynos_pm_notifier_chain);

static int exynos_pm_notify(enum exynos_pm_event event, int nr_to_call, int *nr_calls)
{
	int ret;

	ret = __raw_notifier_call_chain(&exynos_pm_notifier_chain, event, NULL,
		nr_to_call, nr_calls);

	return notifier_to_errno(ret);
}

int exynos_pm_register_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&exynos_pm_notifier_lock, flags);
	ret = raw_notifier_chain_register(&exynos_pm_notifier_chain, nb);
	write_unlock_irqrestore(&exynos_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_register_notifier);

int exynos_pm_unregister_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&exynos_pm_notifier_lock, flags);
	ret = raw_notifier_chain_unregister(&exynos_pm_notifier_chain, nb);
	write_unlock_irqrestore(&exynos_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_unregister_notifier);

#ifdef CONFIG_SEC_PM_DEBUG
static unsigned int lpa_log_en;
module_param(lpa_log_en, uint, 0644);
#endif

int exynos_lpa_prepare(void)
{
	int nr_calls = 0;
	int ret = 0;

	read_lock(&exynos_pm_notifier_lock);
	ret = exynos_pm_notify(LPA_PREPARE, -1, &nr_calls);
#ifdef CONFIG_SEC_PM_DEBUG
	if (unlikely(lpa_log_en) && ret < 0) {
		struct notifier_block *nb, *next_nb;
		struct notifier_block *nh = exynos_pm_notifier_chain.head;
		int nr_to_call = nr_calls - 1;

		nb = rcu_dereference_raw(nh);

		while (nb && nr_to_call) {
			next_nb = rcu_dereference_raw(nb->next);
			nb = next_nb;
			nr_to_call--;
		}

		if (nb)
			pr_info("%s: failed at %ps\n", __func__,
					(void *)nb->notifier_call);
	}
#endif
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_lpa_prepare);

int exynos_lpa_enter(void)
{
	int nr_calls;
	int ret = 0;

	read_lock(&exynos_pm_notifier_lock);
	ret = exynos_pm_notify(LPA_ENTER, -1, &nr_calls);
	if (ret)
		/*
		 * Inform listeners (nr_calls - 1) about failure of CPU PM
		 * PM entry who are notified earlier to prepare for it.
		 */
		exynos_pm_notify(LPA_ENTER_FAIL, nr_calls - 1, NULL);
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_lpa_enter);

int exynos_lpa_exit(void)
{
	int ret;

	read_lock(&exynos_pm_notifier_lock);
	ret = exynos_pm_notify(LPA_EXIT, -1, NULL);
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_lpa_exit);

#ifdef CONFIG_SEC_PM_DEBUG
static unsigned int lpc_log_en;
module_param(lpc_log_en, uint, 0644);
#endif

int exynos_lpc_prepare(void)
{
	int nr_calls = 0;
	int ret = 0;

	read_lock(&exynos_pm_notifier_lock);
	ret = exynos_pm_notify(LPC_PREPARE, -1, &nr_calls);
#ifdef CONFIG_SEC_PM_DEBUG
	if (unlikely(lpc_log_en) && ret < 0) {
		struct notifier_block *nb, *next_nb;
		struct notifier_block *nh = exynos_pm_notifier_chain.head;
		int nr_to_call = nr_calls - 1;

		nb = rcu_dereference_raw(nh);

		while (nb && nr_to_call) {
			next_nb = rcu_dereference_raw(nb->next);
			nb = next_nb;
			nr_to_call--;
		}

		if (nb)
			pr_info("%s: failed at %ps\n", __func__,
					(void *)nb->notifier_call);
	}
#endif
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_lpc_prepare);
