/*
 * kernel/exynos54xx/sensorhub/brcm/bbdpl/bcm_gps_spi.c
 *
 * Driver for BCM4773 SPI/SSI UART Interface
 *
 * Comments:
 *       1. SPI masters are fully supported by the Linux kernel's s3c64xx driver (drivers/spi/spi-s3c64xx.c).
 *          Make sure that your kernel has these drivers compiled - check that CONFIG_SPI, CONFIG_SPI_MASTER and CONFIG_SPI_S3C64XX
 *          are all set to 'y' or 'm' (in case of modules, one might have to rebuild and load the modules before one will be able to play
 *          with your SPI device).
 *          T (GN4 + 4773 A0) phone:
 *            - The configuration file is arch/arm/configs/exynos5430-kqlte_00_defconfig
 *          H (GN3 + 4773) A0 phone
 *            - The configuration file is arch/arm/configs/ha3_00_defconfig
 *
 *		 2. All the gpio pins SCLK,CS0,MOSI,MISO and HOST_REQ should be defined in according to Samsung Reference.
 *		    - HOST_REQ: GPA3_3, Address: 0x1058_0000 + 0x0068, Bit [7:6],RW, Value 0x1=Pull-down enabled
 *          - MOSI: GPD6_1, Address 0x14CC_0000 + 0x01A8:  Bit [3:2],RW, Value 0x1=Pull-down enabled, Reset 0x1
 *                                           PAD Control:  Bit [7:4],RW, Value 0x2=SPI_0_MOSI, Reset 0x0
 *          - MISO: GPD6_0, Address 0x14CC_0000 + 0x01A8:  Bit [3:2],RW, Value 0x1=Pull-down enabled, Reset 0x1
 *                                           PAD Control:  Bit [3:0],RW, Value 0x2=SPI_0_MOSI, Reset 0x0
 *          - CS0:  GPD8_1, Address 0x14CC_0000 + 0x0188:  Bit [3:2],RW, Value 0x1=Pull-down enabled, Reset 0x1
 *                                           PAD Control:  Bit [7:4],RW, Value 0x2=SPI_0_Nss, Reset 0x0
 *          - SCLK: GPD8_0, Address 0x14CC_0000 + 0x0188:  Bit [1:0],RW, Value 0x1=Pull-down enabled, Reset 0x1
 *                                           PAD Control:  Bit [3:0],RW, Value 0x2=SPI_0_CLK, Reset 0x0
 *          T (GN4 + 4773 A0) phone:
 *            - in arch/arm/boot/dts/exynos5430-kqlte_eur_open_00.dts : spi_0: spi@14d2000
 *            -    arch/arm/boot/dts/exynos5430-pinctrl.dtsi :
 *          H (GN3 + 4773) A0 phone:
 *            - in arch/arm/mach-exynos/board-universal5420-sensor.c
 *
 *       3. FIXME: Autobaud, cleaning TX buffer on BCM4773 and do_identification area in probe function because we assue that
 *          NSTANDBY is low after power up. bcm4773_do_identify is temporary commented out.
 *       4. FIXME: This driver uses spi_sync and the complete( ) function to wait for completion, something which takes a few us.
 *          Therefore, this driver may be optimized by using its complete callback function to fire the work to be done/deal with status.
 *       5. FIXME: DMA (CONFIG_SPI_BCM4773_DMA) support may be added for transfer data less fifo_level_mask. It's from 0x1F-0x1FF bytes.
 *          Check spi-s3c64xx.c.
 *
 * Changelog:
 *    2014-03-19 KOM Rev 1. Initial version. Only polling mode. Main workqueue is started from bcm4773_start_tx_work()
 *                          No IRQ from slave
 *    2014-03-20 KOM Rev 2. IRQ support, autobaud and do_identification were added in probe function.
 *    2014-03-21 KOM Rev 3. Checking host_req pin was added in IRQ handler to get rid of asking slave(BCM4773) how many bytes
 *                          are available to read.
 *                          Writing in half duplex mode was added to write more 255 bytes to slave at once if TX UART circle buffer has more 255 bytes.
 *                          Where maximum writing to slave is 4K because it's UART circle buffer size.
 *                          It reduced number of work loops in main work queue. Downloading firmware takes 1.2-2.3s directly and 1.4-1.5s over BBD.
 *                          All measurements are from LHD logs and probably they have overhead e.g. open patch file, read, close etc.
 *   2014-03-21 KOM Rev 4.  Cleaning up and adding comments
 *   2014-03-31 KOM Rev 9.  Merging T-phone & H-phone code. Adding reading always when HOST_REQ is enabled in rxtx_workqueue.
 *   2014-03-31 KOM Rev 10. DMA support added for packet less then 256 bytes. CONFIG_SPI_BCM4773_DMA should be defined in the config file.
 *   2014-04-02 KOM Rev 14. Call of bcm4773_autobaud added to bcm4773_startup(). Probably need to remove call from bcm4773_spi_probe()
 *                          It looks like CONFIG_SPI_BCM4773_DMA (DMA is for >256 bytes ) doesn't improve speed up and may be removed later.
 *   2014-04-02 KOM Rev 15. Using 4096 for BCM4773_MSG_BUF_SIZE instead of 254 because bug fixed in bus driver exynos5420/drivers/spi/spi-s3-s3c64xx.c
 *   2014-04-03 KOM Rev 16. [JIRA FW4773-251] fixed. No malformed packed in gl log for testing H-pnone.
 *                          - Keep using length in bcm4773_read_status for return value (*length) not length in next bcm4773_read. See bcm4773_read_transmit() for more details.
 *                          - Don't use Full diplex mode (CONFIG_SPI_NO_FULL_DUPLEX). Need to fix that issue.
 *                          - Temporary using hardcoded irq 205 in probe instead of 187 for HOST_REQ for H-pnone because something chaned in kernel.
 *                            Need to talk with JK to fix it.
 *   2014-04-04 KOM Rev 17. [JIRA FW4773-251] full-duplex issue fixed.  Both H and T-phones worked faster then before fix during whole day without crash and errors
 *   2014-04-10 KOM Rev 18. [JIRA FW4773-262] using 32bits per word instead of 8bits for DMA transfer data from host to slave in half duplex mode for packets' size > 256 bytes.
 *                          This fix reduces time gap between bytes from  ~1.65us to 200ns.
 *                  Rev 20. Profile code modified (DEBUG_PROFILE==2)
 *                  Rev 21. [JIRA FW4773-271] spin_lock/spin_unlock added to protect "tx_enabled" in bcm4773_irq_handler().
 *                  Rev 22.                   spin_lock/spin_unlock removed, rxtx_work profile added, uart_port statisctic added. DEBUG_PROFILE is set.
 *                  Rev 23.                   DEBUG_PROFILE==2 for bus driver removed.
 *                  Rev 24. tx_disable removed to start rxtx_work. disable/enable removed for interrupt handler.
 *                  Rev 25. the SSI-SPI-DEBUG mode write operation added
 *                  Rev 26. [JIRA FW4773-294] the SSI-SPI-DEBUG mode w/r operations added (bcm477x_debug_write and bcm477x_debug_transfer).
 *                          BBD support added (bbd_main.c and bbd_si_spi.c) into CL 205072 also.
 */

#define DEBUG

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/kthread.h>
#include <linux/circ_buf.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/suspend.h>
#include <linux/kernel.h>
#include "bcm_gps_spi.h"
#include "bbd_internal.h"

#include <linux/ssp_platformdata.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>

#include <linux/of_gpio.h>
#include <asm/io.h>
#include <asm/irq.h>

#define xCONFIG_SPI_BCM4773_DMA       0
#define CONFIG_SPI_DMA_BITS_PER_WORD  32
#define xCONFIG_SPI_NO_FULL_DUPLEX    0
#define CONFIG_SPI_NO_IDENTIFY        1
#define CONFIG_SPI_NO_AUTOBAUD       0
#define CONFIG_DDS
#define CONFIG_4WORD_BURST


#ifdef CONFIG_SPI_DMA_BITS_PER_WORD
#define CONFIG_SPI_DMA_BYTES_PER_WORD (CONFIG_SPI_DMA_BITS_PER_WORD >> 3)
#else
#define CONFIG_SPI_DMA_BYTES_PER_WORD 1
#endif

#ifdef CONFIG_4WORD_BURST
#define WORD_BURST_SIZE		4
#else
#define WORD_BURST_SIZE		1
#endif


#ifdef CONFIG_SPI_BCM4773_DMA   /* Should be defined in the configuration file, e.g. arm/arch/panda_defconfig or drivers/char/Kconfig */
#include <linux/dma-mapping.h>
#endif

#define GPS_VERSION    "1.26"
#define PFX            "bcmgps:"
/* FIXME: Temporary disabled. Do we need this code at all ? */


#ifdef DEBUG
#define xDEBUG_PROFILE                  1
#define xDEBUG_SET_TERMIOS              0
#define DEBUG_TRANSFER                 0
#define xDEBUG_TRANSFER__READ_TRANSMIT  0
#define xDEBUG_TRANSFER__RXTX_WORK      0
#define xDEBUG_TX                       0
#define xDEBUG_IRQ_HANDLER              0
#define xDEBUG_RX                       0
#define xDEBUG_INSERT_FLIP_STRING       0
#define xDEBUG_DDS_GPIO			0

#ifdef DEBUG_TRANSFER
bool ssi_dbg;
static void pk_log(const struct device *dev,char* dir,unsigned char cmd_stat, char* data, int len, int transferred, short status);
#endif  //DEBUG_TRANSFER
#ifdef DEBUG_PROFILE
unsigned long clock_get_us(void);
struct bcm4773_profile
{		
	unsigned long	count[2];
	unsigned long	total_time[2];
	unsigned long	total_size[2];
	unsigned long	min_time[2];
	unsigned long	max_time[2];
	unsigned long	rxtx_work[2];      // rxtx_work_profile
	unsigned long	rxtx_enter[2];     //
};

#define WRITE_TRANS_NUM 500

struct bcm4773_profile_collect_data
{		
	int             trans_type;  // 0 - DMA, 1 - GPIO 
	int             io;          // 0 - W, 1 - R
	unsigned long	len;
	unsigned long	time;
	unsigned long	start_time;
};

int    trans = 0;
int    bcm4773_trans = 0;
struct bcm4773_profile profile_rxtx_work;
struct bcm4773_profile profile_spi_sync;

