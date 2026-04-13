.option relax
.option pic

.section .sym_wp, "aw", @progbits
.global sym_wp
sym_wp: .word 0

.section .sym_op, "aw", @progbits
.global sym_op
sym_op: .word 0

.section .sym_wn, "aw", @progbits
.global sym_wn
sym_wn: .word 0

.section .sym_on, "aw", @progbits
.global sym_on
sym_on: .word 0

.text
.global foo
foo:
    lga a0, sym_wp
    lga a1, sym_op
    lga a2, sym_wn
    lga a3, sym_on
