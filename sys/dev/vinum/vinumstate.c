/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
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
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: vinumstate.c,v 2.10 1999/01/17 06:19:23 grog Exp grog $
 */

#define REALLYKERNEL
#include "opt_vinum.h"
#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>

/* Update drive state */
/* Return 1 if the state changes, otherwise 0 */
int 
set_drive_state(int driveno, enum drivestate newstate, enum setstateflags flags)
{
    struct drive *drive = &DRIVE[driveno];
    int oldstate = drive->state;
    int sdno;

    if (drive->state == drive_unallocated)		    /* no drive to do anything with, */
	return 0;

    if (newstate != oldstate) {				    /* don't change it if it's not different */
	if ((newstate == drive_down)			    /* the drive's going down */
	&&(!(flags & setstate_force))
	    && (drive->opencount != 0))			    /* we can't do it */
	    return 0;					    /* don't do it */
	drive->state = newstate;			    /* set the state */
	if (drive->label.name[0] != '\0')		    /* we have a name, */
	    printf("vinum: drive %s is %s\n", drive->label.name, drive_state(drive->state));
	if ((drive->state == drive_up)
	    && (drive->vp == NULL))			    /* should be open, but we're not */
	    init_drive(drive, 1);			    /* which changes the state again */
	if (newstate != oldstate) {			    /* state has changed */
	    for (sdno = 0; sdno < vinum_conf.subdisks_used; sdno++) { /* find this drive's subdisks */
		if (SD[sdno].driveno == driveno)	    /* belongs to this drive */
		    update_sd_state(sdno);		    /* update the state */
	    }
	}
	if ((flags & setstate_configuring) == 0)	    /* configuring? */
	    save_config();				    /* no: save the updated configuration now */
	return 1;
    }
    return 0;
}

/* Try to set the subdisk state.  Return 1 if state changed to
 * what we wanted, -1 if it changed to something else, and 0
 * if no change.
 *
 * This routine is called both from the user (up, down states
 * only) and internally.
 */
