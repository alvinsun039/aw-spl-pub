/*
 * (C) Copyright 2015 Hans de Goede <hdegoede@redhat.com>
 */

#ifndef _SUNXI_CPU_H
#define _SUNXI_CPU_H

#include <config.h>

#if defined(CONFIG_SUNXI_NCAT)
#include <arch/cpu_ncat.h>
#elif defined(CONFIG_SUNXI_NCAT_2_0)
#include <arch/cpu_ncat_2_0.h>
#elif defined(CONFIG_SUNXI_NCAT_V2)
#include <arch/cpu_ncat_v2.h>
#elif defined(CONFIG_SUNXI_VERSION1)
#include <arch/cpu_version1.h>
#elif defined(CONFIG_ARCH_SUN60IW1)
#include <arch/sun60iw1p1/cpu_sun60iw1.h>
#elif defined(CONFIG_ARCH_SUN60IW2)
#include <arch/sun60iw2p1/cpu_sun60iw2.h>
#elif defined(CONFIG_ARCH_SUN55IW3)
#include <arch/sun55iw3p1/cpu_sun55iw3.h>
#elif defined(CONFIG_ARCH_SUN55IW5)
#include <arch/sun55iw5p1/cpu_sun55iw5.h>
#elif defined(CONFIG_ARCH_SUN55IW6)
#include <arch/sun55iw6p1/cpu_sun55iw6.h>
#elif defined(CONFIG_ARCH_SUN20IW5)
#include <arch/sun20iw5p1/cpu_sun20iw5.h>
#elif defined(CONFIG_ARCH_SUN65IW1)
#include <arch/sun65iw1p1/cpu_sun65iw1.h>
#else
#error "Unsupported plat"
#endif

#endif /* _SUNXI_CPU_H */
