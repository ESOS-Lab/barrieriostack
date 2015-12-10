#ifndef _BCM_GPS_SPI_H_
#define _BCM_GPS_SPI_H_


typedef unsigned short bcm4773_stat_t;
#define BCM4773_MSG_STAT(w) (w & 0xFF)
#define BCM4773_MSG_LEN(w)  ((w & 0xFF00)>>8)
#define BCM4773_RET(msg)    ((bcm4773_stat_t)msg->cmd_stat | ((bcm4773_stat_t)msg->data[0] << 8))

enum
{
    SSI_HW_FIFO_DEPTH = 128
    ,SSI_MAX_RW_BYTE_COUNT = 255
    ,SSI_OUTPUT_BUFFER_SIZE = 10000   // buffer size for the output. That should match what the APP sends
	,SSI_INPUT_BUFFER_SIZE = 20000    // buffer size for the input. That should match what the APP sends
    ,SSI_OVERLAP_BUFFER_SIZE = 1024   // partial buffer to hold data while overlap operation completes
	,SSI_INPUT_ROUTINE_BUFFER_SIZE = 2000
};

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


//struct bcm_gps_platform_data {
struct bcm4773_gps_platform_data {
	unsigned int gpio_spi; /* HOST_REQ : to indicate that ASIC has data to send. */
	void (*suspend) (void);
	void (*resume)  (void);
};

#endif  //_BCM_GPS_SPI_H_
