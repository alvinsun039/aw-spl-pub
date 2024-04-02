#include <config.h>
#include <common.h>
#include <private_boot0.h>
#include <private_uboot.h>
#include <private_toc.h>
#include <board_helper.h>
#ifdef CFG_SUNXI_GPT
#include <part_efi.h>
#endif

#ifndef CFG_SUNXI_ENV
/*
 * CFG_BOOT0_LOAD_KERNEL=y
 * CFG_KERNEL_BOOTIMAGE=y
 * CFG_KERNEL_CHECKSUM=n #y will check kernel checksum in bimage, but slower
 * CFG_SPINOR_KERNEL_OFFSET=0x20 #first partition, 0x20
 * CFG_SPINOR_LOGICAL_OFFSET=2016
 * CFG_KERNEL_LOAD_ADDR=0x41008000
 * CFG_SUNXI_FDT_ADDR=0x41800000
 */

#ifdef CFG_SUNXI_SPINOR
#if !defined(CFG_SPINOR_LOGICAL_OFFSET) ||                                     \
	!defined(CFG_SPINOR_KERNEL_OFFSET) ||                                  \
	!defined(CFG_KERNEL_LOAD_ADDR) || !defined(CFG_SUNXI_FDT_ADDR)
#error CFG_KERNEL_LOAD_ADDR CFG_SPINOR_LOGICAL_OFFSET CFG_SPINOR_KERNEL_OFFSET CFG_SUNXI_FDT_ADDR \
required for boot kernel function !

#endif
#define LOGICAL_OFFSET    CFG_SPINOR_LOGICAL_OFFSET
#define KERNEL_OFFSET     CFG_SPINOR_KERNEL_OFFSET
#endif /* CFG_SUNXI_SPINOR */

#ifdef CFG_SUNXI_SDMMC
#if !defined(CFG_MMC_KERNEL_OFFSET) ||                                     \
	!defined(CFG_MMC_LOGICAL_OFFSET) ||                                  \
	!defined(CFG_KERNEL_LOAD_ADDR) || !defined(CFG_SUNXI_FDT_ADDR)
#error CFG_KERNEL_LOAD_ADDR CFG_MMC_KERNEL_OFFSET CFG_MMC_LOGICAL_OFFSET CFG_SUNXI_FDT_ADDR \
required for boot kernel function !
#endif

#define LOGICAL_OFFSET    CFG_MMC_LOGICAL_OFFSET
#define KERNEL_OFFSET     CFG_MMC_KERNEL_OFFSET

int sunxi_mmc_exit(int sdc_no, const normal_gpio_cfg *gpio_info, int offset);
int get_card_num(void);
#endif /* CFG_SUNXI_SDMMC */

#ifdef CFG_SUNXI_SPINAND

extern int spinand_get_boot_start(int copy);
#define LOGICAL_OFFSET    0
#define KERNEL_OFFSET     spinand_get_boot_start(0)
#define KERNEL_BACKUP_OFFSET     spinand_get_boot_start(1)

#endif /* CFG_SUNXI_SPINAND */

#if !defined(CFG_SUNXI_SPINOR) &&                                     \
	!defined(CFG_SUNXI_SDMMC) && \
	!defined(CFG_SUNXI_SPINAND)
#error Only supports mmc and nor and spinand
#endif

