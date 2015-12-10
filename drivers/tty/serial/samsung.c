/*
 * Driver core for Samsung SoC onboard UARTs.
 *
 * Ben Dooks, Copyright (c) 2003-2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

/* Hote on 2410 error handling
 *
 * The s3c2410 manual has a love/hate affair with the contents of the
 * UERSTAT register in the UART blocks, and keeps marking some of the
 * error bits as reserved. Having checked with the s3c2410x01,
 * it copes with BREAKs properly, so I am happy to ignore the RESERVED
 * feature from the latter versions of the manual.
 *
 * If it becomes aparrent that latter versions of the 2410 remove these
 * bits, then action will have to be taken to differentiate the versions
 * and change the policy on BREAK
 *
 * BJD, 04-Nov-2004
*/

#if defined(CONFIG_SERIAL_SAMSUNG_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/serial_s3c.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/suspend.h>
#ifdef CONFIG_SERIAL_SAMSUNG_DMA
#include <linux/slab.h>
#include <linux/spinlock.h>
#endif
#include <linux/of.h>

#include <sound/exynos.h>

#include <asm/irq.h>

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
#include <mach/regs-clock.h>
#include <linux/dma/dma-pl330.h>
#endif

#ifdef CONFIG_SAMSUNG_CLOCK
#include <plat/clock.h>
#endif

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
#include <linux/dma-mapping.h>
#endif

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "samsung.h"
#include "../../pinctrl/core.h"
#include <mach/exynos-pm.h>

/* UART name and device definitions */

#define S3C24XX_SERIAL_NAME	"ttySAC"
#define S3C24XX_SERIAL_MAJOR	204
#define S3C24XX_SERIAL_MINOR	64

/* Baudrate definition*/
#define MAX_BAUD	3000000
#define MIN_BAUD	0

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
/* definitions for dma mode */
#define ENABLE_UART_DMA_MODE	true
#define DMA_TRANS_LIMIT	64
#define RX_BUFFER_SIZE	256
#define BURST_1BYTE	0
#endif

#define BT_UART_TRACE 1

#ifdef BT_UART_TRACE
#define BT_LOG_BUFFER_SIZE (0x19000) /* Allocate 100KB of buffer */
#define PROC_DIR	"bluetooth/uart"
#define BLUETOOTH_UART_PORT_LINE 4
#endif

/* macros to change one thing to another */

#define tx_enabled(port) ((port)->unused[0])
#define rx_enabled(port) ((port)->unused[1])

/* flag to ignore all characters coming in */
#define RXSTAT_DUMMY_READ (0x10000000)

static LIST_HEAD(drvdata_list);
s3c_wake_peer_t s3c2410_serial_wake_peer[CONFIG_SERIAL_SAMSUNG_UARTS];
EXPORT_SYMBOL_GPL(s3c2410_serial_wake_peer);

#ifdef BT_UART_TRACE
struct proc_dir_entry *bluetooth_dir, *bt_log_dir;
static void uart_copy_local_buf (int dir, struct local_buf *local_buf, const unsigned int *buf, int len)
{
	int a;
	unsigned long long t;
	unsigned long nanosec_rem;
	int this_cpu = smp_processor_id();

	if (local_buf->index + len*3 + 21 >= local_buf->size) {
		local_buf->index = 0;
	}

	if (dir == 1)
		local_buf->index += sprintf(local_buf->buffer + local_buf->index, "[RX] ");
	else
		local_buf->index += sprintf(local_buf->buffer + local_buf->index, "[TX] ");

	t = cpu_clock(this_cpu);
	nanosec_rem = do_div(t, 1000000000);
	local_buf->index += sprintf(local_buf->buffer + local_buf->index, "[%5lu.%06lu] ", (unsigned long) t, nanosec_rem / 1000);

	for (a = 0; a < len; a++) {
		local_buf->index += sprintf(local_buf->buffer + local_buf->index, "%02X ", buf[a]);
	}

	local_buf->index += sprintf(local_buf->buffer + local_buf->index, "\n");
}
#endif

static void s3c24xx_serial_resetport(struct uart_port *port,
				   struct s3c2410_uartcfg *cfg);
static void s3c24xx_serial_pm(struct uart_port *port, unsigned int level,
			      unsigned int old);
static struct uart_driver s3c24xx_uart_drv;

static inline void uart_clock_enable(struct s3c24xx_uart_port *ourport)
{
	if (ourport->check_separated_clk)
		clk_prepare_enable(ourport->separated_clk);
	clk_prepare_enable(ourport->clk);
}

static inline void uart_clock_disable(struct s3c24xx_uart_port *ourport)
{
	clk_disable_unprepare(ourport->clk);
	if (ourport->check_separated_clk)
		clk_disable_unprepare(ourport->separated_clk);
}

#define MAX_AUD_UART_PIN_STATE	3
#define AUD_UART_PIN_IDLE	0
#define AUD_UART_PIN_LPM	1
#define AUD_UART_PIN_DEFAULT	2
struct pinctrl_state *uart_pin_state[MAX_AUD_UART_PIN_STATE];
struct pinctrl *aud_uart_pinctrl;

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
static struct s3c2410_dma_client samsung_uart_dma_client = {
	.name = "samsung-uart-dma",
};

static void prepare_dma(struct uart_dma_data *dma,
					unsigned len, dma_addr_t buf);
#endif

static inline struct s3c24xx_uart_port *to_ourport(struct uart_port *port)
{
	return container_of(port, struct s3c24xx_uart_port, port);
}

/* translate a port to the device name */

static inline const char *s3c24xx_serial_portname(struct uart_port *port)
{
	return to_platform_device(port->dev)->name;
}

static int s3c24xx_serial_txempty_nofifo(struct uart_port *port)
{
	return rd_regl(port, S3C2410_UTRSTAT) & S3C2410_UTRSTAT_TXE;
}

/*
 * s3c64xx and later SoC's include the interrupt mask and status registers in
 * the controller itself, unlike the s3c24xx SoC's which have these registers
 * in the interrupt controller. Check if the port type is s3c64xx or higher.
 */
static int s3c24xx_serial_has_interrupt_mask(struct uart_port *port)
{
	return to_ourport(port)->info->type == PORT_S3C6400;
}

static void s3c24xx_serial_rx_enable(struct uart_port *port)
{
	unsigned long flags;
	unsigned int ucon, ufcon;
	int count = 10000;

	spin_lock_irqsave(&port->lock, flags);

	while (--count && !s3c24xx_serial_txempty_nofifo(port))
		udelay(100);

	ufcon = rd_regl(port, S3C2410_UFCON);
	ufcon |= S3C2410_UFCON_RESETRX;
	wr_regl(port, S3C2410_UFCON, ufcon);

	ucon = rd_regl(port, S3C2410_UCON);
	ucon |= S3C2410_UCON_RXIRQMODE;
	wr_regl(port, S3C2410_UCON, ucon);

	rx_enabled(port) = 1;
	spin_unlock_irqrestore(&port->lock, flags);
}

static void s3c24xx_serial_rx_disable(struct uart_port *port)
{
	unsigned long flags;
	unsigned int ucon;

	spin_lock_irqsave(&port->lock, flags);

	ucon = rd_regl(port, S3C2410_UCON);
	ucon &= ~S3C2410_UCON_RXIRQMODE;
	wr_regl(port, S3C2410_UCON, ucon);

	rx_enabled(port) = 0;
	spin_unlock_irqrestore(&port->lock, flags);
}

static void s3c24xx_serial_stop_tx(struct uart_port *port)
{
	struct s3c24xx_uart_port *ourport = to_ourport(port);
#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	struct exynos_uart_dma *uart_dma = &ourport->uart_dma;
#endif

	if (tx_enabled(port)) {
		if (s3c24xx_serial_has_interrupt_mask(port))
			__set_bit(S3C64XX_UINTM_TXD,
				portaddrl(port, S3C64XX_UINTM));
		else
			disable_irq_nosync(ourport->tx_irq);
#ifdef CONFIG_SERIAL_SAMSUNG_DMA
		if ((uart_dma->use_dma) && (uart_dma->tx.busy)) {
			uart_dma->tx.busy = 0;
			uart_dma->ops->stop(uart_dma->tx.ch);
		}
#endif
		tx_enabled(port) = 0;
		if (port->flags & UPF_CONS_FLOW)
			s3c24xx_serial_rx_enable(port);
	}
}

static void s3c24xx_serial_start_tx(struct uart_port *port)
{
	struct s3c24xx_uart_port *ourport = to_ourport(port);

	if (!tx_enabled(port)) {
		if (port->flags & UPF_CONS_FLOW)
			s3c24xx_serial_rx_disable(port);

		if (s3c24xx_serial_has_interrupt_mask(port))
			__clear_bit(S3C64XX_UINTM_TXD,
				portaddrl(port, S3C64XX_UINTM));
		else
			enable_irq(ourport->tx_irq);
		tx_enabled(port) = 1;
	}
}

static void s3c24xx_serial_stop_rx(struct uart_port *port)
{
	struct s3c24xx_uart_port *ourport = to_ourport(port);
#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	struct exynos_uart_dma *uart_dma = &ourport->uart_dma;
#endif

	if (rx_enabled(port)) {
		dbg("s3c24xx_serial_stop_rx: port=%p\n", port);
		if (s3c24xx_serial_has_interrupt_mask(port))
			__set_bit(S3C64XX_UINTM_RXD,
				portaddrl(port, S3C64XX_UINTM));
		else
			disable_irq_nosync(ourport->rx_irq);
#ifdef CONFIG_SERIAL_SAMSUNG_DMA
		if (uart_dma->use_dma) {
			uart_dma->ops->stop(uart_dma->rx.ch);
		}
#endif
		rx_enabled(port) = 0;
	}
}

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
static void uart_rx_dma_request(struct s3c24xx_uart_port *ourport)
{
	struct uart_port *port = &ourport->port;
	struct exynos_uart_dma *uart_dma = &ourport->uart_dma;

	uart_dma->rx_dst_addr = dma_map_single(port->dev,
			uart_dma->rx_buff,
			uart_dma->rx.req_size,
			DMA_FROM_DEVICE);
	if (dma_mapping_error(port->dev, uart_dma->rx_dst_addr))
		printk(KERN_ERR "Rx DMA mapping error!!!\n");

	/* prepare rx dma mode */
	prepare_dma(&uart_dma->rx,
			uart_dma->rx.req_size, uart_dma->rx_dst_addr);
}

static void callback_uart_rx_dma(void *data)
{
	struct exynos_uart_dma *uart_dma = container_of(data,
						struct exynos_uart_dma, rx);
	struct s3c24xx_uart_port *ourport = container_of(uart_dma,
					struct s3c24xx_uart_port, uart_dma);
	struct uart_port *port = &ourport->port;
	unsigned int uerstat = ourport->err_occurred;
	unsigned int received_size = 0;
	dma_addr_t src_addr = 0;
	dma_addr_t dst_addr = 0;

	uart_dma->ops->getposition(uart_dma->rx.ch, &src_addr, &dst_addr);
	received_size = dst_addr - (unsigned int)uart_dma->rx_dst_addr;

	dma_unmap_single(port->dev, uart_dma->rx_dst_addr,
			received_size, DMA_FROM_DEVICE);

	if (received_size == 0)
		goto out;

	/* Error check after DMA transfer */
	if (uerstat != 0) {
		printk(KERN_ERR "UART Rx DMA Error(0x%x)!!!\n", uerstat);

		if (uerstat & S3C2410_UERSTAT_BREAK) {
			dbg("break!\n");
			port->icount.brk += received_size;
		}

		if (uerstat & S3C2410_UERSTAT_FRAME)
			port->icount.frame += received_size;
		if (uerstat & S3C2410_UERSTAT_OVERRUN)
			port->icount.overrun += received_size;

		uerstat &= port->read_status_mask;

		ourport->err_occurred = 0;

	}

	tty_insert_flip_string(&port->state->port, uart_dma->rx_buff, received_size);
	tty_flip_buffer_push(&port->state->port);

out:
	if (uart_dma->rx.busy == 1)
		uart_rx_dma_request(ourport);
}

