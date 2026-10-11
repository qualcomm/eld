.text
.globl test_local_plain
.type test_local_plain, @function
test_local_plain:
    xorq %rax, %rax
    addq sym_local@GOTPCREL(%rip), %rax
    retq
.size test_local_plain, .-test_local_plain

.data
.local sym_local
.type sym_local, @object
sym_local:
    .quad 0x12345678
.size sym_local, .-sym_local
