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
    run_chain asm
    run_chain stdint
    run_chain fnptr
    run_chain udiv
    elf_structure_check "$WORK/chain.elf"
else
    echo "SKIP minigcc chain (minigcc binary not found)"
fi

# run_shared_dynlib: minigcc compiles test/dynlib.c (no main), ld links
# it -shared, then structural checks (SONAME, exported names, PT_DYNAMIC
# present) plus the live proof (host dlopen + dlsym + call through the
# harness). A broken dynamic table fails loudly here, never silently.
run_shared_dynlib() {
    if [ ! -x "$MINIGCC" ]; then
        echo "SKIP shared (minigcc binary not found)"
        return
    fi
    if ! "$MINIGCC" "$WORK/dynlib.c" > "$WORK/dynlib.s" 2>/dev/null; then
        echo "FAIL shared: minigcc could not compile dynlib.c"
        note_fail "shared(minigcc)"
        return
    fi
    if ! "$LD_TOOL" -shared -o "$WORK/libdynlib.so" "$WORK/dynlib.s" 2>/dev/null; then
        echo "FAIL shared: -shared link failed"
        note_fail "shared(link)"
        return
    fi
    if command -v readelf >/dev/null 2>&1; then
        if readelf -d "$WORK/libdynlib.so" 2>/dev/null | grep -q "SONAME.*libdynlib.so" && \
           readelf --dyn-syms "$WORK/libdynlib.so" 2>/dev/null | grep -q "dynadd" && \
           readelf --dyn-syms "$WORK/libdynlib.so" 2>/dev/null | grep -q "dynlen" && \
           readelf -l "$WORK/libdynlib.so" 2>/dev/null | grep -q DYNAMIC; then
            echo "PASS shared structure (SONAME, exports, PT_DYNAMIC)"
            PASS=$((PASS + 1))
        else
            echo "FAIL shared structure (SONAME, exports, PT_DYNAMIC)"
            note_fail "shared(structure)"
            return
        fi
    else
        echo "SKIP shared structure (readelf not found)"
    fi
    if ! $CC -o "$WORK/dynlib_host" "$HERE/dynlib_host.c" -ldl 2>/dev/null; then
        echo "FAIL shared: host harness did not compile"
        note_fail "shared(harness)"
        return
    fi
    if "$WORK/dynlib_host" "$WORK/libdynlib.so" 2>/dev/null | grep -q "dynlib: 42 5"; then
        echo "PASS shared live (dlopen, dlsym, call)"
        PASS=$((PASS + 1))
    else
        echo "FAIL shared live (dlopen, dlsym, call)"
        note_fail "shared(live)"
    fi
}

echo "=== shared library (-shared + host dlopen) ==="
run_shared_dynlib

# run_shared_dynuse: -f elf -d links dynuse.c against the fresh
# libdynlib.so; structural checks (DT_NEEDED, GLOB_DAT rows, disjoint
# .rela/.got) plus the live proof (host ld-linux runs the PLT/GOT
# stubs, exit code 47 = dynadd(40,2) + dynlen("hello")). A broken
# layout (e.g. .rela/.got overlap) dies here, never silently.
run_shared_dynuse() {
    local interp=""
    if [ ! -x "$MINIGCC" ]; then
        echo "SKIP dynuse (minigcc binary not found)"
        return
    fi
    for interp in /lib64/ld-linux-x86-64.so.2 /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2; do
        [ -x "$interp" ] && break
        interp=""
    done
    if [ -z "$interp" ]; then
        echo "SKIP dynuse (no host ld-linux found)"
        return
    fi
    cp "$HERE/dynuse.c" "$WORK/" 2>/dev/null || return
    if ! "$MINIGCC" "$WORK/dynuse.c" > "$WORK/dynuse.s" 2>/dev/null; then
        echo "FAIL dynuse: minigcc could not compile dynuse.c"
        note_fail "dynuse(minigcc)"
        return
    fi
    if ! "$LD_TOOL" -f elf -d "$WORK/libdynlib.so" -o "$WORK/dynuse.elf" "$WORK/dynuse.s" 2>/dev/null; then
        echo "FAIL dynuse: -d link failed"
        note_fail "dynuse(link)"
        return
    fi
    if command -v readelf >/dev/null 2>&1; then
        if readelf -d "$WORK/dynuse.elf" 2>/dev/null | grep -q "NEEDED.*libdynlib.so" && \
           readelf -r "$WORK/dynuse.elf" 2>/dev/null | grep -q "R_X86_64_GLOB_DAT"; then
            echo "PASS dynuse structure (NEEDED, GLOB_DAT)"
            PASS=$((PASS + 1))
        else
            echo "FAIL dynuse structure (NEEDED, GLOB_DAT)"
            note_fail "dynuse(structure)"
            return
        fi
    else
        echo "SKIP dynuse structure (readelf not found)"
    fi
    ( cd "$WORK" && LD_LIBRARY_PATH="$WORK" run_prog "$WORK/dynuse.out" "$interp" "./dynuse.elf" )
    local code=$?
    if [ "$code" = "47" ]; then
        echo "PASS dynuse live (ld-linux, exit 47)"
        PASS=$((PASS + 1))
    else
        echo "FAIL dynuse live (exit $code, expected 47)"
        note_fail "dynuse(live)"
    fi
}
echo "=== dynamic executable (-d + host ld-linux) ==="
run_shared_dynuse

