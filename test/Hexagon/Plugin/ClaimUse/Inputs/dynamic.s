.text
.globl _start
_start:
{ r0 = memw(r1 + ##claimed_data@GOT) }
{ call claimed_function@PLT }
{ r0 = memw(r1 + ##other_data@GOT) }
{ call other_function@PLT }
{ jumpr r31 }
.globl claimed_function
.type claimed_function,@function
claimed_function:
{ jumpr r31 }
.globl other_function
.type other_function,@function
other_function:
{ jumpr r31 }
.data
.globl claimed_data
.type claimed_data,@object
claimed_data: .word 123
.size claimed_data,4
.globl other_data
.type other_data,@object
other_data: .word 456
.size other_data,4
