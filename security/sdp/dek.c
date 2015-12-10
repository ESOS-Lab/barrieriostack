#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/random.h>
#include <linux/err.h>
#include <sdp/dek_common.h>
#include <sdp/dek_ioctl.h>
#include <sdp/dek_aes.h>

/*
 * Need to move this to defconfig
 */
#define CONFIG_PUB_CRYPTO
//#define CONFIG_SDP_IOCTL_PRIV

#ifdef CONFIG_PUB_CRYPTO
#include <sdp/pub_crypto_emul.h>
#endif

#define DEK_LOG_COUNT		100

/* Keys */
kek_t SDPK_sym[SDP_MAX_USERS];
kek_t SDPK_Rpub[SDP_MAX_USERS];
kek_t SDPK_Rpri[SDP_MAX_USERS];
kek_t SDPK_Dpub[SDP_MAX_USERS];
kek_t SDPK_Dpri[SDP_MAX_USERS];
kek_t SDPK_EDpub[SDP_MAX_USERS];
kek_t SDPK_EDpri[SDP_MAX_USERS];

extern void ecryptfs_mm_drop_cache(int userid);

/* Crypto tfms */
struct crypto_blkcipher *sdp_tfm[SDP_MAX_USERS];

/* Log buffer */
struct log_struct
{
	int len;
	char buf[256];
	struct list_head list;
	spinlock_t list_lock;
};
struct log_struct log_buffer;
static int log_count = 0;

/* Wait queue */
wait_queue_head_t wq;
static int flag = 0;

int dek_is_sdp_uid(uid_t uid) {
	int userid = uid / PER_USER_RANGE;
	int key_arr_idx = userid-100;

	if ((userid < 100) || (userid > 100+SDP_MAX_USERS-1)) {
		return 0;
	}

	if((SDPK_Rpub[key_arr_idx].len > 0) ||
			(SDPK_Dpub[key_arr_idx].len > 0) ||
			(SDPK_EDpub[key_arr_idx].len > 0))
		return 1;

	return 0;
}
EXPORT_SYMBOL(dek_is_sdp_uid);

static int is_system_server(void) {
	uid_t uid = current_uid();

	switch(uid) {
#if 0
	case 0: //root
		DEK_LOGD("allowing root to access SDP device files\n");
#endif
	case 1000:
		return 1;
	default:
		break;
	}

	return 0;
}

// is_conatiner_app(current.uid);
static int is_container_app(void) {
	uid_t uid = current_uid();

	int userid = uid / PER_USER_RANGE;

	if(userid >= 100)
		return 1;


	return 0;
}

static int is_root(void) {
	uid_t uid = current_uid();

	switch(uid) {
	case 0: //root
		//DEK_LOGD("allowing root to access SDP device files\n");
		return 1;
	default:
		;
	}

	return 0;
}

static int zero_out(char *buf, unsigned int len) {
	char zero = 0;
	int retry_cnt = 3;
	int i;

	retry:
	if(retry_cnt > 0) {
		memset((void *)buf, zero, len);
		for (i = 0; i < len; ++i) {
			zero |= buf[i];
			if (zero) {
				DEK_LOGE("the memory was not properly zeroed\n");
				retry_cnt--;
				goto retry;
			}
		}
	} else {
		DEK_LOGE("FATAL : can't zero out!!\n");
		return -1;
	}

	return 0;
}

/* Log */
static void dek_add_to_log(int userid, char * buffer);


static int dek_open_evt(struct inode *inode, struct file *file)
{
	return 0;
}

static int dek_release_evt(struct inode *ignored, struct file *file)
{
	return 0;
}

static int dek_open_req(struct inode *inode, struct file *file)
{
	return 0;
}

static int dek_release_req(struct inode *ignored, struct file *file)
{
	return 0;
}

static int dek_open_kek(struct inode *inode, struct file *file)
{
	return 0;
}

static int dek_release_kek(struct inode *ignored, struct file *file)
{
	return 0;
}

#ifdef CONFIG_SDP_KEY_DUMP
void dek_dump(unsigned char *buf, int len) {
	int i;

	printk("len=%d: ", len);
	for(i=0;i<len;++i) {
	    if((i%16) == 0)
	        printk("\n");
		printk("%02X ", (unsigned char)buf[i]);
	}
	printk("\n");
}

void dump_all_keys(int key_arr_idx) {
	printk("SDPK_sym: ");
	dek_dump(SDPK_sym[key_arr_idx].buf, SDPK_sym[key_arr_idx].len);

	printk("SDPK_Rpub: ");
	dek_dump(SDPK_Rpub[key_arr_idx].buf, SDPK_Rpub[key_arr_idx].len);
	printk("SDPK_Rpri: ");
	dek_dump(SDPK_Rpri[key_arr_idx].buf, SDPK_Rpri[key_arr_idx].len);

	printk("SDPK_Dpub: ");
	dek_dump(SDPK_Dpub[key_arr_idx].buf, SDPK_Dpub[key_arr_idx].len);
	printk("SDPK_Dpri: ");
	dek_dump(SDPK_Dpri[key_arr_idx].buf, SDPK_Dpri[key_arr_idx].len);

    printk("SDPK_EDpub: ");
    dek_dump(SDPK_EDpub[key_arr_idx].buf, SDPK_EDpub[key_arr_idx].len);
    printk("SDPK_EDpri: ");
    dek_dump(SDPK_EDpri[key_arr_idx].buf, SDPK_EDpri[key_arr_idx].len);
}
#else
void dek_dump(unsigned char *buf, int len) {
}

void dump_all_keys(int key_arr_idx) {
}
#endif

static int dek_is_persona(int userid) {
	if ((userid < 100) || (userid > 100+SDP_MAX_USERS-1)) {
		DEK_LOGE("invalid persona id: %d\n", userid);
		dek_add_to_log(userid, "Invalid persona id");
		return 0;
	}
	return 1;
}

int dek_is_persona_locked(int userid) {
	int key_arr_idx = PERSONA_KEY_ARR_IDX(userid);
	if (dek_is_persona(userid)) {
		if (sdp_tfm[key_arr_idx] != NULL) {
			return 0;
		} else {
			return 1;
		}
	} else {
		DEK_LOGE("%s invalid userid %d\n", __func__, userid);
		return -1;
	}
}

