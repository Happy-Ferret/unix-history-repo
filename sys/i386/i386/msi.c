/*-
 * Copyright (c) 2006 Yahoo!, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Support for PCI Message Signalled Interrupts (MSI).  MSI interrupts on
 * x86 are basically APIC messages that the northbridge delivers directly
 * to the local APICs as if they had come from an I/O APIC.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/systm.h>
#include <machine/apicreg.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/apicvar.h>
#include <machine/specialreg.h>
#include <dev/pci/pcivar.h>

/* Fields in address for Intel MSI messages. */
#define	MSI_INTEL_ADDR_DEST		0x000ff000
#define	MSI_INTEL_ADDR_RH		0x00000008
# define MSI_INTEL_ADDR_RH_ON		0x00000008
# define MSI_INTEL_ADDR_RH_OFF		0x00000000
#define	MSI_INTEL_ADDR_DM		0x00000004
# define MSI_INTEL_ADDR_DM_PHYSICAL	0x00000000
# define MSI_INTEL_ADDR_DM_LOGICAL	0x00000004

/* Fields in data for Intel MSI messages. */
#define	MSI_INTEL_DATA_TRGRMOD		IOART_TRGRMOD	/* Trigger mode. */
# define MSI_INTEL_DATA_TRGREDG		IOART_TRGREDG
# define MSI_INTEL_DATA_TRGRLVL		IOART_TRGRLVL
#define	MSI_INTEL_DATA_LEVEL		0x00004000	/* Polarity. */
# define MSI_INTEL_DATA_DEASSERT	0x00000000
# define MSI_INTEL_DATA_ASSERT		0x00004000
#define	MSI_INTEL_DATA_DELMOD		IOART_DELMOD	/* Delivery mode. */
# define MSI_INTEL_DATA_DELFIXED	IOART_DELFIXED
# define MSI_INTEL_DATA_DELLOPRI	IOART_DELLOPRI
# define MSI_INTEL_DATA_DELSMI		IOART_DELSMI
# define MSI_INTEL_DATA_DELNMI		IOART_DELNMI
# define MSI_INTEL_DATA_DELINIT		IOART_DELINIT
# define MSI_INTEL_DATA_DELEXINT	IOART_DELEXINT
#define	MSI_INTEL_DATA_INTVEC		IOART_INTVEC	/* Interrupt vector. */

/*
 * Build Intel MSI message and data values from a source.  AMD64 systems
 * seem to be compatible, so we use the same function for both.
 */
#define	INTEL_ADDR(msi)							\
	(MSI_INTEL_ADDR_BASE | (msi)->msi_cpu << 12 |			\
	    MSI_INTEL_ADDR_RH_OFF | MSI_INTEL_ADDR_DM_PHYSICAL)
#define	INTEL_DATA(msi)							\
	(MSI_INTEL_DATA_TRGREDG | MSI_INTEL_DATA_DELFIXED | (msi)->msi_vector)

static MALLOC_DEFINE(M_MSI, "msi", "PCI MSI");

/*
 * MSI sources are bunched into groups.  This is because MSI forces
 * all of the messages to share the address and data registers and
 * thus certain properties (such as the local APIC ID target on x86).
 * Each group has a 'first' source that contains information global to
 * the group.  These fields are marked with (g) below.
 *
 * Note that local APIC ID is kind of special.  Each message will be
 * assigned an ID by the system; however, a group will use the ID from
 * the first message.
 *
 * For MSI-X, each message is isolated.
 */
struct msi_intsrc {
	struct intsrc msi_intsrc;
	device_t msi_dev;		/* Owning device. (g) */
	struct msi_intsrc *msi_first;	/* First source in group. */
	u_int msi_irq;			/* IRQ cookie. */
	u_int msi_msix;			/* MSI-X message. */
	u_int msi_vector:8;		/* IDT vector. */
	u_int msi_cpu:8;		/* Local APIC ID. (g) */
	u_int msi_count:8;		/* Messages in this group. (g) */
};

