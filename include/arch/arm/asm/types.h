#ifndef __SUNXI_ARM_TYPES_H
#define __SUNXI_ARM_TYPES_H

typedef signed char __s8;
typedef unsigned char __u8;

typedef signed short __s16;
typedef unsigned short __u16;

typedef signed int __s32;
typedef unsigned int __u32;

__extension__ typedef  long long __s64;
__extension__ typedef unsigned long long __u64;


typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long long s64;
typedef unsigned long long u64;

#ifdef CONFIG_PHYS_64BIT
typedef unsigned long long phys_addr_t;
typedef unsigned long long phys_size_t;
#else
typedef unsigned long phys_addr_t;
typedef unsigned long phys_size_t;
#endif

#ifdef	CONFIG_ARM64
#define BITS_PER_LONG 64
#else	/* CONFIG_ARM32 */
#define BITS_PER_LONG 32
#endif	/* CONFIG_ARM64 */

#ifdef __aarch64__
typedef long __kernel_ptrdiff_t;
#else
typedef int __kernel_ptrdiff_t;
#endif


#endif

