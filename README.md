# ld — miniGCC assembler and linker

`ld` assembles the x86-64 AT&T dialect emitted by the
[miniGCC](https://github.com/grisuno/miniGCC) C compiler into executable
formats. It is self-contained: no external assembler or linker.

## Related projects

| Repository | Role |
|------------|------|
| [miniGCC](https://github.com/grisuno/miniGCC) | C compiler: C to x86-64 AT&T assembly |
| [ld](https://github.com/grisuno/ld) | this repository: assembly to a Linux ELF or a CVM module |
| [cvm](https://github.com/grisuno/cvm) | the CVM / cvm2 bytecode interpreter |
| [miniOS](https://github.com/grisuno/miniOS) | the kernel that hosts the whole toolchain |

The test suite drives miniGCC and cvm2, so clone them as siblings of this
repository (or point `MINIGCC=` and `CVM2=` at them):

```bash
git clone https://github.com/grisuno/miniGCC ../miniGCC
git clone https://github.com/grisuno/cvm     ../cvm
```

## Formats

| Flag | Output | Runs with |
|------|--------|-----------|
| `-f cvm` (default) | CVM v2 module (`.cvm`) | the cvm2 stack-bytecode interpreter (MiniOS `run file.cvm`) |
| `-f elf` | static PIE `ET_DYN` ELF64 executable | natively on Linux, and inside MiniOS (`run file.elf`) |

## Usage

```
usage: ld [-f cvm|elf] [-o out] [-xstack N] input.s
  -f cvm     CVM v2 module (default)
  -f elf     static PIE Linux ELF executable
  -o out     output path (default: input.cvm / input.elf)
  -xstack N  x86 stack region size for the CVM backend (default 262144)
```

## Example

```
minigcc hello.c > hello.s
ld -f elf hello.s -o hello        # runs on Linux: ./hello
ld hello.s -o hello.cvm           # runs on cvm2 / inside MiniOS
```

`ld` also runs *inside* MiniOS, where it closes the toolchain loop without
leaving the machine:

```
miniOS> edit p.c
miniOS> run minigcc.o p.c > p.s
miniOS> run ld.o -f elf -o p.elf p.s
miniOS> run p.elf
```

## ELF details

The ELF backend encodes x86-64 machine code directly. Output is a static,
position-independent `ET_DYN` ELF64 with two `PT_LOAD` segments
(RX text+rodata, RW data+bss), no runtime relocations. A built-in stub
layer provides the mini libc used by miniGCC programs:
`write read exit exit_group open close lseek brk malloc free memcpy memset
memmove memcmp strlen strcmp strncmp strcpy strncpy strchr` plus stdio
`fopen fclose fread fwrite fseek ftell rewind fputc fputs fprintf printf
sprintf puts putchar snprintf` and the `stdin stdout stderr` stream slots.
Any other undefined symbol is a hard error.

The built-in formatter understands `%s %d %o %%` with width, zero-fill and
the `l` modifier. It truncates rather than reporting a would-be length, so
the byte count handed to `write` can never exceed the format buffer. All four
entry points share one core that reads arguments from an array; how many
variadic arguments each accepts follows from the argument registers it has
left over: `printf` 5, `fprintf` and `sprintf` 4, `snprintf` 3. `%d` consumes
its argument as 32-bit signed (glibc parity); the neg flag lives in one
frame slot shared by set, test and clear.

## Instruction coverage

Beyond everything miniGCC generates, the ELF backend encodes the raw
templates miniGCC passes through from basic `asm`: `nop cli sti hlt rdtsc
lock xaddq xchgq incq decq mfence`, with `disp32` fixups that account for
trailing immediates (`mov`/`alu`/`cmp` immediate-to-memory forms carry the
imm after the disp). Anything without a machine meaning in the target is a
fail-closed `unsupported instruction` diagnostic with file and line — the CVM
backend has no translation for privileged or lock-prefixed ops, so a module
using them is rejected at assembly time instead of being silently
miscompiled. Privileged load/store forms (`in out lidt lgdt ltr mov-cr`)
are future work.

## Self-hosting miniGCC

`ld` is the only assembler and linker miniGCC needs to bootstrap itself:
miniGCC compiles its own source, `ld` links every generation, and the chain
must reach the bootstrap fixed point. Two suites pin this milestone:

```bash
../miniGCC/test_ld_selfhost.sh   # GNU-free chain: fixed point + behaviour
RUN_SLOW=1 tests/run_tests.sh    # this repository's suite, self-host included
```

The same chain builds `minigcc.elf` for the MiniOS ramdisk (`make selfhost`
in the miniOS repository): a compiler that is its own child, running inside
the OS it was built with.

## Development

See `CLAUDE.md` for the full contract (SDD + TDD + BDD + mutation testing).

```
gcc -std=c99 -Wall -Wextra -Wpedantic -O2 -o ld ld.c
make test          # same, then: tests/run_tests.sh (cvm + elf + minigcc chain)
tests/run_tests.sh     # behavioral suite: cvm + elf + minigcc chain
tests/mutate.sh        # mutation testing (every mutant must be killed)
```

The suite assembles every fixture to both formats and runs each (cvm2 for
`.cvm`, natively for `.elf`), including a chain that compiles C with miniGCC
first (`chain`, `fmt`, `globals`, `asm`, `stdint`). privileged `cli/sti/hlt`
assembles for ELF but is rejected fail-closed for CVM, and the same holds
for the `lock`-prefixed atomics (`sync` case).

Fixtures are copied into a scratch directory and every tool runs against the
copies: mutation testing deliberately builds broken binaries from this suite,
and a mutant that mishandles an argument path must not be able to reach the
pristine sources.

<!-- readmenator-kb-link -->
## Knowledge Base

This project has been analyzed by [ReadMenator](https://github.com/grisuno/ReadMenator),
a zero-token polyglot static analysis tool. Analysis outputs are available:

- **[KNOWLEDGE_BASE.md](./KNOWLEDGE_BASE.md)** -- Full architecture reference with all
  classes, functions, imports, dependency graphs, UML class diagrams, security
  audit findings, community analysis, and more.
- **[readmenator-agent/](./readmenator-agent/)** -- Agent-friendly, grep-optimized index.
  - `INDEX.md` -- Quick reference: what each file does
  - `API.md` -- Public function contracts
  - `GOTCHAS.md` -- Change warnings
  - `SECURITY.md` -- Findings by severity

AI agents: Read `readmenator-agent/INDEX.md` for fast project context.
Developers: Read `KNOWLEDGE_BASE.md` for full architecture reference.
<!-- /readmenator-kb-link -->

