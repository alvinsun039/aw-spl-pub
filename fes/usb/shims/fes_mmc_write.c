/*
 * Copyright (c) 2026, edk2-a733 contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
/*
 * fes_mmc_write.c - FES1 real-media write backend
 *
 * Approach (from the disassembly): the third argument of the sdcard
 * library's mmc_send_cmd(mmc, cmd, data) drives the data-register setup
 * (b4 branch: direction bits 0x2200/0x2600, multi-block 0x1000, byte
 * count), and the command path (response and busy waits) is complete.
 * A block write can therefore use the library's data-command path.  If
 * that path does not move data through the CPU, fall back to the PIO
 * loop in this file:
 *   A733 SMHC (u-boot-2018 layout): gctrl=0x00 blksz=0x10 bytecnt=0x14
 *   cmd=0x18 arg=0x1c rint=0x38 status=0x3c fifo=0x200
 *   wait while FIFO_FULL (status bit4); the write direction is
 *   GCTRL_ACCESS_BY_AHB (bit31).
 */
#include <common.h>
#include "fes_printf.h"
#include "fes_regs.h"
#define printf tick_printf

/* ---- Library interface (symbols from libsun60iw2p1_sdcard.a) ---- */
extern void *find_mmc_device(int dev_num);
extern int mmc_send_cmd(void *mmc, void *cmd, void *data);

/* mmc_cmd layout (library disassembly: cmdidx@0, arg@4, resp_type/bits@8, response@12) */
struct fes_mmc_cmd {
	unsigned int cmdidx;
	unsigned int arg;
	unsigned int resp_type;
	unsigned int response[4];
};

#define MMC_CMD_WRITE_SINGLE	24
#define MMC_CMD_WRITE_MULTI	25
#define MMC_CMD_READ_SINGLE	17
#define MMC_CMD_READ_MULTI	18
#define MMC_RSP_R1		0x15	/* u-boot R1: PRESENT|CRC|OPCODE */
#define MMC_CMD_READ_SINGLE_OR_MULTI(n) ((n) > 1 ? MMC_CMD_READ_MULTI : MMC_CMD_READ_SINGLE)

/* mmc_data layout (deduced from mmc_send_cmd's accesses):
 *   +0 buffer, +4 flags (bit31 = write), +8 blocks, +c blocksize
 *   (differs from the u-boot layout; follow the library disassembly). */
struct fes_mmc_data {
	union {
		void *dest;
		const void *src;
	};
	unsigned long flags;
	unsigned long blocks;
	unsigned int blocksize;
};
#define MMC_DATA_WRITE	(1UL)
#define MMC_DATA_READ	(0UL)

/* SMHC registers and command bits (u-boot mmc.h layout; PIO path) */
#define SMHC_GCTRL	0x00
#define SMHC_CLKCR	0x04
#define SMHC_WIDTH	0x0c
/* IDMAC (v4p1 descriptor) */
#define SMHC_DMAC	0x80
#define SMHC_DLBA	0x84
#define SMHC_IDST	0x88
#define SMHC_IDIE	0x8c
#define SMHC_VERS	0x300
#define SMHC_NTSR	0x5c
#define FES_DES_ALIGN	16
#define FES_DES_MAX	(1 << 12)	/* 4 KB/segment: 32 KB hung (buf_size field); 128 segments at 0xD0000 are safe */
#define FES_DES_N	(524288 / FES_DES_MAX)	/* 128 segments x 16 B = 2 KB at 0xD0000 (bss starts at 0xD1A40) */
#define SMHC_BLKSZ	0x10
#define SMHC_BYTCNT	0x14
#define SMHC_CMD	0x18
#define SMHC_ARG	0x1c
#define SMHC_RINT	0x38
#define SMHC_STATUS	0x3c
#define SMHC_FIFO	0x200
#define GCTRL_ACCESS_BY_AHB	(1U << 31)
#define CMD_START		(1U << 31)
#define CMD_RESP_EXPIRE		(1U << 6)
#define CMD_CHK_CRC		(1U << 8)
#define CMD_DATA_EXPIRE		(1U << 9)
#define CMD_WRITE		(1U << 10)
#define CMD_WAIT_PRE_OVER	(1U << 13)
#define CMD_AUTO_STOP		(1U << 12)
#define CMD_UPCLK_ONLY		(1U << 21)
#define RINT_CMD_DONE		(1U << 2)
#define RINT_DATA_OVER		(1U << 3)
#define RINT_AUTO_CMD_DONE	(1U << 14)
#define RINT_ERR		(0x0002U | 0x0040U | 0x0080U | 0x0100U | \
			0x0200U | 0x0400U | 0x0800U | 0x1000U | \
			0x2000U | 0x8000U)	/* = u-boot 0xBFC2 */
#define STATUS_FIFO_FULL	(1U << 3)
#define STATUS_TXWL		(1U << 1)
#define STATUS_RXWL		(1U << 0)
#define STATUS_FIFO_EMPTY	(1U << 2)
#define STATUS_CARD_BUSY	(1U << 9)
#define STATUS_DATA_FSM_BUSY	(1U << 10)	/* A733: bit10 */
#define STATUS_FIFO_FULL_BIT	STATUS_FIFO_FULL

/* Raw base forwarding, shared by the SD (SMHC0) path and read/write_blocks_base */
static int fes_mmc_pio_read_raw(uint base, unsigned long start, void *dst,
				unsigned int nblk);
static int fes_mmc_dma_read_raw(uint base, unsigned long start, void *dst,
				unsigned int nblk);
static int fes_mmc_pio_write_raw(uint base, unsigned long start,
				 const void *buf, unsigned int nblk);
static int fes_mmc_dma_write_raw(uint base, unsigned long start,
				 const void *buf, unsigned int nblk);

/* The IDMAC write runs in the background between start and wait.  There is
 * one SMHC controller and one descriptor array, so at most one is in flight. */
static int  fes_wr_busy;
static uint fes_wr_base;
static uint fes_wr_nblk;


int fes_mmc_write_start_base(uint base, unsigned long start, const void *buf,
			     unsigned int nblk);
int fes_mmc_write_wait_base(void);

int fes_mmc_read_blocks_base(uint base, unsigned long start, void *dst,
			     unsigned int nblk);
int fes_mmc_write_blocks_base(uint base, unsigned long start,
			      const void *buf, unsigned int nblk);

/* Typed wrappers for the shim/glue layer (avoid clashing with u-boot mmc.h) */
void *fes_mmc_find_device(int dev_num)
{
	return find_mmc_device(dev_num);
}

/* Real read backend (post-burn verify): library mmc_bread(mmc, start, blkcnt, dst) */
extern int mmc_bread(void *mmc, unsigned long start, unsigned int blkcnt,
		     void *dst);
#define STATUS_RXWL		(1U << 0)
#define STATUS_FIFO_EMPTY	(1U << 2)
#define STATUS_CARD_BUSY	(1U << 9)
#define STATUS_DATA_FSM_BUSY	(1U << 10)	/* A733: bit10 */
#define STATUS_FIFO_FULL_BIT	STATUS_FIFO_FULL

static uint fes_mmc_reg_base(void *mmc)
{
	return *(uint *)(*(uint *)((char *)mmc + 32));
}

static inline uint fes_read32(uint a) { return *(volatile uint *)a; }
static inline void fes_write32(uint a, uint v) { *(volatile uint *)a = v; }

/* Controlled read (mirror of the PIO write): CMD18/17, FIFO loop, DATA_OVER */
static int fes_mmc_pio_read_raw(uint base, unsigned long start, void *dst,
			       unsigned int nblk)
{
	uint bytecnt = nblk * 512;
	uint *dstw = (uint *)dst;
	uint i, tmo;

	if (!base)
		return -1;
	/* The IDMAC owns the FIFO when DMAC[7] is set; the CPU then reads
	 * zeros.  Reset it before switching the FIFO to AHB access. */
	fes_write32(base + SMHC_DMAC, 1);
	{
		uint td = 100000;
		while ((fes_read32(base + SMHC_DMAC) & 1) && --td)
			;
	}
	fes_write32(base + SMHC_DMAC, 0);
	fes_write32(base + SMHC_GCTRL, fes_read32(base + SMHC_GCTRL) | GCTRL_ACCESS_BY_AHB);
	fes_write32(base + SMHC_BLKSZ, 512);
	fes_write32(base + SMHC_BYTCNT, bytecnt);
	fes_write32(base + SMHC_ARG, (uint)start);
	/* Clear RINT before the command so stale bits cannot mislead */
	fes_write32(base + SMHC_RINT, fes_read32(base + SMHC_RINT));
	/* Multi-block needs AUTO_STOP (bit12); otherwise the open-ended transfer
	 * is never terminated by CMD12, the card stays in the data session and
	 * the next command gets RESP_TIMEOUT. */
	fes_write32(base + SMHC_CMD, CMD_START | CMD_RESP_EXPIRE | CMD_CHK_CRC |
		    CMD_DATA_EXPIRE | CMD_WAIT_PRE_OVER |
		    ((nblk > 1) ? CMD_AUTO_STOP : 0) |
		    ((nblk > 1) ? MMC_CMD_READ_MULTI : MMC_CMD_READ_SINGLE));
	{
		/* One word per FIFO_EMPTY check.  A fixed four-word burst after
		 * the flag clears reads past the data the card has delivered so
		 * far: the FIFO returns its last word for the empty entries, and
		 * the SD bus at its bring-up clock fills it slower than the eMMC
		 * bus the burst was written for.  The observed failure was a GPT
		 * header whose first two words were right and whose next words
		 * were the second word repeated. */
		uint nw = bytecnt >> 2;
		uint *d = dstw;
		while (nw) {
			tmo = 2000000;
			while (fes_read32(base + SMHC_STATUS) & STATUS_FIFO_EMPTY) {
				if (--tmo == 0) {
					printf("[FES] rd fail st=%x rint=%x\n",
					       fes_read32(base + SMHC_STATUS),
					       fes_read32(base + SMHC_RINT));
					return -6;
				}
			}
			*d++ = fes_read32(base + SMHC_FIFO);
			nw--;
		}
	}
	tmo = 8000000;
	while (!(fes_read32(base + SMHC_RINT) &
		 ((nblk > 1) ? (RINT_AUTO_CMD_DONE | RINT_ERR)
			     : (RINT_DATA_OVER | RINT_ERR)))) {
		if (--tmo == 0)
			return -7;
	}
	if (fes_read32(base + SMHC_RINT) & RINT_ERR) {
		printf("[FES] rd err rint=%x\n",
		       fes_read32(base + SMHC_RINT));
		return -8;
	}
	#ifdef CONFIG_FES_DEBUG
	printf("[FES] rd done wait=%u rint=%x\n", 8000000 - tmo,
	       fes_read32(base + SMHC_RINT));
#endif
	return 0;
}

