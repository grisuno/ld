# API

## ld.c

### die `static void die(const char *msg)`
- Defined: `ld.c:339`
- Doc: ================================================================ Diagnostics and memory * ==============================

### breserve `static void breserve(unsigned char **p, long *cap, long need)`
- Defined: `ld.c:348`

### fixup_reserve `static void fixup_reserve(void)`
- Defined: `ld.c:368`
- Doc: Grow the fixup table so that one more entry fits. The table is heap allocated rather than statically reserved: a worst-c

### creserve `static void creserve(char **p, long *cap, long need)`
- Defined: `ld.c:383`

### parse_num `static long parse_num(const char *s)`
- Defined: `ld.c:399`

### trim `static char *trim(char *s)`
- Defined: `ld.c:431`

### strip_comment `static void strip_comment(char *s)`
- Defined: `ld.c:442`
- Doc: Truncate at the first '#' outside a double-quoted string: '#' is the * comment character, but a string literal may carry

### name_copy `static void name_copy(char *dst, const char *src)`
- Defined: `ld.c:456`

### split_word `static void split_word(char *line, char *word, long wcap, char **rest)`
- Defined: `ld.c:463`

### hexval `static int hexval(char c)`
- Defined: `ld.c:476`

### find_sym `static int find_sym(const char *name)`
- Defined: `ld.c:483`

### add_sym `static int add_sym(const char *name, int kind, int sec)`
- Defined: `ld.c:489`

### parse_reg `static int parse_reg(const char *s, int *reg, int *sz)`
- Defined: `ld.c:525`

### parse_mem `static void parse_mem(char *s, Op *op)`
- Defined: `ld.c:537`

### parse_operand `static void parse_operand(char *s, Op *op)`
- Defined: `ld.c:590`

### split_operands `static int split_operands(char *rest, char *o1, char *o2)`
- Defined: `ld.c:621`

### ls_open_file `static void ls_open_file(LineSrc *s, const char *path)`
- Defined: `ld.c:646`

### ls_open_mem `static void ls_open_mem(LineSrc *s, const char *text)`
- Defined: `ld.c:658`

### ls_getline `static int ls_getline(LineSrc *s, char *buf, size_t n)`
- Defined: `ld.c:666`

### ls_close `static void ls_close(LineSrc *s)`
- Defined: `ld.c:682`

### data_put `static void data_put(unsigned char b)`
- Defined: `ld.c:690`
- Doc: ================================================================ Data region helpers * =================================

### data_fill `static void data_fill(long n, unsigned char b)`
- Defined: `ld.c:695`

### data_align `static void data_align(long a)`
- Defined: `ld.c:702`

### blob_put `static void blob_put(unsigned char b)`
- Defined: `ld.c:706`

### blob_append_str `static void blob_append_str(char *s)`
- Defined: `ld.c:711`

### set_section `static int set_section(char *line)`
- Defined: `ld.c:750`

