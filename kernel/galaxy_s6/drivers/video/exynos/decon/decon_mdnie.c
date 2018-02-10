/* linux/drivers/video/mdnie.c
 *
 * Register interface file for Samsung mDNIe driver
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>

#include "./panels/mdnie.h"
#include "mdnie_reg.h"

#if defined(CONFIG_PANEL_S6E3HA2_DYNAMIC) || defined(CONFIG_PANEL_S6E3HF2_DYNAMIC)
#include "decon-mdnie_table_zero.h"
#endif
#if defined(CONFIG_TDMB)
#include "decon-mdnie_table_dmb.h"
#endif

#define MDNIE_SYSFS_PREFIX		"/sdcard/mdnie/"
#define PANEL_COORDINATE_PATH	"/sys/class/lcd/panel/color_coordinate"

#define IS_DMB(idx)			(idx == DMB_NORMAL_MODE)
#define IS_SCENARIO(idx)		((idx < SCENARIO_MAX) && !((idx > VIDEO_NORMAL_MODE) && (idx < CAMERA_MODE)))
#define IS_ACCESSIBILITY(idx)		(idx && (idx < ACCESSIBILITY_MAX))
#define IS_HBM(idx)			(idx >= 6)
#if defined(CONFIG_LCD_HMT)
#define IS_HMT(idx)			(idx >= HMT_MDNIE_ON && idx < HMT_MDNIE_MAX)
#endif

#define SCENARIO_IS_VALID(idx)	(IS_DMB(idx) || IS_SCENARIO(idx))

/* Split 16 bit as 8bit x 2 */
#define GET_MSB_8BIT(x)		((x >> 8) & (BIT(8) - 1))
#define GET_LSB_8BIT(x)		((x >> 0) & (BIT(8) - 1))


static int mdnie_write_table(struct mdnie_info *mdnie, struct mdnie_table *table)
{

	if (IS_ERR_OR_NULL(table->tune[0].sequence)) {
		dev_err(mdnie->dev, "mdnie sequence %s is null, %lx\n",
				table->name, (unsigned long)table->tune[0].sequence);
		return -EPERM;
	}

	mutex_lock(&mdnie->dev_lock);
	mdnie->lpd_store_data = table->tune[0].sequence;
	mdnie->need_update = 1;
	mutex_unlock(&mdnie->dev_lock);
	return 0;
}

static struct mdnie_table *mdnie_find_table(struct mdnie_info *mdnie)
{
	struct mdnie_table *table = NULL;

	mutex_lock(&mdnie->lock);

	if (IS_ACCESSIBILITY(mdnie->accessibility)) {
		table = &accessibility_table[mdnie->accessibility];
		goto exit;
#if defined(CONFIG_LCD_HMT)
	} else if (IS_HMT(mdnie->hmt_mode)) {
		table = &hmt_table[mdnie->hmt_mode];
		goto exit;
#endif
	} else if (IS_HBM(mdnie->auto_brightness)) {
		if((mdnie->scenario == BROWSER_MODE) || (mdnie->scenario == EBOOK_MODE))
			table = &hbm_table[HBM_ON_TEXT];
		else
			table = &hbm_table[HBM_ON];
		goto exit;
#if defined(CONFIG_TDMB)
	} else if (IS_DMB(mdnie->scenario)) {
		table = &dmb_table[mdnie->mode];
		goto exit;
#endif
	} else if (IS_SCENARIO(mdnie->scenario)) {
		table = &tuning_table[mdnie->scenario][mdnie->mode];
		dev_info(mdnie->dev, "%s: scenario = %d mode = %d, %s\n", __func__, mdnie->scenario, mdnie->mode, table->name);
		goto exit;
	}

exit:
	mutex_unlock(&mdnie->lock);

	return table;
}

static void mdnie_update_sequence(struct mdnie_info *mdnie, struct mdnie_table *table)
{
	struct mdnie_table *t = NULL;

	if (mdnie->tuning) {
		t = mdnie_request_table(mdnie->path, table);
		if (!IS_ERR_OR_NULL(t) && !IS_ERR_OR_NULL(t->name)){
			mdnie_write_table(mdnie, t);
			dev_info(mdnie->dev, "%s table t\n", __func__);
		}
		else
			mdnie_write_table(mdnie, table);
	} else
		mdnie_write_table(mdnie, table);
	return;
}

