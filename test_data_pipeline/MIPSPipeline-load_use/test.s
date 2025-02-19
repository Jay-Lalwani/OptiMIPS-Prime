  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
    addi $1, $0, 1
    sw $1, 0($0)
    lw $2, 0($0)
    add $3, $2, $2
    sw $3, 0($0)
    lw $4, 0($0)
    addi $5, $4, 1
    sw $5, 0($0)
    lw $6, 0($0)
    addi $6, $5, 1
    sw $6, 0($0)
    lw $7, 0($0)
    sw $6, 0($7)
    lw $8, 0($7)
    beq $8, $8, endlabel
    addi $10, $10, 1
endlabel:
	.end	__start
	.size	__start, .-__start