static void bcm4773_init_the_profile(struct bcm4773_profile *p)
{
	p->count[0]=0;      p->count[1]=0;
	p->total_time[0]=0; p->total_time[1]=0;
	p->total_size[0]=0; p->total_size[1]=0;
	p->min_time[0]=0;   p->min_time[1]=0;
	p->max_time[0]=0;   p->max_time[1]=0;

	p->rxtx_work[0]=0;   p->rxtx_work[1]=0;   // rxtx_work_profile
	p->rxtx_enter[0]=0;  p->rxtx_enter[1]=0;  //

}

#if DEBUG_PROFILE==2
static void bcm4773_get_freq(unsigned long bit_len,unsigned long time,unsigned long *d,unsigned long *d1,unsigned long *d2 )
{
	unsigned long n;
	
	*d = bit_len / time;
	n = (bit_len - (*d * time)) * 10;
	*d1 = n / time;
	*d1 %= 10;
	n = (*d - (*d1 * time)) * 10;
    *d2 = n / time;					  
	*d2 %= 10;
}
#endif

static void bcm4773_profile_print(struct uart_port	*port,const char *func,long line)
{
	pr_debug(PFX "[SSPBBD]: %s.01 rxtx_work: count %ld time %ld in %s(%ld)\n",__func__,
                  profile_rxtx_work.count[0], profile_rxtx_work.total_time[0],
                  func, line);

#if DEBUG_PROFILE==2
	pr_debug(PFX "[SSPBBD]: %s.02.1 spi_sync: W: count %ld time %ld, R: count %ld time %ld\n",__func__,
                  profile_spi_sync.count[0], profile_spi_sync.total_time[0], 
				  profile_spi_sync.count[1], profile_spi_sync.total_time[1]);
#endif

	pr_debug(PFX "[SSPBBD]: %s.02.2 rxtx_start: tx %ld(%ld) irq %ld(%ld), rxtx_work: tx %ld(%ld) irq %ld(%ld)\n",__func__,
			      profile_spi_sync.rxtx_work[0],profile_spi_sync.rxtx_enter[0],profile_spi_sync.rxtx_work[1],profile_spi_sync.rxtx_enter[1],       // rxtx_work_profile
                  profile_rxtx_work.rxtx_work[0],profile_rxtx_work.rxtx_enter[0],profile_rxtx_work.rxtx_work[1],profile_rxtx_work.rxtx_enter[1]);

	pr_debug(PFX "[SSPBBD]: %s.03 uart_port: RX: icnt=%lu be=%lu oe=%lu, TX: icnt=%lu ff=%lu\n",__func__,
			      (unsigned long)port->icount.rx,
			      (unsigned long)port->icount.brk,
			      (unsigned long)port->icount.overrun,
			      (unsigned long)port->icount.tx,
			      (unsigned long)port->icount.buf_overrun);
}

static void bcm4773_profile_init(struct uart_port	*port)
{
	trans = 0;
    port->icount.rx = 0;
    port->icount.brk = 0;
    port->icount.overrun = 0;
    port->icount.tx = 0;
    port->icount.buf_overrun = 0;

    bcm4773_init_the_profile(&profile_rxtx_work);
	bcm4773_init_the_profile(&profile_spi_sync);

}

#endif  //DEBUG_PROFILE

#endif  //DEBUG

/* Ted :: Exported Samsung SSP Driver */
extern struct spi_driver *pssp_driver;
int bbd_tty_close(void);
/* Setting up maximum size of buffer (UART_XMIT_SIZE) for transfer that corresponds size of xmit_uart_circ buffer */
/* FIXME:: Temporary fix for H-phone: Using transfer size 254 instead of more because there is bug in bus driver spi-s3c64xx.c 
           DMA starts working automatically if transfer block has more 256 bytes size. 
 		   During transfer of a few packets (size > 256 bytes), one block transfer has CS0 low delay about 2.5ms after all bytes has been transferred in packet */
#ifndef CONFIG_MACH_UNIVERSAL5420  // T-Phone
#define     BCM4773_MSG_BUF_SIZE   (UART_XMIT_SIZE) 
#else                              // H-Phone: The issue should be fixed in spi-s3c64xx.c in H-phone kernel
                                   // Using 4096 instead of 254 because bug fixed in bus driver exynos5420/drivers/spi/spi-s3-s3c64xx.c
#define     BCM4773_MSG_BUF_SIZE   (UART_XMIT_SIZE)
#endif
struct bcm4773_message
{		
	unsigned char			cmd_stat;
	unsigned char			data[UART_XMIT_SIZE];
} __attribute__((__packed__));

/* structures */
struct bcm4773_uart_port
{
	int				host_req_pin;
	int				mcu_req;
	int				mcu_resp;
	volatile int			rx_enabled;
	volatile int			tx_enabled;
	struct ktermios			termios;
	spinlock_t			lock;
	struct uart_port		port;
	struct work_struct		rxtx_work;
	struct work_struct		start_tx;
	struct work_struct		stop_rx;
	struct work_struct		stop_tx;
	struct workqueue_struct		*serial_wq;
	spinlock_t			irq_lock;
	atomic_t			irq_enabled;
	atomic_t			suspending;

	struct spi_transfer             master_transfer;
	struct spi_transfer             dbg_transfer;
	struct wake_lock             bcm4773_wake_lock;
};

/* UART name and device definitions */
#define BCM4773_SERIAL_NAME	"ttyBCM"
#define BCM4773_SERIAL_MAJOR	205
#define BCM4773_SERIAL_MINOR	0

struct uart_driver		bcm4773_uart_driver=
{
	.owner			= THIS_MODULE,
	.driver_name	= BCM4773_SERIAL_NAME,
	.dev_name		= BCM4773_SERIAL_NAME,
	.nr			    = 1,
	.major			= BCM4773_SERIAL_MAJOR,
	.minor			= BCM4773_SERIAL_MINOR,
	.cons			= NULL,
};

static struct bcm4773_uart_port	*g_bport;

static struct spi_driver	bcm4773_spi_driver;
static struct uart_ops		bcm4773_serial_ops;
static bcm4773_stat_t bcm4773_read_transmit( struct bcm4773_uart_port *bcm_port, int *length );
static irqreturn_t bcm4773_irq_handler( int irq, void *pdata );
#ifndef CONFIG_SPI_NO_AUTOBAUD
static void bcm4773_autobaud(struct bcm4773_uart_port *bport);
#endif
static bcm4773_stat_t bcm4773_reset_fifo( struct bcm4773_uart_port *bcm_port );
#ifndef CONFIG_SPI_NO_IDENTIFY
static int bcm4773_do_identify( struct bcm4773_uart_port *bcm_port );
#endif

//bool esw_ready(void);

static inline struct spi_device *bcm4773_uart_to_spidevice( struct bcm4773_uart_port *bcm_port )
{
	return to_spi_device( bcm_port->port.dev );
}

static inline unsigned char *bcm4773_get_write_buf( struct bcm4773_uart_port *bcm_port )
{
	struct bcm4773_message	*mesg=(struct bcm4773_message *) bcm_port->master_transfer.tx_buf;

	return mesg->data;
}

static inline unsigned char *bcm4773_get_read_buf( struct bcm4773_uart_port *bcm_port )
{
	struct bcm4773_message	*mesg=(struct bcm4773_message *) bcm_port->master_transfer.rx_buf;

	return mesg->data;
}


/**
 * bcm4773_debug_buffer - Return a pointer to a buffer usable by the SSI-debug layer above us.
 * @bport: bcm uart device which is requesting to transfer
 * @len: length of internal tx buffer
 */
static inline unsigned char* bcm4773_debug_buffer(struct bcm4773_uart_port *bport, size_t* len)
{
	if ( len ) {
		struct bcm4773_message	*bcm4773_write_msg=(struct bcm4773_message *) (bport->dbg_transfer.tx_buf);
		memset(bcm4773_write_msg, 0xA5, sizeof(struct bcm4773_message));
		*len = sizeof( *bcm4773_write_msg);
		return (unsigned char*) bcm4773_write_msg;
	}
	return NULL;
}


/**
 * bcm4773_debug_transfer - Write data in debug mode. No need to worry about RX because this is not streaming interface,it is half-duplex
 * @bcm_port: bcm uart device which is requesting transfer
 * @tx: a pointer to a tx buffer usable by the SSI-debug layer.
 * @rx: a pointer to a rx buffer usable by the SSI-debug layer.
 * @len: length of internal tx buffer
 */
#if 0
static inline int bcm4773_debug_transfer(struct bcm4773_uart_port *bport, unsigned char *tx,unsigned char *rx,int len)
{
	int status = 0;
	struct spi_transfer    t = {0,};
	struct spi_message    m;
	unsigned char cmd_stat;
#ifdef DEBUG_TRANSFER
	char   the_dir[10] = {'w',' ',0 };
#endif

	t.tx_buf = tx;
	memset(rx, 0x00, len);
	t.rx_buf = rx;
	t.len = len;

	t.tx_dma=0;
	t.rx_dma=0;
	m.is_dma_mapped=0;

#ifdef DEBUG_TRANSFER
	the_dir[0] = cmd_stat & SSI_READ_TRANS ? 'W':'w';
	pk_log(bport->port.dev,the_dir, cmd_stat,(char *)&rx[0], len, len, status);
#endif

	/* Prepare the data. */
	memcpy( &t, &(bport->master_transfer), sizeof( struct spi_transfer ) );
	cmd_stat = tx[0];
	spi_message_init(&m);

	spi_message_add_tail(&t, &m);
	m.spi=bcm4773_uart_to_spidevice( bport );

	status = spi_sync_locked( m.spi, &m );

	if( status )
		return 0;
	else {
#ifdef DEBUG_TRANSFER
		the_dir[0] = cmd_stat & SSI_READ_TRANS ? 'R':'r';
		cmd_stat = rx[0];
		pk_log(bport->port.dev,the_dir, cmd_stat, (char *)&rx[0], len, len, status);
#endif
		return len;
	}
}
#endif


