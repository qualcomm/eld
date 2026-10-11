.text
.globl test_local_rex
.type test_local_rex, @function
test_local_rex:
    movq sym_local@GOTPCREL(%rip), %rax
    retq
.size test_local_rex, .-test_local_rex

.data
.local sym_local
.type sym_local, @object
sym_local:
    .quad 0x12345678
.size sym_local, .-sym_local