static void enable_rx_dma(struct uart_port *port)
{
	unsigned long flags;
	unsigned int ufcon, ucon;

	spin_lock_irqsave(&port->lock, flags);

	ufcon = rd_regl(port, S3C2410_UFCON);
	ufcon &= ~(0x7 << 4);
	ufcon |= (0x1 << 2) | (0x1 << 1) | S5PV210_UFCON_RXTRIG64;
	wr_regl(port, S3C2410_UFCON, ufcon);

	/* set Rx mode to DMA mode */
	ucon = rd_regl(port, S3C2410_UCON);
	ucon &= ~((0x7 << UCON_RXBURST_SZ) |
			(0x1 << UCON_TIMEOUT_VAL) |
			(0x1 << UCON_EMPTYINT_EN) |
			(0x1 << UCON_DMASUS_EN) |
			UCON_RXMODE_CL);
	ucon |= (BURST_1BYTE << UCON_RXBURST_SZ) |
			(0xf << UCON_TIMEOUT_VAL) |
			(0x1 << UCON_EMPTYINT_EN) |
			(0x1 << UCON_DMASUS_EN) |
			(0x1 << UCON_TIMEOUT_EN) |
			UCON_RXDMA_MODE;
	wr_regl(port, S3C2410_UCON, ucon);

	spin_unlock_irqrestore(&port->lock, flags);

}

static void callback_uart_tx_dma(void *data)
{
	struct exynos_uart_dma *uart_dma;
	struct s3c24xx_uart_port *ourport;
	struct uart_port *port;
	struct circ_buf *xmit;
	unsigned long ucon, uintm;

	uart_dma = container_of(data, struct exynos_uart_dma, tx);
	ourport = container_of(uart_dma, struct s3c24xx_uart_port, uart_dma);
	port = &ourport->port;

	dbg("callback_uart_dma\n");
	dma_unmap_single(port->dev, uart_dma->tx_src_addr,
			uart_dma->tx.req_size, DMA_TO_DEVICE);

	uart_dma->tx.busy = 0;

	xmit = &port->state->xmit;
	xmit->tail = (xmit->tail + uart_dma->tx.req_size)&(UART_XMIT_SIZE - 1);
	port->icount.tx += uart_dma->tx.req_size;

	/* Set Tx mode to interrupt mode */
	ucon = rd_regl(port, S3C2410_UCON);
	ucon &= ~((0x7 << UCON_TXBURST_SZ) | UCON_TXMODE_CL);
	ucon |= UCON_TXCPU_MODE;
	wr_regl(port,  S3C2410_UCON, ucon);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	/* Unmask tx DMA */
	uintm = rd_regl(port, S3C64XX_UINTM);
	uintm &= ~(0x1 << 2);
	wr_regl(port, S3C64XX_UINTM, uintm);
}

static void prepare_dma(struct uart_dma_data *dma,
					unsigned len, dma_addr_t buf)
{
	struct exynos_uart_dma *uart_dma;
	struct samsung_dma_prep info;
	struct samsung_dma_config config;

	info.cap = DMA_SLAVE;
	info.len = len;
	info.fp = callback_uart_tx_dma;
	info.fp_param = dma;
	info.direction = dma->direction;
	info.buf = buf;

	config.direction = dma->direction;
	config.fifo = dma->fifo_base;
	/* burst size = 1byte (1, 4, 8bytes) */
	config.width = DMA_SLAVE_BUSWIDTH_1_BYTE;

	if (dma->direction == DMA_MEM_TO_DEV) {
		uart_dma = container_of((void *)dma,
				struct exynos_uart_dma, tx);
		info.fp = callback_uart_tx_dma;
		uart_dma->tx.busy = 1;
	} else {
		uart_dma = container_of((void *)dma,
				struct exynos_uart_dma, rx);
		info.fp = callback_uart_rx_dma;
	}

	uart_dma->ops->config(dma->ch, &config);
	uart_dma->ops->prepare(dma->ch, &info);
	uart_dma->ops->trigger(dma->ch);
}

static int acquire_dma(struct exynos_uart_dma *uart_dma)
{
	struct samsung_dma_req req;
	struct device *dev = &uart_dma->pdev->dev;

	uart_dma->ops = samsung_dma_get_ops();

	req.cap = DMA_SLAVE;
	req.client = &samsung_uart_dma_client;

	if (uart_dma->rx.busy == 0) {
		uart_dma->rx.ch = uart_dma->ops->request(uart_dma->rx.req_ch,
						&req, dev, "rx");
		uart_dma->rx.busy = 1;
	}
	uart_dma->tx.ch = uart_dma->ops->request(uart_dma->tx.req_ch, &req, dev, "tx");

	return 1;
}
#endif

static void s3c24xx_serial_enable_ms(struct uart_port *port)
{
}

static inline struct s3c24xx_uart_info *s3c24xx_port_to_info(struct uart_port *port)
{
	return to_ourport(port)->info;
}

static inline struct s3c2410_uartcfg *s3c24xx_port_to_cfg(struct uart_port *port)
{
	struct s3c24xx_uart_port *ourport;

	if (port->dev == NULL)
		return NULL;

	ourport = container_of(port, struct s3c24xx_uart_port, port);
	return ourport->cfg;
}

static int s3c24xx_serial_rx_fifocnt(struct s3c24xx_uart_port *ourport,
				     unsigned long ufstat)
{
	struct s3c24xx_uart_info *info = ourport->info;

	if (ufstat & info->rx_fifofull)
		return ourport->port.fifosize;

	return (ufstat & info->rx_fifomask) >> info->rx_fifoshift;
}

static int s3c24xx_serial_tx_fifocnt(struct s3c24xx_uart_port *ourport,
				     unsigned long ufstat)
{
	struct s3c24xx_uart_info *info = ourport->info;

	if (ufstat & info->tx_fifofull)
		return ourport->port.fifosize;

	return (ufstat & info->tx_fifomask) >> info->tx_fifoshift;
}

/* ? - where has parity gone?? */
#define S3C2410_UERSTAT_PARITY (0x1000)

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
static void uart_rx_drain_fifo(struct s3c24xx_uart_port *ourport, int size)
{
	struct uart_port *port = &ourport->port;
	unsigned int ch, flag, ufstat, uerstat;
	int count = 0;

	while (size-- > 0) {
		ufstat = rd_regl(port, S3C2410_UFSTAT);
		if (s3c24xx_serial_rx_fifocnt(ourport, ufstat) == 0)
			break;

		uerstat = rd_regl(port, S3C2410_UERSTAT);
		ch = rd_regb(port, S3C2410_URXH);

		/* insert the character into the buffer */
		flag = TTY_NORMAL;
		port->icount.rx++;

		if (unlikely(uerstat & S3C2410_UERSTAT_ANY)) {
			printk(KERN_ERR "DMA rx drain fifo error!\n");
			if (uerstat & S3C2410_UERSTAT_FRAME) {
				printk(KERN_ERR "frame!!");
				port->icount.frame++;
			}
			if (uerstat & S3C2410_UERSTAT_OVERRUN) {
				printk(KERN_ERR "overrun!!");
				port->icount.overrun++;
			}
				uerstat &= port->read_status_mask;

			if (uerstat & S3C2410_UERSTAT_BREAK) {
				printk(KERN_ERR "break!!");
				flag = TTY_BREAK;
			} else if (uerstat & S3C2410_UERSTAT_PARITY) {
				printk(KERN_ERR "parity!!");
				flag = TTY_PARITY;
			} else if (uerstat & (S3C2410_UERSTAT_FRAME |
					    S3C2410_UERSTAT_OVERRUN))
				flag = TTY_FRAME;
		}
		uart_insert_char(port, uerstat, S3C2410_UERSTAT_OVERRUN,
				 ch, flag);
		count++;
	}
	tty_flip_buffer_push(&port->state->port);
}
#endif

