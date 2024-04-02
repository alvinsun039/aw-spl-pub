/*
 * (C) Copyright allwinnertech
 *
 */

#ifndef _CPU_SUN20IW5_H
#define _CPU_SUN20IW5_H

/*CPUX*/
#define SUNXI_BROM_BASE                0x00000000
#define SUNXI_SRAM_C_BASE              0x02000000
#define SUNXI_VE_BASE                  0x41C0E000
#define SUNXI_PIO_BASE                0x42000000
#define SUNXI_PWM_BASE                 0x42000C00
#define SUNXI_CCM_APP_BASE             0x42001000
#define SUNXI_GPADC_BASE               0x42009000
#define SUNXI_THS_BASE                 0x42009400
#define SUNXI_ADDA_BASE                0x42030000
#define SUNXI_I2S_BASE                 0x42032000
#define SUNXI_TIMER_BASE               0x42050000
#define SUNXI_TRNG_BASE                0x42059000
#define SUNXI_UART0_BASE               0x42500000
#define SUNXI_UART1_BASE               0x42500400
#define SUNXI_UART2_BASE               0x42500800
#define SUNXI_UART3_BASE               0x42500C00
#define SUNXI_RTWI_BASE		       0x42502000
#define SUNXI_TWI0_BASE                0x42502000
#define SUNXI_TWI1_BASE                0x42502400
#define SUNXI_TWI2_BASE                0x42502800
#define SUNXI_SYSCRL_BASE             0x43000000
#define SUNXI_DMA_BASE               0x43001000
#define SUNXI_DMAC0__SGDMA__BASE       0x43002000
#define SUNXI_SPINLOCK_BASE            0x43005000
#define SUNXI_SID_BASE                ((ulong)0x43006000)
#define SUNXI_HSTIMER_BASE             0x43008000
#define SUNXI_DCU_BASE                 0x43010000
#define SUNXI_E907_CFG_BASE            0x43030000
#define SUNXI_E907_WDG_BASE            0x43031000
#define SUNXI_E907_TIMESTAMP_BASE      0x43032000
#define SUNXI_E907_MSGBOX_BASE         0x43033000
#define SUNXI_WLAN_CTRL_BASE           0x43043000
#define SUNXI_CE_BASE                  ((ulong)0x43044000)
#define SUNXI_PWR_REQ_BASE             0x43045000
#define SUNXI_MSI_MEMC_BASE            0x43102000
#define SUNXI_A27L2_CFG_BASE           0x49100000
#define SUNXI_A27L2_WDG_BASE           0x49101000
#define SUNXI_A27L2_MSGBOX_BASE        0x49102000
#define SUNXI_SMHC0_BASE		0x44020000
#define SUNXI_SMHC1_BASE               ((ulong)0x44021000)
#define SUNXI_SMC_BASE		       ((ulong)0x44021000)
#define SUNXI_SPI0_BASE                0x44025000
#define SUNXI_SPI1_BASE                0x44026000
#define SUNXI_USB0_BASE                0x44100000
#define SUNXI_GMAC_BASE                0x44500000
#define SUNXI_SPI_FLASH_BASE           0x44F00000
#define SUNXI_DE_BASE                  0x45000000
#define SUNXI_G2D_BASE                 0x45410000
#define SUNXI_DISPLAY_TOP_BASE         0x45460000
#define SUNXI_TCON_LCD0_BASE           0x45461000
#define SUNXI_CSI_BASE                 0x45800000
#define SUNXI_ISP_BASE                 0x45900000
#define SUNXI_RPRCM_BASE                0x4A000000
#define SUNXI_RTC_TIMER_BASE           0x4A000400
#define SUNXI_PMU_RTC_BASE             0x4A000800
#define SUNXI_RTC_BASE              0x4A000C00
#define SUNXI_RTC_WDG_BASE             0x4A001000
#define SUNXI_CCM_AON_BASE             0x4A010000
#define SUNXI_CCM_APP_BASE             0x42001000
#define SUNXI_CCM_BASE			0x4A010000
#define SUNXI_PMU_AON_BASE             0x4A011000
#define SUNXI_RCCAL_BASE               0x4A011400
#define SUNXI_WIFI_SRAM_BASE           0x68000000
#define SUNXI_WIFI_PERI_BASE           0x79C00000
#define SUNXI_SPI_FLASH_XIP_BASE       0x10000000
#define SUNXI_DRAM_SPACE_BASE          0x80000000
#define SUNXI_RISCV_CLINT_BASE         0xE0000000
#define SUNXI_RISCV_CLIC_BASE          0xE0800000
#define SUNXI_RESERVE_BASE             0xE0805000
#define SUNXI_PLIC_BASE                0x48000000
#define SUNXI_PLMT_BASE                0x48400000
#define SUNXI_PLIC_SW_BASE             0x48800000
#define SUNXI_PLDM_BASE                0x48C00000
#define SUNXI_L2C_BASE                 0x49000000
#define	SUNXI_R_PIO_BASE	       0x00000000

