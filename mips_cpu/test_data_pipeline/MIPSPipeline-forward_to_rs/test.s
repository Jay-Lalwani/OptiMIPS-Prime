  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $4, $1, 2
   addi $2, $1, 1
   addi $3, $1, 4
   addi $2, $2, 2
   add $3, $2, $1
   sw $4, 0($3)
   lw $5, 0($3)
   addi $4, $4, 15
   add $6, $5, $5
	.end	__start
	.size	__start, .-__start
