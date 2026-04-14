#!/usr/bin/env bash
# Generates N_FILES source TUs that each define PER_FILE string-literal
# getters, cycling through UNIQUE_POOL distinct string values, plus a
# main.c driver that calls every getter and verifies the number of
# distinct string addresses returned equals UNIQUE_POOL.
set -euo pipefail

OUT_DIR="$1"
N_FILES=10
PER_FILE=100
UNIQUE_POOL=250

for ((f = 0; f < N_FILES; ++f)); do
  src="$OUT_DIR/constants_${f}.c"
  {
    for ((i = 0; i < PER_FILE; ++i)); do
      id=$((f * PER_FILE + i))
      v=$((id % UNIQUE_POOL))
      printf 'const char *get_const_%d_%d(void) { return "MERGE_CONST_%03d"; }\n' "$f" "$i" "$v"
    done
  } > "$src"
done

main_src="$OUT_DIR/main.c"
{
  cat <<HDR
#define EXPECTED_UNIQUE ${UNIQUE_POOL}
#include <stdint.h>
#include <stdio.h>

typedef const char *(*getter_t)(void);
HDR

  for ((f = 0; f < N_FILES; ++f)); do
    for ((i = 0; i < PER_FILE; ++i)); do
      printf 'const char *get_const_%d_%d(void);\n' "$f" "$i"
    done
  done

  cat <<'MID'

int main(void) {
  static getter_t getters[] = {
MID

  for ((f = 0; f < N_FILES; ++f)); do
    for ((i = 0; i < PER_FILE; ++i)); do
      printf '    get_const_%d_%d,\n' "$f" "$i"
    done
  done

  cat <<'TAIL'
  };

  const size_t total = sizeof(getters) / sizeof(getters[0]);
  uintptr_t addrs[10000];
  size_t unique = 0;

  for (size_t i = 0; i < total; ++i) {
    const char *s = getters[i]();
    uintptr_t p = (uintptr_t)s;
    addrs[i] = p;

    int seen = 0;
    for (size_t j = 0; j < i; ++j) {
      if (addrs[j] == p) {
        seen = 1;
        break;
      }
    }
    if (!seen)
      ++unique;

    printf("%04zu ", i);
    fputs(s, stdout);
    printf(" 0x%lx\n", (unsigned long)p);
  }

  printf("SUMMARY total=%zu unique_addresses=%zu expected_unique=%d\n",
         total, unique, EXPECTED_UNIQUE);

  if (unique != EXPECTED_UNIQUE)
    return 1;

  puts("PASS: mergeable constants combined across multiple files");
  return 0;
}
TAIL
} > "$main_src"
