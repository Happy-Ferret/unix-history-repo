/* Copyright (C) 2002 Free Software Foundation, Inc.

   This file is part of GNU CC.

   GNU CC is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GNU CC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU CC; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, if you include this header file into source
   files compiled by GCC, this header file does not by itself cause
   the resulting executable to be covered by the GNU General Public
   License.  This exception does not however invalidate any other
   reasons why the executable file might be covered by the GNU General
   Public License.  */

/* Implemented from the specification included in the Intel C++ Compiler
   User Guide and Reference, version 5.0.  */

#ifndef _XMMINTRIN_H_INCLUDED
#define _XMMINTRIN_H_INCLUDED

/* We need type definitions from the MMX header file.  */
#include <mmintrin.h>

/* The data type indended for user use.  */
typedef int __m128 __attribute__ ((__mode__(__V4SF__)));

/* Internal data types for implementing the instrinsics.  */
typedef int __v4sf __attribute__ ((__mode__(__V4SF__)));
typedef int __v4si __attribute__ ((__mode__(__V4SI__)));

/* Create a selector for use with the SHUFPS instruction.  */
#define _MM_SHUFFLE(fp3,fp2,fp1,fp0) \
 (((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | (fp0))

/* Constants for use with _mm_prefetch.  */
enum _mm_hint
{
  _MM_HINT_T0 = 3,
  _MM_HINT_T1 = 2,
  _MM_HINT_T2 = 1,
  _MM_HINT_NTA = 0
};

/* Bits in the MXCSR.  */
#define _MM_EXCEPT_MASK       0x003f
#define _MM_EXCEPT_INVALID    0x0001
#define _MM_EXCEPT_DENORM     0x0002
#define _MM_EXCEPT_DIV_ZERO   0x0004
#define _MM_EXCEPT_OVERFLOW   0x0008
#define _MM_EXCEPT_UNDERFLOW  0x0010
#define _MM_EXCEPT_INEXACT    0x0020

#define _MM_MASK_MASK         0x1f80
#define _MM_MASK_INVALID      0x0080
#define _MM_MASK_DENORM       0x0100
#define _MM_MASK_DIV_ZERO     0x0200
#define _MM_MASK_OVERFLOW     0x0400
#define _MM_MASK_UNDERFLOW    0x0800
#define _MM_MASK_INEXACT      0x1000

#define _MM_ROUND_MASK        0x6000
#define _MM_ROUND_NEAREST     0x0000
#define _MM_ROUND_DOWN        0x2000
#define _MM_ROUND_UP          0x4000
#define _MM_ROUND_TOWARD_ZERO 0x6000

#define _MM_FLUSH_ZERO_MASK   0x8000
#define _MM_FLUSH_ZERO_ON     0x8000
#define _MM_FLUSH_ZERO_OFF    0x0000

/* Perform the respective operation on the lower SPFP (single-precision
   floating-point) values of A and B; the upper three SPFP values are
   passed through from A.  */

static __inline __m128
_mm_add_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_addss ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_sub_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_subss ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_mul_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_mulss ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_div_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_divss ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_sqrt_ss (__m128 __A)
{
  return (__m128) __builtin_ia32_sqrtss ((__v4sf)__A);
}

static __inline __m128
_mm_rcp_ss (__m128 __A)
{
  return (__m128) __builtin_ia32_rcpss ((__v4sf)__A);
}

static __inline __m128
_mm_rsqrt_ss (__m128 __A)
{
  return (__m128) __builtin_ia32_rsqrtss ((__v4sf)__A);
}

static __inline __m128
_mm_min_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_minss ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_max_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_maxss ((__v4sf)__A, (__v4sf)__B);
}

/* Perform the respective operation on the four SPFP values in A and B.  */

static __inline __m128
_mm_add_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_addps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_sub_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_subps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_mul_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_mulps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_div_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_divps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_sqrt_ps (__m128 __A)
{
  return (__m128) __builtin_ia32_sqrtps ((__v4sf)__A);
}

static __inline __m128
_mm_rcp_ps (__m128 __A)
{
  return (__m128) __builtin_ia32_rcpps ((__v4sf)__A);
}

static __inline __m128
_mm_rsqrt_ps (__m128 __A)
{
  return (__m128) __builtin_ia32_rsqrtps ((__v4sf)__A);
}

static __inline __m128
_mm_min_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_minps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_max_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_maxps ((__v4sf)__A, (__v4sf)__B);
}

