/*
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from:	@(#)pmap.c	7.7 (Berkeley)	5/12/91
 *	$Id: pmap.c,v 1.75 1996/02/04 21:20:35 davidg Exp $
 */

/*
 * Derived from hp300 version by Mike Hibler, this version by William
 * Jolitz uses a recursive map [a pde points to the page directory] to
 * map the page tables using the pagetables themselves. This is done to
 * reduce the impact on kernel virtual memory for lots of sparse address
 * space, and to reduce the cost of memory to each process.
 *
 *	Derived from: hp300/@(#)pmap.c	7.1 (Berkeley) 12/5/90
 */
/*
 * Major modifications by John S. Dyson primarily to support
 * pageable page tables, eliminating pmap_attributes,
 * discontiguous memory pages, and using more efficient string
 * instructions. Jan 13, 1994.  Further modifications on Mar 2, 1994,
 * general clean-up and efficiency mods.
 */

/*
 *	Manages physical address maps.
 *
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.  These pseudo-maps are
 *	used to store intermediate results from copy
 *	operations to and from address spaces.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/queue.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <machine/pcb.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>

#include <i386/isa/isa.h>

#define PMAP_KEEP_PDIRS

static void	init_pv_entries __P((int));

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define	pmap_pde(m, v)	(&((m)->pm_pdir[((vm_offset_t)(v) >> PD_SHIFT)&1023]))
#define pdir_pde(m, v) (m[((vm_offset_t)(v) >> PD_SHIFT)&1023])

#define pmap_pte_pa(pte)	(*(int *)(pte) & PG_FRAME)

#define pmap_pde_v(pte)		((*(int *)pte & PG_V) != 0)
#define pmap_pte_w(pte)		((*(int *)pte & PG_W) != 0)
#define pmap_pte_m(pte)		((*(int *)pte & PG_M) != 0)
#define pmap_pte_u(pte)		((*(int *)pte & PG_U) != 0)
#define pmap_pte_v(pte)		((*(int *)pte & PG_V) != 0)

#define pmap_pte_set_w(pte, v)		((v)?(*(int *)pte |= PG_W):(*(int *)pte &= ~PG_W))
#define pmap_pte_set_prot(pte, v)	((*(int *)pte &= ~PG_PROT), (*(int *)pte |= (v)))

/*
 * Given a map and a machine independent protection code,
 * convert to a vax protection code.
 */
#define pte_prot(m, p)	(protection_codes[p])
static int protection_codes[8];

static struct pmap kernel_pmap_store;
pmap_t kernel_pmap;

vm_offset_t avail_start;	/* PA of first available physical page */
vm_offset_t avail_end;		/* PA of last available physical page */
vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */
static boolean_t pmap_initialized = FALSE;	/* Has pmap_init completed? */
static vm_offset_t vm_first_phys;

static int nkpt;

extern vm_offset_t clean_sva, clean_eva;
extern int cpu_class;

/*
 * All those kernel PT submaps that BSD is so fond of
 */
pt_entry_t *CMAP1;
static pt_entry_t *CMAP2, *ptmmap;
static pv_entry_t pv_table;
caddr_t CADDR1, ptvmmap;
static caddr_t CADDR2;
static pt_entry_t *msgbufmap;
struct msgbuf *msgbufp;

static void	free_pv_entry __P((pv_entry_t pv));
static pt_entry_t *
		get_pt_entry __P((pmap_t pmap));
static pv_entry_t
		get_pv_entry __P((void));
static void	i386_protection_init __P((void));
static void	pmap_alloc_pv_entry __P((void));
static void	pmap_changebit __P((vm_offset_t pa, int bit, boolean_t setem));
static void	pmap_enter_quick __P((pmap_t pmap, vm_offset_t va,
				      vm_offset_t pa));
static int	pmap_is_managed __P((vm_offset_t pa));
static void	pmap_remove_all __P((vm_offset_t pa));
static void	pmap_remove_entry __P((struct pmap *pmap, pv_entry_t pv,
				       vm_offset_t va));
static vm_page_t
		pmap_pte_vm_page __P((pmap_t pmap, vm_offset_t pt));
static boolean_t
		pmap_testbit __P((vm_offset_t pa, int bit));
static void *	pmap_getpdir __P((void));
void	pmap_prefault __P((pmap_t pmap, vm_offset_t addra,
				   vm_map_entry_t entry, vm_object_t object));

/*
 * The below are finer grained pmap_update routines.  These eliminate
 * the gratuitious tlb flushes on non-i386 architectures.
 */
static __inline void
pmap_update_1pg( vm_offset_t va) {
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386)
		pmap_update();
	else
#endif
		__asm __volatile(".byte 0xf,0x1,0x38": :"a" (va));
}

static __inline void
pmap_update_2pg( vm_offset_t va1, vm_offset_t va2) {
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386) {
		pmap_update();
	} else
#endif
	{
		__asm __volatile(".byte 0xf,0x1,0x38": :"a" (va1));
		__asm __volatile(".byte 0xf,0x1,0x38": :"a" (va2));
	}
}

/*
 *	Routine:	pmap_pte
 *	Function:
 *		Extract the page table entry associated
 *		with the given map/virtual_address pair.
 * [ what about induced faults -wfj]
 */

