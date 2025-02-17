  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $16, $0, 5
   addi $17, $0, 5
   addi $18, $0, 300
   add $8, $0, $0
loopi:
   slt $12, $8, $16
   beq $12, $0, endi
   add $9, $0, $0
loopj:
   slt $12, $9, $17
   beq $12, $9, endj
   add $10, $8, $9
   sll $11, $9, 5
   add $11, $18, $11
   sw $10, 0($11)
   addi $9, $9, 1
   j loopj
endj:
   addi $8, $8, 1
   j loopi
endi:
	.end	__start
	.size	__start, .-__start