/* Perform logical bit-wise operations on 128-bit values.  */

static __inline __m128
_mm_and_ps (__m128 __A, __m128 __B)
{
  return __builtin_ia32_andps (__A, __B);
}

static __inline __m128
_mm_andnot_ps (__m128 __A, __m128 __B)
{
  return __builtin_ia32_andnps (__A, __B);
}

static __inline __m128
_mm_or_ps (__m128 __A, __m128 __B)
{
  return __builtin_ia32_orps (__A, __B);
}

static __inline __m128
_mm_xor_ps (__m128 __A, __m128 __B)
{
  return __builtin_ia32_xorps (__A, __B);
}

/* Perform a comparison on the lower SPFP values of A and B.  If the
   comparison is true, place a mask of all ones in the result, otherwise a
   mask of zeros.  The upper three SPFP values are passed through from A.  */

static __inline __m128
_mm_cmpeq_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpeqss ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmplt_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpltss ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmple_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpless ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpgt_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpgtss ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpge_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpgess ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpneq_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpneqss ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpnlt_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpnltss ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpnle_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpnless ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpngt_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpngtss ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpnge_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpngess ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpord_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpordss ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpunord_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpunordss ((__v4sf)__A, (__v4sf)__B);
}

/* Perform a comparison on the four SPFP values of A and B.  For each
   element, if the comparison is true, place a mask of all ones in the
   result, otherwise a mask of zeros.  */

static __inline __m128
_mm_cmpeq_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpeqps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmplt_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpltps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmple_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpleps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpgt_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpgtps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpge_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpgeps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpneq_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpneqps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpnlt_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpnltps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpnle_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpnleps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpngt_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpngtps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpnge_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpngeps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpord_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpordps ((__v4sf)__A, (__v4sf)__B);
}

static __inline __m128
_mm_cmpunord_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_cmpunordps ((__v4sf)__A, (__v4sf)__B);
}

/* Compare the lower SPFP values of A and B and return 1 if true
   and 0 if false.  */

static __inline int
_mm_comieq_ss (__m128 __A, __m128 __B)
{
  return __builtin_ia32_comieq ((__v4sf)__A, (__v4sf)__B);
}

static __inline int
_mm_comilt_ss (__m128 __A, __m128 __B)
{
  return __builtin_ia32_comilt ((__v4sf)__A, (__v4sf)__B);
}

static __inline int
_mm_comile_ss (__m128 __A, __m128 __B)
{
  return __builtin_ia32_comile ((__v4sf)__A, (__v4sf)__B);
}

static __inline int
_mm_comigt_ss (__m128 __A, __m128 __B)
{
  return __builtin_ia32_comigt ((__v4sf)__A, (__v4sf)__B);
}

static __inline int
_mm_comige_ss (__m128 __A, __m128 __B)
{
  return __builtin_ia32_comige ((__v4sf)__A, (__v4sf)__B);
}

static __inline int
_mm_comineq_ss (__m128 __A, __m128 __B)
{
  return __builtin_ia32_comineq ((__v4sf)__A, (__v4sf)__B);
}

static __inline int
_mm_ucomieq_ss (__m128 __A, __m128 __B)
{
  return __builtin_ia32_ucomieq ((__v4sf)__A, (__v4sf)__B);
}

static __inline int
_mm_ucomilt_ss (__m128 __A, __m128 __B)
{
  return __builtin_ia32_ucomilt ((__v4sf)__A, (__v4sf)__B);
}

static __inline int
_mm_ucomile_ss (__m128 __A, __m128 __B)
{
  return __builtin_ia32_ucomile ((__v4sf)__A, (__v4sf)__B);
}

static __inline int
_mm_ucomigt_ss (__m128 __A, __m128 __B)
{
  return __builtin_ia32_ucomigt ((__v4sf)__A, (__v4sf)__B);
}

static __inline int
_mm_ucomige_ss (__m128 __A, __m128 __B)
{
  return __builtin_ia32_ucomige ((__v4sf)__A, (__v4sf)__B);
}

static __inline int
_mm_ucomineq_ss (__m128 __A, __m128 __B)
{
  return __builtin_ia32_ucomineq ((__v4sf)__A, (__v4sf)__B);
}

/* Convert the lower SPFP value to a 32-bit integer according to the current
   rounding mode.  */
