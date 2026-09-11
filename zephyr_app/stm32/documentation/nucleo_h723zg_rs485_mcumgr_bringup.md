# nucleo_h723zg rs485_mcumgr: bring-up issues and root causes

Porting `nucleo_g474re/rs485_mcumgr` to `nucleo_h723zg` (same app, same MCUboot +
RS485/mcumgr update flow) surfaced four separate issues before the board would
boot. None of them were visible from source alone - each needed either a flash
readback, a debug-level MCUboot log, or reading MCUboot's own bootutil source.
Recorded here so a future STM32H7 port (or a similar large-erase-sector part)
doesn't have to rediscover them.

## Board-specific config (for reference)

- RS485 data: LPUART1, PB6/PB7 (Arduino D1/D0) - not USART1 like the G4 board;
  USART1 isn't wired to this board's header at all.
- RS485 DE/RE: PG12 (Arduino D7) - not PA8.
- Debug console: USART3 (ST-LINK VCP), the board's existing `zephyr,console` -
  not LPUART1 like the G4 board.
- Flash: 1MB, single bank, **128KB erase sectors** (`stm32h723.dtsi`). This
  128KB sector size is the common thread behind three of the four issues
  below - it's much larger than most STM32 parts Zephyr is usually built for.
- Unlike `nucleo_g474re.dts`, the upstream `nucleo_h723zg.dts` defines **no**
  flash partition table, no `zephyr,code-partition` chosen, no `mcuboot-led0`
  alias, and doesn't enable IWDG - all had to be added via overlay.

## Issue 1 - stale/wrong build directory reused silently

**Symptom:** first `west build -b nucleo_h723zg --sysbuild -d build` printed
`ninja: no work to do` and produced a single-image (non-sysbuild) build with
`CONFIG_FLASH_LOAD_OFFSET=0` and no MCUboot - a leftover from an earlier plain
`west build` in the same directory.

**Root cause:** incremental `west build`/`west flash` reuses whatever's in
the build directory if CMake doesn't think anything relevant changed - it does
not warn if the existing config doesn't match the command you just typed
(e.g. missing `--sysbuild`), and in this session it also didn't reliably pick
up devicetree overlay edits on its own (`ninja: no work to do` recurred later
too, after real `.overlay` changes).

**Fix:** when changing board-level devicetree overlays for this project,
always force a full reconfigure - don't rely on incremental `west build`:

```
rm -rf build
west build -p always -b nucleo_h723zg --sysbuild -d build
west flash -d build
```

## Issue 2 - MCUboot linked and flashed at the app's address, not 0x0

**Symptom:** both MCUboot and the app were flashed to `0x08020000`. Nothing
ended up at the reset vector (`0x08000000`), so the chip kept running
whatever was there before (the old blinky image) - "flashed but nothing
changed."

