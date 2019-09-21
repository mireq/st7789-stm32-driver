.syntax unified
.cpu cortex-m3
.fpu softvfp
.thumb

.section .text.startup_


.thumb_func
.global start_
start_:
	ldr r0, stacktop
	mov sp, r0
	ldr r2, =_bss_start
	b loop_zero_bss
zero_bss:
	movs r3, #0x00
	str r3, [r2], #0x04
loop_zero_bss:
	ldr r3, = _bss_end
	cmp r2, r3
	bcc zero_bss
	bl main
	b hang

.thumb_func
hang:
	b .

.align
stacktop: .word 0x20005000

.end