static void mdnie_update(struct mdnie_info *mdnie)
{
	struct mdnie_table *table = NULL;

	/*if (!mdnie->enable) {
		dev_err(mdnie->dev, "mdnie state is off\n");
		return;
	}*/

	table = mdnie_find_table(mdnie);
	if (!IS_ERR_OR_NULL(table) && !IS_ERR_OR_NULL(table->name)) {
		unsigned int s[3], i = 0;
		mdnie_t *wbuf=NULL;

		mdnie_update_sequence(mdnie, table);
		dev_info(mdnie->dev, "%s\n", table->name);

		mutex_lock(&mdnie->lock);

                s[0]=s[1]=s[2]=0;
                wbuf = table->tune[MDNIE_CMD1].sequence;
                while (wbuf[i] != END_SEQ) {
                    if(wbuf[i]>=MDNIE_WHITE_R && wbuf[i]<=MDNIE_WHITE_B) {
                            unsigned int k = wbuf[i]-MDNIE_WHITE_R;
                            u16 val = ((0xFF00 & wbuf[i+1]) >> 8);
                            val &= 0x00FF;
                            s[k]=val;
                    }
                    i += 2;
                }
		mdnie->white_r = s[0];
		mdnie->white_g = s[1];
		mdnie->white_b = s[2];

		mutex_unlock(&mdnie->lock);
	}
	return;
}

void mdnie_restore_data(struct mdnie_info *mdnie)
{
	int i = 0;
	const unsigned short *wbuf = NULL;
	wbuf = mdnie->lpd_store_data;

	if(mdnie->need_update || !mdnie->enable) {
		if(!IS_ERR_OR_NULL(wbuf)) {
			while (wbuf[i] != END_SEQ) {
				mdnie_write(wbuf[i]*4, wbuf[i+1]);
				i += 2;
			}
			mdnie->need_update = 0;
		} else {
			dev_info(mdnie->dev, "%s tuning buffer is null\n", __func__);
		}
	}
	return;
}

void decon_mdnie_start(struct mdnie_info *mdnie, u32 w, u32 h)
{
	if(IS_ERR_OR_NULL(mdnie))
		return;
   
	mutex_lock(&mdnie->lock);
	mdnie_reg_start(w, h);
	mdnie_restore_data(mdnie);
	mdnie->enable = 1;
	mutex_unlock(&mdnie->lock);
}

void decon_mdnie_stop(struct mdnie_info *mdnie)
{
	if(IS_ERR_OR_NULL(mdnie))
		return;

	mutex_lock(&mdnie->lock);
        mdnie_reg_stop();
	mdnie->enable = 0;
	mutex_unlock(&mdnie->lock);
}

void decon_mdnie_frame_update(struct mdnie_info *mdnie, u32 xres, u32 yres)
{
	if(IS_ERR_OR_NULL(mdnie))
		return;

	mutex_lock(&mdnie->lock);
	if(mdnie->enable) {
		mdnie_restore_data(mdnie);
		mdnie_reg_frame_update(xres, yres);
		/*dev_info(mdnie->dev, "%s: xres=%d, yres=%d\n", __func__, xres, yres);*/
	}
	mutex_unlock(&mdnie->lock);
}

u32 decon_mdnie_input_read(void)
{
	return mdnie_reg_read_input_data();
}

static void update_color_position(struct mdnie_info *mdnie, unsigned int idx)
{
	u8 mode, scenario;

	dev_info(mdnie->dev, "%s: idx=%d\n", __func__, idx);

	mutex_lock(&mdnie->lock);

	for (mode = 0; mode < MODE_MAX; mode++) {
		for (scenario = 0; scenario <= EMAIL_MODE; scenario++) {
			unsigned int i = 0;
			mdnie_t *wbuf=NULL;
			wbuf = tuning_table[scenario][mode].tune[MDNIE_CMD1].sequence;
			if (IS_ERR_OR_NULL(wbuf))
				continue;
                        if (scenario != EBOOK_MODE){
                            while (wbuf[i] != END_SEQ) {
                                if(wbuf[i]>=MDNIE_WHITE_R && wbuf[i]<=MDNIE_WHITE_B) {
                                    unsigned int k = wbuf[i]-MDNIE_WHITE_R;
                                    u16 val = (0x00FF & wbuf[i+1]);
                                    val |= ((0x00FF & coordinate_data[mode][idx][k]) << 8);
                                    /*dev_info(mdnie->dev, "%02x : %04x->%04x\n", wbuf[i], wbuf[i+1], val);*/
                                    wbuf[i+1] = val;
                                }
                                i += 2;
                            }
                        }
		}
	}

	mutex_unlock(&mdnie->lock);
}

