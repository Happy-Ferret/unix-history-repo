/* Procedure integration for GNU CC.
   Copyright (C) 1988 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@mcc.com)

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */


#include <stdio.h>

#include "config.h"
#include "rtl.h"
#include "tree.h"
#include "flags.h"
#include "insn-flags.h"
#include "expr.h"

#include "obstack.h"
#define	obstack_chunk_alloc	xmalloc
#define	obstack_chunk_free	free
extern int xmalloc ();
extern void free ();

extern struct obstack permanent_obstack, maybepermanent_obstack;
extern struct obstack *rtl_obstack, *saveable_obstack, *current_obstack;

extern rtx stack_slot_list;

#define MIN(x,y) ((x < y) ? x : y)

extern tree pushdecl ();
extern tree poplevel ();

/* Default max number of insns a function can have and still be inline.
   This is overridden on RISC machines.  */
#ifndef INTEGRATE_THRESHOLD
#define INTEGRATE_THRESHOLD(DECL) \
  (8 * (8 + list_length (DECL_ARGUMENTS (DECL)) + 16*TREE_INLINE (DECL)))
#endif

/* This is the target of the inline function being expanded,
   or NULL if there is none.  */
static rtx inline_target;

/* We must take special care not to disrupt life too severely
   when performing procedure integration.  One thing that that
   involves is not creating illegitimate address which reload
   cannot fix.  Since we don't know what the frame pointer is
   not capable of (in a machine independent way), we create
   a pseudo-frame pointer which will have to do for now.  */
static rtx before_inline_fp_rtx, inline_fp_rtx;

/* Convert old frame-pointer offsets to new.  Parameters which only
   produce values (no addresses, and are never assigned), map directly
   to the pseudo-reg of the incoming value.  Parameters that are
   assigned to but do not have their address taken are given a fresh
   pseudo-register.  Parameters that have their address take are
   given a fresh stack-slot.  */
static rtx *parm_map;

/* ?? Should this be done here??  It is not right now.
   Keep track of whether a given pseudo-register is the sum
   of the frame pointer and a const_int (or zero).  */
static char *fp_addr_p;

/* For the local variables of the procdure being integrated that live
   on the frame, FRAME_POINTER_DELTA says how much to change their
   offsets by, so that they now live in the correct place on the
   frame of the function being compiled.  */
static int fp_delta;

/* When an insn is being copied by copy_rtx_and_substitute,
   this is nonzero if we have copied an ASM_OPERANDS.
   In that case, it is the original input-operand vector.
   Likewise in copy_for_inline.  */
static rtvec orig_asm_operands_vector;

/* When an insn is being copied by copy_rtx_and_substitute,
   this is nonzero if we have copied an ASM_OPERANDS.
   In that case, it is the copied input-operand vector.
   Likewise in copy_for_inline.  */
static rtvec copy_asm_operands_vector;

/* Likewise, this is the copied constraints vector.  */
static rtvec copy_asm_constraints_vector;

/* In save_for_inline, nonzero if past the parm-initialization insns.  */
static int in_nonparm_insns;

/* Return a copy of an rtx (as needed), substituting pseudo-register,
   labels, and frame-pointer offsets as necessary.  */
static rtx copy_rtx_and_substitute ();
/* Variant, used for memory addresses that are not memory_address_p.  */
static rtx copy_address ();

/* Return the rtx corresponding to a given index in the stack arguments.  */
static rtx access_parm_map ();

static void copy_parm_decls ();
static void copy_decl_tree ();

static rtx try_fold_cc0 ();

/* We do some simple constant folding optimization.  This optimization
   really exists primarily to save time inlining a function.  It
   also helps users who ask for inline functions without -O.  */
static rtx fold_out_const_cc0 ();

/* Zero if the current function (whose FUNCTION_DECL is FNDECL)
   is safe and reasonable to integrate into other functions.
   Nonzero means value is a warning message with a single %s
   for the function's name.  */

char *
function_cannot_inline_p (fndecl)
     register tree fndecl;
{
  register rtx insn;
  tree last = tree_last (TYPE_ARG_TYPES (TREE_TYPE (fndecl)));
  int max_insns = INTEGRATE_THRESHOLD (fndecl);
  register int ninsns = 0;
  register tree parms;

  /* No inlines with varargs.  `grokdeclarator' gives a warning
     message about that if `inline' is specified.  This code
     it put in to catch the volunteers.  */
  if (last && TREE_VALUE (last) != void_type_node)
    return "varargs function cannot be inline";

  /* If its not even close, don't even look.  */
  if (get_max_uid () > 4 * max_insns)
    return "function too large to be inline";

  /* Don't inline functions with large stack usage,
     since they can make other recursive functions burn up stack.  */
  if (!TREE_INLINE (fndecl) && get_frame_size () > 100)
    return "function stack frame for inlining";

  /* We can't inline functions that return structures
     the old-fashioned PCC way, copying into a static block.  */
#ifdef PCC_STATIC_STRUCT_RETURN
  if (flag_pcc_struct_return
      && (TYPE_MODE (TREE_TYPE (TREE_TYPE (fndecl))) == BLKmode
	  || RETURN_IN_MEMORY (TREE_TYPE (TREE_TYPE (fndecl)))))
    return "inline functions not supported for this return value type";
#endif

  /* Don't inline functions which have BLKmode arguments.
     Don't inline functions that take the address of
       a parameter and do not specify a function prototype.  */
  for (parms = DECL_ARGUMENTS (fndecl); parms; parms = TREE_CHAIN (parms))
    {
      if (TYPE_MODE (TREE_TYPE (parms)) == BLKmode)
	{
#if 0
	  return "function with large aggregate parameter cannot be inline";
#else
	  TREE_ADDRESSABLE (parms) = 1;
#endif
	}
      if (last == NULL_TREE && TREE_ADDRESSABLE (parms))
	return "no prototype, and parameter address used; cannot be inline";
#if 0
      /* If an aggregate is thought of as "in memory"
	 then its components are referred to by narrower memory refs.
	 If the actual parameter is a reg, these refs can't be translated,
	 esp. since copy_rtx_and_substitute doesn't know whether it is
	 reading or writing.  */
      if ((TREE_CODE (TREE_TYPE (parms)) == RECORD_TYPE
	   || TREE_CODE (TREE_TYPE (parms)) == UNION_TYPE)
	  && GET_CODE (DECL_RTL (parms)) == MEM)
	return "address of an aggregate parameter is used; cannot be inline";
#endif
    }

  if (get_max_uid () > max_insns)
    {
      for (ninsns = 0, insn = get_first_nonparm_insn (); insn && ninsns < max_insns;
	   insn = NEXT_INSN (insn))
	{
	  if (GET_CODE (insn) == INSN
	      || GET_CODE (insn) == JUMP_INSN
	      || GET_CODE (insn) == CALL_INSN)
	    ninsns++;
	}

      if (ninsns >= max_insns)
	return "function too large to be inline";
    }

  return 0;
}

/* Variables used within save_for_inline.  */

/* Mapping from old pesudo-register to new pseudo-registers.
   The first element of this map is reg_map[FIRST_PSEUDO_REGISTER].
   It is allocated in `save_for_inline' and `expand_inline_function',
   and deallocated on exit from each of those routines.  */
static rtx *reg_map;

/* Mapping from old code-labels to new code-labels.
   The first element of this map is label_map[min_labelno].
   It is allocated in `save_for_inline' and `expand_inline_function',
   and deallocated on exit from each of those routines.  */
static rtx *label_map;

/* Mapping from old insn uid's to copied insns.
   It is allocated in `save_for_inline' and `expand_inline_function',
   and deallocated on exit from each of those routines.  */
static rtx *insn_map;

/* Map pseudo reg number into the PARM_DECL for the parm living in the reg.
   Zero for a reg that isn't a parm's home.
   Only reg numbers less than max_parm_reg are mapped here.  */
static tree *parmdecl_map;

/* Map pseudo reg number to equivalent constant.  We cannot in general
   substitute constants into parameter pseudo registers, since a
   machine descriptions (the Sparc md, maybe others) won't always handle
   the resulting insns.  So if an incoming parameter has a constant
   equivalent, we record it here, and if the resulting insn is
   recognizable, we go with it.  */
static rtx *const_equiv_map;

/* Nonzero if we should try using a constant equivalent.
   Set to zero if constant equivalent resulted in insn which could
   not be recognized.  */
static int try_use_const;

/* Use "iteration numbering" to speedily pull constant values
   from registers when testing conditionals.  */
static unsigned int *const_age_map, const_age;

/* Cleared before attempting to inline any functions.
   Set when const equiv is used.  Used to test whether insn
   is safe for md or not.  */
static int used_const_equiv;

/* Keep track of first pseudo-register beyond those that are parms.  */
static int max_parm_reg;

/* Offset from arg ptr to the first parm of this inline function.  */
static int first_parm_offset;

/* On machines that perform a function return with a single
   instruction, such as the VAX, these return insns must be
   mapped into branch statements.  */
extern rtx return_label;

/* Save any constant pool constants in an insn.  */
static void save_constants ();

/* Note when parameter registers are the destination of a SET.  */
static void note_modified_parmregs ();

/* Copy an rtx for save_for_inline.  */
static rtx copy_for_inline ();

/* Make the insns and PARM_DECLs of the current function permanent
   and record other information in DECL_SAVED_INSNS to allow inlining
   of this function in subsequent calls.  */