int 
set_sd_state(int sdno, enum sdstate newstate, enum setstateflags flags)
{
    struct sd *sd = &SD[sdno];
    struct plex *plex;
    struct volume *vol;
    int oldstate = sd->state;
    int status = 1;					    /* status to return */

    if ((newstate == oldstate)
	|| (sd->state == sd_unallocated))		    /* no subdisk to do anything with, */
	return 0;

    if (sd->driveoffset < 0) {				    /* not allocated space */
	sd->state = sd_down;
	if (newstate != sd_down) {
	    if (sd->plexno >= 0)
		sdstatemap(&PLEX[sd->plexno]);		    /* count up subdisks */
	    return -1;
	}
    } else {						    /*  space allocated */
	switch (newstate) {
	case sd_down:
	    if ((!flags & setstate_force)		    /* but gently */
	    &&(sd->plexno >= 0))			    /* and we're attached to a plex, */
		return 0;				    /* don't do it */
	    break;

	case sd_up:
	    if (DRIVE[sd->driveno].state != drive_up)	    /* can't bring the sd up if the drive isn't, */
		return 0;				    /* not even by force */
	    switch (sd->state) {
	    case sd_crashed:
	    case sd_down:				    /* been down, no data lost */
		if ((sd->plexno >= 0)			    /* we're associated with a plex */
		&&(((PLEX[sd->plexno].state < plex_firstup) /* and it's not up */
		||(PLEX[sd->plexno].subdisks > 1))))	    /* or it's the only one */
		    break;				    /* do it */
		/* XXX Get this right: make sure that other plexes in
		 * the volume cover this address space, otherwise
		 * we make this one sd_up.
		 *
		 * Do we even want this any more?
		 */
		sd->state = sd_reborn;			    /* here it is again */
		printf("vinum: subdisk %s is %s, not %s\n", sd->name, sd_state(sd->state), sd_state(newstate));
		status = -1;
		break;

	    case sd_init:				    /* brand new */
		if (flags & setstate_configuring)	    /* we're doing this while configuring */
		    break;
		/* otherwise it's like being empty */
		/* FALLTHROUGH */

	    case sd_empty:
		if ((sd->plexno >= 0)			    /* we're associated with a plex */
		&&(((PLEX[sd->plexno].state < plex_firstup) /* and it's not up */
		||(PLEX[sd->plexno].subdisks > 1))))	    /* or it's the only one */
		    break;
		/* Otherwise it's just out of date */
		/* FALLTHROUGH */

	    case sd_stale:				    /* out of date info, need reviving */
	    case sd_obsolete:
		/* 1.  If the subdisk is not part of a plex, bring it up, don't revive.

		 * 2.  If the subdisk is part of a one-plex volume or an unattached plex,
		 *     and it's not RAID-5, we *can't revive*.  The subdisk doesn't
		 *     change its state.
		 * 
		 * 3.  If the subdisk is part of a one-plex volume or an unattached plex,
		 *     and it's RAID-5, but more than one subdisk is down, we *still
		 *     can't revive*.  The subdisk doesn't change its state.
		 * 
		 * 4.  If the subdisk is part of a multi-plex volume, we'll change to
		 *     reviving and let the revive routines find out whether it will work
		 *     or not.  If they don't, the revive stops with an error message,
		 *     but the state doesn't change (FWIW).*/
		if (sd->plexno < 0)			    /* no plex associated, */
		    break;				    /* bring it up */
		plex = &PLEX[sd->plexno];
		if (plex->volno >= 0)			    /* have a volume */
		    vol = &VOL[plex->volno];
		else
		    vol = NULL;
		if (((vol == NULL)			    /* no volume */ ||(vol->plexes == 1)) /* or only one plex in volume */
		&&((plex->organization != plex_raid5)	    /* or it's a RAID-5 plex */
		||(plex->sddowncount > 1)))		    /* with more than one subdisk down, */
		    return 0;				    /* can't do it */
		sd->state = sd_reviving;		    /* put in reviving state */
		sd->revived = 0;			    /* nothing done yet */
		status = EAGAIN;			    /* need to repeat */
		break;

		/* XXX This is silly.  We need to be able to
		 * bring the subdisk up when it's finished
		 * initializing, but not from the user.  We
		 * use the same ioctl in each case, but Vinum(8)
		 * doesn't supply the -f flag, so we use that
		 * to decide whether to do it or not */
	    case sd_initializing:
		if (flags & setstate_force)
		    break;				    /* do it if we have to */
		return 0;				    /* no */

	    case sd_reviving:
		if (flags & setstate_force)		    /* insist, */
		    break;
		return EAGAIN;				    /* no, try again */

	    default:					    /* can't do it */
		/* There's no way to bring subdisks up directly from
		 * other states.  First they need to be initialized
		 * or revived */
		return 0;
	    }
	    break;

	default:					    /* other ones, only internal with force */
	    if (flags & setstate_force == 0)		    /* no force?  What's this? */
		return 0;				    /* don't do it */
	}
    }
    if (status == 1) {					    /* we can do it, */
	sd->state = newstate;
	printf("vinum: %s is %s\n", sd->name, sd_state(sd->state));
    } else						    /* we don't get here with status 0 */
	printf("vinum: %s is %s, not %s\n", sd->name, sd_state(sd->state), sd_state(newstate));
    if (sd->plexno >= 0)				    /* we belong to a plex */
	update_plex_state(sd->plexno);			    /* update plex state */
    if ((flags & setstate_configuring) == 0)		    /* save config now */
	save_config();
    return status;
}

/* Set the state of a plex dependent on its subdisks.
 * This time round, we'll let plex state just reflect
 * aggregate subdisk state, so this becomes an order of
 * magnitude less complicated.  In particular, ignore
 * the requested state.
 */