int dek_generate_dek(int userid, dek_t *newDek) {
	if (!dek_is_persona(userid)) {
		DEK_LOGE("%s invalid userid %d\n", __func__, userid);
		return -EFAULT;
	}

	newDek->len = DEK_LEN;
	get_random_bytes(newDek->buf, newDek->len);

	if (newDek->len <= 0 || newDek->len > DEK_LEN) {
		zero_out((char *)newDek, sizeof(dek_t));
		return -EFAULT;
	}
#if DEK_DEBUG
	else {
		DEK_LOGD("DEK: ");
		dek_dump(newDek->buf, newDek->len);
	}
#endif

	return 0;
}

static int dek_encrypt_dek(int userid, dek_t *plainDek, dek_t *encDek) {
	int ret = 0;
	int key_arr_idx;

	if (!dek_is_persona(userid)) {
		DEK_LOGE("%s invalid userid %d\n", __func__, userid);
		return -EFAULT;
	}
	key_arr_idx = PERSONA_KEY_ARR_IDX(userid);
#if DEK_DEBUG
	DEK_LOGD("plainDek from user: ");
	dek_dump(plainDek->buf, plainDek->len);
#endif
	if (sdp_tfm[key_arr_idx] != NULL) {
		if (dek_aes_encrypt(sdp_tfm[key_arr_idx], plainDek->buf, encDek->buf, plainDek->len)) {
			DEK_LOGE("aes encrypt failed\n");
			dek_add_to_log(userid, "aes encrypt failed");
			encDek->len = 0;
		} else {
			encDek->len = plainDek->len;
			encDek->type = DEK_TYPE_AES_ENC;
		}
	} else {
#ifdef CONFIG_PUB_CRYPTO
		/*
		 * Do an asymmetric crypto
		 */
	    switch(get_sdp_sysfs_asym_alg()) {
	    case SDPK_ALGOTYPE_ASYMM_RSA:
	        if(SDPK_Rpub[key_arr_idx].len > 0) {
	            ret = rsa_encryptByPub(plainDek, encDek, &SDPK_Rpub[key_arr_idx]);
	        }else{
	            DEK_LOGE("SDPK_Rpub for id: %d\n", userid);
	            dek_add_to_log(userid, "encrypt failed, no SDPK_Rpub");
	            return -EIO;
	        }
	        break;
	    case SDPK_ALGOTYPE_ASYMM_DH:
	        if(SDPK_Dpub[key_arr_idx].len > 0) {
	            ret = dh_encryptDEK(plainDek, encDek, &SDPK_Dpub[key_arr_idx]);
	        }else{
	            DEK_LOGE("SDPK_Dpub for id: %d\n", userid);
	            dek_add_to_log(userid, "encrypt failed, no SDPK_Dpub");
	            return -EIO;
	        }
	        break;
	    case SDPK_ALGOTYPE_ASYMM_ECDH:
	        if(SDPK_EDpub[key_arr_idx].len > 0) {
	            ret = ecdh_encryptDEK(plainDek, encDek, &SDPK_EDpub[key_arr_idx]);
	        }else{
	            DEK_LOGE("SDPK_EDpub for id: %d\n", userid);
	            dek_add_to_log(userid, "encrypt failed, no SDPK_EDpub");
	            return -EIO;
	        }
	        break;
	    default:
	        dek_add_to_log(userid, "no ASYMM algo supported");
	        return -EOPNOTSUPP;
	    }
#else
		DEK_LOGE("pub crypto not supported : %d\n", userid);
		dek_add_to_log(userid, "encrypt failed, no key");
		return -EOPNOTSUPP;
#endif
	}

	if (encDek->len <= 0 || encDek->len > DEK_MAXLEN) {
		DEK_LOGE("dek_encrypt_dek, incorrect len=%d\n", encDek->len);
		zero_out((char *)encDek, sizeof(dek_t));
		return -EFAULT;
	}
#if DEK_DEBUG
	else {
		DEK_LOGD("encDek to user: ");
		dek_dump(encDek->buf, encDek->len);
	}
#endif

	return ret;
}

int dek_encrypt_dek_efs(int userid, dek_t *plainDek, dek_t *encDek) {
	return dek_encrypt_dek(userid, plainDek, encDek);
}

