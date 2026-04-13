.option relax
.option pic

.text
.global foo
foo:
    lga a0, sym_cli_pos
    lga a1, sym_cli_neg
    lga a2, sym_cli_pos_oob
    lga a3, sym_cli_neg_oob
    lga a0, sym_addi_pos
    lga a1, sym_addi_neg
    lga a2, sym_addi_pos_oob
    lga a3, sym_addi_neg_oob
    lga a0, sym_zero
    lga zero, sym_cli_pos
    lga a1, sym_hidden