static void	msi_create_source(void);
static void	msi_enable_source(struct intsrc *isrc);
static void	msi_disable_source(struct intsrc *isrc, int eoi);
static void	msi_eoi_source(struct intsrc *isrc);
static void	msi_enable_intr(struct intsrc *isrc);
static void	msi_disable_intr(struct intsrc *isrc);
static int	msi_vector(struct intsrc *isrc);
static int	msi_source_pending(struct intsrc *isrc);
static int	msi_config_intr(struct intsrc *isrc, enum intr_trigger trig,
		    enum intr_polarity pol);
static void	msi_assign_cpu(struct intsrc *isrc, u_int apic_id);

struct pic msi_pic = { msi_enable_source, msi_disable_source, msi_eoi_source,
		       msi_enable_intr, msi_disable_intr, msi_vector,
		       msi_source_pending, NULL, NULL, msi_config_intr,
		       msi_assign_cpu };

static int msi_enabled;
static int msi_last_irq;
static struct mtx msi_lock;

static void
msi_enable_source(struct intsrc *isrc)
{
}

static void
msi_disable_source(struct intsrc *isrc, int eoi)
{

	if (eoi == PIC_EOI)
		lapic_eoi();
}

static void
msi_eoi_source(struct intsrc *isrc)
{

	lapic_eoi();
}

static void
msi_enable_intr(struct intsrc *isrc)
{
	struct msi_intsrc *msi = (struct msi_intsrc *)isrc;

	if (msi->msi_vector == 0)
		msi_assign_cpu(isrc, 0);
	apic_enable_vector(msi->msi_cpu, msi->msi_vector);
}

static void
msi_disable_intr(struct intsrc *isrc)
{
	struct msi_intsrc *msi = (struct msi_intsrc *)isrc;

	apic_disable_vector(msi->msi_cpu, msi->msi_vector);
}

static int
msi_vector(struct intsrc *isrc)
{
	struct msi_intsrc *msi = (struct msi_intsrc *)isrc;

	return (msi->msi_irq);
}

static int
msi_source_pending(struct intsrc *isrc)
{

	return (0);
}

static int
msi_config_intr(struct intsrc *isrc, enum intr_trigger trig,
    enum intr_polarity pol)
{

	return (ENODEV);
}

static void
msi_assign_cpu(struct intsrc *isrc, u_int apic_id)
{
	struct msi_intsrc *msi = (struct msi_intsrc *)isrc;
	int old_vector;
	u_int old_id;
	int vector;

	/* Store information to free existing irq. */
	old_vector = msi->msi_vector;
	old_id = msi->msi_cpu;
	if (old_vector && old_id == apic_id)
		return;
	/* Allocate IDT vector on this cpu. */
	vector = apic_alloc_vector(apic_id, msi->msi_irq);
	if (vector == 0)
		return; /* XXX alloc_vector panics on failure. */
	msi->msi_cpu = apic_id;
	msi->msi_vector = vector;
	if (bootverbose)
		printf("msi: Assigning %s IRQ %d to local APIC %u vector %u\n",
		    msi->msi_msix ? "MSI-X" : "MSI", msi->msi_irq,
		    msi->msi_cpu, msi->msi_vector);
	pci_remap_msi_irq(msi->msi_dev, msi->msi_irq);
	/*
	 * Free the old vector after the new one is established.  This is done
	 * to prevent races where we could miss an interrupt.
	 */
	if (old_vector)
		apic_free_vector(old_id, old_vector, msi->msi_irq);
}


void
msi_init(void)
{

	/* Check if we have a supported CPU. */
	switch (cpu_vendor_id) {
	case CPU_VENDOR_INTEL:
	case CPU_VENDOR_AMD:
		break;
	case CPU_VENDOR_CENTAUR:
		if (I386_CPU_FAMILY(cpu_id) == 0x6 &&
		    I386_CPU_MODEL(cpu_id) >= 0xf)
			break;
		/* FALLTHROUGH */
	default:
		return;
	}

	msi_enabled = 1;
	intr_register_pic(&msi_pic);
	mtx_init(&msi_lock, "msi", NULL, MTX_DEF);
}

