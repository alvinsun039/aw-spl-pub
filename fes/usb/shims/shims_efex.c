/*
 * Copyright (c) 2026, edk2-a733 contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
/*
 * shims_efex.c - minimal support layer that lets the vendor sunxi USB
 * stack run in the FES1 context.
 *
 * Goal: link and run the twelve vendor objects (controller, base, efex,
 * burn, queues) plus gic_v3.o without U-Boot:
 *   - libc, heap, cache, time, interrupts: real implementations, weak so
 *     a strong fes1 symbol wins;
 *   - board/mode/environment: deterministic defaults;
 *   - security/efuse/SMC: stubs (FES1 has no TOS).
 *   - Flash/Sprite: DRAM fallback until the media backend attaches
 *
 */
#include <common.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/usb.h>
#include <sunxi_flash.h>
#include <linux/libfdt.h>
#include <sprite_download.h>
#include "fes_printf.h"

/* ============ 1. Basic libc (weak) ============ */

__attribute__((weak)) void *memcpy(void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	while (n--)
		*d++ = *s++;
	return dst;
}

__attribute__((weak)) void *memset(void *dst, int c, size_t n)
{
	unsigned char *d = dst;
	while (n--)
		*d++ = (unsigned char)c;
	return dst;
}

__attribute__((weak)) int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b) { a++; b++; }
	return (unsigned char)*a - (unsigned char)*b;
}

__attribute__((weak)) char *strcpy(char *d, const char *s)
{
	char *r = d;
	while ((*d++ = *s++))
		;
	return r;
}

__attribute__((weak)) char *strncpy(char *d, const char *s, size_t n)
{
	char *r = d;
	while (n-- && (*d++ = *s++))
		;
	while (n-- > 0)  /* pad */
		*d++ = 0;
	return r;
}

__attribute__((weak)) size_t strlen(const char *s)
{
	const char *p = s;
	while (*p)
		p++;
	return (size_t)(p - s);
}

/* ---- Minimal printf family: UART0 at 0x02500000, LSR+0x14 THRE ---- */

static void uart0_put_raw(char c)
{
	volatile unsigned int *lsr = (volatile unsigned int *)(0x02500000 + 0x14);
	volatile unsigned int *thr = (volatile unsigned int *)(0x02500000 + 0x00);
	unsigned int t = 0;
	while (!(*lsr & (1 << 5))) {
		if (++t > 1000000)
			break;
	}
	*thr = c;
}

/*
 * The vendor printf family emits "\n" without "\r" (its U-Boot serial driver
 * used to turn the LF into CRLF).  This shim drives the UART directly, so the
 * terminal would staircase every line; add the CR here, once, and leave an
 * existing "\r\n" alone.
 */
static void uart0_putc(char c)
{
	static int last_was_cr;

	if (c == '\n' && !last_was_cr)
		uart0_put_raw('\r');
	uart0_put_raw(c);
	last_was_cr = (c == '\r');
}