static __inline int
_mm_cvtss_si32 (__m128 __A)
{
  return __builtin_ia32_cvtss2si ((__v4sf) __A);
}

/* Convert the two lower SPFP values to 32-bit integers according to the
   current rounding mode.  Return the integers in packed form.  */
static __inline __m64
_mm_cvtps_pi32 (__m128 __A)
{
  return (__m64) __builtin_ia32_cvtps2pi ((__v4sf) __A);
}

/* Truncate the lower SPFP value to a 32-bit integer.  */
static __inline int
_mm_cvttss_si32 (__m128 __A)
{
  return __builtin_ia32_cvttss2si ((__v4sf) __A);
}

/* Truncate the two lower SPFP values to 32-bit integers.  Return the
   integers in packed form.  */
static __inline __m64
_mm_cvttps_pi32 (__m128 __A)
{
  return (__m64) __builtin_ia32_cvttps2pi ((__v4sf) __A);
}

/* Convert B to a SPFP value and insert it as element zero in A.  */
static __inline __m128
_mm_cvtsi32_ss (__m128 __A, int __B)
{
  return (__m128) __builtin_ia32_cvtsi2ss ((__v4sf) __A, __B);
}

/* Convert the two 32-bit values in B to SPFP form and insert them
   as the two lower elements in A.  */
static __inline __m128
_mm_cvtpi32_ps (__m128 __A, __m64 __B)
{
  return (__m128) __builtin_ia32_cvtpi2ps ((__v4sf) __A, (__v2si)__B);
}

/* Convert the four signed 16-bit values in A to SPFP form.  */
static __inline __m128
_mm_cvtpi16_ps (__m64 __A)
{
  __v4hi __sign;
  __v2si __hisi, __losi;
  __v4sf __r;

  /* This comparison against zero gives us a mask that can be used to
     fill in the missing sign bits in the unpack operations below, so
     that we get signed values after unpacking.  */
  __sign = (__v4hi) __builtin_ia32_mmx_zero ();
  __sign = __builtin_ia32_pcmpgtw (__sign, (__v4hi)__A);

  /* Convert the four words to doublewords.  */
  __hisi = (__v2si) __builtin_ia32_punpckhwd ((__v4hi)__A, __sign);
  __losi = (__v2si) __builtin_ia32_punpcklwd ((__v4hi)__A, __sign);

  /* Convert the doublewords to floating point two at a time.  */
  __r = (__v4sf) __builtin_ia32_setzerops ();
  __r = __builtin_ia32_cvtpi2ps (__r, __hisi);
  __r = __builtin_ia32_movlhps (__r, __r);
  __r = __builtin_ia32_cvtpi2ps (__r, __losi);

  return (__m128) __r;
}

/* Convert the four unsigned 16-bit values in A to SPFP form.  */
static __inline __m128
_mm_cvtpu16_ps (__m64 __A)
{
  __v4hi __zero = (__v4hi) __builtin_ia32_mmx_zero ();
  __v2si __hisi, __losi;
  __v4sf __r;

  /* Convert the four words to doublewords.  */
  __hisi = (__v2si) __builtin_ia32_punpckhwd ((__v4hi)__A, __zero);
  __losi = (__v2si) __builtin_ia32_punpcklwd ((__v4hi)__A, __zero);

  /* Convert the doublewords to floating point two at a time.  */
  __r = (__v4sf) __builtin_ia32_setzerops ();
  __r = __builtin_ia32_cvtpi2ps (__r, __hisi);
  __r = __builtin_ia32_movlhps (__r, __r);
  __r = __builtin_ia32_cvtpi2ps (__r, __losi);

  return (__m128) __r;
}

/* Convert the low four signed 8-bit values in A to SPFP form.  */
static __inline __m128
_mm_cvtpi8_ps (__m64 __A)
{
  __v8qi __sign;

  /* This comparison against zero gives us a mask that can be used to
     fill in the missing sign bits in the unpack operations below, so
     that we get signed values after unpacking.  */
  __sign = (__v8qi) __builtin_ia32_mmx_zero ();
  __sign = __builtin_ia32_pcmpgtb (__sign, (__v8qi)__A);

  /* Convert the four low bytes to words.  */
  __A = (__m64) __builtin_ia32_punpcklbw ((__v8qi)__A, __sign);

  return _mm_cvtpi16_ps(__A);
}

