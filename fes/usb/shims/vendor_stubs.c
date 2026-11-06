/*
 * Copyright (c) 2026, edk2-a733 contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
/*
 * vendor_stubs.c - link-time stubs for the vendor sdcard library.
 *
 * libsun60iw2p1_sdcard.a is a single object file: linking it for
 * sunxi_mmc_init()/mmc_init()/find_mmc_device() also drags in its SPI
 * flash, GPADC and FDT paths.  Those functions are never called from the
 * FES code path (eMMC only), but their references must resolve.
 *
 * Keep the stubs honest: -1 / NULL means "not available here", so an
 * accidental call fails visibly instead of pretending to succeed.
 */
int fdt_check_header(void *fdt)
{
	(void)fdt;
	return -1;
}

const void *fdt_getprop(const void *fdt, int node, const char *name, int *lenp)
{
	(void)fdt; (void)node; (void)name;
	if (lenp)
		*lenp = 0;
	return 0;
}

int fdt_set_node_status(void *fdt, int node, int status)
{
	(void)fdt; (void)node; (void)status;
	return -1;
}

const char *fdt_strerror(int err)
{
	(void)err;
	return "fdt-unavailable";
}

int spi_xfer(void *bus, unsigned int bitlen, const void *dout, void *din,
	     unsigned long flags)
{
	(void)bus; (void)bitlen; (void)dout; (void)din; (void)flags;
	return -1;
}

int sunxi_read_gpadc_vol(unsigned int channel)
{
	(void)channel;
	return 0;
}

/*
 * FES command handlers that the FES path does not use: our host tool
 * sends down/up only.  Present so the pristine vendor usb_efex.c links.
 */
int sunxi_board_flash_probe(void)
{
	return 0;
}

int get_board_flash_type(void)
{
	return 0x12;	/* eMMC, same value as get_boot_storage_type() */
}

int sunxi_flash_sprite_switch(int flash_type, int work_mode)
{
	(void)flash_type; (void)work_mode;
	return -1;
}