int 
set_plex_state(int plexno, enum plexstate state, enum setstateflags flags)
{
    struct plex *plex;					    /* point to our plex */
    enum plexstate oldstate;
    enum volplexstate vps;				    /* how do we compare with the other plexes? */

    plex = &PLEX[plexno];				    /* point to our plex */
    oldstate = plex->state;

    if ((plex->state == plex_unallocated)		    /* or no plex to do anything with, */
    ||((state == oldstate)				    /* or we're already there */
    &&(state != plex_up)))				    /* and it's not up */
	return 0;

    vps = vpstate(plex);				    /* how do we compare with the other plexes? */

    switch (state) {
	/* We can't bring the plex up, even by force,
	 * unless it's ready.  update_plex_state
	 * checks that */
    case plex_up:					    /* bring the plex up */
	update_plex_state(plex->plexno);		    /* it'll come up if it can */
	break;

    case plex_down:					    /* want to take it down */
	if (((vps == volplex_onlyus)			    /* we're the only one up */
	||(vps == volplex_onlyusup))			    /* we're the only one up */
	&&(!(flags & setstate_force)))			    /* and we don't want to use force */
	    return 0;					    /* can't do it */
	plex->state = state;				    /* do it */
	invalidate_subdisks(plex, sd_down);		    /* and down all up subdisks */
	break;

	/* This is only requested internally.
	 * Trust ourselves */
    case plex_faulty:
	plex->state = state;				    /* do it */
	invalidate_subdisks(plex, sd_crashed);		    /* and crash all up subdisks */
	break;

    case plex_initializing:
	/* XXX consider what safeguards we need here */
	if ((flags & setstate_force) == 0)
	    return 0;
	plex->state = state;				    /* do it */
	break;

	/* What's this? */
    default:
	return 0;
    }
    if (plex->state != oldstate)			    /* we've changed, */
	printf("vinum: %s is %s\n", plex->name, plex_state(plex->state)); /* tell them about it */
    /* Now see what we have left, and whether
     * we're taking the volume down */
    if (plex->volno >= 0)				    /* we have a volume */
	update_volume_state(plex->volno);		    /* update its state */
    if ((flags & setstate_configuring) == 0)		    /* save config now */
	save_config();					    /* yes: save the updated configuration */
    return 1;
}

/* Update the state of a plex dependent on its plexes. */
int 
set_volume_state(int volno, enum volumestate state, enum setstateflags flags)
{
    struct volume *vol = &VOL[volno];			    /* point to our volume */

    if ((vol->state == state)				    /* we're there already */
    ||(vol->state == volume_unallocated))		    /* or no volume to do anything with, */
	return 0;

    if (state == volume_up)				    /* want to come up */
	update_volume_state(volno);
    else if (state == volume_down) {			    /* want to go down */
	if ((vol->opencount == 0)			    /* not open */
	||((flags & setstate_force) != 0)) {		    /* or we're forcing */
	    vol->state = volume_down;
	    printf("vinum: volume %s is %s\n", vol->name, volume_state(vol->state));
	    if ((flags & setstate_configuring) == 0)	    /* save config now */
		save_config();				    /* yes: save the updated configuration */
	    return 1;
	}
    }
    return 0;						    /* no change */
}

/* Set the state of a subdisk based on its environment */
void 
update_sd_state(int sdno)
{
    struct sd *sd;
    struct drive *drive;
    enum sdstate oldstate;

    sd = &SD[sdno];
    oldstate = sd->state;
    drive = &DRIVE[sd->driveno];

    if (drive->state == drive_up) {
	switch (sd->state) {
	case sd_down:
	case sd_crashed:
	    sd->state = sd_reborn;			    /* back up again with no loss */
	    break;

	default:
	    break;
	}
    } else {						    /* down or worse */
	switch (sd->state) {
	case sd_up:
	case sd_reborn:
	case sd_reviving:
	    sd->state = sd_crashed;			    /* lost our drive */
	    break;

	default:
	    break;
	}
    }
    if (sd->state != oldstate)				    /* state has changed, */
	printf("vinum: %s is %s\n", sd->name, sd_state(sd->state)); /* say so */
    if (sd->plexno >= 0)				    /* we're part of a plex, */
	update_plex_state(sd->plexno);			    /* update its state */
}

