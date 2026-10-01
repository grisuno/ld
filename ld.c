/*
 * ld.c — assembler and linker for the miniGCC x86-64 (AT&T) dialect.
 *
 * Consumes the assembly emitted by miniGCC and produces one of two
 * executable formats:
 *   -f cvm   CVM v2 stack-bytecode module (run with the cvm2 interpreter,
 *            including the cvm.o interpreter embedded in MiniOS)
 *   -f elf   static position-independent ET_DYN ELF64 executable that runs
 *            natively on Linux and inside MiniOS (load_exec_elf + syscall ABI)
 *
 * Design: single encode pass per backend with fixed-size instruction
 * encodings; all cross-references are disp32 fixups resolved at assembly
 * time (branches and sym(%rip) operands), so the ELF output carries no
 * runtime relocations.  Every tunable constant lives in the Config section.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Config
 * ================================================================ */

#define CFG_MAX_SYMBOLS 16384
#define CFG_MAX_FIXUPS  524288
#define CFG_FIXUP_INIT  4096
#define CFG_LINE_MAX    1024
#define CFG_NAME_MAX    128
#define CFG_MAX_NATS    512
#define CFG_MAX_ERRORS  32
#define CFG_GROW_UNIT   1024

#define CFG_ABI_BYTES   104
#define CFG_STACK_BASE  104
#define CFG_XSTACK_DEF  262144
/* Room reserved past the stack region for the argv pointer array that the
 * interpreter builds at the stack top; an argument list longer than this
 * cap is rejected by the interpreter, never silently truncated. */
#define CFG_MAX_ARGS    1024

#define CFG_REG_LOCALS  18
#define CFG_SLOT_FLAGS_A 14
#define CFG_SLOT_FLAGS_B 15
#define CFG_SLOT_S0     16
#define CFG_SLOT_S1     17

#define CFG_GSLOT_RSP   2
#define CFG_GSLOT_RBP   3
#define CFG_GSLOT_ARGS  4
#define CFG_GSLOT_RET   10

#define CFG_CVM_MAGIC_0 0x43
#define CFG_CVM_MAGIC_1 0x56
#define CFG_CVM_MAGIC_2 0x4D
#define CFG_CVM_MAGIC_3 0x04
#define CFG_CVM_VER_MAJ  1
#define CFG_CVM_VER_MIN  0
#define CFG_CVM_HDR_SIZE 40

#define CFG_ELF_PAGE     4096
#define CFG_ELF_HSIZE    64
#define CFG_ELF_PHENTSZ  56
#define CFG_ELF_PHNUM    2
#define CFG_ELF_SHENTSZ  64
#define CFG_ELF_SHNUM    6
#define CFG_ELF_SHSTRNDX 5
#define CFG_ELF_ET_DYN   3
#define CFG_ELF_EM_X8664 62
#define CFG_ELF_PF_R     4
#define CFG_ELF_PF_W     2
#define CFG_ELF_PF_X     1
#define CFG_ELF_PT_LOAD  1
#define CFG_ELF_PT_DYNAMIC 2
#define CFG_ELF_PT_GNU_STACK 0x6474e551
#define CFG_ELF_SHT_PROGBITS 1
#define CFG_ELF_SHT_NOBITS   8
#define CFG_ELF_SHT_STRTAB   3
#define CFG_ELF_SHT_HASH     5
#define CFG_ELF_SHT_DYNAMIC  6
#define CFG_ELF_SHT_DYNSYM   11
#define CFG_ELF_SHT_RELA     4
#define CFG_ELF_DT_NULL   0
#define CFG_ELF_DT_NEEDED 1
#define CFG_ELF_DT_RELA   7
#define CFG_ELF_DT_RELASZ 8
#define CFG_ELF_DT_RELAENT 9
#define CFG_ELF_DT_HASH   4
#define CFG_ELF_DT_STRTAB 5
#define CFG_ELF_DT_SYMTAB 6
#define CFG_ELF_DT_STRSZ  10
#define CFG_ELF_DT_SYMENT 11
#define CFG_ELF_DT_SONAME 14
#define CFG_ELF_STB_GLOBAL 1
#define CFG_ELF_STT_NOTYPE 0
#define CFG_ELF_STT_FUNC   2
#define CFG_ELF_R_GLOB_DAT   6
#define CFG_ELF_R_RELATIVE   8
#define CFG_ELF_SHN_UNDEF  0
#define CFG_LD_MAXLIBS     8
#define CFG_LD_MAXIMPORTS  512
#define CFG_LD_LIB_MAXSZ   (4 * 1024 * 1024)
#define CFG_ELF_DYN_NENTRY 7
#define CFG_ELF_DYNSYM_ENT 24
#define CFG_ELF_SHF_A     2
#define CFG_ELF_SHF_X     4
#define CFG_ELF_SHF_W     1
#define CFG_ELF_TEXT_BASE (CFG_ELF_HSIZE + CFG_ELF_PHNUM * CFG_ELF_PHENTSZ)

#define CFG_FMT_CVM 0
#define CFG_FMT_ELF 1

/* x86 opcode groups */
#define X86_G1_ADD 0x00
#define X86_G1_OR  0x08
#define X86_G1_AND 0x20
#define X86_G1_SUB 0x28
#define X86_G1_XOR 0x30
#define X86_G1_CMP 0x38

#define X86_JCC_JE  0x84
#define X86_JCC_JNE 0x85
#define X86_JCC_JL  0x8C
#define X86_JCC_JG  0x8F
#define X86_JCC_JLE 0x8E
#define X86_JCC_JGE 0x8D
#define X86_JCC_JA  0x87
#define X86_JCC_JAE 0x83
#define X86_JCC_JB  0x82
#define X86_JCC_JBE 0x86

#define X86_SET_E  0x94
#define X86_SET_NE 0x95
#define X86_SET_L  0x9C
#define X86_SET_G  0x9F
#define X86_SET_LE 0x9E
#define X86_SET_GE 0x9D
#define X86_SET_A  0x97
#define X86_SET_AE 0x93
#define X86_SET_B  0x92
#define X86_SET_BE 0x96

#define X86_SYS_WRITE      1
#define X86_SYS_READ       0
#define X86_SYS_OPEN       2
#define X86_SYS_CLOSE      3
#define X86_SYS_LSEEK      8
#define X86_SYS_BRK       12
#define X86_SYS_EXIT      60
#define X86_SYS_EXIT_GROUP 231

#define REG_RAX 0
#define REG_RCX 1
#define REG_RDX 2
#define REG_RBX 3
#define REG_RSP 4
#define REG_RBP 5
#define REG_RSI 6
#define REG_RDI 7

#define SEC_TEXT   0
#define SEC_BSS    1
#define SEC_DATA   2
#define SEC_RODATA 3

#define SYM_FUNC   0
#define SYM_LABEL  1
#define SYM_GLOBAL 2
#define SYM_BLOB   3

#define K_REG 0
#define K_IMM 1
#define K_MEM 2
#define K_SYM 3
#define K_SYM_IMM 4
#define K_IND 5

#define OP_NOP          0
#define OP_PUSH_IMM64   1
#define OP_PUSH_IMM32   2
#define OP_PUSH_IMM8    3
#define OP_PUSH_ZERO    4
#define OP_PUSH_ONE     5
#define OP_PUSH_LOCAL   16
#define OP_STORE_LOCAL  17
#define OP_PUSH_GLOBAL  18
#define OP_STORE_GLOBAL 19
#define OP_ADD          32
#define OP_SUB          33
#define OP_MUL          34
#define OP_DIV          35
#define OP_MOD          36
#define OP_NEG          37
#define OP_UDIV         38
#define OP_UMOD         39
#define OP_AND          48
#define OP_OR           49
#define OP_XOR          50
#define OP_NOT          51
#define OP_SHL          52
#define OP_SHR          53
#define OP_USHR         54
#define OP_CMP_EQ       64
#define OP_CMP_NE       65
#define OP_CMP_LT       66
#define OP_CMP_LE       67
#define OP_CMP_GT       68
#define OP_CMP_GE       69
#define OP_LNOT         70
#define OP_CMP_ULT      71
#define OP_CMP_ULE      72
#define OP_CMP_UGT      73
#define OP_CMP_UGE      74
#define OP_JMP          80
#define OP_JZ           81
#define OP_JNZ          82
#define OP_CALL         96
#define OP_RET          97
#define OP_CALL_NATIVE  98
#define OP_CALL_INDIRECT 99
#define OP_LOAD8        112
#define OP_LOAD16       111
#define OP_LOAD32       113
#define OP_LOAD64       114
#define OP_STORE8       115
#define OP_STORE16      110
#define OP_STORE32      116
#define OP_STORE64      117
#define OP_LEA_LOCAL    118
#define OP_LEA_GLOBAL   119
#define OP_ALLOC        128
#define OP_FREE         129
#define OP_LEA_DATA     130
#define OP_SYSCALL      144
#define OP_HALT         255

/* ================================================================
 *  Data model
 * ================================================================ */

typedef struct {
    char name[CFG_NAME_MAX];
    int  kind;
    int  sec;
    long off;
    long size;
    long base;
    long end;
    long fidx;
    int  shadowed;
    int  is_import;
    int  import_idx;
    int  is_stub;
} Sym;

typedef struct {
    int  kind;
    int  reg;
    int  regsz;
    int  has_base;
    int  base;
    int  has_index;
    int  index;
    long scale;
    long disp;
    long imm;
    int  is_rip;
    char sym[CFG_NAME_MAX];
} Op;

typedef struct {
    long pos;
    long addend;
    char name[CFG_NAME_MAX];
} Fixup;

typedef struct {
    FILE       *fp;
    const char *mem;
    size_t      mem_len;
    size_t      mem_pos;
    int         is_mem;
} LineSrc;

typedef struct {
    char name[CFG_NAME_MAX];
    long code_off;
    int  num_locals;
    int  argc;
} Func;

typedef struct {
    char name[CFG_NAME_MAX];
    long off;
    long size;
    long base;
} GVar;

typedef struct {
    char name[CFG_NAME_MAX];
    long off;
    long base;
    long end;
} Blob;

typedef struct {
    char name[CFG_NAME_MAX];
} Nat;

typedef struct {
    char name[CFG_NAME_MAX];
    long off;
} Label;

static Sym  syms[CFG_MAX_SYMBOLS];
static int  n_syms;

static unsigned char *blob_data;
static long blob_len;
static long blob_cap;

static Func  funcs[CFG_MAX_SYMBOLS];
static int   n_funcs;
static GVar  globals[CFG_MAX_SYMBOLS];
static int   n_globals;
static Blob  blobs[CFG_MAX_SYMBOLS];
static int   n_blobs;
static Nat   nats[CFG_MAX_NATS];
static int   n_nats;

static unsigned char *data_region;
static long data_len;
static long data_cap;

static unsigned char *code;
static long code_len;
static long term_pos = -1;
static long code_cap;

static char *pool;
static long pool_len;
static long pool_cap;

static Label labels[CFG_MAX_SYMBOLS];
static int   n_labels;
static Fixup *fixups;
static int    n_fixups;
static int    cap_fixups;

static int  entry_func;
static int  entry_sym;
static long cur_line;
static char cur_file[CFG_NAME_MAX];
static int  extern_used[3];
static long extern_off[3];
static long g_xstack;
static int  error_count;
static char globl_name[CFG_NAME_MAX];
static int  globl_pending;

static long elf_text_size;
static long elf_rodata_size;
static long elf_data_size;
static long elf_bss_size;
static long elf_text_base;
static long elf_rodata_base;
static long elf_data_base;
static long elf_bss_base;

/* Shared-library mode (-shared): ET_DYN with SONAME plus dynamic
 * tables. Bases and sizes below are link addresses (zero-based,
 * like the rest of the layout); the count of exported function
 * symbols drives the dynsym/dynstr/hash sizes. The dynstr pool and
 * per-symbol name offsets live until ld_release. */
static int   g_shared;
static long  elf_dyn_base;
static long  elf_dynsym_base;
static long  elf_dynstr_base;
static long  elf_dynstr_size;
static long  elf_hash_base;
static long  elf_hash_size;
static long elf_ndyn;
static char *elf_dynstr;
static long *elf_dynoff;
static char elf_soname[CFG_NAME_MAX];

/* -d libraries (L2): verified import names plus the SONAMEs the
 * executable records as DT_NEEDED. Parsed eagerly after argument
 * parsing so link errors name the missing library, never a raw
 * offset. */
typedef struct {
    char name[CFG_NAME_MAX];
    int is_func;
    long value;
} LdImport;

/* RELATIVE rows derive straight from the import table (one GOT slot
 * each), so no carried relocation list exists. */

static char ld_lib_paths[CFG_LD_MAXLIBS][CFG_NAME_MAX];
static char ld_lib_sonames[CFG_LD_MAXLIBS][CFG_NAME_MAX];
static int ld_nlibs;
static LdImport ld_imports[CFG_LD_MAXIMPORTS];
static int ld_nimports;
static long elf_plt_base;
static long elf_got_base;
static long elf_got_size;
static long *elf_impoff;
static long *elf_needoff;
static long elf_dyn_tags[24];
static unsigned long long elf_dyn_vals[24];
static long elf_dyn_nentry;
static long elf_rela_base;

/* Basename without directories, for the DT_SONAME tag. */
static void ld_soname_of(const char *path, char *out) {    const char *b = path;
    const char *q = path;
    while (*q) {
        if (*q == '/') b = q + 1;
        q++;
    }
    size_t n = strlen(b);
    if (n >= CFG_NAME_MAX) n = CFG_NAME_MAX - 1;
    memcpy(out, b, n);
    out[n] = 0;
}

static long synth_n;

/* Release every heap arena the link built. On Unix exit() reclaims them,
 * but MiniOS runs ET_REL objects in-kernel and returns from main into the
 * persistent heap, so a link that never frees bleeds ~0.5 MB per run. */
static void ld_release(void) {
    free(blob_data);
    blob_data = 0;
    blob_len = 0;
    blob_cap = 0;
    free(data_region);
    data_region = 0;
    data_len = 0;
    data_cap = 0;
    free(code);
    code = 0;
    code_len = 0;
    code_cap = 0;
    term_pos = -1;
    free(pool);
    pool = 0;
    pool_len = 0;
    pool_cap = 0;
    free(fixups);
    fixups = 0;
    free(elf_dynstr);
    elf_dynstr = 0;
    free(elf_dynoff);
    elf_dynoff = 0;
    free(elf_impoff);
    elf_impoff = 0;
    free(elf_needoff);
    elf_needoff = 0;
    n_fixups = 0;
    cap_fixups = 0;
    n_syms = 0;
    n_funcs = 0;
    n_globals = 0;
    n_blobs = 0;
    n_nats = 0;
    n_labels = 0;
    entry_func = 0;
    entry_sym = 0;
    error_count = 0;
}

/* ================================================================
 *  Diagnostics and memory
 * ================================================================ */

static void die(const char *msg) {
    fprintf(stderr, "ld: %s:%ld: %s\n", cur_file, cur_line, msg);
    error_count++;
    if (error_count > CFG_MAX_ERRORS) {
        fprintf(stderr, "ld: too many errors, aborting\n");
        ld_release();
        exit(1);
    }
}

static void breserve(unsigned char **p, long *cap, long need) {
    if (need <= *cap) return;
    long nc = *cap ? *cap : CFG_GROW_UNIT;
    while (nc < need) {
        if (nc > 0x40000000L) { fprintf(stderr, "ld: buffer too large\n"); exit(1); }
        nc *= 2;
    }
    unsigned char *np = (unsigned char *)realloc(*p, (size_t)nc);
    if (!np) {
        fprintf(stderr, "ld: out of memory\n");
        exit(1);
    }
    *p = np;
    *cap = nc;
}

/* Grow the fixup table so that one more entry fits. The table is heap
 * allocated rather than statically reserved: a worst-case static array would
 * dominate the image and put it out of reach of hosts with a small heap. */
static void fixup_reserve(void) {
    if (n_fixups < cap_fixups) return;
    int nc = cap_fixups ? cap_fixups * 2 : CFG_FIXUP_INIT;
    if (nc > CFG_MAX_FIXUPS || nc <= cap_fixups) {
        die("too many fixups");
        exit(1);
    }
    Fixup *nf = (Fixup *)realloc(fixups, (size_t)nc * sizeof(Fixup));
    if (!nf) {
        fprintf(stderr, "ld: out of memory\n");
        exit(1);
    }
    fixups = nf;
    cap_fixups = nc;
}

static void creserve(char **p, long *cap, long need) {
    if (need <= *cap) return;
    long nc = *cap ? *cap : CFG_GROW_UNIT;
    while (nc < need) {
        if (nc > 0x40000000L) { fprintf(stderr, "ld: buffer too large\n"); exit(1); }
        nc *= 2;
    }
    char *np = (char *)realloc(*p, (size_t)nc);
    if (!np) {
        fprintf(stderr, "ld: out of memory\n");
        exit(1);
    }
    *p = np;
    *cap = nc;
}

static long parse_num(const char *s) {
    int neg = 0;
    const char *p = s;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') p++;
    int base = 10;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
    unsigned long long v = 0;
    int any = 0;
    for (; *p; p++) {
        int d;
        if (*p >= '0' && *p <= '9') d = *p - '0';
        else if (*p >= 'a' && *p <= 'f') d = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F') d = *p - 'A' + 10;
        else break;
        if (d >= base) break;
        if (v > (9223372036854775807ULL - (unsigned long long)d) / (unsigned long long)base) {
            die("integer literal out of range");
            return 0;
        }
        v = v * (unsigned long long)base + (unsigned long long)d;
        any = 1;
    }
    if (!any) { die("bad integer literal"); return 0; }
    if (neg) {
        if (v > 9223372036854775808ULL) { die("integer literal out of range"); return 0; }
        return (long)(-(long long)v);
    }
    if (v > 9223372036854775807ULL) return (long)(long long)v;
    return (long)v;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) e--;
    *e = 0;
    return s;
}

/* Truncate at the first '#' outside a double-quoted string: '#' is the
 * comment character, but a string literal may carry one ("#"). */
static void strip_comment(char *s) {
    int in_str = 0;
    for (; *s; s++) {
        if (in_str && *s == '\\' && s[1]) {
            s++;
            continue;
        }
        if (*s == '"') in_str = !in_str;
        else if (*s == '#' && !in_str) {
            *s = 0;
            return;
        }
    }
}

static void name_copy(char *dst, const char *src) {
    long n = (long)strlen(src);
    if (n > CFG_NAME_MAX - 1) n = CFG_NAME_MAX - 1;
    memcpy(dst, src, (size_t)n);
    dst[n] = 0;
}

static void split_word(char *line, char *word, long wcap, char **rest) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    char *w = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    long n = p - w;
    if (n >= wcap) n = wcap - 1;
    memcpy(word, w, (size_t)n);
    word[n] = 0;
    while (*p == ' ' || *p == '\t') p++;
    *rest = p;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int find_sym(const char *name) {
    for (int i = 0; i < n_syms; i++)
        if (strcmp(syms[i].name, name) == 0) return i;
    return -1;
}

static int add_sym(const char *name, int kind, int sec) {
    int i = find_sym(name);
    if (i >= 0) return i;
    if (n_syms >= CFG_MAX_SYMBOLS) { die("too many symbols"); return -1; }
    i = n_syms++;
    memset(&syms[i], 0, sizeof(syms[i]));
    name_copy(syms[i].name, name);
    syms[i].kind = kind;
    syms[i].sec = sec;
    return i;
}

/* ================================================================
 *  Register tables (standard x86-64 encoding order)
 * ================================================================ */

static const char *reg64_names[] = {
    "%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp", "%rsi", "%rdi",
    "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15", 0
};

static const char *reg32_names[] = {
    "%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi",
    "%r8d", "%r9d", "%r10d", "%r11d", "%r12d", "%r13d", "%r14d", "%r15d", 0
};

static const char *reg8_names[] = {
    "%al", "%cl", "%dl", "%bl", "%spl", "%bpl", "%sil", "%dil",
    "%r8b", "%r9b", "%r10b", "%r11b", "%r12b", "%r13b", "%r14b", "%r15b", 0
};

static const char *reg16_names[] = {
    "%ax", "%cx", "%dx", "%bx", "%sp", "%bp", "%si", "%di",
    "%r8w", "%r9w", "%r10w", "%r11w", "%r12w", "%r13w", "%r14w", "%r15w", 0
};

static int parse_reg(const char *s, int *reg, int *sz) {
    for (int i = 0; reg64_names[i]; i++)
        if (strcmp(s, reg64_names[i]) == 0) { *reg = i; *sz = 8; return 1; }
    for (int i = 0; reg32_names[i]; i++)
        if (strcmp(s, reg32_names[i]) == 0) { *reg = i; *sz = 4; return 1; }
    for (int i = 0; reg8_names[i]; i++)
        if (strcmp(s, reg8_names[i]) == 0) { *reg = i; *sz = 1; return 1; }
    for (int i = 0; reg16_names[i]; i++)
        if (strcmp(s, reg16_names[i]) == 0) { *reg = i; *sz = 2; return 1; }
    return 0;
}

static void parse_mem(char *s, Op *op) {
    char *lp = strchr(s, '(');
    char *rp = strchr(lp ? lp : s, ')');
    if (!lp || !rp) { die("bad memory operand"); return; }
    *lp = 0;
    *rp = 0;
    char *prefix = trim(s);
    char *inside = trim(lp + 1);
    if (strcmp(inside, "%rip") == 0) {
        if (!*prefix) { die("rip operand without symbol"); return; }
        op->kind = K_SYM;
        op->is_rip = 1;
        name_copy(op->sym, prefix);
        return;
    }
    op->kind = K_MEM;
    op->disp = *prefix ? parse_num(prefix) : 0;
    op->has_base = 0;
    op->has_index = 0;
    op->scale = 1;
    char parts[3][64];
    int np = 0;
    char *q = inside;
    while (*q && np < 3) {
        char *start = q;
        while (*q && *q != ',') q++;
        long n = q - start;
        if (n >= 63) n = 63;
        memcpy(parts[np], start, (size_t)n);
        parts[np][n] = 0;
        trim(parts[np]);
        np++;
        if (*q == ',') q++;
    }
    if (np >= 1 && parts[0][0] == '%') {
        int r, z;
        if (!parse_reg(parts[0], &r, &z)) { die("bad base register"); return; }
        op->has_base = 1;
        op->base = r;
    }
    if (np >= 2 && parts[1][0] == '%') {
        int r, z;
        if (!parse_reg(parts[1], &r, &z)) { die("bad index register"); return; }
        op->has_index = 1;
        op->index = r;
    }
    if (np >= 3 && parts[2][0]) {
        op->scale = parse_num(parts[2]);
        if (op->scale != 1 && op->scale != 2 && op->scale != 4 && op->scale != 8)
            { die("bad scale"); op->scale = 1; }
    }
}

static void parse_operand(char *s, Op *op) {
    memset(op, 0, sizeof(*op));
    s = trim(s);
    if (!*s) { die("missing operand"); return; }
    if (s[0] == '*' && s[1] == '%') {
        int r, z;
        if (!parse_reg(s + 1, &r, &z)) { die("bad indirect register"); return; }
        op->kind = K_IND;
        op->reg = r;
        op->regsz = z;
        return;
    }
    if (s[0] == '$') {
        char *name = trim(s + 1);
        if (*name != '-' && *name != '+' && !(*name >= '0' && *name <= '9')) {
            op->kind = K_SYM_IMM;
            name_copy(op->sym, name);
            return;
        }
        op->kind = K_IMM;
        op->imm = parse_num(name);
        return;
    }
    if (s[0] == '%') {
        int r, z;
        if (!parse_reg(s, &r, &z)) { die("bad register"); return; }
        op->kind = K_REG;
        op->reg = r;
        op->regsz = z;
        return;
    }
    if (strchr(s, '(')) {
        parse_mem(s, op);
        return;
    }
    op->kind = K_SYM;
    name_copy(op->sym, s);
}

static int split_operands(char *rest, char *o1, char *o2) {
    o1[0] = 0;
    o2[0] = 0;
    char *p = rest;
    int depth = 0;
    while (*p) {
        if (*p == '(') depth++;
        if (*p == ')' && depth > 0) depth--;
        if (*p == ',' && depth == 0) break;
        p++;
    }
    if (*p == ',') {
        *p = 0;
        p++;
        strncpy(o1, trim(rest), CFG_LINE_MAX - 1);
        o1[CFG_LINE_MAX - 1] = 0;
        strncpy(o2, trim(p), CFG_LINE_MAX - 1);
        o2[CFG_LINE_MAX - 1] = 0;
        return 2;
    }
    strncpy(o1, trim(rest), CFG_LINE_MAX - 1);
    o1[CFG_LINE_MAX - 1] = 0;
    return 1;
}

static void ls_open_file(LineSrc *s, const char *path) {
    s->fp = fopen(path, "r");
    s->is_mem = 0;
    s->mem = 0;
    s->mem_len = 0;
    s->mem_pos = 0;
    if (!s->fp) {
        fprintf(stderr, "ld: cannot open %s\n", path);
        exit(1);
    }
}

