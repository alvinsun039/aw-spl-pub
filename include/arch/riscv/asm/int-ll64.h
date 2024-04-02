/*
 * (C) Copyright 2020
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 */

#ifndef _SUNXI_GENERIC_INT_LL64_H
#define _SUNXI_GENERIC_INT_LL64_H

#ifndef __ASSEMBLY__

typedef signed char __s8;
typedef unsigned char __u8;

typedef signed short __s16;
typedef unsigned short __u16;

typedef signed int __s32;
typedef unsigned int __u32;

#ifdef __GNUC__
__extension__ typedef signed long long __s64;
__extension__ typedef unsigned long long __u64;
#else
typedef signed long long __s64;
typedef unsigned long long __u64;
#endif

typedef __s8  s8;
typedef __u8  u8;
typedef __s16 s16;
typedef __u16 u16;
typedef __s32 s32;
typedef __u32 u32;
typedef __s64 s64;
typedef __u64 u64;

#endif /* __ASSEMBLY__ */


#endif /* _SUNXI_GENERIC_INT_LL64_H */
