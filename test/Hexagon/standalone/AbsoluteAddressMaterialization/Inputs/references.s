.text
.globl entry, offset, high_bit, all_bits, zero_value, ordinary, branch
.globl new_value, loop_packet, absolute_branch
.hidden entry
.hidden data, high, maximum, zero
.type entry,@function
entry:
{ r0 = add(pc, ##data@PCREL); jumpr r31 }

.p2align 2
offset:
{ r17 = add(pc, ##data@PCREL+5) }
{ jumpr r31 }

high_bit:
{ r31 = add(pc, ##high@PCREL) }

all_bits:
{ r1 = add(pc, ##maximum@PCREL); jumpr r31 }

zero_value:
{ r2 = add(pc, ##zero@PCREL); jumpr r31 }

# Ordinary image-relative data still needs PC-relative addressing.
ordinary:
{ r0 = add(pc, ##normal@PCREL); jumpr r31 }

# B32_PCREL_X also extends branches, which must not be rewritten.
branch:
{ jump ##entry }

new_value:
{ r0 = add(pc, ##data@PCREL); memw(r1) = r0.new }
{ jumpr r31 }

loop_packet:
{ loop0(.Lloop, #1) }
.Lloop:
{ r3 = add(pc, ##data@PCREL); r4 = #1 }:endloop0
{ jumpr r31 }

absolute_branch:
{ jump ##data }

.data
.local normal
.type normal,@object
normal:
.word 123
.size normal, .-normal
