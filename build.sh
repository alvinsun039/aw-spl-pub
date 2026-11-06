#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause-Patent
# Copyright (c) 2026, edk2-a733 contributors
#
# Build the A733 FES1 loader from this vendor tree.
#
# Prerequisites:
#   - an ARM cross compiler (arm-linux-gnueabi-gcc, or the vendor
#     toolchain under ../tools/toolchain);
#   - Allwinner's prebuilt DRAM training object, pinned to the version
#     this branch was validated with:
#
#       git clone https://github.com/radxa/allwinner-dramlib.git \
#           ../allwinner-dramlib
#       git -C ../allwinner-dramlib checkout 35e84c8
#
# FES_MMC_DEV=0 selects an SD-default image (SMHC0); the default 2
# selects eMMC (SMHC2).  FES_DRAM_LIB overrides the DRAM object path.

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$root"

PLATFORM=sun60iw2p1
BOARD=a733
FES_MMC_DEV=${FES_MMC_DEV:-2}

# Cross compiler: an explicit CROSS_COMPILE wins, then the vendor
# toolchain the tree expects, then the system toolchain.
if [ -z "${CROSS_COMPILE:-}" ]; then
	if [ -x "$root/../tools/toolchain/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-gcc" ]; then
		: # the Makefile default points here
	elif command -v arm-linux-gnueabi-gcc >/dev/null 2>&1; then
		CROSS_COMPILE=arm-linux-gnueabi-
	else
		echo "error: no ARM cross compiler found" >&2
		echo "       install gcc-arm-linux-gnueabi or set CROSS_COMPILE" >&2
		exit 1
	fi
fi
CC_BIN=${CROSS_COMPILE:-$root/../tools/toolchain/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-}gcc

# DRAM training object: binary-only vendor input, not part of this
# repository.
DRAMLIB=${FES_DRAM_LIB:-}
if [ -z "$DRAMLIB" ]; then
	for d in "$root/../dramlib" "$root/../allwinner-dramlib"; do
		if [ -f "$d/$PLATFORM/spl_libdram/libdram" ]; then
			DRAMLIB=$d/$PLATFORM/spl_libdram/libdram
			break
		fi
	done
fi

if [ -z "$DRAMLIB" ] || [ ! -f "$DRAMLIB" ]; then
	cat >&2 <<EOF
error: DRAM training object not found.

  git clone https://github.com/radxa/allwinner-dramlib.git "$root/../allwinner-dramlib"
  git -C "$root/../allwinner-dramlib" checkout 35e84c8
  ./build.sh

or set FES_DRAM_LIB to an existing libdram object.
EOF
	exit 1
fi

expected=d05bc328619cb64f4d273f8d83670f59d085d3758a0968eb4a7f6328f6c42c8e
if command -v sha256sum >/dev/null 2>&1; then
	got=$(sha256sum "$DRAMLIB" | awk '{print $1}')
	if [ "$got" != "$expected" ]; then
		echo "warning: DRAM object is not the validated version" >&2
		echo "  expected $expected (dramlib 35e84c8, v1.4.6)" >&2
		echo "  got      $got" >&2
		if [ "${ALLOW_UNTESTED_DRAM:-0}" != 1 ]; then
			echo "  set ALLOW_UNTESTED_DRAM=1 to build anyway" >&2
			exit 1
		fi
	fi
fi

# FES_MMC_DEV is a compiler define, and the object stamps do not track
# it; always rebuild so switching media cannot reuse the other variant.
rm -rf "$root/fes/usb/build" "$root/fes/fes1.elf" "$root/fes/fes1.bin" \
	"$root/fes/fes1.map" "$root/fes/fes1.lds" "$root"/fes/main/*.o

echo "building FES1 for $PLATFORM/$BOARD (FES_MMC_DEV=$FES_MMC_DEV)"
make "p=$PLATFORM" "b=$BOARD" fes \
	${CROSS_COMPILE:+CROSS_COMPILE=$CROSS_COMPILE} \
	"FES_DRAM_LIB=$DRAMLIB" \
	"FES_MMC_DEV=$FES_MMC_DEV"

out=$root/out/$PLATFORM/bin/fes1_$PLATFORM.bin
commit=$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo unknown)
ci_info=$(sed -n 's/^#define CI_INFO "\(.*\)"/\1/p' "$root/board/$BOARD/commit_info.h" 2>/dev/null | head -1 || true)

echo
echo "source:   $commit (CI_INFO ${ci_info:-unknown})"
echo "compiler: $("$CC_BIN" --version 2>/dev/null | head -1)"
echo "binutils: $("${CC_BIN%gcc}ld" --version 2>/dev/null | head -1)"
echo "image:    $out"
if command -v md5sum >/dev/null 2>&1; then
	echo "md5:      $(md5sum "$out" | awk '{print $1}')"
fi
if command -v sha256sum >/dev/null 2>&1; then
	echo "sha256:   $(sha256sum "$out" | awk '{print $1}')"
fi

case "$FES_MMC_DEV" in
2) ref_md5=d7ebd9fdb296d84b700e990764bec4ea
   ref_sha=9aadcf8a3c93ce6be80e7eb2dedec3d5d89eb95ceca4dc1de002d8bfc69956f7 ;;
0) ref_md5=0e54ddc9d01abb7a069778c43ef7e28a
   ref_sha=1f024ce9c889c1a934171c1d9caff63285038aabc100943b3aa26463a3af1df3 ;;
*) ref_md5=; ref_sha= ;;
esac
if [ -n "$ref_sha" ] && command -v sha256sum >/dev/null 2>&1; then
	echo
	echo "reference (FES_MMC_DEV=$FES_MMC_DEV):"
	echo "  md5    $ref_md5"
	echo "  sha256 $ref_sha"
	echo "  built with GCC 13.3.0 + binutils 2.42"
	if [ "$(sha256sum "$out" | awk '{print $1}')" = "$ref_sha" ]; then
		echo "  same source, inputs and toolchain: matches the reference image"
	else
		echo "  differs: expected with another toolchain; compare behavior,"
		echo "  not only the hash, before reporting a problem"
	fi
fi
