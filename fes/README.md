# FES1 loader for the Allwinner A733

FES1 is the second-stage loader used by `tools/pack/flash.py` in the
`edk2-a733` tree.  The BROM loads it over FEL; it trains the LPDDR5 on a
cold board, then exposes Allwinner's FES protocol on the same USB
connection and writes the SD slot or the eMMC.

Nothing here depends on a U-Boot source tree: the vendor USB/EFEX stack
and the headers it needs are vendored under `usb/` (provenance in
`usb/vendor/README` and `usb/include/ub/README`).  The code sits on the
vendor `spl-pub` branch `product-aiot-stable` at `8f30b7d`
(aiot-linux-v1.4.6 release).

## Build

From the repository root:

```sh
./build.sh                    # eMMC default (SMHC2)
FES_MMC_DEV=0 ./build.sh      # SD default (SMHC0)
```

The script picks the vendor toolchain at `../tools/toolchain` when it
exists, otherwise `arm-linux-gnueabi-gcc`, and finds the DRAM object in
`../dramlib` or `../allwinner-dramlib`.  The equivalent manual command:

```sh
make p=sun60iw2p1 b=a733 fes CROSS_COMPILE=arm-linux-gnueabi- \
     FES_DRAM_LIB=../allwinner-dramlib/sun60iw2p1/spl_libdram/libdram
```

The image lands in `out/sun60iw2p1/bin/fes1_sun60iw2p1.bin` and is also
copied to `fes/`.  `FES_VERBOSE=1` keeps the per-register traces; a
release build prints only status and error lines.

The DRAM object is vendor material and is not part of this repository.
Pin it to `35e84c8` (v1.4.6); the current upstream HEAD is a different
blob and has not been validated.  `build.sh` checks its sha256 and
refuses an unknown blob unless `ALLOW_UNTESTED_DRAM=1`.

The image is reproducible: a clean build with the same object produces
the same bytes.

## Entry contract

The host writes the image to `0x90000` and a magic to the RTC data
register at `0x07090114`, then executes it once:

| magic | behaviour |
|---|---|
| `TFES` (`0x53454654`) | train the DRAM, then enter FES in the same run |
| `FES1` (`0x46455331`) | skip training (boot0 already trained), enter FES |
| `0` | train and return to the BROM FEL loop |

`TFES` is what a cold FEL needs: the BROM's `exec` does not invalidate
the caches, and a second execute in a FEL-with-DRAM session is
unreliable, so training and the FES stack share one image and one run.
The magic is consumed on entry.

## Media

| medium | controller | bus | FES chunk |
|---|---|---|---|
| eMMC | SMHC2 via port C ALT3 | 8-bit, HS200 200 MHz (HS 50 MHz fallback) | 64 KiB |
| SD | SMHC0 via port F | 4-bit, HSSDR52 50 MHz | 128 KiB |

Port C carries both SMHC3 (ALT4) and SMHC2 (ALT3); cold FEL never runs
boot0's pinmux, so FES1 programs port C for ALT3 itself.  The eMMC
switches to HS200 with CMD6 and is verified against its vendor boot0
block; the clock falls back to HS if the check fails.

A client transaction is limited to 128 KiB: a 256 KiB data phase leaves
the device stuck until power-cycled.  The media layer splits a request
per medium, so the same client works on both slots.

## Measured throughput

Every write was read back and compared, and the original contents were
restored afterwards:

| medium | write | read |
|---|---|---|
| eMMC (HS200, IDMAC) | **20.8 MB/s** | 21.5 MB/s |
| SD (50 MHz, 4-bit) | **17.5 MB/s** | 14.5 MB/s |

Two changes got here.  The SD clock first: the old code selected HOSC with
M=0, which this controller divides by four, so the card ran at 6 MHz
(24 MHz / 4) rather than the 24 MHz the comment claimed.  SMHC0 now takes
PERI0_400M / 2 (a 200 MHz module clock) with the CLKCR field at 1, i.e.
50 MHz on the card, and CMD6 switches the card to high-speed timing before
the clock goes up — the same plan as the EDK2 `A733SmhcDxe` SD profile.

