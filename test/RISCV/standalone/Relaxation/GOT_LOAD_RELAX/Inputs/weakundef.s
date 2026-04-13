.option relax
.option pic

.weak sym_weakundef

.text
.global foo
foo:
    lga a0, sym_weakundef
