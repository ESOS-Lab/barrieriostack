#ifndef _BCM477x_SSI_SPI_H_
#define _BCM477x_SSI_SPI_H_



//KOM FIXME: Compilation Bugs fixed. Coppied from bcm477x_ssi_spi.c#1
#define MAX_TX_RX_BUF_SIZE (2048 + 2)

enum
{
    SSI_MODE_STREAM = 0
    ,SSI_MODE_DEBUG  = 0x80
};

enum
{
    SSI_MODE_HALF_DUPLEX = 0
    ,SSI_MODE_FULL_DUPLEX = 0x40
};

enum
{
    SSI_WRITE_TRANS = 0x0
    ,SSI_READ_TRANS  = 0x20
};


struct bcm477x_platform_data {
    unsigned int host_req_gpio; /* HOST_REQ : to indicate that ASIC has data to send. */
};

typedef int (*bcm477x_on_read_cb)(unsigned char *buf, size_t len);

struct bcm477x_ssi_spi
{
    bool is_opened;
    bool is_driver_loaded;
    struct spi_device *spi;

    bcm477x_on_read_cb on_read;
};

extern int bcm477x_open(bcm477x_on_read_cb on_read);
extern void bcm477x_close(void);
extern int bcm477x_write(const unsigned char* buf, size_t len, int flag);

#endif  //_BCM477x_SSI_SPI_H_
