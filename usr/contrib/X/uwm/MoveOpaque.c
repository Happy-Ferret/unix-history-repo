#ifndef lint
static char *rcsid_MoveOpaque_c = "$Header: MoveOpaque.c,v 10.3 86/02/01 16:23:22 tony Rel $";
#endif	lint

/************************************************************************
 *									*
 *			Copyright (c) 1986 by				*
 *		Digital Equipment Corporation, Maynard, MA		*
 *		         All Rights Reserved.				*
 *									*
 *	Permission to use, copy, modify, and distribute this software	*
 *	and its documentation is hereby granted only to licensees of 	*
 *	The Regents of the University of California pursuant to their	*
 *	license agreement for the Berkeley Software Distribution 	*
 *	provided that the following appears on all copies.		*
 *									*
 *            "LICENSED FROM DIGITAL EQUIPMENT CORPORATION		*
 *                      COPYRIGHT (C) 1986				*	
 *                 DIGITAL EQUIPMENT CORPORATION			*
 *                         MAYNARD, MA					*
 *                     ALL RIGHTS RESERVED.				*
 *									*
 *      THE INFORMATION IN THIS SOFTWARE IS SUBJECT TO CHANGE WITHOUT	* 
 *	NOTICE AND SHOULD NOT BE CONSTRUED AS A COMMITMENT BY DIGITAL	*
 *	EQUIPMENT CORPORATION.  DIGITAL MAKES NO REPRESENTATIONS	*
 *	ABOUT SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE. IT IS	*
 *	SUPPLIED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.		*
 *									*	
 * 	IF THE UNIVERSITY OF CALIFORNIA OR ITS LICENSEES MODIFY 	*	
 *	THE SOFTWARE IN A MANNER CREATING DERIVATIVE COPYRIGHT 		*	
 *	RIGHTS APPROPRIATE COPYRIGHT LEGENDS MAY BE PLACED ON THE	*
 *	DERIVATIVE WORK IN ADDITION TO THAT SET FORTH ABOVE."		*	
 *									*
 ************************************************************************/
 

/*
 * MODIFICATION HISTORY
 *
 * 000 -- M. Gancarz, DEC Ultrix Engineering Group
 */

#ifndef lint
static char *sccsid = "@(#)MoveOpaque.c	1.7	1/24/86";
#endif

#include "uwm.h"

Bool MoveOpaque(window, mask, button, x, y)
Window window;				/* Event window. */
int mask;				/* Button/key mask. */
short button;				/* Button event detail. */
int x, y;				/* Event mouse position. */
{
    int prev_x, prev_y;			/* Previous mouse location. */
    int cur_x, cur_y;			/* Current mouse location. */
    int win_x, win_y;			/* Current window location. */
    WindowInfo window_info;		/* Event window information. */
    Window sub_window;			/* Query mouse event sub-window. */
    XButtonEvent button_event;		/* Button event packet. */

    /*
     * Do not try to move the root window.
     */
    if (window == RootWindow)
        return(FALSE);

    /*
     * Change the cursor.
     */
    status = XGrabButton(RootWindow, ArrowCrossCursor, mask, EVENTMASK);
    if (status == FAILURE)
        Error("MoveOpaque -> Unable to grab button and change cursor.");

    /*
     * Gather info on the event window.
     */
    status = XQueryWindow(window, &window_info);
    if (status == FAILURE) return(FALSE);

    /*
     * Initialize movement variables.
     */
    prev_x = cur_x = x;
    prev_y = cur_y = y;
    win_x = window_info.x;
    win_y = window_info.y;

    /*
     * Main loop.
     */
    while (TRUE) {

    	/*
	 * Check to see if we have a change in mouse button status.
	 * This is how we get out of this "while" loop.
	 */
	if (XPending() && GetButton(&button_event)) {

	    /*
	     * If the button event was something other than the
             * release of the original button pressed, then move the
             * window back to where it was originally.
	     */
            if ((button_event.type != ButtonReleased) ||
                ((button_event.detail & ValueMask) != button))
                XMoveWindow(window, window_info.x, window_info.y);

            /*
             * Reset the cursor and return.
             */
            Grab(mask);
            return(TRUE);
	}

        /*
         * Take care of all the little things that have changed; 
         * i.e., move the window, if necessary.
         */
        XUpdateMouse(RootWindow, &cur_x, &cur_y, &sub_window);
        if ((cur_x != prev_x) || (cur_y != prev_y)) {
            win_x += (cur_x - prev_x);
            win_y += (cur_y - prev_y);
            XMoveWindow(window, win_x, win_y);
    	    prev_x = cur_x;
    	    prev_y = cur_y;
    	}
    }
}