/* Convert the low four unsigned 8-bit values in A to SPFP form.  */
static __inline __m128
_mm_cvtpu8_ps(__m64 __A)
{
  __v8qi __zero = (__v8qi) __builtin_ia32_mmx_zero ();
  __A = (__m64) __builtin_ia32_punpcklbw ((__v8qi)__A, __zero);
  return _mm_cvtpu16_ps(__A);
}

/* Convert the four signed 32-bit values in A and B to SPFP form.  */
static __inline __m128
_mm_cvtpi32x2_ps(__m64 __A, __m64 __B)
{
  __v4sf __zero = (__v4sf) __builtin_ia32_setzerops ();
  __v4sf __sfa = __builtin_ia32_cvtpi2ps (__zero, (__v2si)__A);
  __v4sf __sfb = __builtin_ia32_cvtpi2ps (__zero, (__v2si)__B);
  return (__m128) __builtin_ia32_movlhps (__sfa, __sfb);
}

/* Convert the four SPFP values in A to four signed 16-bit integers.  */
static __inline __m64
_mm_cvtps_pi16(__m128 __A)
{
  __v4sf __hisf = (__v4sf)__A;
  __v4sf __losf = __builtin_ia32_movhlps (__hisf, __hisf);
  __v2si __hisi = __builtin_ia32_cvtps2pi (__hisf);
  __v2si __losi = __builtin_ia32_cvtps2pi (__losf);
  return (__m64) __builtin_ia32_packssdw (__losi, __hisi);
}

/* Convert the four SPFP values in A to four signed 8-bit integers.  */
static __inline __m64
_mm_cvtps_pi8(__m128 __A)
{
  __v4hi __tmp = (__v4hi) _mm_cvtps_pi16 (__A);
  __v4hi __zero = (__v4hi) __builtin_ia32_mmx_zero ();
  return (__m64) __builtin_ia32_packsswb (__tmp, __zero);
}

/* Selects four specific SPFP values from A and B based on MASK.  */
#if 0
static __inline __m128
_mm_shuffle_ps (__m128 __A, __m128 __B, int __mask)
{
  return (__m128) __builtin_ia32_shufps ((__v4sf)__A, (__v4sf)__B, __mask);
}
#else
#define _mm_shuffle_ps(A, B, MASK) \
 ((__m128) __builtin_ia32_shufps ((__v4sf)(A), (__v4sf)(B), (MASK)))
#endif


/* Selects and interleaves the upper two SPFP values from A and B.  */
static __inline __m128
_mm_unpackhi_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_unpckhps ((__v4sf)__A, (__v4sf)__B);
}

/* Selects and interleaves the lower two SPFP values from A and B.  */
static __inline __m128
_mm_unpacklo_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_unpcklps ((__v4sf)__A, (__v4sf)__B);
}

/* Sets the upper two SPFP values with 64-bits of data loaded from P;
   the lower two values are passed through from A.  */
static __inline __m128
_mm_loadh_pi (__m128 __A, __m64 *__P)
{
  return (__m128) __builtin_ia32_loadhps ((__v4sf)__A, (__v2si *)__P);
}

/* Stores the upper two SPFP values of A into P.  */
static __inline void
_mm_storeh_pi (__m64 *__P, __m128 __A)
{
  __builtin_ia32_storehps ((__v2si *)__P, (__v4sf)__A);
}

/* Moves the upper two values of B into the lower two values of A.  */
static __inline __m128
_mm_movehl_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_movhlps ((__v4sf)__A, (__v4sf)__B);
}

/* Moves the lower two values of B into the upper two values of A.  */
static __inline __m128
_mm_movelh_ps (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_movlhps ((__v4sf)__A, (__v4sf)__B);
}

/* Sets the lower two SPFP values with 64-bits of data loaded from P;
   the upper two values are passed through from A.  */
static __inline __m128
_mm_loadl_pi (__m128 __A, __m64 *__P)
{
  return (__m128) __builtin_ia32_loadlps ((__v4sf)__A, (__v2si *)__P);
}

/* Stores the lower two SPFP values of A into P.  */
static __inline void
_mm_storel_pi (__m64 *__P, __m128 __A)
{
  __builtin_ia32_storelps ((__v2si *)__P, (__v4sf)__A);
}

/* Creates a 4-bit mask from the most significant bits of the SPFP values.  */
static __inline int
_mm_movemask_ps (__m128 __A)
{
  return __builtin_ia32_movmskps ((__v4sf)__A);
}

