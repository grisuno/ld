#!/bin/bash
# Mutation testing for ld: every mutant in MUTATIONS is injected into a
# private copy of ld.c, rebuilt, and run against the BDD suite. A mutant
# that survives (suite fully green) exposes a test gap.
#
# Mutation format: "name | sed -i expression | file"
#   name     unique mutant id
#   expr     sed program applied once (first match)
#   file     target: ld.c

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/.."
CVM2="${CVM2:-$ROOT/../cvm/cvm2/cvm}"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/ld_mut.XXXXXX")" || exit 1
KEEP_WORK="${KEEP_WORK:-0}"
if [ "$KEEP_WORK" = "1" ]; then
    echo "mutation workspace: $WORK"
else
    trap 'rm -rf "$WORK"' EXIT
fi

CC="gcc -std=c99 -Wall -Wextra -Wpedantic -O2"

MUTATIONS="
movslq-0f-prefix | s/elf_movzx(o1, o2, 0x63, 1, 0)/elf_movzx(o1, o2, 0x63, 1, 1)/ | ld.c
g1-add-to-or | s/#define X86_G1_ADD 0x00/#define X86_G1_ADD 0x08/ | ld.c
g1-sub-to-and | s/#define X86_G1_SUB 0x28/#define X86_G1_SUB 0x20/ | ld.c
jcc-je-to-jne | s/#define X86_JCC_JE  0x84/#define X86_JCC_JE  0x85/ | ld.c
elf-type-exec | s/#define CFG_ELF_ET_DYN   3/#define CFG_ELF_ET_DYN   2/ | ld.c
cvm-magic-bad | s/#define CFG_CVM_MAGIC_3 0x04/#define CFG_CVM_MAGIC_3 0x05/ | ld.c
cvm-je-to-jne | s/e1(OP_CMP_EQ); ejnz(o1->sym);/e1(OP_CMP_NE); ejnz(o1->sym);/ | ld.c
x86-jmp-to-call | s/elf_branch(0xE9, o1)/elf_branch(0xE8, o1)/ | ld.c
elf-entry-main | s/start_idx = find_sym(\"_start\");/start_idx = -1;/ | ld.c
printf-fmt-arg | s/movq %rdi, %rdx/movq %rsi, %rdx/ | ld.c
printf-arg4-slot | s/movq %r8, 24(%rsp)/movq %r8, 32(%rsp)/ | ld.c
printf-arg1-slot | s/movq %rsi, 0(%rsp)/movq %rsi, 8(%rsp)/ | ld.c
vfmt-arg-stride | s/addq \$8, %r14/addq \$16, %r14/ | ld.c
fmt-dec-conv | s/cmpb \$100, %al/cmpb \$101, %al/ | ld.c
fmt-oct-conv | s/cmpb \$111, %al/cmpb \$112, %al/ | ld.c
fmt-str-conv | s/cmpb \$115, %al/cmpb \$116, %al/ | ld.c
"

KILLED=0
SURVIVED=0
BROKEN=0

RUN_SLOW=1 export RUN_SLOW

OLDIFS=$IFS
IFS='
'
for entry in $MUTATIONS; do
    IFS=$OLDIFS
    [ -z "$entry" ] && continue
    name="${entry%%|*}"
    name="$(echo "$name" | tr -d ' ')"
    rest="${entry#*|}"
    expr="${rest%%|*}"
    file="$(echo "${rest#*|}" | tr -d ' ')"
    [ -z "$name" ] && continue
    dir="$WORK/$name"
    mkdir -p "$dir"
    cp "$ROOT/$file" "$dir/$file"
    # sed -i exits 0 even when nothing matched, so a mutation that fails to
    # apply would silently rebuild the original source and be reported as a
    # surviving mutant. Compare against the pristine file instead.
    if ! eval "sed -i '$expr' '$dir/$file'" 2>/dev/null; then
        echo "MUTANT $name: ERROR (sed failed)"
        BROKEN=$((BROKEN + 1))
        IFS='
'
        continue
    fi
    if cmp -s "$ROOT/$file" "$dir/$file"; then
        echo "MUTANT $name: ERROR (expression matched nothing)"
        BROKEN=$((BROKEN + 1))
        IFS='
'
        continue
    fi
    if ! $CC -o "$dir/ld" "$dir/$file" 2> "$dir/build.log"; then
        echo "MUTANT $name: KILLED (build failure)"
        KILLED=$((KILLED + 1))
        IFS='
'
        continue
    fi
    LD_TOOL="$dir/ld" CVM2="$CVM2" "$HERE/run_tests.sh" > "$dir/suite.log" 2>&1
    rc=$?
    if [ "$rc" -eq 0 ]; then
        echo "MUTANT $name: SURVIVED (test gap!)"
        sed -n 's/^=== summary/    suite: summary/p' "$dir/suite.log"
        SURVIVED=$((SURVIVED + 1))
    else
        echo "MUTANT $name: KILLED"
        KILLED=$((KILLED + 1))
    fi
    IFS='
'
done
IFS=$OLDIFS

echo ""
echo "=== mutation summary: $KILLED killed, $SURVIVED survived, $BROKEN broken ==="
if [ "$BROKEN" -gt 0 ]; then
    echo "A broken mutant never reached the suite; fix its expression."
    exit 1
fi
if [ "$SURVIVED" -gt 0 ]; then
    echo "A surviving mutant means the suite does not cover that behavior."
    exit 1
fi
exit 0
