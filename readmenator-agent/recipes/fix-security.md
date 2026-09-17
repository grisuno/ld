# Recipe: Fix a Security Finding

- `test/mutate.sh:69` [critical] S001: Command injection via eval — can execute arbitrary commands
  Fix: Avoid shell=True and string-built commands; use argument arrays and input allowlists.
- `ld.c:1859` [high] C004: Buffer overflow risk: sprintf — use snprintf instead
  Fix: Use bounded functions (strncpy, snprintf) with explicit sizes and NUL termination.
- `ld.c:1860` [high] C004: Buffer overflow risk: sprintf — use snprintf instead
  Fix: Use bounded functions (strncpy, snprintf) with explicit sizes and NUL termination.

Verify: `readmenator . --audit && grep -c 'CRITICAL\|HIGH' readmenator-agent/SECURITY.md`
