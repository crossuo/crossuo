#include "x86_arch.h"
.text	

.def	_mul_1x1;	.scl 3;	.type 32;	.endef
.p2align	4
_mul_1x1:
	subq	$128+8,%rsp
	movq	$-1,%r9
	leaq	(%rax,%rax,1),%rsi
	shrq	$3,%r9
	leaq	(,%rax,4),%rdi
	andq	%rax,%r9
	leaq	(,%rax,8),%r12
	sarq	$63,%rax
	leaq	(%r9,%r9,1),%r10
	sarq	$63,%rsi
	leaq	(,%r9,4),%r11
	andq	%rbp,%rax
	sarq	$63,%rdi
	movq	%rax,%rdx
	shlq	$63,%rax
	andq	%rbp,%rsi
	shrq	$1,%rdx
	movq	%rsi,%rcx
	shlq	$62,%rsi
	andq	%rbp,%rdi
	shrq	$2,%rcx
	xorq	%rsi,%rax
	movq	%rdi,%rbx
	shlq	$61,%rdi
	xorq	%rcx,%rdx
	shrq	$3,%rbx
	xorq	%rdi,%rax
	xorq	%rbx,%rdx

	movq	%r9,%r13
	movq	$0,0(%rsp)
	xorq	%r10,%r13
	movq	%r9,8(%rsp)
	movq	%r11,%r14
	movq	%r10,16(%rsp)
	xorq	%r12,%r14
	movq	%r13,24(%rsp)

	xorq	%r11,%r9
	movq	%r11,32(%rsp)
	xorq	%r11,%r10
	movq	%r9,40(%rsp)
	xorq	%r11,%r13
	movq	%r10,48(%rsp)
	xorq	%r14,%r9
	movq	%r13,56(%rsp)
	xorq	%r14,%r10

	movq	%r12,64(%rsp)
	xorq	%r14,%r13
	movq	%r9,72(%rsp)
	xorq	%r11,%r9
	movq	%r10,80(%rsp)
	xorq	%r11,%r10
	movq	%r13,88(%rsp)

	xorq	%r11,%r13
	movq	%r14,96(%rsp)
	movq	%r8,%rsi
	movq	%r9,104(%rsp)
	andq	%rbp,%rsi
	movq	%r10,112(%rsp)
	shrq	$4,%rbp
	movq	%r13,120(%rsp)
	movq	%r8,%rdi
	andq	%rbp,%rdi
	shrq	$4,%rbp

	movq	(%rsp,%rsi,8),%xmm0
	movq	%r8,%rsi
	andq	%rbp,%rsi
	shrq	$4,%rbp
	movq	(%rsp,%rdi,8),%rcx
	movq	%r8,%rdi
	movq	%rcx,%rbx
	shlq	$4,%rcx
	andq	%rbp,%rdi
	movq	(%rsp,%rsi,8),%xmm1
	shrq	$60,%rbx
	xorq	%rcx,%rax
	pslldq	$1,%xmm1
	movq	%r8,%rsi
	shrq	$4,%rbp
	xorq	%rbx,%rdx
	andq	%rbp,%rsi
	shrq	$4,%rbp
	pxor	%xmm1,%xmm0
	movq	(%rsp,%rdi,8),%rcx
	movq	%r8,%rdi
	movq	%rcx,%rbx
	shlq	$12,%rcx
	andq	%rbp,%rdi
	movq	(%rsp,%rsi,8),%xmm1
	shrq	$52,%rbx
	xorq	%rcx,%rax
	pslldq	$2,%xmm1
	movq	%r8,%rsi
	shrq	$4,%rbp
	xorq	%rbx,%rdx
	andq	%rbp,%rsi
	shrq	$4,%rbp
	pxor	%xmm1,%xmm0
	movq	(%rsp,%rdi,8),%rcx
	movq	%r8,%rdi
	movq	%rcx,%rbx
	shlq	$20,%rcx
	andq	%rbp,%rdi
	movq	(%rsp,%rsi,8),%xmm1
	shrq	$44,%rbx
	xorq	%rcx,%rax
	pslldq	$3,%xmm1
	movq	%r8,%rsi
	shrq	$4,%rbp
	xorq	%rbx,%rdx
	andq	%rbp,%rsi
	shrq	$4,%rbp
	pxor	%xmm1,%xmm0
	movq	(%rsp,%rdi,8),%rcx
	movq	%r8,%rdi
	movq	%rcx,%rbx
	shlq	$28,%rcx
	andq	%rbp,%rdi
	movq	(%rsp,%rsi,8),%xmm1
	shrq	$36,%rbx
	xorq	%rcx,%rax
	pslldq	$4,%xmm1
	movq	%r8,%rsi
	shrq	$4,%rbp
	xorq	%rbx,%rdx
	andq	%rbp,%rsi
	shrq	$4,%rbp
	pxor	%xmm1,%xmm0
	movq	(%rsp,%rdi,8),%rcx
	movq	%r8,%rdi
	movq	%rcx,%rbx
	shlq	$36,%rcx
	andq	%rbp,%rdi
	movq	(%rsp,%rsi,8),%xmm1
	shrq	$28,%rbx
	xorq	%rcx,%rax
	pslldq	$5,%xmm1
	movq	%r8,%rsi
	shrq	$4,%rbp
	xorq	%rbx,%rdx
	andq	%rbp,%rsi
	shrq	$4,%rbp
	pxor	%xmm1,%xmm0
	movq	(%rsp,%rdi,8),%rcx
	movq	%r8,%rdi
	movq	%rcx,%rbx
	shlq	$44,%rcx
	andq	%rbp,%rdi
	movq	(%rsp,%rsi,8),%xmm1
	shrq	$20,%rbx
	xorq	%rcx,%rax
	pslldq	$6,%xmm1
	movq	%r8,%rsi
	shrq	$4,%rbp
	xorq	%rbx,%rdx
	andq	%rbp,%rsi
	shrq	$4,%rbp
	pxor	%xmm1,%xmm0
	movq	(%rsp,%rdi,8),%rcx
	movq	%r8,%rdi
	movq	%rcx,%rbx
	shlq	$52,%rcx
	andq	%rbp,%rdi
	movq	(%rsp,%rsi,8),%xmm1
	shrq	$12,%rbx
	xorq	%rcx,%rax
	pslldq	$7,%xmm1
	movq	%r8,%rsi
	shrq	$4,%rbp
	xorq	%rbx,%rdx
	andq	%rbp,%rsi
	shrq	$4,%rbp
	pxor	%xmm1,%xmm0
	movq	(%rsp,%rdi,8),%rcx
	movq	%rcx,%rbx
	shlq	$60,%rcx
	movd	%xmm0,%rsi
	shrq	$4,%rbx
	xorq	%rcx,%rax
	psrldq	$8,%xmm0
	xorq	%rbx,%rdx
	movd	%xmm0,%rdi
	xorq	%rsi,%rax
	xorq	%rdi,%rdx

	addq	$128+8,%rsp
	retq
