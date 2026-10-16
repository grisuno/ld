# Documentation Coverage

Overall: **1.9%** (4 of 207 public modules, types, and callables). Data members are excluded from the ratio.

[Back to index](index.md)

## By kind

| Kind | Documented | Total | Coverage |
|---|---|---|---|
| function | 0 | 2 | 0.0% |
| macro | 2 | 192 | 1.0% |
| module | 1 | 3 | 33.3% |
| struct | 1 | 10 | 10.0% |

## By module (lowest first)

| Module | Documented | Total | Coverage |
|---|---|---|---|
| [`install.sh`](modules/install.sh.md) | 0 | 1 | 0.0% |
| [`ld.c`](modules/ld.c.md) | 3 | 205 | 1.5% |
| [`app`](modules/app.py.md) | 1 | 1 | 100.0% |

## Warnings

DOC001 undocumented public item, DOC002 parameter not documented, DOC003 documented parameter missing from the signature (stale doc), DOC004 return value not documented, DOC005 raised exception not documented.

DOC001: 203

- DOC001 `install.sh:1` module install.sh is not documented
- DOC001 `ld.c:1` module ld.c is not documented
- DOC001 `ld.c:25` macro CFG_MAX_SYMBOLS is not documented
- DOC001 `ld.c:26` macro CFG_MAX_FIXUPS is not documented
- DOC001 `ld.c:27` macro CFG_FIXUP_INIT is not documented
- DOC001 `ld.c:28` macro CFG_LINE_MAX is not documented
- DOC001 `ld.c:29` macro CFG_NAME_MAX is not documented
- DOC001 `ld.c:30` macro CFG_MAX_NATS is not documented
- DOC001 `ld.c:31` macro CFG_MAX_ERRORS is not documented
- DOC001 `ld.c:32` macro CFG_GROW_UNIT is not documented
- DOC001 `ld.c:34` macro CFG_ABI_BYTES is not documented
- DOC001 `ld.c:35` macro CFG_STACK_BASE is not documented
- DOC001 `ld.c:36` macro CFG_XSTACK_DEF is not documented
- DOC001 `ld.c:42` macro CFG_REG_LOCALS is not documented
- DOC001 `ld.c:43` macro CFG_SLOT_FLAGS_A is not documented
- DOC001 `ld.c:44` macro CFG_SLOT_FLAGS_B is not documented
- DOC001 `ld.c:45` macro CFG_SLOT_S0 is not documented
- DOC001 `ld.c:46` macro CFG_SLOT_S1 is not documented
- DOC001 `ld.c:48` macro CFG_GSLOT_RSP is not documented
- DOC001 `ld.c:49` macro CFG_GSLOT_RBP is not documented
- DOC001 `ld.c:50` macro CFG_GSLOT_ARGS is not documented
- DOC001 `ld.c:51` macro CFG_GSLOT_RET is not documented
- DOC001 `ld.c:53` macro CFG_CVM_MAGIC_0 is not documented
- DOC001 `ld.c:54` macro CFG_CVM_MAGIC_1 is not documented
- DOC001 `ld.c:55` macro CFG_CVM_MAGIC_2 is not documented
- DOC001 `ld.c:56` macro CFG_CVM_MAGIC_3 is not documented
- DOC001 `ld.c:57` macro CFG_CVM_VER_MAJ is not documented
- DOC001 `ld.c:58` macro CFG_CVM_VER_MIN is not documented
- DOC001 `ld.c:59` macro CFG_CVM_HDR_SIZE is not documented
- DOC001 `ld.c:61` macro CFG_ELF_PAGE is not documented
- DOC001 `ld.c:62` macro CFG_ELF_HSIZE is not documented
- DOC001 `ld.c:63` macro CFG_ELF_PHENTSZ is not documented
- DOC001 `ld.c:64` macro CFG_ELF_PHNUM is not documented
- DOC001 `ld.c:65` macro CFG_ELF_SHENTSZ is not documented
- DOC001 `ld.c:66` macro CFG_ELF_SHNUM is not documented
- DOC001 `ld.c:67` macro CFG_ELF_SHSTRNDX is not documented
- DOC001 `ld.c:68` macro CFG_ELF_ET_DYN is not documented
- DOC001 `ld.c:69` macro CFG_ELF_EM_X8664 is not documented
- DOC001 `ld.c:70` macro CFG_ELF_PF_R is not documented
- DOC001 `ld.c:71` macro CFG_ELF_PF_W is not documented
- DOC001 `ld.c:72` macro CFG_ELF_PF_X is not documented
- DOC001 `ld.c:73` macro CFG_ELF_PT_LOAD is not documented
- DOC001 `ld.c:74` macro CFG_ELF_PT_DYNAMIC is not documented
- DOC001 `ld.c:75` macro CFG_ELF_PT_GNU_STACK is not documented
- DOC001 `ld.c:76` macro CFG_ELF_SHT_PROGBITS is not documented
- DOC001 `ld.c:77` macro CFG_ELF_SHT_NOBITS is not documented
- DOC001 `ld.c:78` macro CFG_ELF_SHT_STRTAB is not documented
- DOC001 `ld.c:79` macro CFG_ELF_SHT_HASH is not documented
- DOC001 `ld.c:80` macro CFG_ELF_SHT_DYNAMIC is not documented
- DOC001 `ld.c:81` macro CFG_ELF_SHT_DYNSYM is not documented
- DOC001 `ld.c:82` macro CFG_ELF_SHT_RELA is not documented
- DOC001 `ld.c:83` macro CFG_ELF_DT_NULL is not documented
- DOC001 `ld.c:84` macro CFG_ELF_DT_NEEDED is not documented
- DOC001 `ld.c:85` macro CFG_ELF_DT_RELA is not documented
- DOC001 `ld.c:86` macro CFG_ELF_DT_RELASZ is not documented
- DOC001 `ld.c:87` macro CFG_ELF_DT_RELAENT is not documented
- DOC001 `ld.c:88` macro CFG_ELF_DT_HASH is not documented
- DOC001 `ld.c:89` macro CFG_ELF_DT_STRTAB is not documented
- DOC001 `ld.c:90` macro CFG_ELF_DT_SYMTAB is not documented
- DOC001 `ld.c:91` macro CFG_ELF_DT_STRSZ is not documented
- DOC001 `ld.c:92` macro CFG_ELF_DT_SYMENT is not documented
- DOC001 `ld.c:93` macro CFG_ELF_DT_SONAME is not documented
- DOC001 `ld.c:94` macro CFG_ELF_STB_GLOBAL is not documented
- DOC001 `ld.c:95` macro CFG_ELF_STT_NOTYPE is not documented
- DOC001 `ld.c:96` macro CFG_ELF_STT_FUNC is not documented
- DOC001 `ld.c:97` macro CFG_ELF_R_GLOB_DAT is not documented
- DOC001 `ld.c:98` macro CFG_ELF_R_RELATIVE is not documented
- DOC001 `ld.c:99` macro CFG_ELF_SHN_UNDEF is not documented
- DOC001 `ld.c:100` macro CFG_LD_MAXLIBS is not documented
- DOC001 `ld.c:101` macro CFG_LD_MAXIMPORTS is not documented
- DOC001 `ld.c:102` macro CFG_LD_LIB_MAXSZ is not documented
- DOC001 `ld.c:103` macro CFG_ELF_DYN_NENTRY is not documented
- DOC001 `ld.c:104` macro CFG_ELF_DYNSYM_ENT is not documented
- DOC001 `ld.c:105` macro CFG_ELF_SHF_A is not documented
- DOC001 `ld.c:106` macro CFG_ELF_SHF_X is not documented
- DOC001 `ld.c:107` macro CFG_ELF_SHF_W is not documented
- DOC001 `ld.c:108` macro CFG_ELF_TEXT_BASE is not documented
- DOC001 `ld.c:110` macro CFG_FMT_CVM is not documented
- DOC001 `ld.c:111` macro CFG_FMT_ELF is not documented
- DOC001 `ld.c:115` macro X86_G1_OR is not documented
- DOC001 `ld.c:116` macro X86_G1_AND is not documented
- DOC001 `ld.c:117` macro X86_G1_SUB is not documented
- DOC001 `ld.c:118` macro X86_G1_XOR is not documented
- DOC001 `ld.c:119` macro X86_G1_CMP is not documented
- DOC001 `ld.c:121` macro X86_JCC_JE is not documented
- DOC001 `ld.c:122` macro X86_JCC_JNE is not documented
- DOC001 `ld.c:123` macro X86_JCC_JL is not documented
- DOC001 `ld.c:124` macro X86_JCC_JG is not documented
- DOC001 `ld.c:125` macro X86_JCC_JLE is not documented
- DOC001 `ld.c:126` macro X86_JCC_JGE is not documented
- DOC001 `ld.c:127` macro X86_JCC_JA is not documented
- DOC001 `ld.c:128` macro X86_JCC_JAE is not documented
- DOC001 `ld.c:129` macro X86_JCC_JB is not documented
- DOC001 `ld.c:130` macro X86_JCC_JBE is not documented
- DOC001 `ld.c:132` macro X86_SET_E is not documented
- DOC001 `ld.c:133` macro X86_SET_NE is not documented
- DOC001 `ld.c:134` macro X86_SET_L is not documented
- DOC001 `ld.c:135` macro X86_SET_G is not documented
- DOC001 `ld.c:136` macro X86_SET_LE is not documented
- DOC001 `ld.c:137` macro X86_SET_GE is not documented
- DOC001 `ld.c:138` macro X86_SET_A is not documented
- DOC001 `ld.c:139` macro X86_SET_AE is not documented
- DOC001 `ld.c:140` macro X86_SET_B is not documented
- DOC001 `ld.c:141` macro X86_SET_BE is not documented
- DOC001 `ld.c:143` macro X86_SYS_WRITE is not documented
- DOC001 `ld.c:144` macro X86_SYS_READ is not documented
- DOC001 `ld.c:145` macro X86_SYS_OPEN is not documented
- DOC001 `ld.c:146` macro X86_SYS_CLOSE is not documented
- DOC001 `ld.c:147` macro X86_SYS_LSEEK is not documented
- DOC001 `ld.c:148` macro X86_SYS_BRK is not documented
- DOC001 `ld.c:149` macro X86_SYS_EXIT is not documented
- DOC001 `ld.c:150` macro X86_SYS_EXIT_GROUP is not documented
- DOC001 `ld.c:152` macro REG_RAX is not documented
- DOC001 `ld.c:153` macro REG_RCX is not documented
- DOC001 `ld.c:154` macro REG_RDX is not documented
- DOC001 `ld.c:155` macro REG_RBX is not documented
- DOC001 `ld.c:156` macro REG_RSP is not documented
- DOC001 `ld.c:157` macro REG_RBP is not documented
- DOC001 `ld.c:158` macro REG_RSI is not documented
- DOC001 `ld.c:159` macro REG_RDI is not documented
- DOC001 `ld.c:161` macro SEC_TEXT is not documented
- DOC001 `ld.c:162` macro SEC_BSS is not documented
- DOC001 `ld.c:163` macro SEC_DATA is not documented
- DOC001 `ld.c:164` macro SEC_RODATA is not documented
- DOC001 `ld.c:166` macro SYM_FUNC is not documented
- DOC001 `ld.c:167` macro SYM_LABEL is not documented
- DOC001 `ld.c:168` macro SYM_GLOBAL is not documented
- DOC001 `ld.c:169` macro SYM_BLOB is not documented
- DOC001 `ld.c:171` macro K_REG is not documented
- DOC001 `ld.c:172` macro K_IMM is not documented
- DOC001 `ld.c:173` macro K_MEM is not documented
- DOC001 `ld.c:174` macro K_SYM is not documented
- DOC001 `ld.c:175` macro K_SYM_IMM is not documented
- DOC001 `ld.c:176` macro K_IND is not documented
- DOC001 `ld.c:178` macro OP_NOP is not documented
- DOC001 `ld.c:179` macro OP_PUSH_IMM64 is not documented
- DOC001 `ld.c:180` macro OP_PUSH_IMM32 is not documented
- DOC001 `ld.c:181` macro OP_PUSH_IMM8 is not documented
- DOC001 `ld.c:182` macro OP_PUSH_ZERO is not documented
- DOC001 `ld.c:183` macro OP_PUSH_ONE is not documented
- DOC001 `ld.c:184` macro OP_PUSH_LOCAL is not documented
- DOC001 `ld.c:185` macro OP_STORE_LOCAL is not documented
- DOC001 `ld.c:186` macro OP_PUSH_GLOBAL is not documented
- DOC001 `ld.c:187` macro OP_STORE_GLOBAL is not documented
- DOC001 `ld.c:188` macro OP_ADD is not documented
- DOC001 `ld.c:189` macro OP_SUB is not documented
- DOC001 `ld.c:190` macro OP_MUL is not documented
- DOC001 `ld.c:191` macro OP_DIV is not documented
- DOC001 `ld.c:192` macro OP_MOD is not documented
- DOC001 `ld.c:193` macro OP_NEG is not documented
- DOC001 `ld.c:194` macro OP_UDIV is not documented
- DOC001 `ld.c:195` macro OP_UMOD is not documented
- DOC001 `ld.c:196` macro OP_AND is not documented
- DOC001 `ld.c:197` macro OP_OR is not documented
- DOC001 `ld.c:198` macro OP_XOR is not documented
- DOC001 `ld.c:199` macro OP_NOT is not documented
- DOC001 `ld.c:200` macro OP_SHL is not documented
- DOC001 `ld.c:201` macro OP_SHR is not documented
- DOC001 `ld.c:202` macro OP_USHR is not documented
- DOC001 `ld.c:203` macro OP_CMP_EQ is not documented
- DOC001 `ld.c:204` macro OP_CMP_NE is not documented
- DOC001 `ld.c:205` macro OP_CMP_LT is not documented
- DOC001 `ld.c:206` macro OP_CMP_LE is not documented
- DOC001 `ld.c:207` macro OP_CMP_GT is not documented
- DOC001 `ld.c:208` macro OP_CMP_GE is not documented
- DOC001 `ld.c:209` macro OP_LNOT is not documented
- DOC001 `ld.c:210` macro OP_CMP_ULT is not documented
- DOC001 `ld.c:211` macro OP_CMP_ULE is not documented
- DOC001 `ld.c:212` macro OP_CMP_UGT is not documented
- DOC001 `ld.c:213` macro OP_CMP_UGE is not documented
- DOC001 `ld.c:214` macro OP_JMP is not documented
- DOC001 `ld.c:215` macro OP_JZ is not documented
- DOC001 `ld.c:216` macro OP_JNZ is not documented
- DOC001 `ld.c:217` macro OP_CALL is not documented
- DOC001 `ld.c:218` macro OP_RET is not documented
- DOC001 `ld.c:219` macro OP_CALL_NATIVE is not documented
- DOC001 `ld.c:220` macro OP_CALL_INDIRECT is not documented
- DOC001 `ld.c:221` macro OP_LOAD8 is not documented
- DOC001 `ld.c:222` macro OP_LOAD16 is not documented
- DOC001 `ld.c:223` macro OP_LOAD32 is not documented
- DOC001 `ld.c:224` macro OP_LOAD64 is not documented
- DOC001 `ld.c:225` macro OP_STORE8 is not documented
- DOC001 `ld.c:226` macro OP_STORE16 is not documented
- DOC001 `ld.c:227` macro OP_STORE32 is not documented
- DOC001 `ld.c:228` macro OP_STORE64 is not documented
- DOC001 `ld.c:229` macro OP_LEA_LOCAL is not documented
- DOC001 `ld.c:230` macro OP_LEA_GLOBAL is not documented
- DOC001 `ld.c:231` macro OP_ALLOC is not documented
- DOC001 `ld.c:232` macro OP_FREE is not documented
- DOC001 `ld.c:233` macro OP_LEA_DATA is not documented
- DOC001 `ld.c:234` macro OP_SYSCALL is not documented
- DOC001 `ld.c:235` macro OP_HALT is not documented
- DOC001 `ld.c:241` struct Sym is not documented
- DOC001 `ld.c:256` struct Op is not documented
- DOC001 `ld.c:271` struct Fixup is not documented
- DOC001 `ld.c:277` struct LineSrc is not documented
- DOC001 `ld.c:285` struct Func is not documented
- DOC001 `ld.c:292` struct GVar is not documented
- DOC001 `ld.c:299` struct Blob is not documented
- DOC001 `ld.c:306` struct Nat is not documented
- DOC001 `ld.c:310` struct Label is not documented
- DOC001 `ld.c:4469` function image is not documented
- DOC001 `ld.c:4985` function main is not documented
