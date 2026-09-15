# Subsystem: test

## test/argv.c
- Layer: testing
- Language: c
- Symbols:
  - `main` (function, line 2) `int main(int argc, char **argv)`
  - `write` (function, line 1) `int write(int fd, char *buf, int n);`

## test/argv.s
- Layer: testing
- Language: s
- Symbols:
  - `main` (function, line 3)
  - `_start` (function, line 98)

## test/asm.c
- Layer: testing
- Language: c
- Symbols:
  - `main` (function, line 4) `int main(void)`
  - `printf` (function, line 1) `int printf();`
  - `volatile` (function, line 6) `__asm__ volatile("nop");`
  - `__asm` (function, line 7) `__asm("nop");`

## test/chain.c
- Layer: testing
- Language: c
- Symbols:
  - `fib` (function, line 1) `int fib(int n)`
  - `main` (function, line 5) `int main(void)`

## test/fib.s
- Layer: testing
- Language: s
- Symbols:
  - `fib` (function, line 3)
  - `main` (function, line 61)
  - `_start` (function, line 82)

## test/fib2.s
- Layer: testing
- Language: s
- Symbols:
  - `fib` (function, line 3)
  - `main` (function, line 61)
  - `_start` (function, line 82)

## test/fib3.s
- Layer: testing
- Language: s
- Symbols:
  - `fib` (function, line 3)
  - `main` (function, line 61)
  - `_start` (function, line 82)

## test/fmt.c
- Layer: testing
- Language: c
- Symbols:
  - `main` (function, line 2) `int main(void)`
  - `printf` (function, line 1) `int printf();`

## test/globals.c
- Layer: testing
- Language: c
- Symbols:
  - `main` (function, line 10) `int main(void)`
  - `printf` (function, line 1) `int printf();`
  - `puts` (function, line 2) `int puts(char *s);`

## test/hello.c
- Layer: testing
- Language: c
- Symbols:
  - `main` (function, line 1) `int main(void)`

## test/loop.s
- Layer: testing
- Language: s
- Symbols:
  - `main` (function, line 3)
  - `_start` (function, line 21)

## test/movslq.s
- Layer: testing
- Language: s
- Symbols:
  - `main` (function, line 3)
  - `_start` (function, line 14)

## test/mutate.sh
- Layer: testing
- Doc: Mutation testing for ld: every mutant in MUTATIONS is injected into a private copy of ld.c, rebuilt, and run against the
- Language: sh

## test/priv.s
- Layer: testing
- Language: s
- Symbols:
  - `main` (function, line 3)
  - `_start` (function, line 12)

## test/run_tests.sh
- Layer: testing
- Doc: BDD suite for the ld tool (miniGCC asm -> CVM / ELF). Every fixture is assembled to BOTH formats; the .cvm runs on the c
- Language: sh
- Symbols:
  - `run_prog` (function, line 40)
  - `note_fail` (function, line 47)
  - `check` (function, line 53)
  - `run_fixture` (function, line 78)
  - `run_chain` (function, line 107)
  - `elf_structure_check` (function, line 139)

## test/start.s
- Layer: testing
- Language: s
- Symbols:
  - `main` (function, line 3)
  - `_start` (function, line 8)

## test/t1.c
- Layer: testing
- Language: c
- Symbols:
  - `main` (function, line 1) `int main(void)`

## test/t1.s
- Layer: testing
- Language: s
- Symbols:
  - `main` (function, line 3)
  - `_start` (function, line 14)

## test/w1.c
- Layer: testing
- Language: c
- Symbols:
  - `main` (function, line 2) `int main(void)`
  - `write` (function, line 1) `int write(int fd, char *buf, int n);`

## test/w1.s
- Layer: testing
- Language: s
- Symbols:
  - `main` (function, line 3)
  - `_start` (function, line 37)
