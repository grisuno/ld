# Second Brain

*Last synthesized: 2026-09-17 | 26 files | 1 concept pages | offline, zero tokens*

> Raw sources -> readmenator wiki -> links (Karpathy LLM Wiki Pattern, deterministic).
> Start here, then open one community page. Prefer grep over full reads.

## Vault Overview

The codebase centres on `ld.c`, `fnptr.c`, `stdint.c`. Architecturally it is 3 layers, dominant testing (22 files) across 1 import-based communities. Recorded risk surface: 8 security findings and 0 dependency cycles.

Communities are self-contained in the resolved import graph; no cross-boundary bridges were recorded.

Open work clusters around documentation (15% file coverage), 8 security findings, 0 taint paths, and 5 suggested exploration questions in `queries.md`.

## Stats

| Metric | Value |
|--------|-------|
| Files | 26 |
| Symbols | 373 |
| Resolved imports | 0 |
| Languages | c, py, s, sh |
| Communities | 1 |
| Doc coverage | 15% (4/26 files) |
| Security findings | 8 |
| Estimated read cost | ~2069 tokens (chars/4, offline so $0) |

## Reading Order

1. Skim Stats and God Nodes below for blast radius.
2. Open the largest community page first, then follow Connections.
3. Use `queries.md` for the next question; log the answer there.

```
grep -rn '<keyword>' index.md community_*.md
readmenator query "<question>" --target ld
```

## Concept Wiki

- [root (26 files, cohesion 1.00)](./community_0_root.md)

## God Nodes

| File | Score |
|------|-------|
| `ld.c` | 30.7 |
| `test/fnptr.c` | 0.9 |
| `test/stdint.c` | 0.7 |
| `test/run_tests.sh` | 0.6 |
| `test/asm.c` | 0.4 |

## Strongest Connections

- No cross-community connections recorded.

## Navigation Tips

- Obsidian Graph View works: every community page links back here.
- `connections.json` is machine-readable for GraphRAG pipelines.
- `REPORT.md` states what was extracted vs inferred and current limits.
- Regenerate offline: `readmenator . --rebuild` (no network, no tokens).
