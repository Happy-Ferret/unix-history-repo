/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * Copyright (c) 1989, 1990 William Jolitz
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, and William Jolitz.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)vm_machdep.c	7.6 (Berkeley) %G%
 *	Utah $Hdr: vm_machdep.c 1.16.1.1 89/06/23$
 */

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "malloc.h"
#include "buf.h"
#include "user.h"

#include "../include/cpu.h"

#include "vm/vm.h"
#include "vm/vm_kern.h"

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the kernel stack and pcb, making the child
 * ready to run, and marking it so that it can return differently
 * than the parent.  Returns 1 in the child process, 0 in the parent.
 * We currently double-map the user area so that the stack is at the same
 * address in each process; in the future we will probably relocate
 * the frame pointers on the stack after copying.
 */
cpu_fork(p1, p2)
	register struct proc *p1, *p2;
{
	register struct user *up = p2->p_addr;
	int foo, offset, addr, i;
	extern char kstack[];
	extern int mvesp();

	/*
	 * Copy pcb and stack from proc p1 to p2. 
	 * We do this as cheaply as possible, copying only the active
	 * part of the stack.  The stack and pcb need to agree;
	 * this is tricky, as the final pcb is constructed by savectx,
	 * but its frame isn't yet on the stack when the stack is copied.
	 * swtch compensates for this when the child eventually runs.
	 * This should be done differently, with a single call
	 * that copies and updates the pcb+stack,
	 * replacing the bcopy and savectx.
	 */
	p2->p_addr->u_pcb = p1->p_addr->u_pcb;
	offset = mvesp() - (int)kstack;
	bcopy((caddr_t)kstack + offset, (caddr_t)p2->p_addr + offset,
	    (unsigned) ctob(UPAGES) - offset);
	p2->p_regs = p1->p_regs;

	/*
	 * Wire top of address space of child to it's u.
	 * First, fault in a page of pte's to map it.
	 */
        addr = trunc_page((u_int)vtopte(kstack));
	(void)vm_map_pageable(&p2->p_vmspace->vm_map, addr, addr+NBPG, FALSE);
	for (i=0; i < UPAGES; i++)
		pmap_enter(&p2->p_vmspace->vm_pmap, kstack+i*NBPG,
			pmap_extract(kernel_pmap, ((int)p2->p_addr)+i*NBPG),
			VM_PROT_READ, 1);
	pmap_activate(&p2->p_vmspace->vm_pmap, &up->u_pcb);

	/*
	 * 
	 * Arrange for a non-local goto when the new process
	 * is started, to resume here, returning nonzero from setjmp.
	 */
	if (savectx(up, 1)) {
		/*
		 * Return 1 in child.
		 */
		return (1);
	}
	return (0);
}

#if NNPX > 0
extern struct proc *npxproc;
#endif

#ifdef notyet
/*
 * cpu_exit is called as the last action during exit.
 *
 * We change to an inactive address space and a "safe" stack,
 * passing thru an argument to the new stack. Now, safely isolated
 * from the resources we're shedding, we release the address space
 * and any remaining machine-dependent resources, including the
 * memory for the user structure and kernel stack.
 *
 * Next, we assign a dummy context to be written over by swtch,
 * calling it to send this process off to oblivion.
 * [The nullpcb allows us to minimize cost in swtch() by not having
 * a special case].
 */
struct proc *swtch_to_inactive();
cpu_exit(p)
	register struct proc *p;
{
	static struct pcb nullpcb;	/* pcb to overwrite on last swtch */

#if NNPX > 0
	/* free cporcessor (if we have it) */
	if( p == npxproc) npxproc =0;
#endif

	/* move to inactive space and stack, passing arg accross */
	p = swtch_to_inactive(p);

	/* drop per-process resources */
	vmspace_free(p->p_vmspace);
	kmem_free(kernel_map, (vm_offset_t)p->p_addr, ctob(UPAGES));

	p->p_addr = (struct user *) &nullpcb;
	swtch();
	/* NOTREACHED */
}
#else
cpu_exit(p)
	register struct proc *p;
{
	
	/* free coprocessor (if we have it) */
#if NNPX > 0
	if( p == npxproc) npxproc =0;
#endif

	curproc = p;
	swtch();
}

cpu_wait(p) struct proc *p; {

	/* drop per-process resources */
	vmspace_free(p->p_vmspace);
	kmem_free(kernel_map, (vm_offset_t)p->p_addr, ctob(UPAGES));
}
#endif

/*
 * Set a red zone in the kernel stack after the u. area.
 */
