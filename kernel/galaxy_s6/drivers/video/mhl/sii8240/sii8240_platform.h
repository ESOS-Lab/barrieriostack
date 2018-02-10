
#include <linux/i2c.h>
#include <linux/sii8240.h>
#include "../../video/edid.h"

struct sii8240_platform_data *platform_init_data(struct i2c_client *client);
/*
int platform_ap_hdmi_hdcp_auth(struct sii8240_data *sii8240);
void platform_mhl_hpd_handler(bool value);
bool platform_hdmi_hpd_status(void);
*/
