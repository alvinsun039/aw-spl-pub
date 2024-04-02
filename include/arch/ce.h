/*
 * (C) Copyright 2018
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * wangwei <wangwei@allwinnertech.com>
 */

#ifndef _SUNXI_CE_H
#define _SUNXI_CE_H

#include <linux/types.h>
#include <config.h>
#include <arch/cpu.h>

struct ecc_verify_params{
	u8 sign_type;
	u8 *p;
	u8 *a;
	u8 *n;
	u8 *gx;
	u8 *gy;
	u8 *r;
	u8 *s;
	u8 *qx;
	u8 *qy;
	u32 p_len;
	u32 a_len;
	u32 n_len;
	u32 gx_len;
	u32 gy_len;
	u32 r_len;
	u32 s_len;
	u32 qx_len;
	u32 qy_len;
};

#if defined(CFG_SUNXI_CE_20)
#include "ce_2.0.h"
#elif defined(CFG_SUNXI_CE_10)
#include "ce_1.0.h"
#elif defined(CFG_SUNXI_CE_21)
#include "ce_2.1.h"
#elif defined(CFG_SUNXI_CE_23)
#include "ce_2.3.h"
#elif defined(CFG_SUNXI_CE_30)
#include "ce_3.0.h"
#else
#error "Unsupported plat"
#endif

#ifndef __ASSEMBLY__
void sunxi_ce_open(void);
void sunxi_ce_close(void);
int sunxi_sha_calc(u8 *dst_addr, u32 dst_len, u8 *src_addr, u32 src_len);

s32 sunxi_rsa_calc(u8 *n_addr, u32 n_len, u8 *e_addr, u32 e_len, u8 *dst_addr,
		   u32 dst_len, u8 *src_addr, u32 src_len);
int sunxi_trng_gen(u8 *trng_buf, u32 trng_len);
#endif

#endif /* _SUNXI_CE_H */
