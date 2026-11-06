/*
 * Copyright (c) 2026, edk2-a733 contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#ifndef FES_PRINTF_H
#define FES_PRINTF_H

#if defined(__GNUC__)
#define FES_PRINTF_FORMAT __attribute__((format(printf, 1, 2)))
#else
#define FES_PRINTF_FORMAT
#endif

int tick_printf(const char *fmt, ...) FES_PRINTF_FORMAT;

/*
 * Per-operation traces (register dumps, clock probes).  A release image
 * prints only status lines; build with FES_VERBOSE=1 to keep them.
 */
#ifdef CONFIG_FES_VERBOSE
#define fes_dbg(...) tick_printf(__VA_ARGS__)
#else
#define fes_dbg(...) do { } while (0)
#endif

#endif