int fes_mmc_read_blocks(void *mmc, unsigned long start, void *dst,
			unsigned int nblk)
{
	return fes_mmc_read_blocks_base(fes_mmc_reg_base(mmc), start, dst,
					nblk);
}

#define FES_MMC_CHUNK_SECTORS	256	/* 128 KiB: one FES transaction, and the
					 * size the eMMC IDMAC write path is
					 * proven at; a larger client request is
					 * split here. */
unsigned fes_mmc_chunk_sectors = FES_MMC_CHUNK_SECTORS;

int fes_mmc_read_blocks_base(uint base, unsigned long start, void *dst,
			     unsigned int nblk)
{
	if (nblk > fes_mmc_chunk_sectors) {
		unsigned int off = 0;

		while (off < nblk) {
			unsigned int n = nblk - off;
			int rc;

			if (n > fes_mmc_chunk_sectors)
				n = fes_mmc_chunk_sectors;
			rc = fes_mmc_read_blocks_base(base, start + off,
						      (char *)dst + off * 512,
						      n);
			if (rc)
				return rc;
			off += n;
		}
		return 0;
	}
#ifdef CONFIG_FES_SMHC_DMA
	if (nblk * 512 <= 524288 && nblk > 1 &&
	    fes_mmc_dma_read_raw(base, start, dst, nblk) == 0)
		return 0;
#endif
	return fes_mmc_pio_read_raw(base, start, dst, nblk);
}

int fes_mmc_read_blocks_self(void *mmc, unsigned long start, void *dst,
			     unsigned int nblk)
{
	return fes_mmc_pio_read_raw(fes_mmc_reg_base(mmc), start, dst, nblk);
}

/* ---- SMHC IDMAC read (128 KB chained descriptors; reuse rint) ----
 * Descriptors must be in DRAM: BSS lives in SRAM (0x90000, fes1.lds),
 * which IDMA/mbus cannot reach. */
#define FES_DES_ADDR	0x000D0000UL	/* SRAM: 512 KB/32 KB = 16 x 16 B; IDMA reaches it
					 * (do not move it to high DRAM: 0x4B8xxxxx hung once) */
static u32 *fes_des = (u32 *)FES_DES_ADDR;

/* Memory barrier (instruction form: the MCR alias can be swallowed as a no-op) */
static void fes_mb(void)
{
	asm volatile("dsb" ::: "memory");
	asm volatile("isb");
}

static void fes_dc_flush(uint addr, uint len)
{
	uint a, end = addr + len;
	for (a = addr & ~63u; a < end; a += 64)
		asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r"(a));	/* DCCMVAC */
	fes_mb();
}

static void fes_dc_invalidate(uint addr, uint len)
{
	uint a, end = addr + len;
	for (a = addr & ~63u; a < end; a += 64)
		asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r"(a));	/* DCIMVAC */
	fes_mb();
}

/* IDMAC read: hand the descriptor to IDMA with own=1; rint DATA_OVER marks
	 * completion (as in PIO). */
static int fes_mmc_dma_read_raw(uint base, unsigned long start, void *dst,
			       unsigned int nblk)
{
	uint bytecnt = nblk * 512;
	uint i, tmo, nd, shift;
	u32 vers, g;

	if (!base || bytecnt > 524288)
		return -1;
	vers = fes_read32(base + SMHC_VERS) & 0xffffff;
	shift = 2;	/* A733 v5p3x: address compression for vers >= V5P3
			 * (Linux des_addr_shift=2) */
	/* Build the descriptors */
	nd = (bytecnt + FES_DES_MAX - 1) / FES_DES_MAX;
	for (i = 0; i < nd; i++) {
		u32 *d = &fes_des[i * 4];
		u32 len = (bytecnt > (i + 1) * FES_DES_MAX) ?
			  FES_DES_MAX : bytecnt - i * FES_DES_MAX;
		/* Linux pattern: CH|OWN|DIC on all, FD on the first,
		 * LD and DIC cleared on the last, no EOR;
		 * buf_size = full word1; ptr2 = next descriptor (0 at tail) */
		u32 w0 = (1u << 31) | (1u << 1) | (1u << 4);	/* own|dic|chain */
		if (i == 0)
			w0 |= (1u << 3);			/* fd */
		if (i == nd - 1) {
			w0 |= (1u << 2);			/* ld */
			w0 &= ~(1u << 1);			/* clear DIC */
			d[3] = 0;
		} else {
			d[3] = ((u32)&fes_des[(i + 1) * 4]) >> shift;
		}
		d[2] = ((u32)((char *)dst + i * FES_DES_MAX)) >> shift;
		d[0] = w0;
		d[1] = len;
	}
#ifdef CONFIG_FES_DEBUG
	printf("[FES] dma rd vers=%x shift=%d nd=%u des=%x "
	       "des0=%x buf1=%x\n",
	       vers, shift, nd, (unsigned)fes_des, fes_des[0], fes_des[2]);
#endif
	fes_dc_flush((uint)fes_des, nd * 16);
	fes_mb();	/* weak ordering: descriptor writes must be visible to IDMA */
	fes_dc_flush((uint)dst, bytecnt);	/* read path: no dirty lines, flush anyway */
	/* IDMAC enable sequence (u-boot mmc_trans_data_by_dma) */
	/* Hidden u-boot core_init setup: not needed for PIO, required by DMA;
	 * the thldc threshold is the prime suspect. */
	fes_write32(base + 0x08, (0xffffffu << 8) | 0xff);	/* timeout */
	fes_write32(base + 0x100, (512u << 16) | (1u << 2) | 1u); /* thldc */
	fes_write32(base + 0x54, 3);				/* csdc */
	fes_write32(base + 0x50, 0xdeb);			/* dbgc */
	/* rdtmout: u-boot sets this before every data command
	 * (DTO_MAX = 200 ms at 50 MHz). */
	{
		uint rdto = (50u * 1000u * 200u + 255u) / 256u;	/* 39063 */
		fes_write32(base + SMHC_GCTRL,
			    fes_read32(base + SMHC_GCTRL) | (1u << 11));
		fes_write32(base + 0x08,
			    (fes_read32(base + 0x08) & ~(0xffffffu << 8))
			    | (rdto << 8));
	}
	/* IDMAC needs the FIFO on the DMA bus: DMA_ENB set and
	 * FIFO_AC_MOD (bit31, AHB access) clear (EDK2 SmhcInitController). */
	g = fes_read32(base + SMHC_GCTRL);
	fes_write32(base + SMHC_GCTRL,
		    (g | (1u << 5) | (1u << 2)) & ~(1u << 31));
#ifdef CONFIG_FES_DEBUG
	printf("[FES] dma A gctrl=%x\n", fes_read32(base + SMHC_GCTRL));
#endif
	fes_write32(base + SMHC_DMAC, 1);	/* idma reset */
	tmo = 100000;
	while (fes_read32(base + SMHC_DMAC) & 1) {
		if (--tmo == 0) {
			printf("[FES] dma B reset tmo dmac=%x\n",
			       fes_read32(base + SMHC_DMAC));
			return -1;
		}
	}
#ifdef CONFIG_FES_DEBUG
	printf("[FES] dma C dmac=%x\n", fes_read32(base + SMHC_DMAC));
#endif
	fes_write32(base + SMHC_DMAC, (1u << 1) | (1u << 7));	/* fixburst|on */
	fes_write32(base + SMHC_IDIE, 1u << 1);			/* RX irq */
	fes_write32(base + SMHC_DLBA, ((u32)fes_des) >> shift);
	/* FIFO thresholds (A733 tm5: RX_TL=240, TX_TL=15, mode 3); defaults can
	 * stop DMA from starting. */
	fes_write32(base + 0x40, (0x3u << 28) | (15u << 16) | 240);
	/* NTSR new mode: u-boot sets bit31 in init/update_clk; DMA depends on it */
	fes_write32(base + SMHC_NTSR,
		    fes_read32(base + SMHC_NTSR) | (1u << 31));
	/* Command: clear the AHB bit for DMA (only PIO uses AHB access) */
	fes_write32(base + SMHC_GCTRL,
		    (fes_read32(base + SMHC_GCTRL) & ~(1u << 31))
		    | (1u << 5) | (1u << 2));
	fes_write32(base + SMHC_BLKSZ, 512);
	fes_write32(base + SMHC_BYTCNT, bytecnt);
	fes_write32(base + SMHC_ARG, (uint)start);
	fes_write32(base + SMHC_RINT, fes_read32(base + SMHC_RINT));
	fes_write32(base + SMHC_IDST, 0x337);	/* clear IDMAC status */
	fes_mb();	/* make the IDMAC setup visible before the command */
	fes_write32(base + SMHC_CMD, CMD_START | CMD_RESP_EXPIRE | CMD_CHK_CRC |
		    CMD_DATA_EXPIRE | CMD_WAIT_PRE_OVER |
		    ((nblk > 1) ? CMD_AUTO_STOP : 0) |
		    ((nblk > 1) ? MMC_CMD_READ_MULTI : MMC_CMD_READ_SINGLE));
#ifdef CONFIG_FES_DEBUG
	{
		/* Long sample (~1 ms): distinguish a swallowed command from a failed response */
		uint dt = 100000;
		while (dt--);
		printf("[FES] dma post rint=%x idst=%x dmac=%x "
		       "chda=%x cbda=%x cmd=%x\n",
		       fes_read32(base + SMHC_RINT),
		       fes_read32(base + SMHC_IDST),
		       fes_read32(base + SMHC_DMAC),
		       fes_read32(base + 0x90),
		       fes_read32(base + 0x94),
		       fes_read32(base + SMHC_CMD));
	}
#endif
	tmo = 8000000;
	for (;;) {
		u32 r = fes_read32(base + SMHC_RINT);
		u32 done_bit = (nblk > 1) ? RINT_AUTO_CMD_DONE
					   : RINT_DATA_OVER;
		if ((r & done_bit) && (fes_read32(base + SMHC_IDST) & (1u << 1)))
			break;	/* rint done && RX_INT: the data is in memory */
		if (r & RINT_ERR)
			break;
		if (--tmo == 0) {
			printf("[FES] dma rd tmo rint=%x idst=%x "
			       "des=%x dlba=%x gctrl=%x\n",
			       r, fes_read32(base + SMHC_IDST),
			       (unsigned)fes_des,
			       fes_read32(base + SMHC_DLBA),
			       fes_read32(base + SMHC_GCTRL));
			return -2;
		}
	}
	if (fes_read32(base + SMHC_RINT) & RINT_ERR) {
		printf("[FES] dma rd err rint=%x idst=%x dmac=%x des0=%x\n",
		       fes_read32(base + SMHC_RINT),
		       fes_read32(base + SMHC_IDST),
		       fes_read32(base + SMHC_DMAC), fes_des[0]);
		return -3;
	}
	/* Clear DMA */
	fes_write32(base + SMHC_IDST, fes_read32(base + SMHC_IDST));
	{
		extern void invalidate_dcache_range(unsigned long, unsigned long);
		invalidate_dcache_range((unsigned long)dst,
					(unsigned long)dst + bytecnt);
	}
	fes_write32(base + SMHC_IDIE, 0);
	fes_write32(base + SMHC_DMAC, 0);
	fes_write32(base + SMHC_GCTRL,
		    (fes_read32(base + SMHC_GCTRL) & ~(1u << 5)));
	fes_dc_invalidate((uint)dst, bytecnt);
#ifdef CONFIG_FES_DEBUG
	printf("[FES] dma rd ok vers=%x nd=%u\n", vers, nd);
#endif
	return 0;
}

