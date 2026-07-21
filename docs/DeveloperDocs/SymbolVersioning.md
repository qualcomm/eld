# Symbol versioning

All symbol versioning related functionality is guarded by the `ELD_ENABLE_SYMBOL_VERSIONING`
build macro.

## Symbol resolution of versioned symbols

A versioned symbol is of two types:

- **default** versioned symbol (`bar@@V1`): a default versioned symbol
  satisfies satisfies undefined references of both the versioned and
  unversioned references of the symbol. `bar@@V1` definition can satisfy
  undefined references of both `bar@V1` and plain `bar`.
- **non-default** versioned symbol (`bar@V1`): a non-default versioned
  symbol only satisfies undefined references of the versioned references
  of the symbol. `bar@V1` definition can only satisfy undefined references
  of `bar@V1`.

eld supports symbol resolution of versioned symbols by inserting two
symbols for default-versioned symbols into the already-existing symbol resolution
machinery and adding a symbol normalization phase that runs after symbol resolution
to combine the two symbol nodes for the default-versioned symbols into one, if required.

Whenever eld sees a non-default-versioned symbol defintion `bar@V1`, then eld
inserts `bar@V1` to the symbol resolution machinery, and it automatically
gets used to resolve `bar@V1` undefined references.

Here is the interesting part, whenever eld sees a default-versioned symbol
definition `bar@@V1`, then eld inserts two symbols to the symbol resolution
machinery: a canonical symbol `bar@V1` and a non-canonical symbol `bar`.
With this, `bar@@V1` definition resolves undefined references to both
`bar` and `bar@V1`, as it should.

One fundamental rule of versioned-symbol resolution is that, for a default
versioned symbol, both the canonical symbol (`foo@V1`) and the non-canonical
(unversioned symbol `foo`) must resolve to the same definition. Allowing them
to resolve to different symbols is incorrect because, the unversioned
symbol is simply an alias for the versioned symbol (foo@V1), and both originally
referred to the same symbol (`foo@@V1`).

### Why is symbol normalization required?

As we saw above, for a default-versioned symbol definition `bar@@V1`,
eld creates two symbols `bar@V1` and `bar`. Some undefined symbol references
may have resolved to `bar@V1` definition and some may have resolved to `bar`
definition. This is a problem because in reality there is only one symbol
`bar@@V1`. If we keep two symbol nodes per default-versioned symbol definition,
then we will need to also emit both these symbols into the symbol table and
create duplicate GOT/PLT slots for these symbols.

Symbol normalization resolves this duplicate symbol issue by rewriting
all the references to non-canonical symbols (`bar`) into the canonical symbols
(`bar@V1`). A key thing to note here is that we only require the non-canonical
alias during symbol resolution phase.

LLD does not always normalize the symbols and creates duplicate GOT/PLT slots for the same
symbol when a default versioned symbol is accessed as both plain unversioned reference
and versioned reference.

Reproducer:

```bash
#!/usr/bin/env bash

cat >1.c <<\EOF
__asm__(".symver foo1,foo@@V1");
int foo1() {
  return 1;
}
EOF

cat >vs.t <<\EOF
V1 {
  global:
    foo;
};
EOF

cat >main.c <<\EOF
#include <stdio.h>

int foo();

__asm__(".symver foov1, foo@V1");
int foov1();

int main() {
  int u = foo() + foov1();
  printf("u: %d\n",u);
  return 0;
}
EOF

clang-20 -o 1.o 1.c -c -fPIC
clang-20 -o main.o main.c -c -fPIC

LDs=(ld.eld ld.lld ld.bfd)
SFs=(eld lld bfd)

for i in "${!SFs[@]}"; do
  ${LDs[$i]} -o lib1.${SFs[$i]}.so 1.o -shared --version-script vs.t
  clang-20 -o main.${SFs[$i]}.out main.o lib1.${SFs[$i]}.so --ld-path=$(which ${LDs[$i]})
  echo "${SFs[$i]}:"
  llvm-readelf -r main.${SFs[$i]}.out | grep foo
  echo ""
done
```

Output:

