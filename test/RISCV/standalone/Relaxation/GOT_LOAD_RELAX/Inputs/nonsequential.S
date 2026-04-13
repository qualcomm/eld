.option relax
.option pic

#ifdef USE_REL
.data
.global var
var:
.word 10

.global var2
var2:
.word 5
#endif

## The relaxation should work with unrelated instructions
## and relocations interspersed.
.text
.global foo
foo:
.La:
    auipc a0, %got_pcrel_hi(var)
    nop
    nop
    lw a3, %pcrel_lo(.La)(a0)
    lga a1, var2
    lw a2, %pcrel_lo(.La)(a0)

## Check that the relaxation is able to cope with the HI/LO relocations
## not necessarily being processed in that order.
.global bar
bar:
    j .Lgotpcrel_hi1

.Lpcrel_lo1:
    lw a0, %pcrel_lo(.Lgotpcrel_hi1)(a0)
    ret

.Lgotpcrel_hi1:
    auipc a0, %got_pcrel_hi(var2)
    j .Lpcrel_lo1