/* Set the state of a plex based on its environment */
void 
update_plex_state(int plexno)
{
    struct plex *plex;					    /* point to our plex */
    enum plexstate oldstate;
    enum volplexstate vps;				    /* how do we compare with the other plexes? */
    enum sdstates statemap;				    /* get a map of the subdisk states */

    plex = &PLEX[plexno];				    /* point to our plex */
    oldstate = plex->state;

    vps = vpstate(plex);				    /* how do we compare with the other plexes? */
    statemap = sdstatemap(plex);			    /* get a map of the subdisk states */

    if (statemap == sd_upstate)				    /* all subdisks ready for action */
	/* All the subdisks are up.  This also means that
	   * they are consistent, so we can just bring
	   * the plex up */
	plex->state = plex_up;				    /* go for it */
    else if (statemap == sd_emptystate) {		    /* nothing done yet */
	if (((vps & (volplex_otherup | volplex_onlyus)) == 0) /* nothing is up */ &&(plex->state == plex_init) /* we're brand spanking new */
	&&(plex->volno >= 0)				    /* and we have a volume */
	&&(VOL[plex->volno].flags & VF_CONFIG_SETUPSTATE)) { /* and we consider that up */
	    /* Conceptually, an empty plex does not contain valid data,
	     * but normally we'll see this state when we have just
	     * created a plex, and it's either consistent from earlier,
	     * or we don't care about the previous contents (we're going
	     * to create a file system or use it for swap).
	     *
	     * We need to do this in one swell foop: on the next call
	     * we will no longer be just empty.
	     *
	     * This code assumes that all the other plexes are also
	     * capable of coming up (i.e. all the sds are up), but
	     * that's OK: we'll come back to this function for the remaining
	     * plexes in the volume. */
	    struct volume *vol = &VOL[plex->volno];
	    int plexno;

	    for (plexno = 0; plexno < vol->plexes; plexno++)
		PLEX[vol->plex[plexno]].state = plex_up;
	} else if (vps & volplex_otherup == 0) {	    /* no other plexes up */
	    int sdno;

	    plex->state = plex_up;			    /* we can call that up */
	    for (sdno = 0; sdno < plex->subdisks; sdno++) { /* change the subdisks to up state */
		SD[plex->sdnos[sdno]].state = sd_up;
		printf("vinum: %s is up\n", SD[plex->sdnos[sdno]].name); /* tell them about it */
	    }
	} else
	    plex->state = plex_faulty;			    /* no, it's down */
    } else if (statemap & (sd_upstate | sd_rebornstate) == statemap) /* all up or reborn */
	plex->state = plex_flaky;
    else if (statemap & (sd_upstate | sd_rebornstate))	    /* some up or reborn */
	plex->state = plex_corrupt;			    /* corrupt */
    else if (statemap & sd_initstate)			    /* some subdisks initializing */
	plex->state = plex_initializing;
    else						    /* nothing at all up */
	plex->state = plex_faulty;

    if (plex->state != oldstate)			    /* state has changed, */
	printf("vinum: %s is %s\n", plex->name, plex_state(plex->state)); /* tell them about it */
    if (plex->volno >= 0)				    /* we're part of a volume, */
	update_volume_state(plex->volno);		    /* update its state */
}

/* Set volume state based on its components */
void 
update_volume_state(int volno)
{
    struct volume *vol;					    /* our volume */
    int plexno;
    enum volumestate oldstate;

    vol = &VOL[volno];					    /* point to our volume */
    oldstate = vol->state;

    for (plexno = 0; plexno < vol->plexes; plexno++) {
	struct plex *plex = &PLEX[vol->plex[plexno]];	    /* point to the plex */
	if (plex->state >= plex_corrupt) {		    /* something accessible, */
	    vol->state = volume_up;
	    break;
	}
    }
    if (plexno == vol->plexes)				    /* didn't find an up plex */
	vol->state = volume_down;

    if (vol->state != oldstate) {			    /* state changed */
	printf("vinum: %s is %s\n", vol->name, volume_state(vol->state));
	save_config();					    /* save the updated configuration */
    }
}

/* Called from request routines when they find
 * a subdisk which is not kosher.  Decide whether
 * it warrants changing the state.  Return
 * REQUEST_DOWN if we can't use the subdisk,
 * REQUEST_OK if we can. */
