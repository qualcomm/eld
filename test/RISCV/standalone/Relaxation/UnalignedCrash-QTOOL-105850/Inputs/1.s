        .text
        .globl  foo
        .type   foo, @function
        .balign 32
foo:    nop
        // Add a relaxation relocation as RISC-V ABI demands that R_ALIGN
        // relaxations are needed only if the same section contains
        // R_RELAX relocations.
.Lpcrel_hi6:
        auipc   a5,%pcrel_hi(foo)
        lw      a0,%pcrel_lo(.Lpcrel_hi6)(a5)
        ret
        .size   foo, .-foo
