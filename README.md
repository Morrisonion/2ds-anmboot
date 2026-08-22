# 3ds-anmboot

Animated Nintendo 3DS software-launch logo boot wrapper with concurrent Luma3DS loading.

## Installation

`3ds-anmboot` sits in front of Luma3DS. The SD card root `boot.firm` becomes 3ds-anmboot, while the original Luma3DS `boot.firm` is moved into `/anmboot/`.

1. On the root of your 3DS SD card, create a folder named:

   ```text
   anmboot
   ```

2. Take your original `boot.firm` and place it inside that folder as:

   ```text
   /anmboot/boot.firm
   ```

3. Take the `3ds-anmboot.firm` from this release and rename it to:

   ```text
   boot.firm
   ```

4. Put that renamed file in the root of the SD card:

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

Build the standard version from the project root:

```sh
./build.sh
```

For **Old 3DS / Old 3DS XL / Old 2DS**, an optional lightweight build removes all red-wave rendering.

```sh
./build.sh -o
```

Outputs:

```text
./build.sh      -> 3ds-anmboot.firm
./build.sh -o   -> 3ds-anmboot-o3ds.firm
```

`build.sh` regenerates both runtime animation banks from the canonical source assets before compiling:

- `arm11/official_glow.bin` from the original BCLIM/BCLYT/BCLAN data in `tools/official_assets/`
- `arm11/three_bank.bin` and `arm11/three_bank.h` from the native logo assets/animation tables


## Behavior

- Loads Luma from `/anmboot/boot.firm`.
- Preserves normal Luma boot-key behavior by falling back to Luma's own ARM11 helper when boot keys are held.
- When using chainloader to map other firms like `X_luma13.3.firm` `B_open_agb_firm.firm`, the boot screen will not be displayed.

## Source layout

```text
arm9/                 ARM9 loader, FAT32/SD access, FIRM loading and launch
arm11/                ARM11 renderer, handoff logic and native logo assets
common/               shared bare-metal helpers/types
tools/                offline animation-bank generators and FIRM packer
tools/official_assets canonical first-party layout/animation/BCLIM inputs
docs/                 reverse-engineering notes retained for reference
```
