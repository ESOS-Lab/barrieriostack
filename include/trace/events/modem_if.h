#undef TRACE_SYSTEM
#define TRACE_SYSTEM modem_if

#if !defined(_TRACE_MODEM_IF_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MODEM_IF_H

#include <linux/tracepoint.h>

TRACE_EVENT(mif_event,

	TP_PROTO(struct sk_buff *skb, unsigned size, char const *function),

	TP_ARGS(skb, size, function),

	TP_STRUCT__entry(
		__field(void *, skbaddr)
		__field(unsigned, size)
		__array(char, function, SZ_32)
	),

	TP_fast_assign(
		__entry->skbaddr = skb;
		__entry->size = size;
		memcpy(__entry->function, function, SZ_32);
	),

	TP_printk("mif: skbaddr=%p, size=%d, function=%s",
		__entry->skbaddr,
		__entry->size,
		__entry->function)
);

TRACE_EVENT(send_sig,

	TP_PROTO(u16 mask, int value),

	TP_ARGS(mask, value),

	TP_STRUCT__entry(
		__field(u16, mask)
		__field(int, value)
	),

	TP_fast_assign(
		__entry->mask = mask;
		__entry->value = value;
	),

	TP_printk("mif: mask=%x, value=%d", __entry->mask, __entry->value)
);

TRACE_EVENT(mif_log,

	TP_PROTO(const char *tag, long dlen,
		unsigned char *str),

	TP_ARGS(tag, dlen, str),

	TP_STRUCT__entry(
		__array(char, tag, SZ_16)
		__field(long, len)
		__array(char, str, SZ_256)
	),

	TP_fast_assign(
		memcpy(__entry->tag, tag, SZ_16);
		__entry->len = dlen;
		memcpy(__entry->str, str, dlen);
	),

	TP_printk("%s(%ld): %s\n", __entry->tag, __entry->len, __entry->str)
);
#endif

/* This part must be outside protection */
#include <trace/define_trace.h>