static int dek_decrypt_dek(int userid, dek_t *encDek, dek_t *plainDek) {
	int key_arr_idx;
	int dek_type = encDek->type;

	if (!dek_is_persona(userid)) {
		DEK_LOGE("%s invalid userid %d\n", __func__, userid);
		return -EFAULT;
	}
	key_arr_idx = PERSONA_KEY_ARR_IDX(userid);
#if DEK_DEBUG
	DEK_LOGD("encDek from user: ");
	dek_dump(encDek->buf, encDek->len);
#endif
	switch(dek_type) {
	case DEK_TYPE_AES_ENC:
	{
        if (sdp_tfm[key_arr_idx] != NULL) {
            if (dek_aes_decrypt(sdp_tfm[key_arr_idx], encDek->buf, plainDek->buf, encDek->len)) {
                DEK_LOGE("aes decrypt failed\n");
                dek_add_to_log(userid, "aes decrypt failed");
                plainDek->len = 0;
            } else {
                plainDek->len = encDek->len;
                plainDek->type = DEK_TYPE_PLAIN;
            }
        } else {
            DEK_LOGE("no SDPK_sym key for id: %d\n", userid);
            dek_add_to_log(userid, "decrypt failed, persona locked");
            return -EIO;
        }
        return 0;
	}
	case DEK_TYPE_RSA_ENC:
	{
#ifdef CONFIG_PUB_CRYPTO
        if(SDPK_Rpri[key_arr_idx].len > 0) {
            if(rsa_decryptByPair(encDek, plainDek, &SDPK_Rpri[key_arr_idx])){
                DEK_LOGE("rsa_decryptByPair failed");
                return -1;
            }
        }else{
            DEK_LOGE("SDPK_Rpri for id: %d\n", userid);
            dek_add_to_log(userid, "encrypt failed, no SDPK_Rpri");
            return -EIO;
        }
#else
        DEK_LOGE("Not supported key type: %d\n", encDek->type);
        dek_add_to_log(userid, "decrypt failed, DH type not supported");
        return -EOPNOTSUPP;
#endif
        return 0;
	}
	case DEK_TYPE_DH_ENC:
	{
#ifdef CONFIG_PUB_CRYPTO
        if(SDPK_Dpri[key_arr_idx].len > 0) {
            if(dh_decryptEDEK(encDek, plainDek, &SDPK_Dpri[key_arr_idx])){
                DEK_LOGE("dh_decryptEDEK failed");
                return -1;
            }
        }else{
            DEK_LOGE("SDPK_Dpri for id: %d\n", userid);
            dek_add_to_log(userid, "encrypt failed, no SDPK_Dpri");
            return -EIO;
        }
#else
        DEK_LOGE("Not supported key type: %d\n", encDek->type);
        dek_add_to_log(userid, "decrypt failed, DH type not supported");
        return -EOPNOTSUPP;
#endif
        return 0;
	}
	case DEK_TYPE_ECDH256_ENC:
	{
#ifdef CONFIG_PUB_CRYPTO
#if DEK_DEBUG
	    printk("DEK_TYPE_ECDH256_ENC encDek:"); dek_dump(encDek->buf, encDek->len);
#endif
        if(SDPK_EDpri[key_arr_idx].len > 0) {
            if(ecdh_decryptEDEK(encDek, plainDek, &SDPK_EDpri[key_arr_idx])){
                DEK_LOGE("ecdh_decryptEDEK failed");
                return -1;
            }
        }else{
            DEK_LOGE("SDPK_EDpri for id: %d\n", userid);
            dek_add_to_log(userid, "encrypt failed, no SDPK_EDpri");
            return -EIO;
        }
#else
        DEK_LOGE("Not supported key type: %d\n", encDek->type);
        dek_add_to_log(userid, "decrypt failed, ECDH type not supported");
        return -EOPNOTSUPP;
#endif
        return 0;
	}
	default:
	{
        DEK_LOGE("Unsupported edek type: %d\n", encDek->type);
        dek_add_to_log(userid, "decrypt failed, unsupported key type");
        return -EFAULT;
	}
	}
}

int dek_decrypt_dek_efs(int userid, dek_t *encDek, dek_t *plainDek) {
	return dek_decrypt_dek(userid, encDek, plainDek);
}

static void copy_kek(kek_t *to, kek_t *from, int kek_type) {
    memcpy(to->buf, from->buf, from->len);
    to->len = from->len;
    to->type = kek_type;
}

static int dek_on_boot(dek_arg_on_boot *evt) {
	int ret = 0;
	int userid = evt->userid;
	int key_arr_idx;

	/*
	 * TODO : lock needed
	 */

	if (!dek_is_persona(userid)) {
		DEK_LOGE("%s invalid userid %d\n", __func__, userid);
		return -EFAULT;
	}
	key_arr_idx = PERSONA_KEY_ARR_IDX(userid);

	if((evt->SDPK_Rpub.len > KEK_MAX_LEN) ||
	        (evt->SDPK_Dpub.len > KEK_MAX_LEN) ||
	        (evt->SDPK_EDpub.len > KEK_MAX_LEN)) {
	    DEK_LOGE("Invalid args\n");
	    DEK_LOGE("SDPK_Rpub.len : %d\n", evt->SDPK_Rpub.len);
	    DEK_LOGE("SDPK_Dpub.len : %d\n", evt->SDPK_Dpub.len);
	    DEK_LOGE("SDPK_EDpub.len : %d\n", evt->SDPK_EDpub.len);
	    return -EINVAL;
	}

    copy_kek(&SDPK_Rpub[key_arr_idx], &evt->SDPK_Rpub, KEK_TYPE_RSA_PUB);
    copy_kek(&SDPK_Dpub[key_arr_idx], &evt->SDPK_Dpub, KEK_TYPE_DH_PUB);
    copy_kek(&SDPK_EDpub[key_arr_idx], &evt->SDPK_EDpub, KEK_TYPE_ECDH256_PUB);

#ifdef CONFIG_SDP_KEY_DUMP
    if(get_sdp_sysfs_key_dump()) {
        dump_all_keys(key_arr_idx);
    }
#endif

	return ret;
}

static int dek_on_device_locked(dek_arg_on_device_locked *evt) {
	int userid = evt->userid;
	int key_arr_idx;

	if (!dek_is_persona(userid)) {
		DEK_LOGE("%s invalid userid %d\n", __func__, userid);
		return -EFAULT;
	}
	key_arr_idx = PERSONA_KEY_ARR_IDX(userid);

	dek_aes_key_free(sdp_tfm[key_arr_idx]);
	sdp_tfm[key_arr_idx] = NULL;

	zero_out((char *)&SDPK_sym[key_arr_idx], sizeof(kek_t));
	zero_out((char *)&SDPK_Rpri[key_arr_idx], sizeof(kek_t));
    zero_out((char *)&SDPK_Dpri[key_arr_idx], sizeof(kek_t));
    zero_out((char *)&SDPK_EDpri[key_arr_idx], sizeof(kek_t));

	ecryptfs_mm_drop_cache(userid);

#ifdef CONFIG_SDP_KEY_DUMP
    if(get_sdp_sysfs_key_dump()) {
        dump_all_keys(key_arr_idx);
    }
#endif

	return 0;
}