static int get_panel_coordinate(struct mdnie_info *mdnie, int *result)
{
	int ret = 0;

	unsigned short x, y;

	x = mdnie->coordinate[0];
	y = mdnie->coordinate[1];


	result[1] = COLOR_OFFSET_F1(x, y);
	result[2] = COLOR_OFFSET_F2(x, y);
	result[3] = COLOR_OFFSET_F3(x, y);
	result[4] = COLOR_OFFSET_F4(x, y);

	ret = mdnie_calibration(result);
	dev_info(mdnie->dev, "%s: %d, %d, idx=%d\n", __func__, x, y, ret);

//skip_color_correction:
	mdnie->color_correction = 1;

	return ret;
}

static ssize_t mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->mode);
}

static ssize_t mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret;
	int result[5] = {0,};

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: value=%d\n", __func__, value);

	if (value >= MODE_MAX) {
		value = STANDARD;
		return -EINVAL;
	}

	mutex_lock(&mdnie->lock);
	mdnie->mode = value;
	mutex_unlock(&mdnie->lock);

	if (!mdnie->color_correction) {
		ret = get_panel_coordinate(mdnie, result);
		if (ret > 0)
			update_color_position(mdnie, ret);
	}

	mdnie_update(mdnie);

	return count;
}


static ssize_t scenario_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->scenario);
}

static ssize_t scenario_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: value=%d\n", __func__, value);

	if (!SCENARIO_IS_VALID(value))
		value = UI_MODE;

	mutex_lock(&mdnie->lock);
	mdnie->scenario = value;
	mutex_unlock(&mdnie->lock);

	mdnie_update(mdnie);

	return count;
}

static ssize_t tuning_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	char *pos = buf;
	struct mdnie_table *table = NULL;
	int i;

	pos += sprintf(pos, "++ %s: %s\n", __func__, mdnie->path);

	if (!mdnie->tuning) {
		pos += sprintf(pos, "tunning mode is off\n");
		goto exit;
	}

	if (strncmp(mdnie->path, MDNIE_SYSFS_PREFIX, sizeof(MDNIE_SYSFS_PREFIX) - 1)) {
		pos += sprintf(pos, "file path is invalid, %s\n", mdnie->path);
		goto exit;
	}

	table = mdnie_find_table(mdnie);
	table = mdnie_request_table(mdnie->path, table);
	if (!IS_ERR_OR_NULL(table) && !IS_ERR_OR_NULL(table->name)) {
		for (i = 0; i < table->tune[MDNIE_CMD1].size; i++)
			pos += sprintf(pos, "0x%02x ", table->tune[MDNIE_CMD1].sequence[i]);
		pos += sprintf(pos, "\n");
	}

exit:
	pos += sprintf(pos, "-- %s\n", __func__);

	return pos - buf;
}

static ssize_t tuning_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int ret;

	if (sysfs_streq(buf, "0") || sysfs_streq(buf, "1")) {
		ret = kstrtouint(buf, 0, &mdnie->tuning);
		if (ret < 0)
			return ret;
		if (!mdnie->tuning) 
			memset(mdnie->path, 0, sizeof(mdnie->path));

		dev_info(dev, "%s: %s\n", __func__, mdnie->tuning ? "enable" : "disable");
	} else {
		if (!mdnie->tuning)
			return count;

		if (count > (sizeof(mdnie->path) - sizeof(MDNIE_SYSFS_PREFIX))) {
			dev_err(dev, "file name %s is too long\n", mdnie->path);
			return -ENOMEM;
		}

		memset(mdnie->path, 0, sizeof(mdnie->path));
		snprintf(mdnie->path, sizeof(MDNIE_SYSFS_PREFIX) + count-1, "%s%s", MDNIE_SYSFS_PREFIX, buf);
		dev_info(dev, "%s: %s\n", __func__, mdnie->path);

		mdnie_update(mdnie);
	}

	return count;
}

static ssize_t accessibility_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->accessibility);
}

static ssize_t accessibility_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
        unsigned int value, s[9], i = 0;
	int ret;

	ret = sscanf(buf, "%d %x %x %x %x %x %x %x %x %x",
		&value, &s[0], &s[1], &s[2], &s[3],
		&s[4], &s[5], &s[6], &s[7], &s[8]);

	dev_info(dev, "%s: value=%d\n", __func__, value);

	if (ret < 0){
		return ret;
	}
	else {
		if (value >= ACCESSIBILITY_MAX)
			value = ACCESSIBILITY_OFF;

		mutex_lock(&mdnie->lock);

		mdnie->accessibility = value;
		if (value == COLOR_BLIND) {
                        mdnie_t *wbuf=NULL;
			if (ret != 10) {
				mutex_unlock(&mdnie->lock);
				return -EINVAL;
			}
			wbuf = accessibility_table[COLOR_BLIND].tune[MDNIE_CMD1].sequence;
                        while (wbuf[i] != END_SEQ) {
                            if(wbuf[i]>=MDNIE_RED_R && wbuf[i]<=MDNIE_BLUE_B) {
                                unsigned int k = wbuf[i]-MDNIE_RED_R;
                                dev_info(mdnie->dev, "%02x : %04x->%04x\n", wbuf[i], wbuf[i+1], s[k]);
                                wbuf[i+1] = (u16)s[k];
                            }
                            i += 2;
                        }
			dev_info(dev, "%s: %s\n", __func__, buf);
		}

		mutex_unlock(&mdnie->lock);

		mdnie_update(mdnie);
	}

	return count;
}