static void ls_open_mem(LineSrc *s, const char *text) {
    s->fp = 0;
    s->is_mem = 1;
    s->mem = text;
    s->mem_len = strlen(text);
    s->mem_pos = 0;
}

static int ls_getline(LineSrc *s, char *buf, size_t n) {
    if (!s->is_mem) {
        if (!s->fp) return 0;
        return fgets(buf, (int)n, s->fp) != 0;
    }
    if (s->mem_pos >= s->mem_len) return 0;
    size_t i = 0;
    while (i + 1 < n && s->mem_pos < s->mem_len) {
        char c = s->mem[s->mem_pos++];
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = 0;
    return 1;
}

static void ls_close(LineSrc *s) {
    if (!s->is_mem && s->fp) { fclose(s->fp); s->fp = 0; }
}

/* ================================================================
 *  Data region helpers
 * ================================================================ */

static void data_put(unsigned char b) {
    breserve(&data_region, &data_cap, data_len + 1);
    data_region[data_len++] = b;
}

static void data_fill(long n, unsigned char b) {
    if (n <= 0) return;
    breserve(&data_region, &data_cap, data_len + n);
    memset(data_region + data_len, b, (size_t)n);
    data_len += n;
}

static void data_align(long a) {
    while (data_len % a) data_put(0);
}

static void blob_put(unsigned char b) {
    breserve(&blob_data, &blob_cap, blob_len + 1);
    blob_data[blob_len++] = b;
}

static void blob_append_str(char *s) {
    s = trim(s);
    if (*s != '"') { die("expected string literal"); return; }
    s++;
    while (*s && *s != '"') {
        if (*s == '\\') {
            s++;
            char c = *s++;
            if (c == 'n') blob_put('\n');
            else if (c == 't') blob_put('\t');
            else if (c == 'r') blob_put('\r');
            else if (c == 'f') blob_put('\f');
            else if (c == 'v') blob_put('\v');
            else if (c == 'a') blob_put('\a');
            else if (c == 'b') blob_put('\b');
            else if (c == '0') blob_put(0);
            else if (c == '\\') blob_put('\\');
            else if (c == '"') blob_put('"');
            else if (c == '\'') blob_put('\'');
            else if (c == '%') blob_put('%');
            else if (c == 'x' || c == 'X') {
                int v = 0, n = 0;
                while (n < 2 && *s && hexval(*s) >= 0) { v = v * 16 + hexval(*s); s++; n++; }
                blob_put((unsigned char)v);
            } else if (c >= '0' && c <= '7') {
                int v = c - '0', n = 1;
                while (n < 3 && *s >= '0' && *s <= '7') { v = v * 8 + (*s - '0'); s++; n++; }
                blob_put((unsigned char)v);
            } else {
                die("bad string escape");
                blob_put((unsigned char)c);
            }
        } else {
            blob_put((unsigned char)*s++);
        }
    }
    blob_put(0);
}

static int set_section(char *line) {
    char word[32];
    char *rest;
    split_word(line, word, sizeof(word), &rest);
    if (strcmp(word, ".section") == 0) {
        char name[32];
        split_word(rest, name, sizeof(name), &rest);
        if (strstr(name, ".rodata")) return SEC_RODATA;
        if (strstr(name, ".bss")) return SEC_BSS;
        if (strstr(name, ".data")) return SEC_DATA;
        return SEC_TEXT;
    }
    if (strcmp(word, ".text") == 0) return SEC_TEXT;
    if (strcmp(word, ".bss") == 0) return SEC_BSS;
    if (strcmp(word, ".rodata") == 0) return SEC_RODATA;
    if (strcmp(word, ".data") == 0) return SEC_DATA;
    return -1;
}

/* ================================================================
 *  Shared scan pass
 * ================================================================ */

static void scan_directive(char *line, int *section, int pending_global,
                           int pending_blob, int from_stubs) {
    int s = set_section(line);
    if (s >= 0) { *section = s; return; }
    char word[32];
    char *rest;
    split_word(line, word, sizeof(word), &rest);
    if (strcmp(word, ".globl") == 0) {
        char *name = trim(rest);
        char *c = name;
        while (*c && *c != ' ' && *c != '\t') c++;
        *c = 0;
        name_copy(globl_name, name);
        globl_pending = 1;
        return;
    }
    if (strcmp(word, ".weak") == 0 || strcmp(word, ".local") == 0 ||
        strcmp(word, ".ident") == 0 || strcmp(word, ".file") == 0 ||
        strcmp(word, ".size") == 0 || strcmp(word, ".type") == 0 ||
        strcmp(word, ".note.GNU-stack") == 0)
        return;
    if (strcmp(word, ".space") == 0 || strcmp(word, ".zero") == 0) {
        long sz = *rest ? parse_num(rest) : 0;
        if (sz < 0) { die("negative space"); return; }
        if (pending_global >= 0) {
            if (syms[pending_global].size > 0x7FFFFFFFL - sz) { die("global too large"); return; }
            syms[pending_global].size += sz;
        } else if (pending_blob >= 0) {
            while (sz-- > 0) blob_put(0);
            syms[pending_blob].end = blob_len;
        }
        return;
    }
    if (strcmp(word, ".asciz") == 0 || strcmp(word, ".string") == 0 ||
        strcmp(word, ".ascii") == 0) {
        if (pending_blob >= 0) {
            blob_append_str(rest);
            syms[pending_blob].end = blob_len;
        } else if (pending_global >= 0) {
            long base = blob_len;
            blob_append_str(rest);
            syms[pending_global].size += blob_len - base;
        } else {
            die("string data without label");
        }
        return;
    }
    if (strcmp(word, ".byte") == 0 || strcmp(word, ".word") == 0 ||
        strcmp(word, ".long") == 0 || strcmp(word, ".quad") == 0) {
        int sz = strcmp(word, ".byte") == 0 ? 1 :
                 strcmp(word, ".word") == 0 ? 2 :
                 strcmp(word, ".long") == 0 ? 4 : 8;
        char *p = rest;
        while (*p) {
            while (*p == ' ' || *p == '\t' || *p == ',') p++;
            if (!*p) break;
            char *q = p;
            while (*q && *q != ',') q++;
            long n = q - p;
            if (n >= CFG_LINE_MAX - 1) n = CFG_LINE_MAX - 1;
            char tmp[CFG_LINE_MAX];
            memcpy(tmp, p, (size_t)n);
            tmp[n] = 0;
            long val = parse_num(trim(tmp));
            for (int i = 0; i < sz; i++) {
                blob_put((unsigned char)(val & 255));
                val >>= 8;
            }
            p = q;
        }
        if (pending_blob >= 0) syms[pending_blob].end = blob_len;
        else if (pending_global >= 0) syms[pending_global].size = blob_len;
        return;
    }
    if (strcmp(word, ".align") == 0 || strcmp(word, ".balign") == 0) {
        long a = *rest ? parse_num(rest) : 1;
        if (a <= 0) a = 1;
        while (blob_len % a) blob_put(0);
        if (pending_blob >= 0) syms[pending_blob].end = blob_len;
        return;
    }
    if (strcmp(word, ".p2align") == 0) return;
    if (strcmp(word, ".comm") == 0) {
        char name[CFG_NAME_MAX];
        char *r2;
        split_word(rest, name, sizeof(name), &r2);
        long sz = *r2 ? parse_num(r2) : 0;
        if (sz < 0) { die("negative comm size"); return; }
        int i = add_sym(name, SYM_GLOBAL, SEC_BSS);
        if (i >= 0 && syms[i].size < sz) syms[i].size = sz;
        (void)from_stubs;
        return;
    }
    die("unsupported directive");
}

static void scan_src(LineSrc *src, int from_stubs) {
    char line[CFG_LINE_MAX];
    int section = SEC_TEXT;
    int pending_global = -1;
    int pending_blob = -1;
    long lineno = 0;
    while (ls_getline(src, line, sizeof(line))) {
        lineno++;
        cur_line = lineno;
        char *t = trim(line);
        strip_comment(t);
        if (!*t) continue;
        long n = (long)strlen(t);
        if (t[n - 1] == ':') {
            t[n - 1] = 0;
            if (section == SEC_TEXT) {
                int is_globl = globl_pending && strcmp(t, globl_name) == 0;
                if (is_globl) {
                    globl_pending = 0;
                    int existing = find_sym(t);
                    if (existing >= 0) {
                        if (from_stubs) syms[existing].shadowed = 1;
                    } else {
                        int i = add_sym(t, SYM_FUNC, SEC_TEXT);
                        if (i >= 0) {
                            syms[i].shadowed = 0;
                            if (from_stubs) syms[i].is_stub = 1;
                        }
                    }
                } else if (find_sym(t) < 0) {
                    add_sym(t, SYM_LABEL, SEC_TEXT);
                }
                pending_global = -1;
                pending_blob = -1;
            } else if (section == SEC_BSS) {
                int i = add_sym(t, SYM_GLOBAL, SEC_BSS);
                pending_global = i;
                pending_blob = -1;
            } else {
                int i = add_sym(t, SYM_BLOB, section);
                if (i >= 0) {
                    syms[i].base = blob_len;
                    syms[i].end = blob_len;
                }
                pending_blob = i;
                pending_global = -1;
            }
            continue;
        }
        if (t[0] == '.') {
            scan_directive(t, &section, pending_global, pending_blob, from_stubs);
            if (section != SEC_TEXT) globl_pending = 0;
            continue;
        }
        char *rip = strstr(t, "(%rip)");
        if (rip) {
            char *start = rip;
            while (start > t && start[-1] != ' ' && start[-1] != '\t' && start[-1] != ',')
                start--;
            char save = *rip;
            *rip = 0;
            char *name = trim(start);
            if (strcmp(name, "stderr") == 0) extern_used[0] = 1;
            else if (strcmp(name, "stdout") == 0) extern_used[1] = 1;
            else if (strcmp(name, "stdin") == 0) extern_used[2] = 1;
            *rip = save;
        }
    }
}

/* ================================================================
 *  CVM backend
 * ================================================================ */

static int cvm_find_func(const char *name) {
    for (int i = 0; i < n_funcs; i++)
        if (strcmp(funcs[i].name, name) == 0) return i;
    return -1;
}

static int cvm_find_global(const char *name) {
    for (int i = 0; i < n_globals; i++)
        if (strcmp(globals[i].name, name) == 0) return i;
    return -1;
}

static int cvm_find_blob(const char *name) {
    for (int i = 0; i < n_blobs; i++)
        if (strcmp(blobs[i].name, name) == 0) return i;
    return -1;
}

static int cvm_find_nat(const char *name) {
    for (int i = 0; i < n_nats; i++)
        if (strcmp(nats[i].name, name) == 0) return i;
    return -1;
}

static int cvm_add_nat(const char *name) {
    if (n_nats >= CFG_MAX_NATS) { die("too many natives"); return -1; }
    int i = cvm_find_nat(name);
    if (i >= 0) return i;
    i = n_nats++;
    name_copy(nats[i].name, name);
    return i;
}

static void e1(int b) {
    breserve(&code, &code_cap, code_len + 1);
    code[code_len++] = (unsigned char)b;
}

static void e4(long v) {
    breserve(&code, &code_cap, code_len + 4);
    code[code_len++] = (unsigned char)(v & 255);
    code[code_len++] = (unsigned char)((v >> 8) & 255);
    code[code_len++] = (unsigned char)((v >> 16) & 255);
    code[code_len++] = (unsigned char)((v >> 24) & 255);
}

static void e8(unsigned long long v) {
    breserve(&code, &code_cap, code_len + 8);
    for (int i = 0; i < 8; i++) {
        code[code_len++] = (unsigned char)(v & 255);
        v >>= 8;
    }
}

static void eimm(long long v) {
    if ((long long)(signed char)v == v) {
        e1(OP_PUSH_IMM8);
        e1((int)(v & 255));
    } else if ((long long)(int)v == v) {
        e1(OP_PUSH_IMM32);
        e4(v);
    } else {
        e1(OP_PUSH_IMM64);
        e8((unsigned long long)v);
    }
}

static void epush_local(int slot) { e1(OP_PUSH_LOCAL); e4(slot); }
static void estore_local(int slot) { e1(OP_STORE_LOCAL); e4(slot); }
static void epush_global(int slot) { e1(OP_PUSH_GLOBAL); e4(slot); }
static void estore_global(int slot) { e1(OP_STORE_GLOBAL); e4(slot); }

static int cvm_slot(int std) {
    if (std == REG_RSP) return 14;
    if (std == REG_RBP) return 15;
    if (std >= 6) return std - 2;
    return std;
}

static void epush_reg(int r) {
    int s = cvm_slot(r);
    if (s < 14) epush_local(s);
    else epush_global(s == 14 ? CFG_GSLOT_RSP : CFG_GSLOT_RBP);
}

static void estore_reg(int r) {
    int s = cvm_slot(r);
    if (s < 14) estore_local(s);
    else estore_global(s == 14 ? CFG_GSLOT_RSP : CFG_GSLOT_RBP);
}

static long pool_add(const char *s) {
    long n = (long)strlen(s) + 1;
    creserve(&pool, &pool_cap, pool_len + n);
    memcpy(pool + pool_len, s, (size_t)n);
    long off = pool_len;
    pool_len += n;
    return off;
}

static void cvm_fixup_add(long pos, const char *name) {
    fixup_reserve();
    fixups[n_fixups].pos = pos;
    name_copy(fixups[n_fixups].name, name);
    n_fixups++;
}

static void ejmp(const char *lbl) { e1(OP_JMP); cvm_fixup_add(code_len, lbl); e4(0); }
static void ejz(const char *lbl) { e1(OP_JZ); cvm_fixup_add(code_len, lbl); e4(0); }
static void ejnz(const char *lbl) { e1(OP_JNZ); cvm_fixup_add(code_len, lbl); e4(0); }

static long find_label(const char *name) {
    for (int i = 0; i < n_labels; i++)
        if (strcmp(labels[i].name, name) == 0) return labels[i].off;
    return -1;
}

static void add_label(const char *name, long off) {
    if (find_label(name) >= 0) { die("duplicate label"); return; }
    if (n_labels >= CFG_MAX_SYMBOLS) { die("too many labels"); return; }
    name_copy(labels[n_labels].name, name);
    labels[n_labels].off = off;
    n_labels++;
}

static void resolve_fixups(void) {
    for (int i = 0; i < n_fixups; i++) {
        long off = find_label(fixups[i].name);
        if (off < 0) {
            fprintf(stderr, "ld: %s: undefined label %s\n", cur_file, fixups[i].name);
            error_count++;
            continue;
        }
        long rel = off - (fixups[i].pos + 4);
        long p = fixups[i].pos;
        code[p]     = (unsigned char)(rel & 255);
        code[p + 1] = (unsigned char)((rel >> 8) & 255);
        code[p + 2] = (unsigned char)((rel >> 16) & 255);
        code[p + 3] = (unsigned char)((rel >> 24) & 255);
    }
    n_fixups = 0;
    n_labels = 0;
}

static void push_mask32(void) { eimm(4294967295L); e1(OP_AND); }
static void push_mask8(void) { eimm(255); e1(OP_AND); }
static void push_mask16(void) { eimm(65535); e1(OP_AND); }

static void elea_mem(Op *op) {
    if (op->has_base) epush_reg(op->base);
    else eimm(0);
    if (op->has_index) {
        epush_reg(op->index);
        eimm(op->scale);
        e1(OP_MUL);
        e1(OP_ADD);
    }
    if (op->disp) {
        eimm(op->disp);
        e1(OP_ADD);
    }
}

static void elea_operand(Op *op) {
    if (op->kind == K_SYM || op->kind == K_SYM_IMM) {
        long off = -1;
        int g = cvm_find_global(op->sym);
        if (g >= 0) off = globals[g].off;
        if (off < 0) {
            int b = cvm_find_blob(op->sym);
            if (b >= 0) off = blobs[b].off;
        }
        if (off < 0) {
            if (strcmp(op->sym, "stderr") == 0) off = extern_off[0];
            else if (strcmp(op->sym, "stdout") == 0) off = extern_off[1];
            else if (strcmp(op->sym, "stdin") == 0) off = extern_off[2];
        }
        if (off < 0) {
            int f = cvm_find_func(op->sym);
            if (f >= 0) {
                eimm(f);
                return;
            }
            fprintf(stderr, "ld: undefined symbol %s\n", op->sym);
            error_count++;
            return;
        }
        e1(OP_LEA_DATA);
        e4(off);
        return;
    }
    if (op->kind == K_MEM) {
        elea_mem(op);
        return;
    }
    die("invalid lea operand");
}

static void epush_value(Op *op, int size) {
    if (op->kind == K_REG) {
        epush_reg(op->reg);
        if (op->regsz == 4) push_mask32();
        else if (op->regsz == 1) push_mask8();
        return;
    }
    if (op->kind == K_IMM) {
        eimm(op->imm);
        return;
    }
    if (op->kind == K_MEM || op->kind == K_SYM) {
        elea_operand(op);
        if (size == 8) e1(OP_LOAD64);
        else if (size == 4) e1(OP_LOAD32);
        else if (size == 2) e1(OP_LOAD16);
        else e1(OP_LOAD8);
        return;
    }
    if (op->kind == K_SYM_IMM) {
        elea_operand(op);
        return;
    }
    die("invalid operand");
}

static void signext8(void) {
    eimm(255); e1(OP_AND);
    eimm(128); e1(OP_XOR);
    eimm(128); e1(OP_SUB);
}

static void signext32(void) {
    eimm(4294967295L); e1(OP_AND);
    eimm(2147483648L); e1(OP_XOR);
    eimm(2147483648L); e1(OP_SUB);
}

static void signext16(void) {
    eimm(65535); e1(OP_AND);
    eimm(32768); e1(OP_XOR);
    eimm(32768); e1(OP_SUB);
}

static void mov(int size, Op *s, Op *d) {
    if (d->kind == K_REG) {
        if (s->kind == K_REG) {
            epush_reg(s->reg);
            if (size == 4) push_mask32();
            else if (size == 1) push_mask8();
            estore_reg(d->reg);
        } else if (s->kind == K_IMM) {
            eimm(s->imm);
            if (size == 4) push_mask32();
            else if (size == 1) push_mask8();
            estore_reg(d->reg);
        } else if (s->kind == K_SYM_IMM) {
            elea_operand(s);
            estore_reg(d->reg);
        } else if (s->kind == K_MEM || s->kind == K_SYM) {
            elea_operand(s);
            if (size == 8) { e1(OP_LOAD64); estore_reg(d->reg); }
            else if (size == 4) { e1(OP_LOAD32); push_mask32(); estore_reg(d->reg); }
            else {
                e1(OP_LOAD8);
                push_mask8();
                estore_local(CFG_SLOT_S0);
                epush_reg(d->reg);
                eimm(-256);
                e1(OP_AND);
                epush_local(CFG_SLOT_S0);
                e1(OP_OR);
                estore_reg(d->reg);
            }
        } else die("invalid mov source");
        return;
    }
    if (d->kind == K_MEM || d->kind == K_SYM) {
        if (s->kind == K_REG) {
            epush_reg(s->reg);
            if (size == 4) push_mask32();
            else if (size == 1) push_mask8();
            estore_local(CFG_SLOT_S0);
            elea_operand(d);
            epush_local(CFG_SLOT_S0);
            if (size == 8) e1(OP_STORE64);
            else if (size == 4) e1(OP_STORE32);
            else e1(OP_STORE8);
        } else if (s->kind == K_IMM) {
            eimm(s->imm);
            if (size == 4) push_mask32();
            else if (size == 1) push_mask8();
            estore_local(CFG_SLOT_S0);
            elea_operand(d);
            epush_local(CFG_SLOT_S0);
            if (size == 8) e1(OP_STORE64);
            else if (size == 4) e1(OP_STORE32);
            else e1(OP_STORE8);
        } else die("invalid mov source");
        return;
    }
    die("invalid mov destination");
}

static void arith_mem(int opc, int size, Op *d, Op *s) {
    elea_operand(d);
    if (size == 8) e1(OP_LOAD64);
    else if (size == 4) e1(OP_LOAD32);
    else e1(OP_LOAD8);
    epush_value(s, size);
    e1(opc);
    if (size == 4) push_mask32();
    else if (size == 1) push_mask8();
    estore_local(CFG_SLOT_S0);
    elea_operand(d);
    epush_local(CFG_SLOT_S0);
    if (size == 8) e1(OP_STORE64);
    else if (size == 4) e1(OP_STORE32);
    else e1(OP_STORE8);
}

static void arith_reg(int opc, int size, Op *d, Op *s) {
    if (s->kind == K_IMM || s->kind == K_REG) {
        epush_value(s, size);
        epush_reg(d->reg);
        e1(opc);
        if (size == 4) push_mask32();
        else if (size == 1) push_mask8();
        estore_reg(d->reg);
    } else if (s->kind == K_MEM || s->kind == K_SYM) {
        elea_operand(s);
        if (size == 8) e1(OP_LOAD64);
        else if (size == 4) e1(OP_LOAD32);
        else e1(OP_LOAD8);
        epush_reg(d->reg);
        e1(opc);
        if (size == 4) push_mask32();
        else if (size == 1) push_mask8();
        estore_reg(d->reg);
    } else if (s->kind == K_SYM_IMM) {
        elea_operand(s);
        epush_reg(d->reg);
        e1(opc);
        if (size == 4) push_mask32();
        else if (size == 1) push_mask8();
        estore_reg(d->reg);
    } else die("invalid arithmetic operand");
}

static void cvm_push_cmpval(Op *o, int size) {
    epush_value(o, size);
    if (size == 4) signext32();
    else if (size == 1) signext8();
}

static void cvm_cmp(int size, Op *o1, Op *o2) {
    cvm_push_cmpval(o1, size);
    estore_local(CFG_SLOT_FLAGS_A);
    cvm_push_cmpval(o2, size);
    estore_local(CFG_SLOT_FLAGS_B);
}

static void cvm_translate(const char *mn, Op *o1, Op *o2) {
    if (strcmp(mn, "movq") == 0) { mov(8, o1, o2); return; }
    if (strcmp(mn, "movl") == 0) { mov(4, o1, o2); return; }
    if (strcmp(mn, "movb") == 0) { mov(1, o1, o2); return; }
    if (strcmp(mn, "movw") == 0) {
        if (o2->kind == K_REG) {
            if (o1->kind == K_REG) { epush_reg(o1->reg); push_mask16(); estore_reg(o2->reg); }
            else if (o1->kind == K_IMM) { eimm(o1->imm); push_mask16(); estore_reg(o2->reg); }
            else if (o1->kind == K_SYM_IMM) { elea_operand(o1); push_mask16(); estore_reg(o2->reg); }
            else { elea_operand(o1); e1(OP_LOAD16); estore_reg(o2->reg); }
        } else {
            if (o1->kind == K_REG) {
                epush_reg(o1->reg); push_mask16(); estore_local(CFG_SLOT_S0);
                elea_operand(o2); epush_local(CFG_SLOT_S0); e1(OP_STORE16);
            } else if (o1->kind == K_IMM) {
                eimm(o1->imm); push_mask16(); estore_local(CFG_SLOT_S0);
                elea_operand(o2); epush_local(CFG_SLOT_S0); e1(OP_STORE16);
            } else die("invalid movw source");
        }
        return;
    }
    if (strcmp(mn, "movzbq") == 0) {
        if (o1->kind == K_REG) { epush_reg(o1->reg); push_mask8(); estore_reg(o2->reg); }
        else { elea_operand(o1); e1(OP_LOAD8); push_mask8(); estore_reg(o2->reg); }
        return;
    }
    if (strcmp(mn, "movzbl") == 0) {
        if (o1->kind == K_REG) { epush_reg(o1->reg); push_mask8(); estore_reg(o2->reg); }
        else { elea_operand(o1); e1(OP_LOAD8); push_mask8(); estore_reg(o2->reg); }
        return;
    }
    if (strcmp(mn, "movzwl") == 0) {
        if (o1->kind == K_REG) { epush_reg(o1->reg); push_mask16(); estore_reg(o2->reg); }
        else { elea_operand(o1); e1(OP_LOAD16); push_mask16(); estore_reg(o2->reg); }
        return;
    }
    if (strcmp(mn, "movzwq") == 0) {
        if (o1->kind == K_REG) { epush_reg(o1->reg); push_mask16(); estore_reg(o2->reg); }
        else { elea_operand(o1); e1(OP_LOAD16); push_mask16(); estore_reg(o2->reg); }
        return;
    }
    if (strcmp(mn, "movsbq") == 0) {
        if (o1->kind == K_REG) { epush_reg(o1->reg); signext8(); estore_reg(o2->reg); }
        else { elea_operand(o1); e1(OP_LOAD8); estore_reg(o2->reg); }
        return;
    }
    if (strcmp(mn, "movsbl") == 0) {
        if (o1->kind == K_REG) { epush_reg(o1->reg); signext8(); push_mask32(); estore_reg(o2->reg); }
        else { elea_operand(o1); e1(OP_LOAD8); push_mask32(); estore_reg(o2->reg); }
        return;
    }
    if (strcmp(mn, "movswl") == 0) {
        if (o1->kind == K_REG) { epush_reg(o1->reg); signext16(); push_mask32(); estore_reg(o2->reg); }
        else { elea_operand(o1); e1(OP_LOAD16); push_mask32(); estore_reg(o2->reg); }
        return;
    }
    if (strcmp(mn, "movswq") == 0) {
        if (o1->kind == K_REG) { epush_reg(o1->reg); signext16(); estore_reg(o2->reg); }
        else { elea_operand(o1); e1(OP_LOAD16); signext16(); estore_reg(o2->reg); }
        return;
    }
    if (strcmp(mn, "movzwq") == 0) {
        if (o1->kind == K_REG) { epush_reg(o1->reg); push_mask16(); estore_reg(o2->reg); }
        else { elea_operand(o1); e1(OP_LOAD32); push_mask16(); estore_reg(o2->reg); }
        return;
    }
    if (strcmp(mn, "movslq") == 0) {
        if (o1->kind == K_REG) { epush_reg(o1->reg); signext32(); estore_reg(o2->reg); }
        else { elea_operand(o1); e1(OP_LOAD32); estore_reg(o2->reg); }
        return;
    }
    if (strcmp(mn, "leaq") == 0) {
        elea_operand(o1);
        estore_reg(o2->reg);
        return;
    }
    if (strcmp(mn, "pushq") == 0) {
        epush_value(o1, 8);
        estore_local(CFG_SLOT_S0);
        epush_global(CFG_GSLOT_RSP);
        eimm(8);
        e1(OP_SUB);
        estore_global(CFG_GSLOT_RSP);
        epush_global(CFG_GSLOT_RSP);
        epush_local(CFG_SLOT_S0);
        e1(OP_STORE64);
        return;
    }
    if (strcmp(mn, "popq") == 0) {
        epush_global(CFG_GSLOT_RSP);
        e1(OP_LOAD64);
        estore_reg(o1->reg);
        epush_global(CFG_GSLOT_RSP);
        eimm(8);
        e1(OP_ADD);
        estore_global(CFG_GSLOT_RSP);
        return;
    }
    if (strcmp(mn, "addq") == 0) {
        if (o2->kind == K_REG) arith_reg(OP_ADD, 8, o2, o1);
        else arith_mem(OP_ADD, 8, o2, o1);
        return;
    }
    if (strcmp(mn, "addl") == 0) {
        if (o2->kind == K_REG) {
            epush_reg(o2->reg);
            epush_value(o1, 4);
            e1(OP_ADD);
            push_mask32();
            estore_reg(o2->reg);
        } else arith_mem(OP_ADD, 4, o2, o1);
        return;
    }
    if (strcmp(mn, "subq") == 0) {
        if (o2->kind == K_REG) {
            epush_reg(o2->reg);
            epush_value(o1, 8);
            e1(OP_SUB);
            estore_reg(o2->reg);
        } else {
            elea_operand(o2);
            e1(OP_LOAD64);
            epush_value(o1, 8);
            e1(OP_SUB);
            estore_local(CFG_SLOT_S0);
            elea_operand(o2);
            epush_local(CFG_SLOT_S0);
            e1(OP_STORE64);
        }
        return;
    }
    if (strcmp(mn, "subl") == 0) {
        if (o2->kind == K_REG) {
            epush_reg(o2->reg);
            epush_value(o1, 4);
            e1(OP_SUB);
            push_mask32();
            estore_reg(o2->reg);
        } else arith_mem(OP_SUB, 4, o2, o1);
        return;
    }
    if (strcmp(mn, "addb") == 0) {
        if (o2->kind == K_REG) {
            epush_reg(o2->reg);
            epush_value(o1, 1);
            e1(OP_ADD);
            push_mask8();
            estore_reg(o2->reg);
        } else arith_mem(OP_ADD, 1, o2, o1);
        return;
    }
    if (strcmp(mn, "imulq") == 0) {
        epush_reg(o2->reg);
        epush_value(o1, 8);
        e1(OP_MUL);
        estore_reg(o2->reg);
        return;
    }
    if (strcmp(mn, "imull") == 0) {
        epush_reg(o2->reg);
        epush_value(o1, 4);
        e1(OP_MUL);
        push_mask32();
        estore_reg(o2->reg);
        return;
    }
    if (strcmp(mn, "negq") == 0) {
        epush_reg(o1->reg);
        e1(OP_NEG);
        estore_reg(o1->reg);
        return;
    }
    if (strcmp(mn, "notq") == 0) {
        epush_reg(o1->reg);
        e1(OP_NOT);
        estore_reg(o1->reg);
        return;
    }
    if (strcmp(mn, "incq") == 0) {
        if (o1->kind == K_REG) {
            epush_reg(o1->reg);
            eimm(1);
            e1(OP_ADD);
            estore_reg(o1->reg);
        } else if (o1->kind == K_MEM || o1->kind == K_SYM) {
            elea_operand(o1);
            e1(OP_LOAD64);
            eimm(1);
            e1(OP_ADD);
            estore_local(CFG_SLOT_S0);
            elea_operand(o1);
            epush_local(CFG_SLOT_S0);
            e1(OP_STORE64);
        } else die("invalid operand");
        return;
    }
    if (strcmp(mn, "decq") == 0) {
        if (o1->kind == K_REG) {
            epush_reg(o1->reg);
            eimm(1);
            e1(OP_SUB);
            estore_reg(o1->reg);
        } else if (o1->kind == K_MEM || o1->kind == K_SYM) {
            elea_operand(o1);
            e1(OP_LOAD64);
            eimm(1);
            e1(OP_SUB);
            estore_local(CFG_SLOT_S0);
            elea_operand(o1);
            epush_local(CFG_SLOT_S0);
            e1(OP_STORE64);
        } else die("invalid operand");
        return;
    }
    if (strcmp(mn, "andq") == 0) {
        if (o2->kind == K_REG) arith_reg(OP_AND, 8, o2, o1);
        else arith_mem(OP_AND, 8, o2, o1);
        return;
    }
    if (strcmp(mn, "andl") == 0) {
        if (o2->kind == K_REG) {
            epush_reg(o2->reg);
            epush_value(o1, 4);
            e1(OP_AND);
            push_mask32();
            estore_reg(o2->reg);
        } else arith_mem(OP_AND, 4, o2, o1);
        return;
    }
    if (strcmp(mn, "orq") == 0) {
        if (o2->kind == K_REG) arith_reg(OP_OR, 8, o2, o1);
        else arith_mem(OP_OR, 8, o2, o1);
        return;
    }
    if (strcmp(mn, "orl") == 0) {
        if (o2->kind == K_REG) {
            epush_reg(o2->reg);
            epush_value(o1, 4);
            e1(OP_OR);
            push_mask32();
            estore_reg(o2->reg);
        } else arith_mem(OP_OR, 4, o2, o1);
        return;
    }
    if (strcmp(mn, "xorq") == 0) {
        if (o1->kind == K_REG && o2->kind == K_REG && o1->reg == o2->reg) {
            eimm(0);
            estore_reg(o2->reg);
        } else {
            epush_reg(o2->reg);
            epush_value(o1, 8);
            e1(OP_XOR);
            estore_reg(o2->reg);
        }
        return;
    }
    if (strcmp(mn, "xorl") == 0) {
        if (o1->kind == K_REG && o2->kind == K_REG && o1->reg == o2->reg) {
            eimm(0);
            estore_reg(o2->reg);
        } else {
            epush_reg(o2->reg);
            epush_value(o1, 4);
            e1(OP_XOR);
            push_mask32();
            estore_reg(o2->reg);
        }
        return;
    }
    if (strcmp(mn, "salq") == 0) {
        epush_reg(o2->reg);
        epush_value(o1, 8);
        e1(OP_SHL);
        estore_reg(o2->reg);
        return;
    }
    if (strcmp(mn, "shll") == 0) {
        epush_reg(o2->reg);
        epush_value(o1, 4);
        e1(OP_SHL);
        push_mask32();
        estore_reg(o2->reg);
        return;
    }
    if (strcmp(mn, "sall") == 0) {
        epush_reg(o2->reg);
        epush_value(o1, 4);
        e1(OP_SHL);
        push_mask32();
        estore_reg(o2->reg);
        return;
    }
    if (strcmp(mn, "shrl") == 0) {
        epush_reg(o2->reg);
        epush_value(o1, 4);
        e1(OP_USHR);
        push_mask32();
        estore_reg(o2->reg);
        return;
    }
    if (strcmp(mn, "sarl") == 0) {
        epush_reg(o2->reg);
        signext32();
        epush_value(o1, 4);
        e1(OP_SHR);
        push_mask32();
        estore_reg(o2->reg);
        return;
    }
    if (strcmp(mn, "shrq") == 0) {
        epush_reg(o2->reg);
        epush_value(o1, 8);
        e1(OP_USHR);
        estore_reg(o2->reg);
        return;
    }
    if (strcmp(mn, "sarq") == 0) {
        epush_reg(o2->reg);
        epush_value(o1, 8);
        e1(OP_SHR);
        estore_reg(o2->reg);
        return;
    }
    if (strcmp(mn, "testq") == 0) {
        epush_value(o1, 8);
        epush_value(o2, 8);
        e1(OP_AND);
        estore_local(CFG_SLOT_FLAGS_A);
        eimm(0);
        estore_local(CFG_SLOT_FLAGS_B);
        return;
    }
    if (strcmp(mn, "testl") == 0) {
        cvm_push_cmpval(o1, 4);
        cvm_push_cmpval(o2, 4);
        e1(OP_AND);
        estore_local(CFG_SLOT_FLAGS_A);
        eimm(0);
        estore_local(CFG_SLOT_FLAGS_B);
        return;
    }
    if (strcmp(mn, "cmpq") == 0) {
        epush_value(o1, 8);
        estore_local(CFG_SLOT_FLAGS_A);
        epush_value(o2, 8);
        estore_local(CFG_SLOT_FLAGS_B);
        return;
    }
    if (strcmp(mn, "cmpl") == 0) {
        cvm_cmp(4, o1, o2);
        return;
    }
    if (strcmp(mn, "cmpb") == 0) {
        cvm_cmp(1, o1, o2);
        return;
    }
    if (strcmp(mn, "sete") == 0) {
        epush_local(CFG_SLOT_FLAGS_A); epush_local(CFG_SLOT_FLAGS_B); e1(OP_CMP_EQ); estore_reg(o1->reg);
        return;
    }
    if (strcmp(mn, "setne") == 0) {
        epush_local(CFG_SLOT_FLAGS_A); epush_local(CFG_SLOT_FLAGS_B); e1(OP_CMP_NE); estore_reg(o1->reg);
        return;
    }
    if (strcmp(mn, "setl") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_LT); estore_reg(o1->reg);
        return;
    }
    if (strcmp(mn, "setg") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_GT); estore_reg(o1->reg);
        return;
    }
    if (strcmp(mn, "setle") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_LE); estore_reg(o1->reg);
        return;
    }
    if (strcmp(mn, "setge") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_GE); estore_reg(o1->reg);
        return;
    }
    if (strcmp(mn, "seta") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_UGT); estore_reg(o1->reg);
        return;
    }
    if (strcmp(mn, "setae") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_UGE); estore_reg(o1->reg);
        return;
    }
    if (strcmp(mn, "setb") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_ULT); estore_reg(o1->reg);
        return;
    }
    if (strcmp(mn, "setbe") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_ULE); estore_reg(o1->reg);
        return;
    }
    if (strcmp(mn, "je") == 0) {
        epush_local(CFG_SLOT_FLAGS_A); epush_local(CFG_SLOT_FLAGS_B); e1(OP_CMP_EQ); ejnz(o1->sym);
        return;
    }
    if (strcmp(mn, "jne") == 0) {
        epush_local(CFG_SLOT_FLAGS_A); epush_local(CFG_SLOT_FLAGS_B); e1(OP_CMP_NE); ejnz(o1->sym);
        return;
    }
    if (strcmp(mn, "jl") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_LT); ejnz(o1->sym);
        return;
    }
    if (strcmp(mn, "jg") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_GT); ejnz(o1->sym);
        return;
    }
    if (strcmp(mn, "jle") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_LE); ejnz(o1->sym);
        return;
    }
    if (strcmp(mn, "jge") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_GE); ejnz(o1->sym);
        return;
    }
    if (strcmp(mn, "ja") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_UGT); ejnz(o1->sym);
        return;
    }
    if (strcmp(mn, "jae") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_UGE); ejnz(o1->sym);
        return;
    }
    if (strcmp(mn, "jnb") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_UGE); ejnz(o1->sym);
        return;
    }
    if (strcmp(mn, "jb") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_ULT); ejnz(o1->sym);
        return;
    }
    if (strcmp(mn, "jbe") == 0) {
        epush_local(CFG_SLOT_FLAGS_B); epush_local(CFG_SLOT_FLAGS_A); e1(OP_CMP_ULE); ejnz(o1->sym);
        return;
    }
    if (strcmp(mn, "jmp") == 0) { ejmp(o1->sym); return; }
    if (strcmp(mn, "call") == 0) {
        if (o1->kind == K_IND) {
            epush_local(5); estore_global(CFG_GSLOT_ARGS + 0);
            epush_local(4); estore_global(CFG_GSLOT_ARGS + 1);
            epush_local(2); estore_global(CFG_GSLOT_ARGS + 2);
            epush_local(1); estore_global(CFG_GSLOT_ARGS + 3);
            epush_local(6); estore_global(CFG_GSLOT_ARGS + 4);
            epush_local(7); estore_global(CFG_GSLOT_ARGS + 5);
            epush_reg(o1->reg);
            e1(OP_CALL_INDIRECT);
            epush_global(CFG_GSLOT_RET);
            estore_local(0);
            return;
        }
        int f = cvm_find_func(o1->sym);
        if (f >= 0) {
            epush_local(5); estore_global(CFG_GSLOT_ARGS + 0);
            epush_local(4); estore_global(CFG_GSLOT_ARGS + 1);
            epush_local(2); estore_global(CFG_GSLOT_ARGS + 2);
            epush_local(1); estore_global(CFG_GSLOT_ARGS + 3);
            epush_local(6); estore_global(CFG_GSLOT_ARGS + 4);
            epush_local(7); estore_global(CFG_GSLOT_ARGS + 5);
            e1(OP_CALL);
            e4(f);
            e1(0);
            epush_global(CFG_GSLOT_RET);
            estore_local(0);
            return;
        }
        int n = cvm_add_nat(o1->sym);
        epush_local(5);
        epush_local(4);
        epush_local(2);
        epush_local(1);
        epush_local(6);
        epush_local(7);
        e1(OP_CALL_NATIVE);
        e4(n);
        e1(6);
        estore_local(0);
        return;
    }
    if (strcmp(mn, "ret") == 0) {
        epush_local(0);
        estore_global(CFG_GSLOT_RET);
        epush_local(0);
        e1(OP_RET);
        term_pos = code_len - 1;
        return;
    }
    if (strcmp(mn, "leave") == 0) {
        epush_global(CFG_GSLOT_RBP);
        estore_global(CFG_GSLOT_RSP);
        epush_global(CFG_GSLOT_RSP);
        e1(OP_LOAD64);
        estore_global(CFG_GSLOT_RBP);
        epush_global(CFG_GSLOT_RSP);
        eimm(8);
        e1(OP_ADD);
        estore_global(CFG_GSLOT_RSP);
        return;
    }
    if (strcmp(mn, "idivq") == 0) {
        epush_local(0); estore_local(CFG_SLOT_S0);
        epush_value(o1, 8); estore_local(CFG_SLOT_S1);
        epush_local(CFG_SLOT_S0); epush_local(CFG_SLOT_S1); e1(OP_DIV); estore_local(0);
        epush_local(CFG_SLOT_S0); epush_local(CFG_SLOT_S1); e1(OP_MOD); estore_local(2);
        return;
    }
    if (strcmp(mn, "divq") == 0) {
        epush_local(0); estore_local(CFG_SLOT_S0);
        epush_value(o1, 8); estore_local(CFG_SLOT_S1);
        epush_local(CFG_SLOT_S0); epush_local(CFG_SLOT_S1); e1(OP_UDIV); estore_local(0);
        epush_local(CFG_SLOT_S0); epush_local(CFG_SLOT_S1); e1(OP_UMOD); estore_local(2);
        return;
    }
    if (strcmp(mn, "cqto") == 0) {
        epush_local(0);
        eimm(63);
        e1(OP_SHR);
        estore_local(2);
        return;
    }
    if (strcmp(mn, "cltd") == 0) {
        epush_local(0);
        signext32();
        eimm(31);
        e1(OP_SHR);
        estore_local(2);
        return;
    }
    if (strcmp(mn, "cwtl") == 0) {
        epush_local(0);
        signext16();
        push_mask32();
        estore_local(0);
        return;
    }
    if (strcmp(mn, "cdqe") == 0) {
        epush_local(0);
        signext32();
        estore_local(0);
        return;
    }
    if (strcmp(mn, "idivl") == 0) {
        epush_local(0); signext32(); estore_local(CFG_SLOT_S0);
        epush_value(o1, 4); signext32(); estore_local(CFG_SLOT_S1);
        epush_local(CFG_SLOT_S0); epush_local(CFG_SLOT_S1); e1(OP_DIV); push_mask32(); estore_local(0);
        epush_local(CFG_SLOT_S0); epush_local(CFG_SLOT_S1); e1(OP_MOD); push_mask32(); estore_local(2);
        return;
    }
    if (strcmp(mn, "divl") == 0) {
        epush_local(0); push_mask32(); estore_local(CFG_SLOT_S0);
        epush_value(o1, 4); push_mask32(); estore_local(CFG_SLOT_S1);
        epush_local(CFG_SLOT_S0); epush_local(CFG_SLOT_S1); e1(OP_UDIV); push_mask32(); estore_local(0);
        epush_local(CFG_SLOT_S0); epush_local(CFG_SLOT_S1); e1(OP_UMOD); push_mask32(); estore_local(2);
        return;
    }
    if (strcmp(mn, "syscall") == 0) {
        char l1[64], l2[64], l3[64], lend[64];
        sprintf(l1, "..S%lda", synth_n);
        sprintf(l2, "..S%ldb", synth_n);
        sprintf(l3, "..S%ldc", synth_n);
        sprintf(lend, "..S%ldd", synth_n);
        synth_n++;
        epush_local(0); eimm(60); e1(OP_CMP_EQ); ejz(l1);
        epush_local(5);
        e1(OP_SYSCALL); e1(60); e1(1);
        ejmp(lend);
        add_label(l1, code_len);
        epush_local(0); eimm(1); e1(OP_CMP_EQ); ejz(l2);
        epush_local(5); epush_local(4); epush_local(2);
        e1(OP_SYSCALL); e1(1); e1(3);
        estore_local(0);
        ejmp(lend);
        add_label(l2, code_len);
        epush_local(0); eimm(0); e1(OP_CMP_EQ); ejz(l3);
        epush_local(5); epush_local(4); epush_local(2);
        e1(OP_SYSCALL); e1(0); e1(3);
        estore_local(0);
        ejmp(lend);
        add_label(l3, code_len);
        eimm(-1);
        estore_local(0);
        add_label(lend, code_len);
        return;
    }
    if (strcmp(mn, "nop") == 0) { e1(OP_NOP); return; }
    fprintf(stderr, "ld: %s:%ld: unsupported instruction '%s'\n", cur_file, cur_line, mn);
    error_count++;
}

