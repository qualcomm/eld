.global _start

.type _start, %function
_start:
la t0, data
ret
data:
.word 100