static irqreturn_t
s3c24xx_serial_rx_chars(int irq, void *dev_id)
{
	struct s3c24xx_uart_port *ourport = dev_id;
	struct uart_port *port = &ourport->port;
	unsigned long flags = 0;
#ifndef CONFIG_SERIAL_SAMSUNG_DMA
	unsigned int ufcon, ch, flag, ufstat, uerstat;
	int max_count = 64;
#ifdef BT_UART_TRACE
	unsigned int ch_str[64] = {0, };
	int ch_str_cnt = 0;
#endif

	spin_lock_irqsave(&port->lock, flags);
#else
	struct exynos_uart_dma *uart_dma = &ourport->uart_dma;
	unsigned int ufcon, ufstat, utrstat, uerstat = 0;
	unsigned int ch, flag, received_size = 0;
	dma_addr_t src_addr = 0;
	dma_addr_t dst_addr = 0;
	int max_count = 256;

	spin_lock_irqsave(&port->lock, flags);

	if (uart_dma->use_dma == 0) {
		max_count = 64;
		goto rx_use_cpu;
	}

	utrstat = rd_regl(port, S3C2410_UTRSTAT);
	uart_dma->ops->getposition(uart_dma->rx.ch, &src_addr, &dst_addr);
	received_size = dst_addr - (unsigned int)uart_dma->rx_dst_addr;

	if ((received_size == 0) && (((utrstat >> 16) & 0xff) == 0)) {
		wr_regl(port, S3C2410_UTRSTAT, UTRSTAT_TIMEOUT);
		goto out;
	}
	uart_dma->rx.busy = 0;
	callback_uart_rx_dma(&uart_dma->rx);
	uart_dma->ops->flush(uart_dma->rx.ch);
	uart_dma->rx.busy = 1;

	uart_rx_drain_fifo(ourport, (utrstat >> 16) & 0xff);

	wr_regl(port, S3C2410_UTRSTAT, UTRSTAT_TIMEOUT);

	uart_rx_dma_request(ourport);

	goto out;

rx_use_cpu:
#endif

	while (max_count-- > 0) {
		ufcon = rd_regl(port, S3C2410_UFCON);
		ufstat = rd_regl(port, S3C2410_UFSTAT);

		if (s3c24xx_serial_rx_fifocnt(ourport, ufstat) == 0)
			break;

		uerstat = rd_regl(port, S3C2410_UERSTAT);
		ch = rd_regb(port, S3C2410_URXH);

		if (port->flags & UPF_CONS_FLOW) {
			int txe = s3c24xx_serial_txempty_nofifo(port);

			if (rx_enabled(port)) {
				if (!txe) {
					rx_enabled(port) = 0;
					continue;
				}
			} else {
				if (txe) {
					ufcon |= S3C2410_UFCON_RESETRX;
					wr_regl(port, S3C2410_UFCON, ufcon);
					rx_enabled(port) = 1;
					goto out;
				}
				continue;
			}
		}

		/* insert the character into the buffer */

		flag = TTY_NORMAL;
		port->icount.rx++;

		if (unlikely(uerstat & S3C2410_UERSTAT_ANY)) {
			dbg("rxerr: port ch=0x%02x, rxs=0x%08x\n",
			    ch, uerstat);

			/* check for break */
			if (uerstat & S3C2410_UERSTAT_BREAK) {
				dbg("break!\n");
				port->icount.brk++;
				if (uart_handle_break(port))
					goto ignore_char;
			}

			if (uerstat & S3C2410_UERSTAT_FRAME)
				port->icount.frame++;
			if (uerstat & S3C2410_UERSTAT_OVERRUN)
				port->icount.overrun++;

			uerstat &= port->read_status_mask;

			if (uerstat & S3C2410_UERSTAT_BREAK)
				flag = TTY_BREAK;
			else if (uerstat & S3C2410_UERSTAT_PARITY)
				flag = TTY_PARITY;
			else if (uerstat & (S3C2410_UERSTAT_FRAME |
					    S3C2410_UERSTAT_OVERRUN))
				flag = TTY_FRAME;
		}

		if (uart_handle_sysrq_char(port, ch))
			goto ignore_char;

#ifdef BT_UART_TRACE
		if (ourport->port.line == BLUETOOTH_UART_PORT_LINE)
			ch_str[ch_str_cnt++] = ch;
#endif

		uart_insert_char(port, uerstat, S3C2410_UERSTAT_OVERRUN,
				 ch, flag);

 ignore_char:
		continue;
	}

#ifdef BT_UART_TRACE
	if (ourport->port.line == BLUETOOTH_UART_PORT_LINE) {
		uart_copy_local_buf(1, &ourport->local_buf, ch_str, ch_str_cnt);
	}
#endif
	tty_flip_buffer_push(&port->state->port);

 out:
	spin_unlock_irqrestore(&port->lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t s3c24xx_serial_tx_chars(int irq, void *id)
{
	struct s3c24xx_uart_port *ourport = id;
	struct uart_port *port = &ourport->port;
	struct circ_buf *xmit = &port->state->xmit;
#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	struct exynos_uart_dma *uart_dma = &ourport->uart_dma;
	int remain_data;
	unsigned long ucon;
	unsigned long uintm;
#endif
	unsigned long flags;
	int count = 256;

#ifdef BT_UART_TRACE
	unsigned int ch_str[256] = {0, };
	int ch_str_cnt = 0;
#endif

	spin_lock_irqsave(&port->lock, flags);

	if (port->x_char) {
		wr_regb(port, S3C2410_UTXH, port->x_char);

#ifdef BT_UART_TRACE
		if (ourport->port.line == BLUETOOTH_UART_PORT_LINE)
			ch_str[ch_str_cnt++] = (int)port->x_char;
#endif

		port->icount.tx++;
		port->x_char = 0;
		goto out;
	}

	/* if there isn't anything more to transmit, or the uart is now
	 * stopped, disable the uart and exit
	*/

	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		s3c24xx_serial_stop_tx(port);
		goto out;
	}

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	if (uart_dma->use_dma == 0)
		goto tx_use_cpu;

	remain_data = uart_circ_chars_pending(xmit);
	if (remain_data >= DMA_TRANS_LIMIT) {

		ucon = rd_regl(port, S3C2410_UCON);
		ucon &= ~((0x7 << UCON_TXBURST_SZ) | UCON_TXMODE_CL);
		ucon |= (BURST_1BYTE << UCON_TXBURST_SZ) | UCON_TXDMA_MODE;
		wr_regl(port,  S3C2410_UCON, ucon);

		/* Mask Tx interrupt */
		uintm = rd_regl(port, S3C64XX_UINTM);
		uintm |= (0x1 << 2);
		wr_regl(port, S3C64XX_UINTM, uintm);

		/*
		 * If head is over maximum buffer size,
		 * DMA should transfer data up to end of buffer
		 */
		if (xmit->head - xmit->tail < 0)
			uart_dma->tx.req_size = UART_XMIT_SIZE - xmit->tail;
		else
			uart_dma->tx.req_size = remain_data;

		uart_dma->tx_src_addr = dma_map_single(port->dev,
						&(xmit->buf[xmit->tail]),
						uart_dma->tx.req_size,
						DMA_TO_DEVICE);

		if (dma_mapping_error(port->dev, uart_dma->tx_src_addr)) {
			printk(KERN_ERR "DMA Mapping Error!!!\n");
			goto tx_use_cpu;
		}

		dbg("%s: prepare_dma\n", __func__);
		/* parepare DMA */
		prepare_dma(&uart_dma->tx, uart_dma->tx.req_size,
				uart_dma->tx_src_addr);

		goto out;
	}

tx_use_cpu:
#endif
	/* try and drain the buffer... */

	while (!uart_circ_empty(xmit) && count-- > 0) {
		if (rd_regl(port, S3C2410_UFSTAT) & ourport->info->tx_fifofull)
			break;

		wr_regb(port, S3C2410_UTXH, xmit->buf[xmit->tail]);

#ifdef BT_UART_TRACE
		if (ourport->port.line == BLUETOOTH_UART_PORT_LINE)
			ch_str[ch_str_cnt++] = (int)xmit->buf[xmit->tail];
#endif

		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) {
		spin_unlock(&port->lock);
		uart_write_wakeup(port);
		spin_lock(&port->lock);
	}

	if (uart_circ_empty(xmit))
		s3c24xx_serial_stop_tx(port);

 out:
#ifdef BT_UART_TRACE
	if (ourport->port.line == BLUETOOTH_UART_PORT_LINE) {
		uart_copy_local_buf(0, &ourport->local_buf, ch_str, ch_str_cnt);
	}
#endif
	spin_unlock_irqrestore(&port->lock, flags);
	return IRQ_HANDLED;
}

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
static irqreturn_t s3c24xx_serial_err_chars(int irq, void *id)
{
	struct s3c24xx_uart_port *ourport = id;
	struct uart_port *port = &ourport->port;

	ourport->err_occurred = rd_regl(port, S3C2410_UERSTAT);
	printk(KERN_ERR "Rx DMA Error!!!(0x%x)\n", ourport->err_occurred);
	return IRQ_HANDLED;
}
#endif

#ifdef CONFIG_PM_DEVFREQ
static void s3c64xx_serial_qos_func(struct work_struct *work)
{
	struct s3c24xx_uart_port *ourport =
		container_of(work, struct s3c24xx_uart_port, qos_work.work);
	struct uart_port *port = &ourport->port;

	if (ourport->mif_qos_val)
		pm_qos_update_request_timeout(&ourport->s3c24xx_uart_mif_qos,
				ourport->mif_qos_val, ourport->qos_timeout);

	if (ourport->cpu_qos_val)
		pm_qos_update_request_timeout(&ourport->s3c24xx_uart_cpu_qos,
				ourport->cpu_qos_val, ourport->qos_timeout);

	if (ourport->uart_irq_affinity)
		irq_set_affinity(port->irq, cpumask_of(ourport->uart_irq_affinity));
}
#endif

/* interrupt handler for s3c64xx and later SoC's.*/
static irqreturn_t s3c64xx_serial_handle_irq(int irq, void *id)
{
	struct s3c24xx_uart_port *ourport = id;
	struct uart_port *port = &ourport->port;
	unsigned int pend = rd_regl(port, S3C64XX_UINTP);
	irqreturn_t ret = IRQ_HANDLED;

#ifdef CONFIG_PM_DEVFREQ
	if ((ourport->mif_qos_val || ourport->cpu_qos_val)
					&& ourport->qos_timeout)
		schedule_delayed_work(&ourport->qos_work,
						msecs_to_jiffies(100));
#endif

	if (pend & S3C64XX_UINTM_RXD_MSK) {
		ret = s3c24xx_serial_rx_chars(irq, id);
		wr_regl(port, S3C64XX_UINTP, S3C64XX_UINTM_RXD_MSK);
	}
	if (pend & S3C64XX_UINTM_TXD_MSK) {
		ret = s3c24xx_serial_tx_chars(irq, id);
		wr_regl(port, S3C64XX_UINTP, S3C64XX_UINTM_TXD_MSK);
	}

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	if (pend & S3C64XX_UINTM_ERR_MSK) {
		ret = s3c24xx_serial_err_chars(irq, id);
		wr_regl(port, S3C64XX_UINTP, S3C64XX_UINTM_ERR_MSK);
	}
#endif
	return ret;
}

static unsigned int s3c24xx_serial_tx_empty(struct uart_port *port)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);
	unsigned long ufstat;
	unsigned long ufcon;

	ufstat = rd_regl(port, S3C2410_UFSTAT);
	ufcon = rd_regl(port, S3C2410_UFCON);
	if (ufcon & S3C2410_UFCON_FIFOMODE) {
		if ((ufstat & info->tx_fifomask) != 0 ||
		    (ufstat & info->tx_fifofull))
			return 0;

		return 1;
	}

	return s3c24xx_serial_txempty_nofifo(port);
}

/* no modem control lines */
static unsigned int s3c24xx_serial_get_mctrl(struct uart_port *port)
{
	unsigned int umstat;

	umstat = rd_regb(port, S3C2410_UMSTAT);

	if (umstat & S3C2410_UMSTAT_CTS)
		return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
	else
		return TIOCM_CAR | TIOCM_DSR;
}

static void s3c24xx_serial_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	/* todo - possibly remove AFC and do manual CTS */
}

static void s3c24xx_serial_break_ctl(struct uart_port *port, int break_state)
{
	unsigned long flags;
	unsigned int ucon;

	spin_lock_irqsave(&port->lock, flags);

	ucon = rd_regl(port, S3C2410_UCON);

	if (break_state)
		ucon |= S3C2410_UCON_SBREAK;
	else
		ucon &= ~S3C2410_UCON_SBREAK;

	wr_regl(port, S3C2410_UCON, ucon);

	spin_unlock_irqrestore(&port->lock, flags);
}

static void s3c24xx_serial_shutdown(struct uart_port *port)
{
	struct s3c24xx_uart_port *ourport = to_ourport(port);
#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	struct exynos_uart_dma *uart_dma = &ourport->uart_dma;
#endif

	if (ourport->tx_claimed) {
		if (!s3c24xx_serial_has_interrupt_mask(port))
			free_irq(ourport->tx_irq, ourport);
		tx_enabled(port) = 0;
		ourport->tx_claimed = 0;

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
		/* Free DMA tx channels */
		if (uart_dma->use_dma == 1) {
			uart_dma->ops->release(uart_dma->tx.ch,
					&samsung_uart_dma_client);
			dbg("%s tx free DMA : %x\n", __func__, port->mapbase);
		}
#endif
	}

	if (ourport->rx_claimed) {
		if (!s3c24xx_serial_has_interrupt_mask(port))
			free_irq(ourport->rx_irq, ourport);
		ourport->rx_claimed = 0;
		rx_enabled(port) = 0;

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
		/* Free DMA rx channels */
		if (uart_dma->use_dma == 1 && uart_dma->rx.busy == 1) {
			uart_dma->rx.busy = 0;
			dma_unmap_single(port->dev, uart_dma->rx_dst_addr,
					uart_dma->rx.req_size, DMA_FROM_DEVICE);
			uart_dma->ops->release(uart_dma->rx.ch,
					&samsung_uart_dma_client);
			kfree(uart_dma->rx_buff);
			dbg("%s rx free DMA : %x\n", __func__, port->mapbase);
		}
#endif
	}

	/* Clear pending interrupts and mask all interrupts */
	if (s3c24xx_serial_has_interrupt_mask(port)) {
		free_irq(port->irq, ourport);

		wr_regl(port, S3C64XX_UINTP, 0xf);
		wr_regl(port, S3C64XX_UINTM, 0xf);
	}
}

static int s3c24xx_serial_startup(struct uart_port *port)
{
	struct s3c24xx_uart_port *ourport = to_ourport(port);
	int ret;

	dbg("s3c24xx_serial_startup: port=%p (%08lx,%p)\n",
	    port->mapbase, port->membase);

	ourport->cfg->wake_peer[port->line] =
				s3c2410_serial_wake_peer[port->line];

	rx_enabled(port) = 1;

	ret = request_irq(ourport->rx_irq, s3c24xx_serial_rx_chars, 0,
			  s3c24xx_serial_portname(port), ourport);

	if (ret != 0) {
		dev_err(port->dev, "cannot get irq %d\n", ourport->rx_irq);
		return ret;
	}

	ourport->rx_claimed = 1;

	dbg("requesting tx irq...\n");

	tx_enabled(port) = 1;

	ret = request_irq(ourport->tx_irq, s3c24xx_serial_tx_chars, 0,
			  s3c24xx_serial_portname(port), ourport);

	if (ret) {
		dev_err(port->dev, "cannot get irq %d\n", ourport->tx_irq);
		goto err;
	}

	ourport->tx_claimed = 1;

	dbg("s3c24xx_serial_startup ok\n");

	/* the port reset code should have done the correct
	 * register setup for the port controls */

	return ret;

 err:
	s3c24xx_serial_shutdown(port);
	return ret;
}

static int s3c64xx_serial_startup(struct uart_port *port)
{
	struct s3c24xx_uart_port *ourport = to_ourport(port);
#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	struct exynos_uart_dma *uart_dma = &ourport->uart_dma;
	unsigned long uintm = 0;
#endif
	int ret;

	dbg("s3c64xx_serial_startup: port=%p (%08lx,%p)\n",
	    port->mapbase, port->membase);

	ourport->cfg->wake_peer[port->line] =
				s3c2410_serial_wake_peer[port->line];

	wr_regl(port, S3C64XX_UINTM, 0xf);

	if (ourport->use_default_irq == 1)
		ret = devm_request_irq(port->dev, port->irq, s3c64xx_serial_handle_irq,
				IRQF_SHARED, s3c24xx_serial_portname(port), ourport);
	else
		ret = request_threaded_irq(port->irq, NULL, s3c64xx_serial_handle_irq,
				IRQF_ONESHOT, s3c24xx_serial_portname(port), ourport);

	if (ret) {
		dev_err(port->dev, "cannot get irq %d\n", port->irq);
		return ret;
	}

	/* For compatibility with s3c24xx Soc's */
	rx_enabled(port) = 1;
	ourport->rx_claimed = 1;
	tx_enabled(port) = 0;
	ourport->tx_claimed = 1;

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	if (uart_dma->use_dma) {
		/* initialize err_occurred */
		ourport->err_occurred = 0;

		/* Acquire DMA channels */
		while (!acquire_dma(uart_dma))
			msleep(10);

		/* Alloc buffer for dma */
		uart_dma->rx_buff = kmalloc(RX_BUFFER_SIZE, GFP_KERNEL);
		if (uart_dma->rx_buff == NULL) {
			printk(KERN_ERR "%s:kmalloc failed for RX BUFFER\n",
				__func__);
			free_irq(port->irq, ourport);
			return -ENOMEM;
		}
		uart_dma->rx.req_size = RX_BUFFER_SIZE;

		/* uart_rx_dma_request */
		uart_rx_dma_request(ourport);

		/* UnMask Err interrupt */
		uintm = rd_regl(port, S3C64XX_UINTM);
		uintm &= ~(S3C64XX_UINTM_ERR_MSK);
		wr_regl(port, S3C64XX_UINTM, uintm);

		/* enable rx dma mode */
		enable_rx_dma(port);
	}
#endif

	/* Enable Rx Interrupt */
	__clear_bit(S3C64XX_UINTM_RXD, portaddrl(port, S3C64XX_UINTM));
	dbg("s3c64xx_serial_startup ok\n");
	return ret;
}