void
save_for_inline (fndecl)
     tree fndecl;
{
  extern rtx *regno_reg_rtx;	/* in emit-rtl.c.  */
  extern current_function_args_size;

  rtx first_insn, last_insn, insn;
  rtx head, copy;
  tree parms;
  int max_labelno, min_labelno, i, len;
  int max_reg;
  int max_uid;
  rtx first_nonparm_insn;

  /* Make and emit a return-label if we have not already done so.  */

  if (return_label == 0)
    {
      return_label = gen_label_rtx ();
      emit_label (return_label);
    }

  /* Get some bounds on the labels and registers used.  */

  max_labelno = max_label_num ();
  min_labelno = get_first_label_num ();
  max_parm_reg = max_parm_reg_num ();
  max_reg = max_reg_num ();

  /* Set up PARMDECL_MAP which maps pseudo-reg number to its PARM_DECL.

     Set TREE_VOLATILE to 0 if the parm is in a register, otherwise 1.
     Later we set TREE_READONLY to 0 if the parm is modified inside the fn.  */

  parmdecl_map = (tree *) alloca (max_parm_reg * sizeof (tree));
  bzero (parmdecl_map, max_parm_reg * sizeof (tree));

  for (parms = DECL_ARGUMENTS (fndecl); parms; parms = TREE_CHAIN (parms))
    {
      rtx p = DECL_RTL (parms);

      if (GET_CODE (p) == REG)
	{
	  parmdecl_map[REGNO (p)] = parms;
	  TREE_VOLATILE (parms) = 0;
	}
      else
	TREE_VOLATILE (parms) = 1;
      TREE_READONLY (parms) = 1;
    }

  /* Replace any constant pool references with the actual constant.  We will
     put the constant back in the copy made below.  */
  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
    if (GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN
	|| GET_CODE (insn) == CALL_INSN)
      save_constants (PATTERN (insn));

  /* The list of DECL_SAVED_INSNS, starts off with a header which
     contains the following information:

     the first insn of the function (not including the insns that copy
     parameters into registers).
     the first parameter insn of the function,
     the first label used by that function,
     the last label used by that function,
     the highest register number used for parameters,
     the total number of registers used,
     the stack slot list,
     @@ not yet: and some flags that are used to restore compiler globals.  */

  head = gen_inline_header_rtx (NULL, NULL, min_labelno, max_labelno,
				max_parm_reg, max_reg,
				current_function_args_size, stack_slot_list);
  max_uid = INSN_UID (head);

  /* We have now allocated all that needs to be allocated permanently
     on the rtx obstack.  Set our high-water mark, so that we
     can free the rest of this when the time comes.  */

  preserve_data ();

  /* Copy the chain insns of this function.
     Install the copied chain as the insns of this function,
     for continued compilation;
     the original chain is recorded as the DECL_SAVED_INSNS
     for inlining future calls.  */

  /* If there are insns that copy parms from the stack into pseudo registers,
     those insns are not copied.  `expand_inline_function' must
     emit the correct code to handle such things.  */

  insn = get_insns ();
  if (GET_CODE (insn) != NOTE)
    abort ();
  first_insn = rtx_alloc (NOTE);
  NOTE_SOURCE_FILE (first_insn) = NOTE_SOURCE_FILE (insn);
  NOTE_LINE_NUMBER (first_insn) = NOTE_LINE_NUMBER (insn);
  INSN_UID (first_insn) = INSN_UID (insn);
  PREV_INSN (first_insn) = NULL;
  NEXT_INSN (first_insn) = NULL;
  last_insn = first_insn;

  /* Each pseudo-reg in the old insn chain must have a unique rtx in the copy.
     Make these new rtx's now, and install them in regno_reg_rtx, so they
     will be the official pseudo-reg rtx's for the rest of compilation.  */

  reg_map = (rtx *) alloca ((max_reg + 1) * sizeof (rtx));

  len = sizeof (struct rtx_def) + (GET_RTX_LENGTH (REG) - 1) * sizeof (rtunion);
  for (i = max_reg - 1; i >= FIRST_PSEUDO_REGISTER; i--)
    reg_map[i] = (rtx)obstack_copy (&maybepermanent_obstack, regno_reg_rtx[i], len);
  bcopy (reg_map + FIRST_PSEUDO_REGISTER,
	 regno_reg_rtx + FIRST_PSEUDO_REGISTER,
	 (max_reg - FIRST_PSEUDO_REGISTER) * sizeof (rtx));

  /* Likewise each label rtx must have a unique rtx as its copy.  */

  label_map = (rtx *)alloca ((max_labelno - min_labelno) * sizeof (rtx));
  label_map -= min_labelno;

  for (i = min_labelno; i < max_labelno; i++)
    label_map[i] = gen_label_rtx ();

  /* Record the mapping of old insns to copied insns.  */

  insn_map = (rtx *) alloca (max_uid * sizeof (rtx));
  bzero (insn_map, max_uid * sizeof (rtx));

  in_nonparm_insns = 0;
  first_nonparm_insn = get_first_nonparm_insn ();

  /* Now copy the chain of insns.  */

  for (insn = NEXT_INSN (insn); insn; insn = NEXT_INSN (insn))
    {
      orig_asm_operands_vector = 0;
      copy_asm_operands_vector = 0;

      if (insn == first_nonparm_insn)
	in_nonparm_insns = 1;

      switch (GET_CODE (insn))
	{
	case NOTE:
	  /* No need to keep these.  */
	  if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_DELETED)
	    continue;

	  copy = rtx_alloc (NOTE);
	  NOTE_SOURCE_FILE (copy) = NOTE_SOURCE_FILE (insn);
	  NOTE_LINE_NUMBER (copy) = NOTE_LINE_NUMBER (insn);
	  break;

	case INSN:
	case CALL_INSN:
	case JUMP_INSN:
	  copy = rtx_alloc (GET_CODE (insn));
	  PATTERN (copy) = copy_for_inline (PATTERN (insn));
	  INSN_CODE (copy) = -1;
	  LOG_LINKS (copy) = NULL;
	  REG_NOTES (copy) = copy_for_inline (REG_NOTES (insn));
	  RTX_INTEGRATED_P (copy) = RTX_INTEGRATED_P (insn);
	  break;

	case CODE_LABEL:
	  copy = label_map[CODE_LABEL_NUMBER (insn)];
	  break;

	case BARRIER:
	  copy = rtx_alloc (BARRIER);
	  break;

	default:
	  abort ();
	}
      INSN_UID (copy) = INSN_UID (insn);
      insn_map[INSN_UID (insn)] = copy;
      NEXT_INSN (last_insn) = copy;
      PREV_INSN (copy) = last_insn;
      last_insn = copy;
    }

  NEXT_INSN (last_insn) = NULL;

  NEXT_INSN (head) = get_first_nonparm_insn ();
  FIRST_PARM_INSN (head) = get_insns ();
  DECL_SAVED_INSNS (fndecl) = head;
  DECL_FRAME_SIZE (fndecl) = get_frame_size ();
  TREE_INLINE (fndecl) = 1;

  parmdecl_map = 0;
  label_map = 0;
  reg_map = 0;
  return_label = 0;

  set_new_first_and_last_insn (first_insn, last_insn);

  /* The following code does not need preprocessing in the assembler.  */

  app_disable ();

  output_constant_pool (XSTR (XEXP (DECL_RTL (fndecl), 0), 0), fndecl);
}

/* Make the insns and PARM_DECLs of the current function permanent
   and record other information in DECL_SAVED_INSNS to allow inlining
   of this function in subsequent calls.

   Other version.  */

void
save_for_outline (fndecl)
     tree fndecl;
{
  extern rtx *regno_reg_rtx;	/* in emit-rtl.c.  */
  extern current_function_args_size;

  rtx first_insn, last_insn, insn;
  rtx head, copy;
  tree parms;
  int max_labelno, min_labelno, i, len;
  int max_reg;
  int max_uid;
  rtx first_nonparm_insn;

  /* Make and emit a return-label if we have not already done so.  */

  if (return_label == 0)
    {
      return_label = gen_label_rtx ();
      emit_label (return_label);
    }

  /* Get some bounds on the labels and registers used.  */

  max_labelno = max_label_num ();
  min_labelno = get_first_label_num ();
  max_parm_reg = max_parm_reg_num ();
  max_reg = max_reg_num ();

  /* Set up PARMDECL_MAP which maps pseudo-reg number to its PARM_DECL.

     Set TREE_VOLATILE to 0 if the parm is in a register, otherwise 1.
     Later we set TREE_READONLY to 0 if the parm is modified inside the fn.  */

  parmdecl_map = (tree *) alloca (max_parm_reg * sizeof (tree));
  bzero (parmdecl_map, max_parm_reg * sizeof (tree));

  for (parms = DECL_ARGUMENTS (fndecl); parms; parms = TREE_CHAIN (parms))
    {
      rtx p = DECL_RTL (parms);

      if (GET_CODE (p) == REG)
	{
	  parmdecl_map[REGNO (p)] = parms;
	  TREE_VOLATILE (parms) = 0;
	}
      else
	TREE_VOLATILE (parms) = 1;
      TREE_READONLY (parms) = 1;
    }

  /* The list of DECL_SAVED_INSNS, starts off with a header which
     contains the following information:

     the first insn of the function (not including the insns that copy
     parameters into registers).
     the first parameter insn of the function,
     the first label used by that function,
     the last label used by that function,
     the highest register number used for parameters,
     the total number of registers used,
     the stack slot list,
     @@ not yet: and some flags that are used to restore compiler globals.  */

  head = gen_inline_header_rtx (NULL, NULL, min_labelno, max_labelno,
				max_parm_reg, max_reg,
				current_function_args_size, stack_slot_list);
  /* We have now allocated all that needs to be allocated permanently
     on the rtx obstack.  Set our high-water mark, so that we
     can free the rest of this when the time comes.  */

  preserve_data ();

  /* Copy the chain insns of this function.
     Install the copied chain as the insns of this function,
     for continued compilation;
     the original chain is recorded as the DECL_SAVED_INSNS
     for inlining future calls.  */

  /* If there are insns that copy parms from the stack into pseudo registers,
     those insns are not copied.  `expand_inline_function' must
     emit the correct code to handle such things.  */

  insn = get_insns ();
  if (GET_CODE (insn) != NOTE)
    abort ();
  first_insn = rtx_alloc (NOTE);
  NOTE_SOURCE_FILE (first_insn) = NOTE_SOURCE_FILE (insn);
  NOTE_LINE_NUMBER (first_insn) = NOTE_LINE_NUMBER (insn);
  INSN_UID (first_insn) = INSN_UID (insn);
  PREV_INSN (first_insn) = NULL;
  NEXT_INSN (first_insn) = NULL;
  last_insn = first_insn;

  in_nonparm_insns = 0;
  first_nonparm_insn = get_first_nonparm_insn ();

  /* Now copy the chain of insns.  */

  for (insn = NEXT_INSN (insn); insn; insn = NEXT_INSN (insn))
    {
      orig_asm_operands_vector = 0;
      copy_asm_operands_vector = 0;

      if (insn == first_nonparm_insn)
	in_nonparm_insns = 1;

      switch (GET_CODE (insn))
	{
	case INSN:
	case CALL_INSN:
	case JUMP_INSN:
	  note_modified_parmregs (PATTERN (insn));
	  break;

	case NOTE:
	case CODE_LABEL:
	case BARRIER:
	  break;

	default:
	  abort ();
	}
      last_insn = insn;
    }

  NEXT_INSN (head) = get_first_nonparm_insn ();
  FIRST_PARM_INSN (head) = get_insns ();
  DECL_SAVED_INSNS (fndecl) = head;
  DECL_FRAME_SIZE (fndecl) = get_frame_size ();
  TREE_INLINE (fndecl) = 1;

  /* Have to output these, since other functions may refer to them.  */

  /* The following code does not need preprocessing in the assembler.  */

  app_disable ();

  output_constant_pool (XSTR (XEXP (DECL_RTL (fndecl), 0), 0), fndecl);
}

/* References to the constant pool are replaced by the actual constant
    encapsulated with a CONST giving the mode and with RTX_INTEGRATED_P set.

   *** Note that the above only works if the address was not manipulated.
       If the address was not valid and had to be loaded into a register,
       we lose track of the fact that it was in the constant pool, which will
       result in either an abort or generating a reference to an undefined
       label in the assembler code.  No current machine will run into this, but
       this should probably be fixed someday.  */

static void
save_constants (x)
     rtx x;
{
  int i, j;
  char *fmt = GET_RTX_FORMAT (GET_CODE (x));

  for (i = 0; i < GET_RTX_LENGTH (GET_CODE (x)); i++)
    {
      switch (*fmt++)
	{
	case 'E':
	  for (j = 0; j < XVECLEN (x, i); j++)
	    if (GET_CODE (XVECEXP (x, i, j)) == MEM
		&& GET_CODE (XEXP (XVECEXP (x, i, j), 0)) == SYMBOL_REF
		&& CONSTANT_POOL_ADDRESS_P (XEXP (XVECEXP (x, i, j), 0)))
	      {
		XVECEXP (x, i, j) =
		  gen_rtx (CONST, get_pool_mode (XEXP (XVECEXP (x, i, j), 0)),
			   get_pool_constant (XEXP (XVECEXP (x, i, j), 0)));
		RTX_INTEGRATED_P (XVECEXP (x, i, j)) = 1;
	      }
	    else
	      save_constants (XVECEXP (x, i, j));
	  break;

	case 'e':
	    if (GET_CODE (XEXP (x, i)) == MEM
		&& GET_CODE (XEXP (XEXP (x, i), 0)) == SYMBOL_REF
		&& CONSTANT_POOL_ADDRESS_P (XEXP (XEXP (x, i), 0)))
	      {
		XEXP (x, i) = gen_rtx (CONST,
				       get_pool_mode (XEXP (XEXP (x, i), 0)),
				       get_pool_constant (XEXP (XEXP (x, i), 0)));
		RTX_INTEGRATED_P (XEXP (x, i)) = 1;
	      }
	  else
	    save_constants (XEXP (x, i));
	  break;
	}
    }
}

/* Note (recursively) whether a parameter is modified or not.  */

