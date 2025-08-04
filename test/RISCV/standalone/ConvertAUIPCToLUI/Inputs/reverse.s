## .data lives at a low address.
.data
X:      .word 5

## Code lives at least 2GB away, which is controlled by linker script.
.text

## Usually, R_RISCV_PCREL_LO12_I follows R_RISCV_PCREL_HI20.
.Lpcrel_hi0:
        auipc a0, %pcrel_hi(X)
        lw a0, %pcrel_lo(.Lpcrel_hi0)(a0)

        j .Lpcrel_hi1

## R_RISCV_PCREL_HI20 relocation is processed after the paired R_RISCV_PCREL_LO12_I.
## This should produce the same address because the resulting relocation is an absolute one.
.Lpcrel_lo1:
        lw a0, %pcrel_lo(.Lpcrel_hi1)(a0)
        ret

.Lpcrel_hi1:
        auipc a0, %pcrel_hi(X)
        j .Lpcrel_lo1
