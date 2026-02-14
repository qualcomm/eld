.type main,@function
.global main
main:
adrp x0, _GLOBAL_OFFSET_TABLE_
add x0, x0, #:lo12:_GLOBAL_OFFSET_TABLE_
ldr x1, [x0, #:gotpage_lo15:first]
ldr x2, [x0, #:gotpage_lo15:second]