void
msi_create_source(void)
{
	struct msi_intsrc *msi;
	u_int irq;

	mtx_lock(&msi_lock);
	if (msi_last_irq >= NUM_MSI_INTS) {
		mtx_unlock(&msi_lock);
		return;
	}
	irq = msi_last_irq + FIRST_MSI_INT;
	msi_last_irq++;
	mtx_unlock(&msi_lock);

	msi = malloc(sizeof(struct msi_intsrc), M_MSI, M_WAITOK | M_ZERO);	
	msi->msi_intsrc.is_pic = &msi_pic;
	msi->msi_irq = irq;
	intr_register_source(&msi->msi_intsrc);
	nexus_add_irq(irq);
}

/*
 * Try to allocate 'count' interrupt sources with contiguous IDT values.  If
 * we allocate any new sources, then their IRQ values will be at the end of
 * the irqs[] array, with *newirq being the index of the first new IRQ value
 * and *newcount being the number of new IRQ values added.
 */
int
msi_alloc(device_t dev, int count, int maxcount, int *irqs)
{
	struct msi_intsrc *msi, *fsrc;
	int cnt, i;

	if (!msi_enabled)
		return (ENXIO);

again:
	mtx_lock(&msi_lock);

	/* Try to find 'count' free IRQs. */
	cnt = 0;
	for (i = FIRST_MSI_INT; i < FIRST_MSI_INT + NUM_MSI_INTS; i++) {
		msi = (struct msi_intsrc *)intr_lookup_source(i);

		/* End of allocated sources, so break. */
		if (msi == NULL)
			break;

		/* If this is a free one, save its IRQ in the array. */
		if (msi->msi_dev == NULL) {
			irqs[cnt] = i;
			cnt++;
			if (cnt == count)
				break;
		}
	}

	/* Do we need to create some new sources? */
	if (cnt < count) {
		/* If we would exceed the max, give up. */
		if (i + (count - cnt) > FIRST_MSI_INT + NUM_MSI_INTS) {
			mtx_unlock(&msi_lock);
			return (ENXIO);
		}
		mtx_unlock(&msi_lock);

		/* We need count - cnt more sources. */
		while (cnt < count) {
			msi_create_source();
			cnt++;
		}
		goto again;
	}

	/* Ok, we now have the IRQs allocated. */
	KASSERT(cnt == count, ("count mismatch"));

	/* Assign IDT vectors and make these messages owned by 'dev'. */
	fsrc = (struct msi_intsrc *)intr_lookup_source(irqs[0]);
	for (i = 0; i < count; i++) {
		msi = (struct msi_intsrc *)intr_lookup_source(irqs[i]);
		msi->msi_dev = dev;
		msi->msi_vector = 0;
		msi->msi_first = fsrc;
		KASSERT(msi->msi_intsrc.is_handlers == 0,
		    ("dead MSI has handlers"));
	}
	fsrc->msi_count = count;
	mtx_unlock(&msi_lock);

	return (0);
}

int
msi_release(int *irqs, int count)
{
	struct msi_intsrc *msi, *first;
	int i;

	mtx_lock(&msi_lock);
	first = (struct msi_intsrc *)intr_lookup_source(irqs[0]);
	if (first == NULL) {
		mtx_unlock(&msi_lock);
		return (ENOENT);
	}

	/* Make sure this isn't an MSI-X message. */
	if (first->msi_msix) {
		mtx_unlock(&msi_lock);
		return (EINVAL);
	}

	/* Make sure this message is allocated to a group. */
	if (first->msi_first == NULL) {
		mtx_unlock(&msi_lock);
		return (ENXIO);
	}

	/*
	 * Make sure this is the start of a group and that we are releasing
	 * the entire group.
	 */
	if (first->msi_first != first || first->msi_count != count) {
		mtx_unlock(&msi_lock);
		return (EINVAL);
	}
	KASSERT(first->msi_dev != NULL, ("unowned group"));

	/* Clear all the extra messages in the group. */
	for (i = 1; i < count; i++) {
		msi = (struct msi_intsrc *)intr_lookup_source(irqs[i]);
		KASSERT(msi->msi_first == first, ("message not in group"));
		KASSERT(msi->msi_dev == first->msi_dev, ("owner mismatch"));
		msi->msi_first = NULL;
		msi->msi_dev = NULL;
		if (msi->msi_vector)
			apic_free_vector(msi->msi_cpu, msi->msi_vector,
			    msi->msi_irq);
		msi->msi_vector = 0;
	}

	/* Clear out the first message. */
	first->msi_first = NULL;
	first->msi_dev = NULL;
	if (first->msi_vector)
		apic_free_vector(first->msi_cpu, first->msi_vector,
		    first->msi_irq);
	first->msi_vector = 0;
	first->msi_count = 0;

	mtx_unlock(&msi_lock);
	return (0);
}

