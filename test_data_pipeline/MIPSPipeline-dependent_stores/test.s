  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $1, $0, 1
   sw $1, 0($0)
   lw $10, 0($0)
   nop
   sw $10, 4($0)
   lw $2, 4($0)
   add $1, $1, $1
   sw $2, 8($0)
   lw $3, 8($0)
   add $1, $1, $1
   sw $1, 0($0)
   lw $4, 0($0)
   add $1, $1, $1
   add $2, $2, $2
   sw $1, 4($0)
   lw $5, 4($0)
	.end	__start
	.size	__start, .-__start
