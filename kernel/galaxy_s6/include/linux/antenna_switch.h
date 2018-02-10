#ifndef _ANTENNA_SWITCH_H
#define _ANTENNA_SWITCH_H

struct device *sec_device_create(void *drvdata, const char *fmt);

struct antenna_switch_drvdata {
	int gpio_antenna_switch;
};

extern void antenna_switch_work_earjack(bool);
extern void antenna_switch_work_if(bool);

#endif /* _ANTENNA_SWITCH_H */
