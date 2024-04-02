#ifndef _SUNXI_TYPES_H
#define _SUNXI_TYPES_H

#include <linux/posix_types.h>
#include <asm/types.h>
#include <stdbool.h>

typedef __SIZE_TYPE__ size_t;
typedef long     ssize_t;


typedef unsigned char		unchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef long long           loff_t;
typedef unsigned char       u_char;


typedef		__u8		uint8_t;
typedef		__u16		uint16_t;
typedef		__u32		uint32_t;
typedef     __s32       int32_t;
typedef		__u64		uint64_t;
typedef		__u64		u_int64_t;
typedef		__s64		int64_t;

#ifndef uintptr_t
#define uintptr_t unsigned long
#endif


#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef __kernel_ptrdiff_t	ptrdiff_t;
#endif

typedef long time_t;

#define __bitwise__
#define __bitwise
typedef __u16 __bitwise __le16;
typedef __u16 __bitwise __be16;
typedef __u32 __bitwise __le32;
typedef __u32 __bitwise __be32;
#if defined(__GNUC__)
typedef __u64 __bitwise __le64;
typedef __u64 __bitwise __be64;
#endif
typedef __u16 __bitwise __sum16;
typedef __u32 __bitwise __wsum;

#endif
