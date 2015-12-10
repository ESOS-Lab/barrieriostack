#ifndef _LINUX_DEK_AES_H
#define _LINUX_DEK_AES_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>

struct crypto_blkcipher *dek_aes_key_setup(unsigned char *key, int len);
void dek_aes_key_free(struct crypto_blkcipher *sdp_tfm);
int dek_aes_encrypt(struct crypto_blkcipher *sdp_tfm, char *src, char *dst, int len);
int dek_aes_decrypt(struct crypto_blkcipher *sdp_tfm, char *src, char *dst, int len);

#endif