static void aud_uart_gpio_cfg(struct device *dev, int level)
{
	struct pinctrl_state *pins_default;
	int status = 0;

	if (level == S3C24XX_UART_PORT_SUSPEND)
		pins_default = uart_pin_state[AUD_UART_PIN_IDLE];
	else if (level == S3C24XX_UART_PORT_LPM)
		pins_default = uart_pin_state[AUD_UART_PIN_LPM];
	else
		pins_default = uart_pin_state[AUD_UART_PIN_DEFAULT];

	if (IS_ERR(pins_default)) {
		dev_info(dev, "Uart is still not probed!!!\n");
		if (level == S3C24XX_UART_PORT_SUSPEND)
			pins_default = pinctrl_lookup_state(aud_uart_pinctrl,
						PINCTRL_STATE_IDLE);
		else if (level == S3C24XX_UART_PORT_LPM)
			pins_default = pinctrl_lookup_state(aud_uart_pinctrl, "lpm");
		else
			pins_default = pinctrl_lookup_state(aud_uart_pinctrl,
						PINCTRL_STATE_DEFAULT);
	}

	if (!IS_ERR(pins_default)) {
		aud_uart_pinctrl->state = NULL;
		status = pinctrl_select_state(aud_uart_pinctrl, pins_default);
		if (status) {
			dev_err(dev, "could not set default pins\n");
			goto err_no_pinctrl;
		}
	} else {
		dev_err(dev, "could not get default pinstate\n");
		goto err_no_pinctrl;
	}

	return;

err_no_pinctrl:
	dev_err(dev, "failed to configure gpio for audio\n");
}

void aud_uart_gpio_idle(struct device *dev)
{
	/* set aud uart gpio for idle */
	aud_uart_gpio_cfg(dev, S3C24XX_UART_PORT_SUSPEND);
}

/* power power management control */
static void s3c24xx_serial_pm(struct uart_port *port, unsigned int level,
			      unsigned int old)
{
	struct s3c24xx_uart_port *ourport = to_ourport(port);
	unsigned int umcon;

	switch (level) {
	case S3C24XX_UART_PORT_SUSPEND:
		/* disable auto flow control & set nRTS for High */
		umcon = rd_regl(port, S3C2410_UMCON);
		umcon &= ~(S3C2410_UMCOM_AFC | S3C2410_UMCOM_RTS_LOW);
		wr_regl(port, S3C2410_UMCON, umcon);

		if (ourport->domain == DOMAIN_AUD)
			aud_uart_gpio_cfg(&ourport->pdev->dev, level);

		uart_clock_disable(ourport);
		break;

	case S3C24XX_UART_PORT_RESUME:
		uart_clock_enable(ourport);

		if (ourport->domain == DOMAIN_AUD)
			aud_uart_gpio_cfg(&ourport->pdev->dev, level);

		s3c24xx_serial_resetport(port, s3c24xx_port_to_cfg(port));
		break;
	default:
		dev_err(port->dev, "s3c24xx_serial: unknown pm %d\n", level);
	}
}

/* baud rate calculation
 *
 * The UARTs on the S3C2410/S3C2440 can take their clocks from a number
 * of different sources, including the peripheral clock ("pclk") and an
 * external clock ("uclk"). The S3C2440 also adds the core clock ("fclk")
 * with a programmable extra divisor.
 *
 * The following code goes through the clock sources, and calculates the
 * baud clocks (and the resultant actual baud rates) and then tries to
 * pick the closest one and select that.
 *
*/

#define MAX_CLK_NAME_LENGTH 15

static inline int s3c24xx_serial_getsource(struct uart_port *port)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);
	unsigned int ucon;

	if (info->num_clks == 1)
		return 0;

	ucon = rd_regl(port, S3C2410_UCON);
	ucon &= info->clksel_mask;
	return ucon >> info->clksel_shift;
}

static void s3c24xx_serial_setsource(struct uart_port *port,
			unsigned int clk_sel)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);
	unsigned int ucon;

	if (info->num_clks == 1)
		return;

	ucon = rd_regl(port, S3C2410_UCON);
	if ((ucon & info->clksel_mask) >> info->clksel_shift == clk_sel)
		return;

	ucon &= ~info->clksel_mask;
	ucon |= clk_sel << info->clksel_shift;
	wr_regl(port, S3C2410_UCON, ucon);
}

static unsigned int s3c24xx_serial_getclk(struct s3c24xx_uart_port *ourport,
			unsigned int req_baud, struct clk **best_clk,
			unsigned int *clk_num)
{
	struct s3c24xx_uart_info *info = ourport->info;
	struct clk *clk;
	unsigned long rate;
	unsigned int cnt, baud, quot, clk_sel, best_quot = 0;
	char clkname[MAX_CLK_NAME_LENGTH];
	int calc_deviation, deviation = (1 << 30) - 1;

	clk_sel = (ourport->cfg->clk_sel) ? ourport->cfg->clk_sel :
			ourport->info->def_clk_sel;
	for (cnt = 0; cnt < info->num_clks; cnt++) {
		if (!(clk_sel & (1 << cnt)))
			continue;

		snprintf(clkname, sizeof(clkname), "sclk_uart%d", ourport->port.line);
		clk = clk_get(ourport->port.dev, clkname);
		if (IS_ERR(clk))
			continue;

		rate = clk_get_rate(clk);
		if (!rate)
			continue;

		if (ourport->info->has_divslot) {
			unsigned long div = rate / req_baud;

			/* The UDIVSLOT register on the newer UARTs allows us to
			 * get a divisor adjustment of 1/16th on the baud clock.
			 *
			 * We don't keep the UDIVSLOT value (the 16ths we
			 * calculated by not multiplying the baud by 16) as it
			 * is easy enough to recalculate.
			 */

			quot = div / 16;
			baud = rate / div;
		} else {
			quot = (rate + (8 * req_baud)) / (16 * req_baud);
			baud = rate / (quot * 16);
		}
		quot--;

		calc_deviation = req_baud - baud;
		if (calc_deviation < 0)
			calc_deviation = -calc_deviation;

		if (calc_deviation < deviation) {
			*best_clk = clk;
			best_quot = quot;
			*clk_num = cnt;
			deviation = calc_deviation;
		}
	}

	return best_quot;
}

/* udivslot_table[]
 *
 * This table takes the fractional value of the baud divisor and gives
 * the recommended setting for the UDIVSLOT register.
 */
static u16 udivslot_table[16] = {
	[0] = 0x0000,
	[1] = 0x0080,
	[2] = 0x0808,
	[3] = 0x0888,
	[4] = 0x2222,
	[5] = 0x4924,
	[6] = 0x4A52,
	[7] = 0x54AA,
	[8] = 0x5555,
	[9] = 0xD555,
	[10] = 0xD5D5,
	[11] = 0xDDD5,
	[12] = 0xDDDD,
	[13] = 0xDFDD,
	[14] = 0xDFDF,
	[15] = 0xFFDF,
};

static void s3c24xx_serial_set_termios(struct uart_port *port,
				       struct ktermios *termios,
				       struct ktermios *old)
{
	struct s3c2410_uartcfg *cfg = s3c24xx_port_to_cfg(port);
	struct s3c24xx_uart_port *ourport = to_ourport(port);
	struct clk *clk = ERR_PTR(-EINVAL);
	unsigned long flags;
	unsigned int baud, quot, clk_sel = 0;
	unsigned int ulcon;
	unsigned int umcon;
	unsigned int udivslot = 0;

	/*
	 * We don't support modem control lines.
	 */
	termios->c_cflag &= ~(HUPCL | CMSPAR);
	termios->c_cflag |= CLOCAL;

	/*
	 * Ask the core to calculate the divisor for us.
	 */

	baud = uart_get_baud_rate(port, termios, old, MIN_BAUD, MAX_BAUD);
	quot = s3c24xx_serial_getclk(ourport, baud, &clk, &clk_sel);
	if (baud == 38400 && (port->flags & UPF_SPD_MASK) == UPF_SPD_CUST)
		quot = port->custom_divisor;
	if (IS_ERR(clk))
		return;

	/* check to see if we need  to change clock source */

	if (ourport->baudclk != clk) {
		s3c24xx_serial_setsource(port, clk_sel);

		if (!IS_ERR(ourport->baudclk)) {
			clk_disable_unprepare(ourport->baudclk);
			ourport->baudclk = ERR_PTR(-EINVAL);
		}

		clk_prepare_enable(clk);

		ourport->baudclk = clk;
		ourport->baudclk_rate = clk ? clk_get_rate(clk) : 0;
	}

	if (ourport->info->has_divslot) {
		unsigned int div = ourport->baudclk_rate / baud;

		if (cfg->has_fracval) {
			udivslot = (div & 15);
			dbg("fracval = %04x\n", udivslot);
		} else {
			udivslot = udivslot_table[div & 15];
			dbg("udivslot = %04x (div %d)\n", udivslot, div & 15);
		}
	}

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		dbg("config: 5bits/char\n");
		ulcon = S3C2410_LCON_CS5;
		break;
	case CS6:
		dbg("config: 6bits/char\n");
		ulcon = S3C2410_LCON_CS6;
		break;
	case CS7:
		dbg("config: 7bits/char\n");
		ulcon = S3C2410_LCON_CS7;
		break;
	case CS8:
	default:
		dbg("config: 8bits/char\n");
		ulcon = S3C2410_LCON_CS8;
		break;
	}

	/* preserve original lcon IR settings */
	ulcon |= (cfg->ulcon & S3C2410_LCON_IRM);

	if (termios->c_cflag & CSTOPB)
		ulcon |= S3C2410_LCON_STOPB;

	umcon = (termios->c_cflag & CRTSCTS) ? S3C2410_UMCOM_AFC : 0;

	if (termios->c_cflag & PARENB) {
		if (termios->c_cflag & PARODD)
			ulcon |= S3C2410_LCON_PODD;
		else
			ulcon |= S3C2410_LCON_PEVEN;
	} else {
		ulcon |= S3C2410_LCON_PNONE;
	}

	spin_lock_irqsave(&port->lock, flags);

	dbg("setting ulcon to %08x, brddiv to %d, udivslot %08x\n",
	    ulcon, quot, udivslot);

	wr_regl(port, S3C2410_ULCON, ulcon);
	wr_regl(port, S3C2410_UBRDIV, quot);

	if (ourport->info->has_divslot)
		wr_regl(port, S3C2443_DIVSLOT, udivslot);

	wr_regl(port, S3C2410_UMCON, umcon);

	dbg("uart: ulcon = 0x%08x, ucon = 0x%08x, ufcon = 0x%08x\n",
	    rd_regl(port, S3C2410_ULCON),
	    rd_regl(port, S3C2410_UCON),
	    rd_regl(port, S3C2410_UFCON));

	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	/*
	 * Which character status flags are we interested in?
	 */
	port->read_status_mask = S3C2410_UERSTAT_OVERRUN;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= S3C2410_UERSTAT_FRAME | S3C2410_UERSTAT_PARITY;

	/*
	 * Which character status flags should we ignore?
	 */
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= S3C2410_UERSTAT_OVERRUN;
	if (termios->c_iflag & IGNBRK && termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= S3C2410_UERSTAT_FRAME;

	/*
	 * Ignore all characters if CREAD is not set.
	 */
	if ((termios->c_cflag & CREAD) == 0)
		port->ignore_status_mask |= RXSTAT_DUMMY_READ;

	spin_unlock_irqrestore(&port->lock, flags);
}

static const char *s3c24xx_serial_type(struct uart_port *port)
{
	switch (port->type) {
	case PORT_S3C2410:
		return "S3C2410";
	case PORT_S3C2440:
		return "S3C2440";
	case PORT_S3C2412:
		return "S3C2412";
	case PORT_S3C6400:
		return "S3C6400/10";
	default:
		return NULL;
	}
}

#define MAP_SIZE (0x100)

