.text
.global _start
_start:
.word 0

.bss
.local ordinary_local
ordinary_local:
.word 0
.L_temp:
.space 4
.L_temp_end:

.text
.word .L_temp_end - .
