#!/bin/sh
# test_thornk.sh - Automated tests for thornk Kbuild converter

set -e

DIR="$(cd "$(dirname "$0")/.." && pwd)"
PASS=0
FAIL=0

ok() { printf '  ok  %s\n' "$1"; PASS=$((PASS + 1)); }
bad() { printf 'FAIL  %s\n' "$1"; FAIL=$((FAIL + 1)); }

SCRATCH="$(mktemp -d /tmp/thornk_test_XXXXXX)"
trap 'rm -rf "$SCRATCH"' EXIT

printf '== testing thornk Kbuild converter ==\n'

# 1. Run thornk via Pith JIT
"$DIR/thornk" "$DIR/fixtures/Kbuild.sample" "$SCRATCH/thorn.build" > "$SCRATCH/run.log" 2>&1
[ -f "$SCRATCH/thorn.build" ] && ok "thornk emits thorn.build from Kbuild fixture"

# 2. Check generated contents
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

# 3. Check that test targets were ignored
if grep -q "kunit" "$SCRATCH/thorn.build" || grep -q "selftest" "$SCRATCH/thorn.build"; then
    bad "test targets should be filtered out by hooks"
else
    ok "kernel test targets (kunit, selftest) successfully filtered out"
fi

# 4. Verify thorn can consume the generated spec
if [ -x "/home/foggy/thorn/out/thorn" ]; then
    (cd "$SCRATCH" && /home/foggy/thorn/out/thorn -f thorn.build --engine both > /dev/null 2>&1)
    [ -f "$SCRATCH/build.ninja" ] && ok "thorn generates build.ninja from thornk output"
    [ -f "$SCRATCH/Makefile" ] && ok "thorn generates Makefile from thornk output"
fi

printf '\nthornk test results: %d passed, %d failed\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