int
msi_map(int irq, uint64_t *addr, uint32_t *data)
{
	struct msi_intsrc *msi;

	mtx_lock(&msi_lock);
	msi = (struct msi_intsrc *)intr_lookup_source(irq);
	if (msi == NULL) {
		mtx_unlock(&msi_lock);
		return (ENOENT);
	}

	/* Make sure this message is allocated to a device. */
	if (msi->msi_dev == NULL) {
		mtx_unlock(&msi_lock);
		return (ENXIO);
	}

	/*
	 * If this message isn't an MSI-X message, make sure it's part
	 * of a group, and switch to the first message in the
	 * group.
	 */
	if (!msi->msi_msix) {
		if (msi->msi_first == NULL) {
			mtx_unlock(&msi_lock);
			return (ENXIO);
		}
		msi = msi->msi_first;
	}

	*addr = INTEL_ADDR(msi);
	*data = INTEL_DATA(msi);
	mtx_unlock(&msi_lock);
	return (0);
}

int
msix_alloc(device_t dev, int *irq)
{
	struct msi_intsrc *msi;
	int i;

	if (!msi_enabled)
		return (ENXIO);

again:
	mtx_lock(&msi_lock);

	/* Find a free IRQ. */
	for (i = FIRST_MSI_INT; i < FIRST_MSI_INT + NUM_MSI_INTS; i++) {
		msi = (struct msi_intsrc *)intr_lookup_source(i);

		/* End of allocated sources, so break. */
		if (msi == NULL)
			break;

		/* Stop at the first free source. */
		if (msi->msi_dev == NULL)
			break;
	}

	/* Do we need to create a new source? */
	if (msi == NULL) {
		/* If we would exceed the max, give up. */
		if (i + 1 > FIRST_MSI_INT + NUM_MSI_INTS) {
			mtx_unlock(&msi_lock);
			return (ENXIO);
		}
		mtx_unlock(&msi_lock);

		/* Create a new source. */
		msi_create_source();
		goto again;
	}

	/* Setup source. */
	msi->msi_dev = dev;
	msi->msi_vector = 0;
	msi->msi_msix = 1;

	KASSERT(msi->msi_intsrc.is_handlers == 0, ("dead MSI-X has handlers"));
	mtx_unlock(&msi_lock);

	*irq = i;
	return (0);
}

int
msix_release(int irq)
{
	struct msi_intsrc *msi;

	mtx_lock(&msi_lock);
	msi = (struct msi_intsrc *)intr_lookup_source(irq);
	if (msi == NULL) {
		mtx_unlock(&msi_lock);
		return (ENOENT);
	}

	/* Make sure this is an MSI-X message. */
	if (!msi->msi_msix) {
		mtx_unlock(&msi_lock);
		return (EINVAL);
	}

	KASSERT(msi->msi_dev != NULL, ("unowned message"));

	/* Clear out the message. */
	msi->msi_dev = NULL;
	if (msi->msi_vector)
		apic_free_vector(msi->msi_cpu, msi->msi_vector, msi->msi_irq);
	msi->msi_vector = 0;
	msi->msi_msix = 0;

	mtx_unlock(&msi_lock);
	return (0);
}