static ssize_t color_correct_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	char *pos = buf;
	int i, idx, result[5] = {0,};

	if (!mdnie->color_correction)
		return -EINVAL;

	idx = get_panel_coordinate(mdnie, result);

	for (i = 1; i < (int) ARRAY_SIZE(result); i++)
		pos += sprintf(pos, "f%d: %d, ", i, result[i]);
	pos += sprintf(pos, "tune%d\n", idx);

	return pos - buf;
}

static ssize_t bypass_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->bypass);
}

static ssize_t bypass_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	struct mdnie_table *table = NULL;
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if (ret < 0)
		return ret;
	else {
		if (value >= BYPASS_MAX)
			value = BYPASS_OFF;

		value = (value) ? BYPASS_ON : BYPASS_OFF;

		mutex_lock(&mdnie->lock);
		mdnie->bypass = value;
		mutex_unlock(&mdnie->lock);

		table = &bypass_table[value];
		if (!IS_ERR_OR_NULL(table)) {
			mdnie_update_sequence(mdnie, table);
			dev_info(mdnie->dev, "%s\n", table->name);
		}
	}

	return count;
}

static ssize_t auto_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d, hbm: %d\n", mdnie->auto_brightness, mdnie->hbm);
}

static ssize_t auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;
	static unsigned int update;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: value=%d\n", __func__, value);

	mutex_lock(&mdnie->lock);
	update = (IS_HBM(mdnie->auto_brightness) != IS_HBM(value)) ? 1 : 0;
	mdnie->hbm = IS_HBM(value) ? HBM_ON : HBM_OFF;
	mdnie->auto_brightness = value;
	mutex_unlock(&mdnie->lock);

	if (update)
		mdnie_update(mdnie);
	
	return count;
}
static ssize_t sensorRGB_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	return sprintf(buf, "%d %d %d\n", mdnie->white_r,
		mdnie->white_g, mdnie->white_b);
}

static ssize_t sensorRGB_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int s[3];
	int ret;

	ret = sscanf(buf, "%d %d %d"
		, &s[0], &s[1], &s[2]);
	if (ret < 0)
		return ret;

        dev_info(mdnie->dev, "%d, %d, %d, accessibility=%d, mode=%d, senario=%d\n",
		s[0], s[1], s[2], mdnie->accessibility, mdnie->mode, mdnie->scenario);

	if ((mdnie->accessibility == ACCESSIBILITY_OFF)
		&& (mdnie->mode == AUTO)
		&& ((mdnie->scenario == BROWSER_MODE) || (mdnie->scenario == EBOOK_MODE))) {

		mdnie_t *wbuf=NULL;
		struct mdnie_table *table = NULL;
		unsigned int i = 0;

		table = mdnie_find_table(mdnie);
		memcpy(&(mdnie->table_buffer),
			table, sizeof(struct mdnie_table));
		memcpy(mdnie->sequence_buffer,
			table->tune[MDNIE_CMD1].sequence,
			sizeof(mdnie_t)*(table->tune[MDNIE_CMD1].size));
		mdnie->table_buffer.tune[MDNIE_CMD1].sequence
			= mdnie->sequence_buffer;

                mutex_lock(&mdnie->lock);

		wbuf = mdnie->table_buffer.tune[MDNIE_CMD1].sequence;

                while (wbuf[i] != END_SEQ) {
                    if(wbuf[i]>=MDNIE_WHITE_R && wbuf[i]<=MDNIE_WHITE_B) {
                            unsigned int k = wbuf[i]-MDNIE_WHITE_R;
                            u16 val = (0x00FF & wbuf[i+1]);
                            val |= ((0x00FF & s[k]) << 8);
                            dev_info(mdnie->dev, "%02x : %04x->%04x\n", wbuf[i], wbuf[i+1], val);
                            wbuf[i+1] = val;
                    }
                    i += 2;
                }
		mdnie->white_r = s[0];
		mdnie->white_g = s[1];
		mdnie->white_b = s[2];

		mutex_unlock(&mdnie->lock);

		mdnie_update_sequence(mdnie, &(mdnie->table_buffer));
	}

	return count;
}