```bash
eld:
0000000000002230  0000000700000007 R_X86_64_JUMP_SLOT     0000000000000000 foo@V1 + 0

lld:
0000000000003a58  0000000600000007 R_X86_64_JUMP_SLOT     0000000000000000 foo@V1 + 0
0000000000003a68  0000000800000007 R_X86_64_JUMP_SLOT     0000000000000000 foo@V1 + 0

bfd:
0000000000004008  0000000500000007 R_X86_64_JUMP_SLOT     0000000000000000 foo@V1 + 0
```

### Versioned symbols symbol-resolution examples

Symbol resolution of versioned-symbols is largely the same as
non-versioned symbols. However, there are (corner) cases that
may seem surprising. Here we will see some examples of symbol resolution
of versioned symbols. We will also note differences from GNU ld (2.46.50)
and lld (22.0) wherever relevant.

In these examples (A) and (B) are simply used to differentiate symbols and
to avoid any confusion when referring to them.

The order matters in these examples, (A) `bar@@V1`, (B) `bar@V2`, means
that the linker first sees `bar@@V1` and then sees `bar@V2`.

The symbols originates from regular object files unstated otherwise.

1) (A) `bar@@V1`, (B) `bar@V1`

mutliple definition error.

2) (A) `bar@@V1`, (B) weak `bar@V1`

(A) `bar@@V1` is used to resolve undefined references to `bar@V1` and `bar`.

3) (A) weak `bar@@V1`, (B) `bar@V1`

(B) `bar@V1` is used to resolve undefined references to `bar@V1` and `bar`.

(B) `bar@V1` becomes a default-versioned symbol, and appears as `bar@@V1` in
the dynamic symbol table.

4) (A) weak `bar@@V1`, (B) `bar`

(B) `bar` is used to resolve undefined references to `bar@V1` and `bar`.

(B) `bar` becomes a default-versioned symbol, and appears as `bar@@V1` in the
dynamic symbol table.

5) (A) weak `bar@@V1`, (B) `bar@@V1`

(B) `bar@@V1` is used to resolve undefined references to `bar@V1` and `bar`.

6) (A) `bar@@V1`, (B) `bar@@V2`

eld and bfd errors out with multiple definition error.

lld incorrectly links correctly. Reproducer:

```bash
#!/usr/bin/env bash

cat >1.c <<\EOF
__asm__(".symver foo1,foo@@V1");
int foo1() {
  return 1;
}

__asm__(".symver foo2,foo@@V2");
int foo2() {
  return 3;
}

__asm__(".symver bar,bar@V2");
int bar() {
  return 5;
}
EOF

cat >main.c <<\EOF
#include <stdio.h>

int foo();
int bar();

int main() {
  int u = foo() + bar();
  printf("u: %d\n",u);
  return 0;
}
EOF

cat >vs.t <<\EOF
V1 {
  global:
    foo;
};

V2 {
  global:
    foo;
};
EOF

clang-20 -o 1.o 1.c -c -fPIC
clang-20 -o main.o main.c -c

LDs=(ld.eld ld.lld ld.bfd)
SFs=(eld lld bfd)

for i in "${!SFs[@]}"; do
  clang-20 -o lib.${SFs[$i]}.so 1.o --ld-path=$(which ${LDs[$i]}) -shared -fPIC -Wl,--version-script,vs.t
done
```

7) (A) weak `bar@@V1`, (B) `bar`, (C) `bar@V1`

multiple definition error.

8) (A) weak `bar@@V1`, (B) weak `bar`, (C) `bar@V1`

(C) `bar@V1` is used to resolve undefined references to both `bar` and `bar@V1`.

9) (A) weak `bar@@V1`, (B) `bar`, and version script that assigns
   `V2` to `bar`.

(B) `bar` becomes a default versioned symbol and appears as `bar@@V1`
in the dynamic symbol table. **There will be no `bar@@V2` in the symbol table!**

reproducer:

