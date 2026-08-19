#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"; CLANG="${CLANG:-clang}"; OBJCOPY="${OBJCOPY:-llvm-objcopy}"
rm -rf "$ROOT/build"; mkdir -p "$ROOT/build/arm9" "$ROOT/build/arm11"
# Rebuild every generated runtime asset from canonical source inputs.
python3 "$ROOT/tools/generate_official_glow.py"
python3 "$ROOT/tools/generate_three_bank.py"
CF='-ffreestanding -fno-builtin -fno-stack-protector -fno-exceptions -Os -Wall -Wextra -Wno-unused-parameter -marm'
for f in start.S launchstub.S sdmmc.c fat32.c firm.c main.c; do $CLANG --target=arm-none-eabi -mcpu=arm946e-s -march=armv5te $CF -I"$ROOT/common" -I"$ROOT/arm9" -c "$ROOT/arm9/$f" -o "$ROOT/build/arm9/${f%.*}.o"; done
$CLANG --target=arm-none-eabi -mcpu=arm946e-s -march=armv5te $CF -I"$ROOT/common" -c "$ROOT/common/minilib.c" -o "$ROOT/build/arm9/minilib.o"
$CLANG --target=arm-none-eabi -mcpu=arm946e-s -march=armv5te -nostdlib -fuse-ld=lld -Wl,-T,"$ROOT/arm9/link.ld",--gc-sections "$ROOT"/build/arm9/*.o -o "$ROOT/build/arm9.elf"; $OBJCOPY -O binary "$ROOT/build/arm9.elf" "$ROOT/build/arm9.bin"
for f in start.S assets.c native_anim.c gfx.c main.c; do $CLANG --target=arm-none-eabi -mcpu=mpcore -march=armv6k $CF ${ARM11_EXTRA_CFLAGS:-} -I"$ROOT/common" -I"$ROOT/arm11" -c "$ROOT/arm11/$f" -o "$ROOT/build/arm11/${f%.*}.o"; done
$CLANG --target=arm-none-eabi -mcpu=mpcore -march=armv6k $CF -I"$ROOT/common" -c "$ROOT/common/minilib.c" -o "$ROOT/build/arm11/minilib.o"
$CLANG --target=arm-none-eabi -mcpu=mpcore -march=armv6k -nostdlib -fuse-ld=lld -Wl,-T,"$ROOT/arm11/link.ld",--gc-sections "$ROOT"/build/arm11/*.o -o "$ROOT/build/arm11.elf"; $OBJCOPY -O binary "$ROOT/build/arm11.elf" "$ROOT/build/arm11.bin"
python3 "$ROOT/tools/pack_firm.py" --arm9 "$ROOT/build/arm9.bin" --arm11 "$ROOT/build/arm11.bin" --arm11-addr 0x1FFB0000 \
  --data "$ROOT/arm11/official_glow.bin" --data-addr 0x23000000 \
  --data2 "$ROOT/arm11/three_bank.bin" --data2-addr 0x23100000 \
  -o "$ROOT/3ds-anmboot.firm"