static inline int bcm4773_debug_transfer(struct bcm4773_uart_port *bport, const unsigned char* buf, size_t length)
{
	int status = 0;
	struct bcm4773_message		*bcm4773_write_msg=(struct bcm4773_message *) (bport->dbg_transfer.tx_buf);
	struct bcm4773_message		*bcm4773_read_msg=(struct bcm4773_message *) (bport->dbg_transfer.rx_buf);
	struct spi_message		msg;
	struct spi_transfer		transfer   = {0,};
	unsigned char			cmd_stat;
#ifdef DEBUG_TRANSFER
	char				the_dir[10] = {'w',' ',0 };
#endif

	if( length > sizeof( bcm4773_write_msg->data ) || length == 0 )
		return -EFAULT;

	if ( buf != &bcm4773_write_msg->cmd_stat )
		return -EFAULT;

	/* Prepare the data. */
	memcpy( &transfer, &(bport->dbg_transfer), sizeof( transfer ) );
	cmd_stat = bcm4773_write_msg->cmd_stat;
	spi_message_init( &msg );

	msg.spi=bcm4773_uart_to_spidevice( bport );
	msg.is_dma_mapped=0;

	transfer.len = (unsigned int)length;
	transfer.tx_dma=0;
	transfer.rx_dma=0;
	transfer.bits_per_word = 8;

#ifdef DEBUG_TRANSFER
	the_dir[0] = cmd_stat & SSI_READ_TRANS ? 'W':'w';
	pk_log(bport->port.dev,the_dir, cmd_stat,(char *)bcm4773_write_msg->data, length, length, status);
#endif


	spi_message_add_tail( &transfer, &msg );
	
#ifdef CONFIG_SPI_DMA_BITS_PER_WORD
	if ( (cmd_stat == (SSI_MODE_HALF_DUPLEX | SSI_WRITE_TRANS | SSI_MODE_DEBUG)) && (length > SSI_MAX_RW_BYTE_COUNT))
	{
		int align = (CONFIG_SPI_DMA_BYTES_PER_WORD * WORD_BURST_SIZE);
		transfer.bits_per_word = (transfer.len % align) ? 8 : CONFIG_SPI_DMA_BITS_PER_WORD;
		if (transfer.bits_per_word == 8)
			printk("%s: byte not aligned\n", __func__);
	}
#endif

	status = spi_sync_locked( msg.spi, &msg );

	if( status )
		return 0;
	else {
#ifdef DEBUG_TRANSFER
		the_dir[0] = cmd_stat & SSI_READ_TRANS ? 'R':'r';
		cmd_stat = bcm4773_read_msg->cmd_stat;  //bcm4773_write_msg->data[0];
		pk_log(bport->port.dev,the_dir, cmd_stat, (char *)&bcm4773_read_msg->data[0], length, length, status);
#endif
		//return  BCM4773_RET(bcm4773_read_msg);
		return length;
	}
}


static unsigned int bcm4773_get_mctrl( struct uart_port *port )
{
	return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
}

static void bcm4773_set_mctrl( struct uart_port *port, unsigned int mctrl )
{
	return;
}

static unsigned int bcm4773_tx_empty( struct uart_port *port )
{
	if( uart_circ_chars_pending( &port->state->xmit ) == 0 )
		return TIOCSER_TEMT;
	else
		return 0;
}

static void bcm4773_stop_tx_work( struct work_struct *work )
{
#ifdef DEBUG_TX
	struct bcm4773_uart_port	*bport=container_of( work, struct bcm4773_uart_port, stop_tx );
//	unsigned long int		flags;

	pr_debug(PFX "[SSPBBD]: %s tx_enabled %d rx_enabled %d\n",__func__,bport->tx_enabled,bport->rx_enabled);
#endif
	return;
}

static void bcm4773_stop_tx( struct uart_port *port )
{
	struct bcm4773_uart_port	*bport=container_of( port, struct bcm4773_uart_port, port );

	queue_work( bport->serial_wq, &(bport->stop_tx) );
	return;
}

static void bcm4773_start_tx_work( struct work_struct *work )
{
	struct bcm4773_uart_port	*bport=container_of( work, struct bcm4773_uart_port, start_tx );
	unsigned long int		flags;

#ifdef DEBUG_IRQ_HANDLER
	struct uart_port		*port=&(bport->port);
	int pending=uart_circ_chars_pending( &port->state->xmit );
	pr_debug(PFX "[SSPBBD]: %s.RXTX_WORK pn %d, tx %d\n",__func__,pending,bport->tx_enabled);
#endif

	spin_lock_irqsave( &(bport->lock), flags );

	queue_work(bport->serial_wq, &(bport->rxtx_work) );

#ifdef DEBUG_PROFILE
	profile_spi_sync.rxtx_work[0]++;   // rxtx_work_profile

	//profile_spi_sync.rxtx_enter[0]++;
#endif
	spin_unlock_irqrestore( &(bport->lock), flags );

	return;
}

static void bcm4773_start_tx( struct uart_port *port )
{
	struct bcm4773_uart_port	*bport=container_of( port, struct bcm4773_uart_port, port );
	unsigned long int		flags;

	spin_lock_irqsave( &bport->irq_lock, flags);
	if (atomic_xchg(&bport->irq_enabled, 0)) {
		struct irq_desc	*desc = irq_to_desc(bport->port.irq);
		if (desc->depth!=0) {
			printk("[SSPBBD]: %s irq depth mismatch. enabled irq has depth %d!\n", __func__, desc->depth);
			desc->depth = 0;
		}
		disable_irq_nosync(bport->port.irq);
	}
	spin_unlock_irqrestore( &bport->irq_lock, flags);

	queue_work(bport->serial_wq, &(bport->start_tx) );

	return;
}

static void bcm4773_stop_rx_work( struct work_struct *work )
{
	struct bcm4773_uart_port	*bport=container_of( work, struct bcm4773_uart_port, stop_rx );
	unsigned long int		flags;

#ifdef DEBUG_RX
	struct uart_port		*port=&(bport->port);
	pr_debug(PFX "[SSPBBD]: %s tx %d, rx %d, oe %d\n",__func__,bport->tx_enabled,bport->rx_enabled,port->icount.buf_overrun);
#endif

	spin_lock_irqsave( &(bport->lock), flags );
	if( bport->rx_enabled )
	{
		bport->rx_enabled=0;
	}
	
#ifdef DEBUG_PROFILE
    bcm4773_profile_print(__func__,__LINE__);
#endif	

	spin_unlock_irqrestore( &(bport->lock), flags );

	return;
}

static void bcm4773_stop_rx( struct uart_port *port )
{
	struct bcm4773_uart_port	*bport=container_of( port, struct bcm4773_uart_port, port );

	queue_work( bport->serial_wq, &(bport->stop_rx) );
	return;
}

static void bcm4773_enable_ms(struct uart_port *port)
{
	return;
}

static void bcm4773_break_ctl(struct uart_port *port, int break_state)
{
	return;
}

static int bcm4773_startup( struct uart_port *port )
{
	struct bcm4773_uart_port	*bport=container_of( port, struct bcm4773_uart_port, port );
	unsigned long int		flags;

	spin_lock_irqsave( &(bport->lock), flags );
	bport->rx_enabled=1;
	bport->tx_enabled=0;
	spin_unlock_irqrestore( &(bport->lock), flags );

	spin_lock_irqsave( &bport->irq_lock, flags);
	if (!atomic_xchg(&bport->irq_enabled, 1)) {
		struct irq_desc	*desc = irq_to_desc(bport->port.irq);
		if (desc->depth!=1) {
			printk("[SSPBBD]: %s irq depth mismatch. disabled irq has depth %d!\n", __func__, desc->depth);
			desc->depth = 1;
		}
		enable_irq(bport->port.irq);
	}
	spin_unlock_irqrestore( &bport->irq_lock, flags);

	enable_irq_wake(bport->port.irq);

#ifndef CONFIG_SPI_NO_AUTOBAUD
	bcm4773_autobaud(bport);
#endif

#ifdef DEBUG_PROFILE
	bcm4773_profile_init();
#endif	

	return 0;
}

static void bcm4773_shutdown( struct uart_port *port )
{
	struct bcm4773_uart_port	*bport=container_of( port, struct bcm4773_uart_port, port );
	unsigned long int		flags;

	spin_lock_irqsave( &(bport->lock), flags );

	if( bport->rx_enabled )
		bcm4773_stop_rx( port );

	if( bport->tx_enabled )
		bcm4773_stop_tx( port );
		
	spin_unlock_irqrestore( &(bport->lock), flags );
	//cancel_work_sync(&bport->rxtx_work);
	//flush_workqueue( bport->serial_wq );

	msleep(10);

	spin_lock_irqsave( &bport->irq_lock, flags);
	if (atomic_xchg(&bport->irq_enabled, 0)) {
		struct irq_desc	*desc = irq_to_desc(bport->port.irq);
		if (desc->depth!=0) {
			printk("[SSPBBD]: %s irq depth mismatch. enabled irq has depth %d!\n", __func__, desc->depth);
			desc->depth = 0;
		}
		disable_irq_nosync(bport->port.irq);
	}
	spin_unlock_irqrestore( &bport->irq_lock, flags);

	disable_irq_wake(bport->port.irq);
	return;
}

static void bcm4773_set_termios(struct uart_port *port, struct ktermios *termios, struct ktermios *old )
{
	struct bcm4773_uart_port	*bport=container_of( port, struct bcm4773_uart_port, port );

#ifdef DEBUG_SET_TERMIOS
	pr_debug(PFX "[SSPBBD]: %s TTY <== i=0x%08X o=0x%08X c=0x%08X l=0x%08X m%d t%d %d %d...oe %d\n",__func__,
            termios->c_iflag, termios->c_oflag,
            termios->c_cflag, termios->c_lflag,
            termios->c_cc[VMIN], termios->c_cc[VTIME],
            termios->c_ispeed,termios->c_ospeed,
			port->icount.buf_overrun);
	pr_debug(PFX "[SSPBBD]: %s opened line %d\n",__func__, port->line);
#endif

	/* Termios field is fake. */
	memcpy( &(bport->termios), termios, sizeof( bport->termios ) );

	/* It doesn't matter. */
	uart_update_timeout( port, CS8, 115200 );
	return;
}

static const char *bcm4773_type(struct uart_port *port)
{
	return "[SSPBBD]: BCM4773_SPI";
}

static void bcm4773_release_port(struct uart_port *port)
{
	return;
}

static int bcm4773_request_port(struct uart_port *port)
{
	return 0;
}

static void bcm4773_config_port(struct uart_port *port, int flags)
{
	return;
}

static int bcm4773_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	if( port->type == PORT_16550A )
		return 0;
	else
		return -EINVAL;
}