static void
note_modified_parmregs (orig)
     rtx orig;
{
  register rtx x = orig;
  register int i, len;
  register enum rtx_code code;
  register char *format_ptr;

 again:

  if (x == 0)
    return;

  code = GET_CODE (x);

  /* These types may be freely shared.  */

  if (code == SET && in_nonparm_insns)
    {
      rtx dest = SET_DEST (x);

      if (GET_CODE (dest) == REG
	  && REGNO (dest) < max_parm_reg
	  && REGNO (dest) >= FIRST_PSEUDO_REGISTER
	  && parmdecl_map[REGNO (dest)] != 0)
	TREE_READONLY (parmdecl_map[REGNO (dest)]) = 0;
      return;
    }

  /* Now scan the subexpressions recursively.  */

  format_ptr = GET_RTX_FORMAT (code);
  len = GET_RTX_LENGTH (code);

  if (len > 2)
    for (i = len-1; i > 1; i--)
      if (format_ptr[i] == 'e')
	note_modified_parmregs (XEXP (x, 2));
      else if (format_ptr[i] == 'E')
	goto hard;

  if (len > 1)
    if (format_ptr[1] == 'e')
      note_modified_parmregs (XEXP (x, 1));
    else if (format_ptr[i=1] == 'E')
      goto hard;

  if (len > 0)
    if (format_ptr[0] == 'e')
      {
	x = XEXP (x, 0);
	goto again;
      }
    else if (format_ptr[i=0] == 'E')
      goto hard;

  return;

  i = len-1;

 hard:

  while (i >= 0)
    {
      switch (format_ptr[i])
	{
	case 'e':
	  if (i == 0)
	    {
	      x = XEXP (x, 0);
	      goto again;
	    }
	  note_modified_parmregs (XEXP (x, i));
	  break;

	case 'E':
	  if (XVEC (x, i) != NULL && XVECLEN (x, i) != 0)
	    {
	      register int j;

	      for (j = 0; j < XVECLEN (x, i); j++)
		note_modified_parmregs (XVECEXP (x, i, j));
	    }
	  break;
	}
      i -= 1;
    }
}

/* Copy the rtx ORIG recursively, replacing pseudo-regs and labels
   according to `reg_map' and `label_map'.

   If we find a saved constant pool entry, replace it with the constant.
   Since the pool wasn't touched, this should simply restore the old
   address.

   All other kinds of rtx are copied except those that can never be
   changed during compilation.  */

static rtx
copy_for_inline (orig)
     rtx orig;
{
  register rtx x = orig;
  register int i;
  register enum rtx_code code;
  register char *format_ptr;

  if (x == 0)
    return x;

  code = GET_CODE (x);

  /* These types may be freely shared.  */

  switch (code)
    {
    case QUEUED:
    case CONST_INT:
    case CONST_DOUBLE:
    case SYMBOL_REF:
    case PC:
    case CC0:
      return x;

    case CONST:
      /* Get constant pool entry for constant in the pool.  */
      if (RTX_INTEGRATED_P (x))
	return force_const_mem (GET_MODE (x), XEXP (x, 0));
      break;

    case ASM_OPERANDS:
      /* If a single asm insn contains multiple output operands
	 then it contains multiple ASM_OPERANDS rtx's that share operand 3.
	 We must make sure that the copied insn continues to share it.  */
      if (orig_asm_operands_vector == XVEC (orig, 3))
	{
	  x = rtx_alloc (ASM_OPERANDS);
	  XSTR (x, 0) = XSTR (orig, 0);
	  XSTR (x, 1) = XSTR (orig, 1);
	  XINT (x, 2) = XINT (orig, 2);
	  XVEC (x, 3) = copy_asm_operands_vector;
	  XVEC (x, 4) = copy_asm_constraints_vector;
	  XSTR (x, 5) = XSTR (orig, 5);
	  XINT (x, 6) = XINT (orig, 6);
	  return x;
	}
      break;

    case MEM:
      /* A MEM is allowed to be shared if its address is constant
	 or is a constant plus one of the special registers.  */
      if (CONSTANT_ADDRESS_P (XEXP (x, 0)))
	return x;
#if 0 /* This is turned off because it is possible for
	 unshare_all_rtl to copy the address, into memory that won't be saved.
	 Although the MEM can safely be shared, and won't be copied there,
	 the address itself cannot be shared, and may need to be copied.  */
      if (GET_CODE (XEXP (x, 0)) == PLUS
	  && GET_CODE (XEXP (XEXP (x, 0), 0)) == REG
	  && (REGNO (XEXP (XEXP (x, 0), 0)) == FRAME_POINTER_REGNUM
	      || REGNO (XEXP (XEXP (x, 0), 0)) == ARG_POINTER_REGNUM)
	  && CONSTANT_ADDRESS_P (XEXP (XEXP (x, 0), 1)))
#if 0
	/* This statement was accidentally deleted in the remote past.
	   Reinsert it for 1.37.  Don't take the risk now.  */
	return x;
#endif
	if (GET_CODE (XEXP (x, 0)) == REG
	    && (REGNO (XEXP (x, 0)) == FRAME_POINTER_REGNUM
		|| REGNO (XEXP (x, 0)) == ARG_POINTER_REGNUM)
	    && CONSTANT_ADDRESS_P (XEXP (x, 1)))
	return x;
#endif /* 0 */
      break;

    case LABEL_REF:
      {
	/* Must point to the new insn.  */
	return gen_rtx (LABEL_REF, GET_MODE (orig),
			label_map[CODE_LABEL_NUMBER (XEXP (orig, 0))]);
      }

    case REG:
      if (REGNO (x) >= FIRST_PSEUDO_REGISTER)
	return reg_map [REGNO (x)];
      else
	return x;

      /* If a parm that gets modified lives in a pseudo-reg,
	 set its TREE_VOLATILE to prevent certain optimizations.  */
    case SET:
      if (in_nonparm_insns)
	{
	  rtx dest = SET_DEST (x);

	  if (GET_CODE (dest) == REG
	      && REGNO (dest) < max_parm_reg
	      && REGNO (dest) >= FIRST_PSEUDO_REGISTER
	      && parmdecl_map[REGNO (dest)] != 0)
	    TREE_READONLY (parmdecl_map[REGNO (dest)]) = 0;
	}
      /* The insn to load an arg pseudo from a stack slot
	 does not count as modifying it.  */
      break;

      /* Arrange that CONST_INTs always appear as the second operand
	 if they appear, and that `frame_pointer_rtx' or `arg_pointer_rtx'
	 always appear as the first.  */
    case PLUS:
      if (GET_CODE (XEXP (x, 0)) == CONST_INT
	  || (XEXP (x, 1) == frame_pointer_rtx
	      || (ARG_POINTER_REGNUM != FRAME_POINTER_REGNUM
		  && XEXP (x, 1) == arg_pointer_rtx)))
	{
	  rtx t = XEXP (x, 0);
	  XEXP (x, 0) = XEXP (x, 1);
	  XEXP (x, 1) = t;
	}
      break;
    }

  /* Replace this rtx with a copy of itself.  */

  x = rtx_alloc (code);
  bcopy (orig, x, (sizeof (*x) - sizeof (x->fld)
		   + sizeof (x->fld[0]) * GET_RTX_LENGTH (code)));

  /* Now scan the subexpressions recursively.
     We can store any replaced subexpressions directly into X
     since we know X is not shared!  Any vectors in X
     must be copied if X was copied.  */

  format_ptr = GET_RTX_FORMAT (code);

  for (i = 0; i < GET_RTX_LENGTH (code); i++)
    {
      switch (*format_ptr++)
	{
	case 'e':
	  XEXP (x, i) = copy_for_inline (XEXP (x, i));
	  break;

	case 'u':
	  /* Change any references to old-insns to point to the
	     corresponding copied insns.  */
	  return insn_map[INSN_UID (XEXP (x, i))];

	case 'E':
	  if (XVEC (x, i) != NULL && XVECLEN (x, i) != 0)
	    {
	      register int j;

	      XVEC (x, i) = gen_rtvec_v (XVECLEN (x, i), &XVECEXP (x, i, 0));
	      for (j = 0; j < XVECLEN (x, i); j++)
		XVECEXP (x, i, j)
		  = copy_for_inline (XVECEXP (x, i, j));
	    }
	  break;
	}
    }

  if (code == ASM_OPERANDS && orig_asm_operands_vector == 0)
    {
      orig_asm_operands_vector = XVEC (orig, 3);
      copy_asm_operands_vector = XVEC (x, 3);
      copy_asm_constraints_vector = XVEC (x, 4);
    }

  return x;
}

/* Helper function to deal with using constants for kinds of INSNs.
   Return zero if trouble arose by using constants.
   Return one if not.  Caller must know what to do in either case.  */
static int
note_integrated (copy)
     rtx copy;
{
  if (used_const_equiv)
    {
      used_const_equiv = 0;
      if (recog (PATTERN (copy), copy) < 0)
	{
	  int old_optimize = optimize;
	  optimize = 1;
	  delete_insn (copy);
	  optimize = old_optimize;
	  try_use_const = 0;
	  return 0;
	}
    }
  try_use_const = 1;
  RTX_INTEGRATED_P (copy) = 1;
  return 1;
}


/* Non-zero if we are trying to reduce the amount of debug information output */
extern int flag_inline_debug;

/* Integrate the procedure defined by FNDECL.  Note that this function
   may wind up calling itself.  Since the static variables are not
   reentrant, we do not assign them until after the possibility
   or recursion is eliminated.

   If IGNORE is nonzero, do not produce a value.
   Otherwise store the value in TARGET if it is nonzero and that is convenient.

   Value is:
   (rtx)-1 if we could not substitute the function
   0 if we substituted it and it does not produce a value
   else an rtx for where the value is stored.  */

