/*
 * (C) Copyright 2013-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 */

#include <common.h>
#include <asm/io.h>
#include <asm/csr.h>

#define L1_CACHE_BYTES (64)
#define CCTL_L1D_WBINVAL_ALL 6
#define CCTL_REG_MCCTLCOMMAND_NUM 0x7cc

#define CSR_MCACHE_CTL                                                  0x7ca
#define CSR_MMISC_CTL                                                   0x7d0
#define CSR_MICM_CFG                                                    0xfc0
#define CSR_MDCM_CFG                                                    0xfc1
#define CSR_MMSC_CFG                                                    0xfc2

/* MMSC configuration register */

#define BIT(nr)			(1UL << (nr))

#define V5_MMSC_CFG_TLB_ECC_OFFSET_1                    1
#define V5_MMSC_CFG_TLB_ECC_OFFSET_2                    2

#define V5_MMSC_CFG_TLB_ECC_1                                   BIT(V5_MMSC_CFG_TLB_ECC_OFFSET_1)
#define V5_MMSC_CFG_TLB_ECC_2                                   BIT(V5_MMSC_CFG_TLB_ECC_OFFSET_2)

/* MICM configuration regiser */
#define V5_MICM_CFG_IC_ECC_OFFSET_1                             10
#define V5_MICM_CFG_IC_ECC_OFFSET_2                             11

#define V5_MICM_CFG_IC_ECC_1                                    BIT(V5_MICM_CFG_IC_ECC_OFFSET_1)
#define V5_MICM_CFG_IC_ECC_2                                    BIT(V5_MICM_CFG_IC_ECC_OFFSET_2)

/* MDCM configuration regiser */
#define V5_MDCM_CFG_DC_ECC_OFFSET_1                             10
#define V5_MDCM_CFG_DC_ECC_OFFSET_2                             11

#define V5_MDCM_CFG_DC_ECC_1                                    BIT(V5_MDCM_CFG_DC_ECC_OFFSET_1)
#define V5_MDCM_CFG_DC_ECC_2                                    BIT(V5_MDCM_CFG_DC_ECC_OFFSET_2)

/* MMISC control register */
#define V5_MMISC_CTL_NON_BLOCKING_OFFSET                8
#define V5_MMISC_CTL_NON_BLOCKING_EN                    BIT(V5_MMISC_CTL_NON_BLOCKING_OFFSET)

/* MCACHE control register */
#define V5_MCACHE_CTL_IC_EN_OFFSET                              0
#define V5_MCACHE_CTL_DC_EN_OFFSET                              1
#define V5_MCACHE_CTL_IC_ECCEN_OFFSET_1                 2
#define V5_MCACHE_CTL_IC_ECCEN_OFFSET_2                 3
#define V5_MCACHE_CTL_DC_ECCEN_OFFSET_1                 4
#define V5_MCACHE_CTL_DC_ECCEN_OFFSET_2                 5
#define V5_MCACHE_CTL_CCTL_SUEN_OFFSET                  8
#define V5_MCACHE_CTL_L1I_PREFETCH_OFFSET               9
#define V5_MCACHE_CTL_L1D_PREFETCH_OFFSET               10
#define V5_MCACHE_CTL_DC_WAROUND_OFFSET_1               13
#define V5_MCACHE_CTL_DC_WAROUND_OFFSET_2               14
#define V5_MCACHE_CTL_L2C_WAROUND_OFFSET_1              15
#define V5_MCACHE_CTL_L2C_WAROUND_OFFSET_2              16
#define V5_MCACHE_CTL_TLB_ECCEN_OFFSET_1                17
#define V5_MCACHE_CTL_TLB_ECCEN_OFFSET_2                18
#define V5_MCACHE_CTL_DC_COHEN_OFFSET                   19
#define V5_MCACHE_CTL_DC_COHSTA_OFFSET                  20

#define V5_MCACHE_CTL_IC_EN                                             BIT(V5_MCACHE_CTL_IC_EN_OFFSET)
#define V5_MCACHE_CTL_DC_EN                                             BIT(V5_MCACHE_CTL_DC_EN_OFFSET)
#define V5_MCACHE_CTL_IC_ECCEN_1                                BIT(V5_MCACHE_CTL_IC_ECCEN_OFFSET_1)
#define V5_MCACHE_CTL_IC_ECCEN_2                                BIT(V5_MCACHE_CTL_IC_ECCEN_OFFSET_2)
#define V5_MCACHE_CTL_DC_ECCEN_1                                BIT(V5_MCACHE_CTL_DC_ECCEN_OFFSET_1)
#define V5_MCACHE_CTL_DC_ECCEN_2                                BIT(V5_MCACHE_CTL_DC_ECCEN_OFFSET_2)
#define V5_MCACHE_CTL_CCTL_SUEN                                 BIT(V5_MCACHE_CTL_CCTL_SUEN_OFFSET)
#define V5_MCACHE_CTL_L1I_PREFETCH_EN                   BIT(V5_MCACHE_CTL_L1I_PREFETCH_OFFSET)
#define V5_MCACHE_CTL_L1D_PREFETCH_EN                   BIT(V5_MCACHE_CTL_L1D_PREFETCH_OFFSET)
#define V5_MCACHE_CTL_DC_WAROUND_1_EN                   BIT(V5_MCACHE_CTL_DC_WAROUND_OFFSET_1)
#define V5_MCACHE_CTL_DC_WAROUND_2_EN                   BIT(V5_MCACHE_CTL_DC_WAROUND_OFFSET_2)
#define V5_MCACHE_CTL_L2C_WAROUND_1_EN                  BIT(V5_MCACHE_CTL_L2C_WAROUND_OFFSET_1)
#define V5_MCACHE_CTL_L2C_WAROUND_2_EN                  BIT(V5_MCACHE_CTL_L2C_WAROUND_OFFSET_2)
#define V5_MCACHE_CTL_TLB_ECCEN_1                               BIT(V5_MCACHE_CTL_TLB_ECCEN_OFFSET_1)
#define V5_MCACHE_CTL_TLB_ECCEN_2                               BIT(V5_MCACHE_CTL_TLB_ECCEN_OFFSET_2)
#define V5_MCACHE_CTL_DC_COHEN_EN                               BIT(V5_MCACHE_CTL_DC_COHEN_OFFSET)
#define V5_MCACHE_CTL_DC_COHSTA_EN                              BIT(V5_MCACHE_CTL_DC_COHSTA_OFFSET)

