/*
 * (C) Copyright 2023
 *
 * The executable bin for the first running CPU of boot0.
 * 1. The source code of this file is brom/boot0_e907 in product/XRadio/brom
 * 2. Each element in the array is an E907 instruction
 *
 * The booting procedure:
 *   1. The first running CPU jump form BROM to &e907_boot_bin[0].
 *   2. The first running CPU start up the second running CPU which booting
 *      from &e907_boot_bin[E907_BOOT_BIN_SIZE] (the end of e907_boot_bin[]).
 *   3. The SRAM layout: | boot0_head || e907_boot_bin || boot0_2nd_bin |
 */

/* The executable bin size of the first running CPU of boot0. */
#define E907_BOOT_BIN_SIZE	40
unsigned int e907_boot_bin[E907_BOOT_BIN_SIZE] = {
	0x00000597, 0x0A058593, 0x46214601, 0x30063073,
	0x42001737, 0x08070793, 0x05374F94, 0xE6931800,
	0xCF940046, 0x8EC94BD4, 0x5F7CCBD4, 0x0C07E793,
	0x07B7DF7C, 0xA2234910, 0x173720B7, 0x537C4A01,
	0x1007E793, 0x07B7D37C, 0x07374A01, 0xA4238000,
	0x660558E7, 0x03F60613, 0x7C163073, 0x49100737,
	0x5807A223, 0x20472783, 0x1737C785, 0x27834200,
	0x06B70947, 0x8FD50400, 0x08F72A23, 0x47B14781,
	0x7E17B7F3, 0xA7F34791, 0x00737E17, 0xA0011050,
	0xBFC90001, 0x00000000, 0x00000000, 0x00000000,
};