static void cvm_prepare_tables(void) {
    n_funcs = 0;
    n_globals = 0;
    n_blobs = 0;
    for (int i = 0; i < n_syms; i++) {
        Sym *s = &syms[i];
        if (s->kind == SYM_FUNC) {
            if (n_funcs >= CFG_MAX_SYMBOLS) { die("too many functions"); return; }
            name_copy(funcs[n_funcs].name, s->name);
            funcs[n_funcs].code_off = 0;
            funcs[n_funcs].num_locals = CFG_REG_LOCALS;
            funcs[n_funcs].argc = 0;
            s->fidx = n_funcs;
            n_funcs++;
        } else if (s->kind == SYM_GLOBAL) {
            if (n_globals >= CFG_MAX_SYMBOLS) { die("too many globals"); return; }
            name_copy(globals[n_globals].name, s->name);
            globals[n_globals].off = 0;
            globals[n_globals].size = s->size;
            globals[n_globals].base = -1;
            n_globals++;
        } else if (s->kind == SYM_BLOB) {
            if (n_blobs >= CFG_MAX_SYMBOLS) { die("too many data blobs"); return; }
            name_copy(blobs[n_blobs].name, s->name);
            blobs[n_blobs].off = 0;
            blobs[n_blobs].base = s->base;
            blobs[n_blobs].end = s->end;
            n_blobs++;
        }
    }
}

static void cvm_layout_data(void) {
    /* ABI area first (argc, argv, rsp, rbp, args, ret, stack size, stack
     * base), then globals, blobs and extern slots, then the x86 stack
     * region last. The interpreter writes the argv array at the very top
     * of the stack region, so the reserved headroom keeps it clear of the
     * global/blob data instead of overwriting string constants (the fault
     * a fixed ABI-first layout had when a module carried both arguments
     * and blobs). */
    data_fill(CFG_ABI_BYTES, 0);
    while (data_len < CFG_ABI_BYTES) data_put(0);
    for (int i = 0; i < n_globals; i++) {
        data_align(8);
        globals[i].off = data_len;
        data_fill(globals[i].size, 0);
        if (globals[i].base >= 0) {
            long n = globals[i].size;
            if (globals[i].base + n > blob_len) n = blob_len - globals[i].base;
            if (n > 0) memcpy(data_region + globals[i].off, blob_data + globals[i].base, (size_t)n);
        }
    }
    for (int i = 0; i < n_blobs; i++) {
        blobs[i].off = data_len;
        long n = blobs[i].end - blobs[i].base;
        if (n < 0) n = 0;
        breserve(&data_region, &data_cap, data_len + n);
        memcpy(data_region + data_len, blob_data + blobs[i].base, (size_t)n);
        data_len += n;
    }
    for (int i = 0; i < 3; i++) {
        data_align(8);
        extern_off[i] = data_len;
        data_fill(8, 0);
    }
    long stack_base = data_len;
    long stack_size = g_xstack + (long)(CFG_MAX_ARGS + 1) * 8;
    breserve(&data_region, &data_cap, data_len + stack_size);
    memset(data_region + data_len, 0, (size_t)stack_size);
    data_len += stack_size;
    unsigned char szb[8];
    for (int i = 0; i < 8; i++) szb[i] = (unsigned char)((g_xstack >> (i * 8)) & 255);
    memcpy(data_region + 88, szb, 8);
    for (int i = 0; i < 8; i++) szb[i] = (unsigned char)((stack_base >> (i * 8)) & 255);
    memcpy(data_region + 96, szb, 8);
}

static void func_glue(void) {
    epush_global(CFG_GSLOT_ARGS + 0); estore_local(5);
    epush_global(CFG_GSLOT_ARGS + 1); estore_local(4);
    epush_global(CFG_GSLOT_ARGS + 2); estore_local(2);
    epush_global(CFG_GSLOT_ARGS + 3); estore_local(1);
    epush_global(CFG_GSLOT_ARGS + 4); estore_local(6);
    epush_global(CFG_GSLOT_ARGS + 5); estore_local(7);
}

static void entry_glue(void) {
    static const char *addr_nats[3] = {"stderr_addr", "stdout_addr", "stdin_addr"};
    for (int i = 0; i < 3; i++) {
        if (!extern_used[i]) continue;
        int n = cvm_add_nat(addr_nats[i]);
        e1(OP_LEA_DATA);
        e4(extern_off[i]);
        e1(OP_CALL_NATIVE);
        e4(n);
        e1(0);
        e1(OP_STORE64);
    }
}

static void cvm_encode(LineSrc *src) {
    char line[CFG_LINE_MAX];
    int section = SEC_TEXT;
    int in_func = -1;
    long lineno = 0;
    n_labels = 0;
    n_fixups = 0;
    term_pos = -1;
    while (ls_getline(src, line, sizeof(line))) {
        lineno++;
        cur_line = lineno;
        char *t = trim(line);
        strip_comment(t);
        if (!*t) continue;
        long n = (long)strlen(t);
        if (t[n - 1] == ':') {
            t[n - 1] = 0;
            if (section == SEC_TEXT) {
                int fi = cvm_find_func(t);
                if (fi >= 0) {
                    if (in_func >= 0) resolve_fixups();
                    in_func = fi;
                    funcs[fi].code_off = code_len;
                    if (fi == entry_func) entry_glue();
                    func_glue();
                } else if (in_func >= 0) {
                    add_label(t, code_len);
                }
            }
            continue;
        }
        if (t[0] == '.') {
            int s = set_section(t);
            if (s >= 0) {
                section = s;
                if (section != SEC_TEXT && in_func >= 0) {
                    resolve_fixups();
                    in_func = -1;
                }
            }
            continue;
        }
        if (section != SEC_TEXT || in_func < 0) continue;
        char *sp = t;
        while (*sp && *sp != ' ' && *sp != '\t') sp++;
        char mn[64];
        long ml = sp - t;
        if (ml >= 63) ml = 63;
        memcpy(mn, t, (size_t)ml);
        mn[ml] = 0;
        char *rest = sp;
        while (*rest == ' ' || *rest == '\t') rest++;
        Op o1, o2;
        memset(&o1, 0, sizeof(o1));
        memset(&o2, 0, sizeof(o2));
        if (*rest) {
            char o1s[CFG_LINE_MAX], o2s[CFG_LINE_MAX];
            int no = split_operands(rest, o1s, o2s);
            parse_operand(o1s, &o1);
            if (no == 2) parse_operand(o2s, &o2);
        }
        cvm_translate(mn, &o1, &o2);
    }
    if (in_func >= 0) resolve_fixups();
    if (code_len > 0 && term_pos != code_len - 1) {
        e1(OP_HALT);
        term_pos = code_len - 1;
    }
    if (error_count) {
        fprintf(stderr, "ld: %d error(s)\n", error_count);
        exit(1);
    }
}

