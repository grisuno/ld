# API

## ld.c

### die (function) `static void die(const char *msg)`
- Defined: `ld.c:340`
- Doc: ================================================================ Diagnostics and memory * ==============================

### breserve (function) `static void breserve(unsigned char **p, long *cap, long need)`
- Defined: `ld.c:349`

### fixup_reserve (function) `static void fixup_reserve(void)`
- Defined: `ld.c:369`
- Doc: Grow the fixup table so that one more entry fits. The table is heap allocated rather than statically reserved: a worst-c

### creserve (function) `static void creserve(char **p, long *cap, long need)`
- Defined: `ld.c:384`

### parse_num (function) `static long parse_num(const char *s)`
- Defined: `ld.c:400`

### trim (function) `static char *trim(char *s)`
- Defined: `ld.c:432`

### strip_comment (function) `static void strip_comment(char *s)`
- Defined: `ld.c:443`
- Doc: Truncate at the first '#' outside a double-quoted string: '#' is the * comment character, but a string literal may carry

### name_copy (function) `static void name_copy(char *dst, const char *src)`
- Defined: `ld.c:457`

### split_word (function) `static void split_word(char *line, char *word, long wcap, char **rest)`
- Defined: `ld.c:464`

### hexval (function) `static int hexval(char c)`
- Defined: `ld.c:477`

### find_sym (function) `static int find_sym(const char *name)`
- Defined: `ld.c:484`

### add_sym (function) `static int add_sym(const char *name, int kind, int sec)`
- Defined: `ld.c:490`

### parse_reg (function) `static int parse_reg(const char *s, int *reg, int *sz)`
- Defined: `ld.c:526`

### parse_mem (function) `static void parse_mem(char *s, Op *op)`
- Defined: `ld.c:538`

### parse_operand (function) `static void parse_operand(char *s, Op *op)`
- Defined: `ld.c:591`

### split_operands (function) `static int split_operands(char *rest, char *o1, char *o2)`
- Defined: `ld.c:622`

### ls_open_file (function) `static void ls_open_file(LineSrc *s, const char *path)`
- Defined: `ld.c:647`

### ls_open_mem (function) `static void ls_open_mem(LineSrc *s, const char *text)`
- Defined: `ld.c:659`

### ls_getline (function) `static int ls_getline(LineSrc *s, char *buf, size_t n)`
- Defined: `ld.c:667`

### ls_close (function) `static void ls_close(LineSrc *s)`
- Defined: `ld.c:683`

### data_put (function) `static void data_put(unsigned char b)`
- Defined: `ld.c:691`
- Doc: ================================================================ Data region helpers * =================================

### data_fill (function) `static void data_fill(long n, unsigned char b)`
- Defined: `ld.c:696`

### data_align (function) `static void data_align(long a)`
- Defined: `ld.c:703`

### blob_put (function) `static void blob_put(unsigned char b)`
- Defined: `ld.c:707`

### blob_append_str (function) `static void blob_append_str(char *s)`
- Defined: `ld.c:712`

### set_section (function) `static int set_section(char *line)`
- Defined: `ld.c:751`

