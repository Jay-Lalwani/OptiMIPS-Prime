  .set noat
  .text
  .align	2
  .globl	__start
  .ent	__start
  .type	__start, @function
__start:
  addiu $1, $0, 0xfff
  sll $1, $1, 12
  addiu $1, $1, 0xfff
  sll $1, $1, 8
  addiu $1, $1, 0xff
  add $2, $0, $0
loop1:
  andi $3, $1, 1
  beq $3, $0, skip
  addi $2, $2, 1
skip:
  srl $1, $1, 1
  bne $1, $0, loop1
  sw $2, 0($0)
.end	__start
.size	__start, .-__start