/* ---- SMHC IDMAC write (mirror of dma_read: same descriptors, IDIE=TX,
 * WRITE command, wait for busy) ---- */
static int fes_mmc_dma_write_start(uint base, unsigned long start,
				   const void *buf, unsigned int nblk)
{
	uint bytecnt = nblk * 512;
	uint i, tmo, nd, shift;
	u32 vers;
	const u32 *src = (const u32 *)buf;

	if (!base || bytecnt > 524288)
		return -1;
	/* One controller and one descriptor array: never build a transfer on
	 * top of one that is still running. */
	if (fes_wr_busy) {
		int busy_rc = fes_mmc_write_wait_base();

		if (busy_rc)
			return busy_rc;
	}
	vers = fes_read32(base + SMHC_VERS) & 0xffffff;
	shift = 2;	/* A733 v5p3x compression */
	/* Descriptors (Linux pattern: own|dic|ch, FD on first, LD and DIC clear on last) */
	nd = (bytecnt + FES_DES_MAX - 1) / FES_DES_MAX;
	for (i = 0; i < nd; i++) {
		u32 *d = &fes_des[i * 4];
		u32 len = (bytecnt > (i + 1) * FES_DES_MAX) ?
			  FES_DES_MAX : bytecnt - i * FES_DES_MAX;
		u32 w0 = (1u << 31) | (1u << 1) | (1u << 4);
		if (i == 0)
			w0 |= (1u << 3);
		if (i == nd - 1) {
			w0 |= (1u << 2);
			w0 &= ~(1u << 1);
			d[3] = 0;
		} else {
			d[3] = ((u32)&fes_des[(i + 1) * 4]) >> shift;
		}
		d[2] = ((u32)((const char *)src + i * FES_DES_MAX)) >> shift;
		d[0] = w0;
		d[1] = len;
	}
	/* Write path: clean the CPU-written source buffer to DRAM (IDMA reads DRAM) */
	fes_dc_flush((uint)src, bytecnt);
	fes_mb();
	/* u-boot core_init registers, rdtmout and FIFO reset (same as read) */
	fes_write32(base + 0x08, (0xffffffu << 8) | 0xff);
	fes_write32(base + 0x100, (512u << 16) | (1u << 2) | 1u);
	fes_write32(base + 0x54, 3);
	fes_write32(base + 0x50, 0xdeb);
	{
		uint rdto = (50u * 1000u * 200u + 255u) / 256u;
		fes_write32(base + SMHC_GCTRL,
			    fes_read32(base + SMHC_GCTRL) | (1u << 11));
		fes_write32(base + 0x08,
			    (fes_read32(base + 0x08) & ~(0xffffffu << 8))
			    | (rdto << 8));
	}
	fes_write32(base + SMHC_GCTRL,
		    fes_read32(base + SMHC_GCTRL) | (1u << 1));
	{
		volatile uint d;
		/* u-boot spins 10000 times here (~2 ms at this CPU clock)
		 * after re-enabling the card clock; the two writes around it
		 * only need the reset to have landed, and the card was idle
		 * between transactions. */
		for (d = 0; d < 200; d++)
			;
	}
	{
		u32 g = fes_read32(base + SMHC_GCTRL);
		fes_write32(base + SMHC_GCTRL, g | (1u << 5) | (1u << 2));
	}
	fes_write32(base + SMHC_DMAC, 1);
	tmo = 100000;
	while (fes_read32(base + SMHC_DMAC) & 1) {
		if (--tmo == 0)
			return -1;
	}
	fes_write32(base + SMHC_DMAC, (1u << 1) | (1u << 7));
	fes_write32(base + SMHC_IDIE, 1u << 0);			/* TX irq */
	fes_write32(base + SMHC_DLBA, ((u32)fes_des) >> shift);
	fes_write32(base + 0x40, (0x3u << 28) | (15u << 16) | 240);
	fes_write32(base + SMHC_NTSR,
		    fes_read32(base + SMHC_NTSR) | (1u << 31));
	fes_write32(base + SMHC_GCTRL,
		    (fes_read32(base + SMHC_GCTRL) & ~(1u << 31))
		    | (1u << 5) | (1u << 2));
	fes_write32(base + SMHC_BLKSZ, 512);
	fes_write32(base + SMHC_BYTCNT, bytecnt);
	fes_write32(base + SMHC_ARG, (uint)start);
	fes_write32(base + SMHC_RINT, fes_read32(base + SMHC_RINT));
	fes_mb();
	fes_write32(base + SMHC_CMD, CMD_START | CMD_RESP_EXPIRE | CMD_CHK_CRC |
		    CMD_DATA_EXPIRE | CMD_WAIT_PRE_OVER | CMD_WRITE |
		    ((nblk > 1) ? CMD_AUTO_STOP : 0) |
		    ((nblk > 1) ? MMC_CMD_WRITE_MULTI : MMC_CMD_WRITE_SINGLE));
	fes_wr_base = base;
	fes_wr_nblk = nblk;
	fes_wr_busy = 1;
	return 0;
}

