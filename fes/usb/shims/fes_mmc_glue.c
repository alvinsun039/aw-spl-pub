/*
 * Copyright (c) 2026, edk2-a733 contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
/* fes_mmc_glue.c - hook sunxi_flash_write to the real media backend */
#include <common.h>
#include <mmc.h>
#include <sunxi_flash.h>
#include "fes_printf.h"
#include "fes_regs.h"
#define printf tick_printf

extern int fes_mmc_write_blocks(void *mmc, unsigned long start,
				const void *buf, unsigned int nblk);
extern int fes_mmc_write_start(void *mmc, unsigned long start,
			       const void *buf, unsigned int nblk);
extern int fes_mmc_write_start_base(uint base, unsigned long start,
				    const void *buf, unsigned int nblk);
extern int fes_mmc_write_wait_base(void);
extern void *fes_mmc_find_device(int dev_num);

/* FES_MMC_DEV: 0 = SD (SMHC0), 2 = eMMC (SMHC2), per board tests */
#ifndef FES_MMC_DEV
#define FES_MMC_DEV 2  /* dev1 does not exist: A733 has SMHC0 and SMHC2 */
#endif

extern int fes_mmc_read_blocks(void *mmc, unsigned long start, void *dst,
			      unsigned int nblk);

extern int sunxi_mmc_init(unsigned dev, unsigned a, unsigned b, unsigned c);

/* The vendored mmc.h snapshot predates HS200, so the core's switch helper
 * has no public prototype here. */
extern int mmc_switch(struct mmc *mmc, u8 set, u8 index, u8 value);


#if FES_MMC_DEV == 0
extern int fes_sd_init(uint base);
extern int fes_sd_write(uint base, unsigned long start, const void *buf,
			unsigned int nblk);
extern int fes_sd_read(uint base, unsigned long start, void *dst,
		       unsigned int nblk);
static int g_sd_mode;
#endif

/* The eMMC sits on the port C pads, which carry two controller functions:
 * ALT4 reaches SMHC3, ALT3 reaches SMHC2.  The vendor sdcard library only
 * knows SMHC0/SMHC2, so this build uses ALT3/SMHC2 (the EDK2 board table
 * documents both as the same card).  Cold FEL never runs boot0's pinmux,
 * so program it here: without it SMHC2 sees no card and every MMC command
 * times out. */
static void fes_pin_cfg(unsigned bank, unsigned pin, unsigned mux,
			unsigned drv, unsigned pull)
{
	unsigned base = FES_PIO_BANK_BASE + bank * 0x80u;
	volatile unsigned *cfg = (volatile unsigned *)(base + (pin >> 3) * 4);
	volatile unsigned *drvreg =
		(volatile unsigned *)(base + 0x20 + (pin >> 3) * 4);
	volatile unsigned *pullreg =
		(volatile unsigned *)(base + 0x30 + (pin >> 4) * 4);
	unsigned sh = (pin & 7) * 4, ps = (pin & 15) * 2;

	*cfg = (*cfg & ~(0xfu << sh)) | ((mux & 0xf) << sh);
	*drvreg = (*drvreg & ~(0xfu << sh)) | ((drv & 0xf) << sh);
	*pullreg = (*pullreg & ~(3u << ps)) | ((pull & 3) << ps);
}

static void fes_emmc_pins(void)
{
	/* PC0 DS pull-down, PC1 RST, PC5 CLK, PC6 CMD, D0..D7; ALT4, drv 1 */
	static const unsigned char pins[] =
		{ 0, 1, 5, 6, 8, 9, 10, 11, 13, 14, 15, 16 };
	unsigned i;

	for (i = 0; i < sizeof(pins); i++)
		fes_pin_cfg(2, pins[i], 3, 1, pins[i] == 0 ? 2 : 1);
}

