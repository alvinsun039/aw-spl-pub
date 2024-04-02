
#include <common.h>
#include "private_opensbi.h"

void boot0_jmp(phys_addr_t addr)
{
	asm volatile("jr a0");
}

void boot0_jmp_optee(phys_addr_t optee, phys_addr_t uboot)
{
	phys_addr_t dtb_entry = uboot + 2 * 1024 * 1024;

	asm volatile("mv s1, %0" : : "r"(optee) : "memory");
	asm volatile("mv a0, %0" : : "r"(dtb_entry) : "memory");
	asm volatile("mv ra, %0" : : "r"(uboot) : "memory");
	asm volatile("jr     s1");
}

void boot0_jmp_monitor(phys_addr_t monitor_base)
{
#ifdef CONFIG_MONITOR
	asm volatile("jr a0");
__LOOP:
	asm volatile("WFI");
	goto __LOOP;
#endif
}

void boot0_jmp_opensbi(phys_addr_t opensbi_base, phys_addr_t uboot_base)
{
	asm volatile("jr a0");
__LOOP:
	asm volatile("WFI");
	goto __LOOP;
}

void boot_jmp_opensbi(ulong opensbi_addr, u32 core_id, ulong dtb_addr)
{
	asm volatile("mv s1, %0" ::"r"(opensbi_addr) : "memory");
	asm volatile("mv a0, %0" ::"r"(core_id) : "memory");
	asm volatile("mv a1, %0" ::"r"(dtb_addr) : "memory");
	asm volatile("jr s1");
}

static void __jump_kernel(u32 opensbi_entry, u32 dtb_base, u32 kernel_addr)
{
	if (opensbi_entry) {
		printf("opensbi to Linux (%x)...,dtb (%x)\n",
		       (unsigned int)kernel_addr, (unsigned int)dtb_base);
		asm volatile("mv s1, %0" ::"r"(opensbi_entry) : "memory");
		asm volatile("mv a0, %0" ::"r"(0) : "memory");
		asm volatile("mv a1, %0" ::"r"(dtb_base) : "memory");
		asm volatile("mv ra, %0" ::"r"(kernel_addr) : "memory");
		asm volatile("jr s1");
	} else {
		asm volatile("mv s1, %0" ::"r"(kernel_addr) : "memory");
		asm volatile("mv a1, %0" ::"r"(dtb_base) : "memory");
		asm volatile("jr     s1");
	}
}

void boot0_jump_kernel(u32 dtb_base, u32 kernel_addr)
{
	__jump_kernel(0, dtb_base, kernel_addr);
}

void optee_jump_kernel(u32 opensbi_entry, u32 dtb_base, u32 kernel_addr)
{
	__jump_kernel(opensbi_entry, dtb_base, kernel_addr);
}

