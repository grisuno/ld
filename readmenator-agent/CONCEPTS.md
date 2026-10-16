# Concepts

Nouns map atomically to file sets (EXTRACTED); verbs aggregate structural edges (INFERRED).

- `start` | files=11 | mentions=13 | `test/argv.s`, `test/fib.s`, `test/fib2.s`, `test/fib3.s`, `test/loop.s`, `test/movslq.s`, `test/priv.s`, `test/run_tests.sh`, `test/start.s`, `test/t1.s`
- `printf` | files=6 | mentions=6 | `test/asm.c`, `test/fmt.c`, `test/fnptr.c`, `test/globals.c`, `test/sync.c`, `test/udiv.c`
- `run` | files=5 | mentions=24 | `ld.c`, `test/dynlib_host.c`, `test/fnptr.c`, `test/mutate.sh`, `test/run_tests.sh`
- `fib` | files=4 | mentions=6 | `test/chain.c`, `test/fib.s`, `test/fib2.s`, `test/fib3.s`
- `name` | files=3 | mentions=19 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh`
- `shared` | files=3 | mentions=19 | `ld.c`, `test/dynlib_host.c`, `test/run_tests.sh`
- `write` | files=3 | mentions=16 | `ld.c`, `test/argv.c`, `test/w1.c`
- `every` | files=3 | mentions=9 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh`
- `host` | files=3 | mentions=8 | `ld.c`, `test/dynlib_host.c`, `test/run_tests.sh`
- `chain` | files=3 | mentions=7 | `ld.c`, `test/chain.c`, `test/run_tests.sh`
- `dynlib` | files=3 | mentions=7 | `test/dynlib.c`, `test/dynlib_host.c`, `test/run_tests.sh`
- `fmt` | files=3 | mentions=6 | `ld.c`, `test/fmt.c`, `test/run_tests.sh`
- `against` | files=3 | mentions=5 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh`
- `file` | files=3 | mentions=5 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh`
- `program` | files=3 | mentions=5 | `ld.c`, `test/mutate.sh`, `test/run_tests.sh`
- `dlopen` | files=3 | mentions=4 | `ld.c`, `test/dynlib_host.c`, `test/run_tests.sh`
- `dynadd` | files=3 | mentions=3 | `test/dynlib.c`, `test/dynuse.c`, `test/run_tests.sh`
- `dynlen` | files=3 | mentions=3 | `test/dynlib.c`, `test/dynuse.c`, `test/run_tests.sh`
- `elf` | files=2 | mentions=127 | `ld.c`, `test/run_tests.sh`
- `cvm` | files=2 | mentions=25 | `ld.c`, `test/run_tests.sh`
- `import` | files=2 | mentions=16 | `ld.c`, `test/run_tests.sh`
- `rela` | files=2 | mentions=14 | `ld.c`, `test/run_tests.sh`
- `exit` | files=2 | mentions=13 | `ld.c`, `test/run_tests.sh`
- `dynamic` | files=2 | mentions=11 | `ld.c`, `test/run_tests.sh`
- `library` | files=2 | mentions=11 | `ld.c`, `test/run_tests.sh`
- `stub` | files=2 | mentions=11 | `ld.c`, `test/run_tests.sh`
- `plt` | files=2 | mentions=10 | `ld.c`, `test/run_tests.sh`
- `soname` | files=2 | mentions=9 | `ld.c`, `test/run_tests.sh`
- `table` | files=2 | mentions=9 | `ld.c`, `test/run_tests.sh`
- `through` | files=2 | mentions=9 | `ld.c`, `test/run_tests.sh`
- `add` | files=2 | mentions=8 | `ld.c`, `test/stdint.c`
- `call` | files=2 | mentions=8 | `ld.c`, `test/run_tests.sh`
- `code` | files=2 | mentions=8 | `ld.c`, `test/run_tests.sh`
- `function` | files=2 | mentions=8 | `ld.c`, `test/run_tests.sh`
- `got` | files=2 | mentions=8 | `ld.c`, `test/run_tests.sh`
- `local` | files=2 | mentions=8 | `ld.c`, `test/run_tests.sh`
- `plus` | files=2 | mentions=8 | `ld.c`, `test/run_tests.sh`
- `layout` | files=2 | mentions=7 | `ld.c`, `test/run_tests.sh`
- `never` | files=2 | mentions=7 | `ld.c`, `test/run_tests.sh`
- `per` | files=2 | mentions=7 | `ld.c`, `test/run_tests.sh`
- `tables` | files=2 | mentions=7 | `ld.c`, `test/run_tests.sh`
- `build` | files=2 | mentions=5 | `ld.c`, `test/run_tests.sh`
- `built` | files=2 | mentions=5 | `ld.c`, `test/run_tests.sh`
- `calls` | files=2 | mentions=5 | `ld.c`, `test/run_tests.sh`
- `dynuse` | files=2 | mentions=5 | `test/dynuse.c`, `test/run_tests.sh`
- `mini` | files=2 | mentions=5 | `ld.c`, `test/run_tests.sh`
- `must` | files=2 | mentions=5 | `ld.c`, `test/run_tests.sh`
- `names` | files=2 | mentions=5 | `ld.c`, `test/run_tests.sh`
- `needed` | files=2 | mentions=5 | `ld.c`, `test/run_tests.sh`
- `runtime` | files=2 | mentions=5 | `ld.c`, `test/run_tests.sh`

## Dialectic

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