**Root cause:** `sysbuild/mcuboot.overlay` (the overlay sysbuild applies only
to MCUboot's own build) set:

```dts
chosen {
    zephyr,code-partition = &slot0_partition;
};
```

MCUboot's own build already points that chosen node at `&boot_partition` by
itself - this override wasn't needed and silently replaced that default,
making `CONFIG_FLASH_LOAD_OFFSET=0x20000` for MCUboot's own build instead of
`0x0`. Confirmed by comparing `.config` of MCUboot's sub-build against the
working `nucleo_g474re/rs485_mcumgr` build (where `FLASH_LOAD_OFFSET=0x0`
despite that board's *own* dts globally setting the same chosen to slot0 for
every image built for it - MCUboot's build overrides it internally, and
should be left alone).

**Fix:** don't set `zephyr,code-partition` in `sysbuild/mcuboot.overlay` at
all. Only the app-level `boards/nucleo_h723zg.overlay` needs it.

## Issue 3 - NVS needs >=2 flash sectors, storage partition was 1

**Symptom:** once MCUboot booted correctly (Issue 2 fixed), the app printed
`transport init failed` and returned before ever reaching the LED loop - no
blinking at all.

**Root cause:** `storage_partition` was sized as exactly one 128KB erase
sector. Zephyr's NVS backend (`subsys/kvss/nvs/nvs.c:1275`) refuses to mount
on fewer than 2 sectors - it needs at least one spare sector to
garbage-collect into:

```c
if (fs->sector_count < 2) {
    LOG_ERR("Configuration error - sector count");
    return -EINVAL;
}
```

This failed `settings_load()` -> `rs485_envelope_init()` ->
`smp_rs485_transport_init()`, and `main()` returns early on that failure
before reaching the blink loop.

**Fix (partial - see Issue 4 for the actual final fix):** initially just
gave storage more sectors. This turned out to be necessary but not
sufficient - see Issue 4.

## Issue 4 - swap-using-offset requires slot1 to be exactly one sector bigger than slot0

**Symptom:** MCUboot rejected the primary slot's image every single time,
regardless of what was actually flashed there:

```
E: Image in the primary slot is not valid!
E: Unable to find bootable image
```

This one took the longest to pin down because everything *looked* correct:
- Header magic bytes verified correct in flash via SWD readback
  (`STM32_Programmer_CLI -r32 0x08020000 16` matched the local `.bin` exactly).
- `imgtool.py verify` on both the local `zephyr.signed.bin` **and** a flash
  dump of the actual on-chip bytes both independently reported "Image was
  correctly validated" with the same image digest.
- Partition IDs matched exactly between MCUboot's and the app's separately
  compiled devicetrees (`grep _PARTITION_ID` on both `devicetree_generated.h`).
- A full `rm -rf build && west build -p always --sysbuild` (ruling out
  Issue 1 recurring) made no difference.

None of that pointed at the actual bug, which was only visible by turning on
MCUboot's own debug-level logging:

```
CONFIG_MCUBOOT_LOG_LEVEL_DBG=y
CONFIG_MCUBOOT_UTIL_LOG_LEVEL_DBG=y
```

which produced:

```
D: Non-optimal sector distribution, slot0 has 2 usable sectors but slot1 has 1 usable sectors
...
D: bootutil_img_validate: TLV off 50348, end 50680
D: bootutil_img_validate: TLV beyond image size
```

**Root cause:** `app_max_size()` in `bootloader/mcuboot/boot/bootutil/src/swap_offset.c`
(the "swap using offset" algorithm, which this project uses since it needs no
scratch partition) computes:

```c
size_t trailer_sz = boot_trailer_sz(...);      // rounded up to 1 sector
size_t padding_sz  = sector_sz;                // 1 more sector, secondary only

available_pri_sz = num_sectors_pri * sector_sz - trailer_sz;
available_sec_sz = num_sectors_sec * sector_sz - trailer_sz - padding_sz;

return min(available_pri_sz, available_sec_sz);
```

With slot0 and slot1 both sized at 2 sectors (256KB each, which looked like
the obviously-correct symmetric choice), `available_sec_sz` computes to
`2*128K - 128K - 128K = 0`. The maximum bootable image size becomes exactly
**0**, so any image at all - correctly signed or not - gets rejected as "not
valid." This is *by design*: swap-using-offset needs slot1 to reserve both a
trailer sector (like slot0) **and** a padding sector for the shift operation,
so slot1 must have exactly `slot0_sectors + 1` sectors - never the same
count. This is also checked explicitly (with a warning, not an error) in
`boot_slots_compatible()` in the same file.

**Fix:** final partition table, still exactly 1MB and sector-aligned, now
satisfying both this constraint and Issue 3's NVS minimum:

| Partition | Address | Size | Sectors |
|---|---|---|---|
| mcuboot | `0x00000000` | 128K | 1 |
| image-0 (slot0) | `0x00020000` | 256K | 2 |
| image-1 (slot1) | `0x00060000` | 384K | 3 |
| storage | `0x000c0000` | 256K | 2 |

Applied identically in both `boards/nucleo_h723zg.overlay` (the app) and
`sysbuild/mcuboot.overlay` (MCUboot's own build needs the same map to know
its own image-slot bounds).

## Issue 5 - NVS sector size is a uint16_t; 128KB overflows it

**Symptom:** with the Issue 4 partition fix in place, MCUboot validated and
booted the app correctly, but the app immediately printed
`transport init failed: -33` (`-EDOM`) and returned early - still no working
RS485/mcumgr transport, though at least now the header/signature/partition
chain was solid.

**Root cause:** `subsys/settings/src/settings_nvs.c:409-410`:

```c
nvs_sector_size = CONFIG_SETTINGS_NVS_SECTOR_SIZE_MULT * hw_flash_sector.fs_size;
if (nvs_sector_size > UINT16_MAX) {
    return -EDOM;
}
```

NVS stores its sector size in a `uint16_t` (max 65535 bytes). This board's
128KB (131072-byte) hardware erase sector always exceeds that, on any
partition size - this was never fixable by resizing `storage_partition`
(Issue 3's fix was necessary for NVS's 2-sector minimum, but NVS itself
cannot work on this hardware at all).

**Fix:** switch the settings backend from NVS to **ZMS** (Zephyr Memory
Storage, the modern replacement designed for exactly this: larger/variable
erase sizes, RRAM, etc.). ZMS's equivalent check in `settings_zms.c` uses
`uint32_t` and has no such ceiling. In `prj.conf`:

```conf
CONFIG_ZMS=y
CONFIG_SETTINGS_ZMS=y
```

in place of `CONFIG_NVS` / `CONFIG_SETTINGS_NVS`. No devicetree change
needed - same as NVS, `settings_zms.c` falls back to `storage_partition`
automatically when there's no `zephyr,settings-partition` chosen node.

## Takeaway for the next large-erase-sector board port

If a board's flash erase-block-size is >64KB (anything beyond typical STM32
G0/G4/F4/L4 parts, common on H7/H5 with big sectors), two things need
non-default handling for an MCUboot + settings project like this one:

1. Use `CONFIG_ZMS`/`CONFIG_SETTINGS_ZMS`, not NVS - NVS's `uint16_t` sector
   size field will always overflow.
2. If using swap-using-offset (no scratch partition), size slot1 as exactly
   `slot0_sectors + 1` sectors, never equal to slot0 - equal-sized slots
   silently cap the maximum bootable image size at 0.

And independent of erase size: never set `zephyr,code-partition` in a
sysbuild MCUboot-only overlay - MCUboot supplies its own correct default
pointing at `&boot_partition`.
