#!/bin/bash
# BDD suite for the ld tool (miniGCC asm -> CVM / ELF).
# Every fixture is assembled to BOTH formats; the .cvm runs on the cvm2
# interpreter and the .elf runs natively on Linux. Stdout and exit codes
# are diffed against tests/<name>[.<fmt>].expect{,.exit}.
#
# Layout of expectation files (per fixture name N, format F in cvm|elf):
#   tests/N.expect            default stdout
#   tests/N.F.expect          format-specific stdout override
#   tests/N.expect.exit       default exit code
#   tests/N.F.expect.exit     format-specific exit code override
#
# Tool locations (override with env):
#   LD_TOOL  path to the ld binary (default: build from ld.c)
#   CVM2     path to the cvm2 interpreter
#   MINIGCC  path to the miniGCC compiler binary

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/.."
WORK="$(mktemp -d "${TMPDIR:-/tmp}/ld_tests.XXXXXX")" || exit 1
trap 'rm -rf "$WORK"' EXIT

LD_TOOL="${LD_TOOL:-}"
CVM2="${CVM2:-$ROOT/../cvm/cvm2/cvm}"
MINIGCC="${MINIGCC:-$ROOT/../miniGCC/minigcc}"
RUN_SLOW="${RUN_SLOW:-0}"

CC="gcc -std=c99 -Wall -Wextra -Wpedantic -O2"

PASS=0
FAIL=0
FAILED_NAMES=""

RUN_TMO="${RUN_TMO:-20}"

# run_prog <outfile> <cmd...> : run with a timeout; on timeout the
# program is treated as hung (exit code 124, empty output).
run_prog() {
    local out="$1"
    shift
    timeout "$RUN_TMO" "$@" > "$out" 2>/dev/null
    return $?
}

note_fail() {
    FAIL=$((FAIL + 1))
    FAILED_NAMES="$FAILED_NAMES $1"
}

# check <name> <fmt> <actual_stdout_file> <actual_exit>
check() {
    local name="$1" fmt="$2" out="$3" code="$4"
    local expout="$HERE/$name.expect" expcode="$HERE/$name.expect.exit"
    [ -f "$HERE/$name.$fmt.expect" ] && expout="$HERE/$name.$fmt.expect"
    [ -f "$HERE/$name.$fmt.expect.exit" ] && expcode="$HERE/$name.$fmt.expect.exit"
    local ok=1
    if ! cmp -s "$out" "$expout"; then
        echo "FAIL $name ($fmt): stdout mismatch"
        echo "  expected: $(xxd -p "$expout" | head -c 120)"
        echo "  actual:   $(xxd -p "$out" | head -c 120)"
        ok=0
    fi
    if [ "$code" != "$(cat "$expcode")" ]; then
        echo "FAIL $name ($fmt): exit code $code, expected $(cat "$expcode")"
        ok=0
    fi
    if [ "$ok" = "1" ]; then
        echo "PASS $name ($fmt)"
        PASS=$((PASS + 1))
    else
        note_fail "$name($fmt)"
    fi
}

# run_fixture <name> <extra args...>
run_fixture() {
    local name="$1"
    shift
    local s="$WORK/$name.s"
    if [ ! -f "$s" ]; then
        echo "SKIP $name: no fixture"
        return
    fi
    "$LD_TOOL" -f cvm -o "$WORK/$name.cvm" "$s" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "FAIL $name (cvm): assembly failed"
        note_fail "$name(cvm)"
    else
        run_prog "$WORK/$name.cvm.out" "$CVM2" "$WORK/$name.cvm" "$@"
        check "$name" cvm "$WORK/$name.cvm.out" $?
    fi
    "$LD_TOOL" -f elf -o "$WORK/$name.elf" "$s" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "FAIL $name (elf): assembly failed"
        note_fail "$name(elf)"
    else
        chmod +x "$WORK/$name.elf"
        ( cd "$WORK" && run_prog "$WORK/$name.elf.out" "./$name.elf" "$@" )
        check "$name" elf "$WORK/$name.elf.out" $?
    fi
}