static void w32(unsigned char *p, long v) {
    p[0] = (unsigned char)(v & 255);
    p[1] = (unsigned char)((v >> 8) & 255);
    p[2] = (unsigned char)((v >> 16) & 255);
    p[3] = (unsigned char)((v >> 24) & 255);
}

static void w16(unsigned char *p, long v) {
    p[0] = (unsigned char)(v & 255);
    p[1] = (unsigned char)((v >> 8) & 255);
}

static void w64_at(unsigned char *p, unsigned long long v) {
    for (int i = 0; i < 8; i++) {
        p[i] = (unsigned char)(v & 255);
        v >>= 8;
    }
}

static void cvm_write_module(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "ld: cannot write %s\n", path);
        exit(1);
    }

    long *func_name_off = (long *)malloc(sizeof(long) * (size_t)(n_funcs + 1));
    long *global_name_off = (long *)malloc(sizeof(long) * (size_t)(n_globals + 1));
    long *nat_name_off = (long *)malloc(sizeof(long) * (size_t)(n_nats + 1));
    if (!func_name_off || !global_name_off || !nat_name_off) {
        fprintf(stderr, "ld: out of memory\n");
        exit(1);
    }
    for (int i = 0; i < n_funcs; i++) func_name_off[i] = pool_add(funcs[i].name);
    for (int i = 0; i < n_globals; i++) global_name_off[i] = pool_add(globals[i].name);
    for (int i = 0; i < n_nats; i++) nat_name_off[i] = pool_add(nats[i].name);

    unsigned char hdr[CFG_CVM_HDR_SIZE];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = CFG_CVM_MAGIC_0;
    hdr[1] = CFG_CVM_MAGIC_1;
    hdr[2] = CFG_CVM_MAGIC_2;
    hdr[3] = CFG_CVM_MAGIC_3;
    hdr[4] = CFG_CVM_VER_MAJ;
    hdr[5] = 0;
    hdr[6] = CFG_CVM_VER_MIN;
    hdr[7] = 0;
    w32(hdr + 8, n_funcs);
    w32(hdr + 12, n_globals);
    w32(hdr + 16, n_nats);
    w32(hdr + 20, 0);
    w32(hdr + 24, code_len);
    w32(hdr + 28, pool_len);
    w32(hdr + 32, data_len);
    w32(hdr + 36, entry_func);
    fwrite(hdr, 1, CFG_CVM_HDR_SIZE, f);

    for (int i = 0; i < n_funcs; i++) {
        unsigned char fe[20];
        memset(fe, 0, 20);
        w32(fe + 0, func_name_off[i]);
        w32(fe + 4, funcs[i].code_off);
        w32(fe + 8, funcs[i].num_locals);
        w32(fe + 12, funcs[i].argc);
        fwrite(fe, 1, 20, f);
    }

    for (int i = 0; i < n_globals; i++) {
        unsigned char ge[8];
        w32(ge + 0, global_name_off[i]);
        w32(ge + 4, globals[i].size);
        fwrite(ge, 1, 8, f);
    }

    for (int i = 0; i < n_nats; i++) {
        unsigned char ne[4];
        w32(ne + 0, nat_name_off[i]);
        fwrite(ne, 1, 4, f);
    }

    fwrite(code, 1, (size_t)code_len, f);
    long i = 0;
    while (i < data_len) {
        long z = 0;
        while (i + z < data_len && data_region[i + z] == 0 && z < 254) z++;
        if (z > 0) {
            fputc((int)(z - 1), f);
            i += z;
            continue;
        }
        long l = 0;
        while (i + l < data_len && l < 255 && data_region[i + l] != 0) l++;
        fputc(254, f);
        fputc((int)l, f);
        fwrite(data_region + i, 1, (size_t)l, f);
        i += l;
    }
    fwrite(pool, 1, (size_t)pool_len, f);
    fclose(f);
    free(func_name_off);
    free(global_name_off);
    free(nat_name_off);
}

/* ================================================================
 *  ELF backend
 * ================================================================ */

static const char ELF_STUBS_SRC[] =
    ".text\n"
    ".globl write\n"
    "write:\n"
    "    movq $1, %rax\n"
    "    syscall\n"
    "    cmpq $0, %rax\n"
    "    jge .Lstub_write_ok\n"
    "    negq %rax\n"
    "    movl %eax, errno(%rip)\n"
    "    negq %rax\n"
    ".Lstub_write_ok:\n"
    "    ret\n"
    ".globl read\n"
    "read:\n"
    "    movq $0, %rax\n"
    "    syscall\n"
    "    cmpq $0, %rax\n"
    "    jge .Lstub_read_ok\n"
    "    negq %rax\n"
    "    movl %eax, errno(%rip)\n"
    "    negq %rax\n"
    ".Lstub_read_ok:\n"
    "    ret\n"
    ".globl exit\n"
    "exit:\n"
    "    movq $60, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl exit_group\n"
    "exit_group:\n"
    "    movq $231, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl open\n"
    "open:\n"
    "    movq $2, %rax\n"
    "    syscall\n"
    "    cmpq $0, %rax\n"
    "    jge .Lstub_open_ok\n"
    "    negq %rax\n"
    "    movl %eax, errno(%rip)\n"
    "    negq %rax\n"
    ".Lstub_open_ok:\n"
    "    ret\n"
    ".globl close\n"
    "close:\n"
    "    movq $3, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl lseek\n"
    "lseek:\n"
    "    movq $8, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl brk\n"
    "brk:\n"
    "    movq $12, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl socket\n"
    "socket:\n"
    "    movq $41, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl connect\n"
    "connect:\n"
    "    movq $42, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl sendto\n"
    "sendto:\n"
    "    movq %rcx, %r10\n"
    "    movq $44, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl recvfrom\n"
    "recvfrom:\n"
    "    movq %rcx, %r10\n"
    "    movq $45, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl shutdown\n"
    "shutdown:\n"
    "    movq $48, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl poll\n"
    "poll:\n"
    "    movq $7, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl net_dns_resolve\n"
    "net_dns_resolve:\n"
    "    movq $200, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl tls_handshake\n"
    "tls_handshake:\n"
    "    movq $201, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl tls_send\n"
    "tls_send:\n"
    "    movq $202, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl tls_recv\n"
    "tls_recv:\n"
    "    movq $203, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl lz4_compress\n"
    "lz4_compress:\n"
    "    movq $216, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl lz4_decompress\n"
    "lz4_decompress:\n"
    "    movq $217, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl minios_mouse\n"
    "minios_mouse:\n"
    "    movq $219, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl nk_frame\n"
    "nk_frame:\n"
    "    movq $220, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".bss\n"
    "__malloc_cur:\n"
    "    .space 8\n"
    "__fmt_buf:\n"
    "    .space 512\n"
    ".data\n"
    "stdout:\n"
    "    .quad 1\n"
    "stderr:\n"
    "    .quad 2\n"
    "stdin:\n"
    "    .quad 0\n"
    "errno:\n"
    "    .long 0\n"
    "__stdio_slots:\n"
    "    .zero 40\n";
static const char ELF_STUBS_SRC_E[] =
    ".text\n"
    ".globl malloc\n"
    "malloc:\n"
    "    addq $15, %rdi\n"
    "    andq $-16, %rdi\n"
    "    movq __malloc_cur(%rip), %r10\n"
    "    cmpq $0, %r10\n"
    "    jne .Lstub_malloc_have\n"
    "    subq $8, %rsp\n"
    "    movq %rdi, 0(%rsp)\n"
    "    movq $0, %rdi\n"
    "    movq $12, %rax\n"
    "    syscall\n"
    "    movq %rax, %r10\n"
    "    movq %rax, __malloc_cur(%rip)\n"
    "    movq 0(%rsp), %rdi\n"
    "    addq $8, %rsp\n"
    ".Lstub_malloc_have:\n"
    "    leaq (%r10,%rdi,1), %rdi\n"
    "    movq $12, %rax\n"
    "    syscall\n"
    "    cmpq $-1, %rax\n"
    "    je .Lstub_malloc_fail\n"
    "    movq %rax, __malloc_cur(%rip)\n"
    "    movq %r10, %rax\n"
    "    ret\n"
    ".Lstub_malloc_fail:\n"
    "    movl $12, errno(%rip)\n"
    "    movq $0, %rax\n"
    "    ret\n"
    ".globl free\n"
    "free:\n"
    "    movq $0, %rax\n"
    "    ret\n"
    ".globl fopen\n"
    "fopen:\n"
    "    movb 0(%rsi), %r8b\n"
    "    cmpb $119, %r8b\n"
    "    je .Lstub_fopen_w\n"
    "    movq $0, %rsi\n"
    "    movq $2, %rax\n"
    "    syscall\n"
    "    jmp .Lstub_fopen_reg\n"
    ".Lstub_fopen_w:\n"
    "    movq $577, %rsi\n"
    "    movq $2, %rax\n"
    "    syscall\n"
    ".Lstub_fopen_reg:\n"
    "    cmpq $0, %rax\n"
    "    jl .Lstub_fopen_syserr\n"
    "    movq %rax, %r8\n"
    "    leaq __stdio_slots(%rip), %r9\n"
    "    movq $0, %r10\n"
    ".Lstub_fopen_find:\n"
    "    cmpq $40, %r10\n"
    "    je .Lstub_fopen_fail\n"
    "    movq (%r9,%r10,1), %r11\n"
    "    cmpq $0, %r11\n"
    "    je .Lstub_fopen_got\n"
    "    addq $8, %r10\n"
    "    jmp .Lstub_fopen_find\n"
    ".Lstub_fopen_got:\n"
    "    movq %r8, (%r9,%r10,1)\n"
    "    leaq (%r9,%r10,1), %rax\n"
    "    ret\n"
    ".Lstub_fopen_syserr:\n"
    "    negq %rax\n"
    "    movl %eax, errno(%rip)\n"
    ".Lstub_fopen_fail:\n"
    "    movq $0, %rax\n"
    "    ret\n"
    ".globl fclose\n"
    "fclose:\n"
    "    call .Lstub_resolve_fd\n"
    "    movq %rdi, %r8\n"
    "    movq $3, %rax\n"
    "    syscall\n"
    "    movq $0, %rax\n"
    "    ret\n"
    ".globl fread\n"
    "fread:\n"
    "    movq %rsi, %r10\n"
    "    imulq %rdx, %rsi\n"
    "    movq %rdi, %r9\n"
    "    movq %rcx, %rdi\n"
    "    call .Lstub_resolve_fd\n"
    "    movq %rsi, %rdx\n"
    "    movq %r9, %rsi\n"
    "    movq $0, %rax\n"
    "    syscall\n"
    "    cqto\n"
    "    idivq %r10\n"
    "    ret\n"
    ".globl fwrite\n"
    "fwrite:\n"
    "    movq %rsi, %r10\n"
    "    imulq %rdx, %rsi\n"
    "    movq %rdi, %r9\n"
    "    movq %rcx, %rdi\n"
    "    call .Lstub_resolve_fd\n"
    "    movq %rsi, %rdx\n"
    "    movq %r9, %rsi\n"
    "    movq $1, %rax\n"
    "    syscall\n"
    "    cqto\n"
    "    idivq %r10\n"
    "    ret\n"
    ".globl fseek\n"
    "fseek:\n"
    "    movq %rdx, %rcx\n"
    "    call .Lstub_resolve_fd\n"
    "    movq %rcx, %rdx\n"
    "    movq $8, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl ftell\n"
    "ftell:\n"
    "    call .Lstub_resolve_fd\n"
    "    movq $0, %rsi\n"
    "    movq $1, %rdx\n"
    "    movq $8, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl rewind\n"
    "rewind:\n"
    "    call .Lstub_resolve_fd\n"
    "    movq $0, %rsi\n"
    "    movq $0, %rdx\n"
    "    movq $8, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".globl fputc\n"
    "fputc:\n"
    "    movq %rdi, %r8\n"
    "    movq %rsi, %rdi\n"
    "    call .Lstub_resolve_fd\n"
    "    subq $8, %rsp\n"
    "    movb %r8b, 0(%rsp)\n"
    "    movq %rsp, %rsi\n"
    "    movq $1, %rdx\n"
    "    movq $1, %rax\n"
    "    syscall\n"
    "    addq $8, %rsp\n"
    "    movq %r8, %rax\n"
    "    ret\n";
static const char ELF_STUBS_SRC_D[] =
    ".globl fputs\n"
    "fputs:\n"
    "    movq %rdi, %r10\n"
    "    movq %rsi, %rdi\n"
    "    call .Lstub_resolve_fd\n"
    "    movq %rdi, %r9\n"
    "    movq %r10, %rdi\n"
    "    call strlen\n"
    "    movq %rax, %rdx\n"
    "    movq %r9, %rdi\n"
    "    movq %r10, %rsi\n"
    "    movq $1, %rax\n"
    "    syscall\n"
    "    ret\n"
    ".Lstub_resolve_fd:\n"
    "    cmpq $2, %rdi\n"
    "    jle .Lstub_resolve_done\n"
    "    movq 0(%rdi), %rdi\n"
    ".Lstub_resolve_done:\n"
    "    ret\n"
    ".globl fprintf\n"
    "fprintf:\n"
    "    pushq %rbx\n"
    "    movq %rdi, %rbx\n"
    "    subq $48, %rsp\n"
    "    movq %rdx, 0(%rsp)\n"
    "    movq %rcx, 8(%rsp)\n"
    "    movq %r8, 16(%rsp)\n"
    "    movq %r9, 24(%rsp)\n"
    "    movq $0, 32(%rsp)\n"
    "    movq $0, 40(%rsp)\n"
    "    movq %rsi, %rdx\n"
    "    movq %rsp, %rcx\n"
    "    leaq __fmt_buf(%rip), %rdi\n"
    "    movq $256, %rsi\n"
    "    call .Lstub_vfmt\n"
    "    movq %rax, %rdx\n"
    "    movq %rbx, %rdi\n"
    "    call .Lstub_resolve_fd\n"
    "    leaq __fmt_buf(%rip), %rsi\n"
    "    movq $1, %rax\n"
    "    syscall\n"
    "    addq $48, %rsp\n"
    "    popq %rbx\n"
    "    ret\n"
    ".globl printf\n"
    "printf:\n"
    "    subq $56, %rsp\n"
    "    movq %rsi, 0(%rsp)\n"
    "    movq %rdx, 8(%rsp)\n"
    "    movq %rcx, 16(%rsp)\n"
    "    movq %r8, 24(%rsp)\n"
    "    movq %r9, 32(%rsp)\n"
    "    movq $0, 40(%rsp)\n"
    "    movq $0, 48(%rsp)\n"
    "    movq %rdi, %rdx\n"
    "    movq %rsp, %rcx\n"
    "    leaq __fmt_buf(%rip), %rdi\n"
    "    movq $256, %rsi\n"
    "    call .Lstub_vfmt\n"
    "    movq %rax, %rdx\n"
    "    movq $1, %rdi\n"
    "    leaq __fmt_buf(%rip), %rsi\n"
    "    movq $1, %rax\n"
    "    syscall\n"
    "    addq $56, %rsp\n"
    "    ret\n"
    ".globl sprintf\n"
    "sprintf:\n"
    "    subq $48, %rsp\n"
    "    movq %rdx, 0(%rsp)\n"
    "    movq %rcx, 8(%rsp)\n"
    "    movq %r8, 16(%rsp)\n"
    "    movq %r9, 24(%rsp)\n"
    "    movq $0, 32(%rsp)\n"
    "    movq $0, 40(%rsp)\n"
    "    movq %rsi, %rdx\n"
    "    movq %rsp, %rcx\n"
    "    movq $2147483647, %rsi\n"
    "    call .Lstub_vfmt\n"
    "    addq $48, %rsp\n"
    "    ret\n"
    ".globl puts\n"
    "puts:\n"
    "    movq %rdi, %r10\n"
    "    call strlen\n"
    "    movq %rax, %rdx\n"
    "    movq $1, %rdi\n"
    "    movq %r10, %rsi\n"
    "    movq $1, %rax\n"
    "    syscall\n"
    "    subq $8, %rsp\n"
    "    movb $10, 0(%rsp)\n"
    "    movq $1, %rdi\n"
    "    movq %rsp, %rsi\n"
    "    movq $1, %rdx\n"
    "    movq $1, %rax\n"
    "    syscall\n"
    "    addq $8, %rsp\n"
    "    movq $0, %rax\n"
    "    ret\n"
    ".globl putchar\n"
    "putchar:\n"
    "    subq $8, %rsp\n"
    "    movb %dil, 0(%rsp)\n"
    "    movq $1, %rdi\n"
    "    movq %rsp, %rsi\n"
    "    movq $1, %rdx\n"
    "    movq $1, %rax\n"
    "    syscall\n"
    "    addq $8, %rsp\n"
    "    ret\n";
static const char ELF_STUBS_SRC_C[] =
    ".globl snprintf\n"
    "snprintf:\n"
    "    subq $48, %rsp\n"
    "    movq %rcx, 0(%rsp)\n"
    "    movq %r8, 8(%rsp)\n"
    "    movq %r9, 16(%rsp)\n"
    "    movq $0, 24(%rsp)\n"
    "    movq $0, 32(%rsp)\n"
    "    movq %rsp, %rcx\n"
    "    call .Lstub_vfmt\n"
    "    addq $48, %rsp\n"
    "    ret\n"
    ".Lstub_vfmt:\n"
    "    cmpq $0, %rsi\n"
    "    jne .Lstub_snprintf_ok\n"
    "    movq $0, %rax\n"
    "    ret\n"
    ".Lstub_snprintf_ok:\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    subq $40, %rsp\n"
    "    movq %rdi, %r10\n"
    "    movq %rsi, %r11\n"
    "    movq %rsi, 16(%rsp)\n"
    "    movq %rcx, 0(%rsp)\n"
    "    movq $0, 8(%rsp)\n"
    "    movq %rdx, %r13\n"
    "    movq $0, %r14\n"
    ".Lstub_snprintf_loop:\n"
    "    movb 0(%r13), %al\n"
    "    cmpb $0, %al\n"
    "    je .Lstub_snprintf_done\n"
    "    cmpb $37, %al\n"
    "    je .Lstub_snprintf_spec\n"
    "    call .Lstub_fmt_emit\n"
    ".Lstub_snprintf_next:\n"
    "    addq $1, %r13\n"
    "    jmp .Lstub_snprintf_loop\n"
    ".Lstub_snprintf_spec:\n"
    "    addq $1, %r13\n"
    "    movq $0, %r15\n"
    "    movq $0, %rbx\n"
    ".Lstub_snprintf_flags:\n"
    "    movb 0(%r13), %al\n"
    "    cmpb $48, %al\n"
    "    jne .Lstub_snprintf_width\n"
    "    movq $1, %rbx\n"
    "    addq $1, %r13\n"
    "    jmp .Lstub_snprintf_flags\n"
    ".Lstub_snprintf_width:\n"
    "    movb 0(%r13), %al\n"
    "    cmpb $48, %al\n"
    "    jl .Lstub_snprintf_conv\n"
    "    cmpb $57, %al\n"
    "    jg .Lstub_snprintf_conv\n"
    "    movzbq %al, %rax\n"
    "    subq $48, %rax\n"
    "    imulq $10, %r15\n"
    "    addq %rax, %r15\n"
    "    addq $1, %r13\n"
    "    jmp .Lstub_snprintf_width\n"
    ".Lstub_snprintf_conv:\n"
    "    movb 0(%r13), %al\n"
    "    cmpb $108, %al\n"
    "    jne .Lstub_snprintf_conv2\n"
    "    addq $1, %r13\n"
    "    movb 0(%r13), %al\n"
    ".Lstub_snprintf_conv2:\n"
    "    addq $1, %r13\n"
    "    cmpb $37, %al\n"
    "    je .Lstub_snprintf_emit_lit\n"
    "    cmpb $115, %al\n"
    "    je .Lstub_snprintf_str\n"
    "    cmpb $100, %al\n"
    "    je .Lstub_snprintf_dec\n"
    "    cmpb $111, %al\n"
    "    je .Lstub_snprintf_oct\n"
    "    jmp .Lstub_snprintf_loop\n"
    ".Lstub_snprintf_emit_lit:\n"
    "    movb $37, %al\n"
    "    call .Lstub_fmt_emit\n"
    "    jmp .Lstub_snprintf_loop\n"
    ".Lstub_snprintf_str:\n"
    "    movq 0(%rsp), %rax\n"
    "    movq (%rax,%r14,1), %rsi\n"
    "    addq $8, %r14\n"
    ".Lstub_snprintf_strloop:\n"
    "    movb 0(%rsi), %al\n"
    "    cmpb $0, %al\n"
    "    je .Lstub_snprintf_loop\n"
    "    call .Lstub_fmt_emit\n"
    "    addq $1, %rsi\n"
    "    jmp .Lstub_snprintf_strloop\n"
    ".Lstub_snprintf_dec:\n"
    "    movq 0(%rsp), %rax\n"
    "    movslq (%rax,%r14,1), %rdi\n"
    "    addq $8, %r14\n"
    "    movq $10, %r12\n"
    "    cmpq $0, %rdi\n"
    "    jge .Lstub_snprintf_dgen\n"
    "    negq %rdi\n"
    "    movq $1, 8(%rsp)\n"
    "    jmp .Lstub_snprintf_dgen\n"
    ".Lstub_snprintf_oct:\n"
    "    movq 0(%rsp), %rax\n"
    "    movq (%rax,%r14,1), %rdi\n"
    "    addq $8, %r14\n"
    "    movq $8, %r12\n"
    ".Lstub_snprintf_dgen:\n"
    "    subq $32, %rsp\n"
    "    movq %rsp, %rsi\n"
    "    movq $0, %rcx\n"
    ".Lstub_snprintf_dloop:\n"
    "    movq %rdi, %rax\n"
    "    movq $0, %rdx\n"
    "    idivq %r12\n"
    "    movq %rax, %rdi\n"
    "    movb %dl, %al\n"
    "    addb $48, %al\n"
    "    movb %al, 0(%rsi)\n"
    "    addq $1, %rsi\n"
    "    addq $1, %rcx\n"
    "    cmpq $0, %rdi\n"
    "    jne .Lstub_snprintf_dloop\n"
    "    movq %r15, %rax\n"
    "    subq %rcx, %rax\n"
    "    subq 40(%rsp), %rax\n"
    ".Lstub_snprintf_pad:\n"
    "    cmpq $0, %rax\n"
    "    jle .Lstub_snprintf_dout\n"
    "    movq %rax, %r9\n"
    "    movb $32, %al\n"
    "    cmpq $1, %rbx\n"
    "    jne .Lstub_snprintf_pad_char\n"
    "    movb $48, %al\n"
    ".Lstub_snprintf_pad_char:\n"
    "    call .Lstub_fmt_emit\n"
    "    movq %r9, %rax\n"
    "    subq $1, %rax\n"
    "    jmp .Lstub_snprintf_pad\n"
    ".Lstub_snprintf_dout:\n"
    "    cmpq $0, 40(%rsp)\n"
    "    je .Lstub_snprintf_dout2\n"
    "    movb $45, %al\n"
    "    call .Lstub_fmt_emit\n"
    ".Lstub_snprintf_dout2:\n"
    "    cmpq %rsp, %rsi\n"
    "    je .Lstub_snprintf_dclean\n"
    "    subq $1, %rsi\n"
    "    movb 0(%rsi), %al\n"
    "    call .Lstub_fmt_emit\n"
    "    jmp .Lstub_snprintf_dout2\n"
    ".Lstub_snprintf_dclean:\n"
    "    addq $32, %rsp\n"
    "    movq $0, 8(%rsp)\n"
    "    jmp .Lstub_snprintf_loop\n"
    ".Lstub_snprintf_done:\n"
    "    movb $0, 0(%r10)\n"
    "    movq 16(%rsp), %rax\n"
    "    subq %r11, %rax\n"
    "    addq $40, %rsp\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %rbx\n"
    "    ret\n"
    ".Lstub_fmt_emit:\n"
    "    cmpq $1, %r11\n"
    "    jle .Lstub_fmt_emit_drop\n"
    "    movb %al, 0(%r10)\n"
    "    addq $1, %r10\n"
    "    subq $1, %r11\n"
    ".Lstub_fmt_emit_drop:\n"
    "    ret\n";