static int dek_on_device_unlocked(dek_arg_on_device_unlocked *evt) {
	int userid = evt->userid;
	int key_arr_idx;

	/*
	 * TODO : lock needed
	 */

	if (!dek_is_persona(userid)) {
		DEK_LOGE("%s invalid userid %d\n", __func__, userid);
		return -EFAULT;
	}
	key_arr_idx = PERSONA_KEY_ARR_IDX(userid);

	if((evt->SDPK_sym.len > KEK_MAX_LEN) ||
            (evt->SDPK_Rpri.len > KEK_MAX_LEN) ||
            (evt->SDPK_Dpri.len > KEK_MAX_LEN) ||
			(evt->SDPK_EDpri.len > KEK_MAX_LEN)) {
		DEK_LOGE("%s Invalid args\n", __func__);
		DEK_LOGE("SDPK_sym.len : %d\n", evt->SDPK_sym.len);
		DEK_LOGE("SDPK_Rpri.len : %d\n", evt->SDPK_Rpri.len);
        DEK_LOGE("SDPK_Dpri.len : %d\n", evt->SDPK_Dpri.len);
        DEK_LOGE("SDPK_EDpri.len : %d\n", evt->SDPK_EDpri.len);
		return -EINVAL;
	}

    copy_kek(&SDPK_Rpri[key_arr_idx], &evt->SDPK_Rpri, KEK_TYPE_RSA_PRIV);
    copy_kek(&SDPK_Dpri[key_arr_idx], &evt->SDPK_Dpri, KEK_TYPE_DH_PRIV);
    copy_kek(&SDPK_EDpri[key_arr_idx], &evt->SDPK_EDpri, KEK_TYPE_ECDH256_PRIV);
    copy_kek(&SDPK_sym[key_arr_idx], &evt->SDPK_sym, KEK_TYPE_SYM);

	sdp_tfm[key_arr_idx] = dek_aes_key_setup(evt->SDPK_sym.buf, evt->SDPK_sym.len);
	if (IS_ERR(sdp_tfm[key_arr_idx])) {
		DEK_LOGE("error setting up key\n");
		dek_add_to_log(evt->userid, "error setting up key");
		sdp_tfm[key_arr_idx] = NULL;
	}

#ifdef CONFIG_SDP_KEY_DUMP
	if(get_sdp_sysfs_key_dump()) {
	    dump_all_keys(key_arr_idx);
	}
#endif

	return 0;
}

static int dek_on_user_added(dek_arg_on_user_added *evt) {
	int userid = evt->userid;
	int key_arr_idx;

	/*
	 * TODO : lock needed
	 */

	if (!dek_is_persona(userid)) {
		DEK_LOGE("%s invalid userid %d\n", __func__, userid);
		return -EFAULT;
	}
	key_arr_idx = PERSONA_KEY_ARR_IDX(userid);

	if((evt->SDPK_Rpub.len > KEK_MAX_LEN) ||
	        (evt->SDPK_Dpub.len > KEK_MAX_LEN) ||
	        (evt->SDPK_EDpub.len > KEK_MAX_LEN)) {
		DEK_LOGE("Invalid args\n");
		DEK_LOGE("SDPK_Rpub.len : %d\n", evt->SDPK_Rpub.len);
        DEK_LOGE("SDPK_Dpub.len : %d\n", evt->SDPK_Dpub.len);
        DEK_LOGE("SDPK_EDpub.len : %d\n", evt->SDPK_EDpub.len);
		return -EINVAL;
	}

    copy_kek(&SDPK_Rpub[key_arr_idx], &evt->SDPK_Rpub, KEK_TYPE_RSA_PUB);
    copy_kek(&SDPK_Dpub[key_arr_idx], &evt->SDPK_Dpub, KEK_TYPE_DH_PUB);
    copy_kek(&SDPK_EDpub[key_arr_idx], &evt->SDPK_EDpub, KEK_TYPE_ECDH256_PUB);

#ifdef CONFIG_SDP_KEY_DUMP
    if(get_sdp_sysfs_key_dump()) {
        dump_all_keys(key_arr_idx);
    }
#endif

	return 0;
}

static int dek_on_user_removed(dek_arg_on_user_removed *evt) {
	int userid = evt->userid;
	int key_arr_idx;

	/*
	 * TODO : lock needed
	 */

	if (!dek_is_persona(userid)) {
		DEK_LOGE("%s invalid userid %d\n", __func__, userid);
		return -EFAULT;
	}
	key_arr_idx = PERSONA_KEY_ARR_IDX(userid);

	zero_out((char *)&SDPK_sym[key_arr_idx], sizeof(kek_t));
	zero_out((char *)&SDPK_Rpub[key_arr_idx], sizeof(kek_t));
	zero_out((char *)&SDPK_Rpri[key_arr_idx], sizeof(kek_t));
	zero_out((char *)&SDPK_Dpub[key_arr_idx], sizeof(kek_t));
	zero_out((char *)&SDPK_Dpri[key_arr_idx], sizeof(kek_t));
    zero_out((char *)&SDPK_EDpub[key_arr_idx], sizeof(kek_t));
    zero_out((char *)&SDPK_EDpri[key_arr_idx], sizeof(kek_t));

#ifdef CONFIG_SDP_KEY_DUMP
    if(get_sdp_sysfs_key_dump()) {
        dump_all_keys(key_arr_idx);
    }
#endif

	dek_aes_key_free(sdp_tfm[key_arr_idx]);
	sdp_tfm[key_arr_idx] = NULL;

	return 0;
}

// I'm thinking... if minor id can represent persona id