__inline pt_entry_t * __pure
pmap_pte(pmap, va)
	register pmap_t pmap;
	vm_offset_t va;
{

	if (pmap && *pmap_pde(pmap, va)) {
		vm_offset_t frame = (int) pmap->pm_pdir[PTDPTDI] & PG_FRAME;

		/* are we current address space or kernel? */
		if ((pmap == kernel_pmap) || (frame == ((int) PTDpde & PG_FRAME)))
			return ((pt_entry_t *) vtopte(va));
		/* otherwise, we are alternate address space */
		else {
			if (frame != ((int) APTDpde & PG_FRAME)) {
				APTDpde = pmap->pm_pdir[PTDPTDI];
				pmap_update();
			}
			return ((pt_entry_t *) avtopte(va));
		}
	}
	return (0);
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */

vm_offset_t
pmap_extract(pmap, va)
	register pmap_t pmap;
	vm_offset_t va;
{
	vm_offset_t pa;

	if (pmap && *pmap_pde(pmap, va)) {
		vm_offset_t frame = (int) pmap->pm_pdir[PTDPTDI] & PG_FRAME;

		/* are we current address space or kernel? */
		if ((pmap == kernel_pmap)
		    || (frame == ((int) PTDpde & PG_FRAME))) {
			pa = *(int *) vtopte(va);
			/* otherwise, we are alternate address space */
		} else {
			if (frame != ((int) APTDpde & PG_FRAME)) {
				APTDpde = pmap->pm_pdir[PTDPTDI];
				pmap_update();
			}
			pa = *(int *) avtopte(va);
		}
		return ((pa & PG_FRAME) | (va & ~PG_FRAME));
	}
	return 0;

}

/*
 * determine if a page is managed (memory vs. device)
 */
static __inline int
pmap_is_managed(pa)
	vm_offset_t pa;
{
	int i;

	if (!pmap_initialized)
		return 0;

	for (i = 0; phys_avail[i + 1]; i += 2) {
		if (pa >= phys_avail[i] && pa < phys_avail[i + 1])
			return 1;
	}
	return 0;
}

/*
 * find the vm_page_t of a pte (only) given va of pte and pmap
 */
static __inline vm_page_t
pmap_pte_vm_page(pmap, pt)
	pmap_t pmap;
	vm_offset_t pt;
{
	vm_page_t m;

	pt = trunc_page(pt);
	pt = (pt - UPT_MIN_ADDRESS) / PAGE_SIZE;
	pt = ((vm_offset_t) pmap->pm_pdir[pt]) & PG_FRAME;
	m = PHYS_TO_VM_PAGE(pt);
	return m;
}

/*
 * Wire a page table page
 */
__inline vm_page_t
pmap_use_pt(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	vm_offset_t pt;
	vm_page_t m;

	if ((va >= UPT_MIN_ADDRESS) || !pmap_initialized)
		return NULL;

	pt = (vm_offset_t) vtopte(va);
	m = pmap_pte_vm_page(pmap, pt);
	vm_page_hold(m);
	return m;
}

/*
 * Unwire a page table page
 */
__inline void
pmap_unuse_pt(pmap, va, mpte)
	pmap_t pmap;
	vm_offset_t va;
	vm_page_t mpte;
{

	if ((va >= UPT_MIN_ADDRESS) || !pmap_initialized)
		return;

	if (mpte == NULL) {
		vm_offset_t pt;
		pt = (vm_offset_t) vtopte(va);
		mpte = pmap_pte_vm_page(pmap, pt);
	}

	vm_page_unhold(mpte);

	if (pmap != kernel_pmap &&
	    (mpte->hold_count == 0) &&
	    (mpte->wire_count == 0) &&
	    (va < KPT_MIN_ADDRESS)) {
/*
 * We don't free page-table-pages anymore because it can have a negative
 * impact on perf at times.  Now we just deactivate, and it'll get cleaned
 * up if needed...  Also, if the page ends up getting used, it will fault
 * back into the process address space and be reactivated.
 */
#ifdef PMAP_FREE_OLD_PTES
		pmap_page_protect(VM_PAGE_TO_PHYS(mpte), VM_PROT_NONE);
		vm_page_free(mpte);
#else
		mpte->dirty = 0;
		vm_page_deactivate(mpte);
#endif
	}
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 *
 *	On the i386 this is called after mapping has already been enabled
 *	and just syncs the pmap module with what has already been done.
 *	[We can't call it easily with mapping off since the kernel is not
 *	mapped with PA == VA, hence we would have to relocate every address
 *	from the linked base (virtual) address "KERNBASE" to the actual
 *	(physical) address starting relative to 0]
 */
void
pmap_bootstrap(firstaddr, loadaddr)
	vm_offset_t firstaddr;
	vm_offset_t loadaddr;
{
	vm_offset_t va;
	pt_entry_t *pte;

	avail_start = firstaddr;

	/*
	 * XXX The calculation of virtual_avail is wrong. It's NKPT*PAGE_SIZE too
	 * large. It should instead be correctly calculated in locore.s and
	 * not based on 'first' (which is a physical address, not a virtual
	 * address, for the start of unused physical memory). The kernel
	 * page tables are NOT double mapped and thus should not be included
	 * in this calculation.
	 */
	virtual_avail = (vm_offset_t) KERNBASE + firstaddr;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Initialize protection array.
	 */
	i386_protection_init();

	/*
	 * The kernel's pmap is statically allocated so we don't have to use
	 * pmap_create, which is unlikely to work correctly at this part of
	 * the boot sequence (XXX and which no longer exists).
	 */
	kernel_pmap = &kernel_pmap_store;

	kernel_pmap->pm_pdir = (pd_entry_t *) (KERNBASE + IdlePTD);

	kernel_pmap->pm_count = 1;
	nkpt = NKPT;

	/*
	 * Reserve some special page table entries/VA space for temporary
	 * mapping of pages.
	 */
#define	SYSMAP(c, p, v, n)	\
	v = (c)va; va += ((n)*PAGE_SIZE); p = pte; pte += (n);

	va = virtual_avail;
	pte = pmap_pte(kernel_pmap, va);

	/*
	 * CMAP1/CMAP2 are used for zeroing and copying pages.
	 */
	SYSMAP(caddr_t, CMAP1, CADDR1, 1)
	SYSMAP(caddr_t, CMAP2, CADDR2, 1)

	/*
	 * ptmmap is used for reading arbitrary physical pages via /dev/mem.
	 */
	SYSMAP(caddr_t, ptmmap, ptvmmap, 1)

	/*
	 * msgbufmap is used to map the system message buffer.
	 */
	SYSMAP(struct msgbuf *, msgbufmap, msgbufp, 1)

	virtual_avail = va;

	*(int *) CMAP1 = *(int *) CMAP2 = *(int *) PTD = 0;
	pmap_update();
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 *	pmap_init has been enhanced to support in a fairly consistant
 *	way, discontiguous physical memory.
 */
void
pmap_init(phys_start, phys_end)
	vm_offset_t phys_start, phys_end;
{
	vm_offset_t addr;
	vm_size_t npg, s;
	int i;

	/*
	 * calculate the number of pv_entries needed
	 */
	vm_first_phys = phys_avail[0];
	for (i = 0; phys_avail[i + 1]; i += 2);
	npg = (phys_avail[(i - 2) + 1] - vm_first_phys) / PAGE_SIZE;

	/*
	 * Allocate memory for random pmap data structures.  Includes the
	 * pv_head_table.
	 */
	s = (vm_size_t) (sizeof(struct pv_entry) * npg);
	s = round_page(s);
	addr = (vm_offset_t) kmem_alloc(kernel_map, s);
	pv_table = (pv_entry_t) addr;

	/*
	 * init the pv free list
	 */
	init_pv_entries(npg);
	/*
	 * Now it is safe to enable pv_table recording.
	 */
	pmap_initialized = TRUE;
}

/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	For now, VM is already on, we only need to map the
 *	specified memory.
 */
vm_offset_t
pmap_map(virt, start, end, prot)
	vm_offset_t virt;
	vm_offset_t start;
	vm_offset_t end;
	int prot;
{
	while (start < end) {
		pmap_enter(kernel_pmap, virt, start, prot, FALSE);
		virt += PAGE_SIZE;
		start += PAGE_SIZE;
	}
	return (virt);
}

#ifdef PMAP_KEEP_PDIRS
int nfreepdir;
caddr_t *pdirlist;
#define NFREEPDIR 3

static void *
pmap_getpdir() {
	caddr_t *pdir;
	if (pdirlist) {
		--nfreepdir;
		pdir = pdirlist;
		pdirlist = (caddr_t *) *pdir;
		bzero( (caddr_t) pdir, PAGE_SIZE);
	} else {
		pdir = (caddr_t *) kmem_alloc(kernel_map, PAGE_SIZE);
	}

	return (void *) pdir;
}

static void
pmap_freepdir(void *pdir) {
	if (nfreepdir > NFREEPDIR) {
		kmem_free(kernel_map, (vm_offset_t) pdir, PAGE_SIZE);
	} else {
		* (caddr_t *) pdir = (caddr_t) pdirlist;
		pdirlist = (caddr_t *) pdir;
		++nfreepdir;
	}
}
#endif

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	register struct pmap *pmap;
{
	/*
	 * No need to allocate page table space yet but we do need a valid
	 * page directory table.
	 */

#ifdef PMAP_KEEP_PDIRS
	pmap->pm_pdir = pmap_getpdir();
#else
	pmap->pm_pdir = (pd_entry_t *) kmem_alloc(kernel_map, PAGE_SIZE);
#endif

	/* wire in kernel global address entries */
	bcopy(PTD + KPTDI, pmap->pm_pdir + KPTDI, nkpt * PTESIZE);

	/* install self-referential address mapping entry */
	*(int *) (pmap->pm_pdir + PTDPTDI) =
	    ((int) pmap_kextract((vm_offset_t) pmap->pm_pdir)) | PG_V | PG_KW;

	pmap->pm_count = 1;
}

/*
 * grow the number of kernel page table entries, if needed
 */

static vm_page_t nkpg;
vm_offset_t kernel_vm_end;

void
pmap_growkernel(vm_offset_t addr)
{
	struct proc *p;
	struct pmap *pmap;
	int s;

	s = splhigh();
	if (kernel_vm_end == 0) {
		kernel_vm_end = KERNBASE;
		nkpt = 0;
		while (pdir_pde(PTD, kernel_vm_end)) {
			kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
			++nkpt;
		}
	}
	addr = (addr + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
	while (kernel_vm_end < addr) {
		if (pdir_pde(PTD, kernel_vm_end)) {
			kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
			continue;
		}
		++nkpt;
		if (!nkpg) {
			nkpg = vm_page_alloc(kernel_object, 0, VM_ALLOC_SYSTEM);
			if (!nkpg)
				panic("pmap_growkernel: no memory to grow kernel");
			vm_page_wire(nkpg);
			vm_page_remove(nkpg);
			pmap_zero_page(VM_PAGE_TO_PHYS(nkpg));
		}
		pdir_pde(PTD, kernel_vm_end) = (pd_entry_t) (VM_PAGE_TO_PHYS(nkpg) | PG_V | PG_KW);
		nkpg = NULL;

		for (p = (struct proc *) allproc; p != NULL; p = p->p_next) {
			if (p->p_vmspace) {
				pmap = &p->p_vmspace->vm_pmap;
				*pmap_pde(pmap, kernel_vm_end) = pdir_pde(PTD, kernel_vm_end);
			}
		}
		*pmap_pde(kernel_pmap, kernel_vm_end) = pdir_pde(PTD, kernel_vm_end);
		kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
	}
	splx(s);
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(pmap)
	register pmap_t pmap;
{
	int count;

	if (pmap == NULL)
		return;

	count = --pmap->pm_count;
	if (count == 0) {
		pmap_release(pmap);
		free((caddr_t) pmap, M_VMPMAP);
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap)
	register struct pmap *pmap;
{
#ifdef PMAP_KEEP_PDIRS
	pmap_freepdir( (void *)pmap->pm_pdir);
#else
	kmem_free(kernel_map, (vm_offset_t) pmap->pm_pdir, PAGE_SIZE);
#endif
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t pmap;
{
	if (pmap != NULL) {
		pmap->pm_count++;
	}
}

#define PV_FREELIST_MIN ((PAGE_SIZE / sizeof (struct pv_entry)) / 2)

/*
 * Data for the pv entry allocation mechanism
 */
static int pv_freelistcnt;
static pv_entry_t pv_freelist;
static vm_offset_t pvva;
static int npvvapg;

/*
 * free the pv_entry back to the free list
 */
static __inline void
free_pv_entry(pv)
	pv_entry_t pv;
{
	if (!pv)
		return;
	++pv_freelistcnt;
	pv->pv_next = pv_freelist;
	pv_freelist = pv;
}

/*
 * get a new pv_entry, allocating a block from the system
 * when needed.
 * the memory allocation is performed bypassing the malloc code
 * because of the possibility of allocations at interrupt time.
 */
static __inline pv_entry_t
get_pv_entry()
{
	pv_entry_t tmp;

	/*
	 * get more pv_entry pages if needed
	 */
	if (pv_freelistcnt < PV_FREELIST_MIN || pv_freelist == 0) {
		pmap_alloc_pv_entry();
	}
	/*
	 * get a pv_entry off of the free list
	 */
	--pv_freelistcnt;
	tmp = pv_freelist;
	pv_freelist = tmp->pv_next;
	return tmp;
}

/*
 * this *strange* allocation routine *statistically* eliminates the
 * *possibility* of a malloc failure (*FATAL*) for a pv_entry_t data structure.
 * also -- this code is MUCH MUCH faster than the malloc equiv...
 */
static void
pmap_alloc_pv_entry()
{
	/*
	 * do we have any pre-allocated map-pages left?
	 */
	if (npvvapg) {
		vm_page_t m;

		/*
		 * we do this to keep recursion away
		 */
		pv_freelistcnt += PV_FREELIST_MIN;
		/*
		 * allocate a physical page out of the vm system
		 */
		m = vm_page_alloc(kernel_object,
		    OFF_TO_IDX(pvva - vm_map_min(kernel_map)),
		    VM_ALLOC_INTERRUPT);
		if (m) {
			int newentries;
			int i;
			pv_entry_t entry;

			newentries = (PAGE_SIZE / sizeof(struct pv_entry));
			/*
			 * wire the page
			 */
			vm_page_wire(m);
			m->flags &= ~PG_BUSY;
			/*
			 * let the kernel see it
			 */
			pmap_kenter(pvva, VM_PAGE_TO_PHYS(m));

			entry = (pv_entry_t) pvva;
			/*
			 * update the allocation pointers
			 */
			pvva += PAGE_SIZE;
			--npvvapg;

			/*
			 * free the entries into the free list
			 */
			for (i = 0; i < newentries; i++) {
				free_pv_entry(entry);
				entry++;
			}
		}
		pv_freelistcnt -= PV_FREELIST_MIN;
	}
	if (!pv_freelist)
		panic("get_pv_entry: cannot get a pv_entry_t");
}



/*
 * init the pv_entry allocation system
 */
#define PVSPERPAGE 64
void
init_pv_entries(npg)
	int npg;
{
	/*
	 * allocate enough kvm space for PVSPERPAGE entries per page (lots)
	 * kvm space is fairly cheap, be generous!!!  (the system can panic if
	 * this is too small.)
	 */
	npvvapg = ((npg * PVSPERPAGE) * sizeof(struct pv_entry)
		+ PAGE_SIZE - 1) / PAGE_SIZE;
	pvva = kmem_alloc_pageable(kernel_map, npvvapg * PAGE_SIZE);
	/*
	 * get the first batch of entries
	 */
	free_pv_entry(get_pv_entry());
}

static pt_entry_t *
get_pt_entry(pmap)
	pmap_t pmap;
{
	vm_offset_t frame = (int) pmap->pm_pdir[PTDPTDI] & PG_FRAME;

	/* are we current address space or kernel? */
	if (pmap == kernel_pmap || frame == ((int) PTDpde & PG_FRAME)) {
		return PTmap;
	}
	/* otherwise, we are alternate address space */
	if (frame != ((int) APTDpde & PG_FRAME)) {
		APTDpde = pmap->pm_pdir[PTDPTDI];
		pmap_update();
	}
	return APTmap;
}

/*
 * If it is the first entry on the list, it is actually
 * in the header and we must copy the following entry up
 * to the header.  Otherwise we must search the list for
 * the entry.  In either case we free the now unused entry.
 */
static void
pmap_remove_entry(pmap, pv, va)
	struct pmap *pmap;
	pv_entry_t pv;
	vm_offset_t va;
{
	pv_entry_t npv;
	int s;

	s = splhigh();
	if (pmap == pv->pv_pmap && va == pv->pv_va) {
		pmap_unuse_pt(pmap, va, pv->pv_ptem);
		npv = pv->pv_next;
		if (npv) {
			*pv = *npv;
			free_pv_entry(npv);
		} else {
			pv->pv_pmap = NULL;
		}
	} else {
		for (npv = pv->pv_next; npv; (pv = npv, npv = pv->pv_next)) {
			if (pmap == npv->pv_pmap && va == npv->pv_va) {
				pmap_unuse_pt(pmap, va, npv->pv_ptem);
				pv->pv_next = npv->pv_next;
				free_pv_entry(npv);
				break;
			}
		}
	}
	splx(s);
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap, sva, eva)
	struct pmap *pmap;
	register vm_offset_t sva;
	register vm_offset_t eva;
{
	register pt_entry_t *ptp, *ptq;
	vm_offset_t pa;
	register pv_entry_t pv;
	vm_offset_t va;
	pt_entry_t oldpte;
	vm_offset_t pdnxt;
	vm_offset_t ptepaddr;
	vm_page_t mpte;
	int update_needed;

	if (pmap == NULL)
		return;

	ptp = get_pt_entry(pmap);

	/*
	 * special handling of removing one page.  a very
	 * common operation and easy to short circuit some
	 * code.
	 */
	if ((sva + PAGE_SIZE) == eva) {

		if (*pmap_pde(pmap, sva) == 0)
			return;

		ptq = ptp + i386_btop(sva);

		if (!*ptq)
			return;

		oldpte = *ptq;
		if (((int)oldpte) & PG_W)
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		*ptq = 0;

		pa = ((int)oldpte) & PG_FRAME;
		if (pmap_is_managed(pa)) {
			if ((int) oldpte & PG_M) {
				if (sva < USRSTACK + (UPAGES * PAGE_SIZE) ||
				    (sva >= KERNBASE && (sva < clean_sva || sva >= clean_eva))) {
					PHYS_TO_VM_PAGE(pa)->dirty |= VM_PAGE_BITS_ALL;
				}
			}
			pv = pa_to_pvh(pa);
			pmap_remove_entry(pmap, pv, sva);
		} else {
			pmap_unuse_pt(pmap, sva, NULL);
		}
		pmap_update_1pg(sva);
		return;
	}

	update_needed = 0;
	sva = i386_btop(sva);
	pdnxt = ((sva + NPTEPG) & ~(NPTEPG - 1));
	ptepaddr = (vm_offset_t) *pmap_pde(pmap, i386_ptob(sva));
	eva = i386_btop(eva);
	mpte = NULL;

	while (sva < eva) {
		if (sva >= pdnxt) {
			pdnxt = ((sva + NPTEPG) & ~(NPTEPG - 1));
			ptepaddr = (vm_offset_t) *pmap_pde(pmap, i386_ptob(sva));
			mpte = NULL;
		}
		/*
		 * Weed out invalid mappings. Note: we assume that the page
		 * directory table is always allocated, and in kernel virtual.
		 */
		if (ptepaddr == 0) {
			sva = pdnxt;
			continue;
		}

		if (mpte == NULL)
			mpte = PHYS_TO_VM_PAGE(i386_trunc_page(ptepaddr));
		if ((mpte->hold_count == 0) && (mpte->wire_count == 0)) {
			sva = pdnxt;
			continue;
		}

		if (pdnxt > eva)
			pdnxt = eva;
		/*
		 * search for page table entries
		 */
		while ((sva < pdnxt) && (*(ptp + sva) == 0))
			++sva;
		if (sva == pdnxt) {
			continue;
		}

		ptq = ptp + sva;
		/*
		 * Invalidate the PTEs. XXX: should cluster them up and
		 * invalidate as many as possible at once.
		 * Update statistics
		 */
		oldpte = *ptq;
		*ptq = 0;
		if (((int) oldpte) & PG_W)
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		va = i386_ptob(sva);

		++update_needed;
		pa = ((int) oldpte) & PG_FRAME;
		if (!pmap_is_managed(pa)) {
			pmap_unuse_pt(pmap, (vm_offset_t) va, NULL);
			++sva;
			continue;
		}
		if ((int) oldpte & PG_M) {
			if (va < USRSTACK + (UPAGES * PAGE_SIZE) ||
			    (va >= KERNBASE && (va < clean_sva || va >= clean_eva))) {
				PHYS_TO_VM_PAGE(pa)->dirty |= VM_PAGE_BITS_ALL;
			}
		}
		pv = pa_to_pvh(pa);
		pmap_remove_entry(pmap, pv, va);
		++sva;
	}
	if (update_needed)
		pmap_update();
}

/*
 *	Routine:	pmap_remove_all
 *	Function:
 *		Removes this physical page from
 *		all physical maps in which it resides.
 *		Reflects back modify bits to the pager.
 *
 *	Notes:
 *		Original versions of this routine were very
 *		inefficient because they iteratively called
 *		pmap_remove (slow...)
 */
static void
pmap_remove_all(pa)
	vm_offset_t pa;
{
	register pv_entry_t pv, opv, npv;
	register pt_entry_t *pte, *ptp;
	vm_offset_t va;
	struct pmap *pmap;
	vm_page_t m;
	int s;
	int anyvalid = 0;

	/*
	 * Not one of ours
	 */
	/*
	 * XXX this makes pmap_page_protect(NONE) illegal for non-managed
	 * pages!
	 */
	if (!pmap_is_managed(pa))
		return;

	pa = trunc_page(pa);
	opv = pa_to_pvh(pa);
	if (opv->pv_pmap == NULL)
		return;

	m = PHYS_TO_VM_PAGE(pa);
	s = splhigh();
	pv = opv;
	while (pv && ((pmap = pv->pv_pmap) != NULL)) {
		int tpte;
		ptp = get_pt_entry(pmap);
		va = pv->pv_va;
		pte = ptp + i386_btop(va);
		if (tpte = ((int) *pte)) {
			*pte = 0;
			if (tpte & PG_W)
				pmap->pm_stats.wired_count--;
			pmap->pm_stats.resident_count--;
			if (curproc != pageproc)
				anyvalid++;

			/*
			 * Update the vm_page_t clean and reference bits.
			 */
			if ((tpte & PG_M) != 0) {
				if (va < USRSTACK + (UPAGES * PAGE_SIZE) ||
				    (va >= KERNBASE && (va < clean_sva || va >= clean_eva))) {
					m->dirty = VM_PAGE_BITS_ALL;
				}
			}
		}
		pv = pv->pv_next;
	}

	for (pv = opv->pv_next; pv; pv = npv) {
		npv = pv->pv_next;
		pmap_unuse_pt(pv->pv_pmap, pv->pv_va, pv->pv_ptem);
		free_pv_entry(pv);
	}

	opv->pv_pmap = NULL;
	opv->pv_next = NULL;
		
	splx(s);
	if (anyvalid)
		pmap_update();
}


/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap, sva, eva, prot)
	register pmap_t pmap;
	vm_offset_t sva, eva;
	vm_prot_t prot;
{
	register pt_entry_t *pte;
	register vm_offset_t va;
	int i386prot;
	register pt_entry_t *ptp;
	int anychanged = 0;

	if (pmap == NULL)
		return;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}
	if (prot & VM_PROT_WRITE)
		return;

	ptp = get_pt_entry(pmap);

	sva = i386_btop(sva);
	eva = i386_btop(eva);

	while (sva < eva) {
		vm_offset_t pdnxt;
		vm_offset_t ptepaddr;
		vm_page_t mpte;
		int pprot;
		/*
		 * Weed out invalid mappings. Note: we assume that the page
		 * directory table is always allocated, and in kernel virtual.
		 */

		pdnxt = ((sva + NPTEPG) & ~(NPTEPG - 1));
		ptepaddr = (vm_offset_t) *pmap_pde(pmap, i386_ptob(sva));
		if (ptepaddr == 0) {
			sva = pdnxt;
			continue;
		}

		mpte = PHYS_TO_VM_PAGE(i386_trunc_page(ptepaddr));
		if ((mpte->hold_count == 0) && (mpte->wire_count == 0)) {
			sva = pdnxt;
			continue;
		}

		if (pdnxt > eva)
			pdnxt = eva;
		/*
		 * search for page table entries
		 */
		while ((sva < pdnxt) && (*(ptp + sva) == 0))
			++sva;

		if (sva == pdnxt)
			continue;

		pte = ptp + sva;

		va = i386_ptob(sva);
		i386prot = pte_prot(pmap, prot);
		if (va < UPT_MAX_ADDRESS) {
			i386prot |= PG_u;
			if (va >= UPT_MIN_ADDRESS)
				i386prot |= PG_RW;
		}
		pprot = *(int *)pte & PG_PROT;
		if (pprot != i386prot) {
			pmap_pte_set_prot(pte, i386prot);
			anychanged++;
		}
		++sva;
	}
	if (anychanged)
		pmap_update();
}

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
void
pmap_enter(pmap, va, pa, prot, wired)
	register pmap_t pmap;
	vm_offset_t va;
	register vm_offset_t pa;
	vm_prot_t prot;
	boolean_t wired;
{
	register pt_entry_t *pte;
	register pt_entry_t npte;
	vm_offset_t opa;
	register pv_entry_t pv, npv;
	int ptevalid = 0;

	if (pmap == NULL)
		return;

	pv = NULL;

	va = trunc_page(va);
	pa = trunc_page(pa);
	if (va > VM_MAX_KERNEL_ADDRESS)
		panic("pmap_enter: toobig");

	/*
	 * Page Directory table entry not valid, we need a new PT page
	 */
	pte = pmap_pte(pmap, va);
	if (pte == NULL) {
		printf("kernel page directory invalid pdir=%p, va=0x%lx\n",
			pmap->pm_pdir[PTDPTDI], va);
		panic("invalid kernel page directory");
	}
	opa = pmap_pte_pa(pte);

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (opa == pa) {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is wired,
		 * the PT page will be also.
		 */
		if (wired && !pmap_pte_w(pte))
			pmap->pm_stats.wired_count++;
		else if (!wired && pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;

		goto validate;
	}
	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (opa) {
		pmap_remove(pmap, va, va + PAGE_SIZE);
	}
	/*
	 * Enter on the PV list if part of our managed memory Note that we
	 * raise IPL while manipulating pv_table since pmap_enter can be
	 * called at interrupt time.
	 */
	if (pmap_is_managed(pa)) {
		int s;

		pv = pa_to_pvh(pa);
		s = splhigh();
		/*
		 * No entries yet, use header as the first entry
		 */
		if (pv->pv_pmap == NULL) {
			pv->pv_va = va;
			pv->pv_pmap = pmap;
			pv->pv_next = NULL;
			pv->pv_ptem = NULL;
		}
		/*
		 * There is at least one other VA mapping this page. Place
		 * this entry after the header.
		 */
		else {
			npv = get_pv_entry();
			npv->pv_va = va;
			npv->pv_pmap = pmap;
			npv->pv_next = pv->pv_next;
			pv->pv_next = npv;
			pv = npv;
			pv->pv_ptem = NULL;
		}
		splx(s);
	}

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;
	if (wired)
		pmap->pm_stats.wired_count++;

validate:
	/*
	 * Now validate mapping with desired protection/wiring.
	 */
	npte = (pt_entry_t) ((int) (pa | pte_prot(pmap, prot) | PG_V));

	/*
	 * When forking (copy-on-write, etc): A process will turn off write
	 * permissions for any of its writable pages.  If the data (object) is
	 * only referred to by one process, the processes map is modified
	 * directly as opposed to using the object manipulation routine.  When
	 * using pmap_protect, the modified bits are not kept in the vm_page_t
	 * data structure.  Therefore, when using pmap_enter in vm_fault to
	 * bring back writability of a page, there has been no memory of the
	 * modified or referenced bits except at the pte level.  this clause
	 * supports the carryover of the modified and used (referenced) bits.
	 */
	if (pa == opa)
		(int) npte |= (int) *pte & (PG_M | PG_U);

	if (wired)
		(int) npte |= PG_W;
	if (va < UPT_MIN_ADDRESS)
		(int) npte |= PG_u;
	else if (va < UPT_MAX_ADDRESS)
		(int) npte |= PG_u | PG_RW;

	if (*pte != npte) {
		if (*pte)
			ptevalid++;
		*pte = npte;
	}
	if (ptevalid) {
		pmap_update_1pg(va);
	} else {
		if (pv) {
			pv->pv_ptem = pmap_use_pt(pmap, va);
		}
	}
}

/*
 * Add a list of wired pages to the kva
 * this routine is only used for temporary
 * kernel mappings that do not need to have
 * page modification or references recorded.
 * Note that old mappings are simply written
 * over.  The page *must* be wired.
 */
void
pmap_qenter(va, m, count)
	vm_offset_t va;
	vm_page_t *m;
	int count;
{
	int i;
	int anyvalid = 0;
	register pt_entry_t *pte;

	for (i = 0; i < count; i++) {
		vm_offset_t tva = va + i * PAGE_SIZE;
		pt_entry_t npte = (pt_entry_t) ((int) (VM_PAGE_TO_PHYS(m[i]) | PG_RW | PG_V));
		pte = vtopte(tva);
		if (*pte && (*pte != npte))
			pmap_update_1pg(tva);
		*pte = npte;
	}
}
/*
 * this routine jerks page mappings from the
 * kernel -- it is meant only for temporary mappings.
 */
void
pmap_qremove(va, count)
	vm_offset_t va;
	int count;
{
	int i;
	register pt_entry_t *pte;

	for (i = 0; i < count; i++) {
		vm_offset_t tva = va + i * PAGE_SIZE;
		pte = vtopte(tva);
		*pte = 0;
		pmap_update_1pg(tva);
	}
}

/*
 * add a wired page to the kva
 * note that in order for the mapping to take effect -- you
 * should do a pmap_update after doing the pmap_kenter...
 */
void
pmap_kenter(va, pa)
	vm_offset_t va;
	register vm_offset_t pa;
{
	register pt_entry_t *pte;
	int wasvalid = 0;

	pte = vtopte(va);

	if (*pte)
		wasvalid++;

	*pte = (pt_entry_t) ((int) (pa | PG_RW | PG_V));

	if (wasvalid)
		pmap_update_1pg(va);
}

/*
 * remove a page from the kernel pagetables
 */
void
pmap_kremove(va)
	vm_offset_t va;
{
	register pt_entry_t *pte;

	pte = vtopte(va);

	*pte = (pt_entry_t) 0;
	pmap_update_1pg(va);
}

/*
 * this code makes some *MAJOR* assumptions:
 * 1. Current pmap & pmap exists.
 * 2. Not wired.
 * 3. Read access.
 * 4. No page table pages.
 * 5. Tlbflush is deferred to calling procedure.
 * 6. Page IS managed.
 * but is *MUCH* faster than pmap_enter...
 */

static void
pmap_enter_quick(pmap, va, pa)
	register pmap_t pmap;
	vm_offset_t va;
	register vm_offset_t pa;
{
	register pt_entry_t *pte;
	register pv_entry_t pv, npv;
	int s;

	/*
	 * Enter on the PV list if part of our managed memory Note that we
	 * raise IPL while manipulating pv_table since pmap_enter can be
	 * called at interrupt time.
	 */

	pte = vtopte(va);
#if 0
	/* a fault on the page table might occur here */
	if (*pte) {
		pmap_remove(pmap, va, va + PAGE_SIZE);
	}
#endif

	pv = pa_to_pvh(pa);
	s = splhigh();
	/*
	 * No entries yet, use header as the first entry
	 */
	if (pv->pv_pmap == NULL) {
		pv->pv_pmap = pmap;
		pv->pv_va = va;
		pv->pv_next = NULL;
	}
	/*
	 * There is at least one other VA mapping this page. Place this entry
	 * after the header.
	 */
	else {
		npv = get_pv_entry();
		npv->pv_va = va;
		npv->pv_pmap = pmap;
		npv->pv_next = pv->pv_next;
		pv->pv_next = npv;
		pv = npv;
	}
	splx(s);
	pv->pv_ptem = pmap_use_pt(pmap, va);

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;

	/*
	 * Now validate mapping with desired protection/wiring.
	 */
	*pte = (pt_entry_t) ((int) (pa | PG_V | PG_u));

	return;
}

#define MAX_INIT_PT (512)
/*
 * pmap_object_init_pt preloads the ptes for a given object
 * into the specified pmap.  This eliminates the blast of soft
 * faults on process startup and immediately after an mmap.
 */
void
pmap_object_init_pt(pmap, addr, object, pindex, size)
	pmap_t pmap;
	vm_offset_t addr;
	vm_object_t object;
	vm_pindex_t pindex;
	vm_size_t size;
{
	vm_offset_t tmpidx;
	int psize;
	vm_page_t p;
	int objpgs;

	psize = (size >> PAGE_SHIFT);

	if (!pmap || ((psize > MAX_INIT_PT) &&
		(object->resident_page_count > MAX_INIT_PT))) {
		return;
	}

	/*
	 * remove any already used mappings
	 */
	pmap_remove( pmap, trunc_page(addr), round_page(addr + size));

	/*
	 * if we are processing a major portion of the object, then scan the
	 * entire thing.
	 */
	if (psize > (object->size >> 2)) {
		objpgs = psize;

		for (p = object->memq.tqh_first;
		    ((objpgs > 0) && (p != NULL));
		    p = p->listq.tqe_next) {

			tmpidx = p->pindex;
			if (tmpidx < pindex) {
				continue;
			}
			tmpidx -= pindex;
			if (tmpidx >= psize) {
				continue;
			}
			if (((p->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL) &&
			    (p->busy == 0) &&
			    (p->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {
				if (p->queue == PQ_CACHE)
					vm_page_deactivate(p);
				vm_page_hold(p);
				p->flags |= PG_MAPPED;
				pmap_enter_quick(pmap,
					addr + (tmpidx << PAGE_SHIFT),
					VM_PAGE_TO_PHYS(p));
				vm_page_unhold(p);
			}
			objpgs -= 1;
		}
	} else {
		/*
		 * else lookup the pages one-by-one.
		 */
		for (tmpidx = 0; tmpidx < psize; tmpidx += 1) {
			p = vm_page_lookup(object, tmpidx + pindex);
			if (p && (p->busy == 0) &&
			    ((p->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL) &&
			    (p->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {
				if (p->queue == PQ_CACHE)
					vm_page_deactivate(p);
				vm_page_hold(p);
				p->flags |= PG_MAPPED;
				pmap_enter_quick(pmap,
					addr + (tmpidx << PAGE_SHIFT),
					VM_PAGE_TO_PHYS(p));
				vm_page_unhold(p);
			}
		}
	}
}

/*
 * pmap_prefault provides a quick way of clustering
 * pagefaults into a processes address space.  It is a "cousin"
 * of pmap_object_init_pt, except it runs at page fault time instead
 * of mmap time.
 */
#define PFBAK 2
#define PFFOR 2
#define PAGEORDER_SIZE (PFBAK+PFFOR)

static int pmap_prefault_pageorder[] = {
	-NBPG, NBPG, -2 * NBPG, 2 * NBPG
};

void
pmap_prefault(pmap, addra, entry, object)
	pmap_t pmap;
	vm_offset_t addra;
	vm_map_entry_t entry;
	vm_object_t object;
{
	int i;
	vm_offset_t starta;
	vm_offset_t addr;
	vm_pindex_t pindex;
	vm_page_t m;
	int pageorder_index;

	if (entry->object.vm_object != object)
		return;

	if (!curproc || (pmap != &curproc->p_vmspace->vm_pmap))
		return;

	starta = addra - PFBAK * PAGE_SIZE;
	if (starta < entry->start) {
		starta = entry->start;
	} else if (starta > addra) {
		starta = 0;
	}

	for (i = 0; i < PAGEORDER_SIZE; i++) {
		vm_object_t lobject;
		pt_entry_t *pte;

		addr = addra + pmap_prefault_pageorder[i];
		if (addr < starta || addr >= entry->end)
			continue;

		pte = vtopte(addr);
		if (*pte)
			continue;

		pindex = ((addr - entry->start) + entry->offset) >> PAGE_SHIFT;
		lobject = object;
		for (m = vm_page_lookup(lobject, pindex);
		    (!m && (lobject->type == OBJT_DEFAULT) && (lobject->backing_object));
		    lobject = lobject->backing_object) {
			if (lobject->backing_object_offset & PAGE_MASK)
				break;
			pindex += (lobject->backing_object_offset >> PAGE_SHIFT);
			m = vm_page_lookup(lobject->backing_object, pindex);
		}

		/*
		 * give-up when a page is not in memory
		 */
		if (m == NULL)
			break;

		if (((m->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL) &&
		    (m->busy == 0) &&
		    (m->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {

			if (m->queue == PQ_CACHE) {
				if ((cnt.v_free_count + cnt.v_cache_count) <
					cnt.v_free_min)
					break;
				vm_page_deactivate(m);
			}
			vm_page_hold(m);
			m->flags |= PG_MAPPED;
			pmap_enter_quick(pmap, addr, VM_PAGE_TO_PHYS(m));
			vm_page_unhold(m);
		}
	}
}

/*
 *	Routine:	pmap_change_wiring
 *	Function:	Change the wiring attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void
pmap_change_wiring(pmap, va, wired)
	register pmap_t pmap;
	vm_offset_t va;
	boolean_t wired;
{
	register pt_entry_t *pte;

	if (pmap == NULL)
		return;

	pte = pmap_pte(pmap, va);

	if (wired && !pmap_pte_w(pte))
		pmap->pm_stats.wired_count++;
	else if (!wired && pmap_pte_w(pte))
		pmap->pm_stats.wired_count--;

	/*
	 * Wiring is not a hardware characteristic so there is no need to
	 * invalidate TLB.
	 */
	pmap_pte_set_w(pte, wired);
}



/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t dst_pmap, src_pmap;
	vm_offset_t dst_addr;
	vm_size_t len;
	vm_offset_t src_addr;
{
}

/*
 *	Routine:	pmap_kernel
 *	Function:
 *		Returns the physical map handle for the kernel.
 */
pmap_t
pmap_kernel()
{
	return (kernel_pmap);
}

/*
 *	pmap_zero_page zeros the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bzero to clear its contents, one machine dependent page
 *	at a time.
 */
void
pmap_zero_page(phys)
	vm_offset_t phys;
{
	if (*(int *) CMAP2)
		panic("pmap_zero_page: CMAP busy");

	*(int *) CMAP2 = PG_V | PG_KW | trunc_page(phys);
	bzero(CADDR2, PAGE_SIZE);

	*(int *) CMAP2 = 0;
	pmap_update_1pg((vm_offset_t) CADDR2);
}

/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 */
void
pmap_copy_page(src, dst)
	vm_offset_t src;
	vm_offset_t dst;
{
	if (*(int *) CMAP1 || *(int *) CMAP2)
		panic("pmap_copy_page: CMAP busy");

	*(int *) CMAP1 = PG_V | PG_KW | trunc_page(src);
	*(int *) CMAP2 = PG_V | PG_KW | trunc_page(dst);

#if __GNUC__ > 1
	memcpy(CADDR2, CADDR1, PAGE_SIZE);
#else
	bcopy(CADDR1, CADDR2, PAGE_SIZE);
#endif
	*(int *) CMAP1 = 0;
	*(int *) CMAP2 = 0;
	pmap_update_2pg( (vm_offset_t) CADDR1, (vm_offset_t) CADDR2);
}


/*
 *	Routine:	pmap_pageable
 *	Function:
 *		Make the specified pages (by pmap, offset)
 *		pageable (or not) as requested.
 *
 *		A page which is not pageable may not take
 *		a fault; therefore, its page table entry
 *		must remain valid for the duration.
 *
 *		This routine is merely advisory; pmap_enter
 *		will specify that these pages are to be wired
 *		down (or not) as appropriate.
 */
void
pmap_pageable(pmap, sva, eva, pageable)
	pmap_t pmap;
	vm_offset_t sva, eva;
	boolean_t pageable;
{
}

/*
 * this routine returns true if a physical page resides
 * in the given pmap.
 */
boolean_t
pmap_page_exists(pmap, pa)
	pmap_t pmap;
	vm_offset_t pa;
{
	register pv_entry_t pv;
	int s;

	if (!pmap_is_managed(pa))
		return FALSE;

	pv = pa_to_pvh(pa);
	s = splhigh();

	/*
	 * Not found, check current mappings returning immediately if found.
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			if (pv->pv_pmap == pmap) {
				splx(s);
				return TRUE;
			}
		}
	}
	splx(s);
	return (FALSE);
}

/*
 * pmap_testbit tests bits in pte's
 * note that the testbit/changebit routines are inline,
 * and a lot of things compile-time evaluate.
 */
static __inline boolean_t
pmap_testbit(pa, bit)
	register vm_offset_t pa;
	int bit;
{
	register pv_entry_t pv;
	pt_entry_t *pte;
	int s;

	if (!pmap_is_managed(pa))
		return FALSE;

	pv = pa_to_pvh(pa);
	s = splhigh();

	/*
	 * Not found, check current mappings returning immediately if found.
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			/*
			 * if the bit being tested is the modified bit, then
			 * mark UPAGES as always modified, and ptes as never
			 * modified.
			 */
			if (bit & (PG_U|PG_M)) {
				if ((pv->pv_va >= clean_sva) && (pv->pv_va < clean_eva)) {
					continue;
				}
			}
			if (!pv->pv_pmap) {
				printf("Null pmap (tb) at va: 0x%lx\n", pv->pv_va);
				continue;
			}
			pte = pmap_pte(pv->pv_pmap, pv->pv_va);
			if ((int) *pte & bit) {
				splx(s);
				return TRUE;
			}
		}
	}
	splx(s);
	return (FALSE);
}

/*
 * this routine is used to modify bits in ptes
 */
static __inline void
pmap_changebit(pa, bit, setem)
	vm_offset_t pa;
	int bit;
	boolean_t setem;
{
	register pv_entry_t pv;
	register pt_entry_t *pte, npte;
	vm_offset_t va;
	int changed;
	int s;

	if (!pmap_is_managed(pa))
		return;

	pv = pa_to_pvh(pa);
	s = splhigh();

	/*
	 * Loop over all current mappings setting/clearing as appropos If
	 * setting RO do we need to clear the VAC?
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			va = pv->pv_va;

			/*
			 * don't write protect pager mappings
			 */
			if (!setem && (bit == PG_RW)) {
				if (va >= clean_sva && va < clean_eva)
					continue;
			}
			if (!pv->pv_pmap) {
				printf("Null pmap (cb) at va: 0x%lx\n", va);
				continue;
			}
			pte = pmap_pte(pv->pv_pmap, va);
			if (setem) {
				(int) npte = (int) *pte | bit;
			} else {
				(int) npte = (int) *pte & ~bit;
			}
			*pte = npte;
		}
	}
	splx(s);
	if (curproc != pageproc)
		pmap_update();
}

/*
 *      pmap_page_protect:
 *
 *      Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(phys, prot)
	vm_offset_t phys;
	vm_prot_t prot;
{
	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ | VM_PROT_EXECUTE))
			pmap_changebit(phys, PG_RW, FALSE);
		else
			pmap_remove_all(phys);
	}
}

vm_offset_t
pmap_phys_address(ppn)
	int ppn;
{
	return (i386_ptob(ppn));
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page was referenced
 *	by any physical maps.
 */
boolean_t
pmap_is_referenced(vm_offset_t pa)
{
	return pmap_testbit((pa), PG_U);
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page was modified
 *	in any physical maps.
 */
boolean_t
pmap_is_modified(vm_offset_t pa)
{
	return pmap_testbit((pa), PG_M);
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_offset_t pa)
{
	pmap_changebit((pa), PG_M, FALSE);
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */
void
pmap_clear_reference(vm_offset_t pa)
{
	pmap_changebit((pa), PG_U, FALSE);
}

/*
 * Miscellaneous support routines follow
 */

static void
i386_protection_init()
{
	register int *kp, prot;

	kp = protection_codes;
	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
			/*
			 * Read access is also 0. There isn't any execute bit,
			 * so just make it readable.
			 */
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = 0;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = PG_RW;
			break;
		}
	}
}

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory. The non-cacheable bits are set on each
 * mapped page.
 */
void *
pmap_mapdev(pa, size)
	vm_offset_t pa;
	vm_size_t size;
{
	vm_offset_t va, tmpva;
	pt_entry_t *pte;

	pa = trunc_page(pa);
	size = roundup(size, PAGE_SIZE);

	va = kmem_alloc_pageable(kernel_map, size);
	if (!va)
		panic("pmap_mapdev: Couldn't alloc kernel virtual memory");

	for (tmpva = va; size > 0;) {
		pte = vtopte(tmpva);
		*pte = (pt_entry_t) ((int) (pa | PG_RW | PG_V | PG_N));
		size -= PAGE_SIZE;
		tmpva += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	pmap_update();

	return ((void *) va);
}

#ifdef PMAP_DEBUG
pmap_pid_dump(int pid) {
	pmap_t pmap;
	struct proc *p;
	int npte = 0;
	int index;
	for (p = (struct proc *) allproc; p != NULL; p = p->p_next) {
		if (p->p_pid != pid)
			continue;

		if (p->p_vmspace) {
			int i,j;
			index = 0;
			pmap = &p->p_vmspace->vm_pmap;
			for(i=0;i<1024;i++) {
				pd_entry_t *pde;
				pt_entry_t *pte;
				unsigned base = i << PD_SHIFT;
				
				pde = &pmap->pm_pdir[i];
				if (pde && pmap_pde_v(pde)) {
					for(j=0;j<1024;j++) {
						unsigned va = base + (j << PG_SHIFT);
						if (va >= (vm_offset_t) VM_MIN_KERNEL_ADDRESS) {
							if (index) {
								index = 0;
								printf("\n");
							}
							return npte;
						}
						pte = pmap_pte( pmap, va);
						if (pte && pmap_pte_v(pte)) {
							vm_offset_t pa;
							vm_page_t m;
							pa = *(int *)pte;
							m = PHYS_TO_VM_PAGE((pa & PG_FRAME));
							printf("va: 0x%x, pt: 0x%x, h: %d, w: %d, f: 0x%x",
								va, pa, m->hold_count, m->wire_count, m->flags);
							npte++;
							index++;
							if (index >= 2) {
								index = 0;
								printf("\n");
							} else {
								printf(" ");
							}
						}
					}
				}
			}
		}
	}
	return npte;
}
#endif

#ifdef DEBUG

static void	pads __P((pmap_t pm));
static void	pmap_pvdump __P((vm_offset_t pa));

/* print address space of pmap*/
static void
pads(pm)
	pmap_t pm;
{
	unsigned va, i, j;
	pt_entry_t *ptep;

	if (pm == kernel_pmap)
		return;
	for (i = 0; i < 1024; i++)
		if (pm->pm_pdir[i])
			for (j = 0; j < 1024; j++) {
				va = (i << PD_SHIFT) + (j << PG_SHIFT);
				if (pm == kernel_pmap && va < KERNBASE)
					continue;
				if (pm != kernel_pmap && va > UPT_MAX_ADDRESS)
					continue;
				ptep = pmap_pte(pm, va);
				if (pmap_pte_v(ptep))
					printf("%x:%x ", va, *(int *) ptep);
			};

}

static void
pmap_pvdump(pa)
	vm_offset_t pa;
{
	register pv_entry_t pv;

	printf("pa %x", pa);
	for (pv = pa_to_pvh(pa); pv; pv = pv->pv_next) {
#ifdef used_to_be
		printf(" -> pmap %x, va %x, flags %x",
		    pv->pv_pmap, pv->pv_va, pv->pv_flags);
#endif
		printf(" -> pmap %x, va %x",
		    pv->pv_pmap, pv->pv_va);
		pads(pv->pv_pmap);
	}
	printf(" ");
}
#endif