static bool bcm4773_hello(struct bcm4773_uart_port *bport)
{
#ifdef CONFIG_DDS
	int count=0, retries=0;

//	if (!esw_ready())
//		return true;

	gpio_set_value(bport->mcu_req, 1);
#ifdef DEBUG_DDS_GPIO
	printk("%s get MCU_REQ_RESP %d\n", __func__, gpio_get_value(bport->mcu_resp));
	printk("%s set MCU_REQ %d\n", __func__, gpio_get_value(bport->mcu_req));
#endif
	while (!gpio_get_value(bport->mcu_resp)) {
#ifdef DEBUG_DDS_GPIO
		printk("%s get MCU_REQ_RESP %d\n", __func__, gpio_get_value(bport->mcu_resp));
#endif

		if (count++ > 100) {
			gpio_set_value(bport->mcu_req, 0);
			printk("%s get MCU_REQ_RESP timeout. MCU_RESP(gpio%d) not responding to MCU_REQ(gpio%d)\n",
				 __func__, bport->mcu_resp, bport->mcu_req);
			return false;
		}

		mdelay(1);

		if (gpio_get_value(bport->mcu_resp)) {
			break;
		}

		if (count%20==0 && retries++ < 3) {
			gpio_set_value(bport->mcu_req, 0);
#ifdef DEBUG_DDS_GPIO
			printk("%s set MCU_REQ %d\n", __func__, gpio_get_value(bport->mcu_req));
#endif
			mdelay(1);
			gpio_set_value(bport->mcu_req, 1);
#ifdef DEBUG_DDS_GPIO
			printk("%s set MCU_REQ %d\n", __func__, gpio_get_value(bport->mcu_req));
#endif
			mdelay(1);
		}

	}
#ifdef DEBUG_DDS_GPIO
	printk("%s get MCU_REQ_RESP %d\n", __func__, gpio_get_value(bport->mcu_resp));
#endif
#endif
	return true;
}

static void bcm4773_bye(struct bcm4773_uart_port *bport)
{
#ifdef CONFIG_DDS
	gpio_set_value(bport->mcu_req, 0);
#ifdef DEBUG_DDS_GPIO
	printk("%s set MCU_REQ %d\n", __func__, gpio_get_value(bport->mcu_req));
#endif
#endif
}

#ifdef CONFIG_MACH_UNIVERSAL5420
void bcm4773_mcu_resp_float(bool do_float)
{
#ifdef CONFIG_DDS
	if(!g_bport)
		return;
//disable MCU_RESP tristating until next LHD release
#if 0
	printk("%s set MCU_RESP float %d\n", __func__, do_float);
	s3c_gpio_setpull(g_bport->mcu_resp, ((do_float) ? S3C_GPIO_PULL_NONE : S3C_GPIO_PULL_UP));
#else
	s3c_gpio_setpull(g_bport->mcu_resp, ((do_float) ? S3C_GPIO_PULL_UP : S3C_GPIO_PULL_UP));
#endif
#endif
}
#endif

/* returns status. */
static bcm4773_stat_t bcm4773_transfer( struct bcm4773_uart_port *bport, unsigned char cmd_stat, int length, int use_dma )
{
	int status = 0;
	struct bcm4773_message		*bcm4773_write_msg=(struct bcm4773_message *) (bport->master_transfer.tx_buf);
	struct bcm4773_message		*bcm4773_read_msg=(struct bcm4773_message *) (bport->master_transfer.rx_buf);
	struct spi_message		msg;
	struct spi_transfer		transfer;
#ifdef DEBUG_TRANSFER
	char   the_dir[10] = {'w',' ',0 };
#endif
#ifdef DEBUG_PROFILE
#if DEBUG_PROFILE==2
	unsigned long t1, t0 = clock_get_us();
	trans = cmd_stat & SSI_READ_TRANS ? 1:0;
#endif
#endif	

	if( length > sizeof( bcm4773_write_msg->data ) || length == 0 )
		return 0xFF;

    /* Prepare the data. */
    memcpy( &transfer, &(bport->master_transfer), sizeof( transfer ) );
    spi_message_init( &msg );

	if( use_dma )
	{
        msg.is_dma_mapped=1;
	}
	else
	{
		transfer.tx_dma=0;
		transfer.rx_dma=0;
        msg.is_dma_mapped=0;
	}

	bcm4773_write_msg->cmd_stat=cmd_stat;

	if ( length != 0 && (cmd_stat & SSI_READ_TRANS) == 0 ) {
	   //unsigned char	*write_buf=bcm4773_get_write_buf( bport );

	   if ( (cmd_stat & SSI_MODE_FULL_DUPLEX)  != 0 ) {
	      bcm4773_write_msg->data[0]=length;
		  length += 1;                                  // 1 means RX_PKT_LEN for Full Duplex Mode
	   } else {
	      transfer.rx_buf = NULL;                       // FIXME: To get rid of call of callback function in bus driver in DMA mode.
		  bcm4773_read_msg->cmd_stat = 0;
		  bcm4773_read_msg->data[0]= 0;
	   }
	}

#ifdef DEBUG_TRANSFER
	the_dir[0] = cmd_stat & SSI_READ_TRANS ? 'W':'w';
	pk_log(bport->port.dev,the_dir, cmd_stat,(char *)bcm4773_write_msg->data, length, length, status);
#endif

	transfer.len=length + 1;  // + 1 byte is for cmd_stat

#ifdef CONFIG_SPI_DMA_BITS_PER_WORD
    if ( (cmd_stat == (SSI_MODE_HALF_DUPLEX | SSI_WRITE_TRANS))  
#ifdef  CONFIG_SPI_NO_FULL_DUPLEX	
	      &&  
	     (length >= SSI_MAX_RW_BYTE_COUNT) 
#endif		 
		) 
	{
	   //transfer.len /= CONFIG_SPI_DMA_BYTES_PER_WORD;
	   transfer.bits_per_word=CONFIG_SPI_DMA_BITS_PER_WORD;
	}
#endif

	spi_message_add_tail( &transfer, &msg );
	msg.spi=bcm4773_uart_to_spidevice( bport );

#ifdef DEBUG_PROFILE
#if DEBUG_PROFILE==2
	bcm4773_trans = 1;	
#endif	
#endif
	status = spi_sync( msg.spi, &msg );

#ifdef DEBUG_PROFILE
#if DEBUG_PROFILE==2
	bcm4773_trans = 0;	   
	t1 = clock_get_us();
	profile_spi_sync.count[trans]++;
	profile_spi_sync.total_time[trans] += (t1-t0);
#endif
#endif	

	if( status )
		return 0xFF;
	else {
#ifdef DEBUG_TRANSFER
		the_dir[0] = cmd_stat & SSI_READ_TRANS ? 'R':'r';
		cmd_stat = bcm4773_read_msg->cmd_stat;  //bcm4773_write_msg->data[0];
		pk_log(bport->port.dev,the_dir, cmd_stat, (char *)&bcm4773_read_msg->data[0], length, length, status);
#endif
		return  BCM4773_RET(bcm4773_read_msg);
	}
}

static bcm4773_stat_t bcm4773_read( struct bcm4773_uart_port *bcm_port, unsigned char cmd_stat, int length )
{
	unsigned char	*write_buf=bcm4773_get_write_buf( bcm_port );

	memset( write_buf, 0, length );

#ifdef CONFIG_SPI_BCM4773_DMA
	return bcm4773_transfer( bcm_port, cmd_stat, length, (length > 1 ? 1 : 0) );
#else
	return bcm4773_transfer( bcm_port, cmd_stat, length, 0 );
#endif
}

static inline bcm4773_stat_t bcm4773_write( struct bcm4773_uart_port *bcm_port, unsigned char cmd_stat, int length)
{
#ifdef CONFIG_SPI_BCM4773_DMA
	return bcm4773_transfer( bcm_port, cmd_stat, length, (length > 1 ? 1 : 0) );
#else
	return bcm4773_transfer( bcm_port, cmd_stat, length, 0 );
#endif
}

static bcm4773_stat_t bcm4773_reset_fifo( struct bcm4773_uart_port *bcm_port )
{
    int i=0;
    bcm4773_stat_t retval;
    int			   length = 0;
    do {
       i++;
       retval = bcm4773_read_transmit( bcm_port, &length );
    } while (i < 3 );

    return 0;
}


static inline bcm4773_stat_t bcm4773_read_status( struct bcm4773_uart_port *bcm_port )
{
	return bcm4773_read( bcm_port, SSI_READ_TRANS | SSI_MODE_HALF_DUPLEX, 1 );
}

static bcm4773_stat_t bcm4773_read_transmit( struct bcm4773_uart_port *bcm_port, int *length )
{
	bcm4773_stat_t retval = bcm4773_read_status( bcm_port );
	bcm4773_stat_t status = BCM4773_MSG_STAT(retval);

#ifdef DEBUG_TRANSFER__READ_TRANSMIT
	pr_debug(PFX "[SSPBBD]:  %s: retval=0x%04X ( len %d , stat = 0x%02X )\n", __func__,retval,BCM4773_MSG_LEN(retval),status);
#endif

	if( status == 0 ) 
	{
		*length=BCM4773_MSG_LEN(retval);
		if( *length != 0 ) 
		{
			int len2;
			// should not read 255 bytes long because it'll trigger buggy DMA because 255 + 2(SSI hdr) > 256
			if (*length == 255) {
				*length = 254;
			}
			retval=bcm4773_read( bcm_port, SSI_READ_TRANS | SSI_MODE_HALF_DUPLEX, *length+1); //KOM:: FIXME It doesn't matter what we are using HALF or FULL DUPLEX
			len2 = BCM4773_MSG_LEN(retval);
			if ( len2 < *length ) {
				struct uart_port *port=&(bcm_port->port);
				pr_err(PFX "[SSPBBD]: %s error reading from the chipset! Read %d, expected %d\n", __func__,len2,*length);
				*length = len2;
			   port->icount.brk++;
		  }
			/* Keep using length in bcm4773_read_status for return value (*length) not length in next bcm4773_read because
			next length in bcm4773_read may be > length of previous read in bcm4773_read_status */
		}
	}

	return retval;
}

static bcm4773_stat_t bcm4773_write_receive( struct bcm4773_uart_port *bcm_port, int n, int *length )
{
	bcm4773_stat_t retval = bcm4773_write( bcm_port, SSI_WRITE_TRANS | SSI_MODE_FULL_DUPLEX, n);
	
	/* See comment in bcm4773_read_transmit
	   Don't need to check status because "retval" is returned 
	   bcm4773_stat_t status = BCM4773_MSG_STAT(retval);
	if( status == 0 )
	*/
	{
	   int len2 = BCM4773_MSG_LEN(retval);
	   if ( n > len2)
	     *length = len2;
	   else 
	     *length = n; 
	}
	
	return retval;
}


