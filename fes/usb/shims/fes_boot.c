/*
 * Copyright (c) 2026, edk2-a733 contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
/*
 * fes_boot.c - FES entry (linked into the spl-pub fes1 build; no _start)
 *
 * boot0 reaches FEL through the detected_r window with the DRAM already
 * trained, so fes1 runs from SRAM at 0x90000.  This file only installs
 * the vector setup, enables the GIC and USB clocks, and hands the EFEX
 * device to the vendor USB stack.  sunxi_usb_main_loop() calls
 * sunxi_usb_init() itself and must not be preceded by another init: a
 * second init is rejected by efex_queue with "already init".
 *
 * The FES state machine depends on the RX ISR (rx_ready_for_data), so the
 * GIC must really work; otherwise the device enumerates but times out at
 * the configuration-descriptor stage.
 */
#include <common.h>

extern int sunxi_usb_dev_register(uint dev_name);
extern int sunxi_usb_main_loop(int delaytime);
extern int arch_interrupt_init(void);
extern void enable_interrupts(void);
extern void fes_fes_vectors_init(void);
extern void clock_set_gic(void);
extern int usb_open_clock(void);
extern int tick_printf(const char *fmt, ...);

/* No _start here: the in-tree FES1 links arch/arm/cpu/armv7/fes1_entry.o
 * (spl-pub), which clears BSS and calls main().  A second _start in this
 * object would keep the archive member out of the link and bypass the
 * training main. */
void fes1_fes_entry(void);

void fes1_fes_entry(void)
{
	tick_printf("[FES] entering EFEX stack\n");

	/* Vector table (VBAR/MVBAR and mode stacks) and full GIC-600 init */
	fes_fes_vectors_init();
	clock_set_gic();	/* BROM FEL leaves the GIC-600 clock off */
	if (arch_interrupt_init()) {
		tick_printf("[FES] gic init failed\n");
		return;
	}

	usb_open_clock();	/* USB clock before any controller register */
	if (sunxi_usb_dev_register(2) < 0) {	/* SUNXI_USB_DEVICE_EFEX */
		tick_printf("[FES] efex register failed\n");
		return;
	}
	tick_printf("[FES] efex registered, usb loop\n");

	enable_interrupts();	/* GIC side is enabled by sunxi_usb_init's irq_enable(188) */
	sunxi_usb_main_loop(0);
}
