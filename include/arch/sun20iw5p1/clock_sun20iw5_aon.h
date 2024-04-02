/*
 * for clk
 */

#ifndef __CCMU_AON_H
#define __CCMU_AON_H

#include <config.h>
#include <arch/cpu.h>

/* pll list */
#define CCMU_PLL_CPUX_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000000)
#define CCMU_PLL_PERI0_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000020)
#define CCMU_PLL_PERI1_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000024)
#define CCMU_PLL_VIDEO_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000040)
#define CCMU_PLL_CSI_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000048)
#define CCMU_AUDIO_CODEC_BGR_REG	 (SUNXI_CCM_AON_BASE + 0x00000078)
#define CCMU_PLL_DDR0_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000080)
#define CCMU_PLL_PERI_PAT0_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000120)
#define CCMU_PLL_PERI_PAT1_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000124)
#define CCMU_PLL_VIDEO_PAT0_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000140)
#define CCMU_PLL_VIDEO_PAT1_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000144)
#define CCMU_PLL_CSI_PAT0_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000148)
#define CCMU_PLL_CSI_PAT1_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x0000014c)
#define CCMU_PLL_AUDIO_PAT0_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000178)
#define CCMU_PLL_AUDIO_PAT1_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x0000017c)
#define CCMU_PLL_DDR_PAT0_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000180)
#define CCMU_PLL_DDR_PAT1_CTRL_REG	 (SUNXI_CCM_AON_BASE + 0x00000184)
#define CCMU_PLL_CPU_BIAS_REG	 (SUNXI_CCM_AON_BASE + 0x00000300)
#define CCMU_PLL_PERI_BIAS_REG	 (SUNXI_CCM_AON_BASE + 0x00000320)
#define CCMU_PLL_VIDEO_BIAS_REG	 (SUNXI_CCM_AON_BASE + 0x00000340)
#define CCMU_PLL_CSI_BIAS_REG	 (SUNXI_CCM_AON_BASE + 0x00000348)
#define CCMU_PLL_AUDIO_BIAS_REG	 (SUNXI_CCM_AON_BASE + 0x00000378)
#define CCMU_PLL_DDR_BIAS_REG	 (SUNXI_CCM_AON_BASE + 0x00000380)
#define CCMU_PLL_CPU_TUN_REG		 (SUNXI_CCM_AON_BASE + 0x00000400)
#define CCMU_PLL_FUNC_CFG_REG	 (SUNXI_CCM_AON_BASE + 0x00000404)

/* pattern list */
#define CCMU_PLL_AUDIO0_PAT0_REG	(SUNXI_CCM_AON_BASE + 0x178)

/* cfg list */
#define CCMU_CPUX_AXI_CFG_REG              (SUNXI_CCM_AON_BASE + 0xD00)
#define CCMU_PSI_AHB1_AHB2_CFG_REG         (SUNXI_CCM_AON_BASE + 0x510)
#define CCMU_APB1_CFG_GREG                 (SUNXI_CCM_AON_BASE + 0x520)
#define CCMU_APB2_CFG_GREG                 (SUNXI_CCM_AON_BASE + 0x524)
#define CCMU_MBUS_CFG_REG                  (SUNXI_CCM_AON_BASE + 0x540)

#define CCMU_CE_CLK_REG                    (SUNXI_CCM_AON_BASE + 0x680)
#define CCMU_CE_BGR_REG                    (SUNXI_CCM_AON_BASE + 0x68C)

/*SYS*/
#define CCMU_DMA_BGR_REG                   (SUNXI_CCM_AON_BASE + 0x70C)
#define CCMU_AVS_CLK_REG                   (SUNXI_CCM_AON_BASE + 0x740)

/* storage */
#define CCMU_DRAM_CLK_REG                  (SUNXI_CCM_AON_BASE + 0x800)
#define CCMU_MBUS_MST_CLK_GATING_REG       (SUNXI_CCM_AON_BASE + 0x804)
#define CCMU_DRAM_BGR_REG                  (SUNXI_CCM_AON_BASE + 0x80C)

#define CCMU_NAND_CLK_REG                  (SUNXI_CCM_AON_BASE + 0x810)
#define CCMU_NAND_BGR_REG                  (SUNXI_CCM_AON_BASE + 0x82C)

/*normal interface*/
#define CCMU_UART_BGR_REG                  (SUNXI_CCM_APP_BASE + 0x80)
#define CCMU_TWI_BGR_REG                   (SUNXI_CCM_AON_BASE + 0x91C)

#define CCMU_SCR_BGR_REG                   (SUNXI_CCM_AON_BASE + 0x93C)

#define CCMU_SPI0_CLK_REG                  (SUNXI_CCM_AON_BASE + 0x940)
#define CCMU_SPI1_CLK_REG                  (SUNXI_CCM_AON_BASE + 0x944)
#define CCMU_SPI_BGR_CLK_REG               (SUNXI_CCM_AON_BASE + 0x96C)
#define CCMU_USB0_CLK_REG                  (SUNXI_CCM_AON_BASE + 0xA70)
#define CCMU_USB_BGR_REG                   (SUNXI_CCM_AON_BASE + 0xA8C)

/*DMA*/
#define DMA_GATING_BASE                  CCMU_DMA_BGR_REG
#define DMA_GATING_PASS                  (1)
#define DMA_GATING_BIT                   (0)

/*CE*/
#define CE_CLK_SRC_MASK                  (0x3)
#define CE_CLK_SRC_SEL_BIT               (24)
#define CE_CLK_SRC                       (0x01)

#define CE_CLK_DIV_RATION_N_BIT          (8)
#define CE_CLK_DIV_RATION_N_MASK         (0x3)
#define CE_CLK_DIV_RATION_N              (0)

#define CE_CLK_DIV_RATION_M_BIT          (0)
#define CE_CLK_DIV_RATION_M_MASK         (0xF)
#define CE_CLK_DIV_RATION_M              (2)

#define CE_SCLK_ONOFF_BIT                (31)
#define CE_SCLK_ON                       (1)

#define CE_GATING_BASE                   CCMU_CE_BGR_REG
#define CE_GATING_PASS                   (1)
#define CE_GATING_BIT                    (0)

#define CE_RST_REG_BASE                  CCMU_CE_BGR_REG
#define CE_RST_BIT                       (16)
#define CE_DEASSERT                      (1)

#define PLL_CPUX_TUNING_REG				(0x1400)
#endif