/* A prior version of this function checked the plex
 * state as well.  At the moment, consider plex states
 * information for the user only.  We'll ignore them
 * and use the subdisk state only.  The last version of
 * this file with the old logic was 2.7. XXX */
enum requeststatus 
checksdstate(struct sd *sd, struct request *rq, daddr_t diskaddr, daddr_t diskend)
{
    struct plex *plex = &PLEX[sd->plexno];
    int writeop = (rq->bp->b_flags & B_READ) == 0;	    /* note if we're writing */

    switch (sd->state) {
	/* We shouldn't get called if the subdisk is up */
    case sd_up:
	return REQUEST_OK;

    case sd_reviving:
	/* Access to a reviving subdisk depends on the
	 * organization of the plex:

	 * - If it's concatenated, access the subdisk up to its current
	 *   revive point.  If we want to write to the subdisk overlapping the
	 *   current revive block, set the conflict flag in the request, asking
	 *   the caller to put the request on the wait list, which will be
	 *   attended to by revive_block when it's done.
	 * - if it's striped, we can't do it (we could do some hairy
	 *   calculations, but it's unlikely to work).
	 * - if it's RAID-5, we can do it as long as only one
	 *   subdisk is down */
	if (plex->state == plex_striped)		    /* plex is striped, */
	    return REQUEST_DOWN;			    /* can't access it now */
	if (diskaddr > (sd->revived
		+ sd->plexoffset
		+ (sd->revive_blocksize >> DEV_BSHIFT)))    /* we're beyond the end */
	    return REQUEST_DOWN;			    /* don't take the sd down again... */
	else if (diskend > (sd->revived + sd->plexoffset)) { /* we finish beyond the end */
	    if (writeop) {
		rq->flags |= XFR_REVIVECONFLICT;	    /* note a potential conflict */
		rq->sdno = sd->sdno;			    /* and which sd last caused it */
	    } else
		return REQUEST_DOWN;			    /* can't read this yet */
	}
	return REQUEST_OK;

    case sd_reborn:
	if (writeop)
	    return REQUEST_OK;				    /* always write to a reborn disk */
	else						    /* don't allow a read */
	    /* Handle the mapping.  We don't want to reject
	       * a read request to a reborn subdisk if that's
	       * all we have. XXX */
	    return REQUEST_DOWN;

    case sd_down:
	if (writeop)					    /* writing to a consistent down disk */
	    set_sd_state(sd->sdno, sd_obsolete, setstate_force); /* it's not consistent now */
	return REQUEST_DOWN;				    /* and it's down one way or another */

    case sd_crashed:
	if (writeop)					    /* writing to a consistent down disk */
	    set_sd_state(sd->sdno, sd_stale, setstate_force); /* it's not consistent now */
	return REQUEST_DOWN;				    /* and it's down one way or another */

    default:
	return REQUEST_DOWN;
    }
}

/* return a state map for the subdisks of a plex */
enum sdstates 
sdstatemap(struct plex *plex)
{
    int sdno;
    enum sdstates statemap = 0;				    /* note the states we find */

    plex->sddowncount = 0;				    /* no subdisks down yet */
    for (sdno = 0; sdno < plex->subdisks; sdno++) {
	struct sd *sd = &SD[plex->sdnos[sdno]];		    /* point to the subdisk */

	switch (sd->state) {
	case sd_empty:
	    statemap |= sd_emptystate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_init:
	    statemap |= sd_initstate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_down:
	    statemap |= sd_downstate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_crashed:
	    statemap |= sd_crashedstate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_obsolete:
	    statemap |= sd_obsolete;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_stale:
	    statemap |= sd_stalestate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_reborn:
	    statemap |= sd_rebornstate;
	    break;

	case sd_up:
	    statemap |= sd_upstate;
	    break;

	case sd_initializing:
	    statemap |= sd_initstate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_unallocated:
	case sd_uninit:
	case sd_reviving:
	    statemap |= sd_otherstate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	}
    }
    return statemap;
}

