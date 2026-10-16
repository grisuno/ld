# Concepts

Second-brain semantic layer: nouns map atomically to file sets (EXTRACTED); verbs aggregate structural edges (INFERRED).

| Concept | Files | Mentions | Top Files |
|---------|-------|----------|-----------|
| `start` | 11 | 13 | `test/argv.s`, `test/fib.s`, `test/fib2.s`, `test/fib3.s`, `test/loop.s` |
| `printf` | 6 | 6 | `test/asm.c`, `test/fmt.c`, `test/fnptr.c`, `test/globals.c`, `test/sync.c` |
| `run` | 5 | 24 | `ld.c`, `test/dynlib_host.c`, `test/fnptr.c`, `test/mutate.sh`, `test/run_tests.sh` |
| `fib` | 4 | 6 | `test/chain.c`, `test/fib.s`, `test/fib2.s`, `test/fib3.s` |
| `name` | 3 | 19 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh` |
| `shared` | 3 | 19 | `ld.c`, `test/dynlib_host.c`, `test/run_tests.sh` |
| `write` | 3 | 16 | `ld.c`, `test/argv.c`, `test/w1.c` |
| `every` | 3 | 9 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh` |
| `host` | 3 | 8 | `ld.c`, `test/dynlib_host.c`, `test/run_tests.sh` |
| `chain` | 3 | 7 | `ld.c`, `test/chain.c`, `test/run_tests.sh` |
| `dynlib` | 3 | 7 | `test/dynlib.c`, `test/dynlib_host.c`, `test/run_tests.sh` |
| `fmt` | 3 | 6 | `ld.c`, `test/fmt.c`, `test/run_tests.sh` |
| `against` | 3 | 5 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh` |
| `file` | 3 | 5 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh` |
| `program` | 3 | 5 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh` |
| `dlopen` | 3 | 4 | `ld.c`, `test/dynlib_host.c`, `test/run_tests.sh` |
| `dynadd` | 3 | 3 | `test/dynlib.c`, `test/dynuse.c`, `test/run_tests.sh` |
| `dynlen` | 3 | 3 | `test/dynlib.c`, `test/dynuse.c`, `test/run_tests.sh` |
| `elf` | 2 | 127 | `ld.c`, `test/run_tests.sh` |
| `cvm` | 2 | 25 | `ld.c`, `test/run_tests.sh` |
| `import` | 2 | 16 | `ld.c`, `test/run_tests.sh` |
| `rela` | 2 | 14 | `ld.c`, `test/run_tests.sh` |
| `exit` | 2 | 13 | `ld.c`, `test/run_tests.sh` |
| `dynamic` | 2 | 11 | `ld.c`, `test/run_tests.sh` |
| `library` | 2 | 11 | `ld.c`, `test/run_tests.sh` |
| `stub` | 2 | 11 | `ld.c`, `test/run_tests.sh` |
| `plt` | 2 | 10 | `ld.c`, `test/run_tests.sh` |
| `soname` | 2 | 9 | `ld.c`, `test/run_tests.sh` |
| `table` | 2 | 9 | `ld.c`, `test/run_tests.sh` |
| `through` | 2 | 9 | `ld.c`, `test/run_tests.sh` |
| `add` | 2 | 8 | `ld.c`, `test/stdint.c` |
| `call` | 2 | 8 | `ld.c`, `test/run_tests.sh` |
| `code` | 2 | 8 | `ld.c`, `test/run_tests.sh` |
| `function` | 2 | 8 | `ld.c`, `test/run_tests.sh` |
| `got` | 2 | 8 | `ld.c`, `test/run_tests.sh` |
| `local` | 2 | 8 | `ld.c`, `test/run_tests.sh` |
| `plus` | 2 | 8 | `ld.c`, `test/run_tests.sh` |
| `layout` | 2 | 7 | `ld.c`, `test/run_tests.sh` |
| `never` | 2 | 7 | `ld.c`, `test/run_tests.sh` |
| `per` | 2 | 7 | `ld.c`, `test/run_tests.sh` |
| `tables` | 2 | 7 | `ld.c`, `test/run_tests.sh` |
| `build` | 2 | 5 | `ld.c`, `test/run_tests.sh` |
| `built` | 2 | 5 | `ld.c`, `test/run_tests.sh` |
| `calls` | 2 | 5 | `ld.c`, `test/run_tests.sh` |
| `dynuse` | 2 | 5 | `test/dynuse.c`, `test/run_tests.sh` |
| `mini` | 2 | 5 | `ld.c`, `test/run_tests.sh` |
| `must` | 2 | 5 | `ld.c`, `test/run_tests.sh` |
| `names` | 2 | 5 | `ld.c`, `test/run_tests.sh` |
| `needed` | 2 | 5 | `ld.c`, `test/run_tests.sh` |
| `runtime` | 2 | 5 | `ld.c`, `test/run_tests.sh` |

## Dialectic Prompts

- Thesis: `against` centralizes 3 files; Antithesis: `build` pulls 2 files with 2 shared (Jaccard 0.67); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 3 files; Antithesis: `built` pulls 2 files with 2 shared (Jaccard 0.67); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 3 files; Antithesis: `call` pulls 2 files with 2 shared (Jaccard 0.67); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 3 files; Antithesis: `calls` pulls 2 files with 2 shared (Jaccard 0.67); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 3 files; Antithesis: `chain` pulls 3 files with 2 shared (Jaccard 0.50); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 3 files; Antithesis: `code` pulls 2 files with 2 shared (Jaccard 0.67); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 3 files; Antithesis: `cvm` pulls 2 files with 2 shared (Jaccard 0.67); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 3 files; Antithesis: `dlopen` pulls 3 files with 2 shared (Jaccard 0.50); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 3 files; Antithesis: `dynamic` pulls 2 files with 2 shared (Jaccard 0.67); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 3 files; Antithesis: `elf` pulls 2 files with 2 shared (Jaccard 0.67); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
