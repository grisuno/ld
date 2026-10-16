# GraphRAG Community Reports

Entities: 508 | Relationships: 561 | Communities: 1 | Themes: 0 | Text units: 427

Query with `readmenator . ask "<question>"` (local: BM25 + Personalized PageRank; global: map-reduce over these reports) or the MCP tool `readmenator.graphrag`.

## Project overview (`root`, root, rating 7.0)

31 files in 1 communities. Highest-impact communities: unassigned files (7.0). God nodes: ld.c, test/run_tests.sh, test/fnptr.c, test/stdint.c, test/dynuse.c.

- [unassigned files] rating 7.0: `app.py` ranks 1 by PageRank, 0 importers, 0 symbols: Autor: Gris Iscomeback Correo electrónico: grisiscomeback[at]gmail[dot]com Fecha de creación: xx/xx/xxxx Licencia: GPL v3  Descripción:
- Key entities: file:ld.c, file:test/run_tests.sh
- Children: c0
- Root rating = highest community rating.

## unassigned files (`c0`, community, rating 7.0)

31 files under test (c 17, s 10, sh 3, py 1), mostly testing. Core file app.py (PageRank 0.0323, imported by 0 files): Autor: Gris Iscomeback Correo electrónico: grisiscomeback[at]gmail[dot]com Fecha de creación: xx/xx/xxxx Licencia: GPL v3  Descripción: Key abstractions: Sym, Op, Fixup, LineSrc, Func, GVar.

- `app.py` ranks 1 by PageRank, 0 importers, 0 symbols: Autor: Gris Iscomeback Correo electrónico: grisiscomeback[at]gmail[dot]com Fecha de creación: xx/xx/xxxx Licencia: GPL v3  Descripción:
- `install.sh` ranks 2 by PageRank, 0 importers, 0 symbols.
- `ld.c` ranks 3 by PageRank, 0 importers, 349 symbols: LdImport: d libraries (L2): verified import names plus the SONAMEs the executable records as DT_NEEDED.
- Hotspot `ld.c`: 349 symbols, 3 connections (score 1.00).
- Hotspot `test/stdint.c`: 6 symbols, 2 connections (score 0.41).
- Key entities: file:ld.c, file:test/run_tests.sh, file:test/fnptr.c, file:test/stdint.c, file:test/dynuse.c, file:test/fib.s, file:test/fib2.s, file:test/fib3.s
- Rating 7.0/10 = 7 x PageRank share 1.00 + 3 x risk 0.00. Internal imports: 0.
