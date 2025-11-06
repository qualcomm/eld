.text
.globl _start
_start:
    # Standard RISCV function prologue
    addi sp, sp, -16
    sd ra, 8(sp)
    sd fp, 0(sp)
    addi fp, sp, 16

    # Function body (simple nop)
    nop

    # Standard RISCV function epilogue
    ld fp, 0(sp)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

// RISCV64-specific SFrame section
.section .sframe,"a"
.align 8
    // SFrame Header
    .short 0xdee2          // sfh_preamble.sfp_magic
    .byte 2                // sfh_preamble.sfp_version (SFRAME_VERSION_2)
    .byte 1                // sfh_preamble.sfp_flags (SFRAME_F_FDE_SORTED)
    .byte 6                // sfh_abi_arch (SFRAME_ABI_RISCV64_ENDIAN_LITTLE)
    .byte -16              // sfh_cfa_fixed_fp_offset (Frame pointer offset)
    .byte -8               // sfh_cfa_fixed_ra_offset (Return address offset)
    .byte 0                // sfh_auxhdr_len (No auxiliary header)
    .long 1                // sfh_num_fdes (Number of function descriptors)
    .long 1                // sfh_num_fres (Number of frame row entries)
    .long 24               // sfh_fre_len (Frame row entries length)
    .long 24               // sfh_fdeoff (Function descriptor offset)
    .long 48               // sfh_freoff (Frame row entries offset)

    // Function Descriptor Entry (FDE)
    .long _start           // sfde_func_start_address
    .long 24               // sfde_func_size
    .long 0                // sfde_func_start_fre_off
    .long 1                // sfde_func_num_fres
    .byte 0x10             // sfde_func_info (SFRAME_FDE_TYPE_PCINC << 4 | SFRAME_FRE_TYPE_ADDR4)
    .byte 0                // sfde_func_rep_size
    .short 0               // sfde_func_padding2

    // Frame Row Entry (FRE)
    .long 0                // sfre_start_addr (Offset from function start)
    .byte 0x08             // sfre_info (CFA base register and offsets)