/* Return the contents of the control register.  */
static __inline unsigned int
_mm_getcsr (void)
{
  return __builtin_ia32_stmxcsr ();
}

/* Read exception bits from the control register.  */
static __inline unsigned int
_MM_GET_EXCEPTION_STATE (void)
{
  return _mm_getcsr() & _MM_EXCEPT_MASK;
}

static __inline unsigned int
_MM_GET_EXCEPTION_MASK (void)
{
  return _mm_getcsr() & _MM_MASK_MASK;
}

static __inline unsigned int
_MM_GET_ROUNDING_MODE (void)
{
  return _mm_getcsr() & _MM_ROUND_MASK;
}

static __inline unsigned int
_MM_GET_FLUSH_ZERO_MODE (void)
{
  return _mm_getcsr() & _MM_FLUSH_ZERO_MASK;
}

/* Set the control register to I.  */
static __inline void
_mm_setcsr (unsigned int __I)
{
  __builtin_ia32_ldmxcsr (__I);
}

/* Set exception bits in the control register.  */
static __inline void
_MM_SET_EXCEPTION_STATE(unsigned int __mask)
{
  _mm_setcsr((_mm_getcsr() & ~_MM_EXCEPT_MASK) | __mask);
}

static __inline void
_MM_SET_EXCEPTION_MASK (unsigned int __mask)
{
  _mm_setcsr((_mm_getcsr() & ~_MM_MASK_MASK) | __mask);
}

static __inline void
_MM_SET_ROUNDING_MODE (unsigned int __mode)
{
  _mm_setcsr((_mm_getcsr() & ~_MM_ROUND_MASK) | __mode);
}

static __inline void
_MM_SET_FLUSH_ZERO_MODE (unsigned int __mode)
{
  _mm_setcsr((_mm_getcsr() & ~_MM_FLUSH_ZERO_MASK) | __mode);
}

/* Create a vector with element 0 as *P and the rest zero.  */
static __inline __m128
_mm_load_ss (float *__P)
{
  return (__m128) __builtin_ia32_loadss (__P);
}

/* Create a vector with all four elements equal to *P.  */
static __inline __m128
_mm_load1_ps (float *__P)
{
  __v4sf __tmp = __builtin_ia32_loadss (__P);
  return (__m128) __builtin_ia32_shufps (__tmp, __tmp, _MM_SHUFFLE (0,0,0,0));
}

static __inline __m128
_mm_load_ps1 (float *__P)
{
  return _mm_load1_ps (__P);
}

/* Load four SPFP values from P.  The address must be 16-byte aligned.  */
static __inline __m128
_mm_load_ps (float *__P)
{
  return (__m128) __builtin_ia32_loadaps (__P);
}

/* Load four SPFP values from P.  The address need not be 16-byte aligned.  */
static __inline __m128
_mm_loadu_ps (float *__P)
{
  return (__m128) __builtin_ia32_loadups (__P);
}

/* Load four SPFP values in reverse order.  The address must be aligned.  */
static __inline __m128
_mm_loadr_ps (float *__P)
{
  __v4sf __tmp = __builtin_ia32_loadaps (__P);
  return (__m128) __builtin_ia32_shufps (__tmp, __tmp, _MM_SHUFFLE (0,1,2,3));
}

/* Create a vector with element 0 as F and the rest zero.  */
static __inline __m128
_mm_set_ss (float __F)
{
  return (__m128) __builtin_ia32_loadss (&__F);
}

/* Create a vector with all four elements equal to F.  */
static __inline __m128
_mm_set1_ps (float __F)
{
  __v4sf __tmp = __builtin_ia32_loadss (&__F);
  return (__m128) __builtin_ia32_shufps (__tmp, __tmp, _MM_SHUFFLE (0,0,0,0));
}

static __inline __m128
_mm_set_ps1 (float __F)
{
  return _mm_set1_ps (__F);
}

/* Create the vector [Z Y X W].  */
static __inline __m128
_mm_set_ps (float __Z, float __Y, float __X, float __W)
{
  union {
    float __a[4];
    __m128 __v;
  } __u;

  __u.__a[0] = __W;
  __u.__a[1] = __X;
  __u.__a[2] = __Y;
  __u.__a[3] = __Z;

  return __u.__v;
}

