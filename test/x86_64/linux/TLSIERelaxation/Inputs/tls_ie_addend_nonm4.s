        # GOTTPOFF with addend 0 (non-standard). GNU as always emits addend -4
        # for GOTTPOFF; use .reloc to inject addend 0 explicitly. The linker
        # must keep the GOT slot when the addend is not -4.
        .globl tls_var_bad
        .hidden tls_var_bad
        .section .tbss,"awT",@nobits
tls_var_bad: .long 0

        .text
        .globl get_tls_bad
        .type  get_tls_bad,@function
get_tls_bad:
        movq 0(%rip), %rax
        .reloc .-4, R_X86_64_GOTTPOFF, tls_var_bad+0
        ret
