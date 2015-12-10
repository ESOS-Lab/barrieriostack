#ifndef __MDM_HSIC_PM_H__
#define __MDM_HSIC_PM_H__
#include <linux/usb.h>

enum pwr_stat {
	POWER_OFF,
	POWER_ON,
};

enum hsic_lpa_states {
	STATE_HSIC_LPA_ENTER,
	STATE_HSIC_LPA_WAKE,
	STATE_HSIC_LPA_PHY_INIT,
	STATE_HSIC_LPA_CHECK,
	STATE_HSIC_LPA_ENABLE,
	STATE_HSIC_SUSPEND,
	STATE_HSIC_RESUME,
};

struct mdm_hsic_pm_data;

void request_active_lock_set(const char *name);
void request_active_lock_release(const char *name);
void request_boot_lock_set(const char *name);
void request_boot_lock_release(const char *name);
int check_udev_suspend_allowed(const char *name);
bool check_request_blocked(const char *name);
int pm_dev_runtime_get_enabled(struct usb_device *udev);
int pm_dev_wait_lpa_wake(void);

/*add wakelock interface for fast dormancy*/
#ifdef CONFIG_HAS_WAKELOCK
void fast_dormancy_wakelock(const char *name);
#else
void fast_dormancy_wakelock(const char *name) {}
#endif

/**
 * register_udev_to_pm_dev - called at interface driver probe function
 *
 * @name:		name of pm device to register usb device
 * @udev:		pointer of udev to register
 */
int register_udev_to_pm_dev(const char *name, struct usb_device *udev);

/**
 * unregister_udev_from_pm_dev - called at interface driver disconnect function
 *
 * @name:		name of pm device to unregister usb device
 * @udev:		pointer of udev to unregister
 */
void unregister_udev_from_pm_dev(const char *name, struct usb_device *udev);

int set_qmicm_mode(const char *name);
int get_qmicm_mode(const char *name);

extern struct blocking_notifier_head mdm_reset_notifier_list;
extern void mdm_force_fatal(void);
extern void print_mdm_gpio_state(void);
extern bool lpa_handling;
extern int hello_packet_rx;
extern int ep0_timeout_cnt;

extern int s5p_usb_phy_init(struct platform_device *pdev, int type);
extern int s5p_usb_phy_exit(struct platform_device *pdev, int type);
extern void set_ap2mdm_errfatal(void);
#endif /* __MDM_HSIC_PM_H__ */
