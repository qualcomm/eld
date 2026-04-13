.option relax
.option pic

.data
.global rel_var
rel_var:
.word 10

.text
.global foo
foo:
    lga a1, abs_var + 4
    lga a2, rel_var + 4
