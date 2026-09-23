# UNSUPPORTED: riscv64

# RUN: split-file %s %t

# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=-relax %t/a.s -o %t/a.o
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=-relax %t/baz.s -o %t/baz.o
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=-relax %t/encap.s -o %t/encap.o
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=-relax %t/weak.s -o %t/weak.o
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=-relax %t/shared.s -o %t/shared.o
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=-relax %t/refshared.s -o %t/refshared.o

# RUN: %link -shared %t/shared.o -o %t/libbaz.so

# RUN: %not %link %t/a.o -T %t/assign.t -o /dev/null 2>&1 | %filecheck %s --check-prefix=ASSIGN
# RUN: %not %link %t/a.o -T %t/provide.t -o /dev/null 2>&1 | %filecheck %s --check-prefix=PROVIDE
# RUN: %not %link %t/baz.o %t/a.o -T %t/section.t -o /dev/null 2>&1 | %filecheck %s --check-prefix=OBJECT
# RUN: %not %link %t/a.o --defsym baz=0x81090494 -T %t/defsym.t -o /dev/null 2>&1 | %filecheck %s --check-prefix=DEFSYM
# RUN: %not %link %t/encap.o -T %t/encap.t -o /dev/null 2>&1 | %filecheck %s --check-prefix=INTERNAL
# RUN: %not %link %t/weak.o -T %t/defsym.t -o /dev/null 2>&1 | %filecheck %s --check-prefix=UNDEF
# RUN: %not %link %t/refshared.o %t/libbaz.so -T %t/defsym.t -o /dev/null 2>&1 | %filecheck %s --check-prefix=SHARED

# RUN: %not %link %t/baz.o %t/a.o -T %t/sectionprovide.t -o /dev/null 2>&1 | %filecheck %s --check-prefix=OBJECT

# ASSIGN: Error: {{.*}}a.o:(.text): relocation R_RISCV_JAL out of range: {{.*}}; references 'baz'

# PROVIDE: Error: {{.*}}a.o:(.text): relocation R_RISCV_JAL out of range: {{.*}}; references 'baz'

# OBJECT: Error: {{.*}}a.o:(.text): relocation R_RISCV_JAL out of range: {{.*}}; references {{.*}}baz.o('baz')

# DEFSYM: Error: {{.*}}a.o:(.text): relocation R_RISCV_JAL out of range: {{.*}}; references 'baz'

# INTERNAL: Error: {{.*}}encap.o:(.text): relocation R_RISCV_JAL out of range: {{.*}}; references '__start_foo'

# UNDEF:     Error: {{.*}}weak.o:(.text): relocation R_RISCV_JAL out of range: {{.*}}; references 'wk'
# UNDEF-NOT: defined in

# SHARED: Error: {{.*}}refshared.o:(.text): relocation R_RISCV_JAL out of range: {{.*}}; references {{.*}}libbaz.so('baz')

#--- a.s
  .option exact
  .text
  .globl _start
_start:
  jal ra, baz

#--- baz.s
  .section .baz,"ax",%progbits
  .globl baz
baz:
  nop

#--- assign.t
baz = 0x81090494;
SECTIONS {
  . = 0x812000a0;
  .text : { *(.text) }
}

#--- provide.t
SECTIONS {
  . = 0x812000a0;
  .text : { *(.text) }
}
PROVIDE(baz = 0x81090494);

#--- section.t
SECTIONS {
  . = 0x81090494;
  .baz : { *(.baz) }
  . = 0x812000a0;
  .text : { *(.text) }
}

#--- sectionprovide.t
SECTIONS {
  . = 0x81090494;
  .baz : { *(.baz) }
  . = 0x812000a0;
  .text : { *(.text) }
}
PROVIDE(baz = 0x81090494);

#--- weak.s
  .option exact
  .text
  .globl _start
_start:
  jal ra, wk
  .weak wk

#--- shared.s
  .text
  .globl baz
baz:
  nop

#--- refshared.s
  .option exact
  .text
  .globl _start
_start:
  jal ra, baz

#--- defsym.t
SECTIONS {
  . = 0x812000a0;
  .text : { *(.text) }
}

#--- encap.s
  .option exact
  .text
  .globl _start
_start:
  jal ra, __start_foo
  .section foo,"a",%progbits
  .long 0

#--- encap.t
SECTIONS {
  . = 0x812000a0;
  .text : { *(.text) }
  . = 0x91090494;
  foo : { *(foo) }
}
