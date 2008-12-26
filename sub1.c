/* mpfr_sub1 -- internal function to perform a "real" subtraction

Copyright 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 Free Software Foundation, Inc.
Contributed by the Arenaire and Cacao projects, INRIA.

This file is part of the GNU MPFR Library.

The GNU MPFR Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

The GNU MPFR Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details.

You should have received a copy of the GNU Lesser General Public License
along with the GNU MPFR Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
MA 02110-1301, USA. */

#include "mpfr-impl.h"

/* compute sign(b) * (|b| - |c|), with |b| > |c|, diff_exp = EXP(b) - EXP(c)
   Returns 0 iff result is exact,
   a negative value when the result is less than the exact value,
   a positive value otherwise.
*/

int
mpfr_sub1 (mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c, mp_rnd_t rnd_mode)
{
  int sign;
  mp_exp_unsigned_t diff_exp;
  mp_prec_t cancel, cancel1;
  mp_size_t cancel2, an, bn, cn, cn0;
  mp_limb_t *ap, *bp, *cp;
  mp_limb_t carry, bb, cc, borrow = 0;
  int inexact, shift_b, shift_c, is_exact = 1, down = 0, add_exp = 0;
  int sh, k;
  MPFR_TMP_DECL(marker);

  MPFR_TMP_MARK(marker);
  ap = MPFR_MANT(a);
  an = MPFR_LIMB_SIZE(a);

  sign = mpfr_cmp2 (b, c, &cancel);
  if (MPFR_UNLIKELY(sign == 0))
    {
      if (rnd_mode == GMP_RNDD)
        MPFR_SET_NEG (a);
      else
        MPFR_SET_POS (a);
      MPFR_SET_ZERO (a);
      MPFR_RET (0);
    }

  /*
   * If subtraction: sign(a) = sign * sign(b)
   * If addition: sign(a) = sign of the larger argument in absolute value.
   *
   * Both cases can be simplidied in:
   * if (sign>0)
   *    if addition: sign(a) = sign * sign(b) = sign(b)
   *    if subtraction, b is greater, so sign(a) = sign(b)
   * else
   *    if subtraction, sign(a) = - sign(b)
   *    if addition, sign(a) = sign(c) (since c is greater)
   *      But if it is an addition, sign(b) and sign(c) are opposed!
   *      So sign(a) = - sign(b)
   */

  if (sign < 0) /* swap b and c so that |b| > |c| */
    {
      mpfr_srcptr t;
      MPFR_SET_OPPOSITE_SIGN (a,b);
      t = b; b = c; c = t;
    }
  else
    MPFR_SET_SAME_SIGN (a,b);

  /* Check if c is too small.
     A more precise test is to replace 2 by
      (rnd == GMP_RNDN) + mpfr_power2_raw (b)
      but it is more expensive and not very useful */
  if (MPFR_UNLIKELY (MPFR_GET_EXP (c) <= MPFR_GET_EXP (b)
                     - (mp_exp_t) MAX (MPFR_PREC (a), MPFR_PREC (b)) - 2))
    {
      /* Remember, we can't have an exact result! */
      /*   A.AAAAAAAAAAAAAAAAA
         = B.BBBBBBBBBBBBBBB
          -                     C.CCCCCCCCCCCCC */
      /* A = S*ABS(B) +/- ulp(a) */
      MPFR_SET_EXP (a, MPFR_GET_EXP (b));
      MPFR_RNDRAW_EVEN (inexact, a, MPFR_MANT (b), MPFR_PREC (b),
                        rnd_mode, MPFR_SIGN (a),
                        if (MPFR_UNLIKELY ( ++MPFR_EXP (a) > __gmpfr_emax))
                        inexact = mpfr_overflow (a, rnd_mode, MPFR_SIGN (a)));
      /* inexact = mpfr_set4 (a, b, rnd_mode, MPFR_SIGN (a));  */
      if (inexact == 0)
        {
          /* a = b (Exact)
             But we know it isn't (Since we have to remove `c')
             So if we round to Zero, we have to remove one ulp.
             Otherwise the result is correctly rounded. */
          if (MPFR_IS_LIKE_RNDZ (rnd_mode, MPFR_IS_NEG (a)))
            {
              mpfr_nexttozero (a);
              MPFR_RET (- MPFR_INT_SIGN (a));
            }
          MPFR_RET (MPFR_INT_SIGN (a));
        }
      else
        {
          /*   A.AAAAAAAAAAAAAA
             = B.BBBBBBBBBBBBBBB
              -                   C.CCCCCCCCCCCCC */
          /* It isn't exact so Prec(b) > Prec(a) and the last
             Prec(b)-Prec(a) bits of `b' are not zeros.
             Which means that removing c from b can't generate a carry
             execpt in case of even rounding.
             In all other case the result and the inexact flag should be
             correct (We can't have an exact result).
             In case of EVEN rounding:
               1.BBBBBBBBBBBBBx10
             -                     1.CCCCCCCCCCCC
             = 1.BBBBBBBBBBBBBx01  Rounded to Prec(b)
             = 1.BBBBBBBBBBBBBx    Nearest / Rounded to Prec(a)
             Set gives:
               1.BBBBBBBBBBBBB0   if inexact == EVEN_INEX  (x == 0)
               1.BBBBBBBBBBBBB1+1 if inexact == -EVEN_INEX (x == 1)
             which means we get a wrong rounded result if x==1,
             i.e. inexact= MPFR_EVEN_INEX */
          if (MPFR_UNLIKELY (inexact == MPFR_EVEN_INEX*MPFR_INT_SIGN (a)))
            {
              mpfr_nexttozero (a);
              inexact = -MPFR_INT_SIGN (a);
            }
          MPFR_RET (inexact);
        }
    }

  diff_exp = (mp_exp_unsigned_t) MPFR_GET_EXP (b) - MPFR_GET_EXP (c);

  /* reserve a space to store b aligned with the result, i.e. shifted by
     (-cancel) % BITS_PER_MP_LIMB to the right */
  bn      = MPFR_LIMB_SIZE (b);
  MPFR_UNSIGNED_MINUS_MODULO (shift_b, cancel);
  cancel1 = (cancel + shift_b) / BITS_PER_MP_LIMB;

  /* the high cancel1 limbs from b should not be taken into account */
  if (MPFR_UNLIKELY (shift_b == 0))
    {
      bp = MPFR_MANT(b); /* no need of an extra space */
      /* Ensure ap != bp */
      if (MPFR_UNLIKELY (ap == bp))
        {
          bp = (mp_ptr) MPFR_TMP_ALLOC(bn * BYTES_PER_MP_LIMB);
          MPN_COPY (bp, ap, bn);
        }
    }
  else
    {
      bp = (mp_ptr) MPFR_TMP_ALLOC ((bn + 1) * BYTES_PER_MP_LIMB);
      bp[0] = mpn_rshift (bp + 1, MPFR_MANT(b), bn++, shift_b);
    }

  /* reserve a space to store c aligned with the result, i.e. shifted by
      (diff_exp-cancel) % BITS_PER_MP_LIMB to the right */
  cn      = MPFR_LIMB_SIZE(c);
  if ((UINT_MAX % BITS_PER_MP_LIMB) == (BITS_PER_MP_LIMB-1)
      && ((-(unsigned) 1)%BITS_PER_MP_LIMB > 0))
    shift_c = (diff_exp - cancel) % BITS_PER_MP_LIMB;
  else
    {
      shift_c = diff_exp - (cancel % BITS_PER_MP_LIMB);
      shift_c = (shift_c + BITS_PER_MP_LIMB) % BITS_PER_MP_LIMB;
    }
  MPFR_ASSERTD( shift_c >= 0 && shift_c < BITS_PER_MP_LIMB);

  if (MPFR_UNLIKELY(shift_c == 0))
    {
       cp = MPFR_MANT(c);
      /* Ensure ap != cp */
      if (ap == cp)
        {
          cp = (mp_ptr) MPFR_TMP_ALLOC (cn * BYTES_PER_MP_LIMB);
          MPN_COPY(cp, ap, cn);
        }
    }
 else
    {
      cp = (mp_ptr) MPFR_TMP_ALLOC ((cn + 1) * BYTES_PER_MP_LIMB);
      cp[0] = mpn_rshift (cp + 1, MPFR_MANT(c), cn++, shift_c);
    }

#ifdef DEBUG
  printf ("shift_b=%d shift_c=%d diffexp=%lu\n", shift_b, shift_c,
          (unsigned long) diff_exp);
#endif

  MPFR_ASSERTD (ap != cp);
  MPFR_ASSERTD (bp != cp);

  /* here we have shift_c = (diff_exp - cancel) % BITS_PER_MP_LIMB,
        0 <= shift_c < BITS_PER_MP_LIMB
     thus we want cancel2 = ceil((cancel - diff_exp) / BITS_PER_MP_LIMB) */

  cancel2 = (long int) (cancel - (diff_exp - shift_c)) / BITS_PER_MP_LIMB;
  /* the high cancel2 limbs from b should not be taken into account */
#ifdef DEBUG
  printf ("cancel=%lu cancel1=%lu cancel2=%ld\n",
          (unsigned long) cancel, (unsigned long) cancel1, (long) cancel2);
#endif

  /*               ap[an-1]        ap[0]
             <----------------+-----------|---->
             <----------PREC(a)----------><-sh->
 cancel1
 limbs        bp[bn-cancel1-1]
 <--...-----><----------------+-----------+----------->
  cancel2
  limbs       cp[cn-cancel2-1]                                    cancel2 >= 0
    <--...--><----------------+----------------+---------------->
                (-cancel2)                                        cancel2 < 0
                   limbs      <----------------+---------------->
  */

  /* first part: put in ap[0..an-1] the value of high(b) - high(c),
     where high(b) consists of the high an+cancel1 limbs of b,
     and high(c) consists of the high an+cancel2 limbs of c.
   */

  /* copy high(b) into a */
  if (MPFR_LIKELY(an + (mp_size_t) cancel1 <= bn))
    /* a: <----------------+-----------|---->
       b: <-----------------------------------------> */
      MPN_COPY (ap, bp + bn - (an + cancel1), an);
  else
    /* a: <----------------+-----------|---->
       b: <-------------------------> */
    if ((mp_size_t) cancel1 < bn) /* otherwise b does not overlap with a */
      {
        MPN_ZERO (ap, an + cancel1 - bn);
        MPN_COPY (ap + an + cancel1 - bn, bp, bn - cancel1);
      }
    else
      MPN_ZERO (ap, an);

#ifdef DEBUG
  printf("after copying high(b), a="); mpfr_print_binary(a); putchar('\n');
#endif

  /* subtract high(c) */
  if (MPFR_LIKELY(an + cancel2 > 0)) /* otherwise c does not overlap with a */
    {
      mp_limb_t *ap2;

      if (cancel2 >= 0)
        {
          if (an + cancel2 <= cn)
            /* a: <----------------------------->
               c: <-----------------------------------------> */
            mpn_sub_n (ap, ap, cp + cn - (an + cancel2), an);
          else
            /* a: <---------------------------->
               c: <-------------------------> */
            {
              ap2 = ap + an + cancel2 - cn;
              if (cn > cancel2)
                mpn_sub_n (ap2, ap2, cp, cn - cancel2);
            }
        }
      else /* cancel2 < 0 */
        {
          if (an + cancel2 <= cn)
            /* a: <----------------------------->
               c: <-----------------------------> */
            borrow = mpn_sub_n (ap, ap, cp + cn - (an + cancel2),
                                an + cancel2);
          else
            /* a: <---------------------------->
               c: <----------------> */
            {
              ap2 = ap + an + cancel2 - cn;
              borrow = mpn_sub_n (ap2, ap2, cp, cn);
            }
          ap2 = ap + an + cancel2;
          mpn_sub_1 (ap2, ap2, -cancel2, borrow);
        }
    }

#ifdef DEBUG
  printf("after subtracting high(c), a=");
  mpfr_print_binary(a);
  putchar('\n');
#endif

  /* now perform rounding */
  sh = (mp_prec_t) an * BITS_PER_MP_LIMB - MPFR_PREC(a);
  /* last unused bits from a */
  carry = ap[0] & MPFR_LIMB_MASK (sh);
  ap[0] -= carry;

  if (MPFR_LIKELY(rnd_mode == GMP_RNDN))
    {
      if (MPFR_LIKELY(sh))
        {
          is_exact = (carry == 0);
          /* can decide except when carry = 2^(sh-1) [middle]
             or carry = 0 [truncate, but cannot decide inexact flag] */
          down = (carry < (MPFR_LIMB_ONE << (sh - 1)));
          if (carry > (MPFR_LIMB_ONE << (sh - 1)))
            goto add_one_ulp;
          else if ((0 < carry) && down)
            {
              inexact = -1; /* result if smaller than exact value */
              goto truncate;
            }
        }
    }
  else /* directed rounding: set rnd_mode to RNDZ iff towards zero */
    {
      if (MPFR_IS_RNDUTEST_OR_RNDDNOTTEST(rnd_mode, MPFR_IS_NEG(a)))
        rnd_mode = GMP_RNDZ;

      if (carry)
        {
          if (rnd_mode == GMP_RNDZ)
            {
              inexact = -1;
              goto truncate;
            }
          else /* round away */
            goto add_one_ulp;
        }
    }

  /* we have to consider the low (bn - (an+cancel1)) limbs from b,
     and the (cn - (an+cancel2)) limbs from c. */
  bn -= an + cancel1;
  cn0 = cn;
  cn -= (long int) an + cancel2;

#ifdef DEBUG
  printf ("last %d bits from a are %lu, bn=%ld, cn=%ld\n",
          sh, (unsigned long) carry, (long) bn, (long) cn);
#endif

  for (k = 0; (bn > 0) || (cn > 0); k = 1)
    {
      /* get next limbs */
      bb = (bn > 0) ? bp[--bn] : 0;
      if ((cn > 0) && (cn-- <= cn0))
        cc = cp[cn];
      else
        cc = 0;

      /* down is set when low(b) < low(c) */
      if (down == 0)
        down = (bb < cc);

      /* the case rounding to nearest with sh=0 is special since one couldn't
         subtract above 1/2 ulp in the trailing limb of the result */
      if ((rnd_mode == GMP_RNDN) && sh == 0 && k == 0)
        {
          mp_limb_t half = MPFR_LIMB_HIGHBIT;

          is_exact = (bb == cc);

          /* add one ulp if bb > cc + half
             truncate if cc - half < bb < cc + half
             sub one ulp if bb < cc - half
          */

          if (down)
            {
              if (cc >= half)
                cc -= half;
              else
                bb += half;
            }
          else /* bb >= cc */
            {
              if (cc < half)
                cc += half;
              else
                bb -= half;
            }
        }

#ifdef DEBUG
      printf ("    bb=%lu cc=%lu down=%d is_exact=%d\n",
              (unsigned long) bb, (unsigned long) cc, down, is_exact);
#endif
      if (bb < cc)
        {
          if (rnd_mode == GMP_RNDZ)
            goto sub_one_ulp;
          else if (rnd_mode != GMP_RNDN) /* round away */
            {
              inexact = 1;
              goto truncate;
            }
          else /* round to nearest: special case here since for sh=k=0
                  bb = bb0 - MPFR_LIMB_HIGHBIT */
            {
              if (is_exact && sh == 0)
                {
                  /* For k=0 we can't decide exactness since it may depend
                     from low order bits.
                     For k=1, the first low limbs matched: low(b)-low(c)<0. */
                  if (k)
                    {
                      inexact = 1;
                      goto truncate;
                    }
                }
              else if (down && sh == 0)
                goto sub_one_ulp;
              else
                {
                  inexact = (is_exact) ? 1 : -1;
                  goto truncate;
                }
            }
        }
      else if (bb > cc)
        {
          if (rnd_mode == GMP_RNDZ)
            {
              inexact = -1;
              goto truncate;
            }
          else if (rnd_mode != GMP_RNDN) /* round away */
            goto add_one_ulp;
          else /* round to nearest */
            {
              if (is_exact)
                {
                  inexact = -1;
                  goto truncate;
                }
              else if (down)
                {
                  inexact = 1;
                  goto truncate;
                }
              else
                goto add_one_ulp;
            }
        }
    }

  if ((rnd_mode == GMP_RNDN) && !is_exact)
    {
      /* even rounding rule */
      if ((ap[0] >> sh) & 1)
        {
          if (down)
            goto sub_one_ulp;
          else
            goto add_one_ulp;
        }
      else
        inexact = (down) ? 1 : -1;
    }
  else
    inexact = 0;
  goto truncate;

 sub_one_ulp: /* sub one unit in last place to a */
  mpn_sub_1 (ap, ap, an, MPFR_LIMB_ONE << sh);
  inexact = -1;
  goto end_of_sub;

 add_one_ulp: /* add one unit in last place to a */
  if (MPFR_UNLIKELY(mpn_add_1 (ap, ap, an, MPFR_LIMB_ONE << sh)))
    /* result is a power of 2: 11111111111111 + 1 = 1000000000000000 */
    {
      ap[an-1] = MPFR_LIMB_HIGHBIT;
      add_exp = 1;
    }
  inexact = 1; /* result larger than exact value */

 truncate:
  if (MPFR_UNLIKELY((ap[an-1] >> (BITS_PER_MP_LIMB - 1)) == 0))
    /* case 1 - epsilon */
    {
      ap[an-1] = MPFR_LIMB_HIGHBIT;
      add_exp = 1;
    }

 end_of_sub:
  /* we have to set MPFR_EXP(a) to MPFR_EXP(b) - cancel + add_exp, taking
     care of underflows/overflows in that computation, and of the allowed
     exponent range */
  if (MPFR_LIKELY(cancel))
    {
      mp_exp_t exp_a;

      cancel -= add_exp; /* still valid as unsigned long */
      exp_a = MPFR_GET_EXP (b) - cancel;
      if (MPFR_UNLIKELY(exp_a < __gmpfr_emin))
        {
          MPFR_TMP_FREE(marker);
          if (rnd_mode == GMP_RNDN &&
              (exp_a < __gmpfr_emin - 1 ||
               (inexact >= 0 && mpfr_powerof2_raw (a))))
            rnd_mode = GMP_RNDZ;
          return mpfr_underflow (a, rnd_mode, MPFR_SIGN(a));
        }
      MPFR_SET_EXP (a, exp_a);
    }
  else /* cancel = 0: MPFR_EXP(a) <- MPFR_EXP(b) + add_exp */
    {
      /* in case cancel = 0, add_exp can still be 1, in case b is just
         below a power of two, c is very small, prec(a) < prec(b),
         and rnd=away or nearest */
      mp_exp_t exp_b;

      exp_b = MPFR_GET_EXP (b);
      if (MPFR_UNLIKELY(add_exp && exp_b == __gmpfr_emax))
        {
          MPFR_TMP_FREE(marker);
          return mpfr_overflow (a, rnd_mode, MPFR_SIGN(a));
        }
      MPFR_SET_EXP (a, exp_b + add_exp);
    }
  MPFR_TMP_FREE(marker);
#ifdef DEBUG
  printf ("result is a="); mpfr_print_binary(a); putchar('\n');
#endif
  /* check that result is msb-normalized */
  MPFR_ASSERTD(ap[an-1] > ~ap[an-1]);
  MPFR_RET (inexact * MPFR_INT_SIGN (a));
}
