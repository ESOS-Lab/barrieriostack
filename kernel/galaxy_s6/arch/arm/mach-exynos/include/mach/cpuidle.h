/*
 * copyright (c) 2014 samsung electronics co., ltd.
 *		http://www.samsung.com/
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license version 2 as
 * published by the free software foundation.
*/

#include <linux/compiler.h>

#include <asm/io.h>

#define REG_DIRECTGO_ADDR	(S5P_VA_SYSRAM_NS + 0x24)
#define REG_DIRECTGO_FLAG	(S5P_VA_SYSRAM_NS + 0x20)

#define L2_CCI_OFF		(1 << 1)

struct check_reg {
	void __iomem	*reg;
	unsigned int	check_bit;
};

int exynos_check_reg_status(struct check_reg *reg_list,
				    unsigned int list_cnt)
{
	unsigned int i;
	unsigned int tmp;

	for (i = 0; i < list_cnt; i++) {
		tmp = __raw_readl(reg_list[i].reg);
		if (tmp & reg_list[i].check_bit)
			return -EBUSY;
	}

	return 0;
}

struct sfr_bit_ctrl {
	void __iomem	*reg;
	unsigned int	offset;
	unsigned int	mask;
	unsigned int	val;
};

#define SFR_CTRL(_reg, _offset, _mask, _val)	\
	{					\
		.reg	= _reg,			\
		.offset = _offset,		\
		.mask	= _mask,		\
		.val	= _val,			\
	}

void exynos_set_sfr(struct sfr_bit_ctrl *ptr, int count)
{
	unsigned int tmp;

	for (; count > 0; count--, ptr++) {
		tmp = __raw_readl(ptr->reg);
		tmp &= ~(ptr->mask << ptr->offset);
		tmp |= (ptr->val << ptr->offset);
		__raw_writel(tmp, ptr->reg);
	}
}
