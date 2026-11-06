/*
 * (C) Copyright 2018
 * wangwei <wangwei@allwinnertech.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <common.h>
#include <private_boot0.h>
#include <private_uboot.h>
#include <private_toc.h>
#include <arch/clock.h>
#include <arch/uart.h>
#include <arch/dram.h>
#include <arch/rtc.h>
#include <boot_param.h>

/* FES mode magic in RTC data idx5 (0x07090114):
 *   FES1 - boot0 already trained the DRAM, enter FES directly;
 *   TFES - train first, then enter FES (cold single-exec flow). */
#define FES_MAGIC_FES		0x46455331u	/* "FES1" */
#define FES_MAGIC_TRAIN		0x53454654u	/* "TFES" */

/* ====================================================================
 * Custom FES1 addition: DRAM rail control via proven TWI0 sequence.
 * libdram's dram_vol_set() calls set_ddr_voltage_ext(name, mv, on);
 * we drive AXP8191 directly (single chained session, like stub_pmu19).
 * ==================================================================== */

#define FES_TWI_BASE	0x07083000u
#define FES_PRCM_BASE	0x07010000u
#define FES_TWI_DTR	(*(volatile unsigned *)(FES_TWI_BASE + 0x08))
#define FES_TWI_CTR	(*(volatile unsigned *)(FES_TWI_BASE + 0x0c))
#define FES_TWI_STR	(*(volatile unsigned *)(FES_TWI_BASE + 0x10))
#define FES_TWI_SRR	(*(volatile unsigned *)(FES_TWI_BASE + 0x18))
#define FES_TWI_EFR	(*(volatile unsigned *)(FES_TWI_BASE + 0x1c))

static int fes_rail_session;	/* 0 = need re-init + start, 1 = chained */

/* hybrid experiment: call boot0's sunxi_board_init @0x48fac (boot0 image loaded at 0x47000) */
#define FES_HYBRID_BOARDINIT 1

static void fes_icache_inv(void)
{
	__asm__ volatile("mcr p15, 0, %0, c7, c5, #0" :: "r"(0));	/* ICIALLU */
	__asm__ volatile("mcr p15, 0, %0, c7, c10, #4" :: "r"(0));	/* DSB */
	__asm__ volatile("mcr p15, 0, %0, c7, c5, #4" :: "r"(0));	/* ISB */
}

static void __attribute__((unused)) fes_dump_regs(unsigned base, int words, const char *tag)
{
	int i;

	printf("DUMP %s @%08x\n", tag, base);
	for (i = 0; i < words; i++) {
		if ((i & 7) == 0)
			printf("%08x:", base + i * 4);
		printf(" %08x", *(volatile unsigned *)(base + i * 4));
		if ((i & 7) == 7)
			printf("\n");
	}
	if (words & 7)
		printf("\n");
}

static void fes_delay(volatile unsigned n)
{
	while (n--)
		;
}

static void fes_twi_flag_clear(void)
{
	volatile unsigned n = 0xFFFF;

	FES_TWI_CTR |= 0x08;
	while ((FES_TWI_CTR & 0x08) && --n)
		;
}

static void fes_twi_flag_wait(void)
{
	volatile unsigned n = 0xFFFF;

	while (!(FES_TWI_CTR & 0x08) && --n)
		;
}

static void fes_twi_start(void)
{
	FES_TWI_CTR |= 0x20;		/* MSTART */
	FES_TWI_CTR &= ~0x08;
	fes_twi_flag_clear();
	fes_twi_flag_wait();
}

static void fes_twi_byte(unsigned v)
{
	FES_TWI_DTR = v;
	fes_twi_flag_clear();
	fes_twi_flag_wait();
}

static unsigned fes_twi_read_byte(void)
{
	unsigned v;

	FES_TWI_CTR &= ~0x04;		/* AACK off -> NACK last byte */
	FES_TWI_CTR &= ~0x08;
	fes_twi_flag_clear();
	fes_twi_flag_wait();
	v = FES_TWI_DTR & 0xff;
	return v;
}

static void fes_twi_txpre(void)
{
	volatile unsigned n = 0xFFFF;

	FES_TWI_SRR = 1;
	while (FES_TWI_SRR && --n)
		;
	FES_TWI_CTR = 0x44;		/* BUS_ENB | AACK */
	FES_TWI_EFR = 0;
	n = 2000000;
	while (FES_TWI_STR != 0xF8 && --n)
		;
}

static unsigned fes_twi_read_reg8(unsigned reg)
{
	fes_twi_start();
	fes_twi_byte(0x6c);		/* slave write */
	fes_twi_byte(reg);		/* reg addr */
	fes_twi_start();		/* restart */
	fes_twi_byte(0x6d);		/* slave read */
	return fes_twi_read_byte();
}

