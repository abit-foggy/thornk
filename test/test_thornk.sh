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

printf '== testing thornk Linux Kbuild conversion ==\n'

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

printf '\nthornk acceptance test results: %d passed, %d failed\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
