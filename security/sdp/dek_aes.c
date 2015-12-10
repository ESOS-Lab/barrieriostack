
#include <linux/err.h>
#include <sdp/dek_aes.h>

struct crypto_blkcipher *dek_aes_key_setup(unsigned char *key, int len)
{
	struct crypto_blkcipher *sdp_tfm;

	sdp_tfm = crypto_alloc_blkcipher("cbc(aes)", 0, CRYPTO_ALG_ASYNC);
	if (!IS_ERR(sdp_tfm)) {
		crypto_blkcipher_setkey(sdp_tfm, key, len);
	} else {
		printk("dek: failed to alloc blkcipher\n");
	}
	return sdp_tfm;
}


void dek_aes_key_free(struct crypto_blkcipher *sdp_tfm)
{
	crypto_free_blkcipher(sdp_tfm);
}

int dek_aes_encrypt(struct crypto_blkcipher *sdp_tfm, char *src, char *dst, int len) {
	struct blkcipher_desc desc;
	struct scatterlist src_sg, dst_sg;
	int bsize = crypto_blkcipher_blocksize(sdp_tfm);
	u8 iv[bsize];

	memset(&iv, 0, sizeof(iv));
	desc.tfm = sdp_tfm;
	desc.info = iv;
	desc.flags = 0;

	sg_init_one(&src_sg, src, len);
	sg_init_one(&dst_sg, dst, len);

	return crypto_blkcipher_encrypt_iv(&desc, &dst_sg, &src_sg, len);
}

int dek_aes_decrypt(struct crypto_blkcipher *sdp_tfm, char *src, char *dst, int len) {
	struct blkcipher_desc desc;
	struct scatterlist src_sg, dst_sg;
	int bsize = crypto_blkcipher_blocksize(sdp_tfm);
	u8 iv[bsize];

	memset(&iv, 0, sizeof(iv));
	desc.tfm = sdp_tfm;
	desc.info = iv;
	desc.flags = 0;

	sg_init_one(&src_sg, src, len);
	sg_init_one(&dst_sg, dst, len);

	return crypto_blkcipher_decrypt_iv(&desc, &dst_sg, &src_sg, len);
}