/* determine the state of the volume relative to this plex */
enum volplexstate 
vpstate(struct plex *plex)
{
    struct volume *vol;
    enum volplexstate state = volplex_onlyusdown;	    /* state to return */
    int plexno;

    if (plex->volno < 0)				    /* not associated with a volume */
	return volplex_onlyusdown;			    /* assume the worst */

    vol = &VOL[plex->volno];				    /* point to our volume */
    for (plexno = 0; plexno < vol->plexes; plexno++) {
	if (&PLEX[vol->plex[plexno]] == plex) {		    /* us */
#if RAID5
	    if (PLEX[vol->plex[plexno]].state >= plex_degraded)	/* are we up? */
		state |= volplex_onlyus;		    /* yes */
#else
	    if (PLEX[vol->plex[plexno]].state >= plex_flaky) /* are we up? */
		state |= volplex_onlyus;		    /* yes */
#endif
	} else {
#if RAID5
	    if (PLEX[vol->plex[plexno]].state >= plex_degraded)	/* not us */
		state |= volplex_otherup;		    /* and when they were up, they were up */
	    else
		state |= volplex_alldown;		    /* and when they were down, they were down */
#else
	    if (PLEX[vol->plex[plexno]].state >= plex_flaky) /* not us */
		state |= volplex_otherup;		    /* and when they were up, they were up */
	    else
		state |= volplex_alldown;		    /* and when they were down, they were down */
#endif
	}
    }
    return state;					    /* and when they were only halfway up */
}							    /* they were neither up nor down */

/* Check if all bits b are set in a */
int allset(int a, int b);

int 
allset(int a, int b)
{
    return (a & b) == b;
}

/* Invalidate the subdisks belonging to a plex */
void 
invalidate_subdisks(struct plex *plex, enum sdstate state)
{
    int sdno;

    for (sdno = 0; sdno < plex->subdisks; sdno++) {	    /* for each subdisk */
	struct sd *sd = &SD[plex->sdnos[sdno]];

	switch (sd->state) {
	case sd_unallocated:
	case sd_uninit:
	case sd_init:
	case sd_initializing:
	case sd_empty:
	case sd_obsolete:
	case sd_stale:
	case sd_crashed:
	case sd_down:
	    break;

	case sd_reviving:
	case sd_reborn:
	case sd_up:
	    set_sd_state(plex->sdnos[sdno], state, setstate_force);
	}
    }
}

/* Start an object, in other words do what we can to get it up.
 * This is called from vinumioctl (VINUMSTART).
 * Return error indications via ioctl_reply
 */
void 
start_object(struct vinum_ioctl_msg *data)
{
    int status;
    int objindex = data->index;				    /* data gets overwritten */
    struct _ioctl_reply *ioctl_reply = (struct _ioctl_reply *) data; /* format for returning replies */
    enum setstateflags flags;

    if (data->force != 0)				    /* are we going to use force? */
	flags = setstate_force;				    /* yes */
    else
	flags = setstate_none;				    /* no */

    switch (data->type) {
    case drive_object:
	status = set_drive_state(objindex, drive_up, flags);
	if (DRIVE[objindex].state != drive_up)		    /* set status on whether we really did it */
	    ioctl_reply->error = EINVAL;
	else
	    ioctl_reply->error = 0;
	break;

    case sd_object:
	if (SD[objindex].state == sd_reviving) {	    /* reviving, */
	    ioctl_reply->error = revive_block(objindex);    /* revive another block */
	    ioctl_reply->msg[0] = '\0';			    /* no comment */
	    return;
	}
	status = set_sd_state(objindex, sd_up, flags);	    /* set state */
	if (status == EAGAIN) {				    /* first revive, */
	    ioctl_reply->error = revive_block(objindex);    /* revive the first block */
	    ioctl_reply->error = EAGAIN;
	} else {
	    if (SD[objindex].state != sd_up)		    /* set status on whether we really did it */
		ioctl_reply->error = EINVAL;
	    else
		ioctl_reply->error = 0;
	}
	break;

    case plex_object:
	status = set_plex_state(objindex, plex_up, flags);
	if (PLEX[objindex].state != plex_up)		    /* set status on whether we really did it */
	    ioctl_reply->error = EINVAL;
	else
	    ioctl_reply->error = 0;
	break;

    case volume_object:
	status = set_volume_state(objindex, volume_up, flags);
	if (VOL[objindex].state != volume_up)		    /* set status on whether we really did it */
	    ioctl_reply->error = EINVAL;
	else
	    ioctl_reply->error = 0;
	break;

    default:
	ioctl_reply->error = EINVAL;
	strcpy(ioctl_reply->msg, "Invalid object type");
	return;
    }
    /* There's no point in saying anything here:
     * the userland program does it better */
    ioctl_reply->msg[0] = '\0';
}