# run_shared_override: a -shared library may define names that the
# built-in stubs also provide (strlen, memcmp). The library must export
# them (stubs skipped for -shared) and a -d executable must import them
# through GLOB_DAT instead of keeping the local stub body, proving the
# library definition wins. The live host run returns 5 (strlen length).
run_shared_override() {
    local interp=""
    if [ ! -x "$MINIGCC" ]; then return; fi
    for interp in /lib64/ld-linux-x86-64.so.2 /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2; do
        [ -x "$interp" ] && break
        interp=""
    done
    [ -z "$interp" ] && return
    if ! "$MINIGCC" "$WORK/libstr.c" > "$WORK/libstr.s" 2>/dev/null; then
        echo "FAIL override: minigcc libstr"; note_fail "override(minigcc)"; return
    fi
    if ! "$LD_TOOL" -shared -o "$WORK/libstr.so" "$WORK/libstr.s" 2>/dev/null; then
        echo "FAIL override: -shared link"; note_fail "override(link)"; return
    fi
    if readelf --dyn-syms "$WORK/libstr.so" 2>/dev/null | grep -q "strlen" && \
       readelf --dyn-syms "$WORK/libstr.so" 2>/dev/null | grep -q "memcmp"; then
        echo "PASS override lib exports stub-named functions"
        PASS=$((PASS + 1))
    else
        echo "FAIL override lib exports stub-named functions"
        note_fail "override(export)"
    fi
    if ! "$MINIGCC" "$WORK/usestr.c" > "$WORK/usestr.s" 2>/dev/null; then
        echo "FAIL override: minigcc usestr"; note_fail "override(minigcc2)"; return
    fi
    if ! "$LD_TOOL" -f elf -d "$WORK/libstr.so" -o "$WORK/usestr.elf" "$WORK/usestr.s" 2>/dev/null; then
        echo "FAIL override: -d link"; note_fail "override(dlink)"; return
    fi
    if readelf -r "$WORK/usestr.elf" 2>/dev/null | grep -q "R_X86_64_GLOB_DAT.*strlen"; then
        echo "PASS override exe imports the library symbol"
        PASS=$((PASS + 1))
    else
        echo "FAIL override exe imports the library symbol"
        note_fail "override(import)"
    fi
    ( cd "$WORK" && LD_LIBRARY_PATH="$WORK" run_prog "$WORK/usestr.out" "$interp" "./usestr.elf" )
    local code=$?
    if [ "$code" = "5" ]; then
        echo "PASS override live (ld-linux, exit 5)"
        PASS=$((PASS + 1))
    else
        echo "FAIL override live (exit $code, expected 5)"
        note_fail "override(live)"
    fi
}
echo "=== library definition overrides built-in stubs ==="
cp "$HERE/libstr.c" "$HERE/usestr.c" "$WORK/" 2>/dev/null
run_shared_override

# run_runtime: -runtime emits the built-in C runtime (syscall stubs and
# libc fallbacks) as assembly; each chunk resets .text so a -shared build
# over it keeps every routine a function and exports it. This is the
# primitive a shared libc is built from.
run_runtime() {
    "$LD_TOOL" -runtime > "$WORK/runtime.s" 2>/dev/null
    if [ ! -s "$WORK/runtime.s" ]; then
        echo "FAIL runtime: empty emission"
        note_fail "runtime(empty)"
        return
    fi
    if "$LD_TOOL" -shared -o "$WORK/libcmini.so" "$WORK/runtime.s" 2>/dev/null && \
       readelf --dyn-syms "$WORK/libcmini.so" 2>/dev/null | grep -q "printf" && \
       readelf --dyn-syms "$WORK/libcmini.so" 2>/dev/null | grep -q "malloc" && \
       readelf --dyn-syms "$WORK/libcmini.so" 2>/dev/null | grep -q "strlen"; then
        echo "PASS runtime lib exports runtime functions"
        PASS=$((PASS + 1))
    else
        echo "FAIL runtime lib exports runtime functions"
        note_fail "runtime(export)"
    fi
}
echo "=== -runtime emits a buildable shared-libc source ==="
run_runtime

echo "=== privileged asm: elf encodes, cvm rejects ==="
if "$LD_TOOL" -f elf -o "$WORK/priv.elf" "$HERE/priv.s" 2>/dev/null; then
    echo "PASS priv (elf): cli/sti/hlt assembled"
    PASS=$((PASS + 1))
else
    echo "FAIL priv (elf): cli/sti/hlt rejected"
    note_fail "priv(elf)"
fi
if "$LD_TOOL" -f cvm -o "$WORK/priv.cvm" "$HERE/priv.s" 2>"$WORK/priv.err"; then
    echo "FAIL priv (cvm): raw machine asm accepted for virtualization"
    note_fail "priv(cvm-accept)"
