# ld Toolchain Contract

## Purpose
`ld` is the assembler and linker for the miniGCC toolchain. It consumes the
x86-64 AT&T assembly dialect emitted by miniGCC and produces one of two
executable formats:

1. **CVM modules** (`.cvm`) - stack bytecode executed by the cvm2 interpreter
   (used inside MiniOS via `cvm.o`).
2. **Linux ELF executables** (`.elf`) - static, position-independent
   `ET_DYN` ELF64 binaries. They run natively on Linux and inside MiniOS
   through the `run` command (load_exec_elf + syscall ABI).

Pipeline:
```
C source -> miniGCC -> AT&T asm -> ld (-f cvm | -f elf) -> .cvm | .elf
```

The same pipeline runs inside MiniOS, where `ld` is loaded from the ramdisk
as `ld.o`. That is why the tool must stay modest in memory: its tables are
heap allocated and grown on demand rather than statically reserved for the
worst case.

## Architecture
Single file per contract: `ld.c`. DRY + SOLID.

Layers:
- **Config section**: every constant, size limit, opcode and format value is
  a named constant. No magic numbers anywhere else.
- **Shared front end**: tokenizer/parser for the miniGCC dialect
  (registers, immediates, memory operands, rip-relative symbols), directives
  (`.section .text|.bss|.data|.rodata` and the bare `.text|.bss|.data|.rodata`
  forms miniGCC emits, `.globl`, `.weak`, `.space`, `.zero`,
  `.comm`, `.asciz`, `.string`, `.ascii`, `.byte`, `.word`, `.long`, `.quad`,
  `.align`, `.balign`, `.p2align`), symbol table scan.
- **CVM backend**: emits the cvm2 v2 module format (magic `CVM4`, 40-byte
  header, func/global/native tables, bytecode, RLE data region, string pool).
- **ELF backend**: x86-64 machine code encoder + ELF64 writer + built-in
  syscall stub layer (mini libc). No external assembler or linker needed.

## ELF Backend Contract
- Format: `ET_DYN` (static PIE), `EM_X86_64`, little-endian, ELF64.
- Two `PT_LOAD` segments: RX (text + rodata) and RW (data + bss).
  `p_offset == p_vaddr`, `p_align = 4096`.
- Entry point: `_start` (falls back to `main`; error if neither exists).
- All code is position independent: branches and `sym(%rip)` operands are
  `disp32` fixups resolved at assembly time. No runtime relocations.
  A fixup records `addend` trailing bytes when the disp32 is not the last
  field of its instruction (`mov`/`alu`/`cmp` immediate-to-memory forms
  carry the imm after the disp); resolution subtracts `pos + 4 + addend`.
- Raw machine passthrough (miniGCC basic `asm`): the ELF backend encodes
  `nop cli sti hlt rdtsc lock xaddq xchgq incq decq mfence` alongside the
  compiler-generated set, so a template miniGCC passes verbatim always
  assembles. Anything without a machine meaning in the target stays a
  fail-closed `unsupported instruction` diagnostic, never a silent
  miscompile: the CVM backend has no translation for privileged or
  lock-prefixed ops (`cli sti hlt lock`), so a module using them is
  rejected at assembly time with file and line. Privileged
  load/store forms (`in out lidt lgdt ltr mov-cr`) are future work: they
  need EA-operand encodings the current Format layer does not carry.
- Data directives (`.byte .word .long .quad`) take integer literals only. A
  symbol operand is deliberately rejected: storing an address at rest would
  need either a load-time relocation, which a static PIE never receives, or a
  self-relocating startup, and both contradict the guarantee above. Producers
  emit a zero slot and fill it in with rip-relative code instead, which is
  what miniGCC does for `char *p = "literal";`.
- Built-in stubs (auto-included, dead code eliminated by the format):
  syscall layer `write read exit exit_group open close lseek brk`,
  memory `malloc free memcpy memset memmove memcmp` (`malloc` is a
  brk-based bump allocator; `free` is a no-op),
  strings `strlen strcmp strncmp strcpy strncpy strchr`,
  stdio `fopen fclose fread fwrite fseek ftell rewind fputc fputs
  fprintf printf sprintf puts putchar snprintf` plus the `stdin stdout
  stderr` stream slots. `FILE*` is either a raw fd (0-2) or a pointer to
  an internal stream slot. The built-in formatter supports `%s %d %o %%`
  with width, zero-fill and the `l` modifier. Any other undefined symbol
  is a hard link error (never silently resolved).
- Fixed-size encodings per instruction guarantee a single encode pass.

## Function Pointer Contract
- Source form (miniGCC lowering): `leaq fn(%rip), %reg` takes a function
  address; `call *%reg` calls through it. The operand parser maps `*%reg`
  to a dedicated indirect kind, never to a symbol.