static void s3c24xx_serial_release_port(struct uart_port *port)
{
	release_mem_region(port->mapbase, MAP_SIZE);
}

static int s3c24xx_serial_request_port(struct uart_port *port)
{
	const char *name = s3c24xx_serial_portname(port);
	return request_mem_region(port->mapbase, MAP_SIZE, name) ? 0 : -EBUSY;
}

static void s3c24xx_serial_config_port(struct uart_port *port, int flags)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);

	if (flags & UART_CONFIG_TYPE &&
	    s3c24xx_serial_request_port(port) == 0)
		port->type = info->type;
}

/*
 * verify the new serial_struct (for TIOCSSERIAL).
 */
static int
s3c24xx_serial_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);

	if (ser->type != PORT_UNKNOWN && ser->type != info->type)
		return -EINVAL;

	return 0;
}

static void s3c24xx_serial_wake_peer(struct uart_port *port)
{
	struct s3c2410_uartcfg *cfg = s3c24xx_port_to_cfg(port);

	if (cfg->wake_peer[port->line])
		cfg->wake_peer[port->line](port);
}

#ifdef CONFIG_SERIAL_SAMSUNG_CONSOLE

static struct console s3c24xx_serial_console;

static int __init s3c24xx_serial_console_init(void)
{
	struct clk *console_clk;
	char pclk_name[16], sclk_name[16];

	snprintf(pclk_name, sizeof(pclk_name), "console-pclk%d", CONFIG_S3C_LOWLEVEL_UART_PORT);
	snprintf(sclk_name, sizeof(sclk_name), "console-sclk%d", CONFIG_S3C_LOWLEVEL_UART_PORT);

	pr_info("Enable clock for console to add reference counter\n");

	console_clk = clk_get(NULL, pclk_name);
	if (IS_ERR(console_clk)) {
		pr_err("Can't get %s!(it's not err)\n", pclk_name);
	} else {
		clk_prepare_enable(console_clk);
	}

	console_clk = clk_get(NULL, sclk_name);
	if (IS_ERR(console_clk)) {
		pr_err("Can't get %s!(it's not err)\n", sclk_name);
	} else {
		clk_prepare_enable(console_clk);
	}

	register_console(&s3c24xx_serial_console);
	return 0;
}
console_initcall(s3c24xx_serial_console_init);

#define S3C24XX_SERIAL_CONSOLE &s3c24xx_serial_console
#else
#define S3C24XX_SERIAL_CONSOLE NULL
#endif

#if defined(CONFIG_SERIAL_SAMSUNG_CONSOLE) && defined(CONFIG_CONSOLE_POLL)
static int s3c24xx_serial_get_poll_char(struct uart_port *port);
static void s3c24xx_serial_put_poll_char(struct uart_port *port,
			 unsigned char c);
#endif

static struct uart_ops s3c24xx_serial_ops = {
	.pm		= s3c24xx_serial_pm,
	.tx_empty	= s3c24xx_serial_tx_empty,
	.get_mctrl	= s3c24xx_serial_get_mctrl,
	.set_mctrl	= s3c24xx_serial_set_mctrl,
	.stop_tx	= s3c24xx_serial_stop_tx,
	.start_tx	= s3c24xx_serial_start_tx,
	.stop_rx	= s3c24xx_serial_stop_rx,
	.enable_ms	= s3c24xx_serial_enable_ms,
	.break_ctl	= s3c24xx_serial_break_ctl,
	.startup	= s3c24xx_serial_startup,
	.shutdown	= s3c24xx_serial_shutdown,
	.set_termios	= s3c24xx_serial_set_termios,
	.type		= s3c24xx_serial_type,
	.release_port	= s3c24xx_serial_release_port,
	.request_port	= s3c24xx_serial_request_port,
	.config_port	= s3c24xx_serial_config_port,
	.verify_port	= s3c24xx_serial_verify_port,
	.wake_peer	= s3c24xx_serial_wake_peer,
#if defined(CONFIG_SERIAL_SAMSUNG_CONSOLE) && defined(CONFIG_CONSOLE_POLL)
	.poll_get_char = s3c24xx_serial_get_poll_char,
	.poll_put_char = s3c24xx_serial_put_poll_char,
#endif
};

static struct uart_driver s3c24xx_uart_drv = {
	.owner		= THIS_MODULE,
	.driver_name	= "s3c2410_serial",
	.nr		= CONFIG_SERIAL_SAMSUNG_UARTS,
	.cons		= S3C24XX_SERIAL_CONSOLE,
	.dev_name	= S3C24XX_SERIAL_NAME,
	.major		= S3C24XX_SERIAL_MAJOR,
	.minor		= S3C24XX_SERIAL_MINOR,
};

static struct s3c24xx_uart_port s3c24xx_serial_ports[CONFIG_SERIAL_SAMSUNG_UARTS] = {
	[0] = {
		.port = {
			.lock		= __SPIN_LOCK_UNLOCKED(s3c24xx_serial_ports[0].port.lock),
			.iotype		= UPIO_MEM,
			.uartclk	= 0,
			.fifosize	= 16,
			.ops		= &s3c24xx_serial_ops,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 0,
		}
	},
	[1] = {
		.port = {
			.lock		= __SPIN_LOCK_UNLOCKED(s3c24xx_serial_ports[1].port.lock),
			.iotype		= UPIO_MEM,
			.uartclk	= 0,
			.fifosize	= 16,
			.ops		= &s3c24xx_serial_ops,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 1,
		}
	},
#if CONFIG_SERIAL_SAMSUNG_UARTS > 2

	[2] = {
		.port = {
			.lock		= __SPIN_LOCK_UNLOCKED(s3c24xx_serial_ports[2].port.lock),
			.iotype		= UPIO_MEM,
			.uartclk	= 0,
			.fifosize	= 16,
			.ops		= &s3c24xx_serial_ops,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 2,
		}
	},
#endif
#if CONFIG_SERIAL_SAMSUNG_UARTS > 3
	[3] = {
		.port = {
			.lock		= __SPIN_LOCK_UNLOCKED(s3c24xx_serial_ports[3].port.lock),
			.iotype		= UPIO_MEM,
			.uartclk	= 0,
			.fifosize	= 16,
			.ops		= &s3c24xx_serial_ops,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 3,
		}
	},
#endif
#if CONFIG_SERIAL_SAMSUNG_UARTS > 4
    [4] = {
        .port = {
            .lock       = __SPIN_LOCK_UNLOCKED(s3c24xx_serial_ports[4].port.lock),
            .iotype     = UPIO_MEM,
            .uartclk    = 0,
            .fifosize   = 16,
            .ops        = &s3c24xx_serial_ops,
            .flags      = UPF_BOOT_AUTOCONF,
            .line       = 4,
        }
	}
#endif
};

/* s3c24xx_serial_resetport
 *
 * reset the fifos and other the settings.
*/

static void s3c24xx_serial_resetport(struct uart_port *port,
				   struct s3c2410_uartcfg *cfg)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);
	unsigned long ucon = rd_regl(port, S3C2410_UCON);
	unsigned int ucon_mask;

	ucon_mask = info->clksel_mask;
	if (info->type == PORT_S3C2440)
		ucon_mask |= S3C2440_UCON0_DIVMASK;

	ucon &= ucon_mask;
	wr_regl(port, S3C2410_UCON,  ucon | cfg->ucon);

	/* reset both fifos */
	wr_regl(port, S3C2410_UFCON, cfg->ufcon | S3C2410_UFCON_RESETBOTH);
	wr_regl(port, S3C2410_UFCON, cfg->ufcon);

	/* some delay is required after fifo reset */
	udelay(1);
}

/* s3c24xx_serial_init_port
 *
 * initialise a single serial port from the platform device given
 */

static int s3c24xx_serial_init_port(struct s3c24xx_uart_port *ourport,
				    struct platform_device *platdev)
{
	struct uart_port *port = &ourport->port;
	struct s3c2410_uartcfg *cfg = ourport->cfg;
	struct resource *res;
#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	struct exynos_uart_dma *uart_dma = &ourport->uart_dma;
#endif
	struct clk *clk;
	char clkname[MAX_CLK_NAME_LENGTH];
	int ret;

	dbg("s3c24xx_serial_init_port: port=%p, platdev=%p\n", port, platdev);

	if (platdev == NULL)
		return -ENODEV;

	if (port->mapbase != 0)
		return 0;

	/* setup info for port */
	port->dev	= &platdev->dev;
	ourport->pdev	= platdev;

	/* Startup sequence is different for s3c64xx and higher SoC's */
	if (s3c24xx_serial_has_interrupt_mask(port))
		s3c24xx_serial_ops.startup = s3c64xx_serial_startup;

	port->uartclk = 1;

	if (cfg->uart_flags & UPF_CONS_FLOW) {
		dbg("s3c24xx_serial_init_port: enabling flow control\n");
		port->flags |= UPF_CONS_FLOW;
	}

	/* sort our the physical and virtual addresses for each UART */

	res = platform_get_resource(platdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(port->dev, "failed to find memory resource for uart\n");
		return -EINVAL;
	}

	dbg("resource %p (%lx..%lx)\n", res, res->start, res->end);

	port->membase = devm_ioremap(port->dev, res->start, resource_size(res));
	if (!port->membase) {
		dev_err(port->dev, "failed to remap controller address\n");
		return -EBUSY;
	}

	port->mapbase = res->start;
	ret = platform_get_irq(platdev, 0);
	if (ret < 0)
		port->irq = 0;
	else {
		port->irq = ret;
		ourport->rx_irq = ret;
		ourport->tx_irq = ret + 1;
	}

	ret = platform_get_irq(platdev, 1);
	if (ret > 0)
		ourport->tx_irq = ret;

#if defined(CONFIG_PM_RUNTIME) && defined(CONFIG_SND_SAMSUNG_AUDSS)
	if (ourport->domain == DOMAIN_AUD)
		lpass_register_subip(&platdev->dev, "aud-uart");
#endif
	if (of_get_property(platdev->dev.of_node,
			"samsung,separate-uart-clk", NULL))
		ourport->check_separated_clk = 1;
	else
		ourport->check_separated_clk = 0;

	snprintf(clkname, sizeof(clkname), "gate_uart%d", ourport->port.line);
	ourport->clk = clk_get(&platdev->dev, clkname);
	if (IS_ERR(ourport->clk)) {
		pr_err("%s: Controller clock not found\n",
				dev_name(&platdev->dev));
		return PTR_ERR(ourport->clk);
	}

	if (ourport->check_separated_clk) {
		snprintf(clkname, sizeof(clkname), "gate_pclk%d", ourport->port.line);
		ourport->separated_clk = clk_get(&platdev->dev, clkname);
		if (IS_ERR(ourport->separated_clk)) {
			pr_err("%s: Controller clock not found\n",
					dev_name(&platdev->dev));
			return PTR_ERR(ourport->separated_clk);
		}

		ret = clk_prepare_enable(ourport->separated_clk);
		if (ret) {
			pr_err("uart: clock failed to prepare+enable: %d\n", ret);
			clk_put(ourport->separated_clk);
			return ret;
		}
	}

	ret = clk_prepare_enable(ourport->clk);
	if (ret) {
		pr_err("uart: clock failed to prepare+enable: %d\n", ret);
		clk_put(ourport->clk);
		return ret;
	}

	uart_clock_enable(ourport);
	if (ourport->sclk_clk_rate != 0) {
		snprintf(clkname, sizeof(clkname), "sclk_uart%d", port->line);
		clk = clk_get(&platdev->dev, clkname);
		clk_set_rate(clk, ourport->sclk_clk_rate);
	}

	/* Keep all interrupts masked and cleared */
	if (s3c24xx_serial_has_interrupt_mask(port)) {
		wr_regl(port, S3C64XX_UINTM, 0xf);
		wr_regl(port, S3C64XX_UINTP, 0xf);
		wr_regl(port, S3C64XX_UINTSP, 0xf);
	}

	dbg("port: map=%08x, mem=%08x, irq=%d (%d,%d), clock=%ld\n",
	    port->mapbase, port->membase, port->irq,
	    ourport->rx_irq, ourport->tx_irq, port->uartclk);

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	/* set tx/rx fifo base for dma */
	if (uart_dma->use_dma) {
		uart_dma->tx.fifo_base = port->mapbase + S3C2410_UTXH;
		uart_dma->rx.fifo_base = port->mapbase + S3C2410_URXH;
	}
#endif

	/* reset the fifos (and setup the uart) */
	s3c24xx_serial_resetport(port, cfg);
	uart_clock_disable(ourport);
	return 0;
}

