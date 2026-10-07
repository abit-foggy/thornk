#!/bin/sh
# test/test_thornk.sh - Verification script for thornk Kbuild converter
set -e

DIR="$(cd "$(dirname "$0")/.." && pwd)"
PASS=0
FAIL=0

ok() { printf '  ok  %s\n' "$1"; PASS=$((PASS + 1)); }
bad() { printf 'FAIL  %s\n' "$1"; FAIL=$((FAIL + 1)); }

OUT_DIR="$DIR/out"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

printf '== testing thornk Kbuild conversion ==\n'

# 0. Standalone single Kbuild file conversion
SCRATCH="$(mktemp -d /tmp/thornk_single_XXXXXX)"
"$DIR/thornk" "$DIR/test/fixtures/Kbuild.sample" "$SCRATCH/thorn.build" > "$SCRATCH/run.log" 2>&1
[ -f "$SCRATCH/thorn.build" ] && ok "thornk emits thorn.build from single Kbuild file"
grep -q 'engine.project("linux_subsystem")' "$SCRATCH/thorn.build" \
    && ok "thorn.build sets project name"
grep -q 'engine.add_target("e1000e", engine.static_lib)' "$SCRATCH/thorn.build" \
    && ok "thorn.build adds e1000e composite module as static_lib"
grep -q 'engine.add_source("e1000e", "netdev.c")' "$SCRATCH/thorn.build" \
    && ok "thorn.build contains composite module sources"
grep -q 'engine.add_include("e1000e", "include/uapi")' "$SCRATCH/thorn.build" \
    && ok "thorn.build contains injected include directories"
grep -q 'engine.add_cflag("e1000e", "-D__KERNEL__")' "$SCRATCH/thorn.build" \
    && ok "thorn.build contains injected compiler flags"

if grep -q "kunit" "$SCRATCH/thorn.build" || grep -q "selftest" "$SCRATCH/thorn.build"; then
    bad "test targets should be filtered out by hooks"
else
    ok "kernel test targets (kunit, selftest) successfully filtered out"
fi

THORN_BIN=""
if command -v thorn >/dev/null 2>&1; then
    THORN_BIN="$(command -v thorn)"
elif [ -x "$DIR/vendor/thorn/out/thorn" ]; then
    THORN_BIN="$DIR/vendor/thorn/out/thorn"
elif [ -x "/home/foggy/thorn/out/thorn" ]; then
    THORN_BIN="/home/foggy/thorn/out/thorn"
fi
if [ -n "$THORN_BIN" ]; then
    (cd "$SCRATCH" && "$THORN_BIN" -f thorn.build --engine both > /dev/null 2>&1)
    [ -f "$SCRATCH/build.ninja" ] && ok "thorn generates build.ninja from thornk output"
    [ -f "$SCRATCH/Makefile" ] && ok "thorn generates Makefile from thornk output"
fi
rm -rf "$SCRATCH"

# 1. Build and invoke thornk on mini_kernel fixture
"$DIR/thornk" "$DIR/test/fixtures/mini_kernel" \
    --config "$DIR/test/fixtures/mini_kernel/.config" \
    --arch x86 \
    --out "$OUT_DIR/build.ninja" > "$OUT_DIR/thornk.log" 2>&1

[ -f "$OUT_DIR/build.ninja" ] && ok "thornk generates out/build.ninja from mini_kernel tree"
[ -f "$OUT_DIR/thorn.build" ] && ok "thornk generates out/thorn.build intermediate spec"

# 2. Check assembly support (.S -> as_cpp rule)
grep -q 'setup\.o: as_cpp .*setup\.S' "$OUT_DIR/build.ninja" \
    && ok "native assembly setup.S mapped to as_cpp compile rule"

# 3. Check C compiler rules and freestanding flags
grep -q 'main\.o: cc .*main\.c' "$OUT_DIR/build.ninja" \
    && ok "C source main.c mapped to cc compile rule"
grep -q 'nostdinc' "$OUT_DIR/build.ninja" \
    && ok "freestanding kernel flag -nostdinc injected"
grep -q '__KERNEL__' "$OUT_DIR/build.ninja" \
    && ok "freestanding kernel flag -D__KERNEL__ injected"

# 4. Check composite device driver resolution (e1000e)
grep -q 'netdev\.o: cc .*netdev\.c' "$OUT_DIR/build.ninja" \
    && ok "e1000e composite member netdev.c mapped to cc rule"
grep -q 'ethtool\.o: cc .*ethtool.c' "$OUT_DIR/build.ninja" \
    && ok "e1000e composite member ethtool.c mapped to cc rule"
grep -q 'hw\.o: cc .*hw\.c' "$OUT_DIR/build.ninja" \
    && ok "e1000e conditional member hw.c included via CONFIG_E1000E_HW=y"

# 5. Verify exclusion of unconfigured targets
if grep -q "unused" "$OUT_DIR/build.ninja"; then
    bad "unconfigured target unused.o must NOT appear in Ninja graph"
else
    ok "unconfigured target unused.o correctly excluded from Ninja graph"
fi

# 6. Check archive targets
grep -q 'built-in\.a' "$OUT_DIR/build.ninja" \
    && ok "built-in.a static library rule present for arch/x86/boot"
grep -q 'e1000e' "$OUT_DIR/build.ninja" \
    && ok "e1000e static library rule present for drivers/net"

# 7. Execute Ninja build graph via samu
(cd "$DIR" && samu -f "$OUT_DIR/build.ninja" > "$OUT_DIR/samu.log" 2>&1) \
    && ok "samu executes generated out/build.ninja cleanly to completion"

