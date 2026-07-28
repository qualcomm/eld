        # A hidden (non-preemptible) STT_GNU_IFUNC symbol. Even though it is
        # non-preemptible, it must NOT be relaxed: the IRELATIVE resolver runs
        # at load time and deposits the selected implementation's address in
        # the GOT slot. Relaxing to a PC-relative access would bypass that.
        .text
        .globl resolver
        .hidden resolver
        .type  resolver,@function
resolver:
        ret

        .globl my_ifunc
        .hidden my_ifunc
        .type  my_ifunc,@gnu_indirect_function
        .set   my_ifunc, resolver

        .globl h
        .type  h,@function
h:
        movl    my_ifunc@GOTPCREL(%rip), %eax
        ret