int fes_mmc_write_wait_base(void)
{
	uint base, nblk, tmo;
	int rc = 0;

	if (!fes_wr_busy)
		return 0;
	base = fes_wr_base;
	nblk = fes_wr_nblk;
	/* Completion: rint done and idst bit0 (TX) */
	tmo = 8000000;
	for (;;) {
		u32 r = fes_read32(base + SMHC_RINT);
		u32 done_bit = (nblk > 1) ? RINT_AUTO_CMD_DONE
					   : RINT_DATA_OVER;
		if ((r & done_bit) && (fes_read32(base + SMHC_IDST) & (1u << 0)))
			break;
		if (r & RINT_ERR) {
			printf("[FES] dma wr err rint=%x idst=%x st=%x\n",
			       r, fes_read32(base + SMHC_IDST),
			       fes_read32(base + SMHC_STATUS));
			rc = -2;
			goto out;
		}
		if (--tmo == 0) {
			printf("[FES] dma wr tmo rint=%x idst=%x\n",
			       r, fes_read32(base + SMHC_IDST));
			rc = -3;
			goto out;
		}
	}
	/* Wait for card busy after the write (eMMC programming time) */
	tmo = 4000000;
	while (fes_read32(base + SMHC_STATUS) & STATUS_CARD_BUSY) {
		if (--tmo == 0) {
			rc = -4;
			goto out;
		}
	}
	/* Clear DMA */
	fes_write32(base + SMHC_IDST, fes_read32(base + SMHC_IDST));
	fes_write32(base + SMHC_IDIE, 0);
	fes_write32(base + SMHC_DMAC, 0);
	fes_write32(base + SMHC_GCTRL,
		    (fes_read32(base + SMHC_GCTRL) & ~(1u << 5)));
out:
	fes_wr_busy = 0;
	return rc;
}

static int fes_mmc_dma_write_raw(uint base, unsigned long start,
				 const void *buf, unsigned int nblk)
{
	int rc;

	rc = fes_mmc_dma_write_start(base, start, buf, nblk);
	if (rc)
		return rc;
	return fes_mmc_write_wait_base();
}

/* ---- SD (SMHC0) local init: the library only drives dev2 (eMMC) ----
 * Sequence: CMD0, CMD8 (0x1AA), ACMD41 (poll 0x40FF8000), CMD2, CMD3,
 * CMD7, ACMD6 (4-bit).
 * Clock: 24 MHz (SDR cards allow 25 MHz; USB is the bottleneck). */
static int fes_sd_raw_cmd(uint base, uint idx, uint arg, int resp_flags,
			  u32 *resp)
{
	uint tmo, cmdv;
	fes_write32(base + SMHC_ARG, arg);
	fes_write32(base + SMHC_RINT, fes_read32(base + SMHC_RINT));
	cmdv = CMD_START | idx;
	if (idx == 0)
		cmdv |= (1u << 15);	/* SEND_INIT_SEQ: CMD0 with 80-clock init (EDK2) */
	if (resp_flags) {
		cmdv |= CMD_RESP_EXPIRE;
		if (resp_flags != 3)
			cmdv |= CMD_CHK_CRC;	/* R3 (OCR/ACMD41) has no CRC (EDK2) */
		if (resp_flags == 2)
			cmdv |= (1u << 7);	/* R2 long response */
	}
	fes_write32(base + SMHC_CMD, cmdv);
	tmo = 3000000;
	while (!(fes_read32(base + SMHC_RINT) &
		 (RINT_CMD_DONE | RINT_ERR))) {
		if (--tmo == 0) {
			printf("[FES] sd cmd%d tmo rint=%x st=%x\n",
			       idx, fes_read32(base + SMHC_RINT),
			       fes_read32(base + SMHC_STATUS));
			return -1;
		}
	}
	if (fes_read32(base + SMHC_RINT) & RINT_ERR) {
		uint tm = 1000000;
		printf("[FES] sd cmd%d err rint=%x\n", idx,
		       fes_read32(base + SMHC_RINT));
		/* EDK2 failure recovery: GCTRL HW_RESET, wait for self-clear, UPCLK,
	 * clear RINT, reset FIFO. */
		fes_write32(base + SMHC_GCTRL, 0x7);
		while (fes_read32(base + SMHC_GCTRL) & 0x7) {
			if (--tm == 0)
				break;
		}
		fes_write32(base + SMHC_CMD,
			    (1u << 31) | (1u << 21) | (1u << 13));
		tm = 1000000;
		while (fes_read32(base + SMHC_CMD) & (1u << 31)) {
			if (--tm == 0)
				break;
		}
		fes_write32(base + SMHC_RINT, 0xffffffff);
		fes_write32(base + SMHC_GCTRL,
			    fes_read32(base + SMHC_GCTRL) | (1u << 1));
		tm = 1000000;
		while (fes_read32(base + SMHC_GCTRL) & (1u << 1)) {
			if (--tm == 0)
				break;
		}
		fes_write32(base + SMHC_GCTRL,
			    fes_read32(base + SMHC_GCTRL) | (1u << 4)
			    | (1u << 31));
		printf("[FES] sd cmd%d errret\n", idx);
		return -2;
	}
	if (resp) {
		if (resp_flags == 2) {
			resp[0] = fes_read32(base + 0x2c);	/* resp3 */
			resp[1] = fes_read32(base + 0x28);
			resp[2] = fes_read32(base + 0x24);
			resp[3] = fes_read32(base + 0x20);
		} else {
			resp[0] = fes_read32(base + 0x20);
		}
	}
	return 0;
}

#define SD_PAUSE() do { volatile uint _d; for (_d = 0; _d < 200000; _d++); } while (0)

/* ---- AXP (PMIC) DC1SW2: SD card VDD (board DTS: vmmc = reg_dc1sw2,
 * PWR_OUT_CTRL2 (0x12) bit7; AXP I2C = 0x34, TWI0 = 0x02510000) ---- */
#define TWI0_BASE	0x07083000UL	/* s_twi0 (R domain); AXP8191 lives here */
#define TWI_CTRL	0x0c
#define TWI_STS		0x10
#define TWI_DATA	0x08
#define TWI_ADDR	0x04
#define AXP_I2C_ADDR	0x36
#define AXP_PWR_CTRL2	0x11	/* AXP8191 DCDC_CTL2 (dc1sw2 = bit4) */
#define TWI_ACK	(1u << 2)
#define TWI_IF	(1u << 3)
#define TWI_STP	(1u << 4)
#define TWI_STA	(1u << 5)
#define TWI_EN	(1u << 6)
#define A_() do { uint _d; for (_d = 0; _d < 2000; _d++); } while (0)

static int twi_wait_sts2(uint base, uint want)
{
	uint tmo = 1000000;
	while ((fes_read32(base + TWI_STS) & 0xff) != want) {
		if (--tmo == 0)
			return -1;
	}
	return 0;
}

static void twi_clear_int(uint base)
{
	fes_write32(base + TWI_CTRL, fes_read32(base + TWI_CTRL) | (1u << 5));
}

/* AXP register read-modify-write (I2C: start, addrW, reg, restart, addrR,
 * data, stop). */
static int axp_write_reg(uint reg, uint d)
{
	volatile uint *v = (volatile uint *)TWI0_BASE;
	uint i, tmo;
	v[3] |= TWI_EN;
	v[7] = 0; v[6] = 1; A_();
	v[3] |= TWI_STA; v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -1;
	if ((v[4] & 0xff) != 0x08) return -2;
	v[2] = (AXP_I2C_ADDR << 1) | 0;
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -3;
	if ((v[4] & 0xff) != 0x18) return -4;
	v[2] = reg;	/* register address goes in data (twi_send_byte_addr) */
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -5;
	if ((v[4] & 0xff) != 0x28) return -6;
	v[2] = d;
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -7;
	if ((v[4] & 0xff) != 0x28) return -8;
	v[3] |= TWI_STP; v[3] |= TWI_IF;
	for (i = 0; i < 1000; i++)
		;
	return 0;
}

static int axp_unlock(void)
{
	static const u8 codes[] = { 0x0F, 0x5A, 0x7F, 0x33, 0xA5 };
	uint i;
	for (i = 0; i < sizeof(codes); i++) {
		axp_write_reg(0xF0, codes[i]);	/* WRITE_LOCK_F1 code */
	}
	axp_write_reg(0xFF, 0);			/* REG_ADD_EXT main group */
	return 0;
}

