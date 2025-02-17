  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $1, $0, 5
   addi $2, $0, 1
   addi $3, $0, 2
   add $4, $1, $1
	.end	__start
	.size	__start, .-__start
