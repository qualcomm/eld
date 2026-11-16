## A local-dynamic GOT pair describes this module, not an individual TLS symbol.
## Its DTPMOD32 relocation must use symbol index zero. In particular, deciding
## which dynamic symbols to export and sorting relocations must not dereference
## the absent symbol. Keep symbol-bearing GD and IE relocations in the same
## output to exercise both forms of DTPMOD32 and relocation sorting.

# RUN: %clang -target armv7a-linux-gnueabihf -c %s -o %t.o
# RUN: %readelf -rW %t.o | %filecheck %s --check-prefix=INPUT
# RUN: %link %linkopts -m armelf_linux_eabi -shared %t.o -o %t.so
# RUN: %readelf -rW %t.so | %filecheck %s --check-prefixes=LD,PURE
# RUN: %readelf --dyn-syms -W %t.so | %filecheck %s --check-prefix=SYMS --implicit-check-not=local_tls
# RUN: %objdump -s -j .got %t.so | %filecheck %s --check-prefix=GOT
# RUN: %link %linkopts -m armelf_linux_eabi -shared -z nocombreloc %t.o -o %t.unsorted.so
# RUN: %readelf -rW %t.unsorted.so | %filecheck %s --check-prefixes=LD,PURE
# RUN: %llvm-mc -triple=armv7a-linux-gnueabihf -filetype=obj -defsym=MIXED=1 %s -o %t.mixed.o
# RUN: %link %linkopts -m armelf_linux_eabi -shared %t.mixed.o -o %t.mixed.so
# RUN: %readelf -rW %t.mixed.so | %filecheck %s --check-prefixes=LD,MIXED
# RUN: %readelf --dyn-syms -W %t.mixed.so | %filecheck %s --check-prefix=MIXED-SYMS --implicit-check-not=local_tls
# RUN: %link %linkopts -m armelf_linux_eabi -pie -e get_local_tls --defsym=__tls_get_addr=0 %t.o -o %t.pie
# RUN: %readelf -rW %t.pie | %filecheck %s --check-prefix=PIE
# RUN: %readelf --dyn-syms -W %t.pie | %filecheck %s --check-prefix=PIE-SYMS --implicit-check-not=local_tls

# INPUT: R_ARM_THM_CALL {{.*}} __tls_get_addr
# INPUT: R_ARM_TLS_LDM32 {{.*}} local_tls
# INPUT: R_ARM_TLS_LDO32 {{.*}} local_tls

# PURE: Relocation section '.rel.dyn' {{.*}} contains 1 entries:
# MIXED: Relocation section '.rel.dyn' {{.*}} contains 4 entries:
# LD: {{[0-9a-f]+}} 00000011 R_ARM_TLS_DTPMOD32
# MIXED-DAG: R_ARM_TLS_DTPMOD32 {{[0-9a-f]+}} global_tls
# MIXED-DAG: R_ARM_TLS_DTPOFF32 {{[0-9a-f]+}} global_tls
# MIXED-DAG: R_ARM_TLS_TPOFF32 {{[0-9a-f]+}} ie_tls
# LD: Relocation section '.rel.plt'
# LD: R_ARM_JUMP_SLOT {{.*}} __tls_get_addr

# SYMS: Symbol table '.dynsym' contains 3 entries:
# SYMS-DAG: __tls_get_addr
# SYMS-DAG: get_local_tls
# MIXED-SYMS: Symbol table '.dynsym' contains 5 entries:
# MIXED-SYMS-DAG: __tls_get_addr
# MIXED-SYMS-DAG: get_local_tls
# MIXED-SYMS-DAG: global_tls
# MIXED-SYMS-DAG: ie_tls
# PIE: Relocation section '.rel.dyn' {{.*}} contains 1 entries:
# PIE: {{[0-9a-f]+}} 00000011 R_ARM_TLS_DTPMOD32
# PIE-SYMS: Symbol table '.dynsym' contains 1 entries:

## Both the module-ID addend and the module-relative offset start at zero.
# GOT: Contents of section .got:
# GOT-NEXT: {{[0-9a-f]+}} 00000000 00000000

.syntax unified
.thumb
.section .tbss,"awT",%nobits
.align 2
local_tls:
  .space 4

.text
.align 1
.globl get_local_tls
.thumb_func
get_local_tls:
  push {r7, lr}
  ldr r3, .Ltls
.Lpic:
  add r3, pc
  mov r0, r3
  bl __tls_get_addr(PLT)
  ldr r2, .Ltls+4
  add r0, r0, r2
  pop {r7, pc}

.align 2
.Ltls:
  .word local_tls(tlsldm) + (. - .Lpic - 4)
  .word local_tls(tlsldo)

.ifdef MIXED
  .word global_tls(tlsgd)
  .word ie_tls(gottpoff)
.endif