else
    if grep -q "unsupported instruction 'cli'" "$WORK/priv.err"; then
        echo "PASS priv (cvm): cli rejected fail-closed"
        PASS=$((PASS + 1))
    else
        echo "FAIL priv (cvm): wrong diagnostic"
        note_fail "priv(cvm-diag)"
    fi
fi

echo "=== atomics: elf encodes+runs, cvm rejects ==="
if [ -x "$MINIGCC" ]; then
    if "$MINIGCC" "$WORK/sync.c" > "$WORK/sync.s" 2>/dev/null; then
        if "$LD_TOOL" -f elf -o "$WORK/sync.elf" "$WORK/sync.s" 2>/dev/null; then
            chmod +x "$WORK/sync.elf"
            ( cd "$WORK" && run_prog "$WORK/sync.elf.out" "./sync.elf" )
            check sync elf "$WORK/sync.elf.out" $?
        else
            echo "FAIL sync (elf): assembly failed"
            note_fail "sync(elf)"
        fi
        if "$LD_TOOL" -f cvm -o "$WORK/sync.cvm" "$WORK/sync.s" 2>"$WORK/sync.err"; then
            echo "FAIL sync (cvm): lock-prefixed asm accepted for virtualization"
            note_fail "sync(cvm-accept)"
        else
            if grep -q "unsupported instruction 'lock'" "$WORK/sync.err"; then
                echo "PASS sync (cvm): lock rejected fail-closed"
                PASS=$((PASS + 1))
            else
                echo "FAIL sync (cvm): wrong diagnostic"
                note_fail "sync(cvm-diag)"
            fi
        fi
    else
        echo "FAIL sync: minigcc could not compile sync.c"
        note_fail "sync(minigcc)"
    fi
else
    echo "SKIP sync (minigcc binary not found)"
fi

if [ "$RUN_SLOW" = "1" ] && [ -x "$MINIGCC" ] && [ -f "$ROOT/../miniGCC/minigcc.c" ]; then
    echo "=== self-host chain (minigcc.c via minigcc + ld) ==="
    SRCC="$ROOT/../miniGCC/minigcc.c"
    # Generation 2: gen1 minigcc compiles itself; this ld links it.
    if ! run_prog "$WORK/g2.s" "$MINIGCC" "$SRCC"; then
        echo "FAIL selfhost: gen1 minigcc could not compile minigcc.c"
        note_fail "selfhost(gen1)"
    else
        if ! "$LD_TOOL" -f elf -o "$WORK/g2.elf" "$WORK/g2.s" 2>/dev/null; then
            echo "FAIL selfhost: ld could not link gen2"
            note_fail "selfhost(gen2-link)"
        else
            chmod +x "$WORK/g2.elf"
            # Generation 3: the ld-linked compiler compiles itself again.
            if ! run_prog "$WORK/g3.s" "$WORK/g2.elf" "$SRCC"; then
                echo "FAIL selfhost: gen2 (ld-linked) hung or crashed"
                note_fail "selfhost(gen3)"
            elif ! "$LD_TOOL" -f elf -o "$WORK/g3.elf" "$WORK/g3.s" 2>/dev/null; then
                echo "FAIL selfhost: ld could not link gen3"
                note_fail "selfhost(gen3-link)"
            else
                chmod +x "$WORK/g3.elf"
                # Generation 4: fixed point required.
                if ! run_prog "$WORK/g4.s" "$WORK/g3.elf" "$SRCC"; then
                    echo "FAIL selfhost: gen3 hung or crashed"
                    note_fail "selfhost(gen4)"
                elif cmp -s "$WORK/g3.s" "$WORK/g4.s"; then
                    echo "PASS selfhost: fixed point g3.s == g4.s"
                    PASS=$((PASS + 1))
                else
                    echo "FAIL selfhost: fixed point not reached"
                    note_fail "selfhost(fixed-point)"
                fi
                # Behaviour: the self-hosted compiler compiles chain.c;
                # its output must match tests/selfhost.expect.
                if ! run_prog "$WORK/selfhost.s" "$WORK/g3.elf" "$WORK/chain.c"; then
                    echo "FAIL selfhost: self-hosted compiler hung or crashed on chain.c"
                    note_fail "selfhost(chain-compile)"
                elif ! "$LD_TOOL" -f elf -o "$WORK/selfhost.elf" "$WORK/selfhost.s" 2>/dev/null; then
                    echo "FAIL selfhost: ld could not link chain.c output"
                    note_fail "selfhost(chain-link)"
                else
                    chmod +x "$WORK/selfhost.elf"
                    run_prog "$WORK/selfhost.out" "$WORK/selfhost.elf"
                    check selfhost elf "$WORK/selfhost.out" $?
                fi
            fi
        fi
    fi
elif [ "$RUN_SLOW" = "1" ]; then
    echo "SKIP self-host chain (minigcc binary or minigcc.c not found)"
fi

echo ""
echo "=== summary: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -gt 0 ]; then
    echo "failed:$FAILED_NAMES"
    exit 1
fi
exit 0
