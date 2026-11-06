/*
 * Copyright (c) 2026, edk2-a733 contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#ifndef FES_REGS_H
#define FES_REGS_H

/* CCU: SMHC module clocks and bus gates (A733 type1 layout). */
#define FES_CCU_SMHC0_CLK	0x02002D00UL
#define FES_CCU_SMHC0_BGR	0x02002D0CUL
#define FES_CCU_SMHC2_CLK	0x02002D20UL

/* PRCM: port F power switch (PF drives the SD slot). */
#define FES_PF_CFG_BASE		0x02000300UL
#define FES_PF_POWER_CTRL	0x02000350UL
#define FES_PF_POWER_STAT	0x02000348UL

/* SMHC controller instances. */
#define FES_SMHC0_BASE		0x04020000UL
#define FES_SMHC2_CLKCR		0x04022004UL

/* PIO: first bank's configuration registers. */
#define FES_PIO_BANK_BASE	0x02000080UL

#endif