```bash
#!/usr/bin/env bash

cat >1.c <<\EOF
__asm__(".symver bar1, bar@@V1");
__attribute__((weak))
int bar1() {
  return 1;
}
EOF

cat >2.c <<\EOF
int bar() {
  return 3;
}
EOF

cat >3.c <<\EOF
int bar();

__asm__(".symver bar3v1, bar@V1");
int bar3v1();

int b() {
  return bar() + bar3v1();
}
EOF

cat >vs.t <<\EOF
V2 {
  global:
    bar;
};

V1 {
  global:
    bar;
};
EOF

cat >main.c <<\EOF
#include <stdio.h>
#define show(x) printf("%s: %d\n", #x, x);

int b();
int bar();

__asm__(".symver bar1, bar@V1");
int bar1();

int b_main() {
  return bar() + bar1();
}

int main() {
 show(b());
 show(b_main());
 return 0;
}
EOF

TARGET="x86_64-linux-gnu"

clang -target ${TARGET} -o 1.o 1.c -c -fPIC
clang -target ${TARGET} -o 2.o 2.c -c -fPIC
clang -target ${TARGET} -o 3.o 3.c -c -fPIC
clang -target ${TARGET} -o main.o main.c -c

LDs=(ld.eld ld.lld ld.bfd)
SFs=(eld lld bfd)

for i in "${!SFs[@]}"; do
  ${LDs[$i]} -o lib12.${SFs[$i]}.so 1.o 2.o 3.o --version-script vs.t -shared
  clang -target ${TARGET} --ld-path=$(which ${LDs[$i]}) -o main.${SFs[$i]}.out main.o lib12.${SFs[$i]}.so
done
```

10) (A) `bar`, (B) weak `bar@@V1`, and version script that assigns
   `V2` to `bar`.

With GNU ld, the dynamic symbol table contains both `bar@@V1` and `bar@@V2`, and
with lld and eld, the dynamic symbol table only contains `bar@@V1`.

Additionally, GNU seems to incorrectly resolves `bar@V1` undefined references to
weak `bar@@V1`, instead of the plain `bar`. LLD and eld correctly behaves here.

Reproducer:

```bash
#!/usr/bin/env bash

cat >1.c <<\EOF
__asm__(".symver bar1, bar@@V1");
__attribute__((weak))
int bar1() {
  return 1;
}

int bar() {
  return 5;
}
EOF

cat >2.c <<\EOF
int baz() {
  return 3;
}
EOF

cat >3.c <<\EOF
int bar();

__asm__(".symver bar3v1, bar@V1");
int bar3v1();

int b() {
  return bar() + bar3v1();
}
EOF

cat >vs.t <<\EOF
V2 {
  global:
    bar;
};

V1 {
  global:
    bar;
};
EOF

cat >main.c <<\EOF
#include <stdio.h>
#define show(x) printf("%s: %d\n", #x, x);

int b();
int bar();

__asm__(".symver bar1, bar@V1");
int bar1();

int b_main() {
  return bar() + bar1();
}

int main() {
 show(b());
 show(b_main());
 return 0;
}
EOF

TARGET="x86_64-linux-gnu"

clang -target ${TARGET} -o 1.o 1.c -c -fPIC
clang -target ${TARGET} -o 2.o 2.c -c -fPIC
clang -target ${TARGET} -o 3.o 3.c -c -fPIC
clang -target ${TARGET} -o main.o main.c -c

LDs=(ld.eld ld.lld ld.bfd)
SFs=(eld lld bfd)

for i in "${!SFs[@]}"; do
  ${LDs[$i]} -o lib12.${SFs[$i]}.so 1.o 2.o 3.o --version-script vs.t -shared
  clang -target ${TARGET} --ld-path=$(which ${LDs[$i]}) -o main.${SFs[$i]}.out main.o lib12.${SFs[$i]}.so
done
```

11) (A) bar, (B) `bar@@V1`, and version script that assigns
    `bar` to the version node `V2`.

With GNU ld, the dynamic symbol table contains both `bar@@V1` and `bar@@V2`, and
with lld, the dynamic symbol table only contains `bar@@V1`.

> [!IMPORTANT]
> eld currently reports multiple definition error.

12) (A) `bar@@V1`, (B) `bar`, and version script that assigns
    `bar` to the version node `V2`.

Multiple definition error.