rtx
expand_inline_function (fndecl, parms, target, ignore, type, structure_value_addr)
     tree fndecl, parms;
     rtx target;
     int ignore;
     tree type;
     rtx structure_value_addr;
{
  extern int lineno;
  tree formal, actual;
  rtx header = DECL_SAVED_INSNS (fndecl);
  rtx insns = FIRST_FUNCTION_INSN (header);
  rtx insn;
  int max_regno = MAX_REGNUM (header) + 1;
  register int i;
  int min_labelno = FIRST_LABELNO (header);
  int max_labelno = LAST_LABELNO (header);
  int nargs;
  rtx *arg_vec;
  rtx local_return_label = 0;
  rtx follows_call = 0;
  rtx this_struct_value_rtx = 0;

  /* Hack around non-reentrancy of static variables.  */
  rtx *old_const_equiv_map = const_equiv_map;
  unsigned *old_const_age_map = const_age_map;
  unsigned old_const_age = const_age;

  /* If we need INLINE_FP_RTX, set it up immediately
     following this insn.  */

  if (max_regno < FIRST_PSEUDO_REGISTER)
    abort ();

  nargs = list_length (DECL_ARGUMENTS (fndecl));

  /* We expect PARMS to have the right length; don't crash if not.  */
  if (list_length (parms) != nargs)
    return (rtx)-1;

  /* Also check that the parms type match.  Since the appropriate
     conversions or default promotions have already been applied,
     the machine modes should match exactly.  */
  for (formal = DECL_ARGUMENTS (fndecl),
       actual = parms;
       formal;
       formal = TREE_CHAIN (formal),
       actual = TREE_CHAIN (actual))
    {
      tree arg = TREE_VALUE (actual);
      enum machine_mode mode = TYPE_MODE (DECL_ARG_TYPE (formal));
      if (mode != TYPE_MODE (TREE_TYPE (arg)))
	return (rtx)-1;
      /* If they are block mode, the types should match exactly.
         They don't match exactly if TREE_TYPE (FORMAL) == ERROR_MARK_NODE,
	 which could happen if the parameter has incomplete type.  */
      if (mode == BLKmode && TREE_TYPE (arg) != TREE_TYPE (formal))
	return (rtx)-1;
    }

  const_equiv_map = (rtx *)alloca (max_regno * sizeof (rtx));
  bzero (const_equiv_map, max_regno * sizeof (rtx));
  const_age_map = (unsigned *)alloca (max_regno * sizeof (unsigned));
  bzero (const_age_map, max_regno * sizeof (unsigned));
  try_use_const = 1;
  /* Trick: set to large number so that variables set in first
     basic block keep their values.  After first label is seen,
     we wrap.  */
  const_age = (unsigned)-1;

  /* Make a binding contour to keep inline cleanups called at
     outer function-scope level from looking like they are shadowing
     parameter declarations.  */
  pushlevel (0);

  /* Make a fresh binding contour that we can easily remove.  */
  pushlevel (0);
  expand_start_bindings (0);

  /* Get all the actual args as RTL, and store them in ARG_VEC.  */

  arg_vec = (rtx *)alloca (nargs * sizeof (rtx));

  if (flag_inline_debug)
    /* Say where this function starts.  */
    emit_note (DECL_SOURCE_FILE (fndecl), DECL_SOURCE_LINE (fndecl));

  for (formal = DECL_ARGUMENTS (fndecl),
       actual = parms,
       i = 0;
       formal;
       formal = TREE_CHAIN (formal),
       actual = TREE_CHAIN (actual),
       i++)
    {
      /* Actual parameter, already converted to DECL_ARG_TYPE (formal).  */
      tree arg = TREE_VALUE (actual);
      /* Mode of the value supplied.  */
      enum machine_mode tmode = TYPE_MODE (DECL_ARG_TYPE (formal));
      /* Mode of the variable used within the function.  */
      enum machine_mode imode = TYPE_MODE (TREE_TYPE (formal));
      rtx copy;

#if 0
      /* PARM_DECL nodes no longer have this.  */
      emit_note (DECL_SOURCE_FILE (formal), DECL_SOURCE_LINE (formal));
#endif

      /* Make a place to hold the argument value, still in mode TMODE,
	 and put it in COPY.  */
      if (TREE_ADDRESSABLE (formal))
	{
	  int size = int_size_in_bytes (DECL_ARG_TYPE (formal));
	  copy = assign_stack_local (tmode, size);
	  if (!memory_address_p (DECL_MODE (formal), XEXP (copy, 0)))
	    copy = change_address (copy, VOIDmode, copy_rtx (XEXP (copy, 0)));
	  store_expr (arg, copy, 0);
	}
      else if (! TREE_READONLY (formal)
	       || TREE_VOLATILE (formal))
	{
	  /* If parm is modified or if it hasn't a pseudo reg,
	     we may not simply substitute the actual value;
	     copy it through a register.  */
	  copy = gen_reg_rtx (tmode);
	  store_expr (arg, copy, 0);
	}
      else
	{
	  copy = expand_expr (arg, 0, tmode, 0);

	  /* We do not use CONSTANT_ADDRESS_P here because
	     the set of cases where that might make a difference
	     are a subset of the cases that arise even when
	     it is a CONSTANT_ADDRESS_P (i.e., fp_delta
	     gets into the act.  */
	  if (GET_CODE (copy) != REG)
	    {
#if 0
	      if (! CONSTANT_P (copy))
		copy = copy_to_reg (copy);
	      else if (! optimize)
		copy = copy_to_mode_reg (imode, copy);
#else
	      /*   Sigh.  */
	      if (! CONSTANT_P (copy))
		copy = copy_to_reg (copy);
	      else
		{
		  if (GET_CODE (DECL_RTL (formal)) == REG)
		    {
		      int regno = REGNO (DECL_RTL (formal));
		      const_equiv_map[regno] = copy;
		      const_age_map[regno] = (unsigned)-2;
		    }
		  copy = copy_to_mode_reg (imode, copy);
		}
#endif
	    }
	}
      /* If passed mode != nominal mode, COPY is now the passed mode.
	 Convert it to the nominal mode (i.e. truncate it).  */
      if (tmode != imode)
	copy = convert_to_mode (imode, copy, 0);
      arg_vec[i] = copy;
    }

  copy_parm_decls (DECL_ARGUMENTS (fndecl), arg_vec);

  /* Perform postincrements before actually calling the function.  */
  emit_queue ();

  /* clean up stack so that variables might have smaller offsets.  */
  do_pending_stack_adjust ();

  /* Pass the function the address in which to return a structure value.
     Note that a constructor can cause someone to call us with
     STRUCTURE_VALUE_ADDR, but the initialization takes place
     via the first parameter, rather than the struct return address.  */
  if (structure_value_addr && aggregate_value_p (DECL_RESULT (fndecl)))
    {
      if (GET_CODE (structure_value_addr) == REG
	  && (struct_value_rtx == 0 || GET_CODE (struct_value_rtx) == MEM))
	this_struct_value_rtx = structure_value_addr;
      else
 	this_struct_value_rtx = copy_to_mode_reg (Pmode, structure_value_addr);
    }

  /* Now prepare for copying the insns.
     Set up reg_map, parm_map and label_map saying how to translate
     the pseudo-registers, stack-parm references and labels when copying.  */

  reg_map = (rtx *) alloca (max_regno * sizeof (rtx));
  bzero (reg_map, max_regno * sizeof (rtx));

  parm_map = (rtx *)alloca ((FUNCTION_ARGS_SIZE (header) + UNITS_PER_WORD - 1)
			    / UNITS_PER_WORD * sizeof (rtx));
  bzero (parm_map, ((FUNCTION_ARGS_SIZE (header) + UNITS_PER_WORD - 1)
		    / UNITS_PER_WORD * sizeof (rtx)));

  /* Note that expand_expr (called above) can clobber first_parm_offset.  */
  first_parm_offset = FIRST_PARM_OFFSET (fndecl);
  parm_map -= first_parm_offset / UNITS_PER_WORD;

  if (DECL_ARGUMENTS (fndecl))
    {
      tree decl = DECL_ARGUMENTS (fndecl);

      for (formal = decl, i = 0; formal; formal = TREE_CHAIN (formal), i++)
	{
	  /* Create an entry in PARM_MAP that says what pseudo register
	     is associated with an address we might compute.  */
	  if (DECL_OFFSET (formal) >= 0)
	    {
	      /* This parameter has a home in the stack.  */
	      parm_map[DECL_OFFSET (formal) / BITS_PER_WORD] = arg_vec[i];
	    }
	  else
	    {
	      /* Parameter that was passed in a register;
		 does it have a home on the stack (as a local)?  */
	      rtx frtx = DECL_RTL (formal);
	      rtx offset = 0;
	      if (GET_CODE (frtx) == MEM)
		{
		  frtx = XEXP (frtx, 0);
		  if (GET_CODE (frtx) == PLUS)
		    {
		      if (XEXP (frtx, 0) == frame_pointer_rtx
			  && GET_CODE (XEXP (frtx, 1)) == CONST_INT)
			offset = XEXP (frtx, 1);
		      else if (XEXP (frtx, 1) == frame_pointer_rtx
			       && GET_CODE (XEXP (frtx, 0)) == CONST_INT)
			offset = XEXP (frtx, 0);
#if FRAME_POINTER_REGNUM != ARG_POINTER_REGNUM
		      /* If there is a separate arg pointer
			 and REG_PARM_STACK_SPACE is defined,
			 parms passed in regs can be copied
			 to slots reached via the arg pointer.  */
		      if (XEXP (frtx, 0) == arg_pointer_rtx
			  && GET_CODE (XEXP (frtx, 1)) == CONST_INT)
			offset = XEXP (frtx, 1);
		      else if (XEXP (frtx, 1) == arg_pointer_rtx
			       && GET_CODE (XEXP (frtx, 0)) == CONST_INT)
			offset = XEXP (frtx, 0);
#endif
		    }
		  if (offset)
		    parm_map[INTVAL (offset) / UNITS_PER_WORD] = arg_vec[i];
		  else if (TREE_TYPE (formal) != error_mark_node)
		    abort ();
		}
	      else if (GET_CODE (frtx) != REG)
		abort ();
	    }
	  /* Create an entry in REG_MAP that says what rtx is associated
	     with a pseudo register from the function being inlined.  */
	  if (GET_CODE (DECL_RTL (formal)) == REG)
	    reg_map[REGNO (DECL_RTL (formal))] = arg_vec[i];
	}
    }

  /* Make certain that we can accept struct_value_{incoming_rtx,rtx},
     and map it.  */
  if (this_struct_value_rtx == 0)
    ;
  else if (GET_CODE (struct_value_incoming_rtx) == REG)
    reg_map[REGNO (XEXP (DECL_RTL (DECL_RESULT (fndecl)), 0))]
      = this_struct_value_rtx;
  else if (GET_CODE (struct_value_incoming_rtx) == MEM
	   && XEXP (XEXP (struct_value_incoming_rtx, 0), 0) == frame_pointer_rtx
	   && GET_CODE (XEXP (XEXP (struct_value_incoming_rtx, 0), 1)) == CONST_INT)
#if 1
    reg_map[REGNO (XEXP (DECL_RTL (DECL_RESULT (fndecl)), 0))]
      = this_struct_value_rtx;
#else
    parm_map[INTVAL (XEXP (XEXP (struct_value_incoming_rtx, 0), 1)) / UNITS_PER_WORD]
      = this_struct_value_rtx;
#endif
  else
    abort ();

  label_map = (rtx *)alloca ((max_labelno - min_labelno) * sizeof (rtx));
  label_map -= min_labelno;

  for (i = min_labelno; i < max_labelno; i++)
    label_map[i] = gen_label_rtx ();

  /* As we copy insns, record the correspondence, so that inter-insn
     references can be copied into isomorphic structure.  */

  insn_map = (rtx *) alloca (INSN_UID (header) * sizeof (rtx));
  bzero (insn_map, INSN_UID (header) * sizeof (rtx));

  /* Set up a target to translate the inline function's value-register.  */

  if (structure_value_addr != 0 || TYPE_MODE (type) == VOIDmode)
    inline_target = 0;
  else
    {
      /* Machine mode function was declared to return.   */
      enum machine_mode departing_mode = TYPE_MODE (type);
      /* (Possibly wider) machine mode it actually computes
	 (for the sake of callers that fail to declare it right).  */
      enum machine_mode arriving_mode
	= TYPE_MODE (DECL_RESULT_TYPE (fndecl));

      /* Don't use MEMs as direct targets because on some machines
	 substituting a MEM for a REG makes invalid insns.
	 Let the combiner substitute the MEM if that is valid.  */
      if (target && GET_CODE (target) == REG
	  && GET_MODE (target) == departing_mode)
	inline_target = target;
      else
	{
	  inline_target = gen_reg_rtx (departing_mode);
	  if (target == 0)
	    target = inline_target;
	}

      /* If function's value was promoted before return,
	 avoid machine mode mismatch when we substitute INLINE_TARGET.
	 But TARGET is what we will return to the caller.  */
      if (arriving_mode != departing_mode)
	inline_target = gen_rtx (SUBREG, arriving_mode, inline_target, 0);
    }

  /* Make space in current function's stack frame
     for the stack frame of the inline function.
     Adjust all frame-pointer references by the difference
     between the offset to this space
     and the offset to the equivalent space in the inline
     function's frame.
     This difference equals the size of preexisting locals.  */

  fp_delta = get_frame_size ();
#ifdef FRAME_GROWS_DOWNWARD
  fp_delta = - fp_delta;
#endif

  before_inline_fp_rtx = get_last_insn ();
  inline_fp_rtx = 0;

  /* Now allocate the space for that to point at.  */

  assign_stack_local (VOIDmode, DECL_FRAME_SIZE (fndecl));

  /* Now copy the insns one by one.  */

  for (insn = insns; insn; insn = NEXT_INSN (insn))
    {
      rtx copy, pattern, next = 0;

    retry:
      orig_asm_operands_vector = 0;
      copy_asm_operands_vector = 0;

      switch (GET_CODE (insn))
	{
	case INSN:
	  pattern = PATTERN (insn);

	  /* Special handling for the insn immediately after a CALL_INSN
	     that returned a value:
	     If it does copy the value, we must avoid the usual translation
	     of the return-register into INLINE_TARGET.
	     If it just USEs the value, the inline function expects it to
	     stay in the return-register and be returned,
	     so copy it into INLINE_TARGET.  */

	  if (follows_call
	      /* Allow a stack-adjust, handled normally, to come in between
		 the call and the value-copying insn.  */
	      && ! (GET_CODE (pattern) == SET
		    && SET_DEST (pattern) == stack_pointer_rtx))
	    {
	      if (GET_CODE (pattern) == SET
		  && rtx_equal_p (SET_SRC (pattern), follows_call))
		/* This insn copies the value: take special care to copy
		   that value to this insn's destination.  */
		{
		  copy = emit_insn (gen_rtx (SET, VOIDmode,
					     copy_rtx_and_substitute (SET_DEST (pattern)),
					     follows_call));
		  if (! note_integrated (copy))
		    {
		      next = 0;
		      goto retry;
		    }
		  follows_call = 0;
		  break;
		}
	      else if (GET_CODE (pattern) == USE
		       && rtx_equal_p (XEXP (pattern, 0), follows_call))
		/* This insn does nothing but says the value is expected
		   to flow through to the inline function's return-value.
		   Make that happen, then ignore this insn.  */
		{
		  copy = emit_insn (gen_rtx (SET, VOIDmode, inline_target,
					     follows_call));
		  if (! note_integrated (copy))
		    {
		      next = 0;
		      goto retry;
		    }
		  follows_call = 0;
		  break;
		}
	      /* If it does neither, this value must be ignored.  */
	      follows_call = 0;
	    }

	  copy = 0;
	  if (GET_CODE (pattern) == USE
	      && GET_CODE (XEXP (pattern, 0)) == REG)
	    {
	      /* The (USE (REG n)) at return from the function should
		 be ignored since we are changing (REG n) into
		 inline_target.  */
	      if (! ignore && REG_FUNCTION_VALUE_P (XEXP (pattern, 0)))
		break;
	      /* Don't emit a (USE (REG n)) of something which
		 is now constant.  */
	      if (REGNO (XEXP (pattern, 0)) >= FIRST_PSEUDO_REGISTER
		  && (const_age == (unsigned)-1
		      || const_age_map[REGNO (XEXP (pattern, 0))] >= const_age))
		break;
	    }

	  /* Ignore setting a function value that we don't want to use.  */
	  if (inline_target == 0
	      && GET_CODE (pattern) == SET
	      && GET_CODE (SET_DEST (pattern)) == REG
	      && REG_FUNCTION_VALUE_P (SET_DEST (pattern)))
	    break;

	  /* Try to do some quick constant folding here.
	     This will save save execution time of the compiler,
	     as well time and space of the program if done here.  */
	  if (GET_CODE (pattern) == SET
	      && SET_DEST (pattern) == cc0_rtx)
	    next = try_fold_cc0 (insn);

	  if (next != 0)
	    {
	      used_const_equiv = 0;
	      insn = next;
	    }
	  else
	    {
	      rtx note = find_reg_note (insn, REG_EQUIV, 0);
	      copy = emit_insn (copy_rtx_and_substitute (pattern));
	      if (! note_integrated (copy))
		{
		  next = 0;
		  goto retry;
		}

	      /* If we are copying an insn that loads a constant,
		 record the constantness.  */
	      if (note)
		REG_NOTES (copy)
		  = gen_rtx (EXPR_LIST, REG_EQUIV, XEXP (note, 0),
			     REG_NOTES (copy));

	      if (GET_CODE (pattern) == SET)
		{
		  rtx dest = SET_DEST (pattern);
		  if (GET_CODE (dest) == REG)
		    {
		      int regno = REGNO (dest);

		      if (regno >= FIRST_PSEUDO_REGISTER
			  && CONSTANT_P (SET_SRC (pattern))
			  && (const_equiv_map[regno] == 0
			      /* Following clause is a hack to make
				 case work where GNU C++ reassigns
				 a variable to make cse work right.  */
			      || ! rtx_equal_p (const_equiv_map[regno],
						SET_SRC (pattern))))
			{
			  const_equiv_map[regno] = SET_SRC (pattern);
			  const_age_map[regno] = const_age;
			}
		    }
		  else
		    {
		      while (GET_CODE (dest) == SUBREG
			     || GET_CODE (dest) == STRICT_LOW_PART
			     || GET_CODE (dest) == SIGN_EXTRACT
			     || GET_CODE (dest) == ZERO_EXTRACT)
			dest = SUBREG_REG (dest);

		      /* Forget everything we thought we knew.  */
		      if (GET_CODE (dest) == REG
			  && REGNO (dest) >= FIRST_PSEUDO_REGISTER)
			const_equiv_map[REGNO (dest)] = 0;
		    }
		}
	    }
	  break;

	case JUMP_INSN:
	  follows_call = 0;
	  if (GET_CODE (PATTERN (insn)) == RETURN)
	    {
	      if (local_return_label == 0)
		local_return_label = gen_label_rtx ();
	      emit_jump (local_return_label);
	      break;
	    }
	  copy = emit_jump_insn (copy_rtx_and_substitute (PATTERN (insn)));
	  if (! note_integrated (copy))
	    {
	      next = 0;
	      goto retry;
	    }
	  break;

	case CALL_INSN:
	  copy = emit_call_insn (copy_rtx_and_substitute (PATTERN (insn)));
	  if (! note_integrated (copy))
	    {
	      next = 0;
	      goto retry;
	    }
	  /* Special handling needed for the following INSN depending on
	     whether it copies the value from the fcn return reg.  */
	  if (GET_CODE (PATTERN (insn)) == SET)
	    follows_call = SET_DEST (PATTERN (insn));
	  else if (GET_CODE (PATTERN (insn)) == PARALLEL
		   && GET_CODE (XVECEXP (PATTERN (insn), 0, 0)) == SET)
	    follows_call = SET_DEST (XVECEXP (PATTERN (insn), 0, 0));
	  break;

	case CODE_LABEL:
	  const_age += 2;
	  copy = emit_label (label_map[CODE_LABEL_NUMBER (insn)]);
	  follows_call = 0;
	  if (const_age & 1)
	    {
	      int i;

	      const_age += 1;
	      for (i = max_regno; i >= 0; i--)
		if (const_age_map[i] == (unsigned)-1)
		  const_age_map[i] = 0;
	    }
	  break;

	case BARRIER:
	  const_age += 2;
	  copy = emit_barrier ();
	  break;

	case NOTE:
	  /* It is important to discard function-end and function-beg notes,
	     so we have only one of each in the current function.  */
	  if (NOTE_LINE_NUMBER (insn) != NOTE_INSN_FUNCTION_END
	      && NOTE_LINE_NUMBER (insn) != NOTE_INSN_FUNCTION_BEG
	      && (flag_inline_debug || NOTE_LINE_NUMBER (insn) < 0))
	    copy = emit_note (NOTE_SOURCE_FILE (insn), NOTE_LINE_NUMBER (insn));
	  else
	    copy = 0;
	  break;

	default:
	  abort ();
	  break;
	}
      if (! (used_const_equiv == 0))
	abort ();
      insn_map[INSN_UID (insn)] = copy;
    }

  if (local_return_label)
    emit_label (local_return_label);

  /* Don't try substituting constants when making up a DECLs rtl.
     That would probably only confuse the debugger, but I don't
     know for sure.  */
  try_use_const = 0;
  /* Make copies of the decls of the symbols in the inline function, so that
     the copies of the variables get declared in the current function.  */
  copy_decl_tree (DECL_INITIAL (fndecl), 0);
  /* For safety.  */
  if (try_use_const)
    used_const_equiv = 0;

  /* End the scope containing the copied formal parameter variables.  */

  expand_end_bindings (getdecls (), 1, 1);
  poplevel (1, 1, 0);
  poplevel (0, 0, 0);
  emit_line_note (input_filename, lineno);

  reg_map = NULL;
  label_map = NULL;

  const_equiv_map = old_const_equiv_map;
  const_age_map = old_const_age_map;
  const_age = old_const_age;

  if (ignore || TYPE_MODE (type) == VOIDmode)
    return 0;

  if (structure_value_addr)
    {
      if (target)
	return target;
      return gen_rtx (MEM, TYPE_MODE (type),
		      memory_address (BLKmode, structure_value_addr));
    }
  else if (target && target != inline_target
	   && (GET_CODE (inline_target) != SUBREG
	       || SUBREG_REG (inline_target) != target))
    {
      /* Copy result back to TARGET if TARGET is not INLINE_TARGET.
         In general, these should always wind up being the same mode,
	 after SUBREGs, if any, are stripped.  */
      convert_move (target, inline_target, 0);
    }

  return target;
}