static const char ELF_STUBS_SRC_B[] =
    ".globl memcpy\n"
    "memcpy:\n"
    "    movq %rdi, %r10\n"
    "    movq $0, %rax\n"
    ".Lstub_memcpy_loop:\n"
    "    cmpq %rdx, %rax\n"
    "    je .Lstub_memcpy_done\n"
    "    movb (%rsi,%rax,1), %r8b\n"
    "    movb %r8b, (%r10,%rax,1)\n"
    "    addq $1, %rax\n"
    "    jmp .Lstub_memcpy_loop\n"
    ".Lstub_memcpy_done:\n"
    "    movq %r10, %rax\n"
    "    ret\n"
    ".globl memset\n"
    "memset:\n"
    "    movq %rdi, %r10\n"
    "    movq $0, %rax\n"
    ".Lstub_memset_loop:\n"
    "    cmpq %rdx, %rax\n"
    "    je .Lstub_memset_done\n"
    "    movb %sil, (%r10,%rax,1)\n"
    "    addq $1, %rax\n"
    "    jmp .Lstub_memset_loop\n"
    ".Lstub_memset_done:\n"
    "    movq %r10, %rax\n"
    "    ret\n"
    ".globl memmove\n"
    "memmove:\n"
    "    movq %rdi, %r10\n"
    "    cmpq %rsi, %rdi\n"
    "    jae .Lstub_memmove_fwd\n"
    ".Lstub_memmove_bwd:\n"
    "    addq %rdx, %rsi\n"
    "    addq %rdx, %rdi\n"
    ".Lstub_memmove_bwd2:\n"
    "    cmpq $0, %rdx\n"
    "    je .Lstub_memmove_done\n"
    "    subq $1, %rdx\n"
    "    subq $1, %rsi\n"
    "    subq $1, %rdi\n"
    "    movb (%rsi), %r8b\n"
    "    movb %r8b, (%rdi)\n"
    "    jmp .Lstub_memmove_bwd2\n"
    ".Lstub_memmove_fwd:\n"
    "    movq $0, %rax\n"
    ".Lstub_memmove_fwd2:\n"
    "    cmpq %rdx, %rax\n"
    "    je .Lstub_memmove_done\n"
    "    movb (%rsi,%rax,1), %r8b\n"
    "    movb %r8b, (%r10,%rax,1)\n"
    "    addq $1, %rax\n"
    "    jmp .Lstub_memmove_fwd2\n"
    ".Lstub_memmove_done:\n"
    "    movq %r10, %rax\n"
    "    ret\n"
    ".globl memcmp\n"
    "memcmp:\n"
    "    movq $0, %rax\n"
    ".Lstub_memcmp_loop:\n"
    "    cmpq %rdx, %rax\n"
    "    je .Lstub_memcmp_eq\n"
    "    movb (%rdi,%rax,1), %r8b\n"
    "    movzbq %r8b, %r9\n"
    "    movb (%rsi,%rax,1), %r8b\n"
    "    movzbq %r8b, %r10\n"
    "    cmpq %r10, %r9\n"
    "    jne .Lstub_memcmp_ne\n"
    "    addq $1, %rax\n"
    "    jmp .Lstub_memcmp_loop\n"
    ".Lstub_memcmp_ne:\n"
    "    movq %r9, %rax\n"
    "    subq %r10, %rax\n"
    "    ret\n"
    ".Lstub_memcmp_eq:\n"
    "    movq $0, %rax\n"
    "    ret\n"
    ".globl strlen\n"
    "strlen:\n"
    "    movq $0, %rax\n"
    ".Lstub_strlen_loop:\n"
    "    movb (%rdi,%rax,1), %r8b\n"
    "    cmpb $0, %r8b\n"
    "    je .Lstub_strlen_done\n"
    "    addq $1, %rax\n"
    "    jmp .Lstub_strlen_loop\n"
    ".Lstub_strlen_done:\n"
    "    ret\n"
    ".globl strcmp\n"
    "strcmp:\n"
    ".Lstub_strcmp_loop:\n"
    "    movb (%rdi), %r8b\n"
    "    movzbq %r8b, %r9\n"
    "    movb (%rsi), %r8b\n"
    "    movzbq %r8b, %r10\n"
    "    cmpq %r10, %r9\n"
    "    jne .Lstub_strcmp_ne\n"
    "    cmpb $0, %r8b\n"
    "    je .Lstub_strcmp_eq\n"
    "    addq $1, %rdi\n"
    "    addq $1, %rsi\n"
    "    jmp .Lstub_strcmp_loop\n"
    ".Lstub_strcmp_ne:\n"
    "    movq %r9, %rax\n"
    "    subq %r10, %rax\n"
    "    ret\n"
    ".Lstub_strcmp_eq:\n"
    "    movq $0, %rax\n"
    "    ret\n"
    ".globl strncmp\n"
    "strncmp:\n"
    ".Lstub_strncmp_loop:\n"
    "    cmpq $0, %rdx\n"
    "    je .Lstub_strncmp_eq\n"
    "    subq $1, %rdx\n"
    "    movb (%rdi), %r8b\n"
    "    movzbq %r8b, %r9\n"
    "    movb (%rsi), %r8b\n"
    "    movzbq %r8b, %r10\n"
    "    cmpq %r10, %r9\n"
    "    jne .Lstub_strncmp_ne\n"
    "    cmpb $0, %r8b\n"
    "    je .Lstub_strncmp_eq\n"
    "    addq $1, %rdi\n"
    "    addq $1, %rsi\n"
    "    jmp .Lstub_strncmp_loop\n"
    ".Lstub_strncmp_ne:\n"
    "    movq %r9, %rax\n"
    "    subq %r10, %rax\n"
    "    ret\n"
    ".Lstub_strncmp_eq:\n"
    "    movq $0, %rax\n"
    "    ret\n"
    ".globl strcpy\n"
    "strcpy:\n"
    "    movq %rdi, %r10\n"
    ".Lstub_strcpy_loop:\n"
    "    movb (%rsi), %r8b\n"
    "    movb %r8b, (%rdi)\n"
    "    addq $1, %rsi\n"
    "    addq $1, %rdi\n"
    "    cmpb $0, %r8b\n"
    "    jne .Lstub_strcpy_loop\n"
    "    movq %r10, %rax\n"
    "    ret\n"
    ".globl strncpy\n"
    "strncpy:\n"
    "    movq %rdi, %r10\n"
    ".Lstub_strncpy_loop:\n"
    "    cmpq $0, %rdx\n"
    "    je .Lstub_strncpy_done\n"
    "    subq $1, %rdx\n"
    "    movb (%rsi), %r8b\n"
    "    movb %r8b, (%rdi)\n"
    "    addq $1, %rdi\n"
    "    cmpb $0, %r8b\n"
    "    je .Lstub_strncpy_pad\n"
    "    addq $1, %rsi\n"
    "    jmp .Lstub_strncpy_loop\n"
    ".Lstub_strncpy_pad:\n"
    "    cmpq $0, %rdx\n"
    "    je .Lstub_strncpy_done\n"
    "    subq $1, %rdx\n"
    "    movb $0, (%rdi)\n"
    "    addq $1, %rdi\n"
    "    jmp .Lstub_strncpy_pad\n"
    ".Lstub_strncpy_done:\n"
    "    movq %r10, %rax\n"
    "    ret\n"
    ".globl strchr\n"
    "strchr:\n"
    ".Lstub_strchr_loop:\n"
    "    movb (%rdi), %r8b\n"
    "    cmpb %sil, %r8b\n"
    "    je .Lstub_strchr_hit\n"
    "    cmpb $0, %r8b\n"
    "    je .Lstub_strchr_miss\n"
    "    addq $1, %rdi\n"
    "    jmp .Lstub_strchr_loop\n"
    ".Lstub_strchr_hit:\n"
    "    movq %rdi, %rax\n"
    "    ret\n"
    ".Lstub_strchr_miss:\n"
    "    movq $0, %rax\n"
    "    ret\n";

static long x86_align_up(long v, long a) {
    return (v + a - 1) & ~(a - 1);
}

static void x8(int b) {
    breserve(&code, &code_cap, code_len + 1);
    code[code_len++] = (unsigned char)b;
}

static void x16(long v) {
    breserve(&code, &code_cap, code_len + 2);
    code[code_len++] = (unsigned char)(v & 255);
    code[code_len++] = (unsigned char)((v >> 8) & 255);
}

static void x32(long v) {
    breserve(&code, &code_cap, code_len + 4);
    code[code_len++] = (unsigned char)(v & 255);
    code[code_len++] = (unsigned char)((v >> 8) & 255);
    code[code_len++] = (unsigned char)((v >> 16) & 255);
    code[code_len++] = (unsigned char)((v >> 24) & 255);
}

static void x64(unsigned long long v) {
    breserve(&code, &code_cap, code_len + 8);
    for (int i = 0; i < 8; i++) {
        code[code_len++] = (unsigned char)(v & 255);
        v >>= 8;
    }
}

static void xfix32(const char *sym) {
    fixup_reserve();
    fixups[n_fixups].pos = code_len;
    fixups[n_fixups].addend = 0;
    name_copy(fixups[n_fixups].name, sym);
    n_fixups++;
    x32(0);
}

static void fixup_trail(long t) {
    if (n_fixups > 0)
        fixups[n_fixups - 1].addend = t;
}

static void emit_rex(int w, int r, int x, int b) {
    int v = 0x40 | (w << 3) | (r << 2) | (x << 1) | b;
    if (v != 0x40) x8(v);
}

static void emit_modrm(int mod, int reg, int rm) {
    x8((mod << 6) | ((reg & 7) << 3) | (rm & 7));
}

static void emit_sib(int scale, int index, int base) {
    x8((scale << 6) | ((index & 7) << 3) | (base & 7));
}

static void x86_ea_rex(const Op *op, int regfield, int rexw, int force) {
    int rex_r = (regfield >> 3) & 1;
    int rex_x = op->has_index ? (op->index >> 3) & 1 : 0;
    int rex_b = op->has_base ? (op->base >> 3) & 1 : 0;
    int v = 0x40 | (rexw << 3) | (rex_r << 2) | (rex_x << 1) | rex_b;
    if (force || v != 0x40) x8(v);
}

static void x86_ea_modrm(const Op *op, int regfield) {
    if (op->is_rip) {
        emit_modrm(0, regfield & 7, 5);
        xfix32(op->sym);
        return;
    }
    if (op->has_index) {
        int scale_bits = op->scale == 8 ? 3 : op->scale == 4 ? 2 : op->scale == 2 ? 1 : 0;
        if (!op->has_base) {
            emit_modrm(0, regfield & 7, 4);
            emit_sib(scale_bits, op->index & 7, 5);
            x32(op->disp);
            return;
        }
        int base = op->base & 7;
        long d = op->disp;
        if (base == 5 && d == 0) {
            emit_modrm(1, regfield & 7, 4);
            emit_sib(scale_bits, op->index & 7, 5);
            x8(0);
            return;
        }
        int mod = (d == 0) ? 0 : ((d >= -128 && d <= 127) ? 1 : 2);
        emit_modrm(mod, regfield & 7, 4);
        emit_sib(scale_bits, op->index & 7, base);
        if (mod == 1) x8((int)(d & 255));
        else if (mod == 2) x32(d);
        return;
    }
    if (op->has_base) {
        int base = op->base & 7;
        long d = op->disp;
        if (base == 4) {
            if (d == 0) { emit_modrm(0, regfield & 7, 4); emit_sib(0, 4, 4); }
            else if (d >= -128 && d <= 127) { emit_modrm(1, regfield & 7, 4); emit_sib(0, 4, 4); x8((int)(d & 255)); }
            else { emit_modrm(2, regfield & 7, 4); emit_sib(0, 4, 4); x32(d); }
            return;
        }
        if (base == 5) {
            if (d >= -128 && d <= 127) { emit_modrm(1, regfield & 7, 5); x8((int)(d & 255)); }
            else { emit_modrm(2, regfield & 7, 5); x32(d); }
            return;
        }
        if (d == 0) emit_modrm(0, regfield & 7, base);
        else if (d >= -128 && d <= 127) { emit_modrm(1, regfield & 7, base); x8((int)(d & 255)); }
        else { emit_modrm(2, regfield & 7, base); x32(d); }
        return;
    }
    emit_modrm(0, regfield & 7, 4);
    emit_sib(0, 4, 5);
    x32(op->disp);
}

static void x86_rex_reg(int w, int regfield, int rm) {
    emit_rex(w, (regfield >> 3) & 1, 0, (rm >> 3) & 1);
}

static void x86_rex8(int regfield, int rm) {
    int rf_hi = (regfield >= 8) ? 1 : 0;
    int rm_hi = (rm >= 8) ? 1 : 0;
    int need = rf_hi || rm_hi || (regfield >= 4) || (rm >= 4);
    if (need) emit_rex(0, rf_hi, 0, rm_hi);
}

static void ea_mov(int size, const Op *o, int regfield) {
    x86_ea_rex(o, regfield, size == 8 ? 1 : 0, size == 1 && regfield >= 4);
    x8(size == 1 ? 0x8A : 0x8B);
    x86_ea_modrm(o, regfield);
}

static void ea_mov_to(int size, const Op *o, int regfield) {
    x86_ea_rex(o, regfield, size == 8 ? 1 : 0, size == 1 && regfield >= 4);
    x8(size == 1 ? 0x88 : 0x89);
    x86_ea_modrm(o, regfield);
}

static void ea_alu(int g1, int size, const Op *o, int regfield, int from_mem) {
    x86_ea_rex(o, regfield, size == 8 ? 1 : 0, size == 1 && regfield >= 4);
    x8((from_mem ? (size == 1 ? 0x02 : 0x03) : (size == 1 ? 0x00 : 0x01)) | g1);
    x86_ea_modrm(o, regfield);
}

static void ea_cmp(int size, const Op *o, int regfield, int from_mem) {
    x86_ea_rex(o, regfield, size == 8 ? 1 : 0, size == 1 && regfield >= 4);
    x8(from_mem ? (size == 1 ? 0x3A : 0x3B) : (size == 1 ? 0x38 : 0x39));
    x86_ea_modrm(o, regfield);
}

static void ea_grp(int opc, int size, const Op *o, int regfield) {
    x86_ea_rex(o, regfield, size == 8 ? 1 : 0, size == 1 && regfield >= 4);
    x8(opc);
    x86_ea_modrm(o, regfield);
}

static void elf_mov(int size, const Op *s, const Op *d) {
    if (d->kind == K_REG) {
        if (s->kind == K_REG) {
            if (size == 1) {
                x86_rex8(s->reg, d->reg);
                x8(0x88);
                emit_modrm(3, s->reg & 7, d->reg & 7);
            } else {
                x86_rex_reg(size == 8 ? 1 : 0, s->reg, d->reg);
                x8(0x89);
                emit_modrm(3, s->reg & 7, d->reg & 7);
            }
        } else if (s->kind == K_IMM) {
            long long v = s->imm;
            if (size == 8) {
                if ((long long)(int)v == v) {
                    emit_rex(1, 0, 0, (d->reg >> 3) & 1);
                    x8(0xC7);
                    emit_modrm(3, 0, d->reg & 7);
                    x32(v);
                } else {
                    emit_rex(1, 0, 0, (d->reg >> 3) & 1);
                    x8(0xB8 + (d->reg & 7));
                    x64((unsigned long long)v);
                }
            } else if (size == 4) {
                emit_rex(0, 0, 0, (d->reg >> 3) & 1);
                x8(0xB8 + (d->reg & 7));
                x32(v);
            } else {
                x86_rex8(0, d->reg);
                x8(0xB0 + (d->reg & 7));
                x8((int)(v & 255));
            }
        } else if (s->kind == K_MEM || s->kind == K_SYM) {
            ea_mov(size, s, d->reg);
        } else {
            die("invalid mov source");
        }
        return;
    }
    if (d->kind == K_MEM || d->kind == K_SYM) {
        if (s->kind == K_REG) {
            ea_mov_to(size, d, s->reg);
        } else if (s->kind == K_IMM) {
            if (size == 8 && (long long)(int)s->imm != s->imm) {
                die("immediate out of range");
                return;
            }
            ea_grp(size == 1 ? 0xC6 : 0xC7, size, d, 0);
            if (size == 1) x8((int)(s->imm & 255));
            else x32(s->imm);
            if (d->is_rip) fixup_trail(size == 1 ? 1 : 4);
        } else {
            die("invalid mov source");
        }
        return;
    }
    die("invalid mov destination");
}

static void elf_movzx(const Op *s, const Op *d, int opc, int rexw, int has_0f) {
    if (d->kind != K_REG) { die("invalid movzx destination"); return; }
    if (s->kind == K_REG) {
        x86_rex_reg(rexw, d->reg, s->reg);
        if (has_0f) x8(0x0F);
        x8(opc);
        emit_modrm(3, d->reg & 7, s->reg & 7);
    } else if (s->kind == K_MEM || s->kind == K_SYM) {
        x86_ea_rex(s, d->reg, rexw, 0);
        if (has_0f) x8(0x0F);
        x8(opc);
        x86_ea_modrm(s, d->reg);
    } else {
        die("invalid movzx source");
    }
}

static void elf_movw(const Op *s, const Op *d) {
    if (d->kind == K_REG) {
        if (s->kind == K_REG) {
            x8(0x66);
            x86_rex_reg(0, s->reg, d->reg);
            x8(0x89);
            emit_modrm(3, s->reg & 7, d->reg & 7);
        } else if (s->kind == K_MEM || s->kind == K_SYM) {
            x8(0x66);
            x86_ea_rex(s, d->reg, 0, 0);
            x8(0x8B);
            x86_ea_modrm(s, d->reg);
        } else {
            die("invalid movw source");
        }
    } else if (d->kind == K_MEM || d->kind == K_SYM) {
        if (s->kind == K_REG) {
            x8(0x66);
            x86_ea_rex(d, s->reg, 0, 0);
            x8(0x89);
            x86_ea_modrm(d, s->reg);
        } else if (s->kind == K_IMM) {
            x8(0x66);
            x86_ea_rex(d, 0, 0, 0);
            x8(0xC7);
            x86_ea_modrm(d, 0);
            x16(s->imm);
        } else {
            die("invalid movw source");
        }
    } else {
        die("invalid movw destination");
    }
}

static void elf_lea(const Op *s, const Op *d) {
    if (d->kind != K_REG) { die("invalid lea destination"); return; }
    x86_ea_rex(s, d->reg, 1, 0);
    x8(0x8D);
    x86_ea_modrm(s, d->reg);
}

static void elf_push(const Op *o) {
    if (o->kind == K_REG) {
        emit_rex(0, 0, 0, (o->reg >> 3) & 1);
        x8(0x50 + (o->reg & 7));
    } else if (o->kind == K_IMM) {
        long long v = o->imm;
        if ((long long)(signed char)v == v) {
            x8(0x6A);
            x8((int)(v & 255));
        } else if ((long long)(int)v == v) {
            x8(0x68);
            x32(v);
        } else {
            die("push immediate out of range");
        }
    } else if (o->kind == K_MEM || o->kind == K_SYM) {
        x86_ea_rex(o, 6, 0, 0);
        x8(0xFF);
        x86_ea_modrm(o, 6);
    } else {
        die("invalid push operand");
    }
}

static void elf_pop(const Op *o) {
    if (o->kind == K_REG) {
        emit_rex(0, 0, 0, (o->reg >> 3) & 1);
        x8(0x58 + (o->reg & 7));
    } else if (o->kind == K_MEM || o->kind == K_SYM) {
        x86_ea_rex(o, 0, 0, 0);
        x8(0x8F);
        x86_ea_modrm(o, 0);
    } else {
        die("invalid pop operand");
    }
}

static void elf_alu(int g1, int size, const Op *s, const Op *d) {
    int rexw = (size == 8) ? 1 : 0;
    if (d->kind == K_REG) {
        if (s->kind == K_REG) {
            if (size == 1) {
                x86_rex8(s->reg, d->reg);
                x8(0x00 | g1);
                emit_modrm(3, s->reg & 7, d->reg & 7);
            } else {
                x86_rex_reg(rexw, s->reg, d->reg);
                x8(0x01 | g1);
                emit_modrm(3, s->reg & 7, d->reg & 7);
            }
        } else if (s->kind == K_IMM) {
            long long v = s->imm;
            if ((long long)(signed char)v == v) {
                emit_rex(rexw, 0, 0, (d->reg >> 3) & 1);
                x8(0x83);
                emit_modrm(3, (g1 >> 3) & 7, d->reg & 7);
                x8((int)(v & 255));
            } else {
                emit_rex(rexw, 0, 0, (d->reg >> 3) & 1);
                x8(0x81);
                emit_modrm(3, (g1 >> 3) & 7, d->reg & 7);
                x32(v);
            }
        } else if (s->kind == K_MEM || s->kind == K_SYM) {
            ea_alu(g1, size, s, d->reg, 1);
        } else {
            die("invalid arithmetic operand");
        }
    } else if (d->kind == K_MEM || d->kind == K_SYM) {
        if (s->kind == K_REG) {
            ea_alu(g1, size, d, s->reg, 0);
        } else if (s->kind == K_IMM) {
            long long v = s->imm;
            if (size == 1) {
                x86_ea_rex(d, (g1 >> 3) & 7, 0, 0);
                x8(0x80);
                x86_ea_modrm(d, (g1 >> 3) & 7);
                x8((int)(v & 255));
                if (d->is_rip) fixup_trail(1);
            } else if ((long long)(signed char)v == v) {
                x86_ea_rex(d, (g1 >> 3) & 7, rexw, 0);
                x8(0x83);
                x86_ea_modrm(d, (g1 >> 3) & 7);
                x8((int)(v & 255));
                if (d->is_rip) fixup_trail(1);
            } else {
                x86_ea_rex(d, (g1 >> 3) & 7, rexw, 0);
                x8(0x81);
                x86_ea_modrm(d, (g1 >> 3) & 7);
                x32(v);
                if (d->is_rip) fixup_trail(4);
            }
        } else {
            die("invalid arithmetic operand");
        }
    } else {
        die("invalid arithmetic destination");
    }
}

static void elf_imul(const Op *s, const Op *d) {
    if (d->kind != K_REG) { die("invalid imul destination"); return; }
    if (s->kind == K_REG) {
        x86_rex_reg(1, d->reg, s->reg);
        x8(0x0F);
        x8(0xAF);
        emit_modrm(3, d->reg & 7, s->reg & 7);
    } else if (s->kind == K_MEM || s->kind == K_SYM) {
        x86_ea_rex(s, d->reg, 1, 0);
        x8(0x0F);
        x8(0xAF);
        x86_ea_modrm(s, d->reg);
    } else if (s->kind == K_IMM) {
        long long v = s->imm;
        emit_rex(1, 0, 0, (d->reg >> 3) & 1);
        if ((long long)(signed char)v == v) {
            x8(0x6B);
            emit_modrm(3, d->reg & 7, d->reg & 7);
            x8((int)(v & 255));
        } else {
            x8(0x69);
            emit_modrm(3, d->reg & 7, d->reg & 7);
            x32(v);
        }
    } else {
        die("invalid imul operand");
    }
}

static void elf_imull(const Op *s, const Op *d) {
    if (d->kind != K_REG) { die("invalid imull destination"); return; }
    if (s->kind == K_REG) {
        x86_rex_reg(0, d->reg, s->reg);
        x8(0x0F);
        x8(0xAF);
        emit_modrm(3, d->reg & 7, s->reg & 7);
    } else if (s->kind == K_MEM || s->kind == K_SYM) {
        x86_ea_rex(s, d->reg, 0, 0);
        x8(0x0F);
        x8(0xAF);
        x86_ea_modrm(s, d->reg);
    } else if (s->kind == K_IMM) {
        long long v = s->imm;
        emit_rex(0, 0, 0, (d->reg >> 3) & 1);
        if ((long long)(signed char)v == v) {
            x8(0x6B);
            emit_modrm(3, d->reg & 7, d->reg & 7);
            x8((int)(v & 255));
        } else {
            x8(0x69);
            emit_modrm(3, d->reg & 7, d->reg & 7);
            x32(v);
        }
    } else {
        die("invalid imull operand");
    }
}

static void elf_grp3(const Op *o, int ext) {
    if (o->kind == K_REG) {
        emit_rex(1, 0, 0, (o->reg >> 3) & 1);
        x8(0xF7);
        emit_modrm(3, ext, o->reg & 7);
    } else if (o->kind == K_MEM || o->kind == K_SYM) {
        x86_ea_rex(o, ext, 1, 0);
        x8(0xF7);
        x86_ea_modrm(o, ext);
    } else {
        die("invalid operand");
    }
}

