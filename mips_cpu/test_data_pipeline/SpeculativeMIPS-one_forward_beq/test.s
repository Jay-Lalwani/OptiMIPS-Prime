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
  beq $1, $0, after
  addi $2, $2, 1
  addi $1, $1, -1
  j loop
after:
  sw $2, 0($1)
  .end	__start
  .size	__start, .-__start
