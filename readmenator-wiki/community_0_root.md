# root

*Community 0 | 26 files | cohesion 1.00*

## Definition

This community groups 26 file(s) rooted at `test` with dominant language c (cohesion 1.00). Central symbols: `Blob`, `CFG_ABI_BYTES`, `CFG_CVM_HDR_SIZE`, `CFG_CVM_MAGIC_0`, `CFG_CVM_MAGIC_1`, `CFG_CVM_MAGIC_2`, `CFG_CVM_MAGIC_3`, `CFG_CVM_VER_MAJ`. Core file: `ld.c` (307 symbols). Documented purpose: BDD suite for the ld tool (miniGCC asm -> CVM / ELF). Every fixture is assembled to BOTH formats; the .cvm runs on the cvm2 interpreter and the .elf runs native.

## Files

### `test` (23 files)

| File | Language | Layer | Symbols | Doc |
|------|----------|-------|---------|-----|
| `test/argv.c` | c | testing | 2 | no |
| `test/argv.s` | s | testing | 2 | no |
| `test/asm.c` | c | testing | 4 | no |
| `test/chain.c` | c | testing | 2 | no |
| `test/fib.s` | s | testing | 3 | no |
| `test/fib2.s` | s | testing | 3 | no |
| `test/fib3.s` | s | testing | 3 | no |
| `test/fmt.c` | c | testing | 2 | no |
| `test/fnptr.c` | c | testing | 9 | no |
| `test/globals.c` | c | testing | 3 | no |
| `test/hello.c` | c | testing | 1 | no |
| `test/loop.s` | s | testing | 2 | no |
| `test/movslq.s` | s | testing | 2 | no |
| `test/mutate.sh` | sh | testing | 0 | yes |
| `test/priv.s` | s | testing | 2 | no |
| `test/run_tests.sh` | sh | testing | 6 | yes |
| `test/start.s` | s | testing | 2 | no |

### `.` (3 files)

| File | Language | Layer | Symbols | Doc |
|------|----------|-------|---------|-----|
| `app.py` | py | utility | 0 | yes |
| `install.sh` | sh | utility | 0 | no |
| `ld.c` | c | utility | 307 | no |

*... and 6 more files in this community.*


## Key Symbols

- `CFG_MAX_SYMBOLS` (macro, `ld.c:24`) `#define CFG_MAX_SYMBOLS`
- `CFG_MAX_FIXUPS` (macro, `ld.c:26`) `#define CFG_MAX_FIXUPS`
- `CFG_FIXUP_INIT` (macro, `ld.c:27`) `#define CFG_FIXUP_INIT`
- `CFG_LINE_MAX` (macro, `ld.c:28`) `#define CFG_LINE_MAX`
- `CFG_NAME_MAX` (macro, `ld.c:29`) `#define CFG_NAME_MAX`
- `CFG_MAX_NATS` (macro, `ld.c:30`) `#define CFG_MAX_NATS`
- `CFG_MAX_ERRORS` (macro, `ld.c:31`) `#define CFG_MAX_ERRORS`
- `CFG_GROW_UNIT` (macro, `ld.c:32`) `#define CFG_GROW_UNIT`
- `CFG_ABI_BYTES` (macro, `ld.c:33`) `#define CFG_ABI_BYTES`
- `CFG_STACK_BASE` (macro, `ld.c:35`) `#define CFG_STACK_BASE`
- `CFG_XSTACK_DEF` (macro, `ld.c:36`) `#define CFG_XSTACK_DEF`
- `CFG_MAX_ARGS` (macro, `ld.c:40`) `#define CFG_MAX_ARGS`
- `CFG_REG_LOCALS` (macro, `ld.c:41`) `#define CFG_REG_LOCALS`
- `CFG_SLOT_FLAGS_A` (macro, `ld.c:43`) `#define CFG_SLOT_FLAGS_A`
- `CFG_SLOT_FLAGS_B` (macro, `ld.c:44`) `#define CFG_SLOT_FLAGS_B`
- `CFG_SLOT_S0` (macro, `ld.c:45`) `#define CFG_SLOT_S0`
- `CFG_SLOT_S1` (macro, `ld.c:46`) `#define CFG_SLOT_S1`
- `CFG_GSLOT_RSP` (macro, `ld.c:47`) `#define CFG_GSLOT_RSP`
- `CFG_GSLOT_RBP` (macro, `ld.c:49`) `#define CFG_GSLOT_RBP`
- `CFG_GSLOT_ARGS` (macro, `ld.c:50`) `#define CFG_GSLOT_ARGS`
- `CFG_GSLOT_RET` (macro, `ld.c:51`) `#define CFG_GSLOT_RET`
- `CFG_CVM_MAGIC_0` (macro, `ld.c:52`) `#define CFG_CVM_MAGIC_0`
- `CFG_CVM_MAGIC_1` (macro, `ld.c:54`) `#define CFG_CVM_MAGIC_1`
- `CFG_CVM_MAGIC_2` (macro, `ld.c:55`) `#define CFG_CVM_MAGIC_2`
- `CFG_CVM_MAGIC_3` (macro, `ld.c:56`) `#define CFG_CVM_MAGIC_3`
- `CFG_CVM_VER_MAJ` (macro, `ld.c:57`) `#define CFG_CVM_VER_MAJ`
- `CFG_CVM_VER_MIN` (macro, `ld.c:58`) `#define CFG_CVM_VER_MIN`
- `CFG_CVM_HDR_SIZE` (macro, `ld.c:59`) `#define CFG_CVM_HDR_SIZE`
- `CFG_ELF_PAGE` (macro, `ld.c:60`) `#define CFG_ELF_PAGE`
- `CFG_ELF_HSIZE` (macro, `ld.c:62`) `#define CFG_ELF_HSIZE`