#ifdef CONFIG_SAMSUNG_CLOCK
static ssize_t s3c24xx_serial_show_clksrc(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct uart_port *port = s3c24xx_dev_to_port(dev);
	struct s3c24xx_uart_port *ourport = to_ourport(port);

	if (IS_ERR(ourport->baudclk))
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "* %s\n",
			ourport->baudclk->name ?: "(null)");
}

static DEVICE_ATTR(clock_source, S_IRUGO, s3c24xx_serial_show_clksrc, NULL);
#endif

/* Device driver serial port probe */

static const struct of_device_id s3c24xx_uart_dt_match[];
static int probe_index;

static inline struct s3c24xx_serial_drv_data *s3c24xx_get_driver_data(
			struct platform_device *pdev)
{
#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(s3c24xx_uart_dt_match, pdev->dev.of_node);
		return (struct s3c24xx_serial_drv_data *)match->data;
	}
#endif
	return (struct s3c24xx_serial_drv_data *)
			platform_get_device_id(pdev)->driver_data;
}

void s3c24xx_serial_fifo_wait(void)
{
	struct s3c24xx_uart_port *ourport;
	struct uart_port *port;
	unsigned int fifo_stat;
	unsigned long wait_time;

	list_for_each_entry(ourport, &drvdata_list, node) {
		if (ourport->port.line != CONFIG_S3C_LOWLEVEL_UART_PORT)
			continue;

		wait_time = jiffies + HZ / 4;
		do {
			port = &ourport->port;
			fifo_stat = rd_regl(port, S3C2410_UFSTAT);
			cpu_relax();
		} while (s3c24xx_serial_tx_fifocnt(ourport, fifo_stat)
				&& time_before(jiffies, wait_time));
	}
}
EXPORT_SYMBOL_GPL(s3c24xx_serial_fifo_wait);

static int s3c24xx_serial_notifier(struct notifier_block *self,
				unsigned long cmd, void *v)
{
	switch (cmd) {
	case LPA_ENTER:
		s3c24xx_serial_fifo_wait();
		break;
	}

	return NOTIFY_DONE;
}

#ifdef BT_UART_TRACE
static void s3c24xx_print_reg_status(struct s3c24xx_uart_port *ourport)
{
	if (ourport->port.line == BLUETOOTH_UART_PORT_LINE) {
		struct uart_port *port = &ourport->port;

		unsigned int ulcon = rd_regl(port, S3C2410_ULCON);
		unsigned int ucon = rd_regl(port, S3C2410_UCON);
		unsigned int ufcon = rd_regl(port, S3C2410_UFCON);
		unsigned int utrstat = rd_regl(port, S3C2410_UTRSTAT);
		unsigned int ufstat = rd_regl(port, S3C2410_UFSTAT);
		unsigned int umstat = rd_regl(port, S3C2410_UMSTAT);

		int tx_fifo_full = ufstat & S5PV210_UFSTAT_TXFULL;
		int tx_fifo_count = s3c24xx_serial_tx_fifocnt(ourport, ufstat);

		int rx_fifo_full = ufstat & S5PV210_UFSTAT_RXFULL;
		int rx_fifo_count = s3c24xx_serial_rx_fifocnt(ourport, ufstat);

		pr_err("[BT]: ulcon = 0x%08x, ucon = 0x%08x, ufcon = 0x%08x\n", ulcon, ucon, ufcon);
		pr_err("[BT]: utrstat = 0x%08x, ufstat = 0x%08x, umstat = 0x%08x\n", utrstat, ufstat, umstat);
		pr_err("[BT]: tx_fifo_full = %d, tx_fifo_count = %d\n", tx_fifo_full, tx_fifo_count);
		pr_err("[BT]: rx_fifo_full = %d, rx_fifo_count = %d\n", rx_fifo_full, rx_fifo_count);
	}
}



static ssize_t s3c24xx_serial_bt_log(struct file *file, char __user *userbuf, size_t bytes, loff_t *off)
{
	int ret;
	struct s3c24xx_uart_port *ourport = &s3c24xx_serial_ports[4];
	static int copied_bytes = 0;

	if (copied_bytes >= BT_LOG_BUFFER_SIZE) {
		copied_bytes = 0;
		s3c24xx_print_reg_status(ourport);
		return 0;
	}

	if (copied_bytes + bytes < BT_LOG_BUFFER_SIZE) {
		ret = copy_to_user(userbuf,ourport->local_buf.buffer+copied_bytes, bytes);
		if(ret)
		{
			pr_err("Failed to s3c24xx_serial_bt_log : %d\n", (int)ret);
			return ret;
		}
		copied_bytes += bytes;
		return bytes;
	} else {
		int byte_to_read = BT_LOG_BUFFER_SIZE-copied_bytes;
		ret = copy_to_user(userbuf,ourport->local_buf.buffer+copied_bytes, byte_to_read);
		if(ret)
		{
			pr_err("Failed to s3c24xx_serial_bt_log : %d\n", (int)ret);
			return ret;
		}
		copied_bytes += byte_to_read;
		return byte_to_read;
	}

	return 0;
}
static const struct file_operations proc_fops_btlog = {
	.owner = THIS_MODULE,
	.read = s3c24xx_serial_bt_log,
};
#endif


static struct notifier_block s3c24xx_serial_notifier_block = {
	.notifier_call = s3c24xx_serial_notifier,
};

static int s3c24xx_serial_probe(struct platform_device *pdev)
{
	struct s3c24xx_uart_port *ourport;
#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	struct exynos_uart_dma *uart_dma;
	struct resource	*dma_tx = NULL, *dma_rx = NULL;
#endif
	int ret;
	int port_index = probe_index;

	dbg("s3c24xx_serial_probe(%p)\n", pdev);

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	if (!pdev->dev.of_node) {
		dma_tx = platform_get_resource(pdev, IORESOURCE_DMA, 0);
		if (dma_tx == NULL) {
			dev_err(&pdev->dev, "Unable to get UART-Tx dma resource\n");
			return -ENXIO;
		}

		dma_rx = platform_get_resource(pdev, IORESOURCE_DMA, 1);
		if (dma_rx == NULL) {
			dev_err(&pdev->dev, "Unable to get UART-Rx dma resource\n");
			return -ENXIO;
		}
	}
#endif
	if (pdev->dev.of_node) {
		ret = of_alias_get_id(pdev->dev.of_node, "uart");
		if (ret < 0) {
			dev_err(&pdev->dev, "UART aliases are not defined(%d).\n",
				ret);
		} else {
			port_index = ret;
		}
	}
	ourport = &s3c24xx_serial_ports[port_index];

	ourport->drv_data = s3c24xx_get_driver_data(pdev);
	if (!ourport->drv_data) {
		dev_err(&pdev->dev, "could not find driver data\n");
		return -ENODEV;
	}

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	uart_dma = &ourport->uart_dma;
	uart_dma->pdev = pdev;
	uart_dma->use_dma = ENABLE_UART_DMA_MODE;

	if (uart_dma->use_dma) {
		uart_dma->rx.busy = 0;
		if (!pdev->dev.of_node) {
			uart_dma->tx.req_ch = dma_tx->start;
			uart_dma->rx.req_ch = dma_rx->start;
		}
		uart_dma->tx.direction = DMA_MEM_TO_DEV;
		uart_dma->rx.direction = DMA_DEV_TO_MEM;
	}
#endif
	ourport->baudclk = ERR_PTR(-EINVAL);
	ourport->info = ourport->drv_data->info;
	ourport->cfg = (pdev->dev.platform_data) ?
			(struct s3c2410_uartcfg *)pdev->dev.platform_data :
			ourport->drv_data->def_cfg;

	ourport->port.fifosize = (ourport->info->fifosize) ?
		ourport->info->fifosize :
		ourport->drv_data->fifosize[port_index];

	probe_index++;

	dbg("%s: initialising port %p...\n", __func__, ourport);

#ifdef BT_UART_TRACE
	if (port_index == BLUETOOTH_UART_PORT_LINE) {
		struct proc_dir_entry *ent;

		/* Allocate Data */
		ourport->local_buf.buffer = kmalloc(BT_LOG_BUFFER_SIZE, GFP_KERNEL);

		if (ourport->local_buf.buffer == NULL) {
			pr_err("Could not allocate large buffer\n");
		}
		ourport->local_buf.size = BT_LOG_BUFFER_SIZE;
		ourport->local_buf.index = 0;

		bluetooth_dir = proc_mkdir("bluetooth", NULL);
		if (bluetooth_dir == NULL) {
			pr_err("Unable to create /proc/bluetooth directory\n");
			return -ENOMEM;
		}

		bt_log_dir = proc_mkdir("uart", bluetooth_dir);
		if (bt_log_dir == NULL) {
			pr_err("Unable to create /proc/%s directory\n", PROC_DIR);
			return -ENOMEM;
		}

		ent = proc_create("log", 0, bt_log_dir, &proc_fops_btlog);
		if (ent == NULL) {
			pr_err("Unable to create /proc/%s/log entry\n", PROC_DIR);
			return -ENOMEM;
		}
	}
#endif

#ifdef CONFIG_PM_DEVFREQ
	if (of_property_read_u32(pdev->dev.of_node, "mif_qos_val",
						&ourport->mif_qos_val))
		ourport->mif_qos_val = 0;

	if (of_property_read_u32(pdev->dev.of_node, "cpu_qos_val",
						&ourport->cpu_qos_val))
		ourport->cpu_qos_val = 0;

	if (of_property_read_u32(pdev->dev.of_node, "irq_affinity",
						&ourport->uart_irq_affinity))
		ourport->uart_irq_affinity = 0;

	if (of_property_read_u32(pdev->dev.of_node, "qos_timeout",
					(u32 *)&ourport->qos_timeout))
		ourport->qos_timeout = 0;

	if ((ourport->mif_qos_val || ourport->cpu_qos_val)
					&& ourport->qos_timeout) {
		INIT_DELAYED_WORK(&ourport->qos_work,
						s3c64xx_serial_qos_func);
		/* request pm qos */
		if (ourport->mif_qos_val)
			pm_qos_add_request(&ourport->s3c24xx_uart_mif_qos,
						PM_QOS_BUS_THROUGHPUT, 0);

		if (ourport->cpu_qos_val)
			pm_qos_add_request(&ourport->s3c24xx_uart_cpu_qos,
						PM_QOS_CLUSTER1_FREQ_MIN, 0);
	}
#endif
	if (of_find_property(pdev->dev.of_node, "samsung,lpass-subip", NULL))
		ourport->domain = DOMAIN_AUD;
	else
		ourport->domain = DOMAIN_TOP;

	if (of_find_property(pdev->dev.of_node, "samsung,use-default-irq", NULL))
		ourport->use_default_irq =1;
	else
		ourport->use_default_irq =0;
	if (of_property_read_u32(pdev->dev.of_node, "uart-sclk-frequency",
				(int *)&(ourport->sclk_clk_rate))) {
		dev_err(&pdev->dev, "SCLK frequency is not defined\n");
		ourport->sclk_clk_rate = 0;
	} else {
		dev_err(&pdev->dev, "Set SCLK frequency to %lu\n", ourport->sclk_clk_rate);
	}

	ret = s3c24xx_serial_init_port(ourport, pdev);
	if (ret < 0)
		goto probe_err;

	/* Registering notifier for audio uart */
	if (ourport->domain == DOMAIN_AUD) {
#ifdef CONFIG_SND_SAMSUNG_AUDSS
		lpass_set_gpio_cb(&pdev->dev, &aud_uart_gpio_idle);
#endif
		aud_uart_pinctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(aud_uart_pinctrl)) {
			dev_err(&pdev->dev, "could not get AUD pinctrl\n");
			goto probe_err;
		}
		uart_pin_state[AUD_UART_PIN_IDLE] =
			pinctrl_lookup_state(aud_uart_pinctrl, PINCTRL_STATE_IDLE);
		uart_pin_state[AUD_UART_PIN_LPM] =
			pinctrl_lookup_state(aud_uart_pinctrl, "lpm");
		uart_pin_state[AUD_UART_PIN_DEFAULT] =
			pinctrl_lookup_state(aud_uart_pinctrl, PINCTRL_STATE_DEFAULT);

#ifdef CONFIG_SND_SOC_SAMSUNG
		/* Audio uart always on */
		lpass_get_sync(&pdev->dev);
#endif
	}

	dbg("%s: adding port\n", __func__);
	uart_add_one_port(&s3c24xx_uart_drv, &ourport->port);
	platform_set_drvdata(pdev, &ourport->port);