static void __vfmt(const char *fmt, va_list ap)
{
	char buf[32];

	for (; *fmt; fmt++) {
		if (*fmt != '%') {
			uart0_putc(*fmt);
			continue;
		}
		fmt++;
		switch (*fmt) {
		case 's': {
			const char *s = va_arg(ap, const char *);
			if (!s)
				s = "(null)";
			while (*s)
				uart0_putc(*s++);
			break;
		}
		case 'c':
			uart0_putc((char)va_arg(ap, int));
			break;
		case 'd': {
			int v = va_arg(ap, int);
			char *p = buf + sizeof(buf);
			int neg = 0;
			if (v < 0) { neg = 1; v = -v; }
			*--p = 0;
			do { *--p = '0' + (v % 10); v /= 10; } while (v);
			if (neg)
				*--p = '-';
			while (*p)
				uart0_putc(*p++);
			break;
		}
		case 'u': {
			unsigned int v = va_arg(ap, unsigned int);
			char *p = buf + sizeof(buf);
			*--p = 0;
			do { *--p = '0' + (v % 10); v /= 10; } while (v);
			while (*p)
				uart0_putc(*p++);
			break;
		}
		case 'x': case 'X': {
			unsigned int v = va_arg(ap, unsigned int);
			char *p = buf + sizeof(buf);
			const char *hex = "0123456789abcdef";
			*--p = 0;
			do { *--p = hex[v & 0xf]; v >>= 4; } while (v);
			while (*p)
				uart0_putc(*p++);
			break;
		}
		case 'l':
			if (fmt[1] == 'x' || fmt[1] == 'u' || fmt[1] == 'd') {
				unsigned long v = va_arg(ap, unsigned long);
				char *p = buf + sizeof(buf);
				const char *hex = "0123456789abcdef";
				*--p = 0;
				if (fmt[1] == 'x') {
					do { *--p = hex[v & 0xf]; v >>= 4; } while (v);
				} else {
					do { *--p = '0' + (v % 10); v /= 10; } while (v);
				}
				while (*p)
					uart0_putc(*p++);
				fmt++;
			}
			break;
		default:
			uart0_putc('%');
			if (*fmt)
				uart0_putc(*fmt);
			break;
		}
	}
}

__attribute__((weak)) int printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	__vfmt(fmt, ap);
	va_end(ap);
	return 1;
}

__attribute__((weak)) int sprintf(char *buf, const char *fmt, ...)
{
	/* Rarely used; minimal %s-only implementation */
	va_list ap;
	va_start(ap, fmt);
	if (fmt[0] == '%' && fmt[1] == 's') {
		const char *s = va_arg(ap, const char *);
		while (*s)
			*buf++ = *s++;
	}
	va_end(ap);
	*buf = 0;
	return 1;
}

int tick_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	__vfmt(fmt, ap);
	va_end(ap);
	return 1;
}

/* ============ 2. Heap: static DRAM pool, bump allocation ============ */

/* The heap lives in DRAM, which FES owns after training; the SRAM
 * window stays reserved for the fes1 image.
 * Sizing: efex queue 64K*20 = 1.25 MB, rx/send 2x4 MB, nodes: ~9.5 MB,
 * rounded up to 16 MB. */
#define EFEX_HEAP_BASE	0x48000000UL
#define EFEX_HEAP_SIZE	0x01000000	/* 16MB */
#define EFEX_HEAP_END	(EFEX_HEAP_BASE + EFEX_HEAP_SIZE)

static unsigned long heap_cur = EFEX_HEAP_BASE;

static void *__xalloc(unsigned long n, unsigned long align)
{
	unsigned long a = (heap_cur + align - 1) & ~(align - 1);
	if (a + n > EFEX_HEAP_END)
		return NULL;
	heap_cur = a + n;
	return (void *)a;
}

__attribute__((weak)) void *malloc(size_t n) { return __xalloc(n, 8); }
__attribute__((weak)) void free(void *p) { (void)p; }
__attribute__((weak)) void *memalign(size_t align, size_t n)
{
	static int once;
	if (!once) { once = 1; fes_dbg("[FES] memalign\n"); }
	if (align < 8)
		align = 8;
	return __xalloc(n, align);
}
void *malloc_align(size_t n, size_t align) { return memalign(align, n); }
void free_align(void *p) { free(p); }

/* ============ 3. Cache (ARMv7 MVA) ============ */

void flush_dcache_range(unsigned long start, unsigned long stop)
{
	unsigned long line = 32, addr;
	for (addr = start & ~(line - 1); addr < stop; addr += line)
		asm volatile("mcr p15, 0, %0, c7, c14, 1" : : "r"(addr)); /* DCCMVAC */
	asm volatile("mcr p15, 0, %0, c7, c10, 4" : : "r"(0)); /* DSB */
}

void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
	unsigned long line = 32, addr;
	for (addr = start & ~(line - 1); addr < stop; addr += line)
		asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r"(addr)); /* DCIMVAC */
	asm volatile("mcr p15, 0, %0, c7, c10, 4" : : "r"(0)); /* DSB */
}