static int axp_update_bits(uint reg, uint set_mask)
{
	volatile uint *v = (volatile uint *)TWI0_BASE;
	uint val, i, tmo;

	/* R-TWI0 gate (boot0 usually leaves it on; the PRCM write is idempotent) */
	fes_write32(0x07002E80UL,
		    fes_read32(0x07002E80UL) | 1u | (1u << 16));
	fes_write32(0x02002E80UL,
		    fes_read32(0x02002E80UL) | 1u | (1u << 16));
	for (i = 0; i < 2000; i++)
		;
	fes_dbg("[FES] twi ctl=%x sts=%x\n", v[3], v[4]);
	axp_unlock();				/* try to release write protection */
	/* Check whether writes work, read-only: do not pollute with 0xFF */
	{
		uint wv = 0x00;
		/* read back 0x10 */
		v[3] |= TWI_STA; v[3] |= TWI_IF;
		tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) break;
		v[2] = (AXP_I2C_ADDR << 1) | 0;
		v[3] |= TWI_IF;
		tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) break;
		v[2] = 0x10;
		v[3] |= TWI_IF;
		tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) break;
		v[3] |= TWI_STA; v[3] |= TWI_IF;
		tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) break;
		v[2] = (AXP_I2C_ADDR << 1) | 1;
		v[3] |= TWI_IF;
		tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) break;
		v[3] &= ~TWI_ACK; v[3] |= TWI_IF;
		tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) break;
		wv = v[2];
		v[3] |= TWI_STP; v[3] |= TWI_IF;
		fes_dbg("[FES] axp write-chk 0x10=0x%x\n", wv);
	}
	/* Read sequence: soft reset first, then enable (reset clears ctl) */
	v[7] = 0; v[6] = 1; A_();		/* soft reset */
	v[3] |= TWI_EN;				/* enable after reset */
	v[3] |= TWI_STA; v[3] |= TWI_IF;	/* START, clear IF */
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -1;
	if ((v[4] & 0xff) != 0x08) return -2;
	v[2] = (AXP_I2C_ADDR << 1) | 0;		/* addr W */
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -3;
	if ((v[4] & 0xff) != 0x18) return -4;
	v[2] = reg;	/* register address goes in data (twi_send_byte_addr) */
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -5;
	if ((v[4] & 0xff) != 0x28) return -6;
	v[3] |= TWI_STA; v[3] |= TWI_IF;	/* repeated START */
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -7;
	if ((v[4] & 0xff) != 0x10) return -8;
	v[2] = (AXP_I2C_ADDR << 1) | 1;		/* addr R */
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -9;
	if ((v[4] & 0xff) != 0x40) return -10;
	v[3] &= ~TWI_ACK; v[3] |= TWI_IF;	/* NACK on the last byte */
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -11;
	val = v[2];				/* current CTRL2 value */
	v[3] |= TWI_STP; v[3] |= TWI_IF;	/* STOP */
	val |= set_mask;
	/* Write-back sequence */
	v[3] |= TWI_STA; v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -12;
	if ((v[4] & 0xff) != 0x08) return -13;
	v[2] = (AXP_I2C_ADDR << 1) | 0;
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -14;
	if ((v[4] & 0xff) != 0x18) return -15;
	v[2] = reg;	/* register address goes in data (twi_send_byte_addr) */
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -16;
	if ((v[4] & 0xff) != 0x28) return -17;
	v[2] = val;				/* new DCDC_CTL2 (bit4 = DC1SW2) */
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -18;
	if ((v[4] & 0xff) != 0x28) return -19;
	v[3] |= TWI_STP; v[3] |= TWI_IF;	/* STOP */
	for (i = 0; i < 1000; i++)
		;
	/* Read back to detect write protection */
	v[3] |= TWI_STA; v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -20;
	if ((v[4] & 0xff) != 0x08) return -21;
	v[2] = (AXP_I2C_ADDR << 1) | 0;
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -22;
	if ((v[4] & 0xff) != 0x18) return -23;
	v[2] = reg;	/* register address goes in data (twi_send_byte_addr) */
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -24;
	if ((v[4] & 0xff) != 0x28) return -25;
	v[3] |= TWI_STA; v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -26;
	if ((v[4] & 0xff) != 0x10) return -27;
	v[2] = (AXP_I2C_ADDR << 1) | 1;
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -28;
	if ((v[4] & 0xff) != 0x40) return -29;
	v[3] &= ~TWI_ACK; v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -30;
	val = v[2];				/* read back after write */
	v[3] |= TWI_STP; v[3] |= TWI_IF;
	fes_dbg("[FES] axp ctrl2=0x%x (dc1sw2=%s)\n", val,
	       (val & 0x10) ? "ON" : "OFF-LOCKED?");
	if (!(val & 0x10))
		return -31;
	return 0;
}

static int axp_read_reg(uint reg);	/* fwd */

static int axp_update_bits_clear(uint reg, uint clr_mask)
{
	/* Read-modify-clear; never zero the whole register: DCDC_CTL = 0 cuts
	 * CPU power. */
	int v = axp_read_reg(reg);
	if (v < 0)
		return v;
	return axp_write_reg(reg, (uint)v & ~clr_mask);
}

static int axp_read_reg(uint reg)
{
	volatile uint *v = (volatile uint *)TWI0_BASE;
	uint val, tmo;
	v[3] |= TWI_EN;
	v[7] = 0; v[6] = 1; A_();
	v[3] |= TWI_STA; v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -1;
	if ((v[4] & 0xff) != 0x08) return -2;
	v[2] = (AXP_I2C_ADDR << 1) | 0;
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -3;
	if ((v[4] & 0xff) != 0x18) return -4;
	v[2] = reg;
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -5;
	if ((v[4] & 0xff) != 0x28) return -6;
	v[3] |= TWI_STA; v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -7;
	if ((v[4] & 0xff) != 0x10) return -8;
	v[2] = (AXP_I2C_ADDR << 1) | 1;
	v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -9;
	if ((v[4] & 0xff) != 0x40) return -10;
	v[3] &= ~TWI_ACK; v[3] |= TWI_IF;
	tmo = 1000000; while (!(v[3] & TWI_IF)) if (!--tmo) return -11;
	val = v[2];
	v[3] |= TWI_STP; v[3] |= TWI_IF;
	return val;
}

/* Enable SD card VDD (DC1SW2) */
static int fes_sd_pwr_on(void)
{
	int rc;
	volatile uint d;
	/* DCDC1 input chain (bit0 = DCDC1 on, idempotent) */
	rc = axp_update_bits(0x10, 0x01);
	/* Power cycle: turn DC1SW2 off first (clear bit4) */
	rc = axp_update_bits_clear(AXP_PWR_CTRL2, 0x10);
	for (d = 0; d < 100000; d++)
		;	/* discharge ~60 ms (1.2 s broke the host CSW timeout) */
	/* then turn DC1SW2 on (bit4) */
	rc = axp_update_bits(AXP_PWR_CTRL2, 0x10);
	/* read back and verify */
	rc = axp_read_reg(AXP_PWR_CTRL2);
	fes_dbg("[FES] axp dcdc1/dc1sw2 cycle done ctl2=%x\n",
	       rc < 0 ? 0xee : rc);
	return rc < 0 ? rc : 0;
}
#define SD_LONGWAIT() do { volatile uint _d; for (_d = 0; _d < 4000000; _d++); } while (0)

/* DRAM scratch for the two verification reads, past the FES1 image. */
#define FES_SD_VERIFY_A   0x4B000000UL
#define FES_SD_VERIFY_B   0x4B001000UL
/* R1 bit 7: the card rejected the switch function. */
#define FES_SD_R1_SWITCH_ERROR  (1u << 7)

int fes_sd_read(uint base, unsigned long start, void *dst, unsigned int nblk);

/*
 * Set SMHC0 to one of the two verified rates.  PERI0_400M / 2 is a 200 MHz
 * module clock and SMHC_CLKDIV divides it by 2 * (field + 1): field 1 is
 * 50 MHz, field 3 is 25 MHz, the compatibility maximum.
 */
static void fes_sd_set_field(uint base, uint field)
{
	uint tm;

	fes_write32(base + SMHC_CLKCR,
		    fes_read32(base + SMHC_CLKCR) & ~(1u << 16));
	tm = 1000000;
	fes_write32(base + SMHC_CMD, (1u << 31) | (1u << 21) | (1u << 13));
	while ((fes_read32(base + SMHC_CMD) & (1u << 31)) && --tm)
		;

	/* PERI0_400M (selector 1), gated, M=2 (FACTOR_M = 1), N=1. */
	fes_write32(FES_CCU_SMHC0_CLK, 0x80000000u | (1u << 24) | 1u);
	fes_write32(base + SMHC_CLKCR,
		    (fes_read32(base + SMHC_CLKCR) & ~0xffu) | field | (1u << 16));
	{
		volatile uint d;
		for (d = 0; d < 100000; d++)
			;
	}
	tm = 1000000;
	fes_write32(base + SMHC_CMD, (1u << 31) | (1u << 21) | (1u << 13));
	while ((fes_read32(base + SMHC_CMD) & (1u << 31)) && --tm)
		;
	{
		volatile uint d;
		for (d = 0; d < 100000; d++)
			;
	}
}