/* Create the vector [W X Y Z].  */
static __inline __m128
_mm_setr_ps (float __Z, float __Y, float __X, float __W)
{
  return _mm_set_ps (__W, __X, __Y, __Z);
}

/* Create a vector of zeros.  */
static __inline __m128
_mm_setzero_ps (void)
{
  return (__m128) __builtin_ia32_setzerops ();
}

/* Stores the lower SPFP value.  */
static __inline void
_mm_store_ss (float *__P, __m128 __A)
{
  __builtin_ia32_storess (__P, (__v4sf)__A);
}

/* Store the lower SPFP value across four words.  */
static __inline void
_mm_store1_ps (float *__P, __m128 __A)
{
  __v4sf __va = (__v4sf)__A;
  __v4sf __tmp = __builtin_ia32_shufps (__va, __va, _MM_SHUFFLE (0,0,0,0));
  __builtin_ia32_storeaps (__P, __tmp);
}

static __inline void
_mm_store_ps1 (float *__P, __m128 __A)
{
  _mm_store1_ps (__P, __A);
}

/* Store four SPFP values.  The address must be 16-byte aligned.  */
static __inline void
_mm_store_ps (float *__P, __m128 __A)
{
  __builtin_ia32_storeaps (__P, (__v4sf)__A);
}

/* Store four SPFP values.  The address need not be 16-byte aligned.  */
static __inline void
_mm_storeu_ps (float *__P, __m128 __A)
{
  __builtin_ia32_storeups (__P, (__v4sf)__A);
}

/* Store four SPFP values in reverse order.  The addres must be aligned.  */
static __inline void
_mm_storer_ps (float *__P, __m128 __A)
{
  __v4sf __va = (__v4sf)__A;
  __v4sf __tmp = __builtin_ia32_shufps (__va, __va, _MM_SHUFFLE (0,1,2,3));
  __builtin_ia32_storeaps (__P, __tmp);
}

/* Sets the low SPFP value of A from the low value of B.  */
static __inline __m128
_mm_move_ss (__m128 __A, __m128 __B)
{
  return (__m128) __builtin_ia32_movss ((__v4sf)__A, (__v4sf)__B);
}

/* Extracts one of the four words of A.  The selector N must be immediate.  */
#if 0
static __inline int
_mm_extract_pi16 (__m64 __A, int __N)
{
  return __builtin_ia32_pextrw ((__v4hi)__A, __N);
}
#else
#define _mm_extract_pi16(A, N) \
  __builtin_ia32_pextrw ((__v4hi)(A), (N))
#endif

/* Inserts word D into one of four words of A.  The selector N must be
   immediate.  */
#if 0
static __inline __m64
_mm_insert_pi16 (__m64 __A, int __D, int __N)
{
  return (__m64)__builtin_ia32_pinsrw ((__v4hi)__A, __D, __N);
}
#else
#define _mm_insert_pi16(A, D, N) \
  ((__m64) __builtin_ia32_pinsrw ((__v4hi)(A), (D), (N)))
#endif

/* Compute the element-wise maximum of signed 16-bit values.  */
static __inline __m64
_mm_max_pi16 (__m64 __A, __m64 __B)
{
  return (__m64) __builtin_ia32_pmaxsw ((__v4hi)__A, (__v4hi)__B);
}

/* Compute the element-wise maximum of unsigned 8-bit values.  */
static __inline __m64
_mm_max_pu8 (__m64 __A, __m64 __B)
{
  return (__m64) __builtin_ia32_pmaxub ((__v8qi)__A, (__v8qi)__B);
}

/* Compute the element-wise minimum of signed 16-bit values.  */
static __inline __m64
_mm_min_pi16 (__m64 __A, __m64 __B)
{
  return (__m64) __builtin_ia32_pminsw ((__v4hi)__A, (__v4hi)__B);
}

/* Compute the element-wise minimum of unsigned 8-bit values.  */
static __inline __m64
_mm_min_pu8 (__m64 __A, __m64 __B)
{
  return (__m64) __builtin_ia32_pminub ((__v8qi)__A, (__v8qi)__B);
}

/* Create an 8-bit mask of the signs of 8-bit values.  */
static __inline int
_mm_movemask_pi8 (__m64 __A)
{
  return __builtin_ia32_pmovmskb ((__v8qi)__A);
}