/* Given a chain of PARM_DECLs, ARGS, and a vector of RTL homes VEC,
   copy each decl into a VAR_DECL, push all of those decls
   and give each one the corresponding home.  */

static void
copy_parm_decls (args, vec)
     tree args;
     rtx *vec;
{
  register tree tail;
  register int i;

  for (tail = args, i = 0; tail; tail = TREE_CHAIN (tail), i++)
    {
      register tree decl = build_decl (VAR_DECL, DECL_NAME (tail),
				       TREE_TYPE (tail));
      /* These args would always appear unused, if not for this.  */
      TREE_USED (decl) = 1;
      /* Prevent warning for shadowing with these.  */
      TREE_INLINE (decl) = 1;
      pushdecl (decl);
      DECL_RTL (decl) = vec[i];
    }
}

/* Given a LET_STMT node, push decls and levels
   so as to construct in the current function a tree of contexts
   isomorphic to the one that is given.  */

static void
copy_decl_tree (let, level)
     tree let;
     int level;
{
  tree t, node;

  pushlevel (0);
  
  for (t = STMT_VARS (let); t; t = TREE_CHAIN (t))
    {
      tree d = build_decl (TREE_CODE (t), DECL_NAME (t), TREE_TYPE (t));
      DECL_SOURCE_LINE (d) = DECL_SOURCE_LINE (t);
      DECL_SOURCE_FILE (d) = DECL_SOURCE_FILE (t);
      if (DECL_RTL (t) != 0)
	{
	  if (GET_CODE (DECL_RTL (t)) == MEM
	      && CONSTANT_ADDRESS_P (XEXP (DECL_RTL (t), 0)))
	    /* copy_rtx_and_substitute would call memory_address
	       which would copy the address into a register.
	       Then debugging-output wouldn't know how to handle it.  */
	    DECL_RTL (d) = DECL_RTL (t);
	  else
	    DECL_RTL (d) = copy_rtx_and_substitute (DECL_RTL (t));
	}
      TREE_EXTERNAL (d) = TREE_EXTERNAL (t);
      TREE_STATIC (d) = TREE_STATIC (t);
      TREE_PUBLIC (d) = TREE_PUBLIC (t);
      TREE_LITERAL (d) = TREE_LITERAL (t);
      TREE_ADDRESSABLE (d) = TREE_ADDRESSABLE (t);
      TREE_READONLY (d) = TREE_READONLY (t);
      TREE_VOLATILE (d) = TREE_VOLATILE (t);
      /* These args would always appear unused, if not for this.  */
      TREE_USED (d) = 1;
      /* Prevent warning for shadowing with these.  */
      TREE_INLINE (d) = 1;
      pushdecl (d);
    }

  for (t = STMT_SUBBLOCKS (let); t; t = TREE_CHAIN (t))
    copy_decl_tree (t, level + 1);

  node = poplevel (level > 0, 0, 0);
  if (node)
    TREE_USED (node) = TREE_USED (let);
}

/* Create a new copy of an rtx.
   Recursively copies the operands of the rtx,
   except for those few rtx codes that are sharable.

   Handle constants that need to be placed in the constant pool by
   calling `force_const_mem'.  */

