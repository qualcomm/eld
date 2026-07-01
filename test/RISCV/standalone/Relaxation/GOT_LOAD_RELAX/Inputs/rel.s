.option relax
.option pic

.section zero, "aw", @progbits
.global zero_var
zero_var:
.word 42

.data
.global rel_var
rel_var:
.word 5

.hidden hidden_var
hidden_var:
.word 6

.text
.type ifunc STT_GNU_IFUNC
.hidden ifunc
ifunc:
    ret

.hidden hidden_func
hidden_func:
    ret

.global global_func
global_func:
    ret

.global foo
foo:
    lga a0, rel_var
    lga a1, hidden_var
    lga a2, ifunc
    lga a3, zero_var
    lga a4, hidden_func
    lga a5, global_func