static irqreturn_t bcm4773_irq_handler( int irq, void *pdata )
{
	struct bcm4773_uart_port	*bport=(struct bcm4773_uart_port *) pdata;

	/* [FW4773-413] Check if we really have pending irq */
	bcm4773_stat_t  status = gpio_get_value(bport->host_req_pin);
	if (!status)
		return IRQ_HANDLED;

#ifdef DEBUG_IRQ_HANDLER
	// FIXME Do we need to check HOST_REQ is set or not ???
	bcm4773_stat_t	status = gpio_get_value(bport->host_req_pin);

	struct uart_port		*port=&(bport->port);
	pr_debug(PFX "[SSPBBD]: %s.RXTX_WORK hq %d, rx %d\n",__func__,status,bport->rx_enabled);
#endif
	spin_lock(&bport->irq_lock);
	if (atomic_xchg(&bport->irq_enabled, 0)) {
		struct irq_desc	*desc = irq_to_desc(bport->port.irq);
		if (desc->depth!=0) {
			printk("[SSPBBD]: %s irq depth mismatch. enabled irq has depth %d!\n", __func__, desc->depth);
			desc->depth = 0;
		}
		disable_irq_nosync(bport->port.irq);
	}
	spin_unlock(&bport->irq_lock);

	queue_work(bport->serial_wq, &bport->rxtx_work);

	return IRQ_HANDLED;
}

static void bcm4773_insert_flip_string(struct uart_port	*port, struct bcm4773_uart_port	*bport,int length,const char *func,long line)
{
	int count;

	if( port->state->port.tty != NULL && length > 0 )
	{
		unsigned char *data = bcm4773_get_read_buf( bport);
#ifndef CONFIG_MACH_UNIVERSAL5420  // T-Phone		
		count=tty_insert_flip_string(&port->state->port, data + 1, length );     /* 1 means excluding RX_PKT_LEN */
#else	                           // H-Phone
		count=tty_insert_flip_string(port->state->port.tty, data + 1, length );
#endif		
		if( count < length )
		{
			pr_err(PFX "[SSPBBD]: tty_insert_flip_string input overrun error by (%i - %i) = %i bytes!\n", length, count, length - count );
			port->icount.buf_overrun+=length - count;
		}
		port->icount.rx+=count;
#ifndef CONFIG_MACH_UNIVERSAL5420  // T-Phone		
		tty_flip_buffer_push(&port->state->port);
#else	                           // H-Phone
		tty_flip_buffer_push(port->state->port.tty);
#endif		

#ifdef DEBUG_INSERT_FLIP_STRING
		pr_debug(PFX "[SSPBBD]: %s inserts %d bytes from %s(%ld)\n",__func__,length,func,line);
#endif
	}
}


static void bcm4773_rxtx_work( struct work_struct *work )
{
	struct bcm4773_uart_port	*bport=container_of( work, struct bcm4773_uart_port, rxtx_work );
	struct uart_port		*port=&(bport->port);
	int				length = 0;
	int				pending;
	int				count;
	bcm4773_stat_t			status;
	unsigned long int		flags;
	int				loop_count=0;
	
#ifdef DEBUG_PROFILE
	t0=clock_get_us();
	count = (bport->tx_enabled-1) % 2;      // rxtx_work_profile
	profile_rxtx_work.rxtx_work[count]++;
	profile_rxtx_work.rxtx_enter[count]++;
#endif	

	status = gpio_get_value(bport->host_req_pin);
	pending=uart_circ_chars_pending( &port->state->xmit );
	
#ifdef DEBUG_TRANSFER__RXTX_WORK
	pr_debug(PFX "[SSPBBD]: %s.START pn %d, hq %d, rx %d, tx %d, oe %d\n",__func__,pending,status,bport->rx_enabled,bport->tx_enabled,port->icount.buf_overrun);
#endif
	if (bcm4773_hello(bport)) {
	
		/* Drain the TX/RX buffer in a loop equally. */
		do
		{
			if ( status ) 
			{
				status=bcm4773_read_transmit( bport, &length );

				if( BCM4773_MSG_STAT(status) != 0x00 )
				{ 
					pr_err(PFX "[SSPBBD]: bcm4773_read_transmit buffer error! retval=0x%04X, port->irq = %d\n", status,port->irq);

					break;  //FIXME: return ???
					//spin_lock_irqsave( &(bport->lock), flags );
					//if( bport->tx_enabled )
					//	bport->tx_enabled=0;
					//if( bport->rx_enabled )
					//	enable_irq( port->irq );
					//spin_unlock_irqrestore( &(bport->lock), flags );
					//return;

				} else if ( BCM4773_MSG_LEN(status) != 0x00 ) {

					bcm4773_insert_flip_string(port,bport,length,__func__,__LINE__);
				}

#ifndef CONFIG_SPI_NO_FULL_DUPLEX
				status = gpio_get_value(bport->host_req_pin);   /* Keep checking HOST_REQ for the following write operation in Full Duplex Mode. */
#else
				status = 0;	   
#endif		   
			}

			if( pending != 0 )
			{
				/* We need to update the buffer first, as it can change while we work with it. */
				spin_lock_irqsave(&port->lock, flags);
				if( pending > BCM4773_MSG_BUF_SIZE ) pending=BCM4773_MSG_BUF_SIZE;

				count=CIRC_CNT_TO_END( port->state->xmit.head, port->state->xmit.tail, UART_XMIT_SIZE );
				if( pending > count ) pending=count;

				/* Select Half or Full Duplex mode for writing, because maximum size for Full Duplex is 255 bytes */
				/* Where: 1 means TX_PKT_LEN byte for Full Duplex mode */

#ifndef CONFIG_SPI_NO_FULL_DUPLEX
				//length = pending > SSI_MAX_RW_BYTE_COUNT ? 0:1; 
				length = 1;
#else
				length = 0; 
#endif			
				if ( pending > SSI_MAX_RW_BYTE_COUNT ) {  
					if ( status ) {                        /* Checking HOST_REQ (status) also if data is available to read. */ 
						pending = SSI_MAX_RW_BYTE_COUNT;
					} else  {
#ifdef CONFIG_SPI_DMA_BITS_PER_WORD
						pending = pending - (pending % (CONFIG_SPI_DMA_BYTES_PER_WORD * WORD_BURST_SIZE)) - 1; // "-1" because we need align at 16 or 32 bound and we have extra byte "cmd_stat"
#endif
						length = 0;
					}
				} 			 

				/* Copy the data. */
				memcpy( bcm4773_get_write_buf( bport ) + length, port->state->xmit.buf + port->state->xmit.tail, pending );

				/* Update the circular buffer. */
				port->state->xmit.tail=(port->state->xmit.tail + pending) & (UART_XMIT_SIZE - 1);
				spin_unlock_irqrestore(&port->lock, flags);

				if ( length == 0 ) {
					/* Write the data in Half Duplex mode */
					status=bcm4773_write( bport, SSI_WRITE_TRANS | SSI_MODE_HALF_DUPLEX, pending);
					status = BCM4773_MSG_STAT(status);
				} else {
					/* Write/Read the data in Full Duplex mode */
					status=bcm4773_write_receive( bport, pending, &length );
				}

				if( BCM4773_MSG_STAT(status) == 0x00 )
				{
					/* Transmission successfull. Update the TX count. */
					port->icount.tx+=pending;

					//FIXED: Full Duplex issue,  length = BCM4773_MSG_LEN(status);
					if ( length != 0 )
						bcm4773_insert_flip_string(port,bport,length,__func__,__LINE__);
				}
				else
				{
					/* Very unlikely that we get here.... */
					port->icount.overrun+=pending;
				}
			}

			status = gpio_get_value(bport->host_req_pin);
			pending=uart_circ_chars_pending( &port->state->xmit );

#ifdef DEBUG_TRANSFER__RXTX_WORK_LOOP
			pr_debug(PFX "[SSPBBD]: %s.LOOP pending %d, host_req %d, rx %d,  tx %d\n",__func__,pending,status,bport->rx_enabled,bport->tx_enabled);
#endif


			// check how much we running
			loop_count++;

			if (loop_count >= 100) {
				if ((loop_count % 100) == 0) {
					printk("[SSPBBD]: Warning! looping more than %d in a run. status = %d, pending = %d. Enforcing SSI dump \n", loop_count, status, pending);
					ssi_dbg = true;
				}
				else 
					ssi_dbg = false;
			}
//		} while( ( pending || status) && port->state->port.tty && !port->state->port.tty->closing);
		} while( ( pending || status));

		ssi_dbg = false;

		bcm4773_bye(bport);
	}
	else {
		pr_err("[SSPBBD]: %s timeout!!\n", __func__);
	}

	pending=uart_circ_chars_pending( &port->state->xmit );
	if( (pending < WAKEUP_CHARS) && (port->state->port.tty != NULL) )
		tty_wakeup( port->state->port.tty );

	spin_lock_irqsave( &(bport->lock), flags );


#ifdef DEBUG_TRANSFER__RXTX_WORK
	pr_debug(PFX "[SSPBBD]: %s.EXIT pn %d, hq %d, rx %d, tx %d, oe %d\n",__func__,pending,status,bport->rx_enabled,bport->tx_enabled,port->icount.buf_overrun);
#endif

#ifdef DEBUG_PROFILE
	t1 = clock_get_us();
	profile_rxtx_work.count[0]++;
	profile_rxtx_work.total_time[0] += (t1-t0);
	count = (bport->tx_enabled-1) % 2;      // rxtx_work_profile
	profile_rxtx_work.rxtx_enter[count]--;
#endif
	spin_unlock_irqrestore( &(bport->lock), flags );

	spin_lock_irqsave( &bport->irq_lock, flags);
	if (!atomic_read(&bport->suspending))	// we dont' want to enable irq when going to suspendq
	{
		/* Let irq happen again */
		if (!atomic_xchg(&bport->irq_enabled, 1)) {
			struct irq_desc	*desc = irq_to_desc(bport->port.irq);
			if (desc->depth!=1) {
				printk("[SSPBBD]: %s irq depth mismatch. disabled irq has depth %d!\n", __func__, desc->depth);
				desc->depth = 1;
			}
			enable_irq(bport->port.irq);
		}
	}
	spin_unlock_irqrestore( &bport->irq_lock, flags);
	return;
}


#ifndef CONFIG_SPI_NO_AUTOBAUD
static void bcm4773_autobaud(struct bcm4773_uart_port *bport)
{
	struct spi_device *spidev = bcm4773_uart_to_spidevice( bport );

	unsigned char autobaud[16] = {
	    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
	};
	unsigned char dummy[16];
	struct spi_message msg;
	struct spi_transfer transfer = {
		.tx_buf = autobaud,
		.rx_buf = dummy,
		.len = 16,
	};

	spi_message_init(&msg);
	msg.spi = spidev;
	spi_message_add_tail(&transfer, &msg);

	if (bcm4773_hello(bport)) {
		spi_sync(spidev, &msg);
		bcm4773_bye(bport);
	}
	else {
		pr_err("[SSPBBD]: %s timeout!!\n", __func__);
	}
}
#endif

