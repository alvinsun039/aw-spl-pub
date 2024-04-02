/*
 * (C) Copyright 2020
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 */

#ifndef __ASM_RISCV_TYPES_H
#define __ASM_RISCV_TYPES_H

#include <asm-generic/int-ll64.h>

#ifdef __KERNEL__

#ifdef CONFIG_ARCH_RV64I
#define BITS_PER_LONG 64
#else
#define BITS_PER_LONG 32
#endif

#include <stddef.h>

typedef u32 dma_addr_t;

typedef unsigned long phys_addr_t;
typedef unsigned long phys_size_t;

#endif /* __KERNEL__ */

#endif
