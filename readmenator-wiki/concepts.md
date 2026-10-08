# Concepts

Second-brain semantic layer: nouns map atomically to file sets (EXTRACTED); verbs aggregate structural edges (INFERRED).

| Concept | Files | Mentions | Top Files |
|---------|-------|----------|-----------|
| `start` | 10 | 12 | `test/argv.s`, `test/fib.s`, `test/fib2.s`, `test/fib3.s`, `test/loop.s` |
| `printf` | 6 | 6 | `test/asm.c`, `test/fmt.c`, `test/fnptr.c`, `test/globals.c`, `test/sync.c` |
| `run` | 4 | 12 | `ld.c`, `test/fnptr.c`, `test/mutate.sh`, `test/run_tests.sh` |
| `fib` | 4 | 6 | `test/chain.c`, `test/fib.s`, `test/fib2.s`, `test/fib3.s` |
| `name` | 3 | 11 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh` |
| `fmt` | 3 | 6 | `ld.c`, `test/fmt.c`, `test/run_tests.sh` |
| `write` | 3 | 5 | `ld.c`, `test/argv.c`, `test/w1.c` |
| `file` | 3 | 4 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh` |
| `every` | 3 | 3 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh` |
| `elf` | 2 | 54 | `ld.c`, `test/run_tests.sh` |
| `cvm` | 2 | 25 | `ld.c`, `test/run_tests.sh` |
| `exit` | 2 | 11 | `ld.c`, `test/run_tests.sh` |
| `add` | 2 | 8 | `ld.c`, `test/stdint.c` |
| `args` | 2 | 4 | `ld.c`, `test/run_tests.sh` |
| `argv` | 2 | 4 | `test/argv.c`, `test/argv.s` |
| `chain` | 2 | 4 | `test/chain.c`, `test/run_tests.sh` |
| `format` | 2 | 4 | `test/mutate.sh`, `test/run_tests.sh` |
| `mini` | 2 | 4 | `ld.c`, `test/run_tests.sh` |
| `against` | 2 | 3 | `test/mutate.sh`, `test/run_tests.sh` |
| `asm` | 2 | 3 | `test/asm.c`, `test/run_tests.sh` |
| `layout` | 2 | 3 | `ld.c`, `test/run_tests.sh` |
| `runs` | 2 | 3 | `ld.c`, `test/run_tests.sh` |
| `suite` | 2 | 3 | `test/mutate.sh`, `test/run_tests.sh` |
| `udiv` | 2 | 3 | `ld.c`, `test/udiv.c` |
| `bdd` | 2 | 2 | `test/mutate.sh`, `test/run_tests.sh` |
| `build` | 2 | 2 | `ld.c`, `test/run_tests.sh` |
| `copy` | 2 | 2 | `ld.c`, `test/mutate.sh` |
| `first` | 2 | 2 | `ld.c`, `test/mutate.sh` |
| `per` | 2 | 2 | `ld.c`, `test/run_tests.sh` |
| `program` | 2 | 2 | `test/mutate.sh`, `test/run_tests.sh` |

## Dialectic Prompts

- Thesis: `against` centralizes 2 files; Antithesis: `bdd` pulls 2 files with 2 shared (Jaccard 1.00); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 2 files; Antithesis: `every` pulls 3 files with 2 shared (Jaccard 0.67); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 2 files; Antithesis: `file` pulls 3 files with 2 shared (Jaccard 0.67); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 2 files; Antithesis: `format` pulls 2 files with 2 shared (Jaccard 1.00); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 2 files; Antithesis: `name` pulls 3 files with 2 shared (Jaccard 0.67); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 2 files; Antithesis: `program` pulls 2 files with 2 shared (Jaccard 1.00); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 2 files; Antithesis: `run` pulls 4 files with 2 shared (Jaccard 0.50); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `against` centralizes 2 files; Antithesis: `suite` pulls 2 files with 2 shared (Jaccard 1.00); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `args` centralizes 2 files; Antithesis: `build` pulls 2 files with 2 shared (Jaccard 1.00); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
- Thesis: `args` centralizes 2 files; Antithesis: `cvm` pulls 2 files with 2 shared (Jaccard 1.00); Synthesis: should they merge, split by layer, or keep `bridges` explicit?
