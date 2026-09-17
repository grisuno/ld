# Security Findings

## CRITICAL (1)

- `test/mutate.sh:69` -- Command injection via eval — can execute arbitrary commands [CWE-78]
  Fix: Avoid shell=True and string-built commands; use argument arrays and input allowlists.

## HIGH (4)

- `ld.c:1859` (in `sprintf`) -- Buffer overflow risk: sprintf — use snprintf instead [CWE-121]
  Fix: Use bounded functions (strncpy, snprintf) with explicit sizes and NUL termination.
- `ld.c:1860` (in `sprintf`) -- Buffer overflow risk: sprintf — use snprintf instead [CWE-121]
  Fix: Use bounded functions (strncpy, snprintf) with explicit sizes and NUL termination.
- `ld.c:1861` (in `sprintf`) -- Buffer overflow risk: sprintf — use snprintf instead [CWE-121]
  Fix: Use bounded functions (strncpy, snprintf) with explicit sizes and NUL termination.
- `ld.c:1862` (in `sprintf`) -- Buffer overflow risk: sprintf — use snprintf instead [CWE-121]
  Fix: Use bounded functions (strncpy, snprintf) with explicit sizes and NUL termination.

## MEDIUM (3)

- `test/mutate.sh:16` -- Command substitution with user input — potential injection [CWE-78]
  Fix: Avoid shell=True and string-built commands; use argument arrays and input allowlists.
- `test/mutate.sh:61` -- Command substitution with user input — potential injection [CWE-78]
  Fix: Avoid shell=True and string-built commands; use argument arrays and input allowlists.
- `test/run_tests.sh:22` -- Command substitution with user input — potential injection [CWE-78]
  Fix: Avoid shell=True and string-built commands; use argument arrays and input allowlists.