static void elf_grp3_32(const Op *o, int ext) {
    if (o->kind == K_REG) {
        emit_rex(0, 0, 0, (o->reg >> 3) & 1);
        x8(0xF7);
        emit_modrm(3, ext, o->reg & 7);
    } else if (o->kind == K_MEM || o->kind == K_SYM) {
        x86_ea_rex(o, ext, 0, 0);
        x8(0xF7);
        x86_ea_modrm(o, ext);
    } else {
        die("invalid operand");
    }
}

static void elf_xadd(const Op *s, const Op *d) {
    if (s->kind != K_REG) { die("invalid xadd source"); return; }
    if (d->kind == K_REG) {
        emit_rex(1, (s->reg >> 3) & 1, 0, (d->reg >> 3) & 1);
        x8(0x0F);
        x8(0xC1);
        emit_modrm(3, s->reg & 7, d->reg & 7);
    } else if (d->kind == K_MEM || d->kind == K_SYM) {
        x86_ea_rex(d, s->reg, 1, 0);
        x8(0x0F);
        x8(0xC1);
        x86_ea_modrm(d, s->reg & 7);
    } else {
        die("invalid xadd destination");
    }
}

static void elf_xchg(const Op *s, const Op *d) {
    const Op *r = s->kind == K_REG ? s : d;
    const Op *m = s->kind == K_REG ? d : s;
    if (r->kind != K_REG) { die("xchg needs a register"); return; }
    if (m->kind == K_REG) {
        emit_rex(1, (r->reg >> 3) & 1, 0, (m->reg >> 3) & 1);
        x8(0x87);
        emit_modrm(3, r->reg & 7, m->reg & 7);
    } else if (m->kind == K_MEM || m->kind == K_SYM) {
        x86_ea_rex(m, r->reg, 1, 0);
        x8(0x87);
        x86_ea_modrm(m, r->reg & 7);
    } else {
        die("invalid xchg destination");
    }
}

static void elf_grp_ff(const Op *o, int ext) {
    if (o->kind == K_REG) {
        emit_rex(1, 0, 0, (o->reg >> 3) & 1);
        x8(0xFF);
        emit_modrm(3, ext, o->reg & 7);
    } else if (o->kind == K_MEM || o->kind == K_SYM) {
        x86_ea_rex(o, ext, 1, 0);
        x8(0xFF);
        x86_ea_modrm(o, ext);
    } else {
        die("invalid operand");
    }
}

static void elf_shift_cl(const Op *s, const Op *d, int ext) {
    if (s->kind != K_REG || s->reg != REG_RCX) { die("shift count must be %cl"); return; }
    if (d->kind != K_REG) { die("invalid shift destination"); return; }
    emit_rex(1, 0, 0, (d->reg >> 3) & 1);
    x8(0xD3);
    emit_modrm(3, ext, d->reg & 7);
}

static void elf_shift_cl32(const Op *s, const Op *d, int ext) {
    if (s->kind != K_REG || s->reg != REG_RCX) { die("shift count must be %cl"); return; }
    if (d->kind != K_REG) { die("invalid shift destination"); return; }
    emit_rex(0, 0, 0, (d->reg >> 3) & 1);
    x8(0xD3);
    emit_modrm(3, ext, d->reg & 7);
}

static void elf_testl(const Op *s, const Op *d) {
    if (d->kind != K_REG) { die("invalid test destination"); return; }
    if (s->kind == K_REG) {
        x86_rex_reg(0, s->reg, d->reg);
        x8(0x85);
        emit_modrm(3, s->reg & 7, d->reg & 7);
    } else if (s->kind == K_IMM) {
        if ((long long)(int)s->imm != s->imm) { die("immediate out of range"); return; }
        emit_rex(0, 0, 0, (d->reg >> 3) & 1);
        x8(0xF7);
        emit_modrm(3, 0, d->reg & 7);
        x32(s->imm);
    } else {
        die("invalid test operand");
    }
}

static void elf_test(const Op *s, const Op *d) {
    if (d->kind != K_REG) { die("invalid test destination"); return; }
    if (s->kind == K_REG) {
        x86_rex_reg(1, s->reg, d->reg);
        x8(0x85);
        emit_modrm(3, s->reg & 7, d->reg & 7);
    } else if (s->kind == K_IMM) {
        if ((long long)(int)s->imm != s->imm) { die("immediate out of range"); return; }
        emit_rex(1, 0, 0, (d->reg >> 3) & 1);
        x8(0xF7);
        emit_modrm(3, 0, d->reg & 7);
        x32(s->imm);
    } else {
        die("invalid test operand");
    }
}

static void elf_cmp(int size, const Op *s, const Op *d) {
    int rexw = (size == 8) ? 1 : 0;
    if (d->kind == K_REG) {
        if (s->kind == K_REG) {
            if (size == 1) {
                x86_rex8(s->reg, d->reg);
                x8(0x38);
                emit_modrm(3, s->reg & 7, d->reg & 7);
            } else {
                x86_rex_reg(rexw, s->reg, d->reg);
                x8(0x39);
                emit_modrm(3, s->reg & 7, d->reg & 7);
            }
        } else if (s->kind == K_IMM) {
            long long v = s->imm;
            if (size == 1) {
                x86_rex8(0, d->reg);
                x8(0x80);
                emit_modrm(3, 7, d->reg & 7);
                x8((int)(v & 255));
            } else if ((long long)(signed char)v == v) {
                emit_rex(rexw, 0, 0, (d->reg >> 3) & 1);
                x8(0x83);
                emit_modrm(3, 7, d->reg & 7);
                x8((int)(v & 255));
            } else {
                emit_rex(rexw, 0, 0, (d->reg >> 3) & 1);
                x8(0x81);
                emit_modrm(3, 7, d->reg & 7);
                x32(v);
            }
        } else if (s->kind == K_MEM || s->kind == K_SYM) {
            ea_cmp(size, s, d->reg, 1);
        } else {
            die("invalid compare operand");
        }
    } else if (d->kind == K_MEM || d->kind == K_SYM) {
        if (s->kind == K_REG) {
            ea_cmp(size, d, s->reg, 0);
        } else if (s->kind == K_IMM) {
            long long v = s->imm;
            if (size == 1) {
                x86_ea_rex(d, 7, 0, 0);
                x8(0x80);
                x86_ea_modrm(d, 7);
                x8((int)(v & 255));
                if (d->is_rip) fixup_trail(1);
            } else if ((long long)(signed char)v == v) {
                x86_ea_rex(d, 7, rexw, 0);
                x8(0x83);
                x86_ea_modrm(d, 7);
                x8((int)(v & 255));
                if (d->is_rip) fixup_trail(1);
            } else {
                x86_ea_rex(d, 7, rexw, 0);
                x8(0x81);
                x86_ea_modrm(d, 7);
                x32(v);
                if (d->is_rip) fixup_trail(4);
            }
        } else {
            die("invalid compare operand");
        }
    } else {
        die("invalid compare destination");
    }
}

static void elf_set(int cc, const Op *o) {
    if (o->kind != K_REG || o->regsz != 1) { die("invalid setcc operand"); return; }
    x86_rex8(0, o->reg);
    x8(0x0F);
    x8(cc);
    emit_modrm(3, 0, o->reg & 7);
}

static void elf_branch(int opc, const Op *o) {
    if (o->kind != K_SYM) { die("invalid branch target"); return; }
    if (opc == 0xE8 || opc == 0xE9) {
        x8(opc);
    } else {
        x8(0x0F);
        x8(opc);
    }
    xfix32(o->sym);
}

static void elf_call_ind(const Op *o) {
    if (o->kind != K_IND) { die("invalid call target"); return; }
    emit_rex(0, 0, 0, (o->reg >> 3) & 1);
    x8(0xFF);
    emit_modrm(3, 2, o->reg & 7);
}

static void elf_ins(const char *mn, const Op *o1, const Op *o2) {
    if (strcmp(mn, "movq") == 0) { elf_mov(8, o1, o2); return; }
    if (strcmp(mn, "movl") == 0) { elf_mov(4, o1, o2); return; }
    if (strcmp(mn, "movb") == 0) { elf_mov(1, o1, o2); return; }
    if (strcmp(mn, "movw") == 0) { elf_movw(o1, o2); return; }
    if (strcmp(mn, "movzbq") == 0) { elf_movzx(o1, o2, 0xB6, 1, 1); return; }
    if (strcmp(mn, "movsbq") == 0) { elf_movzx(o1, o2, 0xBE, 1, 1); return; }
    if (strcmp(mn, "movslq") == 0) { elf_movzx(o1, o2, 0x63, 1, 0); return; }
    if (strcmp(mn, "movzbl") == 0) { elf_movzx(o1, o2, 0xB6, 0, 1); return; }
    if (strcmp(mn, "movsbl") == 0) { elf_movzx(o1, o2, 0xBE, 0, 1); return; }
    if (strcmp(mn, "movzwl") == 0) { elf_movzx(o1, o2, 0xB7, 0, 1); return; }
    if (strcmp(mn, "movswl") == 0) { elf_movzx(o1, o2, 0xBF, 0, 1); return; }
    if (strcmp(mn, "movzwq") == 0) { elf_movzx(o1, o2, 0xB7, 1, 1); return; }
    if (strcmp(mn, "movswq") == 0) { elf_movzx(o1, o2, 0xBF, 1, 1); return; }
    if (strcmp(mn, "leaq") == 0) { elf_lea(o1, o2); return; }
    if (strcmp(mn, "pushq") == 0) { elf_push(o1); return; }
    if (strcmp(mn, "popq") == 0) { elf_pop(o1); return; }
    if (strcmp(mn, "addq") == 0) { elf_alu(X86_G1_ADD, 8, o1, o2); return; }
    if (strcmp(mn, "addl") == 0) { elf_alu(X86_G1_ADD, 4, o1, o2); return; }
    if (strcmp(mn, "addb") == 0) { elf_alu(X86_G1_ADD, 1, o1, o2); return; }
    if (strcmp(mn, "subq") == 0) { elf_alu(X86_G1_SUB, 8, o1, o2); return; }
    if (strcmp(mn, "subl") == 0) { elf_alu(X86_G1_SUB, 4, o1, o2); return; }
    if (strcmp(mn, "andq") == 0) { elf_alu(X86_G1_AND, 8, o1, o2); return; }
    if (strcmp(mn, "andl") == 0) { elf_alu(X86_G1_AND, 4, o1, o2); return; }
    if (strcmp(mn, "orq") == 0) { elf_alu(X86_G1_OR, 8, o1, o2); return; }
    if (strcmp(mn, "orl") == 0) { elf_alu(X86_G1_OR, 4, o1, o2); return; }
    if (strcmp(mn, "xorq") == 0) { elf_alu(X86_G1_XOR, 8, o1, o2); return; }
    if (strcmp(mn, "xorl") == 0) { elf_alu(X86_G1_XOR, 4, o1, o2); return; }
    if (strcmp(mn, "cmpq") == 0) { elf_cmp(8, o1, o2); return; }
    if (strcmp(mn, "cmpl") == 0) { elf_cmp(4, o1, o2); return; }
    if (strcmp(mn, "cmpb") == 0) { elf_cmp(1, o1, o2); return; }
    if (strcmp(mn, "imulq") == 0) { elf_imul(o1, o2); return; }
    if (strcmp(mn, "imull") == 0) { elf_imull(o1, o2); return; }
    if (strcmp(mn, "negq") == 0) { elf_grp3(o1, 3); return; }
    if (strcmp(mn, "notq") == 0) { elf_grp3(o1, 2); return; }
    if (strcmp(mn, "incq") == 0) { elf_grp_ff(o1, 0); return; }
    if (strcmp(mn, "decq") == 0) { elf_grp_ff(o1, 1); return; }
    if (strcmp(mn, "idivq") == 0) { elf_grp3(o1, 7); return; }
    if (strcmp(mn, "divq") == 0) { elf_grp3(o1, 6); return; }
    if (strcmp(mn, "idivl") == 0) { elf_grp3_32(o1, 7); return; }
    if (strcmp(mn, "divl") == 0) { elf_grp3_32(o1, 6); return; }
    if (strcmp(mn, "salq") == 0) { elf_shift_cl(o1, o2, 4); return; }
    if (strcmp(mn, "shll") == 0) { elf_shift_cl32(o1, o2, 4); return; }
    if (strcmp(mn, "sall") == 0) { elf_shift_cl32(o1, o2, 4); return; }
    if (strcmp(mn, "shrl") == 0) { elf_shift_cl32(o1, o2, 5); return; }
    if (strcmp(mn, "sarl") == 0) { elf_shift_cl32(o1, o2, 7); return; }
    if (strcmp(mn, "shrq") == 0) { elf_shift_cl(o1, o2, 5); return; }
    if (strcmp(mn, "sarq") == 0) { elf_shift_cl(o1, o2, 7); return; }
    if (strcmp(mn, "testq") == 0) { elf_test(o1, o2); return; }
    if (strcmp(mn, "testl") == 0) { elf_testl(o1, o2); return; }
    if (strcmp(mn, "sete") == 0) { elf_set(X86_SET_E, o1); return; }
    if (strcmp(mn, "setne") == 0) { elf_set(X86_SET_NE, o1); return; }
    if (strcmp(mn, "setl") == 0) { elf_set(X86_SET_L, o1); return; }
    if (strcmp(mn, "setg") == 0) { elf_set(X86_SET_G, o1); return; }
    if (strcmp(mn, "setle") == 0) { elf_set(X86_SET_LE, o1); return; }
    if (strcmp(mn, "setge") == 0) { elf_set(X86_SET_GE, o1); return; }
    if (strcmp(mn, "seta") == 0) { elf_set(X86_SET_A, o1); return; }
    if (strcmp(mn, "setae") == 0) { elf_set(X86_SET_AE, o1); return; }
    if (strcmp(mn, "setb") == 0) { elf_set(X86_SET_B, o1); return; }
    if (strcmp(mn, "setbe") == 0) { elf_set(X86_SET_BE, o1); return; }
    if (strcmp(mn, "je") == 0) { elf_branch(X86_JCC_JE, o1); return; }
    if (strcmp(mn, "jne") == 0) { elf_branch(X86_JCC_JNE, o1); return; }
    if (strcmp(mn, "jl") == 0) { elf_branch(X86_JCC_JL, o1); return; }
    if (strcmp(mn, "jg") == 0) { elf_branch(X86_JCC_JG, o1); return; }
    if (strcmp(mn, "jle") == 0) { elf_branch(X86_JCC_JLE, o1); return; }
    if (strcmp(mn, "jge") == 0) { elf_branch(X86_JCC_JGE, o1); return; }
    if (strcmp(mn, "ja") == 0) { elf_branch(X86_JCC_JA, o1); return; }
    if (strcmp(mn, "jae") == 0) { elf_branch(X86_JCC_JAE, o1); return; }
    if (strcmp(mn, "jnb") == 0) { elf_branch(X86_JCC_JAE, o1); return; }
    if (strcmp(mn, "jb") == 0) { elf_branch(X86_JCC_JB, o1); return; }
    if (strcmp(mn, "jbe") == 0) { elf_branch(X86_JCC_JBE, o1); return; }
    if (strcmp(mn, "jmp") == 0) { elf_branch(0xE9, o1); return; }
    if (strcmp(mn, "call") == 0) {
        if (o1->kind == K_IND) { elf_call_ind(o1); return; }
        elf_branch(0xE8, o1);
        return;
    }
    if (strcmp(mn, "ret") == 0) { x8(0xC3); return; }
    if (strcmp(mn, "leave") == 0) { x8(0xC9); return; }
    if (strcmp(mn, "cqto") == 0) { x8(0x48); x8(0x99); return; }
    if (strcmp(mn, "cltd") == 0) { x8(0x99); return; }
    if (strcmp(mn, "cwtl") == 0) { x8(0x98); return; }
    if (strcmp(mn, "cdqe") == 0) { x8(0x48); x8(0x98); return; }
    if (strcmp(mn, "syscall") == 0) { x8(0x0F); x8(0x05); return; }
    if (strcmp(mn, "nop") == 0) { x8(0x90); return; }
    if (strcmp(mn, "lock") == 0) { x8(0xF0); return; }
    if (strcmp(mn, "xaddq") == 0) { elf_xadd(o1, o2); return; }
    if (strcmp(mn, "xchgq") == 0) { elf_xchg(o1, o2); return; }
    if (strcmp(mn, "mfence") == 0) { x8(0x0F); x8(0xAE); x8(0xF0); return; }
    if (strcmp(mn, "cli") == 0) { x8(0xFA); return; }
    if (strcmp(mn, "sti") == 0) { x8(0xFB); return; }
    if (strcmp(mn, "hlt") == 0) { x8(0xF4); return; }
    if (strcmp(mn, "rdtsc") == 0) { x8(0x0F); x8(0x31); return; }
    fprintf(stderr, "ld: %s:%ld: unsupported instruction '%s'\n", cur_file, cur_line, mn);
    error_count++;
}

static long elf_sym_addr(const Sym *s) {
    switch (s->kind) {
    case SYM_FUNC:
    case SYM_LABEL:
        return elf_text_base + s->off;
    case SYM_BLOB:
        return (s->sec == SEC_RODATA ? elf_rodata_base : elf_data_base) + s->off;
    case SYM_GLOBAL:
        return elf_bss_base + s->off;
    }
    return -1;
}

static int elf_has_dyn(void);
static int elf_has_rela(void);

/* Route one call fixup through the PLT slot of import li. Shared by the
 * fixup resolver so a name defined in a -d library binds through the
 * stub exactly like a bare undefined reference. */
static void elf_route_import(long li, long pos, long addend) {
    long stub = elf_plt_base + li * 16L;
    long disp = stub - (elf_text_base + pos + 4 + addend);
    long q = pos;
    code[q]     = (unsigned char)(disp & 255);
    code[q + 1] = (unsigned char)((disp >> 8) & 255);
    code[q + 2] = (unsigned char)((disp >> 16) & 255);
    code[q + 3] = (unsigned char)((disp >> 24) & 255);
}

/* True when some call fixup names `n` (the program references it). */
static int name_is_called(const char *n) {
    int i;
    for (i = 0; i < n_fixups; i++)
        if (strcmp(fixups[i].name, n) == 0) return 1;
    return 0;
}

/* Import pruning, run after the user source is encoded (fixups known)
 * and before the stub bodies are encoded: keep only the library
 * exports the program actually calls, so the GOT/PLT/RELA tables list
 * no dead rows and the executable does not pay for the whole library
 * surface. Each kept import is marked on its symbol (or a placeholder
 * when no stub shares the name) so the resolver routes through the
 * PLT and the colliding stub body is skipped; a name the user defines
 * stays local. Body pruning (dropping unreferenced stubs) is NOT done:
 * runtime routines share local helper labels (.Lstub_vfmt,
 * .Lstub_resolve_fd) that sit between .globl functions, so skipping a
 * function body can swallow a helper another routine still calls;
 * safe function-level pruning needs block-level helper tracking
 * first. The measured file-size effect is small (a 2 KB stub cut is
 * absorbed by the segment page padding), so it is not worth the risk
 * yet. */
static void elf_prune(void) {
    int i;
    int kept = 0;
    if (ld_nlibs > 0) {
        for (i = 0; i < ld_nimports; i++) {
            const char *nm = ld_imports[i].name;
            int si = find_sym(nm);
            int referenced;
            if (si >= 0) referenced = syms[si].is_stub && name_is_called(nm);
            else referenced = name_is_called(nm);
            if (!referenced) continue;
            if (kept != i) ld_imports[kept] = ld_imports[i];
            kept++;
        }
        ld_nimports = kept;
    }
    for (i = 0; i < ld_nimports; i++) {
        int si = find_sym(ld_imports[i].name);
        if (si < 0)
            si = add_sym(ld_imports[i].name, SYM_FUNC, SEC_TEXT);
        if (si >= 0) {
            syms[si].is_import = 1;
            syms[si].import_idx = i;
            syms[si].shadowed = 1;
        }
    }
}

static void elf_resolve_fixups(void) {
    for (int i = 0; i < n_fixups; i++) {
        int si = find_sym(fixups[i].name);
        if (si >= 0 && syms[si].is_import) {
            if (!elf_has_rela()) {
                fprintf(stderr, "ld: %s: cannot import %s here\n",
                    cur_file, fixups[i].name);
                error_count++;
                continue;
            }
            elf_route_import(syms[si].import_idx, fixups[i].pos,
                fixups[i].addend);
            continue;
        }
        if (si < 0) {
            int li;
            int found = 0;
            for (li = 0; li < ld_nimports; li++) {
                if (strcmp(ld_imports[li].name, fixups[i].name) != 0)
                    continue;
                found = 1;
                if (!ld_imports[li].is_func) {
                    fprintf(stderr, "ld: %s: data import %s unsupported (functions only)\n",
                        cur_file, fixups[i].name);
                    error_count++;
                    break;
                }
                if (!elf_has_rela()) {
                    fprintf(stderr, "ld: %s: cannot import %s here\n",
                        cur_file, fixups[i].name);
                    error_count++;
                    break;
                }
                elf_route_import(li, fixups[i].pos, fixups[i].addend);
                break;
            }
            if (!found) {
                fprintf(stderr, "ld: %s: undefined symbol %s\n", cur_file, fixups[i].name);
                error_count++;
            }
            continue;
        }
        long addr = elf_sym_addr(&syms[si]);
        long rel = addr - (elf_text_base + fixups[i].pos + 4 + fixups[i].addend);
        long p = fixups[i].pos;
        code[p]     = (unsigned char)(rel & 255);
        code[p + 1] = (unsigned char)((rel >> 8) & 255);
        code[p + 2] = (unsigned char)((rel >> 16) & 255);
        code[p + 3] = (unsigned char)((rel >> 24) & 255);
    }
    n_fixups = 0;
}

static void elf_encode_src(LineSrc *src) {
    char line[CFG_LINE_MAX];
    int section = SEC_TEXT;
    int skip_until_next_func = 0;
    long lineno = 0;
    while (ls_getline(src, line, sizeof(line))) {
        lineno++;
        cur_line = lineno;
        char *t = trim(line);
        strip_comment(t);
        if (!*t) continue;
        long n = (long)strlen(t);
        if (t[n - 1] == ':') {
            t[n - 1] = 0;
            if (section == SEC_TEXT) {
                int si = find_sym(t);
                if (si >= 0 && syms[si].kind == SYM_FUNC) {
                    skip_until_next_func = syms[si].shadowed ||
                        (g_shared && strcmp(t, "_start") == 0);
                    if (!skip_until_next_func) syms[si].off = code_len;
                } else if (si >= 0 && syms[si].kind == SYM_LABEL) {
                    if (!skip_until_next_func) syms[si].off = code_len;
                } else if (si < 0 && !skip_until_next_func) {
                    si = add_sym(t, SYM_LABEL, SEC_TEXT);
                    if (si >= 0) syms[si].off = code_len;
                }
            }
            continue;
        }
        if (t[0] == '.') {
            int s = set_section(t);
            if (s >= 0) section = s;
            continue;
        }
        if (section != SEC_TEXT || skip_until_next_func) continue;
        char *sp = t;
        while (*sp && *sp != ' ' && *sp != '\t') sp++;
        char mn[64];
        long ml = sp - t;
        if (ml >= 63) ml = 63;
        memcpy(mn, t, (size_t)ml);
        mn[ml] = 0;
        char *rest = sp;
        while (*rest == ' ' || *rest == '\t') rest++;
        Op o1, o2;
        memset(&o1, 0, sizeof(o1));
        memset(&o2, 0, sizeof(o2));
        if (*rest) {
            char o1s[CFG_LINE_MAX], o2s[CFG_LINE_MAX];
            int no = split_operands(rest, o1s, o2s);
            parse_operand(o1s, &o1);
            if (no == 2) parse_operand(o2s, &o2);
        }
        elf_ins(mn, &o1, &o2);
    }
}

/* Build the dynamic string pool (leading NUL, SONAME, one name per
 * exported function) and record per-symbol offsets, then place the
 * dynamic tables after the data blobs and push .bss past them. The
 * exported set is every defined function symbol; undefined ones
 * already failed in resolve_fixups, which is the self-containment
 * rule. Callers keep the pool until ld_release. */