#ifdef CONFIG_SAMSUNG_CLOCK
	ret = device_create_file(&pdev->dev, &dev_attr_clock_source);
	if (ret < 0)
		dev_err(&pdev->dev, "failed to add clock source attr.\n");
#endif

	list_add_tail(&ourport->node, &drvdata_list);

	return 0;

 probe_err:
	return ret;
}

static int s3c24xx_serial_remove(struct platform_device *dev)
{
	struct uart_port *port = s3c24xx_dev_to_port(&dev->dev);

#ifdef CONFIG_PM_DEVFREQ
	struct s3c24xx_uart_port *ourport = to_ourport(port);

	if (ourport->mif_qos_val && ourport->qos_timeout)
		pm_qos_remove_request(&ourport->s3c24xx_uart_mif_qos);

	if (ourport->cpu_qos_val && ourport->qos_timeout)
		pm_qos_remove_request(&ourport->s3c24xx_uart_cpu_qos);
#endif

	if (port) {
#ifdef CONFIG_SAMSUNG_CLOCK
		device_remove_file(&dev->dev, &dev_attr_clock_source);
#endif

#ifdef BT_UART_TRACE
		if (ourport->port.line == BLUETOOTH_UART_PORT_LINE) {
			remove_proc_entry("lpm", bt_log_dir);
			remove_proc_entry("sleep", bluetooth_dir);
			remove_proc_entry("bluetooth", 0);
			kfree(ourport->local_buf.buffer);
		}
#endif

		uart_remove_one_port(&s3c24xx_uart_drv, port);
	}

	return 0;
}

/* UART power management code */
#ifdef CONFIG_PM_SLEEP
static int s3c24xx_serial_suspend(struct device *dev)
{
	struct uart_port *port = s3c24xx_dev_to_port(dev);

	if (port)
		uart_suspend_port(&s3c24xx_uart_drv, port);

	return 0;
}

static int s3c24xx_serial_resume(struct device *dev)
{
	struct uart_port *port = s3c24xx_dev_to_port(dev);
	struct s3c24xx_uart_port *ourport = to_ourport(port);

	if (port) {
		uart_clock_enable(ourport);
		s3c24xx_serial_resetport(port, s3c24xx_port_to_cfg(port));
		uart_clock_disable(ourport);

		uart_resume_port(&s3c24xx_uart_drv, port);
	}

	return 0;
}

static int s3c24xx_serial_resume_noirq(struct device *dev)
{
	struct uart_port *port = s3c24xx_dev_to_port(dev);
	struct s3c24xx_uart_port *ourport = to_ourport(port);

	if (ourport->domain == DOMAIN_AUD)
		return 0;

	if (port) {
		/* restore IRQ mask */
		if (s3c24xx_serial_has_interrupt_mask(port)) {
			unsigned int uintm = 0xf;
			if (tx_enabled(port))
				uintm &= ~S3C64XX_UINTM_TXD_MSK;
			if (rx_enabled(port))
				uintm &= ~S3C64XX_UINTM_RXD_MSK;
			uart_clock_enable(ourport);
			wr_regl(port, S3C64XX_UINTM, uintm);
			uart_clock_disable(ourport);
		}
	}

	return 0;
}

static const struct dev_pm_ops s3c24xx_serial_pm_ops = {
	.suspend = s3c24xx_serial_suspend,
	.resume = s3c24xx_serial_resume,
	.resume_noirq = s3c24xx_serial_resume_noirq,
};
#define SERIAL_SAMSUNG_PM_OPS	(&s3c24xx_serial_pm_ops)

#else /* !CONFIG_PM_SLEEP */

#define SERIAL_SAMSUNG_PM_OPS	NULL
#endif /* CONFIG_PM_SLEEP */

/* Console code */

#ifdef CONFIG_SERIAL_SAMSUNG_CONSOLE

static struct uart_port *cons_uart;

static int
s3c24xx_serial_console_txrdy(struct uart_port *port, unsigned int ufcon)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);
	unsigned long ufstat, utrstat;

	if (ufcon & S3C2410_UFCON_FIFOMODE) {
		/* fifo mode - check amount of data in fifo registers... */

		ufstat = rd_regl(port, S3C2410_UFSTAT);
		return (ufstat & info->tx_fifofull) ? 0 : 1;
	}

	/* in non-fifo mode, we go and use the tx buffer empty */

	utrstat = rd_regl(port, S3C2410_UTRSTAT);
	return (utrstat & S3C2410_UTRSTAT_TXE) ? 1 : 0;
}

static bool
s3c24xx_port_configured(unsigned int ucon)
{
	/* consider the serial port configured if the tx/rx mode set */
	return (ucon & 0xf) != 0;
}

#ifdef CONFIG_CONSOLE_POLL
/*
 * Console polling routines for writing and reading from the uart while
 * in an interrupt or debug context.
 */

static int s3c24xx_serial_get_poll_char(struct uart_port *port)
{
	struct s3c24xx_uart_port *ourport = to_ourport(port);
	unsigned int ufstat;

	ufstat = rd_regl(port, S3C2410_UFSTAT);
	if (s3c24xx_serial_rx_fifocnt(ourport, ufstat) == 0)
		return NO_POLL_CHAR;

	return rd_regb(port, S3C2410_URXH);
}

static void s3c24xx_serial_put_poll_char(struct uart_port *port,
		unsigned char c)
{
	unsigned int ufcon = rd_regl(cons_uart, S3C2410_UFCON);
	unsigned int ucon = rd_regl(cons_uart, S3C2410_UCON);

	/* not possible to xmit on unconfigured port */
	if (!s3c24xx_port_configured(ucon))
		return;

	while (!s3c24xx_serial_console_txrdy(port, ufcon))
		cpu_relax();
	wr_regb(cons_uart, S3C2410_UTXH, c);
}

#endif /* CONFIG_CONSOLE_POLL */

static void
s3c24xx_serial_console_putchar(struct uart_port *port, int ch)
{
	unsigned int ufcon = rd_regl(cons_uart, S3C2410_UFCON);
	unsigned int ucon = rd_regl(cons_uart, S3C2410_UCON);

	/* not possible to xmit on unconfigured port */
	if (!s3c24xx_port_configured(ucon))
		return;

	while (!s3c24xx_serial_console_txrdy(port, ufcon))
		barrier();
	wr_regb(cons_uart, S3C2410_UTXH, ch);
}

static void
s3c24xx_serial_console_write(struct console *co, const char *s,
			     unsigned int count)
{
	uart_console_write(cons_uart, s, count, s3c24xx_serial_console_putchar);
}

static void __init
s3c24xx_serial_get_options(struct uart_port *port, int *baud,
			   int *parity, int *bits)
{
	struct clk *clk;
	unsigned int ulcon;
	unsigned int ucon;
	unsigned int ubrdiv;
	unsigned long rate;
	char clk_name[MAX_CLK_NAME_LENGTH];

	ulcon  = rd_regl(port, S3C2410_ULCON);
	ucon   = rd_regl(port, S3C2410_UCON);
	ubrdiv = rd_regl(port, S3C2410_UBRDIV);

	dbg("s3c24xx_serial_get_options: port=%p\n"
	    "registers: ulcon=%08x, ucon=%08x, ubdriv=%08x\n",
	    port, ulcon, ucon, ubrdiv);

	if (s3c24xx_port_configured(ucon)) {
		switch (ulcon & S3C2410_LCON_CSMASK) {
		case S3C2410_LCON_CS5:
			*bits = 5;
			break;
		case S3C2410_LCON_CS6:
			*bits = 6;
			break;
		case S3C2410_LCON_CS7:
			*bits = 7;
			break;
		case S3C2410_LCON_CS8:
		default:
			*bits = 8;
			break;
		}

		switch (ulcon & S3C2410_LCON_PMASK) {
		case S3C2410_LCON_PEVEN:
			*parity = 'e';
			break;

		case S3C2410_LCON_PODD:
			*parity = 'o';
			break;

		case S3C2410_LCON_PNONE:
		default:
			*parity = 'n';
		}

		/* now calculate the baud rate */

		snprintf(clk_name, sizeof(clk_name), "sclk_uart%d", port->line);

		clk = clk_get(port->dev, clk_name);
		if (!IS_ERR(clk))
			rate = clk_get_rate(clk);
		else
			rate = 1;

		*baud = rate / (16 * (ubrdiv + 1));
		dbg("calculated baud %d\n", *baud);
	}

}

static int __init
s3c24xx_serial_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	dbg("s3c24xx_serial_console_setup: co=%p (%d), %s\n",
	    co, co->index, options);

	/* is this a valid port */

	if (co->index == -1 || co->index >= CONFIG_SERIAL_SAMSUNG_UARTS)
		co->index = 0;

	port = &s3c24xx_serial_ports[co->index].port;

	/* is the port configured? */

	if (port->mapbase == 0x0)
		return -ENODEV;

	cons_uart = port;

	dbg("s3c24xx_serial_console_setup: port=%p (%d)\n", port, co->index);

	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		s3c24xx_serial_get_options(port, &baud, &parity, &bits);

	dbg("s3c24xx_serial_console_setup: baud %d\n", baud);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console s3c24xx_serial_console = {
	.name		= S3C24XX_SERIAL_NAME,
	.device		= uart_console_device,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.write		= s3c24xx_serial_console_write,
	.setup		= s3c24xx_serial_console_setup,
	.data		= &s3c24xx_uart_drv,
};
#endif /* CONFIG_SERIAL_SAMSUNG_CONSOLE */

#ifdef CONFIG_CPU_S3C2410
static struct s3c24xx_serial_drv_data s3c2410_serial_drv_data = {
	.info = &(struct s3c24xx_uart_info) {
		.name		= "Samsung S3C2410 UART",
		.type		= PORT_S3C2410,
		.fifosize	= 16,
		.rx_fifomask	= S3C2410_UFSTAT_RXMASK,
		.rx_fifoshift	= S3C2410_UFSTAT_RXSHIFT,
		.rx_fifofull	= S3C2410_UFSTAT_RXFULL,
		.tx_fifofull	= S3C2410_UFSTAT_TXFULL,
		.tx_fifomask	= S3C2410_UFSTAT_TXMASK,
		.tx_fifoshift	= S3C2410_UFSTAT_TXSHIFT,
		.def_clk_sel	= S3C2410_UCON_CLKSEL0,
		.num_clks	= 2,
		.clksel_mask	= S3C2410_UCON_CLKMASK,
		.clksel_shift	= S3C2410_UCON_CLKSHIFT,
	},
	.def_cfg = &(struct s3c2410_uartcfg) {
		.ucon		= S3C2410_UCON_DEFAULT,
		.ufcon		= S3C2410_UFCON_DEFAULT,
	},
};
#define S3C2410_SERIAL_DRV_DATA ((kernel_ulong_t)&s3c2410_serial_drv_data)
#else
#define S3C2410_SERIAL_DRV_DATA (kernel_ulong_t)NULL
#endif

#ifdef CONFIG_CPU_S3C2412
static struct s3c24xx_serial_drv_data s3c2412_serial_drv_data = {
	.info = &(struct s3c24xx_uart_info) {
		.name		= "Samsung S3C2412 UART",
		.type		= PORT_S3C2412,
		.fifosize	= 64,
		.has_divslot	= 1,
		.rx_fifomask	= S3C2440_UFSTAT_RXMASK,
		.rx_fifoshift	= S3C2440_UFSTAT_RXSHIFT,
		.rx_fifofull	= S3C2440_UFSTAT_RXFULL,
		.tx_fifofull	= S3C2440_UFSTAT_TXFULL,
		.tx_fifomask	= S3C2440_UFSTAT_TXMASK,
		.tx_fifoshift	= S3C2440_UFSTAT_TXSHIFT,
		.def_clk_sel	= S3C2410_UCON_CLKSEL2,
		.num_clks	= 4,
		.clksel_mask	= S3C2412_UCON_CLKMASK,
		.clksel_shift	= S3C2412_UCON_CLKSHIFT,
	},
	.def_cfg = &(struct s3c2410_uartcfg) {
		.ucon		= S3C2410_UCON_DEFAULT,
		.ufcon		= S3C2410_UFCON_DEFAULT,
	},
};
#define S3C2412_SERIAL_DRV_DATA ((kernel_ulong_t)&s3c2412_serial_drv_data)
#else
#define S3C2412_SERIAL_DRV_DATA (kernel_ulong_t)NULL
#endif

#if defined(CONFIG_CPU_S3C2440) || defined(CONFIG_CPU_S3C2416) || \
	defined(CONFIG_CPU_S3C2443) || defined(CONFIG_CPU_S3C2442)