/* Failed to start or read the flag
in the flash to enter the system backup*/
int load_kernel_from_flash(u32 *kernel_addr,
			   int (*flash_read)(uint, uint, void *))
{
	int ret = 0;
#ifdef CFG_KERNEL_BOOTIMAGE
	u32 kernel_start = KERNEL_OFFSET + LOGICAL_OFFSET;
#ifdef CFG_SPINOR_KERNEL_BACKUP_OFFSET
	u32 kernel_backup =
			CFG_SPINOR_KERNEL_BACKUP_OFFSET + LOGICAL_OFFSET;
	if (!get_switch_kernel_flag()) {
		printf("use kernel backup\n");
		ret = load_bimage(kernel_backup, CFG_KERNEL_LOAD_ADDR,
				  flash_read, kernel_addr);
		if (ret < 0) {
			printf("bootimage backup0 erro\n");
		}
	} else {
#endif /*CFG_SPINOR_KERNEL_BACKUP_OFFSET*/
		ret = load_bimage(kernel_start, CFG_KERNEL_LOAD_ADDR,
				  flash_read, kernel_addr);
		if (ret < 0) {
			printf("load bootimage erro\n");
//CFG_SPINOR_KERNEL_BACKUP_OFFSET=0x37A0
#ifdef CFG_SPINOR_KERNEL_BACKUP_OFFSET
			ret = load_bimage(kernel_backup, CFG_KERNEL_LOAD_ADDR,
					  flash_read, kernel_addr);
			if (ret < 0) {
				printf("bootimage backup0 erro\n");
			}

		}
#endif /*CFG_SPINOR_KERNEL_BACKUP_OFFSET*/

#ifdef CFG_SUNXI_SPINAND
		u32 kernel_backup = KERNEL_BACKUP_OFFSET;
		ret = load_bimage(kernel_backup, CFG_KERNEL_LOAD_ADDR,
				  flash_read, kernel_addr);
		if (ret < 0) {
			printf("bootimage backup0 erro\n");
		}
#endif /* CFG_SUNXI_SPINAND */
	}
#endif

//CFG_KERNEL_UIMAGE=y
#ifdef CFG_KERNEL_UIMAGE
#define UIMAGE_SIZE_KB (2 * 1024)
	ret = load_uimage(KERNEL_OFFSET + LOGICAL_OFFSET,
			  CFG_KERNEL_LOAD_ADDR, UIMAGE_SIZE_KB, spinor_read);
	if (ret < 0) {
		printf("load uimage erro\n");
	}
#endif

#ifdef CFG_SUNXI_SPINAND
extern __s32 SpiNand_PhyExit(void);
	SpiNand_PhyExit();
#endif

	return ret;
}
#else
/*
 * CFG_BOOT0_LOAD_KERNEL=y
 * CFG_KERNEL_BOOTIMAGE=y
 * CFG_KERNEL_CHECKSUM=n #y will check kernel checksum in bimage, but slower
 * CFG_SUNXI_GPT=y
 * CFG_MMC_GPT_ARD=0 # 0 or 40960 sector
 * CFG_SUNXI_ENV=y
 * CFG_SUNXI_ENV_SIZE=0x20000 #linux should be the same with LICHEE_REDUNDANT_ENV_SIZE
 * CFG_SUNXI_HAVE_REDUNDENV=y #if have env and env-redund in partition
 * CFG_KERNEL_LOAD_ADDR=0x41008000
 * CFG_SUNXI_FDT_ADDR=0x41800000
 */

#ifdef CFG_SUNXI_SDMMC
int sunxi_mmc_exit(int sdc_no, const normal_gpio_cfg *gpio_info, int offset);
int get_card_num(void);
#endif /* CFG_SUNXI_SDMMC */

int load_kernel_from_flash(u32 *kernel_addr,
			   int (*flash_read)(uint, uint, void *))
{
	int ret = -1;
	unsigned int kernel_start = 0;
	char *kernel_name = NULL;

	if (init_gpt()) {
		pr_emerg("init gpt fail\n");
		return ret;
	}

	if (fw_env_open() == -1) {
		pr_emerg("open env error\n");
		return ret;
	}

	kernel_name = fw_getenv("boot_partition");
	if (kernel_name == NULL) {
		pr_emerg("no boot_partition in env\n");
		goto load_out;
	}

	if (get_part_info_by_name(kernel_name, &kernel_start, NULL)) {
		goto load_out;
	}

	//must close env first, because load kernel will destroy heap:CONFIG_HEAP_BASE
	fw_env_close();
	ret = load_bimage(kernel_start, CFG_KERNEL_LOAD_ADDR, flash_read, kernel_addr);
	if (ret < 0) {
		pr_emerg("load kernel erro\n");
	}

load_out:
	return ret;
}
#endif //CFG_SUNXI_ENV


void load_and_run_kernel(u32 optee_base, u32 opensbi_base, u32 monitor_base)
{
	int load_flash_success = -1;
	u32 kernel_addr;
	u32 kernel_runner_base;
	int (*flash_read)(uint, uint, void *);
#ifdef CFG_SUNXI_SDMMC
	flash_read = mmc_bread_ext;
#elif CFG_SUNXI_SPINOR
	flash_read = spinor_read;
#elif CFG_SUNXI_SPINAND
extern int spinand_read_kernel(uint sector_num, uint N, void *buffer);
	flash_read = spinand_read_kernel;
#else
	return 0;
#endif

	load_flash_success = load_kernel_from_flash(&kernel_addr, flash_read);
	if (load_flash_success)
		return;

#ifdef CFG_SUNXI_FDT
#ifndef CFG_SUNXI_NO_UPDATE_FDT_PARA
	sunxi_update_fdt_para_for_kernel();
#endif
#endif
	mmu_disable();

#ifdef CFG_ARCH_RISCV
	kernel_runner_base = opensbi_base;
#else
	kernel_runner_base = optee_base;
#endif

#ifdef CFG_SUNXI_SDMMC
	sunxi_mmc_exit(get_card_num(), BT0_head.prvt_head.storage_gpio, 16);
#endif

	if (monitor_base) {
		struct spare_monitor_head *monitor_head =
			(struct spare_monitor_head *)((phys_addr_t)monitor_base);
		monitor_head->secureos_base = optee_base;
		monitor_head->nos_base = kernel_addr; //kernel_base
		boot0_jmp_monitor(monitor_base);
	} else if (kernel_runner_base) {
		optee_jump_kernel(kernel_runner_base, CFG_SUNXI_FDT_ADDR,
				  kernel_addr);
	} else {
		boot0_jump_kernel(CFG_SUNXI_FDT_ADDR, kernel_addr);
	}
}

