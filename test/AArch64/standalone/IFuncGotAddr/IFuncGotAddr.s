#---IFuncGotAddr.s--------------------- Executable --------------------#
#BEGIN_COMMENT
# When code takes the address of an IFUNC symbol through the GOT in a static
# executable (via R_AARCH64_ADR_GOT_PAGE + R_AARCH64_LD64_GOT_LO12_NC), the
# linker must emit an R_AARCH64_IRELATIVE relocation targeting the regular GOT
# entry.  At startup the CRT iterates __rela_iplt_start .. __rela_iplt_end,
# calls each IFUNC resolver, and writes the result into the GOT entry.
#
# This is a regression test for a bug where no IRELATIVE relocation was emitted
# for the regular GOT entry, leaving it permanently set to the resolver address.
# Code that loaded this address as a function pointer would invoke the resolver
# instead of the actual implementation, causing silent memory corruption.
#END_COMMENT
#START_TEST

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: %llvm-mc -filetype=obj -triple=aarch64 def.s -o def.o
# RUN: %llvm-mc -filetype=obj -triple=aarch64 use.s -o use.o
# RUN: %link %linkopts -march aarch64 def.o use.o -o out --section-start .text=0x1000

# RUN: llvm-readobj -r out | %filecheck --check-prefix=RELOCS %s

## Verify relevant sections exist.
# RUN: %readelf -S -W out | %filecheck --check-prefix=SECTIONS %s

# RELOCS: R_AARCH64_IRELATIVE - 0x1008

# SECTIONS: .rela.plt
# SECTIONS: .plt
# SECTIONS: .got.plt

#END_TEST

#--- def.s
// Define an IFUNC symbol whose resolver returns the address of the
// actual implementation.

  .text

// The real implementation.
  .type impl, @function
impl:
  add w0, w0, #1
  ret
  .size impl, .-impl

// The resolver: returns &impl.
  .type resolve_myfunc, @function
resolve_myfunc:
  adrp x0, impl
  add  x0, x0, :lo12:impl
  ret
  .size resolve_myfunc, .-resolve_myfunc

// Declare myfunc as an IFUNC whose resolver is resolve_myfunc.
  .globl myfunc
  .type  myfunc, @gnu_indirect_function
  .set   myfunc, resolve_myfunc

#--- use.s
// Take the address of an IFUNC symbol through the GOT
// (R_AARCH64_ADR_GOT_PAGE + R_AARCH64_LD64_GOT_LO12_NC)
// and also call it directly (R_AARCH64_CALL26 through PLT).

  .text
  .globl _start
  .type  _start, @function
_start:
  // Load &myfunc from GOT -- generates R_AARCH64_ADR_GOT_PAGE +
  // R_AARCH64_LD64_GOT_LO12_NC.  The linker emits an IRELATIVE relocation
  // on this GOT entry so the CRT resolves it at startup.
  adrp x8, :got:myfunc
  ldr  x8, [x8, :got_lo12:myfunc]

  // Call myfunc through PLT -- generates R_AARCH64_CALL26.
  bl   myfunc

  ret
  .size _start, .-_start