static void *fes_mmc_handle(void)
{
	static void *mmc;
	int rc;

#if FES_MMC_DEV == 0
	/* SD (SMHC0): pinmux, gate, card power and a local init sequence */
	{
		static int sd_tried;
		if (!sd_tried) {
			volatile uint *pfA = (volatile uint *)FES_PF_CFG_BASE;
			volatile uint i_d;
			sd_tried = 1;
			fes_dbg("[FES] sd self-init dev0\n");
			/* GPIO: PF0-5 to sdc0, mux 2, drive 3, pull-up 1.
			 * The type4 pinctrl layout puts bank F at 0x300 (0x280
			 * is bank E, which earlier code configured by mistake);
			 * mux and drive are 4 bits per pin, pull 2. */
			for (i_d = 0; i_d < 6; i_d++) {
				*pfA = (*pfA & ~(0xfu << (i_d * 4)))
				       | (2u << (i_d * 4));
			}
			for (i_d = 0; i_d < 6; i_d++) {
				*(pfA + 0x20 / 4) =
				    (*(pfA + 0x20 / 4) & ~(0xfu << (i_d * 4)))
				    | (3u << (i_d * 4));
				*(pfA + 0x30 / 4) =
				    (*(pfA + 0x30 / 4) & ~(3u << (i_d * 2)))
				    | (1u << (i_d * 2));
			}
			fes_dbg("[FES] sd cfg0=%x drv=%x pul=%x\n",
			       *pfA, *(pfA + 0x20 / 4), *(pfA + 0x30 / 4));
			/* Gate: BGR bit0 (gate), bit16 (reset); CLK bit31 */
			*(volatile uint *)FES_CCU_SMHC0_BGR |=
			    (1u << 0) | (1u << 16);
			*(volatile uint *)FES_CCU_SMHC0_CLK |= (1u << 31);
			/* Port F power (type4 pf-switch sequence):
			 * POW_CTRL 0x350: write 0 = 1.8 V, 1 = 3.3 V;
			 * POW_VAL 0x348 reports bit (bank * 2 = 10). */
			fes_dbg("[FES] sd pow350=%x pow348=%x\n",
			       *(volatile uint *)FES_PF_POWER_CTRL,
			       *(volatile uint *)FES_PF_POWER_STAT);
			if ((*(volatile uint *)FES_PF_POWER_CTRL & 1) == 0) {
				*(volatile uint *)FES_PF_POWER_CTRL = 1;	/* -> 3.3 V */
				for (i_d = 0; i_d < 500000; i_d++)
					;
				fes_dbg("[FES] sd pow350->%x pow348=%x\n",
				       *(volatile uint *)FES_PF_POWER_CTRL,
				       *(volatile uint *)FES_PF_POWER_STAT);
			}
			fes_dbg("[FES] sd bgr=%x st=%x\n",
			       *(volatile uint *)FES_CCU_SMHC0_BGR,
			       *(volatile uint *)(FES_SMHC0_BASE + 0x3c));
			{
				int rci = fes_sd_init(0x04020000UL);
				fes_dbg("[FES] sd init rc=%d\n", rci);
				if (rci == 0) {
					g_sd_mode = 1;
					mmc = (void *)1;	/* fake handle */
				}
			}
		}
		return mmc;
	}
#endif
	if (mmc)
		return mmc;
	fes_dbg("[FES] mmc lazy init dev%d\n", FES_MMC_DEV);
	fes_emmc_pins();
	fes_dbg("[FES] emmc pads pc0..2=%x %x %x\n",
	       *(volatile unsigned *)0x02000180,
	       *(volatile unsigned *)0x02000184,
	       *(volatile unsigned *)0x02000188);
	/* sunxi_mmc_init builds its own struct (base = (dev + 0x4020) << 12)
	 * and registers the device.  The second argument is the bus width:
	 * the disassembly branches on 8 for the 8-bit pinmux; 1-bit was
	 * measured at 6.25 MB/s. */
	rc = sunxi_mmc_init(FES_MMC_DEV, 8, 0, 0);
	fes_dbg("[FES] sunxi_mmc_init rc=%d\n", rc);
	mmc = fes_mmc_find_device(FES_MMC_DEV);
	fes_dbg("[FES] find after init = %x\n", (unsigned)mmc);
	if (mmc) {
		rc = mmc_init(mmc);
		fes_dbg("[FES] mmc_init rc=%d\n", rc);
		if (rc) {
			mmc = NULL;
		} else {
			extern int fes_mmc_switch_8bit(void *mmc);
			struct mmc *m = mmc;
			int hrc;

			fes_mmc_switch_8bit(mmc);
			/* HS200: switch the card first, then the host timing and
			 * clock.  The vendor core's set_ios programs the HS200
			 * sample delay; verification below falls back to HS. */
			hrc = mmc_switch(mmc, 1 /* CMD_SET_NORMAL */,
					 185 /* HS_TIMING */, 2 /* HS200 */);
			if (!hrc) {
				extern int fes_mmc_read_blocks(void *mmc,
					unsigned long start, void *dst,
					unsigned int nblk);
				unsigned char *vb = (unsigned char *)0x4B800000UL;

				m->speed_mode = 3;	/* HS200_SDR104 */
				/* sunxi_mmc_init left the module at 800M/8 =
				 * 100 MHz (50 MHz card).  Raise it to 800M/2 =
				 * 400 MHz; the driver's divider then gives
				 * 200 MHz for HS200. */
				*(volatile unsigned *)FES_CCU_SMHC2_CLK = 0x81000002u;
				mmc_set_clock(mmc, 200000000, 0);
				fes_dbg("[FES] emmc clkcr=%x ccu=%x\n",
				       *(volatile unsigned *)FES_SMHC2_CLKCR,
				       *(volatile unsigned *)FES_CCU_SMHC2_CLK);
				fes_dbg("[FES] emmc clock=%u speed=%u ccu2d20=%x\n",
				       m->clock, m->speed_mode,
				       *(volatile unsigned *)FES_CCU_SMHC2_CLK);
				if (fes_mmc_read_blocks(mmc, 256, vb, 1) == 0 &&
				    vb[0] == 0xbe && vb[1] == 0x02 &&
				    vb[2] == 0x00 && vb[3] == 0xea) {
					printf("[FES] emmc HS200 ok\n");
				} else {
					printf("[FES] emmc HS200 verify failed, back to HS\n");
					mmc_switch(mmc, 1, 185, 1);
					m->speed_mode = 1;
					mmc_set_clock(mmc, 50000000, 0);
				}
			} else {
				printf("[FES] emmc HS200 switch failed %d\n", hrc);
			}
		}
	}
	return mmc;
}