/* SD init; 0 on success, after which fes_sd_read/write use the raw base */
int fes_sd_init(uint base)
{
	u32 r[4], rca;
	uint tries, tmo;
	uint hs_field = 3;	/* SMHC_CLKDIV field: 1 = 50 MHz, 3 = 25 MHz */
	int c8ok = 0;	/* CMD8 echo passed = v2 card; ACMD41 also uses it */

	{
		unsigned sctlr;
		asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
		fes_dbg("[FES] sd init sctlr=%x D=%u\n", sctlr,
		       (sctlr >> 2) & 1);
	}
	fes_dbg("[FES] sd init start\n");
	/* SD card VDD (AXP DC1SW2; the board DTS sets vmmc=reg_dc1sw2).
	 * Full power cycle: off, discharge, on (the card needs a clean
	 * power-up sequence). */
	printf("[FES] axp on rc=%d\n", fes_sd_pwr_on());
	/* Let VDD ramp and settle: SD_PAUSE x4 (~0.5 s), bounded by the
	 * host CSW timeout (writes arrive before init; the SD spec only
	 * needs 1 ms plus 74 clocks). */
	SD_PAUSE();
	SD_PAUSE();
	SD_PAUSE();
	SD_PAUSE();
	/* SMHC0 controller version (SMHC2 reads 0x050530; semantics may differ) */
	fes_dbg("[FES] sd vers=%x\n",
	       fes_read32(base + SMHC_VERS) & 0xffffff);
	/* SMHC0 mclk M factor (bits 0-7): M=0 divides by zero, no CLK.
	 * 24 MHz source with M=1 gives 24 MHz (SMHC2: M=8 from 800 MHz). */
	fes_write32(FES_CCU_SMHC0_CLK, 0x80000000UL | 1);
	fes_dbg("[FES] sd mclk->%x\n", fes_read32(FES_CCU_SMHC0_CLK));
	/* mbus/AHB store gate (DTS sdc0 clocks: MBUS_STORE_GATE and
	 * MSI_LITE1); boot0 only enables the eMMC (SMHC2) ones. */
	fes_write32(0x020025C0UL, fes_read32(0x020025C0UL) | (1u << 24));
	fes_write32(0x020025E0UL, fes_read32(0x020025E0UL) | (1u << 30)
		    | (1u << 31));
	fes_dbg("[FES] sd ahbm=%x mbus=%x\n",
	       fes_read32(0x020025C0UL), fes_read32(0x020025E0UL));
	/* Response timeout (EDK2 A733 SD path uses the full 0xFFFFFFFF) */
	fes_write32(base + 0x08, 0xffffffffu);
	fes_write32(base + SMHC_GCTRL,
		    fes_read32(base + SMHC_GCTRL) | (1u << 11));
	/* kernel sunxi_mmc_init_host sequence: hardware reset, enable
	 * interrupts (GCTRL bit4), clear ACCESS_DONE_DIRECT (bit30), FTRGL,
	 * DBGC and RINTR. */
	fes_write32(base + SMHC_GCTRL, 0x7);	/* soft|fifo|dma reset */
	{
		uint d;
		for (d = 0; d < 50000; d++)
			;
	}
	fes_write32(base + SMHC_GCTRL,
		    (fes_read32(base + SMHC_GCTRL) & ~0x7));	/* release reset */
	fes_write32(base + SMHC_RINT, 0xffffffff);		/* clear all interrupts */
	fes_write32(base + 0x40, 0x300F00F0);	/* EDK2 FIFOTH: burst 16 dw, RX 15, TX 240 */
	fes_write32(base + 0x50, 0xdeb);			/* DBGC */
	/* u-boot core_init settings required before commands: thldc and csdc */
	fes_write32(base + 0x100, (512u << 16) | (1u << 2) | 1u);
	fes_write32(base + 0x54, 3);
	fes_dbg("[FES] sd thldc=%x csdc=%x\n",
	       fes_read32(base + 0x100), fes_read32(base + 0x54));
	fes_write32(base + SMHC_GCTRL,
		    (fes_read32(base + SMHC_GCTRL) | (1u << 4))	/* INTERRUPT_ENABLE */
		    & ~(1u << 30));				/* clear ACCESS_DONE_DIRECT */
	fes_write32(base + SMHC_GCTRL,
		    fes_read32(base + SMHC_GCTRL) | (1u << 31));	/* AHB access (EDK2 requires it) */
	fes_dbg("[FES] sd gctrl=%x\n", fes_read32(base + SMHC_GCTRL));
	/* kernel v5p3x: all division is in mclk (M factor), CLKCR div=0.
	 * At 400 kHz: mod = 2 x 400 kHz = 800 kHz = 24 MHz/30, so M=30.
	 * DRV_DL (0x140) bit7 is clear at 400 kHz and set at high speed. */
	fes_dbg("[FES] sd drvdl=%x\n", fes_read32(base + 0x140));
	fes_write32(base + 0x140, fes_read32(base + 0x140) & ~(1u << 7));
	/* EDK2 SetCardClock sequence (CubieA7ASdmmcSlotsDxe):
	 * clock off, UPCLK, mod (24M/4/15 = 400K), clear div, clock on,
	 * stall, UPCLK, stall. */
	fes_write32(base + SMHC_CLKCR,
		    fes_read32(base + SMHC_CLKCR) & ~(1u << 16));
	{
		uint tm = 1000000;
		fes_write32(base + SMHC_CMD,
			    (1u << 31) | (1u << 21) | (1u << 13));
		while (fes_read32(base + SMHC_CMD) & (1u << 31)) {
			if (--tm == 0)
				break;
		}
	}
	fes_write32(FES_CCU_SMHC0_CLK, 0x8000030eu);
	fes_write32(base + SMHC_CLKCR,
		    fes_read32(base + SMHC_CLKCR) & ~0xffu);
	fes_write32(base + SMHC_CLKCR,
		    fes_read32(base + SMHC_CLKCR) | (1u << 16));
	{
		volatile uint d;
		for (d = 0; d < 100000; d++)
			;
	}
	{
		uint tm = 1000000;
		fes_write32(base + SMHC_CMD,
			    (1u << 31) | (1u << 21) | (1u << 13));
		while (fes_read32(base + SMHC_CMD) & (1u << 31)) {
			if (--tm == 0)
				break;
		}
	}
	{
		volatile uint d;
		for (d = 0; d < 100000; d++)
			;
	}
	fes_dbg("[FES] sd clkcr=%x mclk=%x\n",
	       fes_read32(base + SMHC_CLKCR),
	       fes_read32(FES_CCU_SMHC0_CLK));
	fes_dbg("[FES] c0 g=%x ck=%x ns=%x dcdc=%x/%x\n",
	       fes_read32(FES_SMHC0_BASE), fes_read32((FES_SMHC0_BASE + SMHC_CLKCR)),
	       fes_read32((FES_SMHC0_BASE + SMHC_NTSR)),
	       axp_read_reg(0x10) & 0xff, axp_read_reg(0x11) & 0xff);
	/* EDK2's SD path does not set NTSR new mode (only SMHC2/eMMC);
	 * clear bit31 for legacy 3.3 V SD. */
	fes_write32(base + SMHC_NTSR,
		    fes_read32(base + SMHC_NTSR) & ~(1u << 31));
	fes_dbg("[FES] sd ns2=%x\n", fes_read32(base + SMHC_NTSR));
	fes_dbg("[FES] sd r0init=%x\n", r[0]);
	if (fes_sd_raw_cmd(base, 0, 0, 0, NULL)) {	/* CMD0 idle, no response */
		printf("[FES] sd cmd0 fail\n");
		return -1;
	}
	fes_dbg("[FES] sd post0 rint=%x\n",
	       fes_read32(base + SMHC_RINT));
	fes_dbg("[FES] sd mclk=%x\n", fes_read32(FES_CCU_SMHC0_CLK));
	SD_PAUSE();
	SD_PAUSE();	/* card ready after CMD0 (80 clocks sent; ~0.25 s) */
	{
		/* Retry CMD8 three times (cards power up slowly; 50 ms
		 * between tries).  Per EDK2, no CMD8 response means a v1 card
		 * (or no 0x1AA echo): do not fail, skip the echo check and
		 * continue to ACMD41 (r[0] is stale here). */
		uint cr;
		int c8 = -1;
		for (cr = 0; cr < 3; cr++) {
			c8 = fes_sd_raw_cmd(base, 8, 0x1AA, 1, r);
			if (c8 == 0)
				break;
			SD_LONGWAIT();
			SD_LONGWAIT();
			SD_LONGWAIT();
			SD_LONGWAIT();
			SD_LONGWAIT();	/* ~50ms */
		}
		if (c8 == 0 && (r[0] & 0xFFF) == 0x1AA) {
			c8ok = 1;	/* v2: echo check passed */
		}
		if (c8) {
			fes_dbg("[FES] sd cmd8 noresp rc=%d (cont)\n", c8);
		}
		if (c8 == 0 && !c8ok) {
			fes_dbg("[FES] sd cmd8 echo bad r7=%x (cont)\n", r[0]);
		}
		fes_dbg("[FES] sd c8ok=%d (cont acmd41)\n", c8ok);
		/* EDK2: continue to ACMD41 either way; v1 cards do not answer CMD8 */
	}
	tries = 1000;
	do {					/* ACMD41 polling */
		if (fes_sd_raw_cmd(base, 55, 0, 1, r))	/* CMD55 = R1 (EDK2) */
			printf("[FES] sd cmd55 fail\n");
		SD_PAUSE();
		if (fes_sd_raw_cmd(base, 41, c8ok ? 0x40FF8000u : 0x00FF8000u, 3, r)) {
			printf("[FES] sd acmd41 fail ocr=%x\n", r[0]);
			printf("[FES] sd ret-4\n");
			return -4;
		}
		SD_PAUSE();
		if (r[0] & (1u << 31))
			break;
	} while (--tries);
	if (!tries) {
		printf("[FES] sd acmd41 tmo ocr=%x\n", r[0]);
		printf("[FES] sd ret-5\n");
		return -5;
	}
	printf("[FES] sd ocr=%x%s\n", r[0],
	       (r[0] & (1u << 30)) ? " HC" : "");
	if (fes_sd_raw_cmd(base, 2, 0, 2, r))		/* CMD2 CID R2 */
		return -6;
	SD_PAUSE();
	if (fes_sd_raw_cmd(base, 3, 0, 1, r))		/* CMD3 RCA */
		return -7;
	rca = (r[0] >> 16) & 0xFFFF;
	printf("[FES] sd rca=%x\n", rca);
	SD_PAUSE();
	if (fes_sd_raw_cmd(base, 7, (u32)rca << 16, 1, r))	/* CMD7 sel */
		return -8;
	SD_PAUSE();
	tmo = 3000000;
	while (fes_read32(base + SMHC_STATUS) & STATUS_CARD_BUSY) {
		if (--tmo == 0)
			return -9;
	}
	/* ACMD6 (4-bit): CMD55's argument must be the RCA (EDK2 passes
	 * MMC_CMD_APP_CMD with Dev->Rca); in tran state after selection a
	 * CMD55 with arg=0 gets no response (the earlier rint=0x100). */
	if (fes_sd_raw_cmd(base, 55, (u32)rca << 16, 1, r)) {
		printf("[FES] sd acmd6 c55 fail\n");
		return -10;
	}
	SD_PAUSE();
#ifdef FES_SD_1BIT
	fes_write32(base + SMHC_WIDTH, 0);		/* experiment: 1-bit */
	printf("[FES] sd 1bit mode\n");
#else
	if (fes_sd_raw_cmd(base, 6, 0x2, 1, r))
		return -10;
	fes_write32(base + SMHC_WIDTH, 1);		/* controller 4-bit */
#endif
	#ifndef FES_SD_KEEP_SLOW
	/*
	 * Put the card in high-speed timing (CMD6, function group 1 = 1) and
	 * then raise SMHC0.  PERI0_400M divided by 2 is a 200 MHz module clock
	 * and this part's SMHC_CLKDIV divides that by 2 * (field + 1), so field
	 * 1 gives 50 MHz on the card and field 3 gives 25 MHz, the compatibility
	 * timing maximum, for a card that rejects high speed.  The old
	 * HOSC/M=0 setting was divided by four inside the controller and left
	 * the card at 6 MHz.
	 */
	/*
	 * Pick the timing per card, the way the eMMC path picks HS200:
	 * read a sector at the compatibility rate, ask the card for high
	 * speed, read the same sector again at the higher rate, and keep it
	 * only if the contents match.  A card that refuses the function
	 * (R1 SWITCH_ERROR) or cannot hold 50 MHz stays at 25 MHz.
	 */
	{
		unsigned char *ref = (unsigned char *)FES_SD_VERIFY_A;
		unsigned char *chk = (unsigned char *)FES_SD_VERIFY_B;
		int hsoK;

		fes_sd_set_field(base, 3);	/* 25 MHz, compatibility */
		hsoK = (fes_sd_read(base, 0, ref, 1) == 0);
		if (hsoK && fes_sd_raw_cmd(base, 6, 0x80FFFFF1u, 1, r) == 0 &&
		    (r[0] & FES_SD_R1_SWITCH_ERROR) == 0) {
			fes_sd_set_field(base, 1);	/* 50 MHz */
			if (fes_sd_read(base, 0, chk, 1) == 0 &&
			    memcmp(ref, chk, 512) == 0) {
				hs_field = 1;
			}
		}
		if (hs_field != 1) {
			/* Either the card refused, or 50 MHz did not read
			 * back the same data: stay at 25 MHz. */
			fes_sd_set_field(base, 3);
			if (hsoK && fes_sd_read(base, 0, chk, 1) != 0)
				printf("[FES] sd 25 MHz read failed\n");
			hs_field = 3;
		}
	}
	#endif
	fes_mmc_chunk_sectors = 256;	/* SD handles 128 KiB MMC writes */
	printf("[FES] sd init OK, %u MHz, clkcr=%x mclk=%x\n",
	       (hs_field == 1u) ? 50u : 25u,
	       fes_read32(base + SMHC_CLKCR),
	       fes_read32(FES_CCU_SMHC0_CLK));
	return 0;
}