setredzone(pte, vaddr)
	u_short *pte;
	caddr_t vaddr;
{
/* eventually do this by setting up an expand-down stack segment
   for ss0: selector, allowing stack access down to top of u.
   this means though that protection violations need to be handled
   thru a double fault exception that must do an integral task
   switch to a known good context, within which a dump can be
   taken. a sensible scheme might be to save the initial context
   used by sched (that has physical memory mapped 1:1 at bottom)
   and take the dump while still in mapped mode */
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of CLSIZE.
 */
pagemove(from, to, size)
	register caddr_t from, to;
	int size;
{
	register struct pte *fpte, *tpte;

	if (size % CLBYTES)
		panic("pagemove");
	fpte = kvtopte(from);
	tpte = kvtopte(to);
	while (size > 0) {
		*tpte++ = *fpte;
		*(int *)fpte++ = 0;
		from += NBPG;
		to += NBPG;
		size -= NBPG;
	}
	tlbflush();
}

/*
 * Convert kernel VA to physical address
 */
kvtop(addr)
	register caddr_t addr;
{
	vm_offset_t va;

	va = pmap_extract(kernel_pmap, (vm_offset_t)addr);
	if (va == 0)
		panic("kvtop: zero page frame");
	return((int)va);
}

#ifdef notdef
/*
 * The probe[rw] routines should probably be redone in assembler
 * for efficiency.
 */
prober(addr)
	register u_int addr;
{
	register int page;
	register struct proc *p;

	if (addr >= USRSTACK)
		return(0);
	p = u.u_procp;
	page = btop(addr);
	if (page < dptov(p, p->p_dsize) || page > sptov(p, p->p_ssize))
		return(1);
	return(0);
}

probew(addr)
	register u_int addr;
{
	register int page;
	register struct proc *p;

	if (addr >= USRSTACK)
		return(0);
	p = u.u_procp;
	page = btop(addr);
	if (page < dptov(p, p->p_dsize) || page > sptov(p, p->p_ssize))
		return((*(int *)vtopte(p, page) & PG_PROT) == PG_UW);
	return(0);
}

/*
 * NB: assumes a physically contiguous kernel page table
 *     (makes life a LOT simpler).
 */
kernacc(addr, count, rw)
	register u_int addr;
	int count, rw;
{
	register struct pde *pde;
	register struct pte *pte;
	register int ix, cnt;
	extern long Syssize;

	if (count <= 0)
		return(0);
	pde = (struct pde *)((u_int)u.u_procp->p_p0br + u.u_procp->p_szpt * NBPG);
	ix = (addr & PD_MASK) >> PD_SHIFT;
	cnt = ((addr + count + (1 << PD_SHIFT) - 1) & PD_MASK) >> PD_SHIFT;
	cnt -= ix;
	for (pde += ix; cnt; cnt--, pde++)
		if (pde->pd_v == 0)
			return(0);
	ix = btop(addr-0xfe000000);
	cnt = btop(addr-0xfe000000+count+NBPG-1);
	if (cnt > (int)&Syssize)
		return(0);
	cnt -= ix;
	for (pte = &Sysmap[ix]; cnt; cnt--, pte++)
		if (pte->pg_v == 0 /*|| (rw == B_WRITE && pte->pg_prot == 1)*/) 
			return(0);
	return(1);
}

useracc(addr, count, rw)
	register u_int addr;
	int count, rw;
{
	register int (*func)();
	register u_int addr2;
	extern int prober(), probew();

	if (count <= 0)
		return(0);
	addr2 = addr;
	addr += count;
	func = (rw == B_READ) ? prober : probew;
	do {
		if ((*func)(addr2) == 0)
			return(0);
		addr2 = (addr2 + NBPG) & ~PGOFSET;
	} while (addr2 < addr);
	return(1);
}
#endif

extern vm_map_t phys_map;

/*
 * Map an IO request into kernel virtual address space.  Requests fall into
 * one of five catagories:
 *
 *	B_PHYS|B_UAREA:	User u-area swap.
 *			Address is relative to start of u-area (p_addr).
 *	B_PHYS|B_PAGET:	User page table swap.
 *			Address is a kernel VA in usrpt (Usrptmap).
 *	B_PHYS|B_DIRTY:	Dirty page push.
 *			Address is a VA in proc2's address space.
 *	B_PHYS|B_PGIN:	Kernel pagein of user pages.
 *			Address is VA in user's address space.
 *	B_PHYS:		User "raw" IO request.
 *			Address is VA in user's address space.
 *
 * All requests are (re)mapped into kernel VA space via the useriomap
 * (a name with only slightly more meaning than "kernelmap")
 */
vmapbuf(bp)
	register struct buf *bp;
{
	register int npf;
	register caddr_t addr;
	register long flags = bp->b_flags;
	struct proc *p;
	int off;
	vm_offset_t kva;
	register vm_offset_t pa;

	if ((flags & B_PHYS) == 0)
		panic("vmapbuf");
	addr = bp->b_saveaddr = bp->b_un.b_addr;
	off = (int)addr & PGOFSET;
	p = bp->b_proc;
	npf = btoc(round_page(bp->b_bcount + off));
	kva = kmem_alloc_wait(phys_map, ctob(npf));
	bp->b_un.b_addr = (caddr_t) (kva + off);
	while (npf--) {
		pa = pmap_extract(&p->p_vmspace->vm_pmap, (vm_offset_t)addr);
		if (pa == 0)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), kva, trunc_page(pa),
			   VM_PROT_READ|VM_PROT_WRITE, TRUE);
		addr += PAGE_SIZE;
		kva += PAGE_SIZE;
	}
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 */
vunmapbuf(bp)
	register struct buf *bp;
{
	register int npf;
	register caddr_t addr = bp->b_un.b_addr;
	vm_offset_t kva;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	npf = btoc(round_page(bp->b_bcount + ((int)addr & PGOFSET)));
	kva = (vm_offset_t)((int)addr & ~PGOFSET);
	kmem_free_wakeup(phys_map, kva, ctob(npf));
	bp->b_un.b_addr = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}