static void fes_twi_write_reg8(unsigned reg, unsigned val)
{
	fes_twi_start();
	fes_twi_byte(0x6c);
	fes_twi_byte(reg);
	fes_twi_byte(val);
}

/* power tree needed before DRAM access (mirrors boot0):
 * dc1sw1/dcdc1 = 3.3V, dldo2/dldo3 = 1.8V  (+ rail enables)
 */
static void __attribute__((unused)) fes_pmu_power_tree(void)
{
	unsigned v;
	unsigned id;

	*(volatile unsigned *)(FES_PRCM_BASE + 0x19c) |= (1u << 16) | 1u;
	fes_twi_txpre();

	id = fes_twi_read_reg8(0x03);	/* chip id canary */
	printf("pmu id=0x%02x\n", id);

	v = fes_twi_read_reg8(0x12);	/* DCDC1 3.3V */
	fes_twi_write_reg8(0x12, (v & ~0x1f) | (((3300 - 1000) / 100) & 0x1f));
	fes_delay(300000);
	v = fes_twi_read_reg8(0x10);	/* DCDC1 enable */
	fes_twi_write_reg8(0x10, v | 0x01);
	v = fes_twi_read_reg8(0x11);	/* dc1sw1 enable */
	fes_twi_write_reg8(0x11, v | 0x08);
	fes_delay(300000);

	v = fes_twi_read_reg8(0x35);	/* DLDO2 1.8V */
	fes_twi_write_reg8(0x35, (v & ~0x1f) | (((1800 - 500) / 100) & 0x1f));
	v = fes_twi_read_reg8(0x36);	/* DLDO3 1.8V */
	fes_twi_write_reg8(0x36, (v & ~0x1f) | (((1800 - 500) / 100) & 0x1f));
	v = fes_twi_read_reg8(0x22);	/* LDO CTL3: dldo2|dldo3 enable */
	fes_twi_write_reg8(0x22, v | 0x06);
	fes_delay(300000);

	printf("pmu 12=%02x 35=%02x 36=%02x 10=%02x 11=%02x 22=%02x\n",
	       fes_twi_read_reg8(0x12), fes_twi_read_reg8(0x35),
	       fes_twi_read_reg8(0x36), fes_twi_read_reg8(0x10),
	       fes_twi_read_reg8(0x11), fes_twi_read_reg8(0x22));
	fes_rail_session = 0;
}

int set_ddr_voltage_ext(const char *name, int mv, int on)
{
	unsigned reg, val;

	if (!on)
		return 0;

	if (!strcmp(name, "dcdc7")) {
		reg = 0x18;
		val = ((mv - 500) / 10) & 0x7f;
	} else if (!strcmp(name, "dcdc6")) {
		reg = 0x17;
		val = ((mv - 500) / 10) & 0x7f;
	} else if (!strcmp(name, "dcdc8")) {
		reg = 0x19;
		val = ((mv - 500) / 10) & 0x7f;
	} else if (!strcmp(name, "eldo1")) {
		reg = 0x3A;
		val = ((mv - 500) / 25) & 0x3f;
	} else if (!strcmp(name, "eldo2")) {
		reg = 0x3B;
		val = ((mv - 500) / 25) & 0x3f;
	} else {
		printf("rail %s unknown\n", name);
		return 0;
	}

	/* a set starts with dcdc7/dcdc8 (see libdram dram_vol_set) */
	if (!fes_rail_session) {
		*(volatile unsigned *)(FES_PRCM_BASE + 0x19c) |=
			(1u << 16) | 1u;
		fes_twi_txpre();
		fes_rail_session = 1;
	}

	printf("rail %s %dmv r%02x=%02x\n", name, mv, reg, val | 0x80);
	fes_twi_write_reg8(reg, val | 0x80);
	if (!strcmp(name, "eldo1"))
		fes_twi_write_reg8(0x22, fes_twi_read_reg8(0x22) | 0x40);
	fes_delay(1500000);		/* rail settle (as stub_pmu17/19) */
	return 0;
}

void sunxi_smc_en_with_glitch_workaround(void)
{
}

/* Raw hex dump built with %s: printf %x is empty in this build. */
static void fes_hex_puts(const char *tag, unsigned base, unsigned nwords)
{
	char buf[9 * 11 + 1];
	int i, n = 0;

	for (i = 0; i < nwords && i < 8; i++) {
		unsigned v = *(volatile unsigned *)(base + i * 4);
		int j;
		buf[n++] = ' ';
		for (j = 28; j >= 0; j -= 4) {
			unsigned d = (v >> j) & 0xf;
			buf[n++] = (char)(d <= 9 ? '0' + d : 'a' + d - 10);
		}
	}
	buf[n] = '\0';
	printf("%s%s\n", tag, buf);
}