## Internal vs External Edges

- Internal resolved imports (EXTRACTED): 0
- Cross-boundary resolved imports (EXTRACTED): 0

## Connections

- No cross-community bridges recorded. This community is self-contained.

## Risks

- [critical] `test/mutate.sh:69` S001: Command injection via eval — can execute arbitrary commands Fix: Avoid shell=True and string-built commands; use argument arrays and input allowlists.
- [high] `ld.c:1859` (in `sprintf`) C004: Buffer overflow risk: sprintf — use snprintf instead Fix: Use bounded functions (strncpy, snprintf) with explicit sizes and NUL termination.
- [high] `ld.c:1860` (in `sprintf`) C004: Buffer overflow risk: sprintf — use snprintf instead Fix: Use bounded functions (strncpy, snprintf) with explicit sizes and NUL termination.
- [high] `ld.c:1861` (in `sprintf`) C004: Buffer overflow risk: sprintf — use snprintf instead Fix: Use bounded functions (strncpy, snprintf) with explicit sizes and NUL termination.
- [high] `ld.c:1862` (in `sprintf`) C004: Buffer overflow risk: sprintf — use snprintf instead Fix: Use bounded functions (strncpy, snprintf) with explicit sizes and NUL termination.
- [medium] `test/mutate.sh:16` S003: Command substitution with user input — potential injection Fix: Avoid shell=True and string-built commands; use argument arrays and input allowlists.
- [medium] `test/mutate.sh:61` S003: Command substitution with user input — potential injection Fix: Avoid shell=True and string-built commands; use argument arrays and input allowlists.
- [medium] `test/run_tests.sh:22` S003: Command substitution with user input — potential injection Fix: Avoid shell=True and string-built commands; use argument arrays and input allowlists.

## Open Questions

- Why do 22 file(s) lack file-level docs (e.g. `install.sh`)? What purpose do they serve?
- What would break if the most connected file in root changed?
- Should root be split, given cohesion 1.00?

## Sources

- `app.py`
- `install.sh`
- `ld.c`
- `test/argv.c`
- `test/argv.s`
- `test/asm.c`
- `test/chain.c`
- `test/fib.s`
- `test/fib2.s`
- `test/fib3.s`
- `test/fmt.c`
- `test/fnptr.c`
- `test/globals.c`
- `test/hello.c`
- `test/loop.s`
- `test/movslq.s`
- `test/mutate.sh`
- `test/priv.s`
- `test/run_tests.sh`
- `test/start.s`
- *... and 6 more*