static rtx
copy_rtx_and_substitute (orig)
     register rtx orig;
{
  register rtx copy, temp;
  register int i, j;
  register RTX_CODE code;
  register enum machine_mode mode;
  register char *format_ptr;
  int regno;

  if (orig == 0)
    return 0;

  code = GET_CODE (orig);
  mode = GET_MODE (orig);

  switch (code)
    {
    case REG:
      /* If a frame-pointer register shows up, then we
	 must `fix' the reference.  If the stack pointer
	 register shows up, it must be part of stack-adjustments
	 (*not* because we eliminated the frame pointer!).
	 Small hard registers are returned as-is.  Pseudo-registers
	 go through their `reg_map'.  */
      regno = REGNO (orig);
      if (regno < FIRST_PSEUDO_REGISTER)
	{
	  /* Some hard registers are also mapped,
	     but others are not translated.  */
	  if (reg_map[regno] != 0)
	    return reg_map[regno];
	  if (REG_FUNCTION_VALUE_P (orig))
	    {
	      /* This is a reference to the function return value.  If
		 the function doesn't have a return value, error.
		 If it does, it may not be the same mode as `inline_target'
		 because SUBREG is not required for hard regs.
		 If not, adjust mode of inline_target to fit the context.  */
	      if (inline_target == 0)
		{
		  /* If there isn't an inline target, so be it.
		     Just fake up a reg--it won't get used
		     for anything important anyway.  */
		  inline_target = gen_reg_rtx (mode);
		  return inline_target;
		}
	      if (mode == GET_MODE (inline_target))
		return inline_target;
	      return gen_rtx (SUBREG, mode, inline_target, 0);
	    }
	  if (regno == FRAME_POINTER_REGNUM)
	    return plus_constant (orig, fp_delta);
	  return orig;
	}
      if (try_use_const
	  && const_equiv_map[regno] != 0
	  && const_age_map[regno] == (unsigned)-2)
	{
	  used_const_equiv = 1;
	  return const_equiv_map[regno];
	}
      if (reg_map[regno] == NULL)
	reg_map[regno] = gen_reg_rtx (mode);
      return reg_map[regno];

    case SUBREG:
      copy = copy_rtx_and_substitute (SUBREG_REG (orig));
      /* SUBREG is ordinary, but don't make nested SUBREGs.  */
      if (GET_CODE (copy) == SUBREG)
	return gen_rtx (SUBREG, GET_MODE (orig), SUBREG_REG (copy),
			SUBREG_WORD (orig) + SUBREG_WORD (copy));
      /* Don't build a SUBREG of a CONST_INT.  */
      if (GET_CODE (copy) == CONST_INT)
	return copy;
      return gen_rtx (SUBREG, GET_MODE (orig), copy,
		      SUBREG_WORD (orig));

    case CODE_LABEL:
      return label_map[CODE_LABEL_NUMBER (orig)];

    case LABEL_REF:
      copy = rtx_alloc (LABEL_REF);
      PUT_MODE (copy, mode);
      XEXP (copy, 0) = label_map[CODE_LABEL_NUMBER (XEXP (orig, 0))];
      return copy;

    case PC:
    case CC0:
    case CONST_INT:
    case CONST_DOUBLE:
    case SYMBOL_REF:
      return orig;

    case CONST:
      /* Make new constant pool entry for a constant
	 that was in the pool of the inline function.  */
      if (RTX_INTEGRATED_P (orig))
	return force_const_mem (GET_MODE (orig), XEXP (orig, 0));
      break;

    case ASM_OPERANDS:
      /* If a single asm insn contains multiple output operands
	 then it contains multiple ASM_OPERANDS rtx's that share operand 3.
	 We must make sure that the copied insn continues to share it.  */
      if (orig_asm_operands_vector == XVEC (orig, 3))
	{
	  copy = rtx_alloc (ASM_OPERANDS);
	  XSTR (copy, 0) = XSTR (orig, 0);
	  XSTR (copy, 1) = XSTR (orig, 1);
	  XINT (copy, 2) = XINT (orig, 2);
	  XVEC (copy, 3) = copy_asm_operands_vector;
	  XVEC (copy, 4) = copy_asm_constraints_vector;
	  XSTR (copy, 5) = XSTR (orig, 5);
	  XINT (copy, 6) = XINT (orig, 6);
	  return copy;
	}
      break;

    case CALL:
      /* This is given special treatment because the first
	 operand of a CALL is a (MEM ...) which may get
	 forced into a register for cse.  This is undesirable
	 if function-address cse isn't wanted or if we won't do cse.  */
#ifndef NO_FUNCTION_CSE
      if (! (optimize && ! flag_no_function_cse))
#endif
	return gen_rtx (CALL, GET_MODE (orig),
			gen_rtx (MEM, GET_MODE (XEXP (orig, 0)),
				 copy_rtx_and_substitute (XEXP (XEXP (orig, 0), 0))),
			copy_rtx_and_substitute (XEXP (orig, 1)));
      break;

    case PLUS:
      /* Note: treat the PLUS case as though it might be needed
         to be part of an address.  If it turns out that the machine's
	 PLUS insns can handle something more exciting than a ``load
	 effective address'', the optimizer will discover this fact.  */
      /* Take care of the easy case quickly.  */
      if (XEXP (orig, 0) == frame_pointer_rtx
	  || (ARG_POINTER_REGNUM != FRAME_POINTER_REGNUM
	      && XEXP (orig, 0) == arg_pointer_rtx))
	{
	  rtx reg = XEXP (orig, 0), copy = XEXP (orig, 1);

	  if (GET_CODE (copy) == CONST_INT)
	    {
	      int c = INTVAL (copy);

	      if (reg == arg_pointer_rtx && c >= first_parm_offset)
		{
		  copy = access_parm_map (c, VOIDmode);
		  if (GET_CODE (copy) != MEM)
		    /* Should not happen, because a parm we need to address
		       should not be living in a register.
		       (expand_inline_function copied it to a stack slot.)  */
		    abort ();
		  return XEXP (copy, 0);
		}
	      return gen_rtx (PLUS, mode,
			      frame_pointer_rtx,
			      gen_rtx (CONST_INT, SImode,
				       c + fp_delta));
	    }
	  copy = copy_rtx_and_substitute (copy);
	  temp = force_reg (mode, gen_rtx (PLUS, mode, frame_pointer_rtx, copy));
	  return plus_constant (temp, fp_delta);
	}
      else if (reg_mentioned_p (frame_pointer_rtx, orig)
	       || (ARG_POINTER_REGNUM != FRAME_POINTER_REGNUM
		   && reg_mentioned_p (arg_pointer_rtx, orig)))
	{
	  if (GET_CODE (XEXP (orig, 1)) == CONST_INT)
	    {
	      copy = copy_rtx_and_substitute (XEXP (orig, 0));
	      temp = plus_constant (copy, INTVAL (XEXP (orig, 1)));
	    }
	  else
	    {
	      temp = gen_rtx (PLUS, GET_MODE (orig),
			      copy_rtx_and_substitute (XEXP (orig, 0)),
			      copy_rtx_and_substitute (XEXP (orig, 1)));
	    }
	  if (memory_address_p (mode, orig))
	    temp = memory_address (mode, temp);
	}
      else
	{
	  int old_used_const_equiv = used_const_equiv;

	  used_const_equiv = 0;
	  temp = gen_rtx (PLUS, GET_MODE (orig),
			  copy_rtx_and_substitute (XEXP (orig, 0)),
			  copy_rtx_and_substitute (XEXP (orig, 1)));
	  if (used_const_equiv)
	    {
	      if (GET_CODE (XEXP (temp, 0)) == CONST_INT)
		temp = plus_constant (XEXP (temp, 1), INTVAL (XEXP (temp, 0)));
	      else if (GET_CODE (XEXP (temp, 1)) == CONST_INT)
		temp = plus_constant (XEXP (temp, 0), INTVAL (XEXP (temp, 1)));
	      else if (memory_address_p (mode, orig))
		{
		  try_use_const = 0;
		  used_const_equiv = 0;
		  temp = gen_rtx (PLUS, GET_MODE (orig),
				  copy_rtx_and_substitute (XEXP (orig, 0)),
				  copy_rtx_and_substitute (XEXP (orig, 1)));
		}
	    }
	  else if (memory_address_p (mode, orig))
	    temp = memory_address (mode, temp);

	  used_const_equiv |= old_used_const_equiv;
	}
      return temp;

    case MULT:
      {
	int old_used_const_equiv = used_const_equiv;

	used_const_equiv = 0;

	temp = gen_rtx (MULT, GET_MODE (orig),
			copy_rtx_and_substitute (XEXP (orig, 0)),
			copy_rtx_and_substitute (XEXP (orig, 1)));

	if (used_const_equiv)
	  {
	    if (GET_CODE (XEXP (temp, 0)) == CONST_INT
		&& GET_CODE (XEXP (temp, 1)) == CONST_INT)
	      temp = gen_rtx (CONST_INT, VOIDmode,
			      INTVAL (XEXP (temp, 0)) * INTVAL (XEXP (temp, 1)));
	    else
	      {
		try_use_const = 0;
		used_const_equiv = 0;
		temp = gen_rtx (MULT, GET_MODE (orig),
				copy_rtx_and_substitute (XEXP (orig, 0)),
				copy_rtx_and_substitute (XEXP (orig, 1)));
	      }
	  }
	used_const_equiv |= old_used_const_equiv;
      }
      return temp;

    case MEM:
      /* Take care of easiest case here.  */
      copy = XEXP (orig, 0);
      if (copy == frame_pointer_rtx || copy == arg_pointer_rtx)
	return gen_rtx (MEM, mode,
			plus_constant (frame_pointer_rtx, fp_delta));

      /* Allow a pushing-address even if that is not valid as an
	 ordinary memory address.  It indicates we are inlining a special
	 push-insn.  These must be copied; otherwise unshare_all_rtl
	 might clobber them to point at temporary rtl of this function.  */
#ifdef STACK_GROWS_DOWNWARD
      if (GET_CODE (copy) == PRE_DEC && XEXP (copy, 0) == stack_pointer_rtx)
	return gen_rtx (MEM, mode, copy_rtx_and_substitute (copy));
#else
      if (GET_CODE (copy) == PRE_INC && XEXP (copy, 0) == stack_pointer_rtx)
	return gen_rtx (MEM, mode, copy_rtx_and_substitute (copy));
#endif

      /* If this is some other sort of address that isn't generally valid,
	 break out all the registers referred to.  */
      if (! memory_address_p (mode, copy))
	return gen_rtx (MEM, mode, copy_address (copy));

      /* There is no easy way to get our mode to `access_parm_map', which
	 may need to know it, so here is most of the PLUS code duplicated.  */
      if (GET_CODE (copy) == PLUS)
	{
	  if (XEXP (copy, 0) == frame_pointer_rtx
	      || (ARG_POINTER_REGNUM != FRAME_POINTER_REGNUM
		  && XEXP (copy, 0) == arg_pointer_rtx))
	    {
	      rtx reg;
	      reg = XEXP (copy, 0), copy = XEXP (copy, 1);

	      if (GET_CODE (copy) == CONST_INT)
		{
		  int c = INTVAL (copy);

		  if (reg == arg_pointer_rtx && c >= first_parm_offset)
		    return access_parm_map (c, mode);

		  temp = gen_rtx (PLUS, Pmode,
				  frame_pointer_rtx,
				  gen_rtx (CONST_INT, SImode,
					   c + fp_delta));
		  if (! memory_address_p (Pmode, temp))
		    {
		      if (inline_fp_rtx == 0)
			{
			  rtx last = get_last_insn ();
			  inline_fp_rtx
			    = copy_to_mode_reg (Pmode,
						plus_constant (frame_pointer_rtx, fp_delta));
			  reorder_insns (NEXT_INSN (last), get_last_insn (), before_inline_fp_rtx);
			}
		      return gen_rtx (MEM, mode, plus_constant (inline_fp_rtx, c));
		    }
		}
	      copy =  copy_rtx_and_substitute (copy);
	      temp = gen_rtx (PLUS, Pmode, frame_pointer_rtx, copy);
	      temp = plus_constant (temp, fp_delta);
	      temp = memory_address (Pmode, temp);
	    }
	  else if (reg_mentioned_p (frame_pointer_rtx, copy)
		   || (ARG_POINTER_REGNUM != FRAME_POINTER_REGNUM
		       && reg_mentioned_p (arg_pointer_rtx, copy)))
	    {
	      if (GET_CODE (XEXP (copy, 1)) == CONST_INT)
		{
		  temp = copy_rtx_and_substitute (XEXP (copy, 0));
		  temp = plus_constant (temp, INTVAL (XEXP (copy, 1)));
		}
	      else
		{
		  temp = gen_rtx (PLUS, GET_MODE (copy),
				  copy_rtx_and_substitute (XEXP (copy, 0)),
				  copy_rtx_and_substitute (XEXP (copy, 1)));
		}
	    }
	  else
	    {
	      if (GET_CODE (XEXP (copy, 1)) == CONST_INT)
		temp = plus_constant (copy_rtx_and_substitute (XEXP (copy, 0)),
				      INTVAL (XEXP (copy, 1)));
	      else
		{
		  rtx left = copy_rtx_and_substitute (XEXP (copy, 0));
		  rtx right = copy_rtx_and_substitute (XEXP (copy, 1));

		  temp = gen_rtx (PLUS, GET_MODE (copy), left, right);
		}
	    }
	}
      else
	temp = copy_rtx_and_substitute (copy);

      temp = change_address (orig, mode, temp);
      /* Deals with GCC bug for now.  */
      RTX_UNCHANGING_P (temp) = 0;
      return temp;

    case RETURN:
      abort ();
    }

  copy = rtx_alloc (code);
  PUT_MODE (copy, mode);
  copy->in_struct = orig->in_struct;
  copy->volatil = orig->volatil;
  copy->unchanging = orig->unchanging;

  format_ptr = GET_RTX_FORMAT (GET_CODE (copy));

  for (i = 0; i < GET_RTX_LENGTH (GET_CODE (copy)); i++)
    {
      switch (*format_ptr++)
	{
	case '0':
	  break;

	case 'e':
	  XEXP (copy, i) = copy_rtx_and_substitute (XEXP (orig, i));
	  break;

	case 'u':
	  /* Change any references to old-insns to point to the
	     corresponding copied insns.  */
	  XEXP (copy, i) = insn_map[INSN_UID (XEXP (orig, i))];
	  break;

	case 'E':
	  XVEC (copy, i) = XVEC (orig, i);
	  if (XVEC (orig, i) != NULL && XVECLEN (orig, i) != 0)
	    {
	      XVEC (copy, i) = rtvec_alloc (XVECLEN (orig, i));
	      for (j = 0; j < XVECLEN (copy, i); j++)
		XVECEXP (copy, i, j) = copy_rtx_and_substitute (XVECEXP (orig, i, j));
	    }
	  break;

	case 'i':
	  XINT (copy, i) = XINT (orig, i);
	  break;

	case 's':
	  XSTR (copy, i) = XSTR (orig, i);
	  break;

	default:
	  abort ();
	}
    }

  if (code == ASM_OPERANDS && orig_asm_operands_vector == 0)
    {
      orig_asm_operands_vector = XVEC (orig, 3);
      copy_asm_operands_vector = XVEC (copy, 3);
      copy_asm_constraints_vector = XVEC (copy, 4);
    }

  return copy;
}