static void fes_dump_ddr_state(const char *tag)
{
	fes_hex_puts(" ctrl[00/1c/20/2c/30/3c]: ", 0x08871000, 6);
	fes_hex_puts(" ctrl20[00/1c/80]:        ", 0x08872000, 3);
	fes_hex_puts(" pll 0x02002c00:         ", 0x02002c00, 1);
	fes_hex_puts(" phy 0x0a110c80:         ", 0x0a110c80, 1);
	fes_hex_puts(" phy 0x0a110510:         ", 0x0a110510, 1);
	fes_hex_puts(" phy 0x0a510510:         ", 0x0a510510, 1);
	fes_hex_puts(" phy 0x0a92009c:         ", 0x0a92009c, 1);
}

static void  note_dram_log(int dram_init_flag);


int main(void)
{
	int dram_size=0;
	int status;
	sunxi_serial_init(fes1_head.prvt_head.uart_port, (void *)fes1_head.prvt_head.uart_ctrl, 2);
	{
		/* Boot counter probe (RTC slot 0x07090118, next to the magic):
		 * a value above 1 means the CPU has restarted. */
		volatile unsigned *bc = (volatile unsigned *)0x07090118;
		*bc = *bc + 1;
		printf("boot_count=%u\n", *bc);
	}
	/* Disable the watchdogs boot0 leaves running: nothing feeds them
	 * during FES, so they reset the board after ~100 s (rtc[6] = 0x1
	 * confirmed the restart).  Clear MODE and CTRL unconditionally. */
	{
		/* Main-domain WDT (0x02050008) and R-domain WDG
		 * (0x07021000): disable both or they reset the board after
		 * ~100 s without a feeder. */
		unsigned wmode = *(volatile unsigned *)0x02050008;
		unsigned rw_c = *(volatile unsigned *)0x07021000;
		unsigned rw_cfg = *(volatile unsigned *)0x07021004;
		unsigned rw_m = *(volatile unsigned *)0x07021008;
		*(volatile unsigned *)0x02050008 = 0;
		*(volatile unsigned *)0x02050000 = 0;
		*(volatile unsigned *)0x07021008 = 0;
		*(volatile unsigned *)0x07021000 = 0;
		*(volatile unsigned *)0x07021004 = 0;
		printf("wdogs: main=%08x rwdr=%08x rcfg=%08x rmode=%08x -> cleared\n",
		       wmode, rw_c, rw_cfg, rw_m);
	}
	printf("fes begin commit:%s\n", fes1_head.hash);

	/* Host handshake: the host writes the magic to RTC data idx5
	 * (0x07090114).  "FES1" takes the early exit because boot0 already
	 * trained the DRAM; "TFES" does not, so a cold FEL trains first and
	 * enters FES in the same execute. */
	if (*(volatile unsigned *)0x07090114 == FES_MAGIC_FES) {
		*(volatile unsigned *)0x07090114 = 0;	/* consume the magic */
		extern void fes1_fes_entry(void);
		fes1_fes_entry();			/* does not return */
	}

	status = sunxi_board_init();
	if(status)
		return 0;
	sunxi_bootparam_load();
	printf("beign to init dram\n");
#ifdef FES_HYBRID_BOARDINIT
	{
		int mode;
#ifndef FES_NO_CCU13
		int i;
		static const unsigned ccu_list[] = {
			0x02002020, 0x020020a0, 0x020020c0, 0x020020e0,
			0x02002120, 0x02002140, 0x02002160,
			0x02002220, 0x02002240, 0x02002260, 0x02002280,
			0x020022a0, 0x020022e0,
		};
#endif

		fes_icache_inv();

		/* step1/2: boot mode + gpio withstand (direct) */
		mode = (*(volatile unsigned *)0x03000024) & 7;
		printf("mode=%d\n", mode);
		if (mode == 1) {
			*(volatile unsigned *)0x02000040 = 0x01155550;
			printf("withstand set\n");
		}
		*(volatile unsigned *)0x08000144 =
			(mode == 1) ? 0x0a7060025 : 0x0a7070025;
		printf("rtc144=%08x\n", *(volatile unsigned *)0x08000144);

		/* dcdc2 = 840mV via our TWI; hard-reset TWI0 via PRCM gate first */
		fes_rail_session = 0;
		*(volatile unsigned *)(FES_PRCM_BASE + 0x19c) &= ~1u;	/* gate off */
		fes_delay(500000);
		*(volatile unsigned *)(FES_PRCM_BASE + 0x19c) |= (1u << 16) | 1u;	/* on */
		fes_delay(500000);
		printf("twi ckr=%08x str=%08x ct=%08x\n",
		       *(volatile unsigned *)0x07083000,
		       *(volatile unsigned *)(0x07083000 + 0x10),
		       *(volatile unsigned *)(0x07083000 + 0x0c));
		fes_twi_txpre();
		fes_twi_write_reg8(0x13, 0xa2);	/* dcdc2 = 840mV | 0x80 */
		fes_twi_write_reg8(0x10, 0xff);	/* enable ALL DCDC rails (ctrl1) */
		/* Preset the DRAM rails: libdram's first cold ramp leaves the
		 * training at 0xff (cold fails, a warm boot0 rail passes; the
		 * values below come from libdram's own r18/r3a/r17 dump). */
		fes_twi_write_reg8(0x18, 0xba);	/* dcdc7 = 1080mV (r18=ba) */
		fes_twi_write_reg8(0x3a, 0x97);	/* eldo1 = 1080mV (r3a=97) */
		fes_twi_write_reg8(0x17, 0x86);	/* dcdc6 = 560mV DRAM_VCC (r17=86) */
		/* AXP8191 LDO_POWER_ON_OFF_CTL3(0x22) bit6 = eldo1 enable.  The
		 * voltage register alone leaves the DRAM VDDQ rail off on a cold
		 * boot; only boot0's own libdram enables it on the warm path, so
		 * cold FEL training failed at Pstate1 with 0xff until this bit
		 * was set (verified cold: all pstates 7, init dram ok). */
		fes_twi_write_reg8(0x22, fes_twi_read_reg8(0x22) | 0x40);
		fes_delay(1000000);
		printf("rails pre-set: 18=%02x 3a=%02x 17=%02x 22=%02x\n",
		       fes_twi_read_reg8(0x18), fes_twi_read_reg8(0x3a),
		       fes_twi_read_reg8(0x17), fes_twi_read_reg8(0x22));
		fes_delay(1000000);
		printf("dcdc2=840 set\n");
		fes_delay(1500000);

		/* step5: the 13x CCU |= 1<<24 (boot0 0x491b8 head part) */
#ifndef FES_NO_CCU13
		for (i = 0; i < (int)(sizeof(ccu_list) / sizeof(ccu_list[0])); i++)
			*(volatile unsigned *)ccu_list[i] |= (1u << 24);
		printf("ccu13 set\n");
#else
		printf("ccu13 SKIPPED\n");
#endif

		/* rtc reg */
		*(volatile unsigned *)0x08000200 |= 1;

		printf("===PRE-DRAM===\n"); fes_dump_ddr_state("");

		dram_size = init_DRAM(0, (void *)sunxi_bootparam_get_dram_buf());

		printf("===POST-DRAM===\n"); fes_dump_ddr_state("");
	} 
#else
	fes_pmu_power_tree();
	dram_size = init_DRAM(0, (void *)sunxi_bootparam_get_dram_buf());
#endif
	if (rtc_probe_fel_flag()) {
		rtc_clear_fel_flag();
	}
	if (dram_size) {
		note_dram_log(1);
		printf("init dram ok\n");
		/* FES mode: FES1 skips training on a warm board; TFES trains
		 * and enters FES in the same execute from a cold FEL. */
		{
			unsigned magic = *(volatile unsigned *)0x07090114;

			if (magic == FES_MAGIC_FES || magic == FES_MAGIC_TRAIN) {
				*(volatile unsigned *)0x07090114 = 0;	/* consume the magic */
				extern void fes1_fes_entry(void);
				fes1_fes_entry();		/* does not return */
			}
		}
	} else {
		note_dram_log(0);
		printf("init dram fail\n");
		/* On failure return to the BROM instead of spinning, so the
		 * host can retry warm (verified: exec-return leaves FEL intact). */
	}

	mdelay(10);

	return dram_size;
}

static void note_dram_log(int dram_init_flag)
{
	fes_aide_info_t *fes_aide =
		(fes_aide_info_t *)&fes1_head.fes_union_addr.fes1_res_addr;

	memset(fes_aide, 0, sizeof(fes_aide_info_t));
	fes_aide->dram_init_flag   = SYS_PARA_LOG;
	fes_aide->dram_update_flag = dram_init_flag;

	memcpy(fes_aide->dram_paras, fes1_head.prvt_head.dram_para,
	       SUNXI_DRAM_PARA_MAX * 4);
	memcpy((void *)DRAM_PARA_STORE_ADDR, fes1_head.prvt_head.dram_para,
	       SUNXI_DRAM_PARA_MAX * 4);

#ifdef CFG_SUNXI_BOOT_PARAM
#define BOOT_PARAM_FES_TRANSFER_UBOOT_ADDR                                     \
	(DRAM_PARA_STORE_ADDR + SUNXI_DRAM_PARA_MAX * 4)
	sunxi_bootparam_format_and_transfer((void *)BOOT_PARAM_FES_TRANSFER_UBOOT_ADDR);
#endif
}
