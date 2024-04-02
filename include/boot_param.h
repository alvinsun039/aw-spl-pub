/*
 * (C) Copyright 2007-2012
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom wangwei <wangwei@allwinnertech.com>
 */

#ifndef __BOOT_PARAM_H
#define __BOOT_PARAM_H
#include <common.h>
#define BOOT_PARAM_MAGIC		"bootpara"

/* bit map
 * transfer_flag:bit0
 * 1:uboot download bootparam  0: nothing */

#define BOOTPARAM_DOWNLOAD_MASK  (0x1)
struct sunxi_boot_parameter_header {
	u8 magic[8]; //bootpara
	u32 version; // describe the region version
	u32 check_sum;
	u32 length;
	u32 boot0_checksum;
	u32 transfer_flag;
	u8 reserved[4];
};


#define MAX_DRAMPARA_SIZE (96)
typedef struct {
	unsigned int dram_para[MAX_DRAMPARA_SIZE];
	char res[512 - 4 * MAX_DRAMPARA_SIZE];
} boot_dram_info_t;

// BOOT_ PARAM_ SIZE maximum value is 4k
#define BOOT_PARAM_SIZE (2048)
typedef struct sunxi_boot_param_region{
	struct sunxi_boot_parameter_header header;
	char sdmmc_info[256 -32];
	char nand_info[256];
	char spiflash_info[256];
	char ufs[256];
	char ddr_info[512];
	u8 reserved[BOOT_PARAM_SIZE - 512*3];// = 2048 - 32 - sdmmc_size - nand_size - spi_size - ddr_size
} typedef_sunxi_boot_param;


int sunxi_bootparam_load(void);

boot_dram_info_t *sunxi_bootparam_get_dram_buf(void);
typedef_sunxi_boot_param *sunxi_bootparam_get_buf(void);
int sunxi_bootparam_format_and_transfer(void *dest);
#endif