### scan_directive (function) `static void scan_directive(char *line, int *section, int pending_global,
                        ...`
- Defined: `ld.c:774`
- Doc: ================================================================ Shared scan pass * ====================================

### scan_src (function) `static void scan_src(LineSrc *src, int from_stubs)`
- Defined: `ld.c:870`

### cvm_find_func (function) `static int cvm_find_func(const char *name)`
- Defined: `ld.c:941`
- Doc: ================================================================ CVM backend * =========================================

### cvm_find_global (function) `static int cvm_find_global(const char *name)`
- Defined: `ld.c:947`

### cvm_find_blob (function) `static int cvm_find_blob(const char *name)`
- Defined: `ld.c:953`

### cvm_find_nat (function) `static int cvm_find_nat(const char *name)`
- Defined: `ld.c:959`

### cvm_add_nat (function) `static int cvm_add_nat(const char *name)`
- Defined: `ld.c:965`

### e1 (function) `static void e1(int b)`
- Defined: `ld.c:974`

### e4 (function) `static void e4(long v)`
- Defined: `ld.c:979`

### e8 (function) `static void e8(unsigned long long v)`
- Defined: `ld.c:987`

### eimm (function) `static void eimm(long long v)`
- Defined: `ld.c:995`

### epush_local (function) `static void epush_local(int slot)`
- Defined: `ld.c:1008`

### estore_local (function) `static void estore_local(int slot)`
- Defined: `ld.c:1010`

### epush_global (function) `static void epush_global(int slot)`
- Defined: `ld.c:1011`

### estore_global (function) `static void estore_global(int slot)`
- Defined: `ld.c:1012`

### cvm_slot (function) `static int cvm_slot(int std)`
- Defined: `ld.c:1013`

### epush_reg (function) `static void epush_reg(int r)`
- Defined: `ld.c:1020`

### estore_reg (function) `static void estore_reg(int r)`
- Defined: `ld.c:1026`

### pool_add (function) `static long pool_add(const char *s)`
- Defined: `ld.c:1032`

### cvm_fixup_add (function) `static void cvm_fixup_add(long pos, const char *name)`
- Defined: `ld.c:1041`

### ejmp (function) `static void ejmp(const char *lbl)`
- Defined: `ld.c:1048`

### ejz (function) `static void ejz(const char *lbl)`
- Defined: `ld.c:1050`

### ejnz (function) `static void ejnz(const char *lbl)`
- Defined: `ld.c:1051`

### find_label (function) `static long find_label(const char *name)`
- Defined: `ld.c:1052`

### add_label (function) `static void add_label(const char *name, long off)`
- Defined: `ld.c:1058`

### resolve_fixups (function) `static void resolve_fixups(void)`
- Defined: `ld.c:1066`

### push_mask32 (function) `static void push_mask32(void)`
- Defined: `ld.c:1085`

### push_mask8 (function) `static void push_mask8(void)`
- Defined: `ld.c:1087`

### push_mask16 (function) `static void push_mask16(void)`
- Defined: `ld.c:1088`

### elea_mem (function) `static void elea_mem(Op *op)`
- Defined: `ld.c:1089`

### elea_operand (function) `static void elea_operand(Op *op)`
- Defined: `ld.c:1104`

### epush_value (function) `static void epush_value(Op *op, int size)`
- Defined: `ld.c:1138`

### signext8 (function) `static void signext8(void)`
- Defined: `ld.c:1164`

### signext32 (function) `static void signext32(void)`
- Defined: `ld.c:1170`

### signext16 (function) `static void signext16(void)`
- Defined: `ld.c:1176`

### mov (function) `static void mov(int size, Op *s, Op *d)`
- Defined: `ld.c:1182`

### arith_mem (function) `static void arith_mem(int opc, int size, Op *d, Op *s)`
- Defined: `ld.c:1242`

### arith_reg (function) `static void arith_reg(int opc, int size, Op *d, Op *s)`
- Defined: `ld.c:1259`

### cvm_push_cmpval (function) `static void cvm_push_cmpval(Op *o, int size)`
- Defined: `ld.c:1287`

### cvm_cmp (function) `static void cvm_cmp(int size, Op *o1, Op *o2)`
- Defined: `ld.c:1293`

### cvm_translate (function) `static void cvm_translate(const char *mn, Op *o1, Op *o2)`
- Defined: `ld.c:1300`

### cvm_prepare_tables (function) `static void cvm_prepare_tables(void)`
- Defined: `ld.c:1866`

### cvm_layout_data (function) `static void cvm_layout_data(void)`
- Defined: `ld.c:1898`

### func_glue (function) `static void func_glue(void)`
- Defined: `ld.c:1943`

### entry_glue (function) `static void entry_glue(void)`
- Defined: `ld.c:1952`

### cvm_encode (function) `static void cvm_encode(LineSrc *src)`
- Defined: `ld.c:1966`

### w32 (function) `static void w32(unsigned char *p, long v)`
- Defined: `ld.c:2040`

### w16 (function) `static void w16(unsigned char *p, long v)`
- Defined: `ld.c:2047`

### w64_at (function) `static void w64_at(unsigned char *p, unsigned long long v)`
- Defined: `ld.c:2052`

### cvm_write_module (function) `static void cvm_write_module(const char *path)`
- Defined: `ld.c:2059`

### x86_align_up (function) `static long x86_align_up(long v, long a)`
- Defined: `ld.c:2897`

### x8 (function) `static void x8(int b)`
- Defined: `ld.c:2901`

### x16 (function) `static void x16(long v)`
- Defined: `ld.c:2906`

### x32 (function) `static void x32(long v)`
- Defined: `ld.c:2912`

### x64 (function) `static void x64(unsigned long long v)`
- Defined: `ld.c:2920`

### xfix32 (function) `static void xfix32(const char *sym)`
- Defined: `ld.c:2928`

### fixup_trail (function) `static void fixup_trail(long t)`
- Defined: `ld.c:2937`

### emit_rex (function) `static void emit_rex(int w, int r, int x, int b)`
- Defined: `ld.c:2942`

### emit_modrm (function) `static void emit_modrm(int mod, int reg, int rm)`
- Defined: `ld.c:2947`

### emit_sib (function) `static void emit_sib(int scale, int index, int base)`
- Defined: `ld.c:2951`

### x86_ea_rex (function) `static void x86_ea_rex(const Op *op, int regfield, int rexw, int force)`
- Defined: `ld.c:2955`

### x86_ea_modrm (function) `static void x86_ea_modrm(const Op *op, int regfield)`
- Defined: `ld.c:2963`

### x86_rex_reg (function) `static void x86_rex_reg(int w, int regfield, int rm)`
- Defined: `ld.c:3016`

### x86_rex8 (function) `static void x86_rex8(int regfield, int rm)`
- Defined: `ld.c:3020`

### ea_mov (function) `static void ea_mov(int size, const Op *o, int regfield)`
- Defined: `ld.c:3027`

### ea_mov_to (function) `static void ea_mov_to(int size, const Op *o, int regfield)`
- Defined: `ld.c:3033`

### ea_alu (function) `static void ea_alu(int g1, int size, const Op *o, int regfield, int from_mem)`
- Defined: `ld.c:3039`

### ea_cmp (function) `static void ea_cmp(int size, const Op *o, int regfield, int from_mem)`
- Defined: `ld.c:3045`

### ea_grp (function) `static void ea_grp(int opc, int size, const Op *o, int regfield)`
- Defined: `ld.c:3051`

### elf_mov (function) `static void elf_mov(int size, const Op *s, const Op *d)`
- Defined: `ld.c:3057`

### elf_movzx (function) `static void elf_movzx(const Op *s, const Op *d, int opc, int rexw, int has_0f)`
- Defined: `ld.c:3118`

### elf_movw (function) `static void elf_movw(const Op *s, const Op *d)`
- Defined: `ld.c:3135`

### elf_lea (function) `static void elf_lea(const Op *s, const Op *d)`
- Defined: `ld.c:3170`

### elf_push (function) `static void elf_push(const Op *o)`
- Defined: `ld.c:3177`

### elf_pop (function) `static void elf_pop(const Op *o)`
- Defined: `ld.c:3201`

### elf_alu (function) `static void elf_alu(int g1, int size, const Op *s, const Op *d)`
- Defined: `ld.c:3214`

### elf_imul (function) `static void elf_imul(const Op *s, const Op *d)`
- Defined: `ld.c:3277`

### elf_imull (function) `static void elf_imull(const Op *s, const Op *d)`
- Defined: `ld.c:3306`

### elf_grp3 (function) `static void elf_grp3(const Op *o, int ext)`
- Defined: `ld.c:3335`

### elf_grp_ff (function) `static void elf_grp_ff(const Op *o, int ext)`
- Defined: `ld.c:3349`

### elf_shift_cl (function) `static void elf_shift_cl(const Op *s, const Op *d, int ext)`
- Defined: `ld.c:3363`

### elf_shift_cl32 (function) `static void elf_shift_cl32(const Op *s, const Op *d, int ext)`
- Defined: `ld.c:3371`

### elf_testl (function) `static void elf_testl(const Op *s, const Op *d)`
- Defined: `ld.c:3379`

### elf_test (function) `static void elf_test(const Op *s, const Op *d)`
- Defined: `ld.c:3396`

### elf_cmp (function) `static void elf_cmp(int size, const Op *s, const Op *d)`
- Defined: `ld.c:3413`

### elf_set (function) `static void elf_set(int cc, const Op *o)`
- Defined: `ld.c:3481`

### elf_branch (function) `static void elf_branch(int opc, const Op *o)`
- Defined: `ld.c:3489`

### elf_ins (function) `static void elf_ins(const char *mn, const Op *o1, const Op *o2)`
- Defined: `ld.c:3500`

### elf_sym_addr (function) `static long elf_sym_addr(const Sym *s)`
- Defined: `ld.c:3586`

### elf_resolve_fixups (function) `static void elf_resolve_fixups(void)`
- Defined: `ld.c:3599`

### elf_encode_src (function) `static void elf_encode_src(LineSrc *src)`
- Defined: `ld.c:3618`

### elf_layout (function) `static void elf_layout(void)`
- Defined: `ld.c:3674`

### elf_write (function) `static void elf_write(const char *path)`
- Defined: `ld.c:3706`

### elf_build (function) `static void elf_build(const char *in_path, const char *out_path)`
- Defined: `ld.c:3824`

### usage (function) `static void usage(void)`
- Defined: `ld.c:3900`
- Doc: ================================================================ CLI * =================================================

### main (function) `int main(int argc, char **argv)`
- Defined: `ld.c:3911`

### fprintf (function) `fprintf(stderr, "ld: %s:%ld: %s\n", cur_file, cur_line, msg);`
- Defined: `ld.c:342`

### exit (function) `exit(1);`
- Defined: `ld.c:346`

### memcpy (function) `memcpy(dst, src, (size_t)n);`
- Defined: `ld.c:461`

### memset (function) `memset(&syms[i], 0, sizeof(syms[i]));`
- Defined: `ld.c:496`

### strncpy (function) `strncpy(o1, trim(rest), CFG_LINE_MAX - 1);`
- Defined: `ld.c:637`

### sprintf (function) `sprintf(l1, "..S%lda", synth_n);`
- Defined: `ld.c:1835`

### fwrite (function) `fwrite(hdr, 1, CFG_CVM_HDR_SIZE, f);`
- Defined: `ld.c:2096`

### fputc (function) `fputc((int)(z - 1), f);`
- Defined: `ld.c:2127`

### fclose (function) `fclose(f);`
- Defined: `ld.c:2139`

### free (function) `free(func_name_off);`
- Defined: `ld.c:2140`

### strncat (function) `strncat(out, ext, CFG_NAME_MAX - strlen(out) - 1);`
- Defined: `ld.c:3947`

## test/argv.c

### main (function) `int main(int argc, char **argv)`
- Defined: `test/argv.c:2`

### write (function) `int write(int fd, char *buf, int n);`
- Defined: `test/argv.c:1`

## test/argv.s

### main (function)
- Defined: `test/argv.s:3`

### _start (function)
- Defined: `test/argv.s:98`

## test/asm.c

### main (function) `int main(void)`
- Defined: `test/asm.c:4`

### printf (function) `int printf();`
- Defined: `test/asm.c:1`

### volatile (function) `__asm__ volatile("nop");`
- Defined: `test/asm.c:6`

### __asm (function) `__asm("nop");`
- Defined: `test/asm.c:7`

## test/chain.c

### fib (function) `int fib(int n)`
- Defined: `test/chain.c:1`

### main (function) `int main(void)`
- Defined: `test/chain.c:5`

## test/fib.s

### fib (function)
- Defined: `test/fib.s:3`

### main (function)
- Defined: `test/fib.s:61`

### _start (function)
- Defined: `test/fib.s:82`

## test/fib2.s

### fib (function)
- Defined: `test/fib2.s:3`

### main (function)
- Defined: `test/fib2.s:61`

### _start (function)
- Defined: `test/fib2.s:82`

## test/fib3.s

### fib (function)
- Defined: `test/fib3.s:3`

### main (function)
- Defined: `test/fib3.s:61`

### _start (function)
- Defined: `test/fib3.s:82`

## test/fmt.c

### main (function) `int main(void)`
- Defined: `test/fmt.c:2`

### printf (function) `int printf();`
- Defined: `test/fmt.c:1`

## test/globals.c

### main (function) `int main(void)`
- Defined: `test/globals.c:10`

### printf (function) `int printf();`
- Defined: `test/globals.c:1`

### puts (function) `int puts(char *s);`
- Defined: `test/globals.c:2`

## test/hello.c

### main (function) `int main(void)`
- Defined: `test/hello.c:1`

## test/loop.s

### main (function)
- Defined: `test/loop.s:3`

### _start (function)
- Defined: `test/loop.s:21`

## test/movslq.s

### main (function)
- Defined: `test/movslq.s:3`

### _start (function)
- Defined: `test/movslq.s:14`

## test/priv.s

### main (function)
- Defined: `test/priv.s:3`

### _start (function)
- Defined: `test/priv.s:13`

## test/run_tests.sh

### run_prog (function)
- Defined: `test/run_tests.sh:40`
- Doc: run_prog <outfile> <cmd...> : run with a timeout; on timeout the program is treated as hung (exit code 124, empty output

### note_fail (function)
- Defined: `test/run_tests.sh:47`

### check (function)
- Defined: `test/run_tests.sh:53`
- Doc: check <name> <fmt> <actual_stdout_file> <actual_exit>

### run_fixture (function)
- Defined: `test/run_tests.sh:78`
- Doc: run_fixture <name> <extra args...>

### run_chain (function)
- Defined: `test/run_tests.sh:107`
- Doc: run_chain <name> [args...] : compile tests/<name>.c with miniGCC, assemble the result to both formats and check each aga

### elf_structure_check (function)
- Defined: `test/run_tests.sh:139`

## test/start.s

### main (function)
- Defined: `test/start.s:3`

### _start (function)
- Defined: `test/start.s:8`

## test/t1.c

### main (function) `int main(void)`
- Defined: `test/t1.c:1`

## test/t1.s

### main (function)
- Defined: `test/t1.s:3`

### _start (function)
- Defined: `test/t1.s:14`

## test/w1.c

### main (function) `int main(void)`
- Defined: `test/w1.c:2`

### write (function) `int write(int fd, char *buf, int n);`
- Defined: `test/w1.c:1`

## test/w1.s

### main (function)
- Defined: `test/w1.s:3`

### _start (function)
- Defined: `test/w1.s:37`