### scan_directive `static void scan_directive(char *line, int *section, int pending_global,
                        ...`
- Defined: `ld.c:773`
- Doc: ================================================================ Shared scan pass * ====================================

### scan_src `static void scan_src(LineSrc *src, int from_stubs)`
- Defined: `ld.c:869`

### cvm_find_func `static int cvm_find_func(const char *name)`
- Defined: `ld.c:940`
- Doc: ================================================================ CVM backend * =========================================

### cvm_find_global `static int cvm_find_global(const char *name)`
- Defined: `ld.c:946`

### cvm_find_blob `static int cvm_find_blob(const char *name)`
- Defined: `ld.c:952`

### cvm_find_nat `static int cvm_find_nat(const char *name)`
- Defined: `ld.c:958`

### cvm_add_nat `static int cvm_add_nat(const char *name)`
- Defined: `ld.c:964`

### e1 `static void e1(int b)`
- Defined: `ld.c:973`

### e4 `static void e4(long v)`
- Defined: `ld.c:978`

### e8 `static void e8(unsigned long long v)`
- Defined: `ld.c:986`

### eimm `static void eimm(long long v)`
- Defined: `ld.c:994`

### epush_local `static void epush_local(int slot)`
- Defined: `ld.c:1007`

### estore_local `static void estore_local(int slot)`
- Defined: `ld.c:1009`

### epush_global `static void epush_global(int slot)`
- Defined: `ld.c:1010`

### estore_global `static void estore_global(int slot)`
- Defined: `ld.c:1011`

### cvm_slot `static int cvm_slot(int std)`
- Defined: `ld.c:1012`

### epush_reg `static void epush_reg(int r)`
- Defined: `ld.c:1019`

### estore_reg `static void estore_reg(int r)`
- Defined: `ld.c:1025`

### pool_add `static long pool_add(const char *s)`
- Defined: `ld.c:1031`

### cvm_fixup_add `static void cvm_fixup_add(long pos, const char *name)`
- Defined: `ld.c:1040`

### ejmp `static void ejmp(const char *lbl)`
- Defined: `ld.c:1047`

### ejz `static void ejz(const char *lbl)`
- Defined: `ld.c:1049`

### ejnz `static void ejnz(const char *lbl)`
- Defined: `ld.c:1050`

### find_label `static long find_label(const char *name)`
- Defined: `ld.c:1051`

### add_label `static void add_label(const char *name, long off)`
- Defined: `ld.c:1057`

### resolve_fixups `static void resolve_fixups(void)`
- Defined: `ld.c:1065`

### push_mask32 `static void push_mask32(void)`
- Defined: `ld.c:1084`

### push_mask8 `static void push_mask8(void)`
- Defined: `ld.c:1086`

### push_mask16 `static void push_mask16(void)`
- Defined: `ld.c:1087`

### elea_mem `static void elea_mem(Op *op)`
- Defined: `ld.c:1088`

### elea_operand `static void elea_operand(Op *op)`
- Defined: `ld.c:1103`

### epush_value `static void epush_value(Op *op, int size)`
- Defined: `ld.c:1137`

### signext8 `static void signext8(void)`
- Defined: `ld.c:1163`

### signext32 `static void signext32(void)`
- Defined: `ld.c:1169`

### signext16 `static void signext16(void)`
- Defined: `ld.c:1175`

### mov `static void mov(int size, Op *s, Op *d)`
- Defined: `ld.c:1181`

### arith_mem `static void arith_mem(int opc, int size, Op *d, Op *s)`
- Defined: `ld.c:1241`

### arith_reg `static void arith_reg(int opc, int size, Op *d, Op *s)`
- Defined: `ld.c:1258`

### cvm_push_cmpval `static void cvm_push_cmpval(Op *o, int size)`
- Defined: `ld.c:1286`

### cvm_cmp `static void cvm_cmp(int size, Op *o1, Op *o2)`
- Defined: `ld.c:1292`

### cvm_translate `static void cvm_translate(const char *mn, Op *o1, Op *o2)`
- Defined: `ld.c:1299`

### cvm_prepare_tables `static void cvm_prepare_tables(void)`
- Defined: `ld.c:1829`

### cvm_layout_data `static void cvm_layout_data(void)`
- Defined: `ld.c:1861`

### func_glue `static void func_glue(void)`
- Defined: `ld.c:1906`

### entry_glue `static void entry_glue(void)`
- Defined: `ld.c:1915`

### cvm_encode `static void cvm_encode(LineSrc *src)`
- Defined: `ld.c:1929`

### w32 `static void w32(unsigned char *p, long v)`
- Defined: `ld.c:2003`

### w16 `static void w16(unsigned char *p, long v)`
- Defined: `ld.c:2010`

### w64_at `static void w64_at(unsigned char *p, unsigned long long v)`
- Defined: `ld.c:2015`

### cvm_write_module `static void cvm_write_module(const char *path)`
- Defined: `ld.c:2022`

### x86_align_up `static long x86_align_up(long v, long a)`
- Defined: `ld.c:2840`

### x8 `static void x8(int b)`
- Defined: `ld.c:2844`

### x16 `static void x16(long v)`
- Defined: `ld.c:2849`

### x32 `static void x32(long v)`
- Defined: `ld.c:2855`

### x64 `static void x64(unsigned long long v)`
- Defined: `ld.c:2863`

### xfix32 `static void xfix32(const char *sym)`
- Defined: `ld.c:2871`

### emit_rex `static void emit_rex(int w, int r, int x, int b)`
- Defined: `ld.c:2879`

### emit_modrm `static void emit_modrm(int mod, int reg, int rm)`
- Defined: `ld.c:2884`

### emit_sib `static void emit_sib(int scale, int index, int base)`
- Defined: `ld.c:2888`

### x86_ea_rex `static void x86_ea_rex(const Op *op, int regfield, int rexw, int force)`
- Defined: `ld.c:2892`

### x86_ea_modrm `static void x86_ea_modrm(const Op *op, int regfield)`
- Defined: `ld.c:2900`

### x86_rex_reg `static void x86_rex_reg(int w, int regfield, int rm)`
- Defined: `ld.c:2953`

### x86_rex8 `static void x86_rex8(int regfield, int rm)`
- Defined: `ld.c:2957`

### ea_mov `static void ea_mov(int size, const Op *o, int regfield)`
- Defined: `ld.c:2964`

### ea_mov_to `static void ea_mov_to(int size, const Op *o, int regfield)`
- Defined: `ld.c:2970`

### ea_alu `static void ea_alu(int g1, int size, const Op *o, int regfield, int from_mem)`
- Defined: `ld.c:2976`

### ea_cmp `static void ea_cmp(int size, const Op *o, int regfield, int from_mem)`
- Defined: `ld.c:2982`

### ea_grp `static void ea_grp(int opc, int size, const Op *o, int regfield)`
- Defined: `ld.c:2988`

### elf_mov `static void elf_mov(int size, const Op *s, const Op *d)`
- Defined: `ld.c:2994`

### elf_movzx `static void elf_movzx(const Op *s, const Op *d, int opc, int rexw, int has_0f)`
- Defined: `ld.c:3054`

### elf_movw `static void elf_movw(const Op *s, const Op *d)`
- Defined: `ld.c:3071`

### elf_lea `static void elf_lea(const Op *s, const Op *d)`
- Defined: `ld.c:3106`

### elf_push `static void elf_push(const Op *o)`
- Defined: `ld.c:3113`

### elf_pop `static void elf_pop(const Op *o)`
- Defined: `ld.c:3137`

### elf_alu `static void elf_alu(int g1, int size, const Op *s, const Op *d)`
- Defined: `ld.c:3150`

### elf_imul `static void elf_imul(const Op *s, const Op *d)`
- Defined: `ld.c:3210`

### elf_imull `static void elf_imull(const Op *s, const Op *d)`
- Defined: `ld.c:3239`

### elf_grp3 `static void elf_grp3(const Op *o, int ext)`
- Defined: `ld.c:3268`

### elf_shift_cl `static void elf_shift_cl(const Op *s, const Op *d, int ext)`
- Defined: `ld.c:3282`

### elf_shift_cl32 `static void elf_shift_cl32(const Op *s, const Op *d, int ext)`
- Defined: `ld.c:3290`

### elf_testl `static void elf_testl(const Op *s, const Op *d)`
- Defined: `ld.c:3298`

### elf_test `static void elf_test(const Op *s, const Op *d)`
- Defined: `ld.c:3315`

### elf_cmp `static void elf_cmp(int size, const Op *s, const Op *d)`
- Defined: `ld.c:3332`

### elf_set `static void elf_set(int cc, const Op *o)`
- Defined: `ld.c:3397`

### elf_branch `static void elf_branch(int opc, const Op *o)`
- Defined: `ld.c:3405`

### elf_ins `static void elf_ins(const char *mn, const Op *o1, const Op *o2)`
- Defined: `ld.c:3416`

### elf_sym_addr `static long elf_sym_addr(const Sym *s)`
- Defined: `ld.c:3496`

### elf_resolve_fixups `static void elf_resolve_fixups(void)`
- Defined: `ld.c:3509`

### elf_encode_src `static void elf_encode_src(LineSrc *src)`
- Defined: `ld.c:3528`

### elf_layout `static void elf_layout(void)`
- Defined: `ld.c:3584`

### elf_write `static void elf_write(const char *path)`
- Defined: `ld.c:3616`

### elf_build `static void elf_build(const char *in_path, const char *out_path)`
- Defined: `ld.c:3734`

### usage `static void usage(void)`
- Defined: `ld.c:3810`
- Doc: ================================================================ CLI * =================================================

### main `int main(int argc, char **argv)`
- Defined: `ld.c:3821`

## test/argv.c

### main `int main(int argc, char **argv)`
- Defined: `test/argv.c:2`

## test/argv.s

### main
- Defined: `test/argv.s:3`

### _start
- Defined: `test/argv.s:98`

## test/chain.c

### fib `int fib(int n)`
- Defined: `test/chain.c:1`

### main `int main(void)`
- Defined: `test/chain.c:5`

## test/fib.s

### fib
- Defined: `test/fib.s:3`

### main
- Defined: `test/fib.s:61`

### _start
- Defined: `test/fib.s:82`

## test/fib2.s

### fib
- Defined: `test/fib2.s:3`

### main
- Defined: `test/fib2.s:61`

### _start
- Defined: `test/fib2.s:82`

## test/fib3.s

### fib
- Defined: `test/fib3.s:3`

### main
- Defined: `test/fib3.s:61`

### _start
- Defined: `test/fib3.s:82`

## test/fmt.c

### main `int main(void)`
- Defined: `test/fmt.c:2`

## test/globals.c

### main `int main(void)`
- Defined: `test/globals.c:10`

## test/hello.c

### main `int main(void)`
- Defined: `test/hello.c:1`

## test/loop.s

### main
- Defined: `test/loop.s:3`

### _start
- Defined: `test/loop.s:21`

## test/movslq.s

### main
- Defined: `test/movslq.s:3`

### _start
- Defined: `test/movslq.s:14`

## test/run_tests.sh

### run_prog
- Defined: `test/run_tests.sh:40`
- Doc: run_prog <outfile> <cmd...> : run with a timeout; on timeout the program is treated as hung (exit code 124, empty output

### note_fail
- Defined: `test/run_tests.sh:47`

### check
- Defined: `test/run_tests.sh:53`
- Doc: check <name> <fmt> <actual_stdout_file> <actual_exit>

### run_fixture
- Defined: `test/run_tests.sh:78`
- Doc: run_fixture <name> <extra args...>

### run_chain
- Defined: `test/run_tests.sh:107`
- Doc: run_chain <name> [args...] : compile tests/<name>.c with miniGCC, assemble the result to both formats and check each aga

### elf_structure_check
- Defined: `test/run_tests.sh:139`

## test/start.s

### main
- Defined: `test/start.s:3`

### _start
- Defined: `test/start.s:8`

## test/t1.c

### main `int main(void)`
- Defined: `test/t1.c:1`

## test/t1.s

### main
- Defined: `test/t1.s:3`

### _start
- Defined: `test/t1.s:14`

## test/w1.c

### main `int main(void)`
- Defined: `test/w1.c:2`

## test/w1.s

### main
- Defined: `test/w1.s:3`

### _start
- Defined: `test/w1.s:37`
