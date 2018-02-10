#include <linux/kernel.h>
#include <linux/module.h>

bool jig_status = false;

static int __init jig_check(char *str)
{
	jig_status = true;
	return 0;
}
__setup("jig", jig_check);
