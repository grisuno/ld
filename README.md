# ld — miniGCC assembler and linker

`ld` assembles the x86-64 AT&T dialect emitted by the
[miniGCC](https://example.invalid/miniGCC) C compiler into executable
formats. It is self-contained: no external assembler or linker.

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
left over: `printf` 5, `fprintf` and `sprintf` 4, `snprintf` 3.

## Development

See `CLAUDE.md` for the full contract (SDD + TDD + BDD + mutation testing).

```
gcc -std=c99 -Wall -Wextra -Wpedantic -O2 -o ld ld.c
tests/run_tests.sh     # behavioral suite: cvm + elf + minigcc chain
tests/mutate.sh        # mutation testing (every mutant must be killed)
```

Fixtures are copied into a scratch directory and every tool runs against the
copies: mutation testing deliberately builds broken binaries from this suite,
and a mutant that mishandles an argument path must not be able to reach the
pristine sources.