.Lend_mul_1x1:



.globl	bn_GF2m_mul_2x2
.def	bn_GF2m_mul_2x2;	.scl 2;	.type 32;	.endef
.p2align	4
bn_GF2m_mul_2x2:
	movl	OPENSSL_ia32cap_P+4(%rip),%eax
	btl	$IA32CAP_BIT1_PCLMUL,%eax
	jnc	.Lvanilla_mul_2x2

	movd	%rdx,%xmm0
	movd	%r9,%xmm1
	movd	%r8,%xmm2
	movq	40(%rsp),%xmm3
	movdqa	%xmm0,%xmm4
	movdqa	%xmm1,%xmm5
.byte	102,15,58,68,193,0
	pxor	%xmm2,%xmm4
	pxor	%xmm3,%xmm5
.byte	102,15,58,68,211,0
.byte	102,15,58,68,229,0
	xorps	%xmm0,%xmm4
	xorps	%xmm2,%xmm4
	movdqa	%xmm4,%xmm5
	pslldq	$8,%xmm4
	psrldq	$8,%xmm5
	pxor	%xmm4,%xmm2
	pxor	%xmm5,%xmm0
	movdqu	%xmm2,0(%rcx)
	movdqu	%xmm0,16(%rcx)
	retq

.p2align	4
.Lvanilla_mul_2x2:
	leaq	-136(%rsp),%rsp
	movq	176(%rsp),%r10
	movq	%rdi,120(%rsp)
	movq	%rsi,128(%rsp)
	movq	%r14,80(%rsp)
	movq	%r13,88(%rsp)
	movq	%r12,96(%rsp)
	movq	%rbp,104(%rsp)
	movq	%rbx,112(%rsp)