#define SUNXI_KEYADC_BASE                 SUNXI_LRADC_BASE

/*cpus*/
#define SUNXI_RSB_BASE					SUNXI_RRSB_BASE
#define SUNXI_RTWI_BRG_REG				(SUNXI_RPRCM_BASE + 0x019c)
#define SUNXI_RTWI0_RST_BIT				(16)
#define SUNXI_RTWI0_GATING_BIT			(0)
#define SUNXI_RST_BIT				(16)
#define SUNXI_GATING_BIT			(0)


#define SUNXI_RTC_DATA_BASE                 (SUNXI_RTC_BASE + 0x100)
#define AUDIO_CODEC_BIAS_REG				(SUNXI_AUDIO_CODEC + 0x320)
#define AUDIO_POWER_REG						(SUNXI_AUDIO_CODEC + 0x348)
#define SYNXI_VER_REG						(SUNXI_SYSCRL_BASE + 0x24)


/* use for usb correct */
#define VDD_SYS_PWROFF_GATING_REG			(SUNXI_RPRCM_BASE + 0x250)
#define RES_CAL_CTRL_REG                    (SUNXI_RPRCM_BASE + 0X310)
#define ANA_PWR_RST_REG                     (SUNXI_RPRCM_BASE + 0X254)

#define VDD_ADDA_OFF_GATING					(9)
#define CAL_ANA_EN							(1)
#define CAL_EN								(0)

#define RVBARADDR0_L						(SUNXI_CPUXCFG_BASE + 0x40)
#define RVBARADDR0_H						(SUNXI_CPUXCFG_BASE + 0x44)

#define SRAM_CONTRL_REG0					(SUNXI_SYSCRL_BASE + 0x0)
#define SRAM_CONTRL_REG1					(SUNXI_SYSCRL_BASE + 0x4)

#define GPIO_BIAS_MAX_LEN (32)
#define GPIO_BIAS_MAIN_NAME "gpio_bias"
#define GPIO_POW_MODE_REG (0x0340)
#define GPIO_POW_MODE_VAL_REG		(0x0348)
#define GPIO_3_3V_MODE 0
#define GPIO_1_8V_MODE 1


#define PIOC_REG_o_POW_MOD_SEL   0x340
#define PIOC_REG_o_POW_MS_CTL   0x344
#define PIOC_REG_o_POW_MS_VAL   0x34

#define PIOC_REG_POW_MOD_SEL  (SUNXI_PIO_BASE + PIOC_REG_o_POW_MOD_SEL)
#define PIOC_REG_POW_MS_CTL   (SUNXI_PIO_BASE + PIOC_REG_o_POW_MS_CTL)
#define PIOC_REG_POW_VAL   (SUNXI_PIO_BASE + PIOC_REG_o_POW_MS_VAL)

#define PIOC_SEL_Px_3_3V_VOL 0
#define PIOC_SEL_Px_1_8V_VOL 1

#define PIOC_CTL_Px_ENABLE 0
#define PIOC_CTL_Px_DISABLE 1

#define PIOC_VAL_Px_3_3V_VOL 0
#define PIOC_VAL_Px_1_8V_VOL 1

#define PIOC_CTL_Px_DEFUALT PIOC_CTL_Px_ENABLE
#define PIOC_SEL_Px_DEFAULT PIOC_SEL_Px_1_8V_VOL

#endif /*  _CPU_SUN20IW5_H*/