The rate is chosen per card, not assumed.  A sector is read at 25 MHz,
the card is asked for high speed, and the same sector is read again at
50 MHz; the higher rate is kept only if both reads match and the R1 answer
did not set SWITCH_ERROR, and the card is put back at 25 MHz otherwise.
That mirrors the eMMC path, which already verifies HS200 by reading a
known sector and falls back to HS.  Forcing the comparison to fail on the
bench card did drop it to `25 MHz, clkcr=10003` and halve the read rate,
so the fallback is exercised, not theoretical.

The read direction then moved from the PIO FIFO loop to the DMA channel.
FES1 runs at the 24 MHz the BROM leaves it at (measured: 20M dependent
adds take 5.0 s), so filling the FIFO four bytes at a time cost about
20 ms per 128 KiB and capped reads near 5 MB/s.  The transmit path now
mirrors the receive one: configure the endpoint for DMA, start the
channel, and wait for completion.  Three details matter:

* the ISR clears the endpoint's DMA mode, as the receive ISR does;
* the wait is bounded in milliseconds, and a transfer that does not
  complete returns an error instead of letting the host accept a short
  read;
* the FIFO is drained before the endpoint returns to PIO, or the 13-byte
  CSW collides with packets still queued.

For a flash dump the media read and the transmit are now overlapped within
one 128 KiB transaction: the first half is read, its DMA send is started,
and the second half is fetched from the card while the first is on the
wire.  On the SD slot that took the read from 10.7 to 12.0 MB/s - not the
full benefit, because the second half's send still waits for its own read.
The eMMC is unaffected, being USB bound already.

The DMA read side cannot reach SRAM, and every media buffer lives on the
FES1 SRAM heap, so those buffers are staged through a DRAM buffer first.

Writes were limited by the order of the protocol, not by the card.  Timing
one 128 KiB block on the host showed the data phase takes 3.6 ms (35 MB/s)
but the status packet arrived 12.1 ms into the block, because the device
sends it only once the media write has returned - so the card programmed
while the host sat idle.

The fix splits the media write in two: `fes_mmc_write_start_base()` programs
the descriptors and issues the command, and `fes_mmc_write_wait_base()`
collects it later.  A block the client will not read back (a type bit) is
started right after its data arrives, the status packet goes out at once,
and the 128 KiB of the next block then arrives while the card is still
programming the previous one.  Two receive slots alternate, and a slot is
only reused once its own write has been collected, so the buffer the IDMAC
is reading is never overwritten.  A verified write does not set the bit,
because a read-back has to wait for the write anyway.

Both media went from 7.6-8.1 to 11-12 MB/s, which left the receive path and
the per-block setup as the remaining costs.  The USB link itself moves about
22 MB/s in either direction once the CPU is out of the way (measured with
`--type dram`).

