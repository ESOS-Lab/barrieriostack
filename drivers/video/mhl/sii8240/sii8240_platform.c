#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/sii8240.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/sec_batt.h>
#include <linux/battery/sec_charger.h>
#include "sii8240_driver.h"
#include <linux/ctype.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include "sii8240_driver.h"
#include "sii8240_platform.h"

struct sii8240_platform_data *g_pdata;

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#define MHL_LDO1_8 "VCC_1.8V_MHL"
#define MHL_LDO1_2 "VSIL_1.2V"
#else
#define MHL_LDO1_8 "VCC_1.8V_MHL"
#define MHL_LDO3_3 "VCC_3.3V_MHL"
#define MHL_LDO1_2 "VSIL_1.2A"
#endif

#ifdef CONFIG_CHARGER_MAX77823
#define POWER_SUPPLY_NAME "max77823-charger"
#else
#define POWER_SUPPLY_NAME "sec-charger"
#endif

static bool mhl_power_on;

static void muic_mhl_cb(bool otg_enable, int plim)
{
	struct sii8240_platform_data *pdata = g_pdata;
	union power_supply_propval value;
	int i, ret = 0;
	struct power_supply *psy;
	pdata->charging_type = POWER_SUPPLY_TYPE_MISC;

	if (plim == 0x00)
		pdata->charging_type = POWER_SUPPLY_TYPE_MHL_500;
	else if (plim == 0x01)
		pdata->charging_type = POWER_SUPPLY_TYPE_MHL_900;
	else if (plim == 0x02)
		pdata->charging_type = POWER_SUPPLY_TYPE_MHL_1500;
	else if (plim == 0x03)
		pdata->charging_type = POWER_SUPPLY_TYPE_USB;
	else
		pdata->charging_type = POWER_SUPPLY_TYPE_BATTERY;

	for (i = 0; i < 10; i++) {
		psy = power_supply_get_by_name("battery");
		if (psy)
			break;
	}
	if (i == 10) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}

	if (otg_enable) {
		psy_do_property(pdata->charger_name, get,
					POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval == POWER_SUPPLY_TYPE_BATTERY ||
				value.intval == POWER_SUPPLY_TYPE_WIRELESS) {
			if (!lpcharge) {
				value.intval = true;
				psy_do_property(pdata->charger_name, set,
						POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
				pdata->charging_type = POWER_SUPPLY_TYPE_OTG;
			}
		}
	} else {
		value.intval = false;
		pr_info("sii8240 : %s Power supply set Battery\n", __func__);
		psy_do_property(pdata->charger_name, set, POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
	}

	value.intval = pdata->charging_type;
	pr_info("sii8240 : %s Power supply set (%d)\n", __func__, value.intval);
	ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE,
		&value);
	if (ret)
		pr_err("sii8240 : %s fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);

	return;
}

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433) /* KQ-Helsinki MHL-Regulator */
static void of_sii8240_hw_onoff(bool on)
{
	struct sii8240_platform_data *pdata = g_pdata;
	struct regulator *regulator1_8, *regulator1_2;
	int ret = 0;

	regulator1_8 = regulator_get(NULL, MHL_LDO1_8);
	if (IS_ERR(regulator1_8)) {
		pr_err("%s : regulator 1.8 is not available", __func__);
		return;
	}
	regulator1_2 = regulator_get(NULL, MHL_LDO1_2);
	if (IS_ERR(regulator1_2)) {
		pr_err("%s : regulator 1.2 is not available", __func__);
		goto err_exit0;
	}

	if (mhl_power_on == on) {
		pr_info("sii8240 : sii8240_power_onoff : already %d\n", on);
		regulator_put(regulator1_2);
		regulator_put(regulator1_8);
		return;
	}

	mhl_power_on = on;
	pr_info("sii8240 : sii8240_power_onoff : %d\n", on);

	if (on) {
		/* To avoid floating state of the HPD pin *
		 * in the absence of external pull-up     */
		ret = regulator_enable(regulator1_8);
		if (ret < 0) {
			pr_err("%s : regulator 1.8 is not enable", __func__);
			goto err_ext1;
		}
		ret = regulator_enable(regulator1_2);
		if (ret < 0) {
			pr_err("%s : regulator 1.2 is not enable", __func__);
			goto err_ext1;
		}
		usleep_range(10000, 20000);
	} else {
		/* To avoid floating state of the HPD pin *
		 * in the absence of external pull-up     */
		regulator_disable(regulator1_8);
		regulator_disable(regulator1_2);

		gpio_set_value(pdata->mhl_reset, 0);
	}

	regulator_put(regulator1_2);
err_exit0:
	regulator_put(regulator1_8);
	return;

err_ext1:
	regulator_disable(regulator1_8);
	regulator_disable(regulator1_2);

	regulator_put(regulator1_2);
	regulator_put(regulator1_8);

	return;
}
#else

