/*-
 * Copyright (c) 2003 Jake Burkholder.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/nexusvar.h>
#include <machine/ofw_upa.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <sparc64/sbus/ofw_sbus.h>

struct central_devinfo {
	char			*cdi_compat;
	char			*cdi_model;
	char			*cdi_name;
	char			*cdi_type;
	phandle_t		cdi_node;
	struct resource_list	cdi_rl;
};

struct central_softc {
	phandle_t		sc_node;
	int			sc_nrange;
	struct sbus_ranges	*sc_ranges;
};

static device_probe_t central_probe;
static device_attach_t central_attach;
static bus_print_child_t central_print_child;
static bus_probe_nomatch_t central_probe_nomatch;
static bus_alloc_resource_t central_alloc_resource;
static bus_release_resource_t central_release_resource;
static ofw_bus_get_compat_t central_get_compat;
static ofw_bus_get_model_t central_get_model;
static ofw_bus_get_name_t central_get_name;
static ofw_bus_get_node_t central_get_node;
static ofw_bus_get_type_t central_get_type;

static device_method_t central_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		central_probe),
	DEVMETHOD(device_attach,	central_attach),

	/* Bus interface. */
	DEVMETHOD(bus_print_child,	central_print_child),
	DEVMETHOD(bus_probe_nomatch,	central_probe_nomatch),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	central_alloc_resource),
	DEVMETHOD(bus_release_resource,	central_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_compat,	central_get_compat),
	DEVMETHOD(ofw_bus_get_model,	central_get_model),
	DEVMETHOD(ofw_bus_get_name,	central_get_name),
	DEVMETHOD(ofw_bus_get_node,	central_get_node),
	DEVMETHOD(ofw_bus_get_type,	central_get_type),

	{ NULL, NULL }
};

static driver_t central_driver = {
	"central",
	central_methods,
	sizeof(struct central_softc),
};

static devclass_t central_devclass;

DRIVER_MODULE(central, nexus, central_driver, central_devclass, 0, 0);

static int
central_probe(device_t dev)
{

	if (strcmp(nexus_get_name(dev), "central") == 0) {
		device_set_desc(dev, "central");
		return (0);
	}
	return (ENXIO);
}

static int
central_attach(device_t dev)
{
	struct central_devinfo *cdi;
	struct sbus_regs *reg;
	struct central_softc *sc;
	phandle_t child;
	phandle_t node;
	bus_addr_t size;
	bus_addr_t off;
	device_t cdev;
	char *name;
	int nreg;
	int i;

	sc = device_get_softc(dev);
	node = nexus_get_node(dev);
	sc->sc_node = node;

	sc->sc_nrange = OF_getprop_alloc(node, "ranges",
	    sizeof(*sc->sc_ranges), (void **)&sc->sc_ranges);
	if (sc->sc_nrange == -1) {
		device_printf(dev, "can't get ranges\n");
		return (ENXIO);
	}

	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if ((OF_getprop_alloc(child, "name", 1, (void **)&name)) == -1)
			continue;
		cdev = device_add_child(dev, NULL, -1);
		if (cdev != NULL) {
			cdi = malloc(sizeof(*cdi), M_DEVBUF, M_WAITOK | M_ZERO);
			if (cdi == NULL)
				continue;
			cdi->cdi_name = name;
			cdi->cdi_node = child;
			OF_getprop_alloc(child, "compatible", 1,
			    (void **)&cdi->cdi_compat);
			OF_getprop_alloc(child, "device_type", 1,
			    (void **)&cdi->cdi_type);
			OF_getprop_alloc(child, "model", 1,
			    (void **)&cdi->cdi_model);
			resource_list_init(&cdi->cdi_rl);
			nreg = OF_getprop_alloc(child, "reg", sizeof(*reg),
			    (void **)&reg);
			if (nreg != -1) {
				for (i = 0; i < nreg; i++) {
					off = reg[i].sbr_offset;
					size = reg[i].sbr_size;
					resource_list_add(&cdi->cdi_rl,
					    SYS_RES_MEMORY, i, off, off + size,
					    size);
				}
				free(reg, M_OFWPROP);
			}
			device_set_ivars(cdev, cdi);
		} else
			free(name, M_OFWPROP);
	}

	return (bus_generic_attach(dev));
}

