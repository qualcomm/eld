	.text
        .align  2
        .global main
        .type   main, %function
main:
        sub     sp, sp, #16
        mov     x7, 13
        str     w7, [sp,12]
        b       e843419
        ret
        .size   main, .-main

        .section .e843419, "xa"
        .align  2
        .global e843419
        .type   e843419, %function
e843419:
        sub     sp, sp, #16
        mov     x7, 13
        str     w7, [sp,12]
        b       e843419_1
         .fill 4072,1,0
e843419_1:
	adrp x0, data0
        str x7, [x0,12]
        mov	x8, 9
        str x8, [x0, :lo12:data0]

        add x0, x1, x5
        ldr     w7, [sp,12]
        add     w0, w7, w7
        add     sp, sp, 16
        ret
        .size   e843419, .-e843419
        .data
data0:
        .fill 8,1,255
        .balign 1
        .fill 4,1,255
