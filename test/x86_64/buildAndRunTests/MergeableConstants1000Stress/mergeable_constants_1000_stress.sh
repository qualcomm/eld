#REQUIRES: linux, x86
#!/usr/bin/env bash
set -euo pipefail

# Debug notes:
# - All generated sources/objects/binary are under %t.
# - To inspect failures, rerun with:
#     llvm-lit -a -v <path>/mergeable_constants_1000_stress.sh
# - Runtime exits non-zero if unique address count != EXPECTED_UNIQUE.
# - Printed output is checked against mergeable_constants_1000_stress.golden.
BUILD_DIR=%t
%mkdir -p "$BUILD_DIR"

N_FILES=10
PER_FILE=100
UNIQUE_POOL=250
TOTAL=$((N_FILES * PER_FILE))

for ((f = 0; f < N_FILES; ++f)); do
  src="$BUILD_DIR/constants_${f}.c"
  {
    for ((i = 0; i < PER_FILE; ++i)); do
      id=$((f * PER_FILE + i))
      v=$((id % UNIQUE_POOL))
      printf 'const char *get_const_%d_%d(void) { return "MERGE_CONST_%03d"; }\n' "$f" "$i" "$v"
    done
  } > "$src"
done

main_src="$BUILD_DIR/main.c"
{
  cat <<'HDR'
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

sed -i "1i #define EXPECTED_UNIQUE ${UNIQUE_POOL}" "$main_src"

# Compile each generated TU separately to stress cross-file constant merging.
objs=()
for ((f = 0; f < N_FILES; ++f)); do
  src="$BUILD_DIR/constants_${f}.c"
  obj="$BUILD_DIR/constants_${f}.o"
  %clang %clangopts -O2 -c "$src" -o "$obj"
  objs+=("$obj")
done

# Link through ld.eld and run the runtime verifier.
%clang %clangopts -O2 "$main_src" "${objs[@]}" -o "$BUILD_DIR/mergeable_constants_stress" --ld-path=ld.eld
%run "$BUILD_DIR/mergeable_constants_stress" | tee "$BUILD_DIR/mergeable_constants_1000_stress.out"
%filecheck %p/mergeable_constants_1000_stress.golden < "$BUILD_DIR/mergeable_constants_1000_stress.out"

echo "PASS: mergeable_constants_1000_stress"
#CHECK: PASS: mergeable_constants_1000_stress
