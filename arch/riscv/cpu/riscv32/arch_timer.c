// SPDX-License-Identifier: GPL-2.0-only
/*
 * (C) Copyright 2023
 *
 */

#include <common.h>
#include <asm/io.h>

/*
 * 64bit arch timer.CNTPCT
 * Freq = 24000000Hz
 */

static inline u64 get_arch_counter(void)
{
	u64 cnt = 0;
	unsigned int upper, lower;
	unsigned int upper_new;

	asm volatile(
			"1:  rdcycleh %[upper]\n"
			"    rdcycle %[lower]\n"
			"    rdcycleh %[upper_new]\n"
			"    bne %[upper], %[upper_new], 1b\n"
			: [upper] "=r"(upper),
			[lower] "=r"(lower),
			[upper_new] "=&r"(upper_new)
			:
			: "memory"
		    );

	cnt = ((unsigned long long)upper << 32) | lower;

	return cnt;
}

/*
 * get current time.(millisecond)
 */
u32 get_sys_ticks(void)
{
	return (u32)get_arch_counter() / 24000;
}

/*
 * get current time.(microsecond)
 */
u32 timer_get_us(void)
{
	return (u32)get_arch_counter() / 24;
}

__weak void udelay(unsigned long us)
{
	u64 t1, t2;

	t1 = get_arch_counter();
	t2 = t1 + us * 24;
	do {
		t1 = get_arch_counter();
	} while (t2 >= t1);
}

__weak void mdelay(unsigned long ms)
{
	udelay(ms * 1000);
}

__weak void __usdelay(unsigned long us)
{
	udelay(us);
}

__weak void __msdelay(unsigned long ms)
{
	mdelay(ms);
}

__weak int timer_init(void)
{
	return 0;
}

/************************************************************
 * sdelay() - simple spin loop.  Will be constant time as
 *  its generally used in bypass conditions only.  This
 *  is necessary until timers are accessible.
 *
 *  not inline to increase chances its in cache when called
 *************************************************************/
__weak void sdelay(unsigned long loops)
{
}

__weak void timer_exit(void)
{
}