static long dek_do_ioctl_evt(unsigned int minor, unsigned int cmd,
		unsigned long arg) {
	long ret = 0;
	void __user *ubuf = (void __user *)arg;
	void *cleanup = NULL;
	unsigned int size;

	switch (cmd) {
	/*
	 * Event while booting.
	 *
	 * This event comes per persona, the driver is responsible to
	 * verify things good whether it's compromised.
	 */
	case DEK_ON_BOOT: {
		dek_arg_on_boot *evt = kzalloc(sizeof(dek_arg_on_boot), GFP_KERNEL);
		
		DEK_LOGD("DEK_ON_BOOT\n");

		if (evt == NULL) {
			ret = -ENOMEM;
			goto err;
		}		
		cleanup = evt;
		size = sizeof(dek_arg_on_boot);

		if(copy_from_user(evt, ubuf, size)) {
			DEK_LOGE("can't copy from user evt\n");
			ret = -EFAULT;
			goto err;
		}
		ret = dek_on_boot(evt);
		if (ret < 0) {
			dek_add_to_log(evt->userid, "Boot failed");
			goto err;
		}
		dek_add_to_log(evt->userid, "Booted");
		break;
	}
	/*
	 * Event when device is locked.
	 *
	 * Nullify private key which prevents decryption.
	 */
	case DEK_ON_DEVICE_LOCKED: {
		dek_arg_on_device_locked *evt = kzalloc(sizeof(dek_arg_on_device_locked), GFP_KERNEL);
		
		DEK_LOGD("DEK_ON_DEVICE_LOCKED\n");
		if (evt == NULL) {
			ret = -ENOMEM;
			goto err;
		}		
		cleanup = evt;
		size = sizeof(dek_arg_on_device_locked);

		if(copy_from_user(evt, ubuf, size)) {
			DEK_LOGE("can't copy from user evt\n");
			ret = -EFAULT;
			goto err;
		}
		ret = dek_on_device_locked(evt);
		if (ret < 0) {
			dek_add_to_log(evt->userid, "Lock failed");
			goto err;
		}
		dek_add_to_log(evt->userid, "Locked");
		break;
	}
	/*
	 * Event when device unlocked.
	 *
	 * Read private key and decrypt with user password.
	 */
	case DEK_ON_DEVICE_UNLOCKED: {
		dek_arg_on_device_unlocked *evt = kzalloc(sizeof(dek_arg_on_device_unlocked), GFP_KERNEL);

		DEK_LOGD("DEK_ON_DEVICE_UNLOCKED\n");
		if (evt == NULL) {
			ret = -ENOMEM;
			goto err;
		}		
		cleanup = evt;
		size = sizeof(dek_arg_on_device_unlocked);

		if(copy_from_user(evt, ubuf, size)) {
			DEK_LOGE("can't copy from user evt\n");
			ret = -EFAULT;
			goto err;
		}
		ret = dek_on_device_unlocked(evt);
		if (ret < 0) {
			dek_add_to_log(evt->userid, "Unlock failed");
			goto err;
		}
		dek_add_to_log(evt->userid, "Unlocked");
		break;
	}
	/*
	 * Event when new user(persona) added.
	 *
	 * Generate RSA public key and encrypt private key with given
	 * password. Also pub-key and encryped priv-key have to be stored
	 * in a file system.
	 */
	case DEK_ON_USER_ADDED: {
		dek_arg_on_user_added *evt = kzalloc(sizeof(dek_arg_on_user_added), GFP_KERNEL);

		DEK_LOGD("DEK_ON_USER_ADDED\n");
		if (evt == NULL) {
			ret = -ENOMEM;
			goto err;
		}		
		cleanup = evt;
		size = sizeof(dek_arg_on_user_added);

		if(copy_from_user(evt, ubuf, size)) {
			DEK_LOGE("can't copy from user evt\n");
			ret = -EFAULT;
			goto err;
		}
		ret = dek_on_user_added(evt);
		if (ret < 0) {
			dek_add_to_log(evt->userid, "Add user failed");
			goto err;
		}
		dek_add_to_log(evt->userid, "Added user");
		break;
	}
	/*
	 * Event when user is removed.
	 *
	 * Remove pub-key file & encrypted priv-key file.
	 */
	case DEK_ON_USER_REMOVED: {
		dek_arg_on_user_removed *evt = kzalloc(sizeof(dek_arg_on_user_removed), GFP_KERNEL);
		
		DEK_LOGD("DEK_ON_USER_REMOVED\n");
		if (evt == NULL) {
			ret = -ENOMEM;
			goto err;
		}		
		cleanup = evt;
		size = sizeof(dek_arg_on_user_removed);

		if(copy_from_user(evt, ubuf, size)) {
			DEK_LOGE("can't copy from user evt\n");
			ret = -EFAULT;
			goto err;
		}
		ret = dek_on_user_removed(evt);
		if (ret < 0) {
			dek_add_to_log(evt->userid, "Remove user failed");
			goto err;
		}
		dek_add_to_log(evt->userid, "Removed user");
		break;
	}
	/*
	 * Event when password changed.
	 *
	 * Encrypt SDPK_Rpri with new password and store it.
	 */
	case DEK_ON_CHANGE_PASSWORD: {
		DEK_LOGD("DEK_ON_CHANGE_PASSWORD << deprecated. SKIP\n");
		ret = 0;
		dek_add_to_log(0, "Changed password << deprecated");
		break;
	}

	case DEK_DISK_CACHE_CLEANUP: {
		dek_arg_disk_cache_cleanup *evt = kzalloc(sizeof(dek_arg_disk_cache_cleanup), GFP_KERNEL);

		DEK_LOGD("DEK_DISK_CACHE_CLEANUP\n");
		if (evt == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		cleanup = evt;
		size = sizeof(dek_arg_on_user_removed);

		if(copy_from_user(evt, ubuf, size)) {
			DEK_LOGE("can't copy from user evt\n");
			ret = -EFAULT;
			goto err;
		}
		if (!dek_is_persona(evt->userid)) {
			DEK_LOGE("%s invalid userid %d\n", __func__, evt->userid);
			ret = -EFAULT;
			goto err;
		}

		ecryptfs_mm_drop_cache(evt->userid);
		ret = 0;
		dek_add_to_log(evt->userid, "Disk cache clean up");
		break;
	}
	default:
		DEK_LOGE("%s case default\n", __func__);
		ret = -EINVAL;
		break;
	}

err:
	if (cleanup) {
		zero_out((char *)cleanup, size);
		kfree(cleanup);
	}
	return ret;
}

static long dek_do_ioctl_req(unsigned int minor, unsigned int cmd,
		unsigned long arg) {
	long ret = 0;
	void __user *ubuf = (void __user *)arg;

	switch (cmd) {
    case DEK_IS_KEK_AVAIL: {
        dek_arg_is_kek_avail req;

        DEK_LOGD("DEK_IS_KEK_AVAIL\n");

        memset(&req, 0, sizeof(dek_arg_is_kek_avail));
        if(copy_from_user(&req, ubuf, sizeof(req))) {
            DEK_LOGE("can't copy from user\n");
            ret = -EFAULT;
            goto err;
        }

        req.ret = is_kek_available(req.userid, req.kek_type);
        if(req.ret < 0) {
            DEK_LOGE("is_kek_available(id:%d, kek:%d) error\n",
                    req.userid, req.kek_type);
            ret = -ENOENT;
            goto err;
        }

        if(copy_to_user(ubuf, &req, sizeof(req))) {
            DEK_LOGE("can't copy to user req\n");
            zero_out((char *)&req, sizeof(dek_arg_is_kek_avail));
            ret = -EFAULT;
            goto err;
        }

        ret = 0;
    }
    break;
	/*
	 * Request to generate DEK.
	 * Generate DEK and return to the user
	 */
	case DEK_GENERATE_DEK: {
		dek_arg_generate_dek req;

		DEK_LOGD("DEK_GENERATE_DEK\n");

		memset(&req, 0, sizeof(dek_arg_generate_dek));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			DEK_LOGE("can't copy from user req\n");
			ret = -EFAULT;
			goto err;
		}
		dek_generate_dek(req.userid, &req.dek);
		if(copy_to_user(ubuf, &req, sizeof(req))) {
			DEK_LOGE("can't copy to user req\n");
			zero_out((char *)&req, sizeof(dek_arg_generate_dek));
			ret = -EFAULT;
			goto err;
		}
		zero_out((char *)&req, sizeof(dek_arg_generate_dek));
		break;
	}
	/*
	 * Request to encrypt given DEK.
	 *
	 * encrypt dek and return to the user
	 */
	case DEK_ENCRYPT_DEK: {
		dek_arg_encrypt_dek req;

		DEK_LOGD("DEK_ENCRYPT_DEK\n");

		memset(&req, 0, sizeof(dek_arg_encrypt_dek));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			DEK_LOGE("can't copy from user req\n");
			zero_out((char *)&req, sizeof(dek_arg_encrypt_dek));
			ret = -EFAULT;
			goto err;
		}
		ret = dek_encrypt_dek(req.userid,
				&req.plain_dek, &req.enc_dek);
		if (ret < 0) {
			zero_out((char *)&req, sizeof(dek_arg_encrypt_dek));
			goto err;
		}
		if(copy_to_user(ubuf, &req, sizeof(req))) {
			DEK_LOGE("can't copy to user req\n");
			zero_out((char *)&req, sizeof(dek_arg_encrypt_dek));
			ret = -EFAULT;
			goto err;
		}
		zero_out((char *)&req, sizeof(dek_arg_encrypt_dek));
		break;
	}
	/*
	 * Request to decrypt given DEK.
	 *
	 * Decrypt dek and return to the user.
	 * When device is locked, private key is not available, so
	 * the driver must return EPERM or some kind of error.
	 */
	case DEK_DECRYPT_DEK: {
		dek_arg_decrypt_dek req;

		DEK_LOGD("DEK_DECRYPT_DEK\n");

		memset(&req, 0, sizeof(dek_arg_decrypt_dek));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			DEK_LOGE("can't copy from user req\n");
			zero_out((char *)&req, sizeof(dek_arg_decrypt_dek));
			ret = -EFAULT;
			goto err;
		}
		ret = dek_decrypt_dek(req.userid,
				&req.enc_dek, &req.plain_dek);
		if (ret < 0) {
			zero_out((char *)&req, sizeof(dek_arg_decrypt_dek));
			goto err;
		}
		if(copy_to_user(ubuf, &req, sizeof(req))) {
			DEK_LOGE("can't copy to user req\n");
			zero_out((char *)&req, sizeof(dek_arg_decrypt_dek));
			ret = -EFAULT;
			goto err;
		}
		zero_out((char *)&req, sizeof(dek_arg_decrypt_dek));
		break;
	}

	default:
		DEK_LOGE("%s case default\n", __func__);
		ret = -EINVAL;
		break;
	}

	return ret;
