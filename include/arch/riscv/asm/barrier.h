/*
 * (C) Copyright 2020
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 */
#ifndef _SUNXI_RISCV_BARRIER_H
#define _SUNXI_RISCV_BARRIER_H

#ifndef __ASSEMBLY__

#define nop()		__asm__ __volatile__ ("nop")

#define SUNXI_RISCV_FENCE(p, s) \
	__asm__ __volatile__ ("fence " #p "," #s : : : "memory")

#define mb()		SUNXI_RISCV_FENCE(iorw, iorw)
#define rmb()		SUNXI_RISCV_FENCE(ir, ir)
#define wmb()		SUNXI_RISCV_FENCE(ow, ow)

#endif /* __ASSEMBLY__ */

#endif /* _SUNXI_RISCV_BARRIER_H */