/* Get the value corresponding to an address relative to the arg pointer
   at index RELADDRESS.  MODE is the machine mode of the reference.
   MODE is used only when the value is a REG.
   Pass VOIDmode for MODE when the mode is not known;
   in such cases, you should make sure the value is a MEM.  */

static rtx
access_parm_map (reladdress, mode)
     int reladdress;
     enum machine_mode mode;
{
  /* Index in parm_map.  */
  int index = reladdress / UNITS_PER_WORD;
  /* Offset of the data being referenced
     from the beginning of the value for that parm.  */
  int offset = reladdress % UNITS_PER_WORD;
  rtx copy;

  /* If we are referring to the middle of a multiword parm,
     find the beginning of that parm.
     OFFSET gets the offset of the reference from
     the beginning of the parm.  */

  while (parm_map[index] == 0)
    {
      index--;
      if (index < first_parm_offset / UNITS_PER_WORD)
	/* If this abort happens, it means we need
	   to handle "decrementing" INDEX back far
	   enough to start looking among the reg parms
	   instead of the stack parms.  What a mess!  */
	abort ();
      offset += UNITS_PER_WORD;
    }

  copy = parm_map[index];

#ifdef BYTES_BIG_ENDIAN
  /* Subtract from OFFSET the offset of where
     the actual (non-BLKmode) parm value would start.  */
  if (GET_MODE (copy) != BLKmode
      && GET_MODE_SIZE (GET_MODE (copy)) < UNITS_PER_WORD)
    offset
      -= (UNITS_PER_WORD
	  - GET_MODE_SIZE (GET_MODE (copy)));
#endif

  /* For memory ref, adjust it by the desired offset.  */
  if (GET_CODE (copy) == MEM)
    {
      if (offset != 0 || GET_MODE (copy) != mode)
	return change_address (copy, mode,
			       plus_constant (XEXP (copy, 0),
					      offset));
      return copy;
    }

  if (GET_CODE (copy) != REG && GET_CODE (copy) != SUBREG
      && ! CONSTANT_P (copy))
    abort ();
  if (mode == VOIDmode)
    abort ();

  /* A REG cannot be offset by bytes, so use a subreg
     (which is possible only in certain cases).  */
  if (GET_MODE (copy) != mode
      && GET_MODE (copy) != VOIDmode)
    {
      int word;
      /* Crash if the portion of the arg wanted
	 is not the least significant.
	 Functions with refs to other parts of a
	 parameter should not be inline--
	 see function_cannot_inline_p. */
#ifdef BYTES_BIG_ENDIAN
      if (offset + GET_MODE_SIZE (mode)
	  != GET_MODE_SIZE (GET_MODE (copy)))
	abort ();
#else
      if (offset != 0)
	abort ();
#endif
      word = 0;
      if (GET_CODE (copy) == SUBREG)
	word = SUBREG_WORD (copy), copy = SUBREG_REG (copy);
      if (CONSTANT_P (copy))
	copy = force_reg (GET_MODE (copy), copy);
      return gen_rtx (SUBREG, mode, copy, word);
    }

  return copy;
}

/* Like copy_rtx_and_substitute but produces different output, suitable
   for an ideosyncractic address that isn't memory_address_p.
   The output resembles the input except that REGs and MEMs are replaced
   with new psuedo registers.  All the "real work" is done in separate
   insns which set up the values of these new registers.  */

static rtx
copy_address (orig)
     register rtx orig;
{
  register rtx copy;
  register int i, j;
  register RTX_CODE code;
  register enum machine_mode mode;
  register char *format_ptr;

  if (orig == 0)
    return 0;

  code = GET_CODE (orig);
  mode = GET_MODE (orig);

  switch (code)
    {
    case REG:
      if (REGNO (orig) != FRAME_POINTER_REGNUM)
	return copy_rtx_and_substitute (orig);
      return plus_constant (frame_pointer_rtx, fp_delta);

    case PLUS:
      if (XEXP (orig, 0) == frame_pointer_rtx)
	return plus_constant (orig, fp_delta);
      break;

    case MEM:
      return copy_to_reg (copy_rtx_and_substitute (orig));

    case CODE_LABEL:
    case LABEL_REF:
      return copy_rtx_and_substitute (orig);

    case PC:
    case CC0:
    case CONST_INT:
    case CONST_DOUBLE:
    case SYMBOL_REF:
      return orig;
    }

  copy = rtx_alloc (code);
  PUT_MODE (copy, mode);
  copy->in_struct = orig->in_struct;
  copy->volatil = orig->volatil;
  copy->unchanging = orig->unchanging;

  format_ptr = GET_RTX_FORMAT (GET_CODE (copy));

  for (i = 0; i < GET_RTX_LENGTH (GET_CODE (copy)); i++)
    {
      switch (*format_ptr++)
	{
	case '0':
	  break;

	case 'e':
	  XEXP (copy, i) = copy_rtx_and_substitute (XEXP (orig, i));
	  break;

	case 'u':
	  /* Change any references to old-insns to point to the
	     corresponding copied insns.  */
	  XEXP (copy, i) = insn_map[INSN_UID (XEXP (orig, i))];
	  break;

	case 'E':
	  XVEC (copy, i) = XVEC (orig, i);
	  if (XVEC (orig, i) != NULL && XVECLEN (orig, i) != 0)
	    {
	      XVEC (copy, i) = rtvec_alloc (XVECLEN (orig, i));
	      for (j = 0; j < XVECLEN (copy, i); j++)
		XVECEXP (copy, i, j) = copy_rtx_and_substitute (XVECEXP (orig, i, j));
	    }
	  break;

	case 'i':
	  XINT (copy, i) = XINT (orig, i);
	  break;

	case 's':
	  XSTR (copy, i) = XSTR (orig, i);
	  break;

	default:
	  abort ();
	}
    }
  return copy;
}

/* Return the constant equivalent of X.  If X is a manifest
   constant, it is returned.  If X is a register, we check
   to see if we happen to know its value as a constant.  */
static rtx
const_equiv (x)
     rtx x;
{
  if (GET_CODE (x) == REG)
    {
      int regno = REGNO (x);
      if (const_equiv_map[regno]
	  && const_age_map[regno] >= const_age)
	return const_equiv_map[regno];
      return 0;
    }
  if (CONSTANT_P (x))
    return x;
  return 0;
}

/* Attempt to simplify INSN while copying it from an inline fn,
   assuming it is a SET that sets CC0.

   If we simplify it, we emit the appropriate insns and return
   the last insn that we have handled (since we may handle the insn
   that follows INSN as well as INSN itself).

   Otherwise we do nothing and return zero.  */

static rtx
try_fold_cc0 (insn)
     rtx insn;
{
  rtx cnst = copy_rtx_and_substitute (SET_SRC (PATTERN (insn)));
  rtx pat, copy;

  if (! CONSTANT_P (cnst))
    /* Constant equivlancies are with old, not new rtl.  */
    cnst = const_equiv (SET_SRC (PATTERN (insn)));
  if (cnst
      /* @@ Cautious: Don't know how many of these tests we need.  */
      && NEXT_INSN (insn)
      && GET_CODE (pat = PATTERN (NEXT_INSN (insn))) == SET
      && SET_DEST (pat) == pc_rtx
      && GET_CODE (pat = SET_SRC (pat)) == IF_THEN_ELSE
      && GET_RTX_LENGTH (GET_CODE (XEXP (pat, 0))) == 2)
    {
      rtx cnst2;
      rtx cond = XEXP (pat, 0);

      if ((XEXP (cond, 0) == cc0_rtx
	   && (cnst2 = const_equiv (XEXP (cond, 1))))
	  || (XEXP (cond, 1) == cc0_rtx
	      && (cnst2 = const_equiv (XEXP (cond, 0)))))
	{
	  copy = fold_out_const_cc0 (cond, XEXP (pat, 1), XEXP (pat, 2),
				     cnst, cnst2);
	  if (copy)
	    {
	      if (GET_CODE (copy) == LABEL_REF)
		{
		  /* We will branch unconditionally to
		     the label specified by COPY.
		     Eliminate dead code by running down the
		     list of insn until we see a CODE_LABEL.
		     If the CODE_LABEL is the one specified
		     by COPY, we win, and can delete all code
		     up to (but not necessarily including)
		     that label.  Otherwise only win a little:
		     emit the branch insn, and continue expanding.  */
		  rtx tmp = NEXT_INSN (insn);
		  while (tmp && GET_CODE (tmp) != CODE_LABEL)
		    tmp = NEXT_INSN (tmp);
		  if (! tmp)
		    abort ();
		  if (label_map[CODE_LABEL_NUMBER (tmp)] == XEXP (copy, 0))
		    {
		      /* Big win.  */
		      return PREV_INSN (tmp);
		    }
		  else
		    {
		      /* Small win.  Emit the unconditional branch,
			 followed by a BARRIER, so that jump optimization
			 will know what to do.  */
		      emit_jump (XEXP (copy, 0));
		      return NEXT_INSN (insn);
		    }
		}
	      else if (copy == pc_rtx)
		{
		  /* Do not take the branch, just fall through.
		     Jump optimize should handle the elimination of
		     dead code if appropriate.  */
		  return NEXT_INSN (insn);
		}
	      else
		abort ();
	    }
	}
    }
  return 0;
}