static void elf_dyn_prepare(void) {
    long total = 1 + (long)strlen(elf_soname) + 1;
    int i;
    long off;
    elf_ndyn = 0;
    for (i = 0; i < n_syms; i++)
        if (syms[i].kind == SYM_FUNC && !syms[i].shadowed &&
                !(g_shared && strcmp(syms[i].name, "_start") == 0)) elf_ndyn++;
    for (i = 0; i < n_syms; i++)
        if (syms[i].kind == SYM_FUNC && !syms[i].shadowed &&
                !(g_shared && strcmp(syms[i].name, "_start") == 0))
            total += (long)strlen(syms[i].name) + 1;
    elf_dynstr = (char *)malloc((size_t)total);
    elf_dynoff = (long *)malloc(sizeof(long) * (size_t)(n_syms > 0 ? n_syms : 1));
    if (!elf_dynstr || !elf_dynoff) {
        fprintf(stderr, "ld: out of memory\n");
        exit(1);
    }
    for (i = 0; i < n_syms; i++) elf_dynoff[i] = -1;
    elf_dynstr[0] = 0;
    off = 1;
    memcpy(elf_dynstr + off, elf_soname, strlen(elf_soname) + 1);
    off += (long)strlen(elf_soname) + 1;
    for (i = 0; i < n_syms; i++) {
        if (syms[i].kind != SYM_FUNC || syms[i].shadowed ||
                (g_shared && strcmp(syms[i].name, "_start") == 0)) continue;
        elf_dynoff[i] = off;
        memcpy(elf_dynstr + off, syms[i].name, strlen(syms[i].name) + 1);
        off += (long)strlen(syms[i].name) + 1;
    }
    elf_dynstr_size = off;
    elf_dyn_nentry = CFG_ELF_DYN_NENTRY;
    elf_dyn_tags[0] = CFG_ELF_DT_HASH;
    elf_dyn_vals[0] = 0;
    elf_dyn_tags[1] = CFG_ELF_DT_STRTAB;
    elf_dyn_vals[1] = 0;
    elf_dyn_tags[2] = CFG_ELF_DT_SYMTAB;
    elf_dyn_vals[2] = 0;
    elf_dyn_tags[3] = CFG_ELF_DT_STRSZ;
    elf_dyn_vals[3] = (unsigned long long)elf_dynstr_size;
    elf_dyn_tags[4] = CFG_ELF_DT_SYMENT;
    elf_dyn_vals[4] = (unsigned long long)CFG_ELF_DYNSYM_ENT;
    elf_dyn_tags[5] = CFG_ELF_DT_SONAME;
    elf_dyn_vals[5] = 1ULL;
    elf_dyn_tags[6] = CFG_ELF_DT_NULL;
    elf_dyn_vals[6] = 0ULL;
    elf_dyn_base = x86_align_up(elf_data_base + elf_data_size, 8);
    elf_dynsym_base = elf_dyn_base + elf_dyn_nentry * 16L;
    elf_dynstr_base = x86_align_up(elf_dynsym_base + (elf_ndyn + 1) * CFG_ELF_DYNSYM_ENT, 8);
    elf_hash_base = x86_align_up(elf_dynstr_base + elf_dynstr_size, 8);
    elf_hash_size = 8 + 4L * (1 + (elf_ndyn + 1));
    elf_data_size = (elf_hash_base + elf_hash_size) - elf_data_base;
    elf_bss_base = x86_align_up(elf_data_base + elf_data_size, 16);
}
/* Build the executable import tables (L2): dynstr pool holding a
 * leading NUL, one SONAME per -d library, then every imported name,
 * with per-import and per-soname offsets recorded. Layout globals
 * follow the same shape as the shared path so the emitters stay
 * common; the section count grows by the extra .rela. */
static void elf_imp_prepare(void) {
    long total = 1;
    int i;
    long off;
    elf_ndyn = ld_nimports;
    for (i = 0; i < ld_nlibs; i++)
        total += (long)strlen(ld_lib_sonames[i]) + 1;
    for (i = 0; i < ld_nimports; i++)
        total += (long)strlen(ld_imports[i].name) + 1;
    elf_dynstr = (char *)malloc((size_t)total);
    elf_dynoff = (long *)malloc(sizeof(long) * (size_t)(ld_nimports > 0 ? ld_nimports : 1));
    elf_impoff = (long *)malloc(sizeof(long) * (size_t)(ld_nimports > 0 ? ld_nimports : 1));
    elf_needoff = (long *)malloc(sizeof(long) * (size_t)(ld_nlibs > 0 ? ld_nlibs : 1));
    if (!elf_dynstr || !elf_dynoff || !elf_impoff || !elf_needoff) {
        fprintf(stderr, "ld: out of memory\n");
        exit(1);
    }
    elf_dynstr[0] = 0;
    off = 1;
    for (i = 0; i < ld_nlibs; i++) {
        elf_needoff[i] = off;
        memcpy(elf_dynstr + off, ld_lib_sonames[i],
            strlen(ld_lib_sonames[i]) + 1);
        off += (long)strlen(ld_lib_sonames[i]) + 1;
    }
    for (i = 0; i < ld_nimports; i++) {
        elf_impoff[i] = off;
        elf_dynoff[i] = off;
        memcpy(elf_dynstr + off, ld_imports[i].name,
            strlen(ld_imports[i].name) + 1);
        off += (long)strlen(ld_imports[i].name) + 1;
    }
    elf_dynstr_size = off;
    elf_dyn_nentry = 9L + ld_nlibs;
    {
        int ti = 0;
        int li;
        for (li = 0; li < ld_nlibs; li++) {
            elf_dyn_tags[ti] = CFG_ELF_DT_NEEDED;
            elf_dyn_vals[ti] = (unsigned long long)elf_needoff[li];
            ti++;
        }
        elf_dyn_tags[ti] = CFG_ELF_DT_HASH;
        elf_dyn_vals[ti] = 0;
        ti++;
        elf_dyn_tags[ti] = CFG_ELF_DT_STRTAB;
        elf_dyn_vals[ti] = 0;
        ti++;
        elf_dyn_tags[ti] = CFG_ELF_DT_SYMTAB;
        elf_dyn_vals[ti] = 0;
        ti++;
        elf_dyn_tags[ti] = CFG_ELF_DT_STRSZ;
        elf_dyn_vals[ti] = (unsigned long long)elf_dynstr_size;
        ti++;
        elf_dyn_tags[ti] = CFG_ELF_DT_SYMENT;
        elf_dyn_vals[ti] = (unsigned long long)CFG_ELF_DYNSYM_ENT;
        ti++;
        elf_dyn_tags[ti] = CFG_ELF_DT_RELA;
        elf_dyn_vals[ti] = 0ULL;
        ti++;
        elf_dyn_tags[ti] = CFG_ELF_DT_RELASZ;
        elf_dyn_vals[ti] = 0ULL;
        ti++;
        elf_dyn_tags[ti] = CFG_ELF_DT_RELAENT;
        elf_dyn_vals[ti] = 24ULL;
        ti++;
        elf_dyn_tags[ti] = CFG_ELF_DT_NULL;
        elf_dyn_vals[ti] = 0ULL;
        ti++;
        (void)ti;
    }
    elf_dyn_base = x86_align_up(elf_data_base + elf_data_size, 8);
    elf_dynsym_base = elf_dyn_base + elf_dyn_nentry * 16L;
    elf_dynstr_base = x86_align_up(elf_dynsym_base + (elf_ndyn + 1) * CFG_ELF_DYNSYM_ENT, 8);
    elf_hash_base = x86_align_up(elf_dynstr_base + elf_dynstr_size, 8);
    elf_hash_size = 8 + 4L * (1 + (elf_ndyn + 1));
    elf_data_size = (elf_hash_base + elf_hash_size) - elf_data_base;
    elf_rela_base = x86_align_up(elf_data_base + elf_data_size, 8);
    elf_data_size = (elf_rela_base + (long)ld_nimports * 24L) - elf_data_base;
    elf_bss_base = x86_align_up(elf_data_base + elf_data_size, 16);
    elf_got_base = 0;
    elf_got_size = 0;
    if (elf_has_rela()) {
        elf_got_base = x86_align_up(elf_bss_base + elf_bss_size, 8);
        elf_got_size = 8L * ld_nimports;
        elf_bss_size = (elf_got_base - elf_bss_base) + elf_got_size;
    }
}

/* Dynamic tables exist for shared libraries and for executables
 * linked against them; plain executables stay byte-identical. */
static int elf_has_dyn(void) {
    return g_shared || ld_nlibs > 0;
}

/* Extra .rela section only for executables with imports (shared
 * libraries are self-contained by construction). */
static int elf_has_rela(void) {
    return !g_shared && ld_nlibs > 0;
}

static void elf_layout(void) {
    elf_text_base = CFG_ELF_HSIZE +
        (elf_has_dyn() ? CFG_ELF_PHNUM + 2 : CFG_ELF_PHNUM) * CFG_ELF_PHENTSZ;
    elf_text_size = code_len;
    elf_plt_base = 0;
    if (elf_has_rela()) {
        elf_plt_base = x86_align_up(elf_text_base + code_len, 16);
        elf_text_size = (elf_plt_base - elf_text_base) + 16L * ld_nimports;
    }
    elf_rodata_base = x86_align_up(elf_text_base + elf_text_size, 16);
    long rc = 0;
    long dc = 0;
    long bc = 0;
    for (int i = 0; i < n_syms; i++) {
        Sym *s = &syms[i];
        if (s->kind == SYM_BLOB) {
            long sz = s->end - s->base;
            if (sz < 0) sz = 0;
            if (s->sec == SEC_RODATA) {
                s->off = rc;
                rc += sz;
            } else {
                s->off = dc;
                dc += sz;
            }
        } else if (s->kind == SYM_GLOBAL) {
            bc = x86_align_up(bc, 8);
            s->off = bc;
            bc += s->size;
        }
    }
    elf_rodata_size = rc;
    elf_data_size = dc;
    elf_bss_size = bc;
    elf_data_base = x86_align_up(elf_rodata_base + elf_rodata_size, CFG_ELF_PAGE);
    elf_bss_base = x86_align_up(elf_data_base + elf_data_size, 16);
    elf_ndyn = 0;
    elf_dynstr = 0;
    elf_dynoff = 0;
    if (g_shared) elf_dyn_prepare();
    else if (ld_nlibs > 0) elf_imp_prepare();
}

/* Emit one dynamic table entry (tag + value, 16 bytes). */
static void elf_dyn_emit(unsigned char *d, long tag, unsigned long long val) {
    w64_at(d, (unsigned long long)tag);
    w64_at(d + 8, val);
}

/* Place the .rela table after resolve (its count is only known
 * once every fixup routed): extends the data region and pushes
 * .bss, and fills the RELA tag values the prepare could not know. */
static void elf_layout_rela(void) {
    int ti;
    if (!elf_has_rela()) return;
    for (ti = 0; ti < elf_dyn_nentry; ti++) {
        if (elf_dyn_tags[ti] == CFG_ELF_DT_RELA)
            elf_dyn_vals[ti] = (unsigned long long)elf_rela_base;
        else if (elf_dyn_tags[ti] == CFG_ELF_DT_RELASZ)
            elf_dyn_vals[ti] = (unsigned long long)ld_nimports * 24ULL;
    }
}

/* Write the executable import tables: null plus one UNDEF entry
 * per -d import (names via the import offsets), and one RELA row
 * per recorded fixup (offset, import-indexed info, call addend). */
static void elf_write_idynsym(FILE *f) {
    long n = (long)ld_nimports + 1;
    unsigned char *tab = (unsigned char *)malloc((size_t)n * CFG_ELF_DYNSYM_ENT);
    int i;
    if (!tab) {
        fprintf(stderr, "ld: out of memory\n");
        exit(1);
    }
    memset(tab, 0, (size_t)n * CFG_ELF_DYNSYM_ENT);
    for (i = 0; i < ld_nimports; i++) {
        unsigned char *e = tab + (size_t)(i + 1) * CFG_ELF_DYNSYM_ENT;
        w32(e + 0, (unsigned)elf_impoff[i]);
        e[4] = (unsigned char)((CFG_ELF_STB_GLOBAL << 4) |
            (ld_imports[i].is_func ? CFG_ELF_STT_FUNC : CFG_ELF_STT_NOTYPE));
        e[5] = 0;
        w16(e + 6, CFG_ELF_SHN_UNDEF);
        w64_at(e + 8, 0ULL);
        w64_at(e + 16, 0ULL);
    }
    while (ftell(f) < elf_dynsym_base) fputc(0, f);
    fwrite(tab, 1, (size_t)n * CFG_ELF_DYNSYM_ENT, f);
    free(tab);
}

static void elf_write_rela(FILE *f) {
    int i;
    while (ftell(f) < elf_rela_base) fputc(0, f);
    for (i = 0; i < ld_nimports; i++) {
        unsigned char e[24];
        memset(e, 0, sizeof(e));
        w64_at(e + 0, (unsigned long long)elf_got_base + (unsigned long long)i * 8ULL);
        w64_at(e + 8, ((unsigned long long)(i + 1) << 32) |
            (unsigned long long)CFG_ELF_R_GLOB_DAT);
        w64_at(e + 16, 0ULL);
        fwrite(e, 1, sizeof(e), f);
    }
}

/* Write one PLT stub (jmp through its GOT slot) plus fail-closed
 * padding: control must never fall through into the next stub. */
static void elf_write_plt(FILE *f, long stub, long got) {
    long disp = got - (stub + 6);
    fputc(0xFF, f);
    fputc(0x25, f);
    fputc((int)(disp & 255), f);
    fputc((int)((disp >> 8) & 255), f);
    fputc((int)((disp >> 16) & 255), f);
    fputc((int)((disp >> 24) & 255), f);
    {
        int k;
        for (k = 0; k < 10; k++) fputc(0xCC, f);
    }
}

/* Write the .dynamic table at its link address (caller padded there).
 * SONAME plus the tables a loader and host dlopen need; no NEEDED
 * (shared libs stay self-contained) and no INIT/FINI (miniGCC emits
 * no constructors). */
static void elf_write_dynamic(FILE *f) {
    unsigned char d[16 * 16];
    long i;
    memset(d, 0, sizeof(d));
    for (i = 0; i < elf_dyn_nentry; i++) {
        unsigned long long v = elf_dyn_vals[i];
        long tag = elf_dyn_tags[i];
        if (tag == CFG_ELF_DT_HASH) v = (unsigned long long)elf_hash_base;
        else if (tag == CFG_ELF_DT_STRTAB) v = (unsigned long long)elf_dynstr_base;
        else if (tag == CFG_ELF_DT_SYMTAB) v = (unsigned long long)elf_dynsym_base;
        elf_dyn_emit(d + (size_t)i * 16, tag, v);
    }
    while (ftell(f) < elf_dyn_base) fputc(0, f);
    fwrite(d, 1, (size_t)elf_dyn_nentry * 16, f);
}

/* Write the .dynsym table: null entry plus every defined,
 * non-shadowed function (shadowed bodies were skipped at encode,
 * so their offsets are stale). Values are link addresses; the
 * loader adds its slide. */
static void elf_write_dynsym(FILE *f) {
    long n = elf_ndyn + 1;
    unsigned char *tab = (unsigned char *)malloc((size_t)n * CFG_ELF_DYNSYM_ENT);
    long k = 1;
    int i;
    if (!tab) {
        fprintf(stderr, "ld: out of memory\n");
        exit(1);
    }
    memset(tab, 0, (size_t)n * CFG_ELF_DYNSYM_ENT);
    for (i = 0; i < n_syms; i++) {
        unsigned char *e;
        if (syms[i].kind != SYM_FUNC || syms[i].shadowed ||
                (g_shared && strcmp(syms[i].name, "_start") == 0)) continue;
        e = tab + k * CFG_ELF_DYNSYM_ENT;
        w32(e + 0, (unsigned)elf_dynoff[i]);
        e[4] = (unsigned char)((CFG_ELF_STB_GLOBAL << 4) | CFG_ELF_STT_FUNC);
        e[5] = 0;
        w16(e + 6, 1);
        w64_at(e + 8, (unsigned long long)elf_sym_addr(&syms[i]));
        w64_at(e + 16, 0ULL);
        k++;
    }
    while (ftell(f) < elf_dynsym_base) fputc(0, f);
    fwrite(tab, 1, (size_t)n * CFG_ELF_DYNSYM_ENT, f);
    free(tab);
}

/* Write the .dynstr pool built in layout. */
static void elf_write_dynstr(FILE *f) {
    while (ftell(f) < elf_dynstr_base) fputc(0, f);
    fwrite(elf_dynstr, 1, (size_t)elf_dynstr_size, f);
}

/* Write the SYSV hash over the dynamic symbols: one bucket plus
 * the chain table (chain[0] unused, chain[k] links k to k+1).
 * Single bucket keeps it honest and small; the loader walks names
 * linearly off bucket[0]. Complete enough for host dlopen. */
static void elf_write_hash(FILE *f) {
    long n = elf_ndyn + 1;
    long words = 2 + 1 + n;
    unsigned char *tab = (unsigned char *)malloc((size_t)words * 4);
    long k;
    if (!tab) {
        fprintf(stderr, "ld: out of memory\n");
        exit(1);
    }
    memset(tab, 0, (size_t)words * 4);
    w32(tab + 0, 1);
    w32(tab + 4, (unsigned)n);
    w32(tab + 8, n > 1 ? 1 : 0);
    for (k = 1; k < n; k++)
        w32(tab + 12 + (size_t)k * 4, (unsigned)(k + 1 < n ? k + 1 : 0));
    while (ftell(f) < elf_hash_base) fputc(0, f);
    fwrite(tab, 1, (size_t)words * 4, f);
    free(tab);
}

static void elf_write(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "ld: cannot write %s\n", path);
        exit(1);
    }

    long shoff = x86_align_up(elf_data_base + elf_data_size, 8);
    long phnum = elf_has_dyn() ? CFG_ELF_PHNUM + 2 : CFG_ELF_PHNUM;
    long shnum = 6 + (elf_has_dyn() ? 5 : 0) + (elf_has_rela() ? 1 : 0);
    long shstrndx = 5 + (elf_has_dyn() ? 5 : 0) + (elf_has_rela() ? 1 : 0);

    unsigned char eh[CFG_ELF_HSIZE];
    memset(eh, 0, sizeof(eh));
    eh[0] = 0x7F;
    eh[1] = 'E';
    eh[2] = 'L';
    eh[3] = 'F';
    eh[4] = 2;
    eh[5] = 1;
    eh[6] = 1;
    eh[7] = 0;
    w16(eh + 16, CFG_ELF_ET_DYN);
    w16(eh + 18, CFG_ELF_EM_X8664);
    w32(eh + 20, 1);
    w64_at(eh + 24, entry_sym >= 0 ?
        (unsigned long long)elf_sym_addr(&syms[entry_sym]) : 0);
    w64_at(eh + 32, CFG_ELF_HSIZE);
    w64_at(eh + 40, shoff);
    w16(eh + 52, CFG_ELF_HSIZE);
    w16(eh + 54, CFG_ELF_PHENTSZ);
    w16(eh + 56, (unsigned)phnum);
    w16(eh + 58, CFG_ELF_SHENTSZ);
    w16(eh + 60, (unsigned)shnum);
    w16(eh + 62, (unsigned)shstrndx);
    fwrite(eh, 1, sizeof(eh), f);

    unsigned char ph[CFG_ELF_PHENTSZ * (CFG_ELF_PHNUM + 2)];
    memset(ph, 0, sizeof(ph));
    w32(ph + 0, CFG_ELF_PT_LOAD);
    w32(ph + 4, CFG_ELF_PF_R | CFG_ELF_PF_X);
    w64_at(ph + 8, 0);
    w64_at(ph + 16, 0);
    w64_at(ph + 24, 0);
    w64_at(ph + 32, (unsigned long long)elf_data_base);
    w64_at(ph + 40, (unsigned long long)elf_data_base);
    w64_at(ph + 48, CFG_ELF_PAGE);
    w32(ph + 56, CFG_ELF_PT_LOAD);
    w32(ph + 60, CFG_ELF_PF_R | CFG_ELF_PF_W);
    w64_at(ph + 64, (unsigned long long)elf_data_base);
    w64_at(ph + 72, (unsigned long long)elf_data_base);
    w64_at(ph + 80, (unsigned long long)elf_data_base);
    w64_at(ph + 88, (unsigned long long)elf_data_size);
    /* memsz runs to the BSS end, not data+bss: the data-to-BSS
     * alignment padding belongs to the segment, or the BSS tail
     * (the GOT for -d executables) sticks out past the mapping and
     * any exact loader refuses the image (glibc only survives on
     * page slack). */
    w64_at(ph + 96, (unsigned long long)((elf_bss_base + elf_bss_size) -
        elf_data_base));
    w64_at(ph + 104, CFG_ELF_PAGE);
    if (elf_has_dyn()) {
        w32(ph + 112, CFG_ELF_PT_DYNAMIC);
        w32(ph + 116, CFG_ELF_PF_R | CFG_ELF_PF_W);
        w64_at(ph + 120, (unsigned long long)elf_dyn_base);
        w64_at(ph + 128, (unsigned long long)elf_dyn_base);
        w64_at(ph + 136, (unsigned long long)elf_dyn_base);
        w64_at(ph + 144, (unsigned long long)elf_dyn_nentry * 16ULL);
        w64_at(ph + 152, (unsigned long long)elf_dyn_nentry * 16ULL);
        w64_at(ph + 160, 8);
        w32(ph + 168, CFG_ELF_PT_GNU_STACK);
        w32(ph + 172, CFG_ELF_PF_R | CFG_ELF_PF_W);
        w64_at(ph + 176, 0ULL);
        w64_at(ph + 184, 0ULL);
        w64_at(ph + 192, 0ULL);
        w64_at(ph + 200, 0ULL);
        w64_at(ph + 208, 0ULL);
        w64_at(ph + 216, 8);
    }
    fwrite(ph, 1, (size_t)phnum * CFG_ELF_PHENTSZ, f);

    if (elf_text_base > (long)sizeof(eh) + (long)phnum * CFG_ELF_PHENTSZ) {
        for (long i = (long)sizeof(eh) + (long)phnum * CFG_ELF_PHENTSZ; i < elf_text_base; i++)
            fputc(0, f);
    }
    fwrite(code, 1, (size_t)code_len, f);
    if (elf_has_rela()) {
        int i;
        while (ftell(f) < elf_plt_base) fputc(0, f);
        for (i = 0; i < ld_nimports; i++)
            elf_write_plt(f, elf_plt_base + (long)i * 16L,
                elf_got_base + (long)i * 8L);
    }
    while (ftell(f) < elf_rodata_base) fputc(0, f);
    for (int i = 0; i < n_syms; i++) {
        Sym *s = &syms[i];
        if (s->kind != SYM_BLOB || s->sec != SEC_RODATA) continue;
        long sz = s->end - s->base;
        if (sz <= 0) continue;
        fwrite(blob_data + s->base, 1, (size_t)sz, f);
    }
    while (ftell(f) < elf_data_base) fputc(0, f);
    for (int i = 0; i < n_syms; i++) {
        Sym *s = &syms[i];
        if (s->kind != SYM_BLOB || s->sec != SEC_DATA) continue;
        long sz = s->end - s->base;
        if (sz <= 0) continue;
        fwrite(blob_data + s->base, 1, (size_t)sz, f);
    }
    if (elf_has_dyn()) {
        elf_write_dynamic(f);
        if (g_shared) elf_write_dynsym(f);
        else elf_write_idynsym(f);
        elf_write_dynstr(f);
        elf_write_hash(f);
        if (elf_has_rela()) elf_write_rela(f);
    }
    while (ftell(f) < shoff) fputc(0, f);

    static const char shstr_base[] = "\0.text\0.rodata\0.data\0.bss\0.shstrtab\0";
    static const char shstr_ext[] = ".dynamic\0.dynsym\0.dynstr\0.hash\0.rela\0";
    char shstr[96];
    long shstr_size = (long)sizeof(shstr_base) - 1;
    unsigned char sh[CFG_ELF_SHENTSZ * (CFG_ELF_SHNUM + 6)];
    memcpy(shstr, shstr_base, (size_t)shstr_size);
    if (elf_has_dyn()) {
        memcpy(shstr + shstr_size, shstr_ext, sizeof(shstr_ext) - 1);
        shstr_size += (long)sizeof(shstr_ext) - 1;
        if (!elf_has_rela())
            shstr_size -= (long)sizeof(".rela") - 1;
    }
    memset(sh, 0, sizeof(sh));
    w32(sh + 0 * CFG_ELF_SHENTSZ + 4, 0);
    w32(sh + 1 * CFG_ELF_SHENTSZ + 0, 1);
    w32(sh + 1 * CFG_ELF_SHENTSZ + 4, CFG_ELF_SHT_PROGBITS);
    w64_at(sh + 1 * CFG_ELF_SHENTSZ + 8, CFG_ELF_SHF_A | CFG_ELF_SHF_X);
    w64_at(sh + 1 * CFG_ELF_SHENTSZ + 16, (unsigned long long)elf_text_base);
    w64_at(sh + 1 * CFG_ELF_SHENTSZ + 24, (unsigned long long)elf_text_base);
    w64_at(sh + 1 * CFG_ELF_SHENTSZ + 32, (unsigned long long)elf_text_size);
    w64_at(sh + 1 * CFG_ELF_SHENTSZ + 48, 16);
    w32(sh + 2 * CFG_ELF_SHENTSZ + 0, 7);
    w32(sh + 2 * CFG_ELF_SHENTSZ + 4, CFG_ELF_SHT_PROGBITS);
    w64_at(sh + 2 * CFG_ELF_SHENTSZ + 8, CFG_ELF_SHF_A);
    w64_at(sh + 2 * CFG_ELF_SHENTSZ + 16, (unsigned long long)elf_rodata_base);
    w64_at(sh + 2 * CFG_ELF_SHENTSZ + 24, (unsigned long long)elf_rodata_base);
    w64_at(sh + 2 * CFG_ELF_SHENTSZ + 32, (unsigned long long)elf_rodata_size);
    w64_at(sh + 2 * CFG_ELF_SHENTSZ + 48, 8);
    w32(sh + 3 * CFG_ELF_SHENTSZ + 0, 15);
    w32(sh + 3 * CFG_ELF_SHENTSZ + 4, CFG_ELF_SHT_PROGBITS);
    w64_at(sh + 3 * CFG_ELF_SHENTSZ + 8, CFG_ELF_SHF_A | CFG_ELF_SHF_W);
    w64_at(sh + 3 * CFG_ELF_SHENTSZ + 16, (unsigned long long)elf_data_base);
    w64_at(sh + 3 * CFG_ELF_SHENTSZ + 24, (unsigned long long)elf_data_base);
    w64_at(sh + 3 * CFG_ELF_SHENTSZ + 32, (unsigned long long)elf_data_size);
    w64_at(sh + 3 * CFG_ELF_SHENTSZ + 48, 8);
    w32(sh + 4 * CFG_ELF_SHENTSZ + 0, 21);
    w32(sh + 4 * CFG_ELF_SHENTSZ + 4, CFG_ELF_SHT_NOBITS);
    w64_at(sh + 4 * CFG_ELF_SHENTSZ + 8, CFG_ELF_SHF_A | CFG_ELF_SHF_W);
    w64_at(sh + 4 * CFG_ELF_SHENTSZ + 16, (unsigned long long)elf_bss_base);
    w64_at(sh + 4 * CFG_ELF_SHENTSZ + 24, (unsigned long long)(elf_data_base + elf_data_size));
    w64_at(sh + 4 * CFG_ELF_SHENTSZ + 32, (unsigned long long)elf_bss_size);
    w64_at(sh + 4 * CFG_ELF_SHENTSZ + 48, 8);
    if (elf_has_dyn()) {
        w32(sh + 6 * CFG_ELF_SHENTSZ + 0, 36);
        w32(sh + 6 * CFG_ELF_SHENTSZ + 4, CFG_ELF_SHT_DYNAMIC);
        w64_at(sh + 6 * CFG_ELF_SHENTSZ + 8, CFG_ELF_SHF_A | CFG_ELF_SHF_W);
        w64_at(sh + 6 * CFG_ELF_SHENTSZ + 16, (unsigned long long)elf_dyn_base);
        w64_at(sh + 6 * CFG_ELF_SHENTSZ + 24, (unsigned long long)elf_dyn_base);
        w64_at(sh + 6 * CFG_ELF_SHENTSZ + 32, (unsigned long long)elf_dyn_nentry * 16ULL);
        w32(sh + 6 * CFG_ELF_SHENTSZ + 40, 8);
        w32(sh + 6 * CFG_ELF_SHENTSZ + 44, 0);
        w64_at(sh + 6 * CFG_ELF_SHENTSZ + 48, 8);
        w64_at(sh + 6 * CFG_ELF_SHENTSZ + 56, 16);
        w32(sh + 7 * CFG_ELF_SHENTSZ + 0, 45);
        w32(sh + 7 * CFG_ELF_SHENTSZ + 4, CFG_ELF_SHT_DYNSYM);
        w64_at(sh + 7 * CFG_ELF_SHENTSZ + 8, CFG_ELF_SHF_A);
        w64_at(sh + 7 * CFG_ELF_SHENTSZ + 16, (unsigned long long)elf_dynsym_base);
        w64_at(sh + 7 * CFG_ELF_SHENTSZ + 24, (unsigned long long)elf_dynsym_base);
        w64_at(sh + 7 * CFG_ELF_SHENTSZ + 32, (unsigned long long)(elf_ndyn + 1) * CFG_ELF_DYNSYM_ENT);
        w32(sh + 7 * CFG_ELF_SHENTSZ + 40, 8);
        w32(sh + 7 * CFG_ELF_SHENTSZ + 44, 1);
        w64_at(sh + 7 * CFG_ELF_SHENTSZ + 48, 8);
        w64_at(sh + 7 * CFG_ELF_SHENTSZ + 56, CFG_ELF_DYNSYM_ENT);
        w32(sh + 8 * CFG_ELF_SHENTSZ + 0, 53);
        w32(sh + 8 * CFG_ELF_SHENTSZ + 4, CFG_ELF_SHT_STRTAB);
        w64_at(sh + 8 * CFG_ELF_SHENTSZ + 8, CFG_ELF_SHF_A);
        w64_at(sh + 8 * CFG_ELF_SHENTSZ + 16, (unsigned long long)elf_dynstr_base);
        w64_at(sh + 8 * CFG_ELF_SHENTSZ + 24, (unsigned long long)elf_dynstr_base);
        w64_at(sh + 8 * CFG_ELF_SHENTSZ + 32, (unsigned long long)elf_dynstr_size);
        w64_at(sh + 8 * CFG_ELF_SHENTSZ + 48, 1);
        w32(sh + 9 * CFG_ELF_SHENTSZ + 0, 61);
        w32(sh + 9 * CFG_ELF_SHENTSZ + 4, CFG_ELF_SHT_HASH);
        w64_at(sh + 9 * CFG_ELF_SHENTSZ + 8, CFG_ELF_SHF_A);
        w64_at(sh + 9 * CFG_ELF_SHENTSZ + 16, (unsigned long long)elf_hash_base);
        w64_at(sh + 9 * CFG_ELF_SHENTSZ + 24, (unsigned long long)elf_hash_base);
        w64_at(sh + 9 * CFG_ELF_SHENTSZ + 32, (unsigned long long)elf_hash_size);
        w32(sh + 9 * CFG_ELF_SHENTSZ + 40, 7);
        w32(sh + 9 * CFG_ELF_SHENTSZ + 44, 0);
        w64_at(sh + 9 * CFG_ELF_SHENTSZ + 48, 4);
        w64_at(sh + 9 * CFG_ELF_SHENTSZ + 56, 4);
        w32(sh + 10 * CFG_ELF_SHENTSZ + 48, 1);
    }
    if (elf_has_rela()) {
        w32(sh + 10 * CFG_ELF_SHENTSZ + 0, 67);
        w32(sh + 10 * CFG_ELF_SHENTSZ + 4, CFG_ELF_SHT_RELA);
        w64_at(sh + 10 * CFG_ELF_SHENTSZ + 8, CFG_ELF_SHF_A);
        w64_at(sh + 10 * CFG_ELF_SHENTSZ + 16, (unsigned long long)elf_rela_base);
        w64_at(sh + 10 * CFG_ELF_SHENTSZ + 24, (unsigned long long)elf_rela_base);
        w64_at(sh + 10 * CFG_ELF_SHENTSZ + 32, (unsigned long long)ld_nimports * 24ULL);
        w32(sh + 10 * CFG_ELF_SHENTSZ + 40, 7);
        w32(sh + 10 * CFG_ELF_SHENTSZ + 44, 0);
        w64_at(sh + 10 * CFG_ELF_SHENTSZ + 48, 8);
        w64_at(sh + 10 * CFG_ELF_SHENTSZ + 56, 24);
    }
    {
        long sidx = shstrndx;
        w32(sh + sidx * CFG_ELF_SHENTSZ + 0, 26);
        w32(sh + sidx * CFG_ELF_SHENTSZ + 4, CFG_ELF_SHT_STRTAB);
        w64_at(sh + sidx * CFG_ELF_SHENTSZ + 24, (unsigned long long)(shoff + CFG_ELF_SHENTSZ * shnum));
        w64_at(sh + sidx * CFG_ELF_SHENTSZ + 32, (unsigned long long)shstr_size);
        w64_at(sh + sidx * CFG_ELF_SHENTSZ + 48, 1);
    }
    fwrite(sh, 1, (size_t)CFG_ELF_SHENTSZ * (size_t)shnum, f);
    fwrite(shstr, 1, (size_t)shstr_size, f);
    fclose(f);
}

