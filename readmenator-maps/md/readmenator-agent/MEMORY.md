# Project Memory

> Cross-session context for agents. Sections 1-6 are regenerated from the source tree with zero LLM tokens: declared rules are quoted verbatim with `file:line`, measured baselines come from the scan. Section 7 is written by agents and humans and is preserved across rebuilds.

Generated from 31 files at commit `60b2235f964d`. Read this first, then `readmenator-wiki/index.md`, then `readmenator . ask "<question>"` for anything specific.

## 1. Purpose and domain

- What it is: The test suite drives miniGCC and cvm2, so clone them as siblings of this (`README.md:16`)
- Domain vocabulary (term, files): `start` (11), `printf` (6), `run` (5), `fib` (4), `name` (3), `shared` (3), `write` (3), `host` (3), `chain` (3), `dynlib` (3), `fmt` (3), `against` (3), `file` (3), `program` (3), `dlopen` (3)
- Business rules that the code cannot show live in section 7: record them there.

## 2. Workflow

Detected commands:
- `python -m pytest -q` (tests/ layout)
- `make all` (Makefile)
- `make ld` (Makefile)
- `make test` (Makefile)
- `make clean` (Makefile)
- `CI workflow github-actions-demo.yml` (workflows/github-actions-demo.yml)

Session protocol:
1. Start: read this file, then `readmenator . fresh` (exit 1 means run `readmenator . --rebuild`).
2. Orient: `readmenator-wiki/index.md`; for a question use `readmenator . ask "<question>"` (local = entities + sources, `--global` = community reports).
3. Before editing a file: `grep -n '<file>' readmenator-agent/GOTCHAS.md readmenator-agent/SECURITY.md`.
4. After the change: run the tests above, then `readmenator . --rebuild` so the maps, wiki, and this file stay true.
5. End: record decisions, business rules, and gotchas with `readmenator . remember "<note>" --kind decision`.

## 3. Rules and constraints

Declared:
- All parse buffers bounded (`LINE_MAX`, `NAME_MAX`). (`CLAUDE.md:191`)
- No format string vulnerabilities; `%s` with user data only via `fputs`. (`CLAUDE.md:192`)
- Symbol and fixup tables bounds checked; duplicates rejected. (`CLAUDE.md:193`)
- Integer literals parsed with overflow detection. (`CLAUDE.md:194`)
- ELF writer never emits segments outside the intended file. (`CLAUDE.md:195`)

Measured baseline:
- Security findings at medium or above: 0 (see `readmenator-agent/SECURITY.md`); do not add new ones.
- Dependency cycles: 0; layer violations: 0 (see `readmenator-agent/GOTCHAS.md`).

## 4. Style norms

Declared:
- none declared in instruction files (add them to AGENTS.md or record them in section 7)

Measured baseline:
- c: 17 files, 389 symbols; docstrings on 5% of symbols; functions snake_case (100%); types PascalCase (83%); median file 16 lines, max 5110.
- s: 10 files, 23 symbols; docstrings on 0% of symbols; functions snake_case (100%); median file 46 lines, max 107.
- sh: 3 files, 11 symbols; docstrings on 82% of symbols; functions snake_case (100%); median file 115 lines, max 527.
- py: 1 files, 0 symbols; median file 12 lines, max 12.
- Tests: 28 files under test; follow the existing naming (e.g. `run_tests.sh`).

## 5. Minimum deliverables

Declared:
- none declared in instruction files (add them to AGENTS.md or record them in section 7)

Measured baseline:
- Tests pass: `python -m pytest -q`.
- Docstring coverage stays at or above 7%.
- No new security findings at medium or above (current: 0).
- No new dependency cycles (current: 0).
- Files stay under 300 lines where possible (`readmenator . lint`).
- Docs refreshed: `readmenator . --rebuild`, and decisions recorded in section 7.

## 6. Risks to respect

- God nodes (changes ripple widely): `ld.c`, `test/run_tests.sh`, `test/fnptr.c`, `test/stdint.c`, `test/dynuse.c`
- Hotspots (complex and central): `ld.c`, `test/stdint.c`, `test/dynlib_host.c`, `test/run_tests.sh`, `test/fnptr.c`
- Full blast radius: `readmenator-agent/GOTCHAS.md`; findings: `readmenator-agent/SECURITY.md`.

## 7. Session log (preserved across rebuilds)

Append with `readmenator . remember "<note>" --kind <kind>` (kinds: business, decision, rule, workflow, style, deliverable, gotcha, todo, note) or the MCP tool `readmenator.remember`. Record business rules, decisions and their reasons, workflow changes, and anything the next session must not rediscover.

<!-- readmenator:memory:notes:begin -->
<!-- readmenator:memory:notes:end -->
