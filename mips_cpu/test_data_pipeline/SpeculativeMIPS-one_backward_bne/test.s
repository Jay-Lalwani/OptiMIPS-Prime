  .set noat
  .text
  .align	2
  .globl	__start
  .ent	__start
  .type	__start, @function
__start:
  addi $1, $0, 500
  add $2, $0, $0
loop:
  addi $2, $2, 2
  addi $1, $1, -1
  bne $1, $0, loop
  sw $2, 0($0)
  .end	__start
  .size	__start, .-__start