#ifdef CFG_USE_DCACHE
/* actually it's open dcache and icache */
__weak void dcache_enable(void)
{
	unsigned long long mcache_ctl_val = csr_read(CSR_MCACHE_CTL);
	unsigned long long mmisc_ctl_val = csr_read(CSR_MMISC_CTL);
	unsigned long long mmsc_cfg_val = csr_read(CSR_MMSC_CFG);
	unsigned long long mdcm_cfg_val = csr_read(CSR_MICM_CFG);
	unsigned long long micm_cfg_val = csr_read(CSR_MDCM_CFG);

	mcache_ctl_val |= (V5_MCACHE_CTL_DC_COHEN_EN | V5_MCACHE_CTL_IC_EN | \
			V5_MCACHE_CTL_DC_EN | V5_MCACHE_CTL_CCTL_SUEN | \
			V5_MCACHE_CTL_L1I_PREFETCH_EN | V5_MCACHE_CTL_L1D_PREFETCH_EN | \
			V5_MCACHE_CTL_DC_WAROUND_1_EN | V5_MCACHE_CTL_L2C_WAROUND_1_EN);

	if ((mmsc_cfg_val & V5_MMSC_CFG_TLB_ECC_1) || (mmsc_cfg_val & V5_MMSC_CFG_TLB_ECC_2))
		mcache_ctl_val |= V5_MCACHE_CTL_TLB_ECCEN_2;
	if ((micm_cfg_val & V5_MICM_CFG_IC_ECC_1) || (micm_cfg_val & V5_MICM_CFG_IC_ECC_2))
		mcache_ctl_val |= V5_MCACHE_CTL_IC_ECCEN_2;
	if ((mdcm_cfg_val & V5_MDCM_CFG_DC_ECC_1) || (mdcm_cfg_val & V5_MDCM_CFG_DC_ECC_1))
		mcache_ctl_val |= V5_MCACHE_CTL_DC_ECCEN_2;

	csr_write(CSR_MCACHE_CTL, mcache_ctl_val);

	/*
	 * Check DC_COHEN_EN, if cannot write to mcache_ctl,
	 * we assume this bitmap not support L2 CM
	 */
	mcache_ctl_val = csr_read(CSR_MCACHE_CTL);
	if ((mcache_ctl_val & V5_MCACHE_CTL_DC_COHEN_EN)) {

		/* Wait for DC_COHSTA bit be set */
		while (!(mcache_ctl_val & V5_MCACHE_CTL_DC_COHSTA_EN))
			mcache_ctl_val = csr_read(CSR_MCACHE_CTL);
	}

	if (!(mmisc_ctl_val & V5_MMISC_CTL_NON_BLOCKING_EN))
		mmisc_ctl_val |= V5_MMISC_CTL_NON_BLOCKING_EN;

	csr_write(CSR_MMISC_CTL, mmisc_ctl_val);
}

__weak void dcache_disable(void)
{

}
#endif



__weak void mmu_enable(u32 dram_size)
{
#ifdef CFG_USE_DCACHE
	dcache_enable();
#endif
}

__weak void mmu_disable(void)
{

}

void data_sync_barrier(void)
{
	asm volatile("fence.i");
}

void flush_dcache_all(void)
{
	csr_write(CCTL_REG_MCCTLCOMMAND_NUM, CCTL_L1D_WBINVAL_ALL);
}

#ifdef CFG_USE_DCACHE
void flush_dcache_range(unsigned long start, unsigned long end)
{
	register unsigned long i asm("a0") = start & ~(L1_CACHE_BYTES - 1);
	for (; i < end; i += L1_CACHE_BYTES)
		asm volatile(".long 0x0295000b");	/*dcache.cpa a0*/
	asm volatile(".long 0x01b0000b");		/*sync.is*/
}

void invalidate_dcache_range(unsigned long start, unsigned long end)
{
	flush_dcache_all();
}
#else /*CFG_USE_DCACHE*/
void invalidate_dcache_range(__maybe_unused unsigned long start,
			     __maybe_unused unsigned long stop)
{
}

void flush_dcache_range(__maybe_unused unsigned long start,
			__maybe_unused unsigned long stop)
{
}
#endif