static int
central_print_child(device_t dev, device_t child)
{
	struct central_devinfo *cdi;
	int rv;

	cdi = device_get_ivars(child);
	rv = bus_print_child_header(dev, child);
	rv += resource_list_print_type(&cdi->cdi_rl, "mem",
	    SYS_RES_MEMORY, "%#lx");
	rv += bus_print_child_footer(dev, child);
	return (rv);
}

static void
central_probe_nomatch(device_t dev, device_t child)
{
	struct central_devinfo *cdi;

	cdi = device_get_ivars(child);
	device_printf(dev, "<%s>", cdi->cdi_name);
	resource_list_print_type(&cdi->cdi_rl, "mem", SYS_RES_MEMORY, "%#lx");
	printf(" type %s (no driver attached)\n",
	    cdi->cdi_type != NULL ? cdi->cdi_type : "unknown");
}

static struct resource *
central_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct resource_list_entry *rle;
	struct central_devinfo *cdi;
	struct central_softc *sc;
	struct resource *res;
	bus_addr_t coffset;
	bus_addr_t cend;
	bus_addr_t phys;
	int isdefault;
	int i;

	isdefault = (start == 0UL && end == ~0UL);
	res = NULL;
	sc = device_get_softc(bus);
	cdi = device_get_ivars(child);
	rle = resource_list_find(&cdi->cdi_rl, type, *rid);
	if (rle == NULL)
		return (NULL);
	if (rle->res != NULL)
		panic("%s: resource entry is busy", __func__);
	if (isdefault) {
		start = rle->start;
		count = ulmax(count, rle->count);
		end = ulmax(rle->end, start + count - 1);
	}
	switch (type) {
	case SYS_RES_IRQ:
		res = bus_generic_alloc_resource(bus, child, type, rid,
		    start, end, count, flags);
		break;
	case SYS_RES_MEMORY:
		for (i = 0; i < sc->sc_nrange; i++) {
			coffset = sc->sc_ranges[i].coffset;
			cend = coffset + sc->sc_ranges[i].size - 1;
			if (start >= coffset && end <= cend) {
				start -= coffset;
				end -= coffset;
				phys = sc->sc_ranges[i].poffset |
				    ((bus_addr_t)sc->sc_ranges[i].pspace << 32);
				res = bus_generic_alloc_resource(bus, child,
				    type, rid, phys + start, phys + end,
				    count, flags);
				rle->res = res;
				break;
			}
		}
		break;
	default:
		break;
	}
	return (res);
}

static int
central_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	struct resource_list_entry *rle;
	struct central_devinfo *cdi;
	int error;

	error = bus_generic_release_resource(bus, child, type, rid, r);
	if (error != 0)
		return (error);
	cdi = device_get_ivars(child);
	rle = resource_list_find(&cdi->cdi_rl, type, rid);
	if (rle == NULL)
		panic("%s: can't find resource", __func__);
	if (rle->res == NULL)
		panic("%s: resource entry is not busy", __func__);
	rle->res = NULL;
	return (error);
}

static const char *
central_get_compat(device_t bus, device_t dev)
{
	struct central_devinfo *dinfo;
 
	dinfo = device_get_ivars(dev);
	return (dinfo->cdi_compat);
}
 
static const char *
central_get_model(device_t bus, device_t dev)
{
	struct central_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->cdi_model);
}

static const char *
central_get_name(device_t bus, device_t dev)
{
	struct central_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->cdi_name);
}

static phandle_t
central_get_node(device_t bus, device_t dev)
{
	struct central_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->cdi_node);
}

static const char *
central_get_type(device_t bus, device_t dev)
{
	struct central_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->cdi_type);
}
