  .set noat
  .text
  .align	2
  .globl	__start
  .ent	__start
  .type	__start, @function
__start:
  addiu $1, $0, 0x492
  sll $1, $1, 12
  addiu $1, $1, 0x492
  sll $1, $1, 8
  addiu $1, $1, 0x49
  addiu $4, $0, 0xb6d
  sll $4, $4, 12
  addiu $4, $4, 0xb6d
  sll $4, $4, 8
  addiu $4, $4, 0xb6
  add $2, $0, $0
loop1:
  andi $3, $1, 1
  beq $3, $0, skip1
  addi $2, $2, 1
skip1:
  andi $5, $4, 1
  beq $5, $0, skip2
  addi $2, $2, 1
skip2:
  srl $1, $1, 1
  bne $1, $0, loop1
  sw $2, 0($0)
  .end	__start
  .size	__start, .-__start