#if defined(CONFIG_LCD_HMT)
static ssize_t hmtColorTemp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "hmt_mode: %d\n", mdnie->hmt_mode);
}

static ssize_t hmtColorTemp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;


	if (value != mdnie->hmt_mode && value < HMT_MDNIE_MAX) {
		mutex_lock(&mdnie->lock);
		mdnie->hmt_mode = value;
		mutex_unlock(&mdnie->lock);
		mdnie_update(mdnie);
	}

	return count;
}
#endif

static struct device_attribute mdnie_attributes[] = {
	__ATTR(mode, 0664, mode_show, mode_store),
	__ATTR(scenario, 0664, scenario_show, scenario_store),
	__ATTR(tuning, 0664, tuning_show, tuning_store),
	__ATTR(accessibility, 0664, accessibility_show, accessibility_store),
	__ATTR(color_correct, 0444, color_correct_show, NULL),
	__ATTR(bypass, 0664, bypass_show, bypass_store),
	__ATTR(auto_brightness, 0664, auto_brightness_show, auto_brightness_store),
//	__ATTR(mdnie, 0444, mdnie_show, NULL),
	__ATTR(sensorRGB, 0664, sensorRGB_show, sensorRGB_store),
#if defined(CONFIG_LCD_HMT)
	__ATTR(hmt_color_temperature, 0664, hmtColorTemp_show, hmtColorTemp_store),
#endif
	__ATTR_NULL,
};

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct mdnie_info *mdnie;
	struct fb_event *evdata = data;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
		break;
	default:
		return 0;
	}

	mdnie = container_of(self, struct mdnie_info, fb_notif);

	fb_blank = *(int *)evdata->data;

	dev_info(mdnie->dev, "%s: %d\n", __func__, fb_blank);

	if (evdata->info->node != 0)
		return 0;

        if (fb_blank == FB_BLANK_UNBLANK) {
		/*mutex_lock(&mdnie->lock);
		mdnie->enable = 1;
		mutex_unlock(&mdnie->lock);

		mdnie_update(mdnie);*/
	} else if (fb_blank == FB_BLANK_POWERDOWN) {
		/*mutex_lock(&mdnie->lock);
		mdnie->enable = 0;
		mutex_unlock(&mdnie->lock);*/
	}

	return 0;
}

static int mdnie_register_fb(struct mdnie_info *mdnie)
{
	memset(&mdnie->fb_notif, 0, sizeof(mdnie->fb_notif));
	mdnie->fb_notif.notifier_call = fb_notifier_callback;
	return fb_register_client(&mdnie->fb_notif);
}

struct mdnie_info* decon_mdnie_register(void)
{
	int ret = 0;
	struct mdnie_info *mdnie;
	static struct class *mdnie_class;

	pr_info("%s\n", __func__);

	mdnie_class = class_create(THIS_MODULE, "mdnie");
	if (IS_ERR_OR_NULL(mdnie_class)) {
		pr_err("failed to create mdnie class\n");
		ret = -EINVAL;
		goto error0;
	}

	mdnie_class->dev_attrs = mdnie_attributes;

	mdnie = kzalloc(sizeof(struct mdnie_info), GFP_KERNEL);
	if (!mdnie) {
		pr_err("failed to allocate mdnie\n");
		ret = -ENOMEM;
		goto error1;
	}

	mdnie->dev = device_create(mdnie_class, NULL, 0, &mdnie, "mdnie");
	if (IS_ERR_OR_NULL(mdnie->dev)) {
		pr_err("failed to create mdnie device\n");
		ret = -EINVAL;
		goto error2;
	}

	mdnie->scenario = UI_MODE;
	mdnie->mode = STANDARD;
	mdnie->enable = 0;
	mdnie->tuning = 0;
	mdnie->accessibility = ACCESSIBILITY_OFF;
	mdnie->bypass = BYPASS_OFF;
	mdnie->coordinate[0] = 0;
	mdnie->coordinate[1] = 0;

	mutex_init(&mdnie->lock);
	mutex_init(&mdnie->dev_lock);

	dev_set_drvdata(mdnie->dev, mdnie);

	mdnie_register_fb(mdnie);

	mdnie->enable = 0;
	mdnie_update(mdnie);

	dev_info(mdnie->dev, "registered successfully\n");

	return mdnie;

error2:
	kfree(mdnie);
error1:
	class_destroy(mdnie_class);
error0:
	return 0;
}