/* Multiply four unsigned 16-bit values in A by four unsigned 16-bit values
   in B and produce the high 16 bits of the 32-bit results.  */
static __inline __m64
_mm_mulhi_pu16 (__m64 __A, __m64 __B)
{
  return (__m64) __builtin_ia32_pmulhuw ((__v4hi)__A, (__v4hi)__B);
}

/* Return a combination of the four 16-bit values in A.  The selector
   must be an immediate.  */
#if 0
static __inline __m64
_mm_shuffle_pi16 (__m64 __A, int __N)
{
  return (__m64) __builtin_ia32_pshufw ((__v4hi)__A, __N);
}
#else
#define _mm_shuffle_pi16(A, N) \
  ((__m64) __builtin_ia32_pshufw ((__v4hi)(A), (N)))
#endif

/* Conditionally store byte elements of A into P.  The high bit of each
   byte in the selector N determines whether the corresponding byte from
   A is stored.  */
static __inline void
_mm_maskmove_si64 (__m64 __A, __m64 __N, char *__P)
{
  __builtin_ia32_maskmovq ((__v8qi)__A, (__v8qi)__N, __P);
}

/* Compute the rounded averages of the unsigned 8-bit values in A and B.  */
static __inline __m64
_mm_avg_pu8 (__m64 __A, __m64 __B)
{
  return (__m64) __builtin_ia32_pavgb ((__v8qi)__A, (__v8qi)__B);
}

/* Compute the rounded averages of the unsigned 16-bit values in A and B.  */
static __inline __m64
_mm_avg_pu16 (__m64 __A, __m64 __B)
{
  return (__m64) __builtin_ia32_pavgw ((__v4hi)__A, (__v4hi)__B);
}

/* Compute the sum of the absolute differences of the unsigned 8-bit
   values in A and B.  Return the value in the lower 16-bit word; the
   upper words are cleared.  */
static __inline __m64
_mm_sad_pu8 (__m64 __A, __m64 __B)
{
  return (__m64) __builtin_ia32_psadbw ((__v8qi)__A, (__v8qi)__B);
}

/* Loads one cache line from address P to a location "closer" to the
   processor.  The selector I specifies the type of prefetch operation.  */
#if 0
static __inline void
_mm_prefetch (void *__P, enum _mm_hint __I)
{
  __builtin_prefetch (__P, 0, __I);
}
#else
#define _mm_prefetch(P, I) \
  __builtin_prefetch ((P), 0, (I))
#endif

/* Stores the data in A to the address P without polluting the caches.  */
static __inline void
_mm_stream_pi (__m64 *__P, __m64 __A)
{
  __builtin_ia32_movntq (__P, __A);
}

/* Likewise.  The address must be 16-byte aligned.  */
static __inline void
_mm_stream_ps (float *__P, __m128 __A)
{
  __builtin_ia32_movntps (__P, (__v4sf)__A);
}

/* Guarantees that every preceeding store is globally visible before
   any subsequent store.  */
static __inline void
_mm_sfence (void)
{
  __builtin_ia32_sfence ();
}

/* The execution of the next instruction is delayed by an implementation
   specific amount of time.  The instruction does not modify the
   architectural state.  */
static __inline void
_mm_pause (void)
{
  __asm__ __volatile__ ("rep; nop" : : );
}

/* Transpose the 4x4 matrix composed of row[0-3].  */
#define _MM_TRANSPOSE4_PS(row0, row1, row2, row3)			\
do {									\
  __v4sf __r0 = (row0), __r1 = (row1), __r2 = (row2), __r3 = (row3);	\
  __v4sf __t0 = __builtin_ia32_shufps (__r0, __r1, 0x44);		\
  __v4sf __t1 = __builtin_ia32_shufps (__r0, __r1, 0xEE);		\
  __v4sf __t2 = __builtin_ia32_shufps (__r2, __r3, 0x44);		\
  __v4sf __t3 = __builtin_ia32_shufps (__r2, __r3, 0xEE);		\
  (row0) = __builtin_ia32_shufps (__t0, __t1, 0x88);			\
  (row1) = __builtin_ia32_shufps (__t0, __t1, 0xDD);			\
  (row2) = __builtin_ia32_shufps (__t2, __t3, 0x88);			\
  (row3) = __builtin_ia32_shufps (__t2, __t3, 0xDD);			\
} while (0)

#endif /* _XMMINTRIN_H_INCLUDED */
