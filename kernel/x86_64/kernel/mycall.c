#include <linux/linkage.h>
#include <linux/kernel.h>

asmlinkage long sys_mycall(void)
{
	printk(KERN_EMERG "My call test\n");
	return 1;
}
