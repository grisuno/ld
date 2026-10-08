# Concepts

Nouns map atomically to file sets (EXTRACTED); verbs aggregate structural edges (INFERRED).

- `start` | files=10 | mentions=12 | `test/argv.s`, `test/fib.s`, `test/fib2.s`, `test/fib3.s`, `test/loop.s`, `test/movslq.s`, `test/priv.s`, `test/start.s`, `test/t1.s`, `test/w1.s`
- `printf` | files=6 | mentions=6 | `test/asm.c`, `test/fmt.c`, `test/fnptr.c`, `test/globals.c`, `test/sync.c`, `test/udiv.c`
- `run` | files=4 | mentions=12 | `ld.c`, `test/fnptr.c`, `test/mutate.sh`, `test/run_tests.sh`
- `fib` | files=4 | mentions=6 | `test/chain.c`, `test/fib.s`, `test/fib2.s`, `test/fib3.s`
- `name` | files=3 | mentions=11 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh`
- `fmt` | files=3 | mentions=6 | `ld.c`, `test/fmt.c`, `test/run_tests.sh`
- `write` | files=3 | mentions=5 | `ld.c`, `test/argv.c`, `test/w1.c`
- `file` | files=3 | mentions=4 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh`
- `every` | files=3 | mentions=3 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh`
- `elf` | files=2 | mentions=54 | `ld.c`, `test/run_tests.sh`
- `cvm` | files=2 | mentions=25 | `ld.c`, `test/run_tests.sh`
- `exit` | files=2 | mentions=11 | `ld.c`, `test/run_tests.sh`
- `add` | files=2 | mentions=8 | `ld.c`, `test/stdint.c`
- `args` | files=2 | mentions=4 | `ld.c`, `test/run_tests.sh`
- `argv` | files=2 | mentions=4 | `test/argv.c`, `test/argv.s`
- `chain` | files=2 | mentions=4 | `test/chain.c`, `test/run_tests.sh`
- `format` | files=2 | mentions=4 | `test/mutate.sh`, `test/run_tests.sh`
- `mini` | files=2 | mentions=4 | `ld.c`, `test/run_tests.sh`
- `against` | files=2 | mentions=3 | `test/mutate.sh`, `test/run_tests.sh`
- `asm` | files=2 | mentions=3 | `test/asm.c`, `test/run_tests.sh`
- `layout` | files=2 | mentions=3 | `ld.c`, `test/run_tests.sh`
- `runs` | files=2 | mentions=3 | `ld.c`, `test/run_tests.sh`
- `suite` | files=2 | mentions=3 | `test/mutate.sh`, `test/run_tests.sh`
- `udiv` | files=2 | mentions=3 | `ld.c`, `test/udiv.c`
- `bdd` | files=2 | mentions=2 | `test/mutate.sh`, `test/run_tests.sh`
- `build` | files=2 | mentions=2 | `ld.c`, `test/run_tests.sh`
- `copy` | files=2 | mentions=2 | `ld.c`, `test/mutate.sh`
- `first` | files=2 | mentions=2 | `ld.c`, `test/mutate.sh`
- `per` | files=2 | mentions=2 | `ld.c`, `test/run_tests.sh`
- `program` | files=2 | mentions=2 | `test/mutate.sh`, `test/run_tests.sh`

## Dialectic

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
