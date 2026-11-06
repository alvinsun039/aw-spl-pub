# A733 FES loader

This branch adds a second-stage FES loader for the Allwinner A733
(Radxa Cubie A7A) to the vendor `spl-pub` U-Boot tree.  It is based on
`product-aiot-stable` at `8f30b7d` (`aiot-linux-v1.4.6`).

The loader itself lives in [fes/](fes/); its detailed documentation is
[fes/README.md](fes/README.md).

## Background

The A733 BROM exposes FEL: it can load a small image into SRAM and jump
to it, but it cannot write the SD card or the eMMC.  Allwinner's normal
boot flow relies on `boot0`, which is not part of this tree.

FES1 fills that gap.  It runs from the FEL window at `0x90000`, trains
the LPDDR5 on a cold board, then speaks the vendor FES protocol over the
same USB connection.  The host can write the media, read it back and
verify it without a running OS.

## Implementation

- **One execute from cold FEL**: the host writes `TFES` to the RTC magic
  at `0x07090114`; the image trains the DRAM and enters FES in the same
  run.  `FES1` skips training on a warm board; no magic trains and
  returns to the BROM.
- **Vendor USB/EFEX stack** (`fes/usb/vendor/`) with a shim layer
  (`fes/usb/shims/`) that supplies the U-Boot services the stack expects.
- **eMMC**: port C is programmed for ALT3/SMHC2 (cold FEL never runs
  boot0's pinmux), 8-bit, HS200 at 200 MHz after CMD6 with a verified
  fallback to HS at 50 MHz.
- **SD**: SMHC0 on port F, initialised locally at 24 MHz, 4-bit
  HSSDR52.
- **Media writes** use the controller's IDMAC where possible; the
  device splits one 128 KiB FES transaction into 64 KiB (eMMC) or
  128 KiB (SD) chunks.
- Measured on 4 MiB: eMMC write 0.59-0.76 s, read 0.72-0.75 s; SD write
  1.11 s, read 1.45 s.

## Build

Prerequisites:

- an ARM cross compiler (`arm-linux-gnueabi-gcc`, for example the Ubuntu
  `gcc-arm-linux-gnueabi` package), or the vendor toolchain at
  `../tools/toolchain`;
- Allwinner's prebuilt DRAM training object, pinned to the version this
  branch was validated with.

```sh
git clone https://github.com/radxa/allwinner-dramlib.git ../allwinner-dramlib
git -C ../allwinner-dramlib checkout 35e84c8   # v1.4.6

./build.sh                    # eMMC default (SMHC2)
FES_MMC_DEV=0 ./build.sh      # SD default (SMHC0)
```

The image is `out/sun60iw2p1/bin/fes1_sun60iw2p1.bin`.  `build.sh`
prints the git commit, the compiler version and both hashes.  The
reference builds (GCC 13.3.0, binutils 2.42, dramlib `35e84c8`) are:

| media | md5 | sha256 |
|---|---|---|
| eMMC | `f12401ee8d491a9c866f34e58e33c2cd` | `23c051d0ae862bff1bc7f8078afc113800e67ac99a3fa9616e22d6d592be980e` |
| SD | `16c7e563905e5485502c509473ae164a` | `3c1307bee5d79346f1058e76d9d5a5bb0649292a9720291fcd37f2b98cedd6be` |

The hashes identify those builds, not a source revision: the image
embeds the vendor `CI_INFO` string, not the git commit.  A different
GCC/binutils version may produce different bytes even with identical
source; that is not a build failure.  `build.sh` refuses an unvalidated
DRAM blob unless `ALLOW_UNTESTED_DRAM=1` is set.

The two board objects `board/a733/libsun60iw2p1_{fes,sdcard}.a` ship
with the vendor branch and are not built here.  See `NOTICE`.

## CI

`.github/workflows/fes1-build.yml` builds both media on every push and
pull request: it installs `gcc-arm-linux-gnueabi`, checks out
`allwinner-dramlib` at `35e84c8`, runs `./build.sh` and compares the
SHA-256 with the reference above.  A different runner toolchain can
change the hash; the job reports it instead of hiding the difference.

## Usage

The host flasher is `tools/pack/flash.py` in the `edk2-a733` tree.  It
drives the board over usbfs and needs no libusb:

```sh
sudo python3 tools/pack/flash.py boot out/sun60iw2p1/bin/fes1_sun60iw2p1.bin
sudo python3 tools/pack/flash.py burn 24576 a7a_m1.fex
sudo python3 tools/pack/flash.py dump 120000000 4194304 out.bin
```

`--chunk` is capped at 128 KiB: a 256 KiB data phase leaves the device
stuck until power-cycled.  FES has no authentication; use it only on a
board and host you control.

## Documentation

- [fes/README.md](fes/README.md): build details, entry magic, media
  setup, limits and measurements.
- [fes/usb/vendor/README](fes/usb/vendor/README): provenance of the
  vendor USB/EFEX sources.
- [fes/usb/include/ub/README](fes/usb/include/ub/README): provenance of
  the U-Boot header snapshot.
- [COPYING](COPYING), [NOTICE](NOTICE): licensing and the prebuilt
  vendor objects.

## Licence

The vendor sources are GPL-2.0-or-later and the shims are
BSD-2-Clause-Patent.  The prebuilt vendor objects are not relicensed
here; see `NOTICE` before distributing an image.