#ifndef CONFIG_SPI_NO_IDENTIFY
static int bcm4773_do_identify( struct bcm4773_uart_port *bcm_port )
{
	const char			ident_write[]= { //0x00, // Space is for TX_PKT_LEN for Full Duplex Mode
			                             0xB0, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x37, 0xb0, 0x01
	                                   };
	const char			ident_read[] = {
			                             0xB0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x8F, 0xB0, 0x01, 0xB0,
										 0x00, 0x01, 0x06, 0x00, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00,
			                             0x20, 0xB0, 0x01
	                                   };

	char				 buffer[255+1];
	int				     length=0,n=0,n2;
	bcm4773_stat_t status;

	/* Do identify. */
	memcpy( bcm4773_get_write_buf( bcm_port ) + 1, ident_write, sizeof( ident_write ) );  // 1 means RX_PKT_LEN for Full Duplex Mode
	status=bcm4773_write_receive( bcm_port, sizeof( ident_write ), &length );

	if( BCM4773_MSG_STAT(status) != 0x00 )
	{
		pr_err(PFX "[SSPBBD]: Error writing identify! Status %.2X\n", status );
		return -ENODEV;
	}
	//FIXED: Full Duplex issue, length = BCM4773_MSG_LEN(status);
	if ( length > 0 ) {
	    memcpy( buffer, bcm4773_get_read_buf( bcm_port ) + 1, length);  // 1 means RX_PKT_LEN for both modes
	}
	n = length;

	mdelay( 10 );
	status=bcm4773_read_transmit( bcm_port, &length );
	if( BCM4773_MSG_STAT(status) != 0x00 )
	{
		pr_err(PFX "[SSPBBD]: Error receiving identify result! Status %.2X\n", status );
		return -ENODEV;
	}

	n2 = BCM4773_MSG_LEN(status);
	length = sizeof(buffer) - n;
	if ( length > n2 )
	   length = n2;
	
	if ( length > 0 ) {
	  memcpy( &buffer[n], bcm4773_get_read_buf( bcm_port ) + 1, length);  // 1 means RX_PKT_LEN for both modes
	  length +=n;
	}
	
    length +=n;

	if( (length != sizeof( ident_read )) || memcmp( ident_read, buffer, length ) )
	{
		pr_err(PFX "[SSPBBD]: Ident data received is not correct!\n" );
		for( length=0; length < sizeof( ident_read ); length++ )
			//pr_notice(PFX "Read[%i]: %.2X, Expect: %.2X\n", length, buffer[length], ident_read[length] );
			pr_notice(PFX "[SSPBBD]: SPI Data Compare passed for byte           %2d. Exp = %.2X, Act = %.2X\n",length,ident_read[length],buffer[length]);
						
		pr_err(PFX "[SSPBBD]: Length received: %i, expected %u\n", n + n2, sizeof( ident_read ) );
		return -ENODEV;
	}

	pr_notice(PFX "[SSPBBD]: Identify success!\n" );
	pr_notice(PFX "[SSPBBD]: Received: " );
	for( length=0; length < sizeof( ident_read ); length++ )
		pr_notice( "%.2X ", buffer[length] );
	pr_notice(PFX "\n[SSPBBD]: Status: %.2X\n", status );
	return 0;
}
#endif

#if 0
static int bcm4773_notifier(struct notifier_block *nb, unsigned long event, void * data);
static struct notifier_block bcm4773_notifier_block = {
        .notifier_call = bcm4773_notifier,
};
#endif

