/*
 * (C) Copyright 2023-2026
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * huangrongcun <huangrongcun@allwinnertech.com>
 */

#ifndef _SUNXI_CERT_H__
#define _SUNXI_CERT_H__

#define SUNXI_EXTENSION_ITEM_MAX (8)
#ifdef CFG_SUNXI_VERIFY_MALLCO_SIZE
#define SUNXI_X509_CERTIFF_MAX_LEN (CFG_SUNXI_VERIFY_MALLCO_SIZE)
#else
#define SUNXI_X509_CERTIFF_MAX_LEN (8192)
#endif

typedef struct {
	u8 *n;
	u32 n_len;
	u8 *e;
	u32 e_len;
} sunxi_key_t;

typedef struct {
	int extension_num;
	u8 *name[SUNXI_EXTENSION_ITEM_MAX];
	uint name_len[SUNXI_EXTENSION_ITEM_MAX];
	u8 *value[SUNXI_EXTENSION_ITEM_MAX];
	uint value_len[SUNXI_EXTENSION_ITEM_MAX];
} sunxi_extension_t;

typedef struct {
	u32 public_key_size;
	u8 *public_key_info;
} aw_cert_info_t;

typedef struct {
	long version;
	long serial_num;
	sunxi_key_t pubkey; //x509
	aw_cert_info_t aw_cert_info;
	sunxi_extension_t extension;
} sunxi_certif_info_t;

#endif //_SUNXI_CERT_H__