static struct s3c24xx_serial_drv_data s3c2440_serial_drv_data = {
	.info = &(struct s3c24xx_uart_info) {
		.name		= "Samsung S3C2440 UART",
		.type		= PORT_S3C2440,
		.fifosize	= 64,
		.has_divslot	= 1,
		.rx_fifomask	= S3C2440_UFSTAT_RXMASK,
		.rx_fifoshift	= S3C2440_UFSTAT_RXSHIFT,
		.rx_fifofull	= S3C2440_UFSTAT_RXFULL,
		.tx_fifofull	= S3C2440_UFSTAT_TXFULL,
		.tx_fifomask	= S3C2440_UFSTAT_TXMASK,
		.tx_fifoshift	= S3C2440_UFSTAT_TXSHIFT,
		.def_clk_sel	= S3C2410_UCON_CLKSEL2,
		.num_clks	= 4,
		.clksel_mask	= S3C2412_UCON_CLKMASK,
		.clksel_shift	= S3C2412_UCON_CLKSHIFT,
	},
	.def_cfg = &(struct s3c2410_uartcfg) {
		.ucon		= S3C2410_UCON_DEFAULT,
		.ufcon		= S3C2410_UFCON_DEFAULT,
	},
};
#define S3C2440_SERIAL_DRV_DATA ((kernel_ulong_t)&s3c2440_serial_drv_data)
#else
#define S3C2440_SERIAL_DRV_DATA (kernel_ulong_t)NULL
#endif

#if defined(CONFIG_CPU_S3C6400) || defined(CONFIG_CPU_S3C6410) || \
	defined(CONFIG_CPU_S5P6440) || defined(CONFIG_CPU_S5P6450) || \
	defined(CONFIG_CPU_S5PC100)
static struct s3c24xx_serial_drv_data s3c6400_serial_drv_data = {
	.info = &(struct s3c24xx_uart_info) {
		.name		= "Samsung S3C6400 UART",
		.type		= PORT_S3C6400,
		.fifosize	= 64,
		.has_divslot	= 1,
		.rx_fifomask	= S3C2440_UFSTAT_RXMASK,
		.rx_fifoshift	= S3C2440_UFSTAT_RXSHIFT,
		.rx_fifofull	= S3C2440_UFSTAT_RXFULL,
		.tx_fifofull	= S3C2440_UFSTAT_TXFULL,
		.tx_fifomask	= S3C2440_UFSTAT_TXMASK,
		.tx_fifoshift	= S3C2440_UFSTAT_TXSHIFT,
		.def_clk_sel	= S3C2410_UCON_CLKSEL2,
		.num_clks	= 4,
		.clksel_mask	= S3C6400_UCON_CLKMASK,
		.clksel_shift	= S3C6400_UCON_CLKSHIFT,
	},
	.def_cfg = &(struct s3c2410_uartcfg) {
		.ucon		= S3C2410_UCON_DEFAULT,
		.ufcon		= S3C2410_UFCON_DEFAULT,
	},
};
#define S3C6400_SERIAL_DRV_DATA ((kernel_ulong_t)&s3c6400_serial_drv_data)
#else
#define S3C6400_SERIAL_DRV_DATA (kernel_ulong_t)NULL
#endif

#ifdef CONFIG_CPU_S5PV210
static struct s3c24xx_serial_drv_data s5pv210_serial_drv_data = {
	.info = &(struct s3c24xx_uart_info) {
		.name		= "Samsung S5PV210 UART",
		.type		= PORT_S3C6400,
		.has_divslot	= 1,
		.rx_fifomask	= S5PV210_UFSTAT_RXMASK,
		.rx_fifoshift	= S5PV210_UFSTAT_RXSHIFT,
		.rx_fifofull	= S5PV210_UFSTAT_RXFULL,
		.tx_fifofull	= S5PV210_UFSTAT_TXFULL,
		.tx_fifomask	= S5PV210_UFSTAT_TXMASK,
		.tx_fifoshift	= S5PV210_UFSTAT_TXSHIFT,
		.def_clk_sel	= S3C2410_UCON_CLKSEL0,
		.num_clks	= 2,
		.clksel_mask	= S5PV210_UCON_CLKMASK,
		.clksel_shift	= S5PV210_UCON_CLKSHIFT,
	},
	.def_cfg = &(struct s3c2410_uartcfg) {
		.ucon		= S5PV210_UCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
	.fifosize = { 256, 64, 16, 16 },
};
#define S5PV210_SERIAL_DRV_DATA ((kernel_ulong_t)&s5pv210_serial_drv_data)
#else
#define S5PV210_SERIAL_DRV_DATA	(kernel_ulong_t)NULL
#endif

#if defined(CONFIG_ARCH_EXYNOS)
static struct s3c24xx_serial_drv_data exynos4210_serial_drv_data = {
	.info = &(struct s3c24xx_uart_info) {
		.name		= "Samsung Exynos4 UART",
		.type		= PORT_S3C6400,
		.has_divslot	= 1,
		.rx_fifomask	= S5PV210_UFSTAT_RXMASK,
		.rx_fifoshift	= S5PV210_UFSTAT_RXSHIFT,
		.rx_fifofull	= S5PV210_UFSTAT_RXFULL,
		.tx_fifofull	= S5PV210_UFSTAT_TXFULL,
		.tx_fifomask	= S5PV210_UFSTAT_TXMASK,
		.tx_fifoshift	= S5PV210_UFSTAT_TXSHIFT,
		.def_clk_sel	= S3C2410_UCON_CLKSEL0,
		.num_clks	= 1,
		.clksel_mask	= 0,
		.clksel_shift	= 0,
	},
	.def_cfg = &(struct s3c2410_uartcfg) {
		.ucon		= S5PV210_UCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
		.has_fracval	= 1,
	},
	.fifosize = { 256, 64, 16, 256 },
};

static struct s3c24xx_serial_drv_data exynos5430_serial_drv_data = {
	.info = &(struct s3c24xx_uart_info) {
		.name		= "Samsung Exynos4 UART",
		.type		= PORT_S3C6400,
		.has_divslot	= 1,
		.rx_fifomask	= S5PV210_UFSTAT_RXMASK,
		.rx_fifoshift	= S5PV210_UFSTAT_RXSHIFT,
		.rx_fifofull	= S5PV210_UFSTAT_RXFULL,
		.tx_fifofull	= S5PV210_UFSTAT_TXFULL,
		.tx_fifomask	= S5PV210_UFSTAT_TXMASK,
		.tx_fifoshift	= S5PV210_UFSTAT_TXSHIFT,
		.def_clk_sel	= S3C2410_UCON_CLKSEL0,
		.num_clks	= 1,
		.clksel_mask	= 0,
		.clksel_shift	= 0,
	},
	.def_cfg = &(struct s3c2410_uartcfg) {
		.ucon		= S5PV210_UCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
		.has_fracval	= 1,
	},
	.fifosize = { 64, 256, 16, 256 },
};

static struct s3c24xx_serial_drv_data exynos7420_serial_drv_data = {
	.info = &(struct s3c24xx_uart_info) {
		.name		= "Samsung Exynos4 UART",
		.type		= PORT_S3C6400,
		.has_divslot	= 1,
		.rx_fifomask	= S5PV210_UFSTAT_RXMASK,
		.rx_fifoshift	= S5PV210_UFSTAT_RXSHIFT,
		.rx_fifofull	= S5PV210_UFSTAT_RXFULL,
		.tx_fifofull	= S5PV210_UFSTAT_TXFULL,
		.tx_fifomask	= S5PV210_UFSTAT_TXMASK,
		.tx_fifoshift	= S5PV210_UFSTAT_TXSHIFT,
		.def_clk_sel	= S3C2410_UCON_CLKSEL0,
		.num_clks	= 1,
		.clksel_mask	= 0,
		.clksel_shift	= 0,
	},
	.def_cfg = &(struct s3c2410_uartcfg) {
		.ucon		= S5PV210_UCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
		.has_fracval	= 1,
	},
#ifdef CONFIG_SERIAL_SAMSUNG_UARTS_4
	.fifosize = { 64, 256, 16, 64 },
#else
	.fifosize = { 64, 256, 16, 64, 256, 64 },
#endif
};
#define EXYNOS4210_SERIAL_DRV_DATA ((kernel_ulong_t)&exynos4210_serial_drv_data)
#define EXYNOS5430_SERIAL_DRV_DATA ((kernel_ulong_t)&exynos5430_serial_drv_data)
#define EXYNOS7420_SERIAL_DRV_DATA ((kernel_ulong_t)&exynos7420_serial_drv_data)
#else
#define EXYNOS4210_SERIAL_DRV_DATA (kernel_ulong_t)NULL
#define EXYNOS5430_SERIAL_DRV_DATA (kernel_ulong_t)NULL
#define EXYNOS7420_SERIAL_DRV_DATA (kernel_ulong_t)NULL
#endif

static struct platform_device_id s3c24xx_serial_driver_ids[] = {
	{
		.name		= "s3c2410-uart",
		.driver_data	= S3C2410_SERIAL_DRV_DATA,
	}, {
		.name		= "s3c2412-uart",
		.driver_data	= S3C2412_SERIAL_DRV_DATA,
	}, {
		.name		= "s3c2440-uart",
		.driver_data	= S3C2440_SERIAL_DRV_DATA,
	}, {
		.name		= "s3c6400-uart",
		.driver_data	= S3C6400_SERIAL_DRV_DATA,
	}, {
		.name		= "s5pv210-uart",
		.driver_data	= S5PV210_SERIAL_DRV_DATA,
	}, {
		.name		= "exynos4210-uart",
		.driver_data	= EXYNOS4210_SERIAL_DRV_DATA,
	}, {
		.name		= "exynos5430-uart",
		.driver_data	= EXYNOS5430_SERIAL_DRV_DATA,
	}, {
		.name		= "exynos7420-uart",
		.driver_data	= EXYNOS7420_SERIAL_DRV_DATA,
	},
	{ },
};
MODULE_DEVICE_TABLE(platform, s3c24xx_serial_driver_ids);

#ifdef CONFIG_OF
static const struct of_device_id s3c24xx_uart_dt_match[] = {
	{ .compatible = "samsung,s3c2410-uart",
		.data = (void *)S3C2410_SERIAL_DRV_DATA },
	{ .compatible = "samsung,s3c2412-uart",
		.data = (void *)S3C2412_SERIAL_DRV_DATA },
	{ .compatible = "samsung,s3c2440-uart",
		.data = (void *)S3C2440_SERIAL_DRV_DATA },
	{ .compatible = "samsung,s3c6400-uart",
		.data = (void *)S3C6400_SERIAL_DRV_DATA },
	{ .compatible = "samsung,s5pv210-uart",
		.data = (void *)S5PV210_SERIAL_DRV_DATA },
	{ .compatible = "samsung,exynos4210-uart",
		.data = (void *)EXYNOS4210_SERIAL_DRV_DATA },
	{ .compatible = "samsung,exynos5430-uart",
		.data = (void *)EXYNOS5430_SERIAL_DRV_DATA },
	{ .compatible = "samsung,exynos7420-uart",
		.data = (void *)EXYNOS7420_SERIAL_DRV_DATA },
	{},
};
MODULE_DEVICE_TABLE(of, s3c24xx_uart_dt_match);
#endif

static struct platform_driver samsung_serial_driver = {
	.probe		= s3c24xx_serial_probe,
	.remove		= s3c24xx_serial_remove,
	.id_table	= s3c24xx_serial_driver_ids,
	.driver		= {
		.name	= "samsung-uart",
		.owner	= THIS_MODULE,
		.pm	= SERIAL_SAMSUNG_PM_OPS,
		.of_match_table	= of_match_ptr(s3c24xx_uart_dt_match),
	},
};

/* module initialisation code */

static int __init s3c24xx_serial_modinit(void)
{
	int ret;

	ret = uart_register_driver(&s3c24xx_uart_drv);
	if (ret < 0) {
		pr_err("Failed to register Samsung UART driver\n");
		return ret;
	}

	exynos_pm_register_notifier(&s3c24xx_serial_notifier_block);

	return platform_driver_register(&samsung_serial_driver);
}

static void __exit s3c24xx_serial_modexit(void)
{
	platform_driver_unregister(&samsung_serial_driver);
	uart_unregister_driver(&s3c24xx_uart_drv);
}

module_init(s3c24xx_serial_modinit);
module_exit(s3c24xx_serial_modexit);

MODULE_ALIAS("platform:samsung-uart");
MODULE_DESCRIPTION("Samsung SoC Serial port driver");
MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>");
MODULE_LICENSE("GPL v2");
