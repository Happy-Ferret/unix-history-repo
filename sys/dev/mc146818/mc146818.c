/*
 * Copyright (c) 2003 Izumi Tsutsui.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: NetBSD: mc146818.c,v 1.4 2003/11/24 06:20:40 tsutsui Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * mc146818 and compatible time of day chip subroutines
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>

#include <machine/bus.h>

#include <dev/mc146818/mc146818reg.h>
#include <dev/mc146818/mc146818var.h>

#include "clock_if.h"

static u_int	mc146818_def_getcent(device_t);
static void	mc146818_def_setcent(device_t, u_int);
static u_int	mc146818_def_read(device_t, u_int);
static void	mc146818_def_write(device_t, u_int, u_int);

int
mc146818_attach(device_t dev)
{
	struct mc146818_softc *sc;

	sc = device_get_softc(dev);

	if (sc->sc_mcread == NULL)
		sc->sc_mcread = mc146818_def_read;
	if (sc->sc_mcwrite == NULL)
		sc->sc_mcwrite = mc146818_def_write;

	if (sc->sc_flag & MC146818_NO_CENT_ADJUST) {
		/*
		 * Note that setting MC146818_NO_CENT_ADJUST means that
		 * the century has to be stored in NVRAM somewhere.
		 */
		if (sc->sc_getcent == NULL)
			sc->sc_getcent = mc146818_def_getcent;
		if (sc->sc_setcent == NULL)
			sc->sc_setcent = mc146818_def_setcent;
	}

	if (!(*sc->sc_mcread)(dev, MC_REGD) & MC_REGD_VRT) {
		device_printf(dev, "mc146818_attach: battery low\n");
		return (ENXIO);
	}

	sc->sc_rega = MC_BASE_32_KHz;
	(*sc->sc_mcwrite)(dev, MC_REGA, sc->sc_rega);

	sc->sc_regb = 0;
	sc->sc_regb |= (sc->sc_flag & MC146818_BCD) ? 0 : MC_REGB_BINARY;
	sc->sc_regb |= (sc->sc_flag & MC146818_12HR) ? 0 : MC_REGB_24HR;
	(*sc->sc_mcwrite)(dev, MC_REGB, sc->sc_regb);

	clock_register(dev, 1000000);	/* 1 second resolution. */

	return (0);
}

/*
 * Get time of day and convert it to a struct timespec.
 * Return 0 on success, an error number otherwise.
 */
int
mc146818_gettime(device_t dev, struct timespec *ts)
{
	struct mc146818_softc *sc;
	struct clocktime ct;
	int timeout, cent, year;

	sc = device_get_softc(dev);

	timeout = 1000000;	/* XXX how long should we wait? */

	/*
	 * XXX:	Use a spinlock to mutex register access and increase the
	 *	likelihood that all registers are read before an update
	 *	occurs.
	 */

	/*
	 * If MC_REGA_UIP is 0 we have at least 244us before the next
	 * update. If it's 1 an update is imminent.
	 */
	for (;;) {
		if (!((*sc->sc_mcread)(dev, MC_REGA) & MC_REGA_UIP))
			break;
		if (--timeout < 0) {
			device_printf(dev, "mc146818_gettime: timeout\n");
			return (EBUSY);
		}
	}

#define	FROMREG(x)	((sc->sc_flag & MC146818_BCD) ? FROMBCD(x) : (x))

	ct.nsec = 0;
	ct.sec = FROMREG((*sc->sc_mcread)(dev, MC_SEC));
	ct.min = FROMREG((*sc->sc_mcread)(dev, MC_MIN));
	ct.hour = FROMREG((*sc->sc_mcread)(dev, MC_HOUR));
	ct.dow = FROMREG((*sc->sc_mcread)(dev, MC_DOW)) - 1;
	ct.day = FROMREG((*sc->sc_mcread)(dev, MC_DOM));
	ct.mon = FROMREG((*sc->sc_mcread)(dev, MC_MONTH));
	year = FROMREG((*sc->sc_mcread)(dev, MC_YEAR));
	if (sc->sc_getcent) {
		cent = (*sc->sc_getcent)(dev);
		year += cent * 100;
	}

	year += sc->sc_year0;
	if (year < POSIX_BASE_YEAR && !(sc->sc_flag & MC146818_NO_CENT_ADJUST))
		year += 100;
	ct.year = year;

	return (clock_ct_to_ts(&ct, ts));
}

