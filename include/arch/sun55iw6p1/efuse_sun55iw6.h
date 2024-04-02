/*
 * (C) Copyright 2013-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 */

#ifndef __EFUSE_H__
#define __EFUSE_H__

#include <arch/cpu.h>

#define SID_PRCTL               (SUNXI_SID_BASE + 0x40)
#define SID_PRKEY               (SUNXI_SID_BASE + 0x50)
#define SID_RDKEY               (SUNXI_SID_BASE + 0x60)
#define SJTAG_AT0               (SUNXI_SID_BASE + 0x80)
#define SJTAG_AT1               (SUNXI_SID_BASE + 0x84)
#define SJTAG_S                 (SUNXI_SID_BASE + 0x88)

#define SID_EFUSE               (SUNXI_SID_BASE + 0x200)
#define SID_SECURE_MODE         (SUNXI_SID_BASE + 0xA0)
#define SID_OP_LOCK  (0xAC)

/*efuse power ctl*/
#define EFUSE_HV_SWITCH			(SUNXI_RTC_BASE + 0x204)

#define EFUSE_CHIPD             (0x00) /* 0x0-0xf, 128bits */

/* write protect */
#define EFUSE_WRITE_PROTECT     (0x50) /* 0x40-0x43, 32bits */
/* read  protect */
#define EFUSE_READ_PROTECT      (0x54) /* 0x44-0x47, 32bits */
/* jtag security */
#define EFUSE_LCJS              (0x58)

#define EFUSE_ROTPK             (0x80) /* 0x70-0x8f, 256bits */
#define EFUSE_NV1               (0xC0) /* 0xC8-0xCB, 32 bits */

/* size (bit)*/
#define SID_CHIPID_SIZE			(128)
#define SID_NV1_SIZE			(32)

#endif    /*  #ifndef __EFUSE_H__  */
