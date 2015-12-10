#ifndef _BCM477x_DEBUG_
#define _BCM477x_DEBUG_

#define MAX_BCM477X_DEBUG_BUFFER_SIZE (64*1024)

struct bcm477x_debug
{
    bool is_opened;

	bool buf_full;
	unsigned long buf_head;
	unsigned long buf_tail;
	unsigned char *buf;
};

#endif // _BCM477x_DEBUG_