err:
	return ret;
}

int is_kek_available(int userid, int kek_type) {
    int ret;
    int key_arr_idx = PERSONA_KEY_ARR_IDX(userid);

    switch(kek_type) {
    case KEK_TYPE_SYM:
        if (SDPK_sym[key_arr_idx].len > 0)
            ret = 1;
        else
            ret = 0;
        break;
    case KEK_TYPE_RSA_PUB:
        if (SDPK_Rpub[key_arr_idx].len > 0)
            ret = 1;
        else
            ret = 0;
        break;
    case KEK_TYPE_RSA_PRIV:
        if (SDPK_Rpri[key_arr_idx].len > 0)
            ret = 1;
        else
            ret = 0;
        break;
    case KEK_TYPE_DH_PUB:
        if (SDPK_Dpub[key_arr_idx].len > 0)
            ret = 1;
        else
            ret = 0;
        break;
    case KEK_TYPE_DH_PRIV:
        if (SDPK_Dpri[key_arr_idx].len > 0)
            ret = 1;
        else
            ret = 0;
        break;
    case KEK_TYPE_ECDH256_PUB:
        if (SDPK_EDpub[key_arr_idx].len > 0)
            ret = 1;
        else
            ret = 0;
        break;
    case KEK_TYPE_ECDH256_PRIV:
        if (SDPK_EDpri[key_arr_idx].len > 0)
            ret = 1;
        else
            ret = 0;
        break;
    default:
        printk("%s : unknown kek type:%d\n", __func__, kek_type);
        ret = -ENOENT;
        break;
    }

    return ret;
}

