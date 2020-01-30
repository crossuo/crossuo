#include "x86_arch.h"
.text	

.globl	bn_mul_mont_gather5
.def	bn_mul_mont_gather5;	.scl 2;	.type 32;	.endef
.p2align	6
bn_mul_mont_gather5:
	movq	%rdi,8(%rsp)
	movq	%rsi,16(%rsp)
	movq	%rsp,%rax
.LSEH_begin_bn_mul_mont_gather5:
	movq	%rcx,%rdi
	movq	%rdx,%rsi
	movq	%r8,%rdx
	movq	%r9,%rcx
	movq	40(%rsp),%r8
	movq	48(%rsp),%r9

	testl	$3,%r9d
	jnz	.Lmul_enter
	cmpl	$8,%r9d
	jb	.Lmul_enter
	jmp	.Lmul4x_enter

.p2align	4
.Lmul_enter:
	movl	%r9d,%r9d
	movd	56(%rsp),%xmm5
	leaq	.Linc(%rip),%r10
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

.Lmul_alloca:
	movq	%rsp,%rax
	leaq	2(%r9),%r11
	negq	%r11
	leaq	-264(%rsp,%r11,8),%rsp
	andq	$-1024,%rsp

	movq	%rax,8(%rsp,%r9,8)
.Lmul_body:
	leaq	128(%rdx),%r12
	movdqa	0(%r10),%xmm0
	movdqa	16(%r10),%xmm1
	leaq	24-112(%rsp,%r9,8),%r10
	andq	$-16,%r10

	pshufd	$0,%xmm5,%xmm5
	movdqa	%xmm1,%xmm4
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
.byte	0x67
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,112(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,128(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,144(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,160(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,176(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,192(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,208(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,224(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,240(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,256(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,272(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,288(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,304(%r10)

	paddd	%xmm2,%xmm3
.byte	0x67
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,320(%r10)

	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,336(%r10)
	pand	64(%r12),%xmm0

	pand	80(%r12),%xmm1
	pand	96(%r12),%xmm2
	movdqa	%xmm3,352(%r10)
	pand	112(%r12),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-128(%r12),%xmm4
	movdqa	-112(%r12),%xmm5
	movdqa	-96(%r12),%xmm2
	pand	112(%r10),%xmm4
	movdqa	-80(%r12),%xmm3
	pand	128(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	144(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	160(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-64(%r12),%xmm4
	movdqa	-48(%r12),%xmm5
	movdqa	-32(%r12),%xmm2
	pand	176(%r10),%xmm4
	movdqa	-16(%r12),%xmm3
	pand	192(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	208(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	224(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	0(%r12),%xmm4
	movdqa	16(%r12),%xmm5
	movdqa	32(%r12),%xmm2
	pand	240(%r10),%xmm4
	movdqa	48(%r12),%xmm3
	pand	256(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	272(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	288(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	por	%xmm1,%xmm0
	pshufd	$78,%xmm0,%xmm1
	por	%xmm1,%xmm0
	leaq	256(%r12),%r12
	movd	%xmm0,%rbx

	movq	(%r8),%r8
	movq	(%rsi),%rax

	xorq	%r14,%r14
	xorq	%r15,%r15

	movq	%r8,%rbp
	mulq	%rbx
	movq	%rax,%r10
	movq	(%rcx),%rax

	imulq	%r10,%rbp
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r10
	movq	8(%rsi),%rax
	adcq	$0,%rdx
	movq	%rdx,%r13

	leaq	1(%r15),%r15
	jmp	.L1st_enter

.p2align	4
.L1st:
	addq	%rax,%r13
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%r13
	movq	%r10,%r11
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)
	movq	%rdx,%r13

.L1st_enter:
	mulq	%rbx
	addq	%rax,%r11
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	leaq	1(%r15),%r15
	movq	%rdx,%r10

	mulq	%rbp
	cmpq	%r9,%r15
	jl	.L1st

	addq	%rax,%r13
	movq	(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%r13
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)
	movq	%rdx,%r13
	movq	%r10,%r11

	xorq	%rdx,%rdx
	addq	%r11,%r13
	adcq	$0,%rdx
	movq	%r13,-8(%rsp,%r9,8)
	movq	%rdx,(%rsp,%r9,8)

	leaq	1(%r14),%r14
	jmp	.Louter
.p2align	4
.Louter:
	leaq	24+128(%rsp,%r9,8),%rdx
	andq	$-16,%rdx
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	movdqa	-128(%r12),%xmm0
	movdqa	-112(%r12),%xmm1
	movdqa	-96(%r12),%xmm2
	movdqa	-80(%r12),%xmm3
	pand	-128(%rdx),%xmm0
	pand	-112(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	-96(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	-80(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	-64(%r12),%xmm0
	movdqa	-48(%r12),%xmm1
	movdqa	-32(%r12),%xmm2
	movdqa	-16(%r12),%xmm3
	pand	-64(%rdx),%xmm0
	pand	-48(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	-32(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	-16(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	0(%r12),%xmm0
	movdqa	16(%r12),%xmm1
	movdqa	32(%r12),%xmm2
	movdqa	48(%r12),%xmm3
	pand	0(%rdx),%xmm0
	pand	16(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	32(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	48(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	64(%r12),%xmm0
	movdqa	80(%r12),%xmm1
	movdqa	96(%r12),%xmm2
	movdqa	112(%r12),%xmm3
	pand	64(%rdx),%xmm0
	pand	80(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	96(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	112(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	por	%xmm5,%xmm4
	pshufd	$78,%xmm4,%xmm0
	por	%xmm4,%xmm0
	leaq	256(%r12),%r12
	movd	%xmm0,%rbx

	xorq	%r15,%r15
	movq	%r8,%rbp
	movq	(%rsp),%r10

	mulq	%rbx
	addq	%rax,%r10
	movq	(%rcx),%rax
	adcq	$0,%rdx

	imulq	%r10,%rbp
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r10
	movq	8(%rsi),%rax
	adcq	$0,%rdx
	movq	8(%rsp),%r10
	movq	%rdx,%r13

	leaq	1(%r15),%r15
	jmp	.Linner_enter

.p2align	4
.Linner:
	addq	%rax,%r13
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	movq	(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)
	movq	%rdx,%r13

.Linner_enter:
	mulq	%rbx
	addq	%rax,%r11
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%r10
	movq	%rdx,%r11
	adcq	$0,%r11
	leaq	1(%r15),%r15

	mulq	%rbp
	cmpq	%r9,%r15
	jl	.Linner

	addq	%rax,%r13
	movq	(%rsi),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	movq	(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)
	movq	%rdx,%r13

	xorq	%rdx,%rdx
	addq	%r11,%r13
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%r13,-8(%rsp,%r9,8)
	movq	%rdx,(%rsp,%r9,8)

	leaq	1(%r14),%r14
	cmpq	%r9,%r14
	jl	.Louter

	xorq	%r14,%r14
	movq	(%rsp),%rax
	leaq	(%rsp),%rsi
	movq	%r9,%r15
	jmp	.Lsub
.p2align	4
.Lsub:	sbbq	(%rcx,%r14,8),%rax
	movq	%rax,(%rdi,%r14,8)
	movq	8(%rsi,%r14,8),%rax
	leaq	1(%r14),%r14
	decq	%r15
	jnz	.Lsub

	sbbq	$0,%rax
	xorq	%r14,%r14
	andq	%rax,%rsi
	notq	%rax
	movq	%rdi,%rcx
	andq	%rax,%rcx
	movq	%r9,%r15
	orq	%rcx,%rsi
.p2align	4
.Lcopy:
	movq	(%rsi,%r14,8),%rax
	movq	%r14,(%rsp,%r14,8)
	movq	%rax,(%rdi,%r14,8)
	leaq	1(%r14),%r14
	subq	$1,%r15
	jnz	.Lcopy

	movq	8(%rsp,%r9,8),%rsi
	movq	$1,%rax

	movq	(%rsi),%r15
	movq	8(%rsi),%r14
	movq	16(%rsi),%r13
	movq	24(%rsi),%r12
	movq	32(%rsi),%rbp
	movq	40(%rsi),%rbx
	leaq	48(%rsi),%rsp
.Lmul_epilogue:
	movq	8(%rsp),%rdi
	movq	16(%rsp),%rsi
	retq
.LSEH_end_bn_mul_mont_gather5:
.def	bn_mul4x_mont_gather5;	.scl 3;	.type 32;	.endef
.p2align	4
bn_mul4x_mont_gather5:
	movq	%rdi,8(%rsp)
	movq	%rsi,16(%rsp)
	movq	%rsp,%rax
.LSEH_begin_bn_mul4x_mont_gather5:
	movq	%rcx,%rdi
	movq	%rdx,%rsi
	movq	%r8,%rdx
	movq	%r9,%rcx
	movq	40(%rsp),%r8
	movq	48(%rsp),%r9

.Lmul4x_enter:
	movl	%r9d,%r9d
	movd	56(%rsp),%xmm5
	leaq	.Linc(%rip),%r10
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

.Lmul4x_alloca:
	movq	%rsp,%rax
	leaq	4(%r9),%r11
	negq	%r11
	leaq	-256(%rsp,%r11,8),%rsp
	andq	$-1024,%rsp

	movq	%rax,8(%rsp,%r9,8)
.Lmul4x_body:
	movq	%rdi,16(%rsp,%r9,8)
	leaq	128(%rdx),%r12
	movdqa	0(%r10),%xmm0
	movdqa	16(%r10),%xmm1
	leaq	32-112(%rsp,%r9,8),%r10

	pshufd	$0,%xmm5,%xmm5
	movdqa	%xmm1,%xmm4
.byte	0x67,0x67
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
.byte	0x67
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,112(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,128(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,144(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,160(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,176(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,192(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,208(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,224(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,240(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,256(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,272(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,288(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,304(%r10)

	paddd	%xmm2,%xmm3
.byte	0x67
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,320(%r10)

	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,336(%r10)
	pand	64(%r12),%xmm0

	pand	80(%r12),%xmm1
	pand	96(%r12),%xmm2
	movdqa	%xmm3,352(%r10)
	pand	112(%r12),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-128(%r12),%xmm4
	movdqa	-112(%r12),%xmm5
	movdqa	-96(%r12),%xmm2
	pand	112(%r10),%xmm4
	movdqa	-80(%r12),%xmm3
	pand	128(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	144(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	160(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-64(%r12),%xmm4
	movdqa	-48(%r12),%xmm5
	movdqa	-32(%r12),%xmm2
	pand	176(%r10),%xmm4
	movdqa	-16(%r12),%xmm3
	pand	192(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	208(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	224(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	0(%r12),%xmm4
	movdqa	16(%r12),%xmm5
	movdqa	32(%r12),%xmm2
	pand	240(%r10),%xmm4
	movdqa	48(%r12),%xmm3
	pand	256(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	272(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	288(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	por	%xmm1,%xmm0
	pshufd	$78,%xmm0,%xmm1
	por	%xmm1,%xmm0
	leaq	256(%r12),%r12
	movd	%xmm0,%rbx

	movq	(%r8),%r8
	movq	(%rsi),%rax

	xorq	%r14,%r14
	xorq	%r15,%r15

	movq	%r8,%rbp
	mulq	%rbx
	movq	%rax,%r10
	movq	(%rcx),%rax

	imulq	%r10,%rbp
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r10
	movq	8(%rsi),%rax
	adcq	$0,%rdx
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	8(%rcx),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	16(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	leaq	4(%r15),%r15
	adcq	$0,%rdx
	movq	%rdi,(%rsp)
	movq	%rdx,%r13
	jmp	.L1st4x
.p2align	4
.L1st4x:
	mulq	%rbx
	addq	%rax,%r10
	movq	-16(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r13
	movq	-8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%r13,-24(%rsp,%r15,8)
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	-8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%rdi,-16(%rsp,%r15,8)
	movq	%rdx,%r13

	mulq	%rbx
	addq	%rax,%r10
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r13
	movq	8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%r13,-8(%rsp,%r15,8)
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	leaq	4(%r15),%r15
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	-16(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%rdi,-32(%rsp,%r15,8)
	movq	%rdx,%r13
	cmpq	%r9,%r15
	jl	.L1st4x

	mulq	%rbx
	addq	%rax,%r10
	movq	-16(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r13
	movq	-8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%r13,-24(%rsp,%r15,8)
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	-8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%rdi,-16(%rsp,%r15,8)
	movq	%rdx,%r13

	xorq	%rdi,%rdi
	addq	%r10,%r13
	adcq	$0,%rdi
	movq	%r13,-8(%rsp,%r15,8)
	movq	%rdi,(%rsp,%r15,8)

	leaq	1(%r14),%r14
.p2align	2
.Louter4x:
	leaq	32+128(%rsp,%r9,8),%rdx
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	movdqa	-128(%r12),%xmm0
	movdqa	-112(%r12),%xmm1
	movdqa	-96(%r12),%xmm2
	movdqa	-80(%r12),%xmm3
	pand	-128(%rdx),%xmm0
	pand	-112(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	-96(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	-80(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	-64(%r12),%xmm0
	movdqa	-48(%r12),%xmm1
	movdqa	-32(%r12),%xmm2
	movdqa	-16(%r12),%xmm3
	pand	-64(%rdx),%xmm0
	pand	-48(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	-32(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	-16(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	0(%r12),%xmm0
	movdqa	16(%r12),%xmm1
	movdqa	32(%r12),%xmm2
	movdqa	48(%r12),%xmm3
	pand	0(%rdx),%xmm0
	pand	16(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	32(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	48(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	64(%r12),%xmm0
	movdqa	80(%r12),%xmm1
	movdqa	96(%r12),%xmm2
	movdqa	112(%r12),%xmm3
	pand	64(%rdx),%xmm0
	pand	80(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	96(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	112(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	por	%xmm5,%xmm4
	pshufd	$78,%xmm4,%xmm0
	por	%xmm4,%xmm0
	leaq	256(%r12),%r12
	movd	%xmm0,%rbx

	xorq	%r15,%r15

	movq	(%rsp),%r10
	movq	%r8,%rbp
	mulq	%rbx
	addq	%rax,%r10
	movq	(%rcx),%rax
	adcq	$0,%rdx

	imulq	%r10,%rbp
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r10
	movq	8(%rsi),%rax
	adcq	$0,%rdx
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	8(%rcx),%rax
	adcq	$0,%rdx
	addq	8(%rsp),%r11
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	16(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	leaq	4(%r15),%r15
	adcq	$0,%rdx
	movq	%rdx,%r13
	jmp	.Linner4x
.p2align	4
.Linner4x:
	mulq	%rbx
	addq	%rax,%r10
	movq	-16(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	-16(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r13
	movq	-8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%rdi,-32(%rsp,%r15,8)
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	-8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	-8(%rsp,%r15,8),%r11
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%r13,-24(%rsp,%r15,8)
	movq	%rdx,%r13

	mulq	%rbx
	addq	%rax,%r10
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r13
	movq	8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%rdi,-16(%rsp,%r15,8)
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	8(%rsp,%r15,8),%r11
	adcq	$0,%rdx
	leaq	4(%r15),%r15
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	-16(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%r13,-40(%rsp,%r15,8)
	movq	%rdx,%r13
	cmpq	%r9,%r15
	jl	.Linner4x

	mulq	%rbx
	addq	%rax,%r10
	movq	-16(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	-16(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r13
	movq	-8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%rdi,-32(%rsp,%r15,8)
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	-8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	-8(%rsp,%r15,8),%r11
	adcq	$0,%rdx
	leaq	1(%r14),%r14
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%r13,-24(%rsp,%r15,8)
	movq	%rdx,%r13

	movq	%rdi,-16(%rsp,%r15,8)

	xorq	%rdi,%rdi
	addq	%r10,%r13
	adcq	$0,%rdi
	addq	(%rsp,%r9,8),%r13
	adcq	$0,%rdi
	movq	%r13,-8(%rsp,%r15,8)
	movq	%rdi,(%rsp,%r15,8)

	cmpq	%r9,%r14
	jl	.Louter4x
	movq	16(%rsp,%r9,8),%rdi
	movq	0(%rsp),%rax
	pxor	%xmm0,%xmm0
	movq	8(%rsp),%rdx
	shrq	$2,%r9
	leaq	(%rsp),%rsi
	xorq	%r14,%r14

	subq	0(%rcx),%rax
	movq	16(%rsi),%rbx
	movq	24(%rsi),%rbp
	sbbq	8(%rcx),%rdx
	leaq	-1(%r9),%r15
	jmp	.Lsub4x
.p2align	4
.Lsub4x:
	movq	%rax,0(%rdi,%r14,8)
	movq	%rdx,8(%rdi,%r14,8)
	sbbq	16(%rcx,%r14,8),%rbx
	movq	32(%rsi,%r14,8),%rax
	movq	40(%rsi,%r14,8),%rdx
	sbbq	24(%rcx,%r14,8),%rbp
	movq	%rbx,16(%rdi,%r14,8)
	movq	%rbp,24(%rdi,%r14,8)
	sbbq	32(%rcx,%r14,8),%rax
	movq	48(%rsi,%r14,8),%rbx
	movq	56(%rsi,%r14,8),%rbp
	sbbq	40(%rcx,%r14,8),%rdx
	leaq	4(%r14),%r14
	decq	%r15
	jnz	.Lsub4x

	movq	%rax,0(%rdi,%r14,8)
	movq	32(%rsi,%r14,8),%rax
	sbbq	16(%rcx,%r14,8),%rbx
	movq	%rdx,8(%rdi,%r14,8)
	sbbq	24(%rcx,%r14,8),%rbp
	movq	%rbx,16(%rdi,%r14,8)

	sbbq	$0,%rax
	movq	%rbp,24(%rdi,%r14,8)
	xorq	%r14,%r14
	andq	%rax,%rsi
	notq	%rax
	movq	%rdi,%rcx
	andq	%rax,%rcx
	leaq	-1(%r9),%r15
	orq	%rcx,%rsi

	movdqu	(%rsi),%xmm1
	movdqa	%xmm0,(%rsp)
	movdqu	%xmm1,(%rdi)
	jmp	.Lcopy4x
.p2align	4
.Lcopy4x:
	movdqu	16(%rsi,%r14,1),%xmm2
	movdqu	32(%rsi,%r14,1),%xmm1
	movdqa	%xmm0,16(%rsp,%r14,1)
	movdqu	%xmm2,16(%rdi,%r14,1)
	movdqa	%xmm0,32(%rsp,%r14,1)
	movdqu	%xmm1,32(%rdi,%r14,1)
	leaq	32(%r14),%r14
	decq	%r15
	jnz	.Lcopy4x

	shlq	$2,%r9
	movdqu	16(%rsi,%r14,1),%xmm2
	movdqa	%xmm0,16(%rsp,%r14,1)
	movdqu	%xmm2,16(%rdi,%r14,1)
	movq	8(%rsp,%r9,8),%rsi
	movq	$1,%rax

	movq	(%rsi),%r15
	movq	8(%rsi),%r14
	movq	16(%rsi),%r13
	movq	24(%rsi),%r12
	movq	32(%rsi),%rbp
	movq	40(%rsi),%rbx
	leaq	48(%rsi),%rsp
.Lmul4x_epilogue:
	movq	8(%rsp),%rdi
	movq	16(%rsp),%rsi
	retq
.LSEH_end_bn_mul4x_mont_gather5:
.globl	bn_scatter5
.def	bn_scatter5;	.scl 2;	.type 32;	.endef
.p2align	4
bn_scatter5:
	cmpq	$0,%rdx
	jz	.Lscatter_epilogue
	leaq	(%r8,%r9,8),%r8
.Lscatter:
	movq	(%rcx),%rax
	leaq	8(%rcx),%rcx
	movq	%rax,(%r8)
	leaq	256(%r8),%r8
	subq	$1,%rdx
	jnz	.Lscatter
.Lscatter_epilogue:
	retq


.globl	bn_gather5
.def	bn_gather5;	.scl 2;	.type 32;	.endef
.p2align	4
bn_gather5:
.LSEH_begin_bn_gather5:

.byte	0x4c,0x8d,0x14,0x24			
.byte	0x48,0x81,0xec,0x08,0x01,0x00,0x00	
	leaq	.Linc(%rip),%rax
	andq	$-16,%rsp

	movd	%r9d,%xmm5
	movdqa	0(%rax),%xmm0
	movdqa	16(%rax),%xmm1
	leaq	128(%r8),%r11
	leaq	128(%rsp),%rax

	pshufd	$0,%xmm5,%xmm5
	movdqa	%xmm1,%xmm4
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm4,%xmm3

	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,-128(%rax)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,-112(%rax)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,-96(%rax)
	movdqa	%xmm4,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,-80(%rax)
	movdqa	%xmm4,%xmm3

	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,-64(%rax)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,-48(%rax)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,-32(%rax)
	movdqa	%xmm4,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,-16(%rax)
	movdqa	%xmm4,%xmm3

	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,0(%rax)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,16(%rax)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,32(%rax)
	movdqa	%xmm4,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,48(%rax)
	movdqa	%xmm4,%xmm3

	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,64(%rax)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,80(%rax)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,96(%rax)
	movdqa	%xmm4,%xmm2
	movdqa	%xmm3,112(%rax)
	jmp	.Lgather

.p2align	5
.Lgather:
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	movdqa	-128(%r11),%xmm0
	movdqa	-112(%r11),%xmm1
	movdqa	-96(%r11),%xmm2
	pand	-128(%rax),%xmm0
	movdqa	-80(%r11),%xmm3
	pand	-112(%rax),%xmm1
	por	%xmm0,%xmm4
	pand	-96(%rax),%xmm2
	por	%xmm1,%xmm5
	pand	-80(%rax),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	-64(%r11),%xmm0
	movdqa	-48(%r11),%xmm1
	movdqa	-32(%r11),%xmm2
	pand	-64(%rax),%xmm0
	movdqa	-16(%r11),%xmm3
	pand	-48(%rax),%xmm1
	por	%xmm0,%xmm4
	pand	-32(%rax),%xmm2
	por	%xmm1,%xmm5
	pand	-16(%rax),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	0(%r11),%xmm0
	movdqa	16(%r11),%xmm1
	movdqa	32(%r11),%xmm2
	pand	0(%rax),%xmm0
	movdqa	48(%r11),%xmm3
	pand	16(%rax),%xmm1
	por	%xmm0,%xmm4
	pand	32(%rax),%xmm2
	por	%xmm1,%xmm5
	pand	48(%rax),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	64(%r11),%xmm0
	movdqa	80(%r11),%xmm1
	movdqa	96(%r11),%xmm2
	pand	64(%rax),%xmm0
	movdqa	112(%r11),%xmm3
	pand	80(%rax),%xmm1
	por	%xmm0,%xmm4
	pand	96(%rax),%xmm2
	por	%xmm1,%xmm5
	pand	112(%rax),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	por	%xmm5,%xmm4
	leaq	256(%r11),%r11
	pshufd	$78,%xmm4,%xmm0
	por	%xmm4,%xmm0
	movq	%xmm0,(%rcx)
	leaq	8(%rcx),%rcx
	subq	$1,%rdx
	jnz	.Lgather

	leaq	(%r10),%rsp
	retq
.LSEH_end_bn_gather5:

.p2align	6
.Linc:
.long	0,0, 1,1
.long	2,2, 2,2
.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,119,105,116,104,32,115,99,97,116,116,101,114,47,103,97,116,104,101,114,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0

.def	mul_handler;	.scl 3;	.type 32;	.endef
.p2align	4
mul_handler:
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

	movq	120(%r8),%rax
	movq	248(%r8),%rbx

	movq	8(%r9),%rsi
	movq	56(%r9),%r11

	movl	0(%r11),%r10d
	leaq	(%rsi,%r10,1),%r10
	cmpq	%r10,%rbx
	jb	.Lcommon_seh_tail

	leaq	48(%rax),%rax

	movl	4(%r11),%r10d
	leaq	(%rsi,%r10,1),%r10
	cmpq	%r10,%rbx
	jb	.Lcommon_seh_tail

	movq	152(%r8),%rax

	movl	8(%r11),%r10d
	leaq	(%rsi,%r10,1),%r10
	cmpq	%r10,%rbx
	jae	.Lcommon_seh_tail

	movq	192(%r8),%r10
	movq	8(%rax,%r10,8),%rax

	leaq	48(%rax),%rax

	movq	-8(%rax),%rbx
	movq	-16(%rax),%rbp
	movq	-24(%rax),%r12
	movq	-32(%rax),%r13
	movq	-40(%rax),%r14
	movq	-48(%rax),%r15
	movq	%rbx,144(%r8)
	movq	%rbp,160(%r8)
	movq	%r12,216(%r8)
	movq	%r13,224(%r8)
	movq	%r14,232(%r8)
	movq	%r15,240(%r8)

.Lcommon_seh_tail:
	movq	8(%rax),%rdi
	movq	16(%rax),%rsi
	movq	%rax,152(%r8)
	movq	%rsi,168(%r8)
	movq	%rdi,176(%r8)

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
.rva	.LSEH_begin_bn_mul_mont_gather5
.rva	.LSEH_end_bn_mul_mont_gather5
.rva	.LSEH_info_bn_mul_mont_gather5

.rva	.LSEH_begin_bn_mul4x_mont_gather5
.rva	.LSEH_end_bn_mul4x_mont_gather5
.rva	.LSEH_info_bn_mul4x_mont_gather5

.rva	.LSEH_begin_bn_gather5
.rva	.LSEH_end_bn_gather5
.rva	.LSEH_info_bn_gather5

.section	.xdata
.p2align	3
.LSEH_info_bn_mul_mont_gather5:
.byte	9,0,0,0
.rva	mul_handler
.rva	.Lmul_alloca,.Lmul_body,.Lmul_epilogue		
.p2align	3
.LSEH_info_bn_mul4x_mont_gather5:
.byte	9,0,0,0
.rva	mul_handler
.rva	.Lmul4x_alloca,.Lmul4x_body,.Lmul4x_epilogue	
.p2align	3
.LSEH_info_bn_gather5:
.byte	0x01,0x0b,0x03,0x0a
.byte	0x0b,0x01,0x21,0x00	
.byte	0x04,0xa3,0x00,0x00	
.p2align	3
