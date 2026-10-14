  .option relax

  .section .pad,"ax",@progbits
pad_dummy:
  ret
  call pad_dummy
  .balign 8

  .globl target_0
target_0:
  ret
  .space 6
  .globl target_1
target_1:
  ret
  .space 6
  .globl target_2
target_2:
  ret
  .space 6
  .globl target_3
target_3:
  ret
  .space 6
  .globl target_4
target_4:
  ret
  .space 6
  .globl target_5
target_5:
  ret
  .space 6
  .globl target_6
target_6:
  ret
  .space 6
  .globl target_7
target_7:
  ret
  .space 6
  .globl target_8
target_8:
  ret
  .space 6
  .globl target_9
target_9:
  ret
  .space 6
  .globl target_10
target_10:
  ret
  .space 6
  .globl target_11
target_11:
  ret
  .space 6

  .section .call.0,"ax",@progbits
  .globl call_0
call_0:
  call target_0
  .section .call.1,"ax",@progbits
  .globl call_1
call_1:
  call target_1
  .section .call.2,"ax",@progbits
  .globl call_2
call_2:
  call target_2
  .section .call.3,"ax",@progbits
  .globl call_3
call_3:
  call target_3
  .section .call.4,"ax",@progbits
  .globl call_4
call_4:
  call target_4
  .section .call.5,"ax",@progbits
  .globl call_5
call_5:
  call target_5
  .section .call.6,"ax",@progbits
  .globl call_6
call_6:
  call target_6
  .section .call.7,"ax",@progbits
  .globl call_7
call_7:
  call target_7
  .section .call.8,"ax",@progbits
  .globl call_8
call_8:
  call target_8
  .section .call.9,"ax",@progbits
  .globl call_9
call_9:
  call target_9
  .section .call.10,"ax",@progbits
  .globl call_10
call_10:
  call target_10
  .section .call.11,"ax",@progbits
  .globl call_11
call_11:
  call target_11
