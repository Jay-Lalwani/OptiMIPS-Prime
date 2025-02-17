  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $2, $1, 1
   addi $3, $1, 1
   add $3, $3, $2
   add $4, $3, $2
   add $5, $4, $3
   add $6, $5, $4
   add $7, $6, $5
   add $8, $7, $6
   add $9, $8, $7
   add $10, $9, $8

	.end	__start
	.size	__start, .-__start
