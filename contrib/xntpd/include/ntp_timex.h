/******************************************************************************
 *                                                                            *
 * Copyright (c) David L. Mills 1993, 1994                                    *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appears in all copies and that both the    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name University of Delaware not be used in     *
 * advertising or publicity pertaining to distribution of the software        *
 * without specific, written prior permission.  The University of Delaware    *
 * makes no representations about the suitability this software for any       *
 * purpose.  It is provided "as is" without express or implied warranty.      *
 *                                                                            *
 ******************************************************************************/

/*
 * Modification history timex.h
 *
 * 20 Feb 94	David L. Mills
 *	Revised status codes and structures for external clock and PPS
 *	signal discipline.
 *
 * 28 Nov 93	David L. Mills
 *	Adjusted parameters to improve stability and increase poll
 *	interval.
 * 
 * 17 Sep 93    David L. Mills
 *      Created file
 */
/*
 * This header file defines the Network Time Protocol (NTP) interfaces
 * for user and daemon application programs. These are implemented using
 * private syscalls and data structures and require specific kernel
 * support.
 *
 * NAME
 *	ntp_gettime - NTP user application interface
 *
 * SYNOPSIS
 *	#include <sys/timex.h>
 *
 *	int syscall(SYS_ntp_gettime, tptr)
 *
 *	int SYS_ntp_gettime		defined in syscall.h header file
 *	struct ntptimeval *tptr		pointer to ntptimeval structure
 *
 * NAME
 *	ntp_adjtime - NTP daemon application interface
 *
 * SYNOPSIS
 *	#include <sys/timex.h>
 *
 *	int syscall(SYS_ntp_adjtime, mode, tptr)
 *
 *	int SYS_ntp_adjtime		defined in syscall.h header file
 *	struct timex *tptr		pointer to timex structure
 *
 */
#include <sys/syscall.h>

/*
 * The following defines establish the engineering parameters of the PLL
 * model. The hz variable is defined in the kernel build environment. It
 * establishes the timer interrupt frequency, 100 Hz for the SunOS
 * kernel, 256 Hz for the Ultrix kernel and 1024 Hz for the OSF/1
 * kernel. The SHIFT_HZ define expresses the same value as the nearest
 * power of two in order to avoid hardware multiply operations.
 */
#define SHIFT_HZ 7		/* log2(hz) */

/*
 * The SHIFT_KG and SHIFT_KF defines establish the damping of the PLL
 * and are chosen by analysis for a slightly underdamped convergence
 * characteristic. The MAXTC define establishes the maximum time
 * constant of the PLL. With the parameters given and the minimum time
 * constant of zero, the PLL will converge in about 15 minutes.
 */
#define SHIFT_KG 6		/* phase factor (shift) */
#define SHIFT_KF 16		/* frequency factor (shift) */
#define MAXTC 6			/* maximum time constant (shift) */

/*
 * SHIFT_SCALE defines the scaling (shift) of the time_phase variable,
 * which serves as a an extension to the low-order bits of the system
 * clock variable time.tv_usec. SHIFT_UPDATE defines the scaling (shift)
 * of the time_offset variable, which represents the current time offset
 * with respect to standard time. SHIFT_USEC defines the scaling (shift)
 * of the time_freq and time_tolerance variables, which represent the
 * current frequency offset and frequency tolerance. FINEUSEC is 1 us in
 * SHIFT_UPDATE units of the time_phase variable.
 */
#define SHIFT_SCALE 23		/* phase scale (shift) */
#define SHIFT_UPDATE (SHIFT_KG + MAXTC) /* time offset scale (shift) */
#define SHIFT_USEC 16		/* frequency offset scale (shift) */
#define FINEUSEC (1 << SHIFT_SCALE) /* 1 us in phase units */

/*
 * Mode codes (timex.mode) 
 */
#define ADJ_OFFSET	0x0001	/* time offset */
#define ADJ_FREQUENCY	0x0002	/* frequency offset */
#define ADJ_MAXERROR	0x0004	/* maximum time error */
#define ADJ_ESTERROR	0x0008	/* estimated time error */
#define ADJ_STATUS	0x0010	/* clock status */
#define ADJ_TIMECONST	0x0020	/* pll time constant */

/*
 * Clock command/status codes (timex.status)
 */
#define TIME_OK		0	/* clock synchronized */
#define TIME_INS	1	/* insert leap second */
#define TIME_DEL	2	/* delete leap second */
#define TIME_OOP	3	/* leap second in progress */
#define TIME_BAD	4	/* kernel clock not synchronized */
#define TIME_ERR	5	/* external clock not synchronized */
/*
 * NTP user interface - used to read kernel clock values
 * Note: maximum error = NTP synch distance = dispersion + delay / 2;
 * estimated error = NTP dispersion.
 */
struct ntptimeval {
	struct timeval time;	/* current time */
	long maxerror;		/* maximum error (us) */
	long esterror;		/* estimated error (us) */
};

/*
 * NTP daemon interface - used to discipline kernel clock oscillator
 */
struct timex {
	int mode;		/* mode selector */
	long offset;		/* time offset (us) */
	long frequency;		/* frequency offset (scaled ppm) */
	long maxerror;		/* maximum error (us) */
	long esterror;		/* estimated error (us) */
	int status;		/* clock command/status */
	long time_constant;	/* pll time constant */
	long precision;		/* clock precision (us) (read only) */
	long tolerance;		/* clock frequency tolerance (scaled
				 * ppm) (read only) */
	/*
	 * The following read-only structure members are implemented
	 * only if the PPS signal discipline is configured in the
	 * kernel.
	 */
	long ybar;		/* frequency estimate (scaled ppm) */
	long disp;		/* dispersion estimate (scaled ppm) */
	int shift;		/* interval duration (s) (shift) */
	long calcnt;		/* calibration intervals */
	long jitcnt;		/* jitter limit exceeded */
	long discnt;		/* dispersion limit exceeded */

};