static long dek_do_ioctl_kek(unsigned int minor, unsigned int cmd,
		unsigned long arg) {
	long ret = 0;
	void __user *ubuf = (void __user *)arg;

	switch (cmd) {
	case DEK_GET_KEK: {
		dek_arg_get_kek req;
		int requested_type = 0;
		int userid;
		int key_arr_idx;

		DEK_LOGD("DEK_GET_KEK\n");

		memset(&req, 0, sizeof(dek_arg_get_kek));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			DEK_LOGE("can't copy from user kek\n");
			ret = -EFAULT;
			goto err;
		}

		userid = req.userid;
		if (!dek_is_persona(userid)) {
			DEK_LOGE("%s invalid userid %d\n", __func__, userid);
			return -EFAULT;
		}
		key_arr_idx = PERSONA_KEY_ARR_IDX(userid);

		requested_type = req.kek_type;
		req.key.len = 0;
		req.key.type = -1;

		switch(requested_type) {
		case KEK_TYPE_SYM:
			if (SDPK_sym[key_arr_idx].len > 0) {
                copy_kek(&req.key, &SDPK_sym[key_arr_idx], KEK_TYPE_SYM);
				DEK_LOGD("SDPK_sym len : %d\n", req.key.len);
			}else{
				DEK_LOGE("SDPK_sym not-available\n");
				ret = -EIO;
				goto err;
			}
			break;
		case KEK_TYPE_RSA_PUB:
			if (SDPK_Rpub[key_arr_idx].len > 0) {
                copy_kek(&req.key, &SDPK_Rpub[key_arr_idx], KEK_TYPE_RSA_PUB);
				DEK_LOGD("SDPK_Rpub len : %d\n", req.key.len);
			} else {
				DEK_LOGE("SDPK_Rpub not-available\n");
				ret = -EIO;
				goto err;
			}
			break;
		case KEK_TYPE_RSA_PRIV:
#ifdef CONFIG_SDP_IOCTL_PRIV
			if (SDPK_Rpri[key_arr_idx].len > 0) {
                copy_kek(&req.key, &SDPK_Rpri[key_arr_idx], KEK_TYPE_RSA_PRIV);
				DEK_LOGD("SDPK_Rpri len : %d\n", req.key.len);
			} else {
				DEK_LOGE("SDPK_Rpri not-available\n");
				ret = -EIO;
				goto err;
			}
#else
			DEK_LOGE("SDPK_Rpri not exposed\n");
			ret = -EOPNOTSUPP;
			goto err;
#endif
			break;
		case KEK_TYPE_DH_PUB:
			if (SDPK_Dpub[key_arr_idx].len > 0) {
                copy_kek(&req.key, &SDPK_Dpub[key_arr_idx], KEK_TYPE_DH_PUB);
				DEK_LOGD("SDPK_Dpub len : %d\n", req.key.len);
			} else {
				DEK_LOGE("SDPK_Dpub not-available\n");
				ret = -EIO;
				goto err;
			}

			break;
		case KEK_TYPE_DH_PRIV:
#ifdef CONFIG_SDP_IOCTL_PRIV
			if (SDPK_Dpri[key_arr_idx].len > 0) {
                copy_kek(&req.key, &SDPK_Dpri[key_arr_idx], KEK_TYPE_DH_PRIV);
				DEK_LOGD("SDPK_Dpri len : %d\n", req.key.len);
			} else {
				DEK_LOGE("SDPK_Dpri not-available\n");
				ret = -EIO;
				goto err;
			}
#else
			DEK_LOGE("SDPK_Dpri not exposed\n");
			ret = -EOPNOTSUPP;
			goto err;
#endif
			break;
        case KEK_TYPE_ECDH256_PUB:
            if (SDPK_EDpub[key_arr_idx].len > 0) {
                copy_kek(&req.key, &SDPK_EDpub[key_arr_idx], KEK_TYPE_ECDH256_PUB);
                DEK_LOGD("SDPK_EDpub len : %d\n", req.key.len);
            } else {
                DEK_LOGE("SDPK_EDpub not-available\n");
                ret = -EIO;
                goto err;
            }

            break;
        case KEK_TYPE_ECDH256_PRIV:
#ifdef CONFIG_SDP_IOCTL_PRIV
            if (SDPK_EDpri[key_arr_idx].len > 0) {
                copy_kek(&req.key, &SDPK_EDpub[key_arr_idx], KEK_TYPE_ECDH256_PRIV);
                DEK_LOGD("SDPK_EDpri len : %d\n", req.key.len);
            } else {
                DEK_LOGE("SDPK_EDpri not-available\n");
                ret = -EIO;
                goto err;
            }
#else
            DEK_LOGE("SDPK_EDpri not exposed\n");
            ret = -EOPNOTSUPP;
            goto err;
#endif
            break;
		default:
			DEK_LOGE("invalid key type\n");
			ret = -EINVAL;
			goto err;
			break;
		}

		if(copy_to_user(ubuf, &req, sizeof(req))) {
			DEK_LOGE("can't copy to user kek\n");
			zero_out((char *)&req, sizeof(dek_arg_get_kek));
			ret = -EFAULT;
			goto err;
		}
		zero_out((char *)&req, sizeof(dek_arg_get_kek));
		break;
	}
	default:
		DEK_LOGE("%s case default\n", __func__);
		ret = -EINVAL;
		break;
	}

	return ret;
err:
	return ret;
}

static long dek_ioctl_evt(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	unsigned int minor;
	if(!is_system_server()) {
		DEK_LOGE("Current process can't access evt device\n");
		DEK_LOGE("Current process info :: "
				"uid=%u gid=%u euid=%u egid=%u suid=%u sgid=%u "
				"fsuid=%u fsgid=%u\n",
				current_uid(), current_gid(), current_euid(),
				current_egid(), current_suid(), current_sgid(),
				current_fsuid(), current_fsgid());
		dek_add_to_log(000, "Access denied to evt device");
		return -EACCES;
	}

	minor = iminor(file->f_path.dentry->d_inode);
	return dek_do_ioctl_evt(minor, cmd, arg);
}

static long dek_ioctl_req(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	unsigned int minor;
	if(!is_container_app() && !is_root()) {
		DEK_LOGE("Current process can't access req device\n");
		DEK_LOGE("Current process info :: "
				"uid=%u gid=%u euid=%u egid=%u suid=%u sgid=%u "
				"fsuid=%u fsgid=%u\n",
				current_uid(), current_gid(), current_euid(),
				current_egid(), current_suid(), current_sgid(),
				current_fsuid(), current_fsgid());
		dek_add_to_log(000, "Access denied to req device");
		return -EACCES;
	}

	minor = iminor(file->f_path.dentry->d_inode);
	return dek_do_ioctl_req(minor, cmd, arg);
}

static long dek_ioctl_kek(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	unsigned int minor;
	if(!is_container_app() && !is_root()) {
		DEK_LOGE("Current process can't access kek device\n");
		DEK_LOGE("Current process info :: "
				"uid=%u gid=%u euid=%u egid=%u suid=%u sgid=%u "
				"fsuid=%u fsgid=%u\n",
				current_uid(), current_gid(), current_euid(),
				current_egid(), current_suid(), current_sgid(),
				current_fsuid(), current_fsgid());
		dek_add_to_log(000, "Access denied to kek device");
		return -EACCES;
	}

	minor = iminor(file->f_path.dentry->d_inode);
	return dek_do_ioctl_kek(minor, cmd, arg);
}

/*
 * DAR engine log
 */

static int dek_open_log(struct inode *inode, struct file *file)
{
	DEK_LOGD("dek_open_log\n");
	return 0;
}

static int dek_release_log(struct inode *ignored, struct file *file)
{
	DEK_LOGD("dek_release_log\n");
	return 0;
}