/* SD raw-base read/write (used after init) */
int fes_sd_write(uint base, unsigned long start, const void *buf,
		 unsigned int nblk);
int fes_sd_read(uint base, unsigned long start, void *dst, unsigned int nblk);

/* CMD6 (SWITCH, 8-bit SDR) and the WIDTH register, following u-boot
 * set_ios: card side CMD6 arg=0x03B70200 (mode=3 write_byte, index=183
 * bus_width, value=2 = 8-bit SDR); host side WIDTH (0x0c) = 0x2.  The
 * library's mmc_set_bus_width only updates the struct field, so neither
 * hardware bit was ever set. */
int fes_mmc_switch_8bit(void *mmc)
{
	uint base = fes_mmc_reg_base(mmc);
	uint tmo;

	if (!base)
		return -1;
	fes_dbg("[FES] sw8 pre clkcr=%x width=%x\n",
	       fes_read32(base + SMHC_CLKCR), fes_read32(base + SMHC_WIDTH));
	fes_write32(base + SMHC_ARG, 0x03B70200);
	fes_write32(base + SMHC_RINT, fes_read32(base + SMHC_RINT));
	fes_write32(base + SMHC_CMD, CMD_START | CMD_RESP_EXPIRE | CMD_CHK_CRC | 6);
	tmo = 3000000;
	while (!(fes_read32(base + SMHC_RINT) & (RINT_CMD_DONE | RINT_ERR))) {
		if (--tmo == 0)
			return -1;
	}
	if (fes_read32(base + SMHC_RINT) & RINT_ERR) {
		printf("[FES] sw8 cmd6 err rint=%x\n",
		       fes_read32(base + SMHC_RINT));
		return -2;
	}
	/* R1b busy: wait for the card to finish programming */
	tmo = 3000000;
	while (fes_read32(base + SMHC_STATUS) & STATUS_CARD_BUSY) {
		if (--tmo == 0)
			return -3;
	}
	fes_write32(base + SMHC_WIDTH, 0x2);	/* controller 8-bit */
	/* A733 SMHC2 mod_clk（CCM 0x02002d20）：src_sel@24 N@8 M[7:0]
	 * Source: 0=24M, 1=PERI0_800M, 2=PERI0_600M ...  Target 50 MHz
	 * (SDR 8-bit) -> mod=100 MHz = 800M/8 -> src=1, M=8, N=0.  Only
	 * raise it when the clock is still the 24 MHz source.
	 * Commit sequence (u-boot mmc_config_clock): CLK off, write CCM,
	 * CLK on, update_clk (CLKCR bit31 request plus UPCLK_ONLY; writing
	 * the CCM register alone has no effect). */
	{
		uint cc = fes_read32(FES_CCU_SMHC2_CLK);
		fes_dbg("[FES] sw8 ccm=%x\n", cc);
		if (((cc >> 24) & 7) == 0) {	/* 24M source = low speed */
			uint clk = fes_read32(base + SMHC_CLKCR);
			uint tmo;
			fes_write32(base + SMHC_CLKCR, clk & ~(1u << 16));	/* clock off */
			/* update: bit31 request plus UPCLK_ONLY */
			fes_write32(base + SMHC_CLKCR,
				    fes_read32(base + SMHC_CLKCR) | (1u << 31));
			fes_write32(base + SMHC_CMD,
				    CMD_START | CMD_UPCLK_ONLY | CMD_WAIT_PRE_OVER);
			tmo = 1000000;
			while (fes_read32(base + SMHC_CMD) & CMD_START) {
				if (--tmo == 0)
					break;
			}
			fes_write32(base + SMHC_CLKCR,
				    fes_read32(base + SMHC_CLKCR) & ~(1u << 31));
			fes_write32(base + SMHC_RINT,
				    fes_read32(base + SMHC_RINT));
			/* change the CCM source/divider */
			cc &= ~(7u << 24);
			cc |= (1u << 24);		/* PERI0_800M */
			cc &= ~(0xffu);
			cc &= ~(0xfu << 8);
			cc |= 8;			/* 800/8=100MHz mod → 50MHz clk */
			fes_write32(FES_CCU_SMHC2_CLK, cc);
			/* commit the new clock */
			fes_write32(base + SMHC_CLKCR,
				    fes_read32(base + SMHC_CLKCR) | (1u << 16)); /* clock on */
			fes_write32(base + SMHC_CLKCR,
				    fes_read32(base + SMHC_CLKCR) | (1u << 31));
			fes_write32(base + SMHC_CMD,
				    CMD_START | CMD_UPCLK_ONLY | CMD_WAIT_PRE_OVER);
			tmo = 1000000;
			while (fes_read32(base + SMHC_CMD) & CMD_START) {
				if (--tmo == 0)
					break;
			}
			fes_write32(base + SMHC_CLKCR,
				    fes_read32(base + SMHC_CLKCR) & ~(1u << 31));
			fes_write32(base + SMHC_RINT,
				    fes_read32(base + SMHC_RINT));
			fes_dbg("[FES] sw8 ccm up=%x clkcr=%x\n",
			       fes_read32(FES_CCU_SMHC2_CLK),
			       fes_read32(base + SMHC_CLKCR));
		}
	}
	fes_dbg("[FES] sw8 done width=%x clkcr=%x\n",
	       fes_read32(base + SMHC_WIDTH), fes_read32(base + SMHC_CLKCR));
	/* Raising the CPU PLL was measured before: 672 MHz left the
	 * per-block time unchanged (USB/protocol bound) and the rebuild
	 * cost about 784 ms, so the clock stays at boot0's setting. */
	return 0;
}