static unsigned r16at(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned long r64at(const unsigned char *p) {
    unsigned long v = 0;
    int i;
    for (i = 7; i >= 0; i--) v = (v << 8) | p[i];
    return v;
}

/* Read one ET_DYN library: verify the header, collect global
 * function symbols (plus non-function globals, so a data import
 * fails with "unsupported" instead of "undefined") and the SONAME
 * for DT_NEEDED. Every bound is checked before use; any violation
 * is a hard link error, never a partial table. */
static int ld_read_lib(const char *path, int libidx) {
    FILE *f;
    long fsize;
    unsigned char eh[64];
    unsigned long shoff;
    unsigned shentsz;
    unsigned shnum;
    unsigned i;
    unsigned dynsym = 0;
    unsigned dynsym_n = 0;
    unsigned dynstr = 0;
    unsigned dynstr_n = 0;
    unsigned dynamic = 0;
    unsigned dynamic_n = 0;
    unsigned char *sec = 0;
    int have_soname = 0;
    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ld: cannot open library %s\n", path);
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "ld: cannot size library %s\n", path);
        fclose(f);
        return -1;
    }
    fsize = ftell(f);
    if (fsize < 64 || fsize > CFG_LD_LIB_MAXSZ) {
        fprintf(stderr, "ld: bad library size %s\n", path);
        fclose(f);
        return -1;
    }
    rewind(f);
    if (fread(eh, 1, 64, f) != 64) {
        fprintf(stderr, "ld: cannot read library %s\n", path);
        fclose(f);
        return -1;
    }
    if (eh[0] != 0x7F || eh[1] != 'E' || eh[2] != 'L' || eh[3] != 'F' ||
            eh[4] != 2 || r16at(eh + 16) != 3) {
        fprintf(stderr, "ld: %s is not a 64-bit ET_DYN library\n", path);
        fclose(f);
        return -1;
    }
    shoff = r64at(eh + 40);
    shentsz = r16at(eh + 58);
    shnum = r16at(eh + 60);
    if (shentsz != 64 || shnum == 0 || shnum > 64 ||
            shoff > (unsigned long)fsize ||
            shnum > ((unsigned long)fsize - shoff) / 64) {
        fprintf(stderr, "ld: bad section table %s\n", path);
        fclose(f);
        return -1;
    }
    sec = (unsigned char *)malloc((size_t)shnum * 64);
    if (!sec) {
        fprintf(stderr, "ld: out of memory\n");
        fclose(f);
        return -1;
    }
    if (fseek(f, (long)shoff, SEEK_SET) != 0 ||
            fread(sec, 64, shnum, f) != shnum) {
        fprintf(stderr, "ld: cannot read sections %s\n", path);
        free(sec);
        fclose(f);
        return -1;
    }
    for (i = 0; i < shnum; i++) {
        unsigned type = (unsigned)(sec[i * 64 + 4] |
            ((unsigned)sec[i * 64 + 5] << 8) |
            ((unsigned)sec[i * 64 + 6] << 16) |
            ((unsigned)sec[i * 64 + 7] << 24));
        unsigned long off = r64at(sec + i * 64 + 24);
        unsigned long size = r64at(sec + i * 64 + 32);
        unsigned link = (unsigned)(sec[i * 64 + 40] |
            ((unsigned)sec[i * 64 + 41] << 8) |
            ((unsigned)sec[i * 64 + 42] << 16) |
            ((unsigned)sec[i * 64 + 43] << 24));
        if (off > (unsigned long)fsize || size > (unsigned long)fsize - off)
            continue;
        if (type == 11 && link < shnum) {
            dynsym = (unsigned)off;
            dynsym_n = (unsigned)(size / 24);
            {
                unsigned long soff;
                unsigned long ssize;
                if (fseek(f, 0, SEEK_END) == 0) { (void)0; }
                soff = r64at(sec + link * 64 + 24);
                ssize = r64at(sec + link * 64 + 32);
                if (soff <= (unsigned long)fsize &&
                        ssize <= (unsigned long)fsize - soff) {
                    dynstr = (unsigned)soff;
                    dynstr_n = (unsigned)ssize;
                }
            }
        } else if (type == 6) {
            dynamic = (unsigned)off;
            dynamic_n = (unsigned)(size / 16);
        }
    }
    if (dynsym_n == 0 || dynstr_n == 0 || dynamic == 0) {
        fprintf(stderr, "ld: %s has no dynamic symbols\n", path);
        free(sec);
        fclose(f);
        return -1;
    }
    {
        unsigned char *dp = (unsigned char *)malloc(16);
        unsigned k;
        if (!dp) {
            fprintf(stderr, "ld: out of memory\n");
            free(sec);
            fclose(f);
            return -1;
        }
        for (k = 0; k < dynamic_n; k++) {
            long at = (long)dynamic + (long)k * 16;
            if (at < 0 || (unsigned long)at + 16 > (unsigned long)fsize)
                break;
            if (fseek(f, at, SEEK_SET) != 0) break;
            if (fread(dp, 1, 16, f) != 16) break;
            if (r64at(dp) == 14) {
                unsigned long so = r64at(dp + 8);
                unsigned long m = 0;
                if (so >= dynstr_n) break;
                while (so + m < dynstr_n && m + 1 < CFG_NAME_MAX) {
                    long at2 = (long)dynstr + (long)so + (long)m;
                    unsigned char c;
                    if (at2 < 0 || (unsigned long)at2 >= (unsigned long)fsize)
                        break;
                    if (fseek(f, at2, SEEK_SET) != 0) break;
                    if (fread(&c, 1, 1, f) != 1) break;
                    ld_lib_sonames[libidx][m] = (char)c;
                    if (!c) break;
                    m++;
                }
                ld_lib_sonames[libidx][CFG_NAME_MAX - 1] = 0;
                if (m > 0) have_soname = 1;
                break;
            }
            if (r64at(dp) == 0) break;
        }
        free(dp);
    }
    if (!have_soname) {
        fprintf(stderr, "ld: %s has no SONAME\n", path);
        free(sec);
        fclose(f);
        return -1;
    }
    {
        unsigned char sym[24];
        unsigned char *pool;
        unsigned k;
        pool = (unsigned char *)malloc(dynstr_n ? dynstr_n : 1);
        if (!pool) {
            fprintf(stderr, "ld: out of memory\n");
            free(sec);
            fclose(f);
            return -1;
        }
        if (fseek(f, (long)dynstr, SEEK_SET) != 0 ||
                fread(pool, 1, dynstr_n, f) != dynstr_n) {
            fprintf(stderr, "ld: cannot read strings %s\n", path);
            free(pool);
            free(sec);
            fclose(f);
            return -1;
        }
        for (k = 1; k < dynsym_n; k++) {
            long at = (long)dynsym + (long)k * 24;
            unsigned info;
            unsigned shndx;
            unsigned name;
            unsigned m;
            if (at < 0 || (unsigned long)at + 24 > (unsigned long)fsize)
                break;
            if (fseek(f, at, SEEK_SET) != 0) break;
            if (fread(sym, 1, 24, f) != 24) break;
            if ((sym[4] >> 4) != 1) continue;
            shndx = (unsigned)sym[6] | ((unsigned)sym[7] << 8);
            if (shndx == 0) continue;
            if (ld_nimports >= CFG_LD_MAXIMPORTS) {
                fprintf(stderr, "ld: too many imports from %s\n", path);
                free(pool);
                free(sec);
                fclose(f);
                return -1;
            }
            info = sym[4] & 0xF;
            name = (unsigned)sym[0] | ((unsigned)sym[1] << 8) |
                ((unsigned)sym[2] << 16) | ((unsigned)sym[3] << 24);
            for (m = 0; m + 1 < CFG_NAME_MAX && name + m < dynstr_n; m++) {
                ld_imports[ld_nimports].name[m] = (char)pool[name + m];
                if (!pool[name + m]) break;
            }
            ld_imports[ld_nimports].name[CFG_NAME_MAX - 1] = 0;
            ld_imports[ld_nimports].is_func = (info == 2);
            ld_imports[ld_nimports].value = (long)r64at(sym + 8);
            if (ld_imports[ld_nimports].name[0]) ld_nimports++;
        }
        free(pool);
    }
    free(sec);
    fclose(f);
    return 0;
}

static void elf_build(const char *in_path, const char *out_path) {
    LineSrc src;
    ld_soname_of(out_path, elf_soname);
    ls_open_file(&src, in_path);
    scan_src(&src, 0);
    ls_close(&src);

    /* A shared library is self-contained by contract (L1): the stubs
     * would both shadow the library's own definitions and wrap syscalls
     * into the shared image, so -shared injects none. Any syscall the
     * library needs must be defined by its source. */
    if (!g_shared) {
    LineSrc stub;
    ls_open_mem(&stub, ELF_STUBS_SRC);
    scan_src(&stub, 1);
    ls_close(&stub);

    LineSrc stub_b;
    ls_open_mem(&stub_b, ELF_STUBS_SRC_B);
    scan_src(&stub_b, 1);
    ls_close(&stub_b);

    LineSrc stub_c;
    ls_open_mem(&stub_c, ELF_STUBS_SRC_C);
    scan_src(&stub_c, 1);
    ls_close(&stub_c);

    LineSrc stub_d;
    ls_open_mem(&stub_d, ELF_STUBS_SRC_D);
    scan_src(&stub_d, 1);
    ls_close(&stub_d);

    LineSrc stub_e;
    ls_open_mem(&stub_e, ELF_STUBS_SRC_E);
    scan_src(&stub_e, 1);
    ls_close(&stub_e);
    }

    int start_idx = find_sym("_start");
    if (start_idx < 0) start_idx = find_sym("main");
    if (start_idx < 0 && !g_shared) {
        fprintf(stderr, "ld: no entry point (need _start or main)\n");
        ld_release();
        exit(1);
    }
    entry_sym = start_idx;

    ls_open_file(&src, in_path);
    elf_encode_src(&src);
    ls_close(&src);

    /* The user's calls are now known as fixups: prune imports and keep
     * only the runtime routines this program actually runs, before any
     * stub body is encoded. */
    elf_prune();

    if (!g_shared) {
    LineSrc stub2;
    ls_open_mem(&stub2, ELF_STUBS_SRC);
    elf_encode_src(&stub2);
    ls_close(&stub2);

    LineSrc stub2_b;
    ls_open_mem(&stub2_b, ELF_STUBS_SRC_B);
    elf_encode_src(&stub2_b);
    ls_close(&stub2_b);

    LineSrc stub2_c;
    ls_open_mem(&stub2_c, ELF_STUBS_SRC_C);
    elf_encode_src(&stub2_c);
    ls_close(&stub2_c);

    LineSrc stub2_d;
    ls_open_mem(&stub2_d, ELF_STUBS_SRC_D);
    elf_encode_src(&stub2_d);
    ls_close(&stub2_d);

    LineSrc stub2_e;
    ls_open_mem(&stub2_e, ELF_STUBS_SRC_E);
    elf_encode_src(&stub2_e);
    ls_close(&stub2_e);
    }

    if (error_count) {
        fprintf(stderr, "ld: %d error(s)\n", error_count);
        ld_release();
        exit(1);
    }

    elf_layout();
    elf_resolve_fixups();
    if (error_count) {
        fprintf(stderr, "ld: %d error(s)\n", error_count);
        ld_release();
        exit(1);
    }
    elf_layout_rela();
    elf_write(out_path);
    ld_release();
}

/* ================================================================
 *  CLI
 * ================================================================ */

static void usage(void) {
    fprintf(stderr,
        "usage: ld [-f cvm|elf] [-shared] [-o out] [-xstack N] input.s\n"
        "  -f cvm     CVM v2 module (default)\n"
        "  -f elf     static PIE Linux ELF executable\n"
        "  -shared    shared library (implies ELF: ET_DYN plus SONAME\n"
        "             and dynsym/dynstr/hash tables; no entry needed,\n"
        "             undefined functions stay hard errors)\n"
        "  -d lib.so  link undefined functions against a shared\n"
        "             library (repeatable): verifies every extern and\n"
        "             records RELA PC32 fixups plus DT_NEEDED\n"
        "  -o out     output path (default: input.cvm / input.elf)\n"
        "  -xstack N  x86 stack size in bytes for the CVM backend (default %d)\n",
        CFG_XSTACK_DEF);
    exit(1);
}

int main(int argc, char **argv) {
    const char *in = 0;
    char out[CFG_NAME_MAX];
    out[0] = 0;
    int fmt = CFG_FMT_CVM;
    g_xstack = CFG_XSTACK_DEF;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-runtime") == 0) {
            /* Each chunk is scanned independently in the linker (its
             * own section state); concatenated they would inherit the
             * previous chunk's .data/.bss and land labels there. A
             * .text reset before every chunk keeps them functions. */
            fputs(".text\n", stdout);
            fputs(ELF_STUBS_SRC, stdout);
            fputs(".text\n", stdout);
            fputs(ELF_STUBS_SRC_B, stdout);
            fputs(".text\n", stdout);
            fputs(ELF_STUBS_SRC_C, stdout);
            fputs(".text\n", stdout);
            fputs(ELF_STUBS_SRC_D, stdout);
            fputs(".text\n", stdout);
            fputs(ELF_STUBS_SRC_E, stdout);
            return 0;
        }
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            strncpy(out, argv[++i], CFG_NAME_MAX - 1);
            out[CFG_NAME_MAX - 1] = 0;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            const char *f = argv[++i];
            if (strcmp(f, "cvm") == 0) fmt = CFG_FMT_CVM;
            else if (strcmp(f, "elf") == 0) fmt = CFG_FMT_ELF;
            else usage();
        } else if (strcmp(argv[i], "-xstack") == 0 && i + 1 < argc) {
            g_xstack = parse_num(argv[++i]);
            if (g_xstack < CFG_STACK_BASE) {
                fprintf(stderr, "ld: stack size too small\n");
                exit(1);
            }
        } else if (strcmp(argv[i], "-shared") == 0) {
            g_shared = 1;
            fmt = CFG_FMT_ELF;
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            if (ld_nlibs >= CFG_LD_MAXLIBS) {
                fprintf(stderr, "ld: too many -d libraries\n");
                exit(1);
            }
            strncpy(ld_lib_paths[ld_nlibs], argv[++i], CFG_NAME_MAX - 1);
            ld_lib_paths[ld_nlibs][CFG_NAME_MAX - 1] = 0;
            ld_nlibs++;
        } else if (argv[i][0] == '-') {
            usage();
        } else if (!in) {
            in = argv[i];
        } else usage();
    }
    if (!in) usage();
    if (!out[0]) {
        strncpy(out, in, CFG_NAME_MAX - 1);
        out[CFG_NAME_MAX - 1] = 0;
        long n = (long)strlen(out);
        const char *ext = (fmt == CFG_FMT_ELF) ? "elf" : "cvm";
        if (n > 2 && out[n - 1] == 's' && out[n - 2] == '.') {
            out[n - 1] = 0;
            strncat(out, ext, CFG_NAME_MAX - strlen(out) - 1);
        } else {
            strncat(out, ".", CFG_NAME_MAX - strlen(out) - 1);
            strncat(out, ext, CFG_NAME_MAX - strlen(out) - 1);
        }
    }
    strncpy(cur_file, in, CFG_NAME_MAX - 1);
    cur_file[CFG_NAME_MAX - 1] = 0;

    if (fmt == CFG_FMT_ELF) {
        int li;
        if (g_shared && ld_nlibs > 0) {
            fprintf(stderr, "ld: -shared takes no -d libraries (leaf-only)\n");
            exit(1);
        }
        for (li = 0; li < ld_nlibs; li++) {
            if (ld_read_lib(ld_lib_paths[li], li) != 0) exit(1);
        }
        elf_build(in, out);
    } else {
        if (ld_nlibs > 0 || g_shared) {
            fprintf(stderr, "ld: -d and -shared need ELF (-f cvm takes neither)\n");
            exit(1);
        }
        LineSrc src;
        ls_open_file(&src, in);
        scan_src(&src, 0);
        ls_close(&src);

        cvm_prepare_tables();
        int start_idx = -1;
        for (int i = 0; i < n_syms; i++) {
            if (syms[i].kind == SYM_FUNC && strcmp(syms[i].name, "_start") == 0) { start_idx = i; break; }
        }
        if (start_idx < 0) {
            for (int i = 0; i < n_syms; i++) {
                if (syms[i].kind == SYM_FUNC && strcmp(syms[i].name, "main") == 0) { start_idx = i; break; }
            }
        }
        if (start_idx < 0) {
            fprintf(stderr, "ld: no entry point (need _start or main)\n");
            ld_release();
            exit(1);
        }
        entry_func = (int)syms[start_idx].fidx;
        entry_sym = start_idx;

        cvm_layout_data();

        LineSrc src2;
        ls_open_file(&src2, in);
        cvm_encode(&src2);
        ls_close(&src2);

        cvm_write_module(out);
        fprintf(stderr, "ld: %s -> %s (%d funcs, %d globals, %d natives, %ld code bytes, %ld data bytes)\n",
                in, out, n_funcs, n_globals, n_nats, code_len, data_len);
        ld_release();
        return 0;
    }
    ld_release();
    return 0;
}