static int bcm4773_spi_probe( struct spi_device *spi )
{
	struct bcm4773_uart_port	*bcm4773_uart_port;
	int				retval;
	struct ssp_platform_data  	dflt;
	
	struct ssp_platform_data *pdata = spi->dev.platform_data;
	if (!pdata) {
		pr_warning(PFX "[SSPBBD]: Platform_data null has been provided. Use local allocation.\n");
		pdata = &dflt; 
		if (!spi->dev.of_node) {
			pr_err(PFX "[SSPBBD]: Failed to find of_node\n");
			return -1;
		}
	}

#ifdef SUPPORT_MCU_HOST_WAKE
	if (of_property_read_u32(spi->dev.of_node, "ssp-hw-rev", &gpbbd_dev->hw_rev)) {
		/* default value is zero(open) for old hw */
		gpbbd_dev->hw_rev = 0;
	}
	printk("[SSPBBD]: %s ssp-hw-rev[%d]\n", __func__, gpbbd_dev->hw_rev);
#endif

	/*   All the gpio pins SCLK,CS0,MOSI,MISO and HOST_REQ should be defined in arch/arm/boot/dts/exynos5430-kqlte_eur_open_00.dts
	 *   The fake HOST_REQ pin is commented out.
	 *   pdata->mcu_int1 = of_get_named_gpio(spi->dev.of_node, "ssp-irq2", 0);
	 */
#ifndef CONFIG_MACH_UNIVERSAL5420  // T-Phone		
	pdata->mcu_int1 = of_get_named_gpio(spi->dev.of_node, "ssp-host-req", 0);
#else	                           // H-Phone
	//FIXME: Because JK modified something in android/kernel/exynos5420/arch/arm/mach-exynos  
	pdata->mcu_int1 = GPIO_MCU_HOST_REQ;
	//pdata->mcu_int1 = 205;
#endif	
		
		/* We don't need to setup the gpio HOST_REQ pin "mcu_int1"
		 *  - gpio_request(pdata->mcu_int1, "mcu_ap_int1");
		 *  - gpio_export(pdata->mcu_int1,1);
		 *  - gpio_direction_input(pdata->mcu_int1);
		 */

	if (pdata->mcu_int1<0) {
		pr_err(PFX "[SSPBBD]: Failed to get mcu_ap_int1 from DT, err %d\n",pdata->mcu_int1);
		return -1;
	}

	spi->irq = gpio_to_irq(pdata->mcu_int1);
	if ( spi->irq < 0 ) {
		pr_err(PFX "[SSPBBD]: Failed to get mcu_ap_int1 gpio %d, err %d\n",pdata->mcu_int1,spi->irq);
		return -1;
	}
		
	if (pdata->mcu_int1 <0)
	{
		pr_err(PFX "[SSPBBD]: Failed to find ssp-host-req in of_node\n");
		return -1;
	}
	pr_debug(PFX "[SSPBBD]: OK, found ssp-host-req %d ", pdata->mcu_int1);
		
	/* Allocate memory for the private driver data. */
	bcm4773_uart_port=(struct bcm4773_uart_port *) kmalloc( sizeof( struct bcm4773_uart_port ), GFP_ATOMIC );

	pr_notice(PFX "[SSPBBD]: %s SPI/SSI UART Driver v"GPS_VERSION", pdata->gpio_spi = %d, spi->irq = %d, ac_data = 0x%p\n",__func__, 
			pdata->mcu_int1,spi->irq, bcm4773_uart_port);

	if( bcm4773_uart_port == NULL )
	{
		pr_err(PFX "[SSPBBD]: Failed to allocate memory for the BCM4773 SPI-UART driver.\n");
		return -ENOMEM;
	}
	else memset( bcm4773_uart_port, 0, sizeof( struct bcm4773_uart_port ) );

	/* Initialize the structure. */
	spin_lock_init( &(bcm4773_uart_port->port.lock) );
	spin_lock_init( &(bcm4773_uart_port->lock) );
	spin_lock_init( &(bcm4773_uart_port->irq_lock) );
	bcm4773_uart_port->port.iotype=UPIO_MEM;
	bcm4773_uart_port->port.irq=spi->irq;
	bcm4773_uart_port->port.custom_divisor=1;
	bcm4773_uart_port->port.fifosize=256;        // It doesn't matter fifosize for us
	bcm4773_uart_port->port.ops=&bcm4773_serial_ops;
	bcm4773_uart_port->port.flags=UPF_BOOT_AUTOCONF;
	bcm4773_uart_port->port.type=PORT_16550A;    // It doesn't matter what serial type is used
	bcm4773_uart_port->port.dev=&(spi->dev);

	/* Allocate memory for the transfer buffer. */
#ifdef CONFIG_SPI_BCM4773_DMA
	spi->dev.coherent_dma_mask = 0xffffffffUL;

	bcm4773_uart_port->master_transfer.rx_buf=dma_alloc_coherent( &(spi->dev), sizeof( struct bcm4773_message ),
								      &(bcm4773_uart_port->master_transfer.rx_dma), GFP_KERNEL | GFP_DMA );
	bcm4773_uart_port->master_transfer.tx_buf=dma_alloc_coherent( &(spi->dev), sizeof( struct bcm4773_message ),
								      &(bcm4773_uart_port->master_transfer.tx_dma), GFP_KERNEL | GFP_DMA );
	bcm4773_uart_port->dbg_transfer.rx_buf=dma_alloc_coherent( &(spi->dev), sizeof( struct bcm4773_message ),
								      &(bcm4773_uart_port->dbg_transfer.rx_dma), GFP_KERNEL | GFP_DMA );
	bcm4773_uart_port->dbg_transfer.tx_buf=dma_alloc_coherent( &(spi->dev), sizeof( struct bcm4773_message ),
								      &(bcm4773_uart_port->dbg_transfer.tx_dma), GFP_KERNEL | GFP_DMA );
#else
	bcm4773_uart_port->master_transfer.tx_buf=kmalloc( sizeof( struct bcm4773_message ), GFP_KERNEL );
	bcm4773_uart_port->master_transfer.rx_buf=kmalloc( sizeof( struct bcm4773_message ), GFP_KERNEL );
	bcm4773_uart_port->master_transfer.tx_dma=0;
	bcm4773_uart_port->master_transfer.rx_dma=0;
	bcm4773_uart_port->dbg_transfer.tx_buf=kmalloc( sizeof( struct bcm4773_message ), GFP_KERNEL );
	bcm4773_uart_port->dbg_transfer.rx_buf=kmalloc( sizeof( struct bcm4773_message ), GFP_KERNEL );
	bcm4773_uart_port->dbg_transfer.tx_dma=0;
	bcm4773_uart_port->dbg_transfer.rx_dma=0;
#endif

	if( !bcm4773_uart_port->master_transfer.tx_buf || !bcm4773_uart_port->master_transfer.rx_buf ||
	    !bcm4773_uart_port->dbg_transfer.tx_buf || !bcm4773_uart_port->dbg_transfer.rx_buf )
	{
		pr_err(PFX "[SSPBBD]: Failed to allocate transfer buffer memory.\n" );
		if( bcm4773_uart_port->master_transfer.tx_buf )
#ifdef CONFIG_SPI_BCM4773_DMA
			dma_free_coherent( &(spi->dev), sizeof( struct bcm4773_message ), (void *) bcm4773_uart_port->master_transfer.tx_buf,
					   bcm4773_uart_port->master_transfer.tx_dma );
#else
			kfree( bcm4773_uart_port->master_transfer.tx_buf );
#endif
		if( bcm4773_uart_port->master_transfer.rx_buf )
#ifdef CONFIG_SPI_BCM4773_DMA
			dma_free_coherent( &(spi->dev), sizeof( struct bcm4773_message ), (void *) bcm4773_uart_port->master_transfer.rx_buf,
					   bcm4773_uart_port->master_transfer.rx_dma );
#else
			kfree( bcm4773_uart_port->master_transfer.rx_buf );
#endif
			kfree( bcm4773_uart_port );
			return -ENOMEM;
	}
	bcm4773_uart_port->master_transfer.speed_hz=0;  /* Setting 0 and SPI low driver will use .max_speed_hz from arch/arm/mach-omap2/board-omap4panda.c.
	                                                   Otherwise we can change speed_hz in bcm4773_set_termios() */
	bcm4773_uart_port->master_transfer.bits_per_word=8;
	bcm4773_uart_port->master_transfer.delay_usecs=0;

	bcm4773_uart_port->dbg_transfer.speed_hz=0; 
	bcm4773_uart_port->dbg_transfer.bits_per_word=8;
	bcm4773_uart_port->dbg_transfer.delay_usecs=0;

	/* Reset FIFO */
	bcm4773_reset_fifo( bcm4773_uart_port );

	/* Detect. */
#ifndef CONFIG_SPI_NO_IDENTIFY
    retval = bcm4773_do_identify( bcm4773_uart_port );
	if( retval < 0 )
	{
		pr_err(PFX "[SSPBBD]: Failed to identify BCM4773 chip! (Is the chip powered?)\n");
#ifdef CONFIG_SPI_BCM4773_DMA
		dma_free_coherent( &(spi->dev), sizeof( struct bcm4773_message ), (void *) bcm4773_uart_port->master_transfer.tx_buf,
				bcm4773_uart_port->master_transfer.tx_dma );
		dma_free_coherent( &(spi->dev), sizeof( struct bcm4773_message ), (void *) bcm4773_uart_port->master_transfer.rx_buf,
				bcm4773_uart_port->master_transfer.rx_dma );
#else
		kfree( bcm4773_uart_port->master_transfer.tx_buf );
		kfree( bcm4773_uart_port->master_transfer.rx_buf );
#endif
		kfree( bcm4773_uart_port );
		return -ENODEV;
	}
#endif	

	/* Register the serial driver. */
	retval=uart_register_driver( &bcm4773_uart_driver );
	if( retval )
	{
		pr_err(PFX "[SSPBBD]: Failed to register BCM4773 SPI-UART driver.\n");
#ifdef CONFIG_SPI_BCM4773_DMA
		dma_free_coherent( &(spi->dev), sizeof( struct bcm4773_message ), (void *) bcm4773_uart_port->master_transfer.tx_buf,
				bcm4773_uart_port->master_transfer.tx_dma );
		dma_free_coherent( &(spi->dev), sizeof( struct bcm4773_message ), (void *) bcm4773_uart_port->master_transfer.rx_buf,
				bcm4773_uart_port->master_transfer.rx_dma );
#else
		kfree( bcm4773_uart_port->master_transfer.tx_buf );
		kfree( bcm4773_uart_port->master_transfer.rx_buf );
#endif
		kfree( bcm4773_uart_port );
		return retval;
	}

	/* Register the one TTY device. */
	uart_add_one_port( &bcm4773_uart_driver, &(bcm4773_uart_port->port) );

	//set closing_wait 0. See jira FW4773-391
	bcm4773_uart_driver.state->port.closing_wait = ASYNC_CLOSING_WAIT_NONE;

	bcm4773_uart_port->host_req_pin = pdata->mcu_int1;
#ifndef CONFIG_MACH_UNIVERSAL5420  // T-Phone
	bcm4773_uart_port->mcu_req = of_get_named_gpio(spi->dev.of_node, "ssp-mcu-req", 0);
	bcm4773_uart_port->mcu_resp = of_get_named_gpio(spi->dev.of_node, "ssp-mcu-resp", 0);
#else                              // H-Phone
	bcm4773_uart_port->mcu_req = EXYNOS5420_GPX3(0);        // MCU_REQ      GPIO6 : EXYNOS_GPX3[0]
	bcm4773_uart_port->mcu_resp = EXYNOS5420_GPX3(3);       // MCU_REQ_RESP GPIO4 : EXYNOS_GPX3[3]
#endif

	/* Set the private info. */
	spi_set_drvdata( spi, bcm4773_uart_port );

	/* Setup the workqueue. */
	INIT_WORK( &(bcm4773_uart_port->rxtx_work), bcm4773_rxtx_work );
	INIT_WORK( &(bcm4773_uart_port->start_tx), bcm4773_start_tx_work );
	INIT_WORK( &(bcm4773_uart_port->stop_rx), bcm4773_stop_rx_work );
	INIT_WORK( &(bcm4773_uart_port->stop_tx), bcm4773_stop_tx_work );
	bcm4773_uart_port->serial_wq= alloc_workqueue("bcm4773_wq", WQ_HIGHPRI | WQ_UNBOUND | WQ_MEM_RECLAIM, 1);

	//FIXME: Do we need make this call here?
	pssp_driver->probe(spi);

	/* Request the interrupt. */
	retval=request_irq( bcm4773_uart_port->port.irq, bcm4773_irq_handler,
			IRQF_TRIGGER_HIGH,
			//IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			bcm4773_uart_driver.driver_name,
			bcm4773_uart_port );
	if( retval )
	{
		pr_err(PFX "[SSPBBD]: Failed to register BCM4773 SPI TTY IRQ %d.\n",bcm4773_uart_port->port.irq);
		uart_remove_one_port( &bcm4773_uart_driver, &(bcm4773_uart_port->port) );
		uart_unregister_driver( &bcm4773_uart_driver );
#ifdef CONFIG_SPI_BCM4773_DMA
		dma_free_coherent( &(spi->dev), sizeof( struct bcm4773_message ), (void *) bcm4773_uart_port->master_transfer.tx_buf,
				bcm4773_uart_port->master_transfer.tx_dma );
		dma_free_coherent( &(spi->dev), sizeof( struct bcm4773_message ), (void *) bcm4773_uart_port->master_transfer.rx_buf,
				bcm4773_uart_port->master_transfer.rx_dma );
#else
		kfree( bcm4773_uart_port->master_transfer.tx_buf );
		kfree( bcm4773_uart_port->master_transfer.rx_buf );
#endif
		kfree( bcm4773_uart_port );
		return retval;
	}

	g_bport = bcm4773_uart_port;

	// Init wakelock
	wake_lock_init(&bcm4773_uart_port->bcm4773_wake_lock, WAKE_LOCK_SUSPEND, "bcm4773_wake_lock");

#if 0
	// Register PM notifier
	register_pm_notifier(&bcm4773_notifier_block);
#endif
	/* Disable interrupts */
	pr_notice(PFX "[SSPBBD]: Initialized. irq = %d\n", bcm4773_uart_port->port.irq);
	disable_irq( bcm4773_uart_port->port.irq );
	atomic_set( &(bcm4773_uart_port->irq_enabled), 0 );

	atomic_set(&bcm4773_uart_port->suspending, 0);

#ifdef CONFIG_DDS
	/* Prepare MCU request/response */
	gpio_request(g_bport->mcu_req, "MCU REQ");
	gpio_direction_output(g_bport->mcu_req, 0);

	gpio_request(g_bport->mcu_resp, "MCU RESP");
	gpio_direction_input(g_bport->mcu_resp);
#endif
	return 0;
}

static inline struct bcm4773_uart_port* bcm4773_spi_get_drvdata( struct spi_device *spi )
{
	// TODO: This is a temporary solution. 
	// 	 We sometimes have wrong drv data from spi_get_drvdata.
	if (g_bport)
		return g_bport;
	else
		return (struct bcm4773_uart_port *) spi_get_drvdata( spi );
}

static int bcm4773_spi_remove( struct spi_device *spi )
{
	struct bcm4773_uart_port	*bport = bcm4773_spi_get_drvdata( spi );
	unsigned long int		flags;

	pr_notice(PFX "[SSPBBD]:  %s : called\n", __func__);

	spin_lock_irqsave( &(bport->lock), flags );

	bport->rx_enabled = 0;
	bport->tx_enabled = 0;

	spin_unlock_irqrestore( &(bport->lock), flags );

	flush_workqueue( bport->serial_wq );
	destroy_workqueue( bport->serial_wq );
	uart_remove_one_port( &bcm4773_uart_driver, &(bport->port) );
	uart_unregister_driver( &bcm4773_uart_driver );
#ifdef CONFIG_SPI_BCM4773_DMA
		dma_free_coherent( &(spi->dev), sizeof( struct bcm4773_message ), (void *) bport->master_transfer.tx_buf,
				bport->master_transfer.tx_dma );
		dma_free_coherent( &(spi->dev), sizeof( struct bcm4773_message ), (void *) bport->master_transfer.rx_buf,
				bport->master_transfer.rx_dma );
		dma_free_coherent( &(spi->dev), sizeof( struct bcm4773_message ), (void *) bport->dbg_transfer.tx_buf,
				bport->dbg_transfer.tx_dma );
		dma_free_coherent( &(spi->dev), sizeof( struct bcm4773_message ), (void *) bport->dbg_transfer.rx_buf,
				bport->dbg_transfer.rx_dma );
#else
	kfree( bport->master_transfer.rx_buf );		
	kfree( bport->master_transfer.tx_buf );
	kfree( bport->dbg_transfer.rx_buf );		
	kfree( bport->dbg_transfer.tx_buf );
#endif
	free_irq( spi->irq, bport );
	kfree( bport );
	g_bport = NULL;

	return 0;
}

static struct uart_ops	bcm4773_serial_ops=
{
        .tx_empty       = bcm4773_tx_empty,
        .get_mctrl      = bcm4773_get_mctrl,
        .set_mctrl      = bcm4773_set_mctrl,
        .stop_tx        = bcm4773_stop_tx,
        .start_tx       = bcm4773_start_tx,
        .stop_rx        = bcm4773_stop_rx,
        .enable_ms      = bcm4773_enable_ms,
        .break_ctl      = bcm4773_break_ctl,
        .startup        = bcm4773_startup,
        .shutdown       = bcm4773_shutdown,
        .set_termios    = bcm4773_set_termios,
        .type           = bcm4773_type,
        .release_port   = bcm4773_release_port,
        .request_port   = bcm4773_request_port,
        .config_port    = bcm4773_config_port,
        .verify_port    = bcm4773_verify_port,
};