# 8. Verify compiled objects and archives on disk
[ -f "$OUT_DIR/arch/x86/boot/built-in.a" ] || [ -f "$OUT_DIR/arch/x86/boot/built-in.a.a" ] \
    && ok "arch/x86/boot/built-in.a archive produced on disk"
[ -f "$OUT_DIR/drivers/net/e1000e.a" ] || [ -f "$OUT_DIR/drivers/net/e1000e.o.a" ] \
    && ok "drivers/net/e1000e archive produced on disk"

# 9. Test legacy Kbuild/Makefile removal via --clean-legacy
CLEAN_DIR=$(mktemp -d /tmp/thornk_clean_XXXXXX)
cp -r "$DIR/test/fixtures/mini_kernel/"* "$CLEAN_DIR/"
legacy_count_before=$(find "$CLEAN_DIR" -name "Makefile*" -o -name "Kbuild*" | wc -l)
[ "$legacy_count_before" -gt 0 ] && ok "fixture copy has $legacy_count_before legacy files"

"$DIR/thornk" "$CLEAN_DIR" --clean-legacy --out "$CLEAN_DIR/build.ninja" > /dev/null 2>&1
legacy_count_after=$(find "$CLEAN_DIR" -name "Makefile*" -o -name "Kbuild*" | wc -l)
[ "$legacy_count_after" -eq 0 ] && ok "--clean-legacy removed all legacy files from tree"
[ -f "$CLEAN_DIR/thorn.build" ] && [ -f "$CLEAN_DIR/build.ninja" ] \
    && ok "--clean-legacy preserved generated thorn.build and build.ninja"
rm -rf "$CLEAN_DIR"

# 10. Test later cleanup via --clean-only
CLEAN_DIR2=$(mktemp -d /tmp/thornk_clean_XXXXXX)
cp -r "$DIR/test/fixtures/mini_kernel/"* "$CLEAN_DIR2/"
"$DIR/thornk" "$CLEAN_DIR2" --clean-only > /dev/null 2>&1
legacy_count_after2=$(find "$CLEAN_DIR2" -name "Makefile*" -o -name "Kbuild*" | wc -l)
[ "$legacy_count_after2" -eq 0 ] && ok "--clean-only removed all legacy files on demand"
rm -rf "$CLEAN_DIR2"

# 11. Test native prepare subcommand
PREP_DIR=$(mktemp -d /tmp/thornk_prep_XXXXXX)
mkdir -p "$PREP_DIR/arch/x86/configs" "$PREP_DIR/kernel" "$PREP_DIR/arch/x86/kernel"
cat << "EOF" > "$PREP_DIR/Makefile"
VERSION = 6
PATCHLEVEL = 12
SUBLEVEL = 4
EOF
cat << "EOF" > "$PREP_DIR/arch/x86/configs/x86_64_defconfig"
CONFIG_64BIT=y
CONFIG_SMP=y
CONFIG_TEST_MOD=m
CONFIG_LOCALVERSION="-thornk"
CONFIG_HEX=0x123
CONFIG_NUM=456
EOF
cat << "EOF" > "$PREP_DIR/kernel/bounds.c"
void f(void) { __asm__ volatile("\n->NR_PAGEFLAGS $24 __NR_PAGEFLAGS\n"); }
EOF
cat << "EOF" > "$PREP_DIR/arch/x86/kernel/asm-offsets.c"
void f(void) { __asm__ volatile("\n->TASK_STATE $0 offsetof(struct task_struct, __state)\n"); }
EOF

"$DIR/thornk" prepare "$PREP_DIR" --defconfig --arch x86 > /dev/null 2>&1

[ -f "$PREP_DIR/.config" ] && ok "thornk prepare creates .config from defconfig"
[ -f "$PREP_DIR/include/generated/autoconf.h" ] && ok "thornk prepare generates include/generated/autoconf.h"
grep -q '#define CONFIG_64BIT 1' "$PREP_DIR/include/generated/autoconf.h" && ok "autoconf.h contains boolean CONFIG_64BIT"
grep -q '#define CONFIG_TEST_MOD_MODULE 1' "$PREP_DIR/include/generated/autoconf.h" && ok "autoconf.h contains modular CONFIG_TEST_MOD_MODULE"
grep -q '#define CONFIG_LOCALVERSION "-thornk"' "$PREP_DIR/include/generated/autoconf.h" && ok "autoconf.h contains string CONFIG_LOCALVERSION"
[ -f "$PREP_DIR/include/generated/uapi/linux/version.h" ] && ok "thornk prepare generates uapi/linux/version.h"
grep -q '#define LINUX_VERSION_CODE 396292' "$PREP_DIR/include/generated/uapi/linux/version.h" && ok "version.h calculates correct LINUX_VERSION_CODE"
[ -f "$PREP_DIR/include/generated/utsversion.h" ] && ok "thornk prepare generates utsversion.h"
[ -f "$PREP_DIR/include/generated/compile.h" ] && ok "thornk prepare generates compile.h"
[ -f "$PREP_DIR/include/generated/bounds.h" ] && ok "thornk prepare generates bounds.h"
grep -q '#define NR_PAGEFLAGS 24' "$PREP_DIR/include/generated/bounds.h" && ok "bounds.h extracts offset macros"
[ -f "$PREP_DIR/arch/x86/include/generated/asm/asm-offsets.h" ] && ok "thornk prepare generates asm-offsets.h"
grep -q '#define TASK_STATE 0' "$PREP_DIR/arch/x86/include/generated/asm/asm-offsets.h" && ok "asm-offsets.h extracts offset macros"
rm -rf "$PREP_DIR"
rm -rf "$DIR/test/fixtures/mini_kernel/include" "$DIR/test/fixtures/mini_kernel/arch/x86/include"

printf '\nthornk acceptance test results: %d passed, %d failed\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]