- ELF backend: `call *%reg` encodes `FF /2` (modrm `mod=11, reg=010`).
  `leaq fn(%rip), %reg` of a function symbol is an ordinary rip-relative
  fixup to the function address. No new relocations: the value is an
  absolute code address inside the same static PIE, resolved at link time.
- CVM backend: a function value is the callee's index in the module func
  table (same domain as the `OP_CALL` immediate). `leaq fn(%rip), %reg`
  pushes that index; `call *%reg` spills the six argument registers to the
  `GSLOT_ARGS` area exactly like a direct call, pushes the index, and emits
  `OP_CALL_INDIRECT` (0x63, no immediate: target is popped from the stack).
  The interpreter validates the index (`BAD_FUNC` fail-closed) and builds
  the frame exactly like `OP_CALL` with zero stack args. Taking the address
  of anything that is not a defined function is a link error, never a
  silent zero.
- Coverage: `test/fnptr.c` goes through miniGCC first (`run_chain fnptr`)
  and must produce identical stdout on `.cvm` and `.elf` (see
  `test/fnptr.expect`).

## Unsigned Division Contract
- Source form (miniGCC lowering): `divq`/`divl` (with a preceding
  `xorl %edx,%edx`) means unsigned; `idivq`/`idivl` (with `cqto`/`cltd`)
  means signed. The two pairs never mix.
- ELF backend: `divq` encodes `F7 /6` (64-bit), `divl` the 32-bit form,
  `idivl` the 32-bit `F7 /7` (previously missing: 32-bit compound
  assignment did not assemble). `xorl` is the generic ALU path.
- CVM backend: `divq` maps to `OP_UDIV` (0x26) / `OP_UMOD` (0x27),
  `divl` to the same pair with 32-bit masking (matching the `idivl`
  shape); `idivq`/`idivl` keep the signed `OP_DIV`/`OP_MOD`. The
  interpreter, JIT (`DIV` + zeroed `edx` instead of `CQO`) and validator
  treat all four identically except for signedness; divide-by-zero is
  fail-closed in both.
- Coverage: `test/udiv.c` goes through miniGCC first (`run_chain udiv`)
  with values above 2^63 (built as `4000000000ul * 4000000000ul`, so no
  reliance on literal folding) and must produce identical stdout on
  `.cvm` (interpreter and `--jit`) and `.elf` (see `test/udiv.expect`).

## Mini libc Contract (ELF backend)
- One formatter core, `.Lstub_vfmt(out, size, fmt, args)`, reads its variadic
  arguments from an array rather than from registers. `printf`, `fprintf`,
  `sprintf` and `snprintf` are thin wrappers that spill their register
  arguments into that array. The earlier design had each wrapper shuffle
  registers into the core's positions, which is how `printf` came to pass its
  first variadic argument as the format string; the array removes the whole
  class of defect. Capacity follows from how many argument registers each
  entry point has left: `printf` 5, `fprintf` and `sprintf` 4, `snprintf` 3.
  Every slot beyond the last argument is zeroed, so an over-long format
  never reads a live register or an adjacent frame slot.
- `%d` reads its argument as 32-bit signed (glibc parity: `movslq` at the
  conversion site); the neg flag lives at frame+8 and is set, tested and
  cleared through that one slot (a clear addressed past the digit buffer
  made every argument after a negative print negated).
- Any change to the formatter must be covered by `tests/fmt.c`, which pins
  each conversion, the width and zero-fill flags, and the argument capacity.
- `snprintf` truncates: it returns the number of characters actually written,
  not the C99 would-be length. `printf`/`fprintf` pass that return straight to
  `write`, so the bound is what keeps the formatter from reading past
  `__fmt_buf`. A C99 return value would reintroduce that overread.
- `printf`, `fprintf` and `sprintf` return the character count; `puts` returns
  zero on success.
- The fixup table is heap allocated and grown on demand up to
  `CFG_MAX_FIXUPS`. It is deliberately not a static worst-case array: that
  reservation dominated the image and put `ld` out of reach of hosts with a
  small heap, which is exactly where it needs to run (MiniOS).

## Development Methodology (SDD + TDD + BDD)
1. **SDD**: every feature begins with a spec in this file.
2. **TDD**: add a failing test case first (`tests/`), then implement.
3. **BDD**: `tests/run_tests.sh` assembles each fixture to `.cvm` AND `.elf`,
   runs the `.cvm` on the cvm2 interpreter and the `.elf` natively on Linux,
   and diffs stdout + exit codes against `tests/*.expect`. `.s` fixtures are
   assembled directly; `.c` fixtures (`chain.c`, `fmt.c`) go through miniGCC
   first, which is what keeps the compiler-to-linker contract honest.
    Fixtures are copied into the scratch directory and every tool runs against
    the copies: the suite is also the input to mutation testing, and a mutant
    that mishandles an argument path must never reach the pristine sources.
    With `RUN_SLOW=1` (which `mutate.sh` sets) the suite also runs the full
    self-host chain: miniGCC compiles its own source and every generation is
    linked by `ld`, which must reach the bootstrap fixed point and still
    reproduce `tests/selfhost.expect`.