/* If (COND_RTX CNST1 CNST2) yield a result we can treat
   as being constant, return THEN_RTX if the result is always
   non-zero, and return ELSE_RTX otherwise.  */
static rtx
fold_out_const_cc0 (cond_rtx, then_rtx, else_rtx, cnst1, cnst2)
     rtx cond_rtx, then_rtx, else_rtx;
     rtx cnst1, cnst2;
{
  int value1, value2;
  int int1 = GET_CODE (cnst1) == CONST_INT;
  int int2 = GET_CODE (cnst2) == CONST_INT;
  if (int1)
    value1 = INTVAL (cnst1);
  else
    value1 = 1;
  if (int2)
    value2 = INTVAL (cnst2);
  else
    value2 = 1;

  switch (GET_CODE (cond_rtx))
    {
    case NE:
      if (int1 && int2)
	if (value1 != value2)
	  return copy_rtx_and_substitute (then_rtx);
	else
	  return copy_rtx_and_substitute (else_rtx);
      if (value1 == 0 || value2 == 0)
	return copy_rtx_and_substitute (then_rtx);
      if (int1 == 0 && int2 == 0)
	if (rtx_equal_p (cnst1, cnst2))
	  return copy_rtx_and_substitute (else_rtx);
      break;
    case EQ:
      if (int1 && int2)
	if (value1 == value2)
	  return copy_rtx_and_substitute (then_rtx);
	else
	  return copy_rtx_and_substitute (else_rtx);
      if (value1 == 0 || value2 == 0)
	return copy_rtx_and_substitute (else_rtx);
      if (int1 == 0 && int2 == 0)
	if (rtx_equal_p (cnst1, cnst2))
	  return copy_rtx_and_substitute (then_rtx);
      break;
    case GE:
      if (int1 && int2)
	if (value1 >= value2)
	  return copy_rtx_and_substitute (then_rtx);
	else
	  return copy_rtx_and_substitute (else_rtx);
      if (value1 == 0)
	return copy_rtx_and_substitute (else_rtx);
      if (value2 == 0)
	return copy_rtx_and_substitute (then_rtx);
      break;
    case GT:
      if (int1 && int2)
	if (value1 > value2)
	  return copy_rtx_and_substitute (then_rtx);
	else
	  return copy_rtx_and_substitute (else_rtx);
      if (value1 == 0)
	return copy_rtx_and_substitute (else_rtx);
      if (value2 == 0)
	return copy_rtx_and_substitute (then_rtx);
      break;
    case LE:
      if (int1 && int2)
	if (value1 <= value2)
	  return copy_rtx_and_substitute (then_rtx);
	else
	  return copy_rtx_and_substitute (else_rtx);
      if (value1 == 0)
	return copy_rtx_and_substitute (then_rtx);
      if (value2 == 0)
	return copy_rtx_and_substitute (else_rtx);
      break;
    case LT:
      if (int1 && int2)
	if (value1 < value2)
	  return copy_rtx_and_substitute (then_rtx);
	else
	  return copy_rtx_and_substitute (else_rtx);
      if (value1 == 0)
	return copy_rtx_and_substitute (then_rtx);
      if (value2 == 0)
	return copy_rtx_and_substitute (else_rtx);
      break;
    case GEU:
      if (int1 && int2)
	if ((unsigned)value1 >= (unsigned)value2)
	  return copy_rtx_and_substitute (then_rtx);
	else
	  return copy_rtx_and_substitute (else_rtx);
      if (value1 == 0)
	return copy_rtx_and_substitute (else_rtx);
      if (value2 == 0)
	return copy_rtx_and_substitute (then_rtx);
      break;
    case GTU:
      if (int1 && int2)
	if ((unsigned)value1 > (unsigned)value2)
	  return copy_rtx_and_substitute (then_rtx);
	else
	  return copy_rtx_and_substitute (else_rtx);
      if (value1 == 0)
	return copy_rtx_and_substitute (else_rtx);
      if (value2 == 0)
	return copy_rtx_and_substitute (then_rtx);
      break;
    case LEU:
      if (int1 && int2)
	if ((unsigned)value1 <= (unsigned)value2)
	  return copy_rtx_and_substitute (then_rtx);
	else
	  return copy_rtx_and_substitute (else_rtx);
      if (value1 == 0)
	return copy_rtx_and_substitute (then_rtx);
      if (value2 == 0)
	return copy_rtx_and_substitute (else_rtx);
      break;
    case LTU:
      if (int1 && int2)
	if ((unsigned)value1 < (unsigned)value2)
	  return copy_rtx_and_substitute (then_rtx);
	else
	  return copy_rtx_and_substitute (else_rtx);
      if (value1 == 0)
	return copy_rtx_and_substitute (then_rtx);
      if (value2 == 0)
	return copy_rtx_and_substitute (else_rtx);
      break;
    }
  /* Could not hack it.  */
  return 0;
}

/* If any CONST expressions with RTX_INTEGRATED_P are present in X,
   they should be in the constant pool.
   Run force_const_mem to put them there.  */

static void
restore_constants (x)
     rtx x;
{
  int i, j;
  char *fmt = GET_RTX_FORMAT (GET_CODE (x));

  for (i = 0; i < GET_RTX_LENGTH (GET_CODE (x)); i++)
    {
      switch (*fmt++)
	{
	case 'E':
	  for (j = 0; j < XVECLEN (x, i); j++)
	    if (RTX_INTEGRATED_P (XVECEXP (x, i, j))
		&& GET_CODE (XVECEXP (x, i, j)) == CONST)
	    XVECEXP (x, i, j) = force_const_mem (GET_MODE (XVECEXP (x, i, j)),
						 XEXP (XVECEXP (x, i, j), 0));
	  else
	    restore_constants (XVECEXP (x, i, j));
	  break;

	case 'e':
	  if (RTX_INTEGRATED_P (XEXP (x, i))
	      && GET_CODE (XEXP (x, i)) == CONST)
	    XEXP (x, i) = force_const_mem (GET_MODE (XEXP (x, i)),
					   XEXP (XEXP (x, i), 0));
	  else
	    restore_constants (XEXP (x, i));
	  break;
	}
    }
}

/* Output the assembly language code for the function FNDECL
   from its DECL_SAVED_INSNS.  Used for inline functions that are output
   at end of compilation instead of where they came in the source.  */

void
output_inline_function (fndecl)
     tree fndecl;
{
  rtx head = DECL_SAVED_INSNS (fndecl);
  rtx last;
  extern rtx stack_slot_list;

  temporary_allocation ();

  current_function_decl = fndecl;

  /* This call is only used to initialize global variables.  */
  init_function_start (fndecl);

  /* Set stack frame size.  */
  assign_stack_local (BLKmode, DECL_FRAME_SIZE (fndecl));

  restore_reg_data (FIRST_PARM_INSN (head));

  stack_slot_list = XEXP (head, 9);

  /* There is no need to output a return label again.  */
  return_label = 0;
  expand_function_end (DECL_SOURCE_FILE (fndecl), DECL_SOURCE_LINE (fndecl));

  /* Find last insn and rebuild the constant pool.  */
  for (last = FIRST_PARM_INSN (head);
       NEXT_INSN (last); last = NEXT_INSN (last))
    {
#if 0
      /* No need to restore these constants again.  */
      if (GET_CODE (last) == INSN || GET_CODE (last) == JUMP_INSN
	  || GET_CODE (last) == CALL_INSN)
	restore_constants (PATTERN (last));
#endif
    }

  set_new_first_and_last_insn (FIRST_PARM_INSN (head), last);

  /* Compile this function all the way down to assembly code.  */
  rest_of_compilation (fndecl);

  current_function_decl = 0;

  permanent_allocation ();
}
#if 0

/* Hashing of rtxs so that we don't make duplicates.
   The entry point is `rtx_hash_canon'.  */

/* Each hash table slot is a bucket containing a chain
   of these structures.  */

struct rtx_hash
{
  struct rtx_hash *next;	/* Next structure in the bucket.  */
  int hashcode;			/* Hash code of this type.  */
  rtx x;			/* The rtx recorded here.  */
};

/* Now here is the hash table.  This works exactly the same way
   that types are hashed in tree.c, except this is for rtxs.  */

#define RTX_HASH_SIZE 199
struct rtx_hash *rtx_hash_table[RTX_HASH_SIZE];

/* Here is how primitive or already-canonicalized types' hash
   codes are made.  */
#define RTX_HASH(RTX) (RTX)

/* Look in the type hash table for a type isomorphic to RTX.
   If one is found, return it.  Otherwise return 0.  */

tree
rtx_hash_lookup (hashcode, x)
     int hashcode;
     tree x;
{
  register struct rtx_hash *h;
  for (h = rtx_hash_table[hashcode % RTX_HASH_SIZE]; h; h = h->next)
    if (h->hashcode == hashcode
	&& GET_CODE (h->x) == GET_CODE (x)
	&& GET_MODE (h->x) == GET_MODE (x)
#if 0
	&& h->x->jump == x->jump
	&& h->x->call == x->call
	&& h->x->unchanging == x->unchanging
	&& h->x->volatil == x->volatil
	&& h->x->in_struct == x->in_struct
	&& h->x->used == x->used
	&& h->x->integrated == x->integrated
#endif
	)
      {
	int i, j;
	int len = GET_RTX_LENGTH (GET_CODE (x));
	char *fmt = GET_RTX_FORMAT (GET_CODE (x));

	for (i = 0; i < len; i++)
	  switch (fmt[i])
	    {
	    case '0':
	      break;

	    case 'e':
	    case 'u':
	    case 's':
	    case 'S':
	      if (XEXP (h->x, i) != XEXP (x, i))
		goto no_dice;
	      break;

	    case 'E':
	      if (XVECLEN (h->x, i) != XVECLEN (x, i))
		goto no_dice;
	      for (j = 0; j < XVECLEN (x, i); j++)
		if (XVECEXP (h->x, i, j) != XVECEXP (x, i, j))
		  goto no_dice;
	      break;

	    case 'i':
	    case 'n':
	      if (INTVAL (XEXP (h->x, i)) != INTVAL (XEXP (x, i)))
		goto no_dice;

	    default:
	      abort ();
	    }

	/* Everything matched.  */
	return h->x;

	/* Try more.  */
      no_dice:
	;
      }

  /* Nothing matched.  */
  return 0;
}

/* Add an entry to the rtx-hash-table
   for a type RTX whose hash code is HASHCODE.  */

void
rtx_hash_add (hashcode, x)
     int hashcode;
     tree x;
{
  register struct rtx_hash *h;

  h = (struct rtx_hash *) oballoc (sizeof (struct rtx_hash));
  h->hashcode = hashcode;
  h->x = x;
  h->next = rtx_hash_table[hashcode % RTX_HASH_SIZE];
  rtx_hash_table[hashcode % RTX_HASH_SIZE] = h;
}

/* Given RTX, and HASHCODE its hash code, return the canonical
   object for an identical rtx if one already exists.
   Otherwise, return RTX, and record it as the canonical object
   if it is a permanent object.

   To use this function, first create a rtx of the sort you want.
   Then compute its hash code from the fields of the rtx that
   make it different from other similar rtxs.
   Then call this function and use the value.
   This function frees the rtx you pass in if it is a duplicate.  */

/* Set to 1 to debug without canonicalization.  Never set by program.  */
int debug_no_rtx_hash = 0;

tree
rtx_hash_canon (hashcode, x)
     int hashcode;
     tree x;
{
  tree x1;

  if (debug_no_rtx_hash)
    return x;

  x1 = rtx_hash_lookup (hashcode, x);
  if (x1 != 0)
    {
      struct obstack *o = maybepermanent_obstack;
      obstack_free (o, x);
      return x1;
    }

  /* If this is a new type, record it for later reuse.  */
  rtx_hash_add (hashcode, x);

  return x;
}
#endif