extern int fes_mmc_read_blocks_self(void *mmc, unsigned long start, void *dst,
			     unsigned int nblk);

#define VB_ADDR	0x4B800000UL	/* DRAM scratch buffer (valid after training) */

static u32 fes_cnt_ms(void)
{
	u32 v;
	asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(v));	/* CNTPCT: read-only, do not write CTL */
	return v / 24000u;
}

/* Non-blocking media write: issues the transfer and returns while the
 * IDMAC still runs, so the USB receive path can proceed.  The result is
 * reported by fes_flash_write_wait(). */
int fes_flash_write_start(unsigned int start_block, unsigned int nblock,
			  void *buffer)
{
#if FES_MMC_DEV == 0
	if (g_sd_mode)
		return fes_mmc_write_start_base(0x04020000UL, start_block,
						buffer, nblock);
#endif
	{
		/* Same lazy-init trap as fes_flash_write_real: the mode is only
		 * known after fes_mmc_handle(), and handle 1 is fake. */
		void *mmc = fes_mmc_handle();

		if (!mmc)
			return -1;
#if FES_MMC_DEV == 0
		if (g_sd_mode)
			return fes_mmc_write_start_base(0x04020000UL,
							start_block, buffer,
							nblock);
#endif
		return fes_mmc_write_start(mmc, start_block, buffer, nblock);
	}
}

int fes_flash_write_wait(void)
{
	return fes_mmc_write_wait_base();
}

int fes_flash_write_real(unsigned int start_block, unsigned int nblock,
			 void *buffer)
{
	u32 w0 = fes_cnt_ms();
	(void)w0;
#ifdef CONFIG_FES_DEBUG
	printf("[FES] wr req s=%u n=%u buf=%x\n",
	       start_block, nblock, (unsigned)buffer);
#endif
#if FES_MMC_DEV == 0
	if (g_sd_mode)
		return fes_sd_write(0x04020000UL, start_block, buffer, nblock);
#endif
	void *mmc = fes_mmc_handle();
	int rc;

	if (!mmc)
		return -1;
#if FES_MMC_DEV == 0
	/* The mode is only known after the lazy init: when a write arrives
	 * before it, g_sd_mode is just set.  Use the raw base, otherwise the
	 * fake handle 1 reaches fes_mmc_reg_base and crashes. */
	if (g_sd_mode)
		return fes_sd_write(0x04020000UL, start_block, buffer, nblock);
#endif
	rc = fes_mmc_write_blocks(mmc, start_block, buffer, nblock);
#ifdef CONFIG_FES_DEBUG
	printf("[FES] wreq %ums rc=%d\n", fes_cnt_ms() - w0, rc);
#endif
	/* The device-side check duplicates the host read-back (every block
	 * is compared there), so it is off by default to save about 50 ms
	 * per 128 KiB; CONFIG_FES_WB_VERIFY re-enables it. */
#ifdef CONFIG_FES_WB_VERIFY
	{
		u8 *vb = (u8 *)VB_ADDR;
		int rrc = fes_mmc_read_blocks_self(mmc, start_block, vb, nblock);
		if (rrc == 0) {
			int same = (memcmp(vb, buffer, nblock * 512) == 0);
			printf("[FES] wb-verify %s w=%d r=%d (s=%u n=%u)\n",
			       same ? "MATCH" : "FAIL", rc, rrc,
			       start_block, nblock);
		} else {
			printf("[FES] wb-verify readfail r=%d (w=%d)\n", rrc, rc);
		}
	}
#endif
	return rc;
}

int fes_flash_read_real(unsigned int start_block, unsigned int nblock,
			void *buffer)
{
#if FES_MMC_DEV == 0
	if (g_sd_mode)
		return fes_sd_read(0x04020000UL, start_block, buffer, nblock);
#endif
	void *mmc = fes_mmc_handle();
	if (!mmc)
		return -1;
#if FES_MMC_DEV == 0
	if (g_sd_mode)
		return fes_sd_read(0x04020000UL, start_block, buffer, nblock);
#endif
	return fes_mmc_read_blocks(mmc, start_block, buffer, nblock);
}
