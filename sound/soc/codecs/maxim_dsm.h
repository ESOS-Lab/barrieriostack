#ifndef _MAXIM_DSM_H
#define _MAXIM_DSM_H

#define DSM_RX_PORT_ID	0x1004

extern int dsm_misc_device_init(void);
extern int dsm_misc_device_deinit(void);

extern int maxim_dsm_status_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);
extern int maxim_dsm_status_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);
extern int maxim_dsm_dump_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);

extern int maxim_dsm_dump_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);

extern struct attribute_group maxim_attribute_group;

#endif
