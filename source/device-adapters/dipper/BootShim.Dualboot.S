_Head:
	/* Jump to the real code */
	b		_Start

_KernelStart:
	/* Jump to the kernel
	   This value needs to be filled when patching */
	b 		#0x02000000

	/* Text Offset */
	.quad 0x00080000

	/* Image Size */
	.quad 0x0BADFACE

	/* Flags. 0x2 informs the bootloader to load kernel as
	 low as possible to make enough room for UEFI */
	.quad 0x2
_StackBase:
	.quad UEFI_BASE

_StackSize:
	.quad UEFI_SIZE

_KernelSize:
	/* Size of kernel for calculating where the FD is */
	.quad 0xDEADBEEF

	/* ARM64 Magic */
	.ascii "ARM\x64"

	/* Reserved */
	.long 0

	/* Magic for identification */
	.ascii "EDK2-MSM"

_Start:
	/* Here it reads 0x17FE00000 to get 
	message from simple-init on last reboot */
	mov		x4, #0x7FE00000
	movk	x4, #0x1, lsl #32
	mov		x6, #128
	mov		w7, #0x5241

_TestLoop:
	ldrh	w5, [x4]
	add		x4, x4, #2
	cmp		w5, w7
	b.eq	_ClearFlags
	subs	x6, x6, #1
	b.ne    _TestLoop

_ChargerProbe:
    bl _ChargingDecision
    /* Reuse the established Android handoff scratch region, without its magic. */
    mov x4, #0x7FE00000
    movk x4, #1, lsl #32
    mov w5, #0x4730
    movk w5, #0x4348, lsl #16
    str w5, [x4]
    str w9, [x4, #4]
    dsb sy
    tbz w9, #0, _CopyUEFI
    mov x1, xzr
    mov x2, xzr
    mov x3, xzr
    b _KernelStart

_CopyUEFI:
	/* Copy the UEFI into the right location */
	adr		x4, _Head
	ldr		x5, _KernelSize
	add		x4, x4, x5

	ldr		x5, _StackBase
	ldr		x6, _StackSize

	bl 		_CopyLoop

    /* Preserve the original dualboot payload handoff after copying the FD.
     * Charger routing must not omit the appended XNUP container. */
    ldr w2, [x4]
    movz w3, #0x4E58
    movk w3, #0x5055, lsl #16
    cmp w2, w3
    b.ne _Entry
    ldr w6, [x4, #4]
    cmp w6, #24
    b.lo _Entry
    mov x7, #0x04000000
    sub x7, x7, #23
    cmp x6, x7
    b.hi _Entry
    add x6, x6, #23
    and x6, x6, #~15
    mov x5, #0xF0000000
    bl _CopyLoop

_Entry:
	ldr		x5, _StackBase
	/* Jump to UEFI */
	br		x5

_CopyLoop:
	ldp		x2, x3, [x4], #16
	stp		x2, x3, [x5], #16
	subs	x6, x6, #16
	b.ne	_CopyLoop
	ret

_ClearFlags:
	mov		x4, #0x7FE00000
	movk	x4, #0x1, lsl #32
	mov		x6, #32

_ClearLoop:
	/* fill in 64bit at a time */
	str		xzr, [x4]
	add		x4, x4, #8
	subs	x6, x6, #1
	b.ne    _ClearLoop
	b 		_KernelStart

_Dead:
	/* We should never get here */
	b		_Dead

.text
.align 4
/* Preserve the original FDT argument and stack. Scratch stack lies in the
 * already-used UEFI target buffer, before FD copy; never overlaps the FDT.
 */
.global _ChargingDecision
_ChargingDecision:
    mov w9, #0
    mov x10, #0x80000000
    cmp x0, x10
    b.lo _ProbeReturn
    mov x10, #0xFFE00000
    cmp x0, x10
    b.hi _ProbeReturn
    tst x0, #3
    b.ne _ProbeReturn
    ldr x10, _StackBase
    sub x11, x10, #0x200, lsl #12
    ldr x12, _StackSize
    add x10, x10, x12
    cmp x0, x11
    b.lo _ProbeStack
    cmp x0, x10
    b.lo _ProbeReturn
_ProbeStack:
    mov x11, sp
    sub x10, x10, #64
    mov sp, x10
    stp x19, x20, [sp]
    str x30, [sp, #16]
    mov x19, x0
    mov x20, x11
    mov x1, #0x200000
    bl hercules_charger_mode
    mov w9, w0
    mov x0, x19
    mov x10, x20
    ldp x19, x20, [sp]
    ldr x30, [sp, #16]
    mov sp, x10
    mov x1, xzr
    mov x2, xzr
    mov x3, xzr
_ProbeReturn:
    ret
