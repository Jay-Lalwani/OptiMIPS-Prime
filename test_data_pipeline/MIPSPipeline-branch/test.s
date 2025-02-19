  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $1, $1, 1
   addi $5, $5, 5
   beq $5, $5, skip
   addi $1, $1, 1
   addi $1, $1, 1
   addi $1, $1, 1
   addi $1, $1, 1
   addi $1, $1, 1
   addi $1, $1, 1
   addi $1, $1, 1
skip:
   addi $2, $2, 2
   addi $2, $2, 2
   addi $2, $2, 2
   bne $2, $5, endlabel
   addi $2, $2, 2
   addi $2, $2, 2
   addi $2, $2, 2
endlabel:
	.end	__start
	.size	__start, .-__start
