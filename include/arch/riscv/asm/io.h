/*
 * (C) Copyright 2020
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 */

#ifndef __SUNXI_RISCV_IO_H
#define __SUNXI_RISCV_IO_H

#ifdef __KERNEL__

#include <linux/types.h>
#include <asm/barrier.h>
#include <asm/byteorder.h>


#define __arch_getb(a)			(*(unsigned char *)(a))
#define __arch_getw(a)			(*(unsigned short *)(a))
#define __arch_getl(a)			(*(unsigned int *)(a))
#define __arch_getq(a)			(*(unsigned long long *)(a))

#define __arch_putb(v, a)		(*(unsigned char *)(a) = (v))
#define __arch_putw(v, a)		(*(unsigned short *)(a) = (v))
#define __arch_putl(v, a)		(*(unsigned int *)(a) = (v))
#define __arch_putq(v, a)		(*(unsigned long long *)(a) = (v))

#define __raw_writeb(v, a)		__arch_putb(v, a)
#define __raw_writew(v, a)		__arch_putw(v, a)
#define __raw_writel(v, a)		__arch_putl(v, a)
#define __raw_writeq(v, a)		__arch_putq(v, a)

#define __raw_readb(a)			__arch_getb(a)
#define __raw_readw(a)			__arch_getw(a)
#define __raw_readl(a)			__arch_getl(a)
#define __raw_readq(a)			__arch_getq(a)

#define dmb()		mb()
#define __iormb()	rmb()
#define __iowmb()	wmb()

#define		GET_LO32(a)			(((phys_addr_t)(a)) & 0xffffffff)

static inline void writeb(u8 val, volatile void __iomem *addr)
{
	__iowmb();
	__arch_putb(val, addr);
}

static inline void writew(u16 val, volatile void __iomem *addr)
{
	__iowmb();
	__arch_putw(val, addr);
}

#define writel(v, x)	rv_writel(v, (volatile void __iomem *)(x))

static inline void rv_writel(u32 val, volatile void __iomem *addr)
{
	__iowmb();
	__arch_putl(val, addr);
}

static inline void writeq(u64 val, volatile void __iomem *addr)
{
	__iowmb();
	__arch_putq(val, addr);
}

static inline u8 readb(const volatile void __iomem *addr)
{
	u8	val;

	val = __arch_getb(addr);
	__iormb();
	return val;
}

static inline u16 readw(const volatile void __iomem *addr)
{
	u16	val;

	val = __arch_getw(addr);
	__iormb();
	return val;
}

#define readl(x)	rv_readl((const volatile void __iomem *)(x))

static inline u32 rv_readl(const volatile void __iomem *addr)
{
	u32	val;

	val = __arch_getl(addr);
	__iormb();
	return val;
}


static inline u64 readq(const volatile void __iomem *addr)
{
	u64	val;

	val = __arch_getq(addr);
	__iormb();
	return val;
}

#define __raw_base_writeb(val, base, off)	__arch_base_putb(val, base, off)
#define __raw_base_writew(val, base, off)	__arch_base_putw(val, base, off)
#define __raw_base_writel(val, base, off)	__arch_base_putl(val, base, off)

#define __raw_base_readb(base, off)	__arch_base_getb(base, off)
#define __raw_base_readw(base, off)	__arch_base_getw(base, off)
#define __raw_base_readl(base, off)	__arch_base_getl(base, off)

#define out_arch(type, endian, a, v)	__raw_write##type(cpu_to_##endian(v), a)
#define in_arch(type, endian, a)	endian##_to_cpu(__raw_read##type(a))

#define out_le32(a, v)			out_arch(l, le32, a, v)
#define out_le16(a, v)			out_arch(w, le16, a, v)

#define in_le32(a)			in_arch(l, le32, a)
#define in_le16(a)			in_arch(w, le16, a)

#define out_be32(a, v)			out_arch(l, be32, a, v)
#define out_be16(a, v)			out_arch(w, be16, a, v)

#define in_be32(a)			in_arch(l, be32, a)
#define in_be16(a)			in_arch(w, be16, a)

#define out_8(a, v)			__raw_writeb(v, a)
#define in_8(a)				__raw_readb(a)

#define clrbits(type, addr, clear) \
	out_##type((addr), in_##type(addr) & ~(clear))

#define setbits(type, addr, set) \
	out_##type((addr), in_##type(addr) | (set))

#define clrsetbits(type, addr, clear, set) \
	out_##type((addr), (in_##type(addr) & ~(clear)) | (set))

#define clrbits_be32(addr, clear) clrbits(be32, addr, clear)
#define setbits_be32(addr, set) setbits(be32, addr, set)
#define clrsetbits_be32(addr, clear, set) clrsetbits(be32, addr, clear, set)

#define clrbits_le32(addr, clear) clrbits(le32, addr, clear)
#define setbits_le32(addr, set) setbits(le32, addr, set)
#define clrsetbits_le32(addr, clear, set) clrsetbits(le32, addr, clear, set)

#define clrbits_be16(addr, clear) clrbits(be16, addr, clear)
#define setbits_be16(addr, set) setbits(be16, addr, set)
#define clrsetbits_be16(addr, clear, set) clrsetbits(be16, addr, clear, set)

#define clrbits_le16(addr, clear) clrbits(le16, addr, clear)
#define setbits_le16(addr, set) setbits(le16, addr, set)
#define clrsetbits_le16(addr, clear, set) clrsetbits(le16, addr, clear, set)

#define clrbits_8(addr, clear) clrbits(8, addr, clear)
#define setbits_8(addr, set) setbits(8, addr, set)
#define clrsetbits_8(addr, clear, set) clrsetbits(8, addr, clear, set)

#endif	/* __KERNEL__ */

#include <asm-generic/io.h>

#endif	/* __SUNXI_RISCV_IO_H */
