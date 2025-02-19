  .set noat
  .text
  .align	2
  .globl	__start
  .ent	__start
  .type	__start, @function
__start:
  addi $1, $0, 512
  addi $2, $0, 0x149e
  addi $3, $0, 1
outerloop:
  and $5, $2, $3
  bne $5, $0, branch1
  addi $6, $6, 1
branch1:
  beq $5, $0, branch2
  addi $7, $7, 2
branch2:
  beq $5, $0, branch3
  addi $8, $8, 4
branch3:
  bne $5, $0, branch4
  addi $9, $9, 8
branch4:
  beq $5, $0, branch5
  addi $8, $8, 16
branch5:
  bne $5, $0, branch6
  addi $7, $7, 32
branch6:
  bne $5, $0, branch7
  addi $6, $6, 64
branch7:
  sll $3, $3, 1
  ori $4, $0, 32768
  bne $4, $3, noreset
  addi $3, $0, 1
noreset:
  addi $1, $1, -1
  bne $1, $0, outerloop
.end	__start
.size	__start, .-__start
