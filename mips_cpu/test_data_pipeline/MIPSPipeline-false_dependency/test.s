  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $1, $0, 1
   addi $2, $0, 2
   beq $1, $2, endlabel
   add $3, $1, $2
   sw $3, 0($0)
   sub $6, $2, $3
   add $4, $1, $2
   sw $4, 0($0)
   sub $6, $4, $3
endlabel:
	.end	__start
	.size	__start, .-__start
