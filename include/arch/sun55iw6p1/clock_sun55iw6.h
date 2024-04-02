#include <config.h>
#include <arch/cpu.h>
#include <arch/sun55iw6p1/clock_autogen.h>
#define sunxi_ccm_reg	  CCMU_st
#define pll1_cfg	  pll_cpu0_ctrl_reg
#define apb2_cfg	  apb1_clk_reg
#define uart_gate_reset	  uart_bgr_reg
#define cpu_axi_cfg	  cpu_clk_reg
#define pll6_cfg	  pll_peri0_ctrl_reg
#define psi_ahb1_ahb2_cfg ahb_clk_reg
#define apb1_cfg	  apb0_clk_reg
#define mbus_cfg	  mbus_clk_reg
#define ve_clk_cfg	  ve_clk_reg
#define de_clk_cfg	  de0_clk_reg
#define mbus_gate	  mbus_mat_clk_gating_reg
#define dma_gate_reset	  dma0_bgr_reg
#define sd0_clk_cfg	  smhc0_clk_reg
#define sd1_clk_cfg	  smhc1_clk_reg
#define sd2_clk_cfg	  smhc2_clk_reg
#define sd_gate_reset	  smhc_bgr_reg
#define twi_gate_reset	  twi0_bgr_reg
#define ce_gate_reset	  ce_bgr_reg
#define ce_clk_cfg	  ce_clk_reg

/* MMC clock bit field */
#define SMHC0_CLK_SRC_PERI0_400M_FREQ (400000000)
#define SMHC0_CLK_SRC_PERI0_300M_FREQ (300000000)
#define SMHC2_CLK_SRC_PERI0_800M_FREQ (800000000)
#define SMHC2_CLK_SRC_PERI0_600M_FREQ (600000000)
#define CCM_MMC_CTRL_M(x) ((x)-1)
#define CCM_MMC_CTRL_N(x) ((x) << SMHC0_CLK_REG_FACTOR_N_OFFSET)
#define CCM_MMC_CTRL_OSCM24                                                    \
	(SMHC0_CLK_REG_CLK_SRC_SEL_HOSC << SMHC0_CLK_REG_CLK_SRC_SEL_OFFSET)
#define CCM_MMC_CTRL_PLL6X2                                                    \
	(SMHC0_CLK_REG_CLK_SRC_SEL_PERI0_400M                                  \
	 << SMHC0_CLK_REG_CLK_SRC_SEL_OFFSET)
#define CCM_MMC_CTRL_PLL_PERIPH2X2                                             \
	(SMHC0_CLK_REG_CLK_SRC_SEL_PERI0_300M                                  \
	 << SMHC0_CLK_REG_CLK_SRC_SEL_OFFSET)
#define CCM_MMC_CTRL_ENABLE                                                    \
	(SMHC0_CLK_REG_SMHC0_CLK_GATING_CLOCK_IS_ON                            \
	 << SMHC0_CLK_REG_SMHC0_CLK_GATING_OFFSET)
/* if doesn't have these delays */
#define CCM_MMC_CTRL_OCLK_DLY(a) ((void)(a), 0)
#define CCM_MMC_CTRL_SCLK_DLY(a) ((void)(a), 0)

/* Module gate/reset shift*/
#define RESET_SHIFT  (16)
#define GATING_SHIFT (0)

/* pll list */
#define CPU_L0_PLL_CTRL_REG        (SUNXI_CPU_PLL_CFG_BASE + 0x04)
#define CCMU_PLL_CPUX_CTRL_REG     (CPU_L0_PLL_CTRL_REG)

#define CCMU_PLL_PERI0_CTRL_REG	 (SUNXI_CCM_BASE + PLL_PERI0_CTRL_REG)
#define CCMU_PLL_PERI1_CTRL_REG	 (SUNXI_CCM_BASE + PLL_PERI1_CTRL_REG)


/* cfg list */
#define CCMU_REG_PLL_C0_CPUX				(SUNXI_CPU_PLL_CFG_BASE + 0x4)
#define CCMU_REG_PLL_C0_DSU				(SUNXI_CPU_PLL_CFG_BASE + 0x8)

#define CCMU_APB_CFG_GREG	   (SUNXI_CCM_BASE + APB0_CLK_REG)
#define CCMU_APB1_CFG_GREG	   (SUNXI_CCM_BASE + APB1_CLK_REG)
#define CCMU_MBUS_CFG_REG	   (SUNXI_CCM_BASE + MBUS_CLK_REG)
#define CCMU_NSI_CLK_GREG	   (SUNXI_CCM_BASE + NSI_CLK_REG)
#define CCMU_NSI_BGR_REG	   (SUNXI_CCM_BASE + NSI_BGR_REG)

#define CCMU_CE_CLK_REG (SUNXI_CCM_BASE + CE_CLK_REG)
#define CCMU_CE_BGR_REG (SUNXI_CCM_BASE + CE_BGR_REG)

/*SYS*/
#define CCMU_DMA_BGR_REG (SUNXI_CCM_BASE + DMA0_BGR_REG)

/* storage */
#define CCMU_MBUS_MST_CLK_GATING_REG (SUNXI_CCM_BASE + MBUS_MAT_CLK_GATING_REG)

#define CCMU_SDMMC0_CLK_REG (SUNXI_CCM_BASE + SMHC0_CLK_REG)
#define CCMU_SDMMC1_CLK_REG (SUNXI_CCM_BASE + SMHC1_CLK_REG)
#define CCMU_SDMMC2_CLK_REG (SUNXI_CCM_BASE + SMHC2_CLK_REG)
#define CCMU_SMHC0_BGR_REG   (SUNXI_CCM_BASE + SMHC0_BGR_REG)

/*normal interface*/
#define CCMU_UART_BGR_REG (SUNXI_CCM_BASE + UART0_BGR_REG)

/*CE */
#define CE_CLK_SRC_MASK	   (0x1)
#define CE_CLK_SRC_SEL_BIT CE_CLK_REG_CLK_SRC_SEL_OFFSET
#define CE_CLK_SRC	   CE_CLK_REG_CLK_SRC_SEL_PERI0_400M

#define CE_CLK_DIV_RATION_M_BIT	 CE_CLK_REG_FACTOR_M_OFFSET
#define CE_CLK_DIV_RATION_M_MASK CE_CLK_REG_FACTOR_M_CLEAR_MASK
#define CE_CLK_DIV_RATION_M	 (2)

#define CE_SCLK_ONOFF_BIT (31)
#define CE_SCLK_ON	  (1)

#define CE_GATING_BASE CCMU_CE_BGR_REG
#define CE_GATING_PASS (1)
#define CE_GATING_BIT  (0)

#define CE_RST_REG_BASE CCMU_CE_BGR_REG
#define CE_RST_BIT	CE_BGR_REG_CE_RST_OFFSET
#define CE_DEASSERT	(1)

#define CE_SYS_GATING_BIT CE_BGR_REG_CE_GATING_MASK
#define CE_SYS_RST_BIT CE_BGR_REG_CE_SYS_RST_OFFSET

/*gpadc gate and reset reg*/
#define CCMU_GPADC_BGR_REG (SUNXI_CCM_BASE + GPADC0_CLK_REG)

/*lpadc gate and reset reg*/
#define CCMU_LRADC_BGR_REG (SUNXI_CCM_BASE + LRADC_BGR_REG)
