  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $10, $1, 10
   addi $4, $1, 2
   addi $4, $1, 5
   add $2, $10, $4
   add $3, $10, $4
   sw $3, 0($3)
   lw $5, 0($3)
   sub $6, $10, $3
   add $7, $5, $5
   beq $7, $7, endlabel
endlabel:
	.end	__start
	.size	__start, .-__start