/* mmc_data layout (deduced from mmc_send_cmd's accesses):
 *   +0 buffer, +4 flags (bit31 = write), +8 blocks, +c blocksize
 *   (differs from the u-boot layout; follow the library disassembly). */

/* mmc_cmd layout (library disassembly: cmdidx@0, arg@4, resp_type/bits@8, response@12) */
/*
 * Write nblk blocks at sector start.  Entry: an initialized mmc handle
 * from find_mmc_device.  First try the library data path; if it does not
 * move data through the CPU, use the PIO path below.
 */
/* ---- PIO write (A733 SMHC, u-boot-2018 mmc.h bits; the library data
 * path does not move data) ---- */
#define SMHC_GCTRL	0x00
#define SMHC_CLKCR	0x04
#define SMHC_WIDTH	0x0c
/* IDMAC (v4p1 descriptor) */
#define SMHC_DMAC	0x80
#define SMHC_DLBA	0x84
#define SMHC_IDST	0x88
#define SMHC_IDIE	0x8c
#define SMHC_VERS	0x300
#define SMHC_NTSR	0x5c
#define FES_DES_ALIGN	16
#define FES_DES_MAX	(1 << 12)	/* 4 KB/segment: 32 KB hung (buf_size field); 128 segments at 0xD0000 are safe */
#define FES_DES_N	(524288 / FES_DES_MAX)	/* 128 segments x 16 B = 2 KB at 0xD0000 (bss starts at 0xD1A40) */
#define SMHC_BLKSZ	0x10
#define SMHC_BYTCNT	0x14
#define SMHC_CMD	0x18
#define SMHC_ARG	0x1c
#define SMHC_RINT	0x38
#define SMHC_STATUS	0x3c
#define SMHC_FIFO	0x200

#define GCTRL_ACCESS_BY_AHB	(1U << 31)
#define CMD_START		(1U << 31)
#define CMD_RESP_EXPIRE		(1U << 6)
#define CMD_CHK_CRC		(1U << 8)
#define CMD_DATA_EXPIRE		(1U << 9)
#define CMD_WRITE		(1U << 10)
#define CMD_WAIT_PRE_OVER	(1U << 13)
#define CMD_AUTO_STOP		(1U << 12)
#define CMD_UPCLK_ONLY		(1U << 21)
#define RINT_CMD_DONE		(1U << 2)
#define RINT_DATA_OVER		(1U << 3)
#define RINT_AUTO_CMD_DONE	(1U << 14)
#define RINT_ERR		(0x0002U | 0x0040U | 0x0080U | 0x0100U | \
			0x0200U | 0x0400U | 0x0800U | 0x1000U | \
			0x2000U | 0x8000U)	/* = u-boot 0xBFC2 */
#define STATUS_FIFO_FULL	(1U << 3)
#define STATUS_TXWL		(1U << 1)

static int fes_mmc_pio_write_raw(uint base, unsigned long start,
			      const void *buf, unsigned int nblk)
{
	uint bytecnt = nblk * 512;
	const uint *src = (const uint *)buf;
	uint i, tmo;

	if (!base)
		return -1;

	/* AHB access and configuration */
	fes_write32(base + SMHC_GCTRL, fes_read32(base + SMHC_GCTRL) | GCTRL_ACCESS_BY_AHB);
	fes_write32(base + SMHC_BLKSZ, 512);
	fes_write32(base + SMHC_BYTCNT, bytecnt);
	fes_write32(base + SMHC_ARG, (uint)start);
	fes_write32(base + SMHC_RINT, fes_read32(base + SMHC_RINT));
	/* Multi-block needs AUTO_STOP: the auto CMD12 ends the open CMD25 write */
	fes_write32(base + SMHC_CMD, CMD_START | CMD_RESP_EXPIRE | CMD_CHK_CRC |
		    CMD_DATA_EXPIRE | CMD_WAIT_PRE_OVER | CMD_WRITE |
		    ((nblk > 1) ? CMD_AUTO_STOP : 0) |
		    ((nblk > 1) ? MMC_CMD_WRITE_MULTI : MMC_CMD_WRITE_SINGLE));

	/* CPU move: wait until the FIFO is writable (not FULL), push 32 bits */
	for (i = 0; i < (bytecnt >> 2); i++) {
		tmo = 1000000;
		while (fes_read32(base + SMHC_STATUS) & STATUS_FIFO_FULL) {
			if (--tmo == 0)
				return -2;
		}
		fes_write32(base + SMHC_FIFO, src[i]);
	}

	/* completion: CMD_DONE and DATA_OVER */
	tmo = 1000000;
	while (!(fes_read32(base + SMHC_RINT) & (RINT_CMD_DONE | RINT_ERR))) {
		if (--tmo == 0)
			return -3;
	}
	tmo = 8000000;
	while (!(fes_read32(base + SMHC_RINT) &
		 ((nblk > 1) ? (RINT_AUTO_CMD_DONE | RINT_ERR)
			     : (RINT_DATA_OVER | RINT_ERR)))) {
		if (--tmo == 0)
			return -4;
	}
	#ifdef CONFIG_FES_DEBUG
	printf("[FES] wr done wait=%u rint=%x\n", 8000000 - tmo,
	       fes_read32(base + SMHC_RINT));
#endif
	if (fes_read32(base + SMHC_RINT) & RINT_ERR) {
		printf("[FES] wr err rint=%x\n", fes_read32(base + SMHC_RINT));
		return -5;	/* a real RINT_ERR bit */
	}
	/* wait for card busy after the write (eMMC programming; an immediate
	 * read gets RESP_TIMEOUT) */
	tmo = 4000000;
	while (fes_read32(base + SMHC_STATUS) & STATUS_CARD_BUSY) {
		if (--tmo == 0) {
			printf("[FES] wr busy tmo st=%x\n",
			       fes_read32(base + SMHC_STATUS));
			return -9;
		}
	}

	return 0;
}

/* One 128 KiB block, issued without waiting for the card: the caller gets
 * control back while the IDMAC still copies.  Returns 0 once the command is
 * on the bus, and the outcome is reported by fes_mmc_write_wait_base(). */
int fes_mmc_write_start_base(uint base, unsigned long start, const void *buf,
			     unsigned int nblk)
{
	if (fes_mmc_write_wait_base())
		return -1;	/* the previous block failed: do not build on it */
	if (nblk > fes_mmc_chunk_sectors)
		return fes_mmc_write_blocks_base(base, start, buf, nblk);
#ifdef CONFIG_FES_SMHC_DMA
	if (nblk * 512 <= 524288 && nblk > 1)
		return fes_mmc_dma_write_start(base, start, buf, nblk);
#endif
	return fes_mmc_pio_write_raw(base, start, buf, nblk);
}

int fes_mmc_write_blocks(void *mmc, unsigned long start, const void *buf,
			 unsigned int nblk)
{
	return fes_mmc_write_blocks_base(fes_mmc_reg_base(mmc), start, buf,
					 nblk);
}

int fes_mmc_write_start(void *mmc, unsigned long start, const void *buf,
			unsigned int nblk)
{
	return fes_mmc_write_start_base(fes_mmc_reg_base(mmc), start, buf,
					nblk);
}

int fes_mmc_write_blocks_base(uint base, unsigned long start,
			      const void *buf, unsigned int nblk)
{
	if (nblk > fes_mmc_chunk_sectors) {
		unsigned int off = 0;

		while (off < nblk) {
			unsigned int n = nblk - off;
			int rc;

			if (n > fes_mmc_chunk_sectors)
				n = fes_mmc_chunk_sectors;
			rc = fes_mmc_write_blocks_base(base, start + off,
						       (const char *)buf + off * 512,
						       n);
			if (rc)
				return rc;
			off += n;
		}
		return 0;
	}
#ifdef CONFIG_FES_DEBUG
	printf("[FES] wrbase %x s=%lu n=%u\n", base, start, nblk);
#endif
#ifdef CONFIG_FES_SMHC_DMA
	if (nblk * 512 <= 524288 && nblk > 1 &&
	    fes_mmc_dma_write_raw(base, start, buf, nblk) == 0)
		return 0;
#endif
	/* Fully local data command: the library data path never fills the FIFO
	 * (cmd25 data timeout) */
	return fes_mmc_pio_write_raw(base, start, buf, nblk);
}


/* ---- SD raw-base entry points (used after fes_sd_init) ---- */
int fes_sd_write(uint base, unsigned long start, const void *buf,
		 unsigned int nblk)
{
	return fes_mmc_write_blocks_base(base, start, buf, nblk);
}

int fes_sd_read(uint base, unsigned long start, void *dst, unsigned int nblk)
{
	return fes_mmc_read_blocks_base(base, start, dst, nblk);
}
