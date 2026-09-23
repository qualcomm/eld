        # A GOTPCRELX relocation placed at offset 0 via .reloc, so the
        # opcode bytes at loc[-2]/loc[-1] would be before the start of the
        # fragment buffer. The linker must not crash; it should skip
        # relaxation and keep the GOT slot.
        .globl foo
        .hidden foo
        .data
foo:    .long 42

        .text
        .globl h
        .type h,@function
h:
        .reloc ., R_X86_64_GOTPCRELX, foo
        nop
        ret
