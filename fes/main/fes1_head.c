/*
 * (C) Copyright 2018
 * wangwei <wangwei@allwinnertech.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#include <private_boot0.h>
#include <commit_info.h>

#ifdef CFG_ARCH_RISCV
#define BROM_FILE_HEAD_SIZE		(sizeof(boot0_file_head_t) & 0x00FFFFF)
#define BROM_FILE_HEAD_BIT_10_1		((BROM_FILE_HEAD_SIZE & 0x7FE) >> 1)
#define BROM_FILE_HEAD_BIT_11		((BROM_FILE_HEAD_SIZE & 0x800) >> 11)
#define BROM_FILE_HEAD_BIT_19_12	((BROM_FILE_HEAD_SIZE & 0xFF000) >> 12)
#define BROM_FILE_HEAD_BIT_20		((BROM_FILE_HEAD_SIZE & 0x100000) >> 20)

#define BROM_FILE_HEAD_SIZE_OFFSET	((BROM_FILE_HEAD_BIT_20 << 31) | \
									(BROM_FILE_HEAD_BIT_10_1 << 21) | \
									(BROM_FILE_HEAD_BIT_11 << 20) | \
									(BROM_FILE_HEAD_BIT_19_12 << 12))
#define JUMP_INSTRUCTION		(BROM_FILE_HEAD_SIZE_OFFSET | 0x6f)
#else
#define BROM_FILE_HEAD_SIZE_OFFSET	(((sizeof(boot0_file_head_t) + sizeof(int) - 1) / sizeof(int) - 2))
#define JUMP_INSTRUCTION		(BROM_FILE_HEAD_SIZE_OFFSET | 0xEA000000)
#endif

const boot0_file_head_t  fes1_head = {
	{
		/* jump_instruction */          
		JUMP_INSTRUCTION,
		BOOT0_MAGIC,
		STAMP_VALUE,
		32,
		sizeof(boot_file_head_t),
		BOOT_PUB_HEAD_VERSION,
		(CONFIG_FES1_RUN_ADDR + sizeof(boot0_file_head_t) - sizeof(fes1_head.fes_union_addr)),
		CONFIG_FES1_RUN_ADDR,
		0,
		{
			0, 0, '3','.','0','.','0',0
		},
	},
	{
		//__u32 prvt_head_size;
		0,
		//char prvt_head_vsn[4];      
		0,
		0,	/*power_mode*/
		{0},/* reserver[2] */
		//unsigned int                dram_para[32] ; 
		{	/* LPDDR5 8G (cxmt) - from SDK cubie_a7a sys_config [dram_para2] */
			1800, 9, 0x0e0e0e0e, 0x0f0f0f0f,
			0xec030e0f, 0, 0xa10a, 0x1001,
			0, 0, 0, 6,
			0, 0, 0, 0x1b,
			0x44, 0, 0x1d, 0,
			0x86, 0, 0x4040, 0,
			0x11080503, 0x3800, 0x3514, 0xb25f0000,
			0, 0, 0x65, 0,
		},
		//__s32			     uart_port;   
		0,
		//normal_gpio_cfg       uart_ctrl[2];  
		{
			{ 2, 8, 4, 1, 1, 0, {0}},//PB8: 4--RX
			{ 2, 9, 4, 1, 1, 0, {0}},//PB9: 4--TX
		},
		//__s32                         enable_jtag;  
		0,
		//normal_gpio_cfg	      jtag_gpio[5];   
		{{0},{0},{0},{0},{0}},
		//normal_gpio_cfg        storage_gpio[32]; 
		{
			{6, 0, 2, 1, 2, 0, {0} },//PF0-5: 2--SDC
			{6, 1, 2, 1, 2, 0, {0} },
			{6, 2, 2, 1, 2, 0, {0} },
			{6, 3, 2, 1, 2, 0, {0} },
			{6, 4, 2, 1, 2, 0, {0} },
			{6, 5, 2, 1, 2, 0, {0} },
		},
		//char                             storage_data[512 - sizeof(normal_gpio_cfg) * 32]; 
		{0}
	 },
	.hash = CI_INFO,
#ifdef CFG_SUNXI_SELECT_DRAM_PARA
#ifdef CFG_DRAM_SELECT_VERSION_2_0
	.fes_union_addr.extd_head.version = "dram2.0",
#else
	.fes_union_addr.extd_head.version = "dram1.0",
#endif
	.fes_union_addr.extd_head.magic = DRAM_EXT_MAGIC,
#endif
};



/*******************************************************************************
*
*                  The jump_instruction field in Boot_file_head
*
*  The field holds a branch instruction (B BACK_OF_Boot_file_head);
*  executing it jumps to the first instruction after Boot_file_head.
*
*  The ARM B instruction is encoded as:
*          +--------+---------+------------------------------+
*          | 31--28 | 27--24  |            23--0             |
*          +--------+---------+------------------------------+
*          |  cond  | 1 0 1 0 |        signed_immed_24       |
*          +--------+---------+------------------------------+
*  The ARM Architecture Reference Manual describes it as follows:
*  Syntax :
*  B{<cond>}  <target_address>
*    <cond>    Is the condition under which the instruction is executed. If the
*              <cond> is ommitted, the AL(always,its code is 0b1110 )is used.
*    <target_address>
*              Specified the address to branch to. The branch target address is
*              calculated by:
*              1.  Sign-extending the 24-bit signed(wro's complement)immediate
*                  to 32 bits.
*              2.  Shifting the result left two bits.
*              3.  Adding to the contents of the PC, which contains the address
*                  of the branch instruction plus 8.
*
*  The top 8 bits are therefore 0b11101010 and the low 24 bits are
* generated from the size of Boot_file_head, assembled as follows:
*  ( sizeof( boot_file_head_t ) + sizeof( int ) - 1 ) / sizeof( int )
*                                              words used by the file header
*  - 2                                         minus the PC prefetch words
*  & 0x00FFFFFF                                keep signed_immed_24
*  | 0xEA000000                                form the B instruction
*
*******************************************************************************/