#ifdef notyet
int
mc146818_getsecs(device_t dev, int *secp)
{
	struct mc146818_softc *sc;
	int sec, timeout;

	sc = device_get_softc(dev);

	timeout = 1000000;	/* XXX how long should we wait? */

	for (;;) {
		if (!((*sc->sc_mcread)(dev, MC_REGA) & MC_REGA_UIP)) {
			sec = FROMREG((*sc->sc_mcread)(dev, MC_SEC));
			break;
		}
		if (--timeout == 0) {
			device_printf(dev, "mc146818_getsecs: timeout\n");
			return (EBUSY);
		}
	}

#undef FROMREG

	*secp = sec;
	return (0);
}
#endif

/*
 * Set the time of day clock based on the value of the struct timespec arg.
 * Return 0 on success, an error number otherwise.
 */
int
mc146818_settime(device_t dev, struct timespec *ts)
{
	struct mc146818_softc *sc;
	struct clocktime ct;
	int cent, year;

	sc = device_get_softc(dev);

	/* Accuracy is only one second. */
	if (ts->tv_nsec >= 500000000)
		ts->tv_sec++;
	ts->tv_nsec = 0;
	clock_ts_to_ct(ts, &ct);

	/* Disable RTC updates and interrupts (if enabled). */
	(*sc->sc_mcwrite)(dev, MC_REGB,
	    ((sc->sc_regb & (MC_REGB_BINARY | MC_REGB_24HR)) | MC_REGB_SET));

#define	TOREG(x)	((sc->sc_flag & MC146818_BCD) ? TOBCD(x) : (x))

	(*sc->sc_mcwrite)(dev, MC_SEC, TOREG(ct.sec));
	(*sc->sc_mcwrite)(dev, MC_MIN, TOREG(ct.min));
	(*sc->sc_mcwrite)(dev, MC_HOUR, TOREG(ct.hour));
	(*sc->sc_mcwrite)(dev, MC_DOW, TOREG(ct.dow + 1));
	(*sc->sc_mcwrite)(dev, MC_DOM, TOREG(ct.day));
	(*sc->sc_mcwrite)(dev, MC_MONTH, TOREG(ct.mon));

	year = ct.year - sc->sc_year0;
	if (sc->sc_setcent) {
		cent = year / 100;
		(*sc->sc_setcent)(dev, cent);
		year -= cent * 100;
	}
	if (year > 99 && (sc->sc_flag & MC146818_NO_CENT_ADJUST) == 0)
		year -= 100;
	(*sc->sc_mcwrite)(dev, MC_YEAR, TOREG(year));

	/* Reenable RTC updates and interrupts. */
	(*sc->sc_mcwrite)(dev, MC_REGB, sc->sc_regb);

#undef TOREG

	return (0);
}

#define	MC_ADDR	0
#define	MC_DATA	1

static u_int
mc146818_def_read(device_t dev, u_int reg)
{
	struct mc146818_softc *sc;

	sc = device_get_softc(dev);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, MC_ADDR, reg);
	return (bus_space_read_1(sc->sc_bst, sc->sc_bsh, MC_DATA));
}

static void
mc146818_def_write(device_t dev, u_int reg, u_int val)
{
	struct mc146818_softc *sc;

	sc = device_get_softc(dev);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, MC_ADDR, reg);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, MC_DATA, val);
}

/*
 * Looks like it's common even across platforms to store the century at
 * 0x32 in the NVRAM of the mc146818.
 */
#define	MC_CENT	0x32

static u_int
mc146818_def_getcent(device_t dev)
{
	struct mc146818_softc *sc;

	sc = device_get_softc(dev);
	return ((*sc->sc_mcread)(dev, MC_CENT));
}

static void
mc146818_def_setcent(device_t dev, u_int cent)
{
	struct mc146818_softc *sc;

	sc = device_get_softc(dev);
	(*sc->sc_mcwrite)(dev, MC_CENT, cent);
}