void flush_cache(unsigned long start, unsigned long size)
{
	flush_dcache_range(start, start + size);
}

/* ============ 4. Interrupts (CPSR + GIC glue) ============ */

int disable_interrupts(void)
{
	unsigned int c;
	asm volatile("mrs %0, cpsr; orr %0, %0, #0x80; msr cpsr_c, %0" : "=r"(c) : : "memory");
	return 1;
}

void enable_interrupts(void)
{
	unsigned int c;
	asm volatile("mrs %0, cpsr; bic %0, %0, #0x80; msr cpsr_c, %0" : "=r"(c) : : "memory");
	(void)0;
}

int interrupts_is_open(void)
{
	unsigned int c;
	asm volatile("mrs %0, cpsr" : "=r"(c));
	return (c & 0x80) ? 0 : 1;
}

/*
 * GIC-600 clock: the BROM FEL path does not enable it, so the first
 * access in gic_distributor_init() data-aborts (the FES image dies right
 * after "gic: normal mode").  Same programming as the vendor
 * clock_sun60iw2.c: APB/GIC_CLK_REG @ CCU+0x560, source 3, divider 0.
 */
void clock_set_gic(void)
{
	unsigned int v = readl(SUNXI_CCM_BASE + 0x560);

	v &= ~((1u << 31) | (7u << 24) | (0x1fu << 0));
	v |=  (1u << 31) | (3u << 24) | (0u << 0);
	writel(v, SUNXI_CCM_BASE + 0x560);
	udelay(20);
}

/* ============ 5. Time (CNTPCT) ============ */

static unsigned long long cntpct(void)
{
	unsigned long long t;
	asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r"(t)); /* CNTPCT */
	return t;
}

static unsigned long cntfrq(void)
{
	unsigned long f;
	asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(f)); /* CNTFRQ */
	if (!f)
		f = 24000000;
	return f;
}

unsigned long runtime_tick(void)
{
	return (unsigned long)(cntpct() / cntfrq() * 1000);
}

