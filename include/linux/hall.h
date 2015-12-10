#ifndef _HALL_H
#define _HALL_H
struct hall_platform_data {
	unsigned int rep:1;		/* enable input subsystem auto repeat */
	int gpio_flip_cover;
	int gpio_certify_cover;
};
extern struct device *sec_key;

#ifdef CONFIG_CERTIFY_HALL_NFC_WA
extern int felica_ant_tuning(int parameter);
#endif

#endif /* _HALL_H */