.Lbody_mul_2x2:
	movq	%rcx,32(%rsp)
	movq	%rdx,40(%rsp)
	movq	%r8,48(%rsp)
	movq	%r9,56(%rsp)
	movq	%r10,64(%rsp)

	movq	$15,%r8
	movq	%rdx,%rax
	movq	%r9,%rbp
	call	_mul_1x1		
	movq	%rax,16(%rsp)
	movq	%rdx,24(%rsp)

	movq	48(%rsp),%rax
	movq	64(%rsp),%rbp
	call	_mul_1x1		
	movq	%rax,0(%rsp)
	movq	%rdx,8(%rsp)

	movq	40(%rsp),%rax
	movq	56(%rsp),%rbp
	xorq	48(%rsp),%rax
	xorq	64(%rsp),%rbp
	call	_mul_1x1		
	movq	0(%rsp),%rbx
	movq	8(%rsp),%rcx
	movq	16(%rsp),%rdi
	movq	24(%rsp),%rsi
	movq	32(%rsp),%rbp

	xorq	%rdx,%rax
	xorq	%rcx,%rdx
	xorq	%rbx,%rax
	movq	%rbx,0(%rbp)
	xorq	%rdi,%rdx
	movq	%rsi,24(%rbp)
	xorq	%rsi,%rax
	xorq	%rsi,%rdx
	xorq	%rdx,%rax
	movq	%rdx,16(%rbp)
	movq	%rax,8(%rbp)

	movq	80(%rsp),%r14
	movq	88(%rsp),%r13
	movq	96(%rsp),%r12
	movq	104(%rsp),%rbp
	movq	112(%rsp),%rbx
	movq	120(%rsp),%rdi
	movq	128(%rsp),%rsi
	leaq	136(%rsp),%rsp
	retq
.Lend_mul_2x2:

.byte	71,70,40,50,94,109,41,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.p2align	4


.def	se_handler;	.scl 3;	.type 32;	.endef
.p2align	4
se_handler:
	pushq	%rsi
	pushq	%rdi
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	pushfq
	subq	$64,%rsp

	movq	152(%r8),%rax
	movq	248(%r8),%rbx

	leaq	.Lbody_mul_2x2(%rip),%r10
	cmpq	%r10,%rbx
	jb	.Lin_prologue

	movq	80(%rax),%r14
	movq	88(%rax),%r13
	movq	96(%rax),%r12
	movq	104(%rax),%rbp
	movq	112(%rax),%rbx
	movq	120(%rax),%rdi
	movq	128(%rax),%rsi

	movq	%rbx,144(%r8)
	movq	%rbp,160(%r8)
	movq	%rsi,168(%r8)
	movq	%rdi,176(%r8)
	movq	%r12,216(%r8)
	movq	%r13,224(%r8)
	movq	%r14,232(%r8)

.Lin_prologue:
	leaq	136(%rax),%rax
	movq	%rax,152(%r8)

	movq	40(%r9),%rdi
	movq	%r8,%rsi
	movl	$154,%ecx
.long	0xa548f3fc		

	movq	%r9,%rsi
	xorq	%rcx,%rcx
	movq	8(%rsi),%rdx
	movq	0(%rsi),%r8
	movq	16(%rsi),%r9
	movq	40(%rsi),%r10
	leaq	56(%rsi),%r11
	leaq	24(%rsi),%r12
	movq	%r10,32(%rsp)
	movq	%r11,40(%rsp)
	movq	%r12,48(%rsp)
	movq	%rcx,56(%rsp)
	call	*__imp_RtlVirtualUnwind(%rip)

	movl	$1,%eax
	addq	$64,%rsp
	popfq
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	popq	%rdi
	popq	%rsi
	retq


.section	.pdata
.p2align	2
.rva	_mul_1x1
.rva	.Lend_mul_1x1
.rva	.LSEH_info_1x1

.rva	.Lvanilla_mul_2x2
.rva	.Lend_mul_2x2
.rva	.LSEH_info_2x2
.section	.xdata
.p2align	3
.LSEH_info_1x1:
.byte	0x01,0x07,0x02,0x00
.byte	0x07,0x01,0x11,0x00	
.LSEH_info_2x2:
.byte	9,0,0,0
.rva	se_handler