__attribute__((weak)) void udelay(unsigned long us)
{
	unsigned long long target = cntpct() + (unsigned long long)us * (cntfrq() / 1000000);
	unsigned long long t;
	do { asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r"(t)); } while (t < target);
}

/* ============ 6. Environment / misc ============ */

int ctrlc(void) { return 0; }
char *env_get(const char *name) { (void)name; return NULL; }
void sunxi_dump(void) {}

/* ============ 7. FDT: no device tree, deterministic returns ============ */

struct fdt_header *working_fdt = NULL;

int fdt_path_offset(const void *fdt, const char *path)
{
	(void)fdt; (void)path;
	return -1; /* FDT_ERR_NOTFOUND */
}

int fdt_getprop_u32(const void *fdt, int node, const char *name, unsigned int *out)
{
	(void)fdt; (void)node; (void)name; (void)out;
	return -1;
}

int fdt_setprop(void *fdt, int node, const char *name, const void *val, int len)
{
	(void)fdt; (void)node; (void)name; (void)val; (void)len;
	return 0;
}

int script_parser_fetch(char *node_path, char *prop_name, int value[], int def_val)
{
	(void)node_path; (void)prop_name; (void)value;
	return def_val;
}

/* ============ 8. Board mode ============ */

int get_boot_storage_type(void) { return 0x12; }	/* eMMC, as in boot0's header */
int get_boot_storage_type_ext(void) { return 0x12; }
int get_boot_work_mode(void) { return 0; }
int sunxi_board_run_fel(void) { return 0; }
char *sunxi_bootargs_load_key(void) { return NULL; }
int sunxi_get_securemode(void) { return 0; }
int sunxi_set_serial_num(char *sn) { (void)sn; return 0; }
int sunxi_probe_secure_monitor(void) { return 0; }
int sunxi_probe_secure_os(void) { return 0; }

/* ============ 9. Security / efuse (no TOS, stubs) ============ */

int arm_svc_efuse_read(void *key_buf, void *read_buf)
{
	(void)key_buf; (void)read_buf;
	return -1;
}
int arm_svc_efuse_write(void *key_buf)
{
	(void)key_buf;
	return -1;
}
int sunxi_secure_object_read(const char *item_name, char *buffer, int buffer_len, int *data_len)
{
	(void)item_name; (void)buffer; (void)buffer_len;
	if (data_len) *data_len = 0;
	return -1;
}
int sunxi_secure_object_set(const char *item_name, int encrypt, int replace, int a, int b, int c)
{
	(void)item_name; (void)encrypt; (void)replace; (void)a; (void)b; (void)c;
	return -1;
}
int sunxi_secure_object_write(const char *item_name, char *buffer, int length)
{
	(void)item_name; (void)buffer; (void)length;
	return -1;
}
int sunxi_secure_storage_init(void) { return 0; }
int sunxi_secure_storage_exit(void) { return 0; }
int sunxi_secure_storage_erase_all(void) { return -1; }
void sunxi_keybox_burn_key(void) {}
int key_extraction_output_generate(unsigned int id, void *out, unsigned int len)
{
	(void)id; (void)out; (void)len;
	return -1;
}

/* ============ 10. USB clocks (mirrors vendor clock_sun60iw2.c) ============ */

int usb_open_clock(void)
{
	unsigned int reg_val;

	fes_dbg("[FES] usb_open_clock\n");

	/* U-Boot's clock_init sets CLKEN; fes1 does not run it, so do it
	 * here or the controller has no clock. */
	reg_val = readl(SUNXI_CCM_BASE + USB0_CLK_REG);
	reg_val |= BIT(31) | BIT(USB0_CLK_REG_USB0_PHY_RSTN_OFFSET);
	reg_val &= ~(0x3u << 24);
	reg_val |= (0x1u << 24);	/* 12M from SYS_CLK24M */
	writel(reg_val, SUNXI_CCM_BASE + USB0_CLK_REG);
	udelay(5000);	/* let PHY/bus clocks settle before touching registers */
	setbits_le32(SUNXI_CCM_BASE + USB0_BGR_REG,
		     BIT(USB0_BGR_REG_USB0_DEVICE_GATING_OFFSET));
	clrbits_le32(SUNXI_CCM_BASE + USB0_BGR_REG,
		     BIT(USB0_BGR_REG_USB0_DEVICE_RST_OFFSET));
	setbits_le32(SUNXI_CCM_BASE + USB0_BGR_REG,
		     BIT(USB0_BGR_REG_USB0_DEVICE_RST_OFFSET));

	reg_val = readl(SUNXI_USBOTG_BASE + USBC_REG_o_SEL);
	reg_val |= (0x01 << 0);
	writel(reg_val, (SUNXI_USBOTG_BASE + USBC_REG_o_SEL));

	reg_val = readl((const volatile void __iomem *)(SUNXI_USBOTG_BASE + USBC_REG_o_PHYCTL));
	reg_val &= ~(0x01 << USBC_PHY_CTL_SIDDQ);
	reg_val |= 0x01 << USBC_PHY_CTL_VBUSVLDEXT;
	writel(reg_val, (volatile void __iomem *)(SUNXI_USBOTG_BASE + USBC_REG_o_PHYCTL));

	return 0;
}

int usb_close_clock(void)
{
	fes_dbg("[FES] usb_close_clock (keep phy on)\n");
	return 0;
}

/* ============ 11. Flash DRAM fallback ============ */

#define EFEX_VFLASH_BASE	0x40000000UL	/* DRAM-backed virtual flash */
#define EFEX_VFLASH_SIZE	(128UL * 1024 * 1024)

int sunxi_flash_init_ext(void) { return 0; }
uint sunxi_flash_size(void) { return EFEX_VFLASH_SIZE / 512; }

extern int fes_flash_read_real(unsigned int start_block, unsigned int nblock,
			       void *buffer);

int sunxi_flash_read(unsigned int start_block, unsigned int nblock, void *buffer)
{
#ifdef CONFIG_FES_REAL_FLASH
	if (fes_flash_read_real(start_block, nblock, buffer) == 0)
		return 1;	/* nonzero = success (callers test !ret) */
	{
		static int warned;
		if (!warned) {
			warned = 1;
			printf("[FES] real flash read failed, using DRAM virtual disk\n");
		}
	}
#endif
	unsigned long off = (unsigned long)start_block * 512;
	if (off + (unsigned long)nblock * 512 > EFEX_VFLASH_SIZE)
		return -1;
	memcpy(buffer, (void *)(EFEX_VFLASH_BASE + off), (unsigned long)nblock * 512);
	return 1;
}

extern int fes_flash_write_real(unsigned int start_block, unsigned int nblock,
				void *buffer);

int sunxi_flash_write(unsigned int start_block, unsigned int nblock, void *buffer)
{
#ifdef CONFIG_FES_REAL_FLASH
	/* Real-media backend; the DRAM disk remains the fallback. */
	if (fes_flash_write_real(start_block, nblock, buffer) == 0)
		return 1;	/* nonzero = success (0 is failure -> csw.status = -1) */
	{
		static int warned;
		if (!warned) {
			warned = 1;
			printf("[FES] real flash write failed, using DRAM virtual disk\n");
		}
	}
#endif
	unsigned long off = (unsigned long)start_block * 512;
	if (off + (unsigned long)nblock * 512 > EFEX_VFLASH_SIZE)
		return -1;
	memcpy((void *)(EFEX_VFLASH_BASE + off), buffer, (unsigned long)nblock * 512);
	return 1;
}

int sunxi_flash_write_end(void) { return 0; }
int sunxi_flash_flush(void) { return 0; }
int sunxi_flash_force_erase(void)
{
	memset((void *)EFEX_VFLASH_BASE, 0xff, EFEX_VFLASH_SIZE);
	return 0;
}

int sunxi_partition_get_info(char *name, void *info)
{
	(void)name; (void)info;
	return -1;
}

int gpt_convert_to_sunxi_mbr(void *sunxi_mbr_buf, char *gpt_buf, int storage_type)
{
	(void)sunxi_mbr_buf; (void)gpt_buf; (void)storage_type;
	return -1;
}

/* ============ 12. Sprite (FES burn session) ============ */

int sunxi_sprite_init(int boot_mode) { (void)boot_mode; return 0; }
int sunxi_sprite_exit(int work_mode) { (void)work_mode; return 0; }

int sunxi_sprite_phyread(unsigned int start_block, unsigned int nblock, void *buffer)
{
	return sunxi_flash_read(start_block, nblock, buffer);
}

int sunxi_sprite_download_boot0(void *buffer, int production_media)
{ (void)buffer; (void)production_media; return -1; }
int sunxi_sprite_download_mbr(void *buffer, uint buffer_size)
{ (void)buffer; (void)buffer_size; return -1; }
int sunxi_sprite_download_uboot(void *buffer, int production_media, int mode)
{ (void)buffer; (void)production_media; (void)mode; return -1; }
int sunxi_sprite_erase_flash(void *arg) { (void)arg; return -1; }
int sunxi_sprite_verify_mbr(void *buf) { (void)buf; return -1; }
int sunxi_sprite_force_erase_key(void) { return 0; }
int sunxi_sprite_part_rawdata_verify(unsigned int start, unsigned int size)
{ (void)start; (void)size; return -1; }
int sprite_erase_for_androidrecovery(void) { return -1; }
int sunxi_update_subsequent_processing(void) { return 0; }
/* ============ 13. Vendor delay aliases (u-boot lib/time) ============ */

void __udelay(unsigned long usec)
{
	udelay(usec);
}

void __msdelay(unsigned long msec)
{
	while (msec--)
		udelay(1000);
}

/* libgcc __aeabi_ldiv0: bare metal has no signal, just return */
int raise(int sig) { (void)sig; return 0; }

