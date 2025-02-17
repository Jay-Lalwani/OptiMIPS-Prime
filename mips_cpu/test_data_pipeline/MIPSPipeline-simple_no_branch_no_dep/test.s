  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $1, $0, 5
   addiu $2, $0, 10
   andi $3, $0, 2
   sw $0, 0($0)
   sw $0, 0($0)
   sw $0, 0($0)
   lw $4, 0($0)
   lw $5, 0($0)
   lw $6, 0($0)
   lw $7, 0($0)
   lui $8, 35
   ori $9, $0, 2
   slti $10, $0, 1
   sltiu $11, $0, 1
   add $12, $1, $2
   addu $13, $1, $2
   and $14, $1, $2
   nor $15, $1, $2
   or $16, $1, $2
   slt $17, $1, $2
   sltu $18, $2, $1
   sll $19, $1, 2
   srl $20, $2, 1
   sub $21, $1, $2
   subu $22, $1, $2
	.end	__start
	.size	__start, .-__start