static void of_sii8240_hw_onoff(bool on)
{
	struct sii8240_platform_data *pdata = g_pdata;
	struct regulator *regulator1_8, *regulator3_3, *regulator1_2;
	int ret = 0;

	regulator1_8 = regulator_get(NULL, MHL_LDO1_8);
	if (IS_ERR(regulator1_8)) {
		pr_err("%s : regulator 1.8 is not available", __func__);
		return;
	}
	regulator3_3 = regulator_get(NULL, MHL_LDO3_3);
	if (IS_ERR(regulator3_3)) {
		pr_err("%s : regulator 3.3 is not available", __func__);
		goto err_exit0;
	}
	regulator1_2 = regulator_get(NULL, MHL_LDO1_2);
	if (IS_ERR(regulator1_2)) {
		pr_err("%s : regulator 1.2 is not available", __func__);
		goto err_exit1;
	}

	if (mhl_power_on == on) {
		pr_info("sii8240 : sii8240_power_onoff : alread %d\n", on);
		regulator_put(regulator1_2);
		regulator_put(regulator3_3);
		regulator_put(regulator1_8);
		return;
	}

	mhl_power_on = on;
	pr_info("sii8240 : sii8240_power_onoff : %d\n", on);

	if (on) {
		/* To avoid floating state of the HPD pin *
		 * in the absence of external pull-up     */
		ret = regulator_enable(regulator3_3);
		if (ret < 0) {
			pr_err("%s : regulator 3.3 is not enable", __func__);
			goto err_ext2;
		}
		ret = regulator_enable(regulator1_8);
		if (ret < 0) {
			pr_err("%s : regulator 1.8 is not enable", __func__);
			goto err_ext2;
		}
		ret = regulator_enable(regulator1_2);
		if (ret < 0) {
			pr_err("%s : regulator 1.2 is not enable", __func__);
			goto err_ext2;
		}
		usleep_range(10000, 20000);
	} else {
		/* To avoid floating state of the HPD pin *
		 * in the absence of external pull-up     */
		regulator_disable(regulator3_3);
		regulator_disable(regulator1_8);
		regulator_disable(regulator1_2);

		gpio_set_value(pdata->mhl_reset, 0);
	}

	regulator_put(regulator1_2);
err_exit1:
	regulator_put(regulator3_3);
err_exit0:
	regulator_put(regulator1_8);
	return;

err_ext2:
	regulator_disable(regulator3_3);
	regulator_disable(regulator1_8);
	regulator_disable(regulator1_2);

	regulator_put(regulator1_2);
	regulator_put(regulator3_3);
	regulator_put(regulator1_8);

	return;
}
#endif

static void of_sii8240_hw_reset(void)
{
	struct sii8240_platform_data *pdata = g_pdata;

	pr_info("%s()\n", __func__);

	usleep_range(10000, 20000);
	gpio_set_value(pdata->mhl_reset, 1);
	usleep_range(5000, 20000);
	gpio_set_value(pdata->mhl_reset, 0);
	usleep_range(10000, 20000);
	gpio_set_value(pdata->mhl_reset, 1);
	usleep_range(10000, 20000);

	return;
}

static int of_sii8240_parse_dt(struct sii8240_platform_data *pdata)
{
	struct device_node *np = pdata->tmds_client->dev.of_node;
	struct device_node *np_charger;

	pdata->mhl_reset = of_get_named_gpio_flags(np,
		"sii8240,mhl_reset", 0, NULL);
	if (pdata->mhl_reset > 0)
		pr_info("sii8240: gpio: mhl_reset = %d\n", pdata->mhl_reset);

	pdata->mhl_scl = of_get_named_gpio_flags(np,
		"sii8240,mhl_scl", 0, NULL);
	if (pdata->mhl_scl > 0)
		pr_info("sii8240: gpio: mhl_scl = %d\n", pdata->mhl_scl);

	pdata->mhl_sda = of_get_named_gpio_flags(np,
		"sii8240,mhl_sda", 0, NULL);
	if (pdata->mhl_sda > 0)
		pr_info("sii8240: gpio: mhl_sda = %d\n", pdata->mhl_sda);

	if (!of_property_read_u32(np, "sii8240,swing_level",
				&pdata->swing_level))
		pr_info("sii8240: swing_level = 0x%X\n", pdata->swing_level);

	/* get charger_name */
	np_charger = of_find_node_by_name(NULL, "battery");
	if (np_charger != NULL) {
		if (!of_property_read_string(np_charger, "battery,charger_name",
					(char const **)&pdata->charger_name))
			pr_info("sii8240: charger_name = %s\n",	pdata->charger_name);
	}

	return 0;
}

static void of_sii8240_gpio_init(struct sii8240_platform_data *pdata)
{
	if (pdata->mhl_reset > 0) {
		if (gpio_request(pdata->mhl_reset, "mhl_reset")) {
			pr_err("[ERROR] %s: unable to request mhl_reset [%d]\n",
					__func__, pdata->mhl_reset);
			return;
		}
		if (gpio_direction_output(pdata->mhl_reset, 0)) {
			pr_err("[ERROR] %s: unable to mhl_reset low[%d]\n",
					__func__, pdata->mhl_reset);
			return;
		}
	}
	return;
}

struct sii8240_platform_data *platform_init_data(struct i2c_client *client)
{
	struct sii8240_platform_data *pdata = NULL;
	pdata = kzalloc(sizeof(struct sii8240_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return NULL;
	}
	g_pdata = pdata;

	pdata->tmds_client = client;
	pdata->hw_onoff = of_sii8240_hw_onoff;
	pdata->hw_reset = of_sii8240_hw_reset;
	pdata->sii8240_muic_cb = muic_mhl_cb;
	pdata->vbus_present = NULL;

	of_sii8240_parse_dt(pdata);
	of_sii8240_gpio_init(pdata);

	return pdata;
}