/* Stop an object, in other words do what we can to get it down
 * This is called from vinumioctl (VINUMSTOP).
 * Return error indications via ioctl_reply.
 */
void 
stop_object(struct vinum_ioctl_msg *data)
{
    int status = 1;
    int objindex = data->index;				    /* save the number from change */
    struct _ioctl_reply *ioctl_reply = (struct _ioctl_reply *) data; /* format for returning replies */

    switch (data->type) {
    case drive_object:
	status = set_drive_state(objindex, drive_down, data->force);
	break;

    case sd_object:
	status = set_sd_state(objindex, sd_down, data->force);
	break;

    case plex_object:
	status = set_plex_state(objindex, plex_down, data->force);
	break;

    case volume_object:
	status = set_volume_state(objindex, volume_down, data->force);
	break;

    default:
	ioctl_reply->error = EINVAL;
	strcpy(ioctl_reply->msg, "Invalid object type");
	return;
    }
    ioctl_reply->msg[0] = '\0';
    if (status == 0)					    /* couldn't do it */
	ioctl_reply->error = EINVAL;
    else
	ioctl_reply->error = 0;
}

/* VINUM_SETSTATE ioctl: set an object state
 * msg is the message passed by the user */
void 
setstate(struct vinum_ioctl_msg *msg)
{
    int sdno;
    struct sd *sd;
    struct plex *plex;
    struct _ioctl_reply *ioctl_reply = (struct _ioctl_reply *) msg; /* format for returning replies */

    switch (msg->state) {
    case object_down:
	stop_object(msg);
	break;

    case object_initializing:
	switch (msg->type) {
	case sd_object:
	    sd = &SD[msg->index];
	    if ((msg->index >= vinum_conf.subdisks_used)
		|| (sd->state == sd_unallocated)) {
		sprintf(ioctl_reply->msg, "Invalid subdisk %d", msg->index);
		ioctl_reply->error = EFAULT;
		return;
	    }
	    set_sd_state(msg->index, sd_initializing, msg->force);
	    if (sd->state != sd_initializing) {
		strcpy(ioctl_reply->msg, "Can't set state");
		ioctl_reply->error = EINVAL;
	    } else
		ioctl_reply->error = 0;
	    break;

	case plex_object:
	    plex = &PLEX[msg->index];
	    if ((msg->index >= vinum_conf.plexes_used)
		|| (plex->state == plex_unallocated)) {
		sprintf(ioctl_reply->msg, "Invalid subdisk %d", msg->index);
		ioctl_reply->error = EFAULT;
		return;
	    }
	    set_plex_state(msg->index, plex_initializing, msg->force);
	    if (plex->state != plex_initializing) {
		strcpy(ioctl_reply->msg, "Can't set state");
		ioctl_reply->error = EINVAL;
	    } else {
		ioctl_reply->error = 0;
		for (sdno = 0; sdno < plex->subdisks; sdno++) {
		    sd = &SD[plex->sdnos[sdno]];
		    set_sd_state(plex->sdnos[sdno], sd_initializing, msg->force);
		    if (sd->state != sd_initializing) {
			strcpy(ioctl_reply->msg, "Can't set state");
			ioctl_reply->error = EINVAL;
			break;
		    }
		}
	    }
	    break;

	default:
	    strcpy(ioctl_reply->msg, "Invalid object");
	    ioctl_reply->error = EINVAL;
	}
	break;

    case object_up:
	start_object(msg);
    }
}