static int bcm4773_suspend( struct spi_device *spi, pm_message_t state )
{
	struct bcm4773_uart_port *bport = bcm4773_spi_get_drvdata( spi );
	unsigned long int		flags;
#if 0
	struct bcm4773_gps_platform_data *pdata=spi->dev.platform_data;

	/* No need to suspend if nothing is running. */
	if( bport->rx_enabled || bport->tx_enabled )
		uart_suspend_port( &bcm4773_uart_driver, &(bport->port) );
#endif

	printk("[SSPBBD]: %s ++ \n", __func__);
	
	atomic_set(&bport->suspending, 1);


	spin_lock_irqsave( &bport->irq_lock, flags);
	if (atomic_xchg(&bport->irq_enabled, 0)) {
		struct irq_desc	*desc = irq_to_desc(bport->port.irq);
		if (desc->depth!=0) {
			printk("[SSPBBD]: %s irq depth mismatch. enabled irq has depth %d!\n", __func__, desc->depth);
			desc->depth = 0;
		}
		disable_irq_nosync(bport->port.irq);
	}
	spin_unlock_irqrestore( &bport->irq_lock, flags);

	if (pssp_driver->suspend)
		pssp_driver->suspend(spi, state);

	if (pssp_driver->driver.pm && pssp_driver->driver.pm->suspend)
		pssp_driver->driver.pm->suspend(&spi->dev);

	flush_workqueue(bport->serial_wq );
	//msleep(10);

	printk("[SSPBBD]: %s -- \n", __func__);
        return 0;
}

static int bcm4773_resume( struct spi_device *spi )
{
	struct bcm4773_uart_port *bport = bcm4773_spi_get_drvdata( spi );
	unsigned long int		flags;

#if 0 // Ted::TODO
	struct bcm4773_gps_platform_data *pdata=spi->dev.platform_data;

	if (pdata->resume)
		pdata->resume();

	/* If the port was suspended, resume now. */
	if( bport->port.suspended )
		uart_resume_port( &bcm4773_uart_driver, &(bport->port) );
#endif

	if (pssp_driver->driver.pm && pssp_driver->driver.pm->suspend)
		pssp_driver->driver.pm->resume(&spi->dev);

	if (pssp_driver->resume)
		pssp_driver->resume(spi);

	atomic_set(&bport->suspending, 0);

	/* Let irq happen again */
	spin_lock_irqsave( &bport->irq_lock, flags);
	if (!atomic_xchg(&bport->irq_enabled, 1)) {
		struct irq_desc	*desc = irq_to_desc(bport->port.irq);
		if (desc->depth!=1) {
			printk("[SSPBBD]: %s irq depth mismatch. disabled irq has depth %d!\n", __func__, desc->depth);
			desc->depth = 1;
		}
		enable_irq(bport->port.irq);
	}
	spin_unlock_irqrestore( &bport->irq_lock, flags);

	wake_lock_timeout(&g_bport->bcm4773_wake_lock, HZ/2);

	return 0;
}

#if 0
static int bcm4773_notifier(struct notifier_block *nb, unsigned long event, void * data)
{
	struct spi_device *spi = bcm4773_uart_to_spidevice(g_bport);
	pm_message_t state = {0};

        switch (event) {
		case PM_SUSPEND_PREPARE:
			printk("%s going to sleep", __func__);
			state.event = event;
			bcm4773_suspend(spi, state);
			break;
		case PM_POST_SUSPEND:
			printk("%s waking up", __func__);
			bcm4773_resume(spi);
			break;
	}
	return NOTIFY_OK;
}
#endif

int spi_suspending(void)
{
	if (!g_bport)
		return 1;
	return atomic_read(&g_bport->suspending);
}

/* Ted */
static void bcm4773_spi_shutdown(struct spi_device *spi)
{
	struct bcm4773_uart_port	*bport = bcm4773_spi_get_drvdata( spi );
	unsigned long int		flags;

	printk("[SSPBBD]: %s ++ \n", __func__);
	
	atomic_set(&bport->suspending, 1);

	spin_lock_irqsave( &bport->irq_lock, flags);
	if (atomic_xchg(&bport->irq_enabled, 0)) {
		struct irq_desc	*desc = irq_to_desc(bport->port.irq);
		if (desc->depth!=0) {
			printk("[SSPBBD]: %s irq depth mismatch. enabled irq has depth %d!\n", __func__, desc->depth);
			desc->depth = 0;
		}
		disable_irq_nosync(bport->port.irq);
	}
	spin_unlock_irqrestore( &bport->irq_lock, flags);

	pssp_driver->shutdown(spi);

	bbd_tty_close();
	printk("[SSPBBD]: %s ** \n", __func__);

	flush_workqueue(bport->serial_wq );
	destroy_workqueue( bport->serial_wq );

	printk("[SSPBBD]: %s -- \n", __func__);
}


static const struct spi_device_id gpsspi_id[] = {
	{"ssp-spi", 0},
    {}
};

#ifdef CONFIG_OF
static struct of_device_id ssp_match_table[] = {
	{ .compatible = "ssp,BCM4773",},
	{},
};
#endif

static struct spi_driver	bcm4773_spi_driver=
{
	.id_table       = gpsspi_id,
	.probe			= bcm4773_spi_probe,
	.remove			= bcm4773_spi_remove,
#if 1
	.suspend		= bcm4773_suspend,
	.resume			= bcm4773_resume,
#endif
	.shutdown		= bcm4773_spi_shutdown,


	.driver=
	{
		.name		= "ssp",
		.owner		= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ssp_match_table
#endif
	},
};



#ifdef DEBUG_PROFILE
#define THOUSAND(p)     (1000 * (p))
#define MILLION(p)     (THOUSAND(THOUSAND(p)))
unsigned long clock_get_us(void)
{
    unsigned long ulMicroseconds;
    struct timeval tv;
	do_gettimeofday(&tv);
    ulMicroseconds = ((unsigned long) tv.tv_sec) * (unsigned long) MILLION(1);
    ulMicroseconds += (unsigned long) tv.tv_usec;
    return ulMicroseconds;
}
EXPORT_SYMBOL_GPL(clock_get_us);
#endif


#ifdef DEBUG_TRANSFER
static unsigned long init_time = 0;
static unsigned long clock_get_ms(void)
{
        struct timeval t;
        unsigned long now;

        do_gettimeofday(&t);
        now = t.tv_usec / 1000 + t.tv_sec * 1000;
        if ( init_time == 0 )
        init_time = now;

        return now - init_time;
}


static void pk_log(const struct device *dev,char* dir,unsigned char cmd_stat, char* data, int len, int transferred, short status)
{
	if (likely(!ssi_dbg))
		return;

	//if(/* ( m_ulLogFlag & LOG_DUMP) && */ m_hLog )
	{
        char acB[960];
		char *p = acB;
        char the_dir[10];
		int  the_trans = transferred;
		int i,j,n;
		bool w;

		char ic = len == transferred ? 'D':'E';
		char xc = dir[0] == 'r' || dir[0] == 'w' ? 'x':'X';
		strcpy(the_dir, dir);

		//if ( len != transferred ) {
		if ( the_trans == 0 )  the_dir[0] = xc;

		//FIXME: There is print issue. Printing 7 digits instead of 6 when clock is over 1000000. "% 1000000" added
		// E.g.
        //#999829D w 0x68,     1: A2
        //#999829D r 0x68,    34: 8D 00 01 52 5F B0 01 B0 00 8E 00 01 53 8B B0 01 B0 00 8F 00 01 54 61 B0 01 B0 00 90 00 01 55 B5
        //         r              B0 01
        //#1000001D w 0x68,     1: A1
        //#1000001D r 0x68,     1: 00
		n = len ? len+1:0;
		p += snprintf(acB,sizeof(acB),"#%06ld%c %s 0x%02X, %5d: %02X ",
				       clock_get_ms() % 1000000,ic, the_dir, cmd_stat,n,cmd_stat); // Where 'D' means droped data, cmd_stat instead of "data[0] & 0xff"

		w = dir[0] == 'w' || dir[0] == 'W';
		i = 0;

		for(n=31; i < len; i = j,n=32)
		{
			for(j = i; j < (i + n) && j < len && the_trans != 0; j++,the_trans--) {
				p += snprintf(p,sizeof(acB) - (p - acB), "%02X ", data[j] & 0xFF);
			}
			dev_dbg(dev,"%s\n",acB); //pr_debug
			if(j < len) {
                p = acB;
				if ( the_trans == 0 )  { the_dir[0] = xc; the_trans--; }
				p += snprintf(acB,sizeof(acB),"         %2s              ",the_dir);
			}
		}

        if ( i == 0 )
        	dev_dbg(dev,"%s\n",acB);  //pr_debug  //

		//fflush(m_hLog);
	}

	if(len != transferred) {
        pr_err("error, stat %d trans'd %d\n", status,transferred);
	}
}
#endif /* DEBUG */


unsigned char* bcm477x_debug_buffer(size_t* len)
{
    if (!g_bport) {
        return 0;
    }

    return bcm4773_debug_buffer(g_bport, len);
}
EXPORT_SYMBOL(bcm477x_debug_buffer);



int bcm477x_debug_write(const unsigned char* buf, size_t len, int flag)
{
    if (!g_bport) {
        return 0;
    }
//    return bcm4773_debug_transfer(g_bport, buf, NULL, len);
    return bcm4773_debug_transfer(g_bport, buf, len);
}
EXPORT_SYMBOL(bcm477x_debug_write);


int bcm477x_debug_transfer(unsigned char *tx, unsigned char *rx, int len, int flag)
{
    if (!g_bport) {
        return 0;
    }
//    return bcm4773_debug_transfer(g_bport, tx, rx, len);
    return bcm4773_debug_transfer(g_bport, tx, len);
}
EXPORT_SYMBOL(bcm477x_debug_transfer);


static int __init bcm4773_spi_init( void )
{
	return spi_register_driver( &bcm4773_spi_driver );
}

static void __exit bcm4773_spi_exit( void )
{
	spi_unregister_driver( &bcm4773_spi_driver );
	return;
}

module_init( bcm4773_spi_init );
module_exit( bcm4773_spi_exit );

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BCM4773 SPI/SSI UART Driver");

