	.file	"main.s"
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"hello"
.LC1:
	.string	"mclinker"
.LC2:
	.string	"world"
	.section	.text.main,"ax",@progbits
        .p2align 2
	.globl main
	.type	main, @function
main:
	allocframe(#16)
        {
          r1 = #0
          r0 = ##.LC0
    	  call myfn
        }
        r0 = ##.LC1
        call myfn
        r0 = ##.LC2
        {
          call myfn
          c13:12 = r1:0
          nop
        }
	deallocframe
        jumpr r31
	.size	main, .-main

	.ident	"GCC: (GNU) 3.4.6 Hexagon Build Version 1.0.00 BT_20080818_1213_lnx64"
