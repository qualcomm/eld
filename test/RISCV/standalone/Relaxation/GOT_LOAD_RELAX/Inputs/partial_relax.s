.option norelax
.option pic

.data
.global rel_var
rel_var:
.word 10

.text
.global foo
foo:
## Only LO is marked with RELAX. No relaxation
.La_abs:
    auipc a1, %got_pcrel_hi(abs_var)
    lw    a1, %pcrel_lo(.La_abs)(a1)
    .reloc .-4, R_RISCV_RELAX

.La_rel:
    auipc a2, %got_pcrel_hi(rel_var)
    lw    a2, %pcrel_lo(.La_rel)(a2)
    .reloc .-4, R_RISCV_RELAX

## Only HI is marked with RELAX. No relaxation.
.Lb_abs:
    auipc a1, %got_pcrel_hi(abs_var)
    .reloc .-4, R_RISCV_RELAX
    lw    a1, %pcrel_lo(.Lb_abs)(a1)

.Lb_rel:
    auipc a2, %got_pcrel_hi(rel_var)
    .reloc .-4, R_RISCV_RELAX
    lw    a2, %pcrel_lo(.Lb_rel)(a2)

## Multiple LOs for one HI, not all are marked RELAX.
## There should be no relaxation
.Lc_abs:
    auipc a1, %got_pcrel_hi(abs_var)
    .reloc .-4, R_RISCV_RELAX
    lw    a2, %pcrel_lo(.Lc_abs)(a1)
    .reloc .-4, R_RISCV_RELAX
    lw    a3, %pcrel_lo(.Lc_abs)(a1)

.Lc_rel:
    auipc a1, %got_pcrel_hi(rel_var)
    .reloc .-4, R_RISCV_RELAX
    lw    a2, %pcrel_lo(.Lc_rel)(a1)
    .reloc .-4, R_RISCV_RELAX
    lw    a3, %pcrel_lo(.Lc_rel)(a1)

## HI with RELAX but no LO. No relaxation.
.Ld_rel:
    auipc a1, %got_pcrel_hi(rel_var)
    .reloc .-4, R_RISCV_RELAX
