# 3ds-anmboot

Animated Nintendo 3DS software-launch logo boot wrapper with concurrent Luma3DS loading and a stock-style handoff.

Current release: **v1.23 (Final)**

## Installation / Usage

`3ds-anmboot` sits in front of Luma3DS. The SD card root `boot.firm` becomes 3ds-anmboot, while the original Luma3DS `boot.firm` is moved into `/anmboot/`.

1. On the root of your 3DS SD card, create a folder named:

   ```text
   anmboot
   ```

2. Take your **original Luma3DS `boot.firm`** and place it inside that folder as:

   ```text
   /anmboot/boot.firm
   ```

3. Take the `3ds-anmboot.firm` from this release and rename it to:

   ```text
   boot.firm
   ```

4. Put that renamed file in the **root of the SD card**:

   ```text
   /boot.firm
   ```

The final SD layout should look like this:

```text
SD:/
├── boot.firm              <- 3ds-anmboot (renamed from 3ds-anmboot.firm)
└── anmboot/
    └── boot.firm          <- original Luma3DS boot.firm
```

On a normal boot, 3ds-anmboot plays the Nintendo 3DS launch animation while Luma3DS performs its ARM9-side startup work, then hands control over to Luma. Normal Luma boot-key behavior is preserved: when boot keys are held, the wrapper falls back to Luma's own ARM11 helper.

## Build

Requirements:

- Python 3
- NumPy
- Pillow
- Clang with ARM targets
- LLD
- `llvm-objcopy`

Build from the project root:

```sh
./build.sh
```

Output:

```text
3ds-anmboot.firm
```

`build.sh` regenerates both runtime animation banks from the canonical source assets before compiling:

- `arm11/official_glow.bin` from the original BCLIM/BCLYT/BCLAN data in `tools/official_assets/`
- `arm11/three_bank.bin` and `arm11/three_bank.h` from the native logo assets/animation tables

This is intentional: the source archive does not rely on stale generated blobs or the old incorrectly decoded PNG copies.

## Behavior

- Loads Luma from `/anmboot/boot.firm`.
- Runs the logo animation while Luma performs its ARM9-side startup work.
- Uses the corrected raw-BCLIM glow decode and stock-matching wave lifetime/tail behavior.
- Stops spawning new waves at handoff and lets existing waves travel/fade naturally.
- Uses the pre-baked red `3` scaler to avoid expensive runtime division.
- Avoids modifying the actively scanned framebuffer during handoff.
- Keeps the bottom-screen Nintendo logo at original size and visually centered.
- Preserves normal Luma boot-key behavior by falling back to Luma's own ARM11 helper when boot keys are held.

## Source layout

```text
arm9/                 ARM9 loader, FAT32/SD access, FIRM loading and launch
arm11/                ARM11 renderer, handoff logic and native logo assets
common/               shared bare-metal helpers/types
tools/                offline animation-bank generators and FIRM packer
tools/official_assets canonical first-party layout/animation/BCLIM inputs
docs/                 reverse-engineering notes retained for reference
```

## Notes

This source tree intentionally excludes historical `.firm` builds, compiler intermediates, preview images, Python caches, and deprecated test scripts. See `NOTICE` for upstream/reference acknowledgements and asset provenance notes.