static ssize_t dek_read_log(struct file *file, char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	struct log_struct *tmp = NULL;
	char log_buf[256];
	int log_buf_len;

	// build error
	//DEK_LOGD("dek_read_log, len=%d, off=%ld, log_count=%d\n",
	//		len, (long int)*off, log_count);

	if (list_empty(&log_buffer.list)) {
		DEK_LOGD("process %i (%s) going to sleep\n",
				current->pid, current->comm);
		flag = 0;
		wait_event_interruptible(wq, flag != 0);

	}
	flag = 0;

	spin_lock(&log_buffer.list_lock);
	if (!list_empty(&log_buffer.list)) {
		tmp = list_first_entry(&log_buffer.list, struct log_struct, list);
		if (tmp != NULL) {
			memcpy(&log_buf, tmp->buf, tmp->len);
			log_buf_len = tmp->len;
			list_del(&tmp->list);
			kfree(tmp);
			log_count--;
			spin_unlock(&log_buffer.list_lock);

			ret = copy_to_user(buffer, log_buf, log_buf_len);
			if (ret) {
				DEK_LOGE("dek_read_log, copy_to_user fail, ret=%d, len=%d\n",
						ret, log_buf_len);
				return -EFAULT;
			}
			len = log_buf_len;
			*off = log_buf_len;

		} else {
			DEK_LOGD("dek_read_log, tmp == null\n");
			len = 0;
			spin_unlock(&log_buffer.list_lock);
		}
	} else {
		DEK_LOGD("dek_read_log, list empty\n");
		len = 0;
		spin_unlock(&log_buffer.list_lock);
	}

	return len;
}

static void dek_add_to_log(int userid, char * buffer) {
	struct timespec ts;
	struct log_struct *tmp = (struct log_struct*)kmalloc(sizeof(struct log_struct), GFP_KERNEL);

	if (tmp) {
		INIT_LIST_HEAD(&tmp->list);

		getnstimeofday(&ts);
		tmp->len = sprintf(tmp->buf, "%ld.%.3ld|%d|%s|%d|%s\n",
				(long)ts.tv_sec,
				(long)ts.tv_nsec / 1000000,
				current->pid,
				current->comm,
				userid,
				buffer);

		spin_lock(&log_buffer.list_lock);
		list_add_tail(&(tmp->list), &(log_buffer.list));
		log_count++;
		if (log_count > DEK_LOG_COUNT) {
			DEK_LOGD("dek_add_to_log - exceeded DEK_LOG_COUNT\n");
			tmp = list_first_entry(&log_buffer.list, struct log_struct, list);
			list_del(&tmp->list);
			kfree(tmp);
			log_count--;
		}
		spin_unlock(&log_buffer.list_lock);

		DEK_LOGD("process %i (%s) awakening the readers, log_count=%d\n",
				current->pid, current->comm, log_count);
		flag = 1;
		wake_up_interruptible(&wq);
	} else {
		DEK_LOGE("dek_add_to_log - failed to allocate buffer\n");
	}
}

const struct file_operations dek_fops_evt = {
		.owner = THIS_MODULE,
		.open = dek_open_evt,
		.release = dek_release_evt,
		.unlocked_ioctl = dek_ioctl_evt,
		.compat_ioctl = dek_ioctl_evt,
};

static struct miscdevice dek_misc_evt = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "dek_evt",
		.fops = &dek_fops_evt,
};

const struct file_operations dek_fops_req = {
		.owner = THIS_MODULE,
		.open = dek_open_req,
		.release = dek_release_req,
		.unlocked_ioctl = dek_ioctl_req,
		.compat_ioctl = dek_ioctl_req,
};

static struct miscdevice dek_misc_req = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "dek_req",
		.fops = &dek_fops_req,
};

const struct file_operations dek_fops_log = {
		.owner = THIS_MODULE,
		.open = dek_open_log,
		.release = dek_release_log,
		.read = dek_read_log,
};

static struct miscdevice dek_misc_log = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "dek_log",
		.fops = &dek_fops_log,
};


const struct file_operations dek_fops_kek = {
		.owner = THIS_MODULE,
		.open = dek_open_kek,
		.release = dek_release_kek,
		.unlocked_ioctl = dek_ioctl_kek,
		.compat_ioctl = dek_ioctl_kek,
};

static struct miscdevice dek_misc_kek = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "dek_kek",
		.fops = &dek_fops_kek,
};

static int __init dek_init(void) {
	int ret;
	int i;

	ret = misc_register(&dek_misc_evt);
	if (unlikely(ret)) {
		DEK_LOGE("failed to register misc_evt device!\n");
		return ret;
	}
	ret = misc_register(&dek_misc_req);
	if (unlikely(ret)) {
		DEK_LOGE("failed to register misc_req device!\n");
		return ret;
	}

	dek_create_sysfs_asym_alg(dek_misc_req.this_device);

	ret = misc_register(&dek_misc_log);
	if (unlikely(ret)) {
		DEK_LOGE("failed to register misc_log device!\n");
		return ret;
	}

	dek_create_sysfs_key_dump(dek_misc_log.this_device);

	ret = misc_register(&dek_misc_kek);
	if (unlikely(ret)) {
		DEK_LOGE("failed to register misc_kek device!\n");
		return ret;
	}

	for(i = 0; i < SDP_MAX_USERS; i++){
		zero_out((char *)&SDPK_sym[i], sizeof(kek_t));
		zero_out((char *)&SDPK_Rpub[i], sizeof(kek_t));
		zero_out((char *)&SDPK_Rpri[i], sizeof(kek_t));
		zero_out((char *)&SDPK_Dpub[i], sizeof(kek_t));
		zero_out((char *)&SDPK_Dpri[i], sizeof(kek_t));
        zero_out((char *)&SDPK_EDpub[i], sizeof(kek_t));
        zero_out((char *)&SDPK_EDpri[i], sizeof(kek_t));
		sdp_tfm[i] = NULL;
	}

	INIT_LIST_HEAD(&log_buffer.list);
	spin_lock_init(&log_buffer.list_lock);
	init_waitqueue_head(&wq);

	printk("dek: initialized\n");
	dek_add_to_log(000, "Initialized");

	return 0;
}

static void __exit dek_exit(void)
{
	printk("dek: unloaded\n");
}

module_init(dek_init)
module_exit(dek_exit)

MODULE_LICENSE("GPL");
