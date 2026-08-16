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

#define CFG_ABI_BYTES   96
#define CFG_STACK_BASE  96
#define CFG_XSTACK_DEF  262144

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
#define CFG_ELF_SHT_PROGBITS 1
#define CFG_ELF_SHT_NOBITS   8
#define CFG_ELF_SHT_STRTAB   3
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
#define OP_AND          48
#define OP_OR           49
#define OP_XOR          50
#define OP_NOT          51
#define OP_SHL          52
#define OP_SHR          53
#define OP_CMP_EQ       64
#define OP_CMP_NE       65
#define OP_CMP_LT       66
#define OP_CMP_LE       67
#define OP_CMP_GT       68
#define OP_CMP_GE       69
#define OP_LNOT         70
#define OP_JMP          80
#define OP_JZ           81
#define OP_JNZ          82
#define OP_CALL         96
#define OP_RET          97
#define OP_CALL_NATIVE  98
#define OP_LOAD8        112
#define OP_LOAD32       113
#define OP_LOAD64       114
#define OP_STORE8       115
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

static long synth_n;

/* ================================================================
 *  Diagnostics and memory
 * ================================================================ */

static void die(const char *msg) {
    fprintf(stderr, "ld: %s:%ld: %s\n", cur_file, cur_line, msg);
    error_count++;
    if (error_count > CFG_MAX_ERRORS) {
        fprintf(stderr, "ld: too many errors, aborting\n");
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

static void strip_comment(char *s) {
    char *c = strchr(s, '#');
    if (c) *c = 0;
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

static int parse_reg(const char *s, int *reg, int *sz) {
    for (int i = 0; reg64_names[i]; i++)
        if (strcmp(s, reg64_names[i]) == 0) { *reg = i; *sz = 8; return 1; }
    for (int i = 0; reg32_names[i]; i++)
        if (strcmp(s, reg32_names[i]) == 0) { *reg = i; *sz = 4; return 1; }
    for (int i = 0; reg8_names[i]; i++)
        if (strcmp(s, reg8_names[i]) == 0) { *reg = i; *sz = 1; return 1; }
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
    if (s[0] == '$') {
        op->kind = K_IMM;
        op->imm = parse_num(s + 1);
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
                        if (i >= 0) syms[i].shadowed = 0;
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
    if (op->kind == K_SYM) {
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
            if (cvm_find_func(op->sym) >= 0) {
                die("address of function not supported");
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
        else e1(OP_LOAD8);
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
    } else die("invalid arithmetic operand");
}

static void cvm_translate(const char *mn, Op *o1, Op *o2) {
    if (strcmp(mn, "movq") == 0) { mov(8, o1, o2); return; }
    if (strcmp(mn, "movl") == 0) { mov(4, o1, o2); return; }
    if (strcmp(mn, "movb") == 0) { mov(1, o1, o2); return; }
    if (strcmp(mn, "movzbq") == 0) {
        if (o1->kind == K_REG) { epush_reg(o1->reg); push_mask8(); estore_reg(o2->reg); }
        else { elea_operand(o1); e1(OP_LOAD8); push_mask8(); estore_reg(o2->reg); }
        return;
    }
    if (strcmp(mn, "movsbq") == 0) {
        if (o1->kind == K_REG) { epush_reg(o1->reg); signext8(); estore_reg(o2->reg); }
        else { elea_operand(o1); e1(OP_LOAD8); estore_reg(o2->reg); }
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
    if (strcmp(mn, "andq") == 0) {
        if (o2->kind == K_REG) arith_reg(OP_AND, 8, o2, o1);
        else arith_mem(OP_AND, 8, o2, o1);
        return;
    }
    if (strcmp(mn, "orq") == 0) {
        if (o2->kind == K_REG) arith_reg(OP_OR, 8, o2, o1);
        else arith_mem(OP_OR, 8, o2, o1);
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
    if (strcmp(mn, "testq") == 0) {
        epush_value(o1, 8);
        epush_value(o2, 8);
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
    if (strcmp(mn, "jmp") == 0) { ejmp(o1->sym); return; }
    if (strcmp(mn, "call") == 0) {
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
    if (strcmp(mn, "cqto") == 0) {
        epush_local(0);
        eimm(63);
        e1(OP_SHR);
        estore_local(2);
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
    data_fill(CFG_ABI_BYTES, 0);
    while (data_len < CFG_STACK_BASE) data_put(0);
    breserve(&data_region, &data_cap, data_len + g_xstack);
    memset(data_region + data_len, 0, (size_t)g_xstack);
    data_len += g_xstack;
    unsigned char szb[8];
    for (int i = 0; i < 8; i++) szb[i] = (unsigned char)((g_xstack >> (i * 8)) & 255);
    memcpy(data_region + 88, szb, 8);
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
    "    ret\n"
    ".globl read\n"
    "read:\n"
    "    movq $0, %rax\n"
    "    syscall\n"
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
    "__stdio_slots:\n"
    "    .zero 40\n"
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
    "    jl .Lstub_fopen_fail\n"
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
    "    movq (%rax,%r14,1), %rdi\n"
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
    "    movq $0, 40(%rsp)\n"
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
    name_copy(fixups[n_fixups].name, sym);
    n_fixups++;
    x32(0);
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
            } else if ((long long)(signed char)v == v) {
                x86_ea_rex(d, (g1 >> 3) & 7, rexw, 0);
                x8(0x83);
                x86_ea_modrm(d, (g1 >> 3) & 7);
                x8((int)(v & 255));
            } else {
                x86_ea_rex(d, (g1 >> 3) & 7, rexw, 0);
                x8(0x81);
                x86_ea_modrm(d, (g1 >> 3) & 7);
                x32(v);
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

static void elf_sal(const Op *s, const Op *d) {
    if (s->kind != K_REG || s->reg != REG_RCX) { die("shift count must be %cl"); return; }
    if (d->kind != K_REG) { die("invalid shift destination"); return; }
    emit_rex(1, 0, 0, (d->reg >> 3) & 1);
    x8(0xD3);
    emit_modrm(3, 4, d->reg & 7);
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
            } else if ((long long)(signed char)v == v) {
                x86_ea_rex(d, 7, rexw, 0);
                x8(0x83);
                x86_ea_modrm(d, 7);
                x8((int)(v & 255));
            } else {
                x86_ea_rex(d, 7, rexw, 0);
                x8(0x81);
                x86_ea_modrm(d, 7);
                x32(v);
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

static void elf_ins(const char *mn, const Op *o1, const Op *o2) {
    if (strcmp(mn, "movq") == 0) { elf_mov(8, o1, o2); return; }
    if (strcmp(mn, "movl") == 0) { elf_mov(4, o1, o2); return; }
    if (strcmp(mn, "movb") == 0) { elf_mov(1, o1, o2); return; }
    if (strcmp(mn, "movzbq") == 0) { elf_movzx(o1, o2, 0xB6, 1, 1); return; }
    if (strcmp(mn, "movsbq") == 0) { elf_movzx(o1, o2, 0xBE, 1, 1); return; }
    if (strcmp(mn, "movslq") == 0) { elf_movzx(o1, o2, 0x63, 1, 0); return; }
    if (strcmp(mn, "leaq") == 0) { elf_lea(o1, o2); return; }
    if (strcmp(mn, "pushq") == 0) { elf_push(o1); return; }
    if (strcmp(mn, "popq") == 0) { elf_pop(o1); return; }
    if (strcmp(mn, "addq") == 0) { elf_alu(X86_G1_ADD, 8, o1, o2); return; }
    if (strcmp(mn, "addb") == 0) { elf_alu(X86_G1_ADD, 1, o1, o2); return; }
    if (strcmp(mn, "subq") == 0) { elf_alu(X86_G1_SUB, 8, o1, o2); return; }
    if (strcmp(mn, "subl") == 0) { elf_alu(X86_G1_SUB, 4, o1, o2); return; }
    if (strcmp(mn, "andq") == 0) { elf_alu(X86_G1_AND, 8, o1, o2); return; }
    if (strcmp(mn, "orq") == 0) { elf_alu(X86_G1_OR, 8, o1, o2); return; }
    if (strcmp(mn, "xorl") == 0) { elf_alu(X86_G1_XOR, 4, o1, o2); return; }
    if (strcmp(mn, "cmpq") == 0) { elf_cmp(8, o1, o2); return; }
    if (strcmp(mn, "cmpl") == 0) { elf_cmp(4, o1, o2); return; }
    if (strcmp(mn, "cmpb") == 0) { elf_cmp(1, o1, o2); return; }
    if (strcmp(mn, "imulq") == 0) { elf_imul(o1, o2); return; }
    if (strcmp(mn, "negq") == 0) { elf_grp3(o1, 3); return; }
    if (strcmp(mn, "notq") == 0) { elf_grp3(o1, 2); return; }
    if (strcmp(mn, "idivq") == 0) { elf_grp3(o1, 7); return; }
    if (strcmp(mn, "salq") == 0) { elf_sal(o1, o2); return; }
    if (strcmp(mn, "testq") == 0) { elf_test(o1, o2); return; }
    if (strcmp(mn, "sete") == 0) { elf_set(X86_SET_E, o1); return; }
    if (strcmp(mn, "setne") == 0) { elf_set(X86_SET_NE, o1); return; }
    if (strcmp(mn, "setl") == 0) { elf_set(X86_SET_L, o1); return; }
    if (strcmp(mn, "setg") == 0) { elf_set(X86_SET_G, o1); return; }
    if (strcmp(mn, "setle") == 0) { elf_set(X86_SET_LE, o1); return; }
    if (strcmp(mn, "setge") == 0) { elf_set(X86_SET_GE, o1); return; }
    if (strcmp(mn, "je") == 0) { elf_branch(X86_JCC_JE, o1); return; }
    if (strcmp(mn, "jne") == 0) { elf_branch(X86_JCC_JNE, o1); return; }
    if (strcmp(mn, "jl") == 0) { elf_branch(X86_JCC_JL, o1); return; }
    if (strcmp(mn, "jg") == 0) { elf_branch(X86_JCC_JG, o1); return; }
    if (strcmp(mn, "jle") == 0) { elf_branch(X86_JCC_JLE, o1); return; }
    if (strcmp(mn, "jge") == 0) { elf_branch(X86_JCC_JGE, o1); return; }
    if (strcmp(mn, "ja") == 0) { elf_branch(X86_JCC_JA, o1); return; }
    if (strcmp(mn, "jae") == 0) { elf_branch(X86_JCC_JAE, o1); return; }
    if (strcmp(mn, "jb") == 0) { elf_branch(X86_JCC_JB, o1); return; }
    if (strcmp(mn, "jbe") == 0) { elf_branch(X86_JCC_JBE, o1); return; }
    if (strcmp(mn, "jmp") == 0) { elf_branch(0xE9, o1); return; }
    if (strcmp(mn, "call") == 0) { elf_branch(0xE8, o1); return; }
    if (strcmp(mn, "ret") == 0) { x8(0xC3); return; }
    if (strcmp(mn, "leave") == 0) { x8(0xC9); return; }
    if (strcmp(mn, "cqto") == 0) { x8(0x48); x8(0x99); return; }
    if (strcmp(mn, "syscall") == 0) { x8(0x0F); x8(0x05); return; }
    if (strcmp(mn, "nop") == 0) { x8(0x90); return; }
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

static void elf_resolve_fixups(void) {
    for (int i = 0; i < n_fixups; i++) {
        int si = find_sym(fixups[i].name);
        if (si < 0) {
            fprintf(stderr, "ld: %s: undefined symbol %s\n", cur_file, fixups[i].name);
            error_count++;
            continue;
        }
        long addr = elf_sym_addr(&syms[si]);
        long rel = addr - (elf_text_base + fixups[i].pos + 4);
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
                    skip_until_next_func = syms[si].shadowed;
                    if (!syms[si].shadowed) syms[si].off = code_len;
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

static void elf_layout(void) {
    elf_text_base = CFG_ELF_TEXT_BASE;
    elf_text_size = code_len;
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
}

static void elf_write(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "ld: cannot write %s\n", path);
        exit(1);
    }

    long shoff = x86_align_up(elf_data_base + elf_data_size, 8);

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
    w64_at(eh + 24, (unsigned long long)elf_sym_addr(&syms[entry_sym]));
    w64_at(eh + 32, CFG_ELF_HSIZE);
    w64_at(eh + 40, shoff);
    w16(eh + 52, CFG_ELF_HSIZE);
    w16(eh + 54, CFG_ELF_PHENTSZ);
    w16(eh + 56, CFG_ELF_PHNUM);
    w16(eh + 58, CFG_ELF_SHENTSZ);
    w16(eh + 60, CFG_ELF_SHNUM);
    w16(eh + 62, CFG_ELF_SHSTRNDX);
    fwrite(eh, 1, sizeof(eh), f);

    unsigned char ph[CFG_ELF_PHENTSZ * CFG_ELF_PHNUM];
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
    w64_at(ph + 96, (unsigned long long)(elf_data_size + elf_bss_size));
    w64_at(ph + 104, CFG_ELF_PAGE);
    fwrite(ph, 1, sizeof(ph), f);

    if (elf_text_base > (long)sizeof(eh) + (long)sizeof(ph)) {
        for (long i = (long)sizeof(eh) + (long)sizeof(ph); i < elf_text_base; i++)
            fputc(0, f);
    }
    fwrite(code, 1, (size_t)code_len, f);
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
    while (ftell(f) < shoff) fputc(0, f);

    static const char shstr[] = "\0.text\0.rodata\0.data\0.bss\0.shstrtab\0";
    unsigned char sh[CFG_ELF_SHENTSZ * CFG_ELF_SHNUM];
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
    w32(sh + 5 * CFG_ELF_SHENTSZ + 0, 26);
    w32(sh + 5 * CFG_ELF_SHENTSZ + 4, CFG_ELF_SHT_STRTAB);
    w64_at(sh + 5 * CFG_ELF_SHENTSZ + 24, (unsigned long long)(shoff + CFG_ELF_SHENTSZ * CFG_ELF_SHNUM));
    w64_at(sh + 5 * CFG_ELF_SHENTSZ + 32, sizeof(shstr) - 1);
    w64_at(sh + 5 * CFG_ELF_SHENTSZ + 48, 1);
    fwrite(sh, 1, sizeof(sh), f);
    fwrite(shstr, 1, sizeof(shstr) - 1, f);
    fclose(f);
}

static void elf_build(const char *in_path, const char *out_path) {
    LineSrc src;
    ls_open_file(&src, in_path);
    scan_src(&src, 0);
    ls_close(&src);

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

    int start_idx = find_sym("_start");
    if (start_idx < 0) start_idx = find_sym("main");
    if (start_idx < 0) {
        fprintf(stderr, "ld: no entry point (need _start or main)\n");
        exit(1);
    }
    entry_sym = start_idx;

    ls_open_file(&src, in_path);
    elf_encode_src(&src);
    ls_close(&src);

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

    if (error_count) {
        fprintf(stderr, "ld: %d error(s)\n", error_count);
        exit(1);
    }

    elf_layout();
    elf_resolve_fixups();
    if (error_count) {
        fprintf(stderr, "ld: %d error(s)\n", error_count);
        exit(1);
    }
    elf_write(out_path);
}

/* ================================================================
 *  CLI
 * ================================================================ */

static void usage(void) {
    fprintf(stderr,
        "usage: ld [-f cvm|elf] [-o out] [-xstack N] input.s\n"
        "  -f cvm     CVM v2 module (default)\n"
        "  -f elf     static PIE Linux ELF executable\n"
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
        elf_build(in, out);
    } else {
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
        return 0;
    }
    return 0;
}
