/*
 * (C) Copyright 2013-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 */
#ifndef _SUNXI_MMC_BOOT0_H
#define _SUNXI_MMC_BOOT0_H

#define CARD_TYPE_MMC	0x8000000
#define CARD_TYPE_SD	0x8000001

#define MMC_EARLY_INIT 1
#define MMC_NOT_INIT   0

enum {
	E_SDMMC_OK	       = 0,
	E_SDMMC_NUM_ERR	       = 1,
	E_SDMMC_INIT_ERR       = 2,
	E_SDMMC_READ_ERR       = 3,
	E_SDMMC_FIND_BOOT1_ERR = 4,
};

void set_mmc_para(int smc_no, void *sdly_addr, phys_addr_t uboot_base);
int get_card_type(void);
unsigned long mmc_bread(int dev_num, unsigned long start, unsigned blkcnt, void *dst);
int sunxi_mmc_init(int sdc_no, unsigned bus_width, const normal_gpio_cfg *gpio_info, int offset);
int sunxi_mmc_exit(int sdc_no, const normal_gpio_cfg *gpio_info, int offset);

#endif /* _SUNXI_MMC_BOOT0_H */