Two things then stood between 12 and 20 MB/s, and neither was the protocol:
per-block timing inside the device showed the media write almost fully
hidden (the next block's start waited only 0.2 ms for it), while the setup
of that next write took 4.3 ms.  Almost half of it was a
`for (d = 0; d < 10000; d++)` spin inherited from u-boot, which at this CPU
clock is about 2 ms of pure waiting per 128 KiB block; the register writes
around it only need the reset to have landed.  Cutting it to 200 iterations
- together with the earlier start/wait split - took eMMC from 12.0 to
20.7 MB/s and SD from 11.0 to 17.5, with the verified path going from 7.1
to 9.5 (eMMC) and 4.8 to 5.7 (SD) for the same reason.

The USB protocol itself costs about 1 ms per block (one command, one status
and one data exchange for every 128 KiB), which is now well under the 6.9 ms
a block takes; a larger transaction would buy at most a few percent, so the
128 KiB shape the device has always used is kept.

Reads had one serialised step left: the first 64 KiB of every block was read
in the command phase, so its 3 ms sat on the critical path (SD), and only the
second half's read overlapped a send.  The block after the requested one is
now read while the requested one is on the wire, using two alternating
buffers - a dump is sequential, so the next request normally hits and costs
no read at all.  Only a read that follows a read prefetches, so the verified
path (write, read back, write, ...) does not pay for read-ahead it cannot use,
and any write or erase drops the prefetched block.

That first attempt corrupted exactly eight bytes at the start of every second
block.  They were `ff ff 00 00 00 00 00 00`: `__sunxi_usb_efex_fill_status()`
builds the reply for the status stage at `base_send_buffer` and sets
`mark = 0xffff`, and that buffer was also the first read slot.  With the old
serial order the status was overwritten by the next block's read before it
was sent; with read-ahead the status landed on top of already-prefetched
data.  The read slots now sit at `RECV_MEM_SIZE/2`, away from every small
reply that uses the start of the send buffer.

Together this took the SD read from 11.8 to 14.5 MB/s and the eMMC read from
20.0 to 21.5.

## Testing

Both media were validated on hardware by dumping a test area, writing a
random pattern, reading it back, comparing, and restoring the original
content.  The eMMC area is the last GiB (sector `120000000`).  A write
can be checked the same way with `flash.py burn` followed by
`flash.py dump`; there is no separate test script.

Choose the SD test area carefully.  Sector `512` is the start of the
GPT gap (the MBR's single protective entry spans LBA 1-61067263, and the
first usable GPT partition starts at LBA 32768), so the gap is only
32256 sectors = 15.75 MiB.  A 16 MiB write at `512` therefore runs past
LBA 32768 and overwrites the first 256 KiB of `mmcblk1p1`.  This
actually happened during the 50 MHz benchmark and is harmless only
because of what that partition holds, but do not rely on it: keep test
writes inside the gap (4 MiB at `512` is safe) or, for larger sizes,
use space inside a scratch partition or the last GiB of the card.

`mmcblk1p1` is a 16 MiB reserved partition with no filesystem; its
entire contents are zero.  If a test write does reach it, restore it by
zeroing the partition (`dd if=/dev/zero of=/dev/mmcblk1p1 bs=1M
conv=fsync`), which returns it to the original all-zero state.

The medium a command addresses is chosen by the FES1 build, not by the
address: `FES_MMC_DEV=2` (the default) talks to the eMMC and
`FES_MMC_DEV=0` to the SD slot.  A sector number that looks like an eMMC
address is therefore written to the card when the SD image happens to be
loaded - and if it is past the end of the card, the controller wraps it
and the write lands somewhere else entirely.  Doing that during this
work put 8 MiB into free space inside the SD root filesystem and left the
card needing a power cycle before the kernel could read it again.  Check
which image is running (`[FES] sd init OK` only appears for the SD build)
before naming a sector, and keep such writes well inside a medium you have
just identified.

## Vendor inputs

The link consumes three prebuilt objects that are not compiled from
this repository:

| object | source | sha256 of the tested file |
|---|---|---|
| `board/a733/libsun60iw2p1_fes.a` | vendor branch | `86b88adc04eedaa50e0a95e858c22ec50de16d68f96ef891feb82c8496a3f119` |
| `board/a733/libsun60iw2p1_sdcard.a` | vendor branch | `e0eedaf36da36fae7c64af00c210f7b890d1f4bb142a6ab7bb04807cf44a83b2` |
| `dramlib/sun60iw2p1/spl_libdram/libdram` | `github.com/radxa/allwinner-dramlib` commit `35e84c8` (v1.4.6) | `d05bc328619cb64f4d273f8d83670f59d085d3758a0968eb4a7f6328f6c42c8e` |

The first two ship with the vendor branch; the DRAM object is supplied
separately through `FES_DRAM_LIB`.  Despite the `.a` suffix the first
two are single relocatable ELF objects, not archives.  None of them is built from source
here, and the DRAM object has no licence file, so they are not covered
by the GPL/BSD grants for this repository.  Check the vendor terms
before redistributing a built image.

## Security

FES has no authentication: anyone with access to the OTG port can read
and write the media.  Use it only on a board you control, and run the
host flasher on a trusted machine (it opens usbfs as root).

## Licence

The vendor USB/EFEX stack and the vendor entry code are
GPL-2.0-or-later; the FES shims are BSD-2-Clause-Patent.  The prebuilt
vendor objects above are not relicensed here.  See `../COPYING` and
`../NOTICE`.