# run_chain <name> [args...] : compile tests/<name>.c with miniGCC, assemble
# the result to both formats and check each against tests/<name>.expect.
run_chain() {
    local name="$1"
    shift
    local c="$WORK/$name.c"
    if [ ! -f "$c" ]; then
        echo "SKIP $name: no C fixture"
        return
    fi
    if ! "$MINIGCC" "$c" > "$WORK/$name.s" 2>/dev/null; then
        echo "FAIL $name: minigcc could not compile $name.c"
        note_fail "$name(minigcc)"
        return
    fi
    "$LD_TOOL" -f elf -o "$WORK/$name.elf" "$WORK/$name.s" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "FAIL $name (elf): assembly failed"
        note_fail "$name(elf)"
    else
        chmod +x "$WORK/$name.elf"
        ( cd "$WORK" && run_prog "$WORK/$name.elf.out" "./$name.elf" "$@" )
        check "$name" elf "$WORK/$name.elf.out" $?
    fi
    "$LD_TOOL" -f cvm -o "$WORK/$name.cvm" "$WORK/$name.s" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "FAIL $name (cvm): assembly failed"
        note_fail "$name(cvm)"
    else
        run_prog "$WORK/$name.cvm.out" "$CVM2" "$WORK/$name.cvm" "$@"
        check "$name" cvm "$WORK/$name.cvm.out" $?
    fi
}

elf_structure_check() {
    local elf="$1"
    if ! command -v readelf >/dev/null 2>&1; then
        echo "SKIP elf structure (readelf not found)"
        return
    fi
    local ok=1
    readelf -h "$elf" | grep -q 'DYN' || ok=0
    readelf -h "$elf" | grep -q 'X86-64' || ok=0
    [ "$(readelf -l "$elf" | grep -c LOAD)" -ge 2 ] || ok=0
    if [ "$ok" = "1" ]; then
        echo "PASS elf structure ($elf)"
        PASS=$((PASS + 1))
    else
        echo "FAIL elf structure ($elf)"
        note_fail "elf-structure"
    fi
}

echo "=== building ld ==="
if [ -z "$LD_TOOL" ]; then
    LD_TOOL="$WORK/ld"
    if ! $CC -o "$LD_TOOL" "$ROOT/ld.c" 2> "$WORK/build.log"; then
        echo "BUILD FAILURE:"
        cat "$WORK/build.log"
        exit 1
    fi
fi
if [ ! -x "$CVM2" ]; then
    echo "cvm2 interpreter not found at $CVM2 (set CVM2=...)"
    exit 1
fi

# Fixtures are copied into the scratch directory and every tool runs against
# the copies. Mutation testing deliberately builds broken binaries from this
# suite, and a mutant that corrupts an argument path must never be able to
# reach the pristine sources.
cp "$HERE"/*.s "$WORK/" 2>/dev/null
cp "$HERE"/*.c "$WORK/" 2>/dev/null
chmod u+w "$WORK"/* 2>/dev/null

echo "=== fixtures: cvm + elf ==="
run_fixture fib
run_fixture fib2
run_fixture fib3
run_fixture t1
run_fixture w1
run_fixture argv a b
run_fixture loop
run_fixture start
run_fixture movslq

echo "=== minigcc chain (C -> asm -> ld -> run) ==="
if [ -x "$MINIGCC" ]; then
    run_chain chain
    run_chain fmt
    run_chain globals
    elf_structure_check "$WORK/chain.elf"
else
    echo "SKIP minigcc chain (minigcc binary not found)"
fi

if [ "$RUN_SLOW" = "1" ] && [ -x "$MINIGCC" ]; then
    echo "=== self-host chain (minigcc.c via minigcc+ld) ==="
    if [ -f "$ROOT/../miniGCC/minigccg2.s" ]; then
        "$LD_TOOL" -f elf -o "$WORK/minigcc.elf" "$ROOT/../miniGCC/minigccg2.s" 2>/dev/null
        chmod +x "$WORK/minigcc.elf"
        run_prog "$WORK/selfhost.s" "$WORK/minigcc.elf" "$WORK/chain.c"
        if [ $? -ne 0 ]; then
            echo "FAIL selfhost: compiler hung or crashed"
            note_fail "selfhost"
        else
            "$LD_TOOL" -f elf -o "$WORK/selfhost.elf" "$WORK/selfhost.s" 2>/dev/null
            chmod +x "$WORK/selfhost.elf"
            run_prog "$WORK/selfhost.out" "$WORK/selfhost.elf"
            check selfhost elf "$WORK/selfhost.out" $?
        fi
    else
        echo "SKIP self-host (minigccg2.s not found)"
    fi
fi

echo ""
echo "=== summary: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -gt 0 ]; then
    echo "failed:$FAILED_NAMES"
    exit 1
fi
exit 0