4. **Mutation testing**: `tests/mutate.sh` injects one-line mutations into
   `ld.c` (opcode swaps, format constants, section flags, libc register
   shuffles), rebuilds, and runs the suite. A mutant that passes the suite is
   a test gap and must be fixed by adding a test, not by deleting the mutant.
   A mutation whose expression matches nothing is reported as *broken*, never
   as surviving: `sed -i` exits zero when it changes nothing, so an
   unverified mutation would rebuild pristine sources and be misread as a gap.
5. **Boy Scout rule**: fix any technical debt or security issue found; never
   out of scope.

## Validation Gate (must pass before any commit)
```bash
make test          # builds ld, then tests/run_tests.sh (cvm + elf + minigcc chain)
tests/mutate.sh                                           # every mutant killed
```

## Code Standards
- English only, no emojis, no inline comments; file-header docstring only.
- Production-ready, secure code: no placeholders, no simplifications.
- Bounds-checked string operations; explicit overflow checks on all
  buffer growth and size calculations (reject before `realloc` overflow).
- Input validated at every entry point; error paths leak nothing.
- No absolute filesystem paths; no hardcoded host assumptions.

## Security Requirements (Non-Negotiable)
- All parse buffers bounded (`LINE_MAX`, `NAME_MAX`).
- No format string vulnerabilities; `%s` with user data only via `fputs`.
- Symbol and fixup tables bounds checked; duplicates rejected.
- Integer literals parsed with overflow detection.
- ELF writer never emits segments outside the intended file.

## CLI Contract
```
usage: ld [-f cvm|elf] [-o out] [-xstack N] input.s
  -f cvm     CVM v2 module (default)
  -f elf     static PIE Linux ELF executable
  -o out     output path (default: input.cvm / input.elf)
  -xstack N  x86 stack region size for the CVM backend (default 262144)
```

## Compatibility
- The CVM backend stores the x86 stack region's offset in the module ABI
  area (`stack_base`, data offset 96) and reserves the argv array area above
  the stack region, so argument passing can never overwrite globals or
  string blobs. Modules need the matching cvm2 interpreter; older modules
  without the stored offset are still loaded by the interpreter (it falls
  back to the fixed 96-byte layout), but a module produced by a new `ld`
  needs a new interpreter and vice versa. Magic is `CVM\x04`, version 1.0.
- ELF backend output runs on Linux kernels (static PIE) and inside MiniOS
  (`load_exec_elf`); it makes only syscalls that MiniOS implements
  (read/write/writev/open/openat/close/lseek/brk/mmap/munmap/exit/exit_group).

<!-- readmenator-agent-kb-link -->
## Project Knowledge Base (MUST read before coding)

MUST read `readmenator-agent/MANIFEST.json` first for freshness. NEVER `glob src/**` before `grep` in `readmenator-agent/INDEX.md`.

Orient first: `ls *.md readmenator-agent/ readmenator-wiki/` (docs only, ignore build noise). Then follow the workflow below.

Workflow: 1) `grep -n '<keyword>' readmenator-agent/INDEX.md readmenator-agent/SYMBOLS.md` 2) `cat readmenator-agent/KB_<subsystem>.md` 3) check `readmenator-agent/GOTCHAS.md` before editing.

This project contains analysis outputs generated by [ReadMenator](https://github.com/grisuno/ReadMenator), a zero-token polyglot static analysis tool.

**For humans:** Read `KNOWLEDGE_BASE.md` -- full architecture reference.

**For agents:** Read `readmenator-agent/INDEX.md` -- grep-friendly index.
  - `readmenator-agent/MANIFEST.json` -- freshness + entrypoints (start here)
  - `readmenator-agent/INDEX.md` -- file -> purpose map
  - `readmenator-agent/SYMBOLS.md` -- symbol index (grep-friendly)
  - `readmenator-agent/API.md` -- public functions + contracts
  - `readmenator-agent/GOTCHAS.md` -- "don't change X because Y breaks"
  - `readmenator-agent/KB_<subsystem>.md` -- per-subsystem context (grep-friendly)
  - `readmenator-agent/SECURITY.md` -- findings by severity
  - `readmenator-agent/recipes/*.md` -- actionable task blocks
**For agents (big picture first):** Read `readmenator-wiki/index.md` --
  overview, reading order, god nodes, connections. Then use the files above.

If MANIFEST date/commit is stale vs `git HEAD`, regenerate:

    pip install readmenator && readmenator . --rebuild
<!-- /readmenator-agent-kb-link -->
