/* serpent-encrypt.c
 *
 * The serpent block cipher.
 *
 * For more details on this algorithm, see the Serpent website at
 * http://www.cl.cam.ac.uk/~rja14/serpent.html
 */

/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2011  Niels Möller
 * Copyright (C) 2010, 2011  Simon Josefsson
 * Copyright (C) 2003, 2004, 2005 Free Software Foundation, Inc.
 *  
 * The nettle library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 * 
 * The nettle library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with the nettle library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */

/* This file is derived from cipher/serpent.c in Libgcrypt v1.4.6.
   The adaption to Nettle was made by Simon Josefsson on 2010-12-07
   with final touches on 2011-05-30.  Changes include replacing
   libgcrypt with nettle in the license template, renaming
   serpent_context to serpent_ctx, renaming u32 to uint32_t, removing
   libgcrypt stubs and selftests, modifying entry function prototypes,
   using FOR_BLOCKS to iterate through data in encrypt/decrypt, using
   LE_READ_UINT32 and LE_WRITE_UINT32 to access data in
   encrypt/decrypt, and running indent on the code. */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <limits.h>

#include "serpent.h"

#include "macros.h"
#include "serpent-internal.h"

/* These are the S-Boxes of Serpent.  They are copied from Serpents
   reference implementation (the optimized one, contained in
   `floppy2') and are therefore:

     Copyright (C) 1998 Ross Anderson, Eli Biham, Lars Knudsen.

  To quote the Serpent homepage
  (http://www.cl.cam.ac.uk/~rja14/serpent.html):

  "Serpent is now completely in the public domain, and we impose no
   restrictions on its use.  This was announced on the 21st August at
   the First AES Candidate Conference. The optimised implementations
   in the submission package are now under the GNU PUBLIC LICENSE
   (GPL), although some comments in the code still say otherwise. You
   are welcome to use Serpent for any application."  */

/* FIXME: Except when used within the key schedule, the inputs are not
   used after the substitution, and hence we could allow them to be
   destroyed. Can this freedom be used to optimize the sboxes? */

/* S0:  3  8 15  1 10  6  5 11 14 13  4  2  7  0  9 12 */
#define SBOX0(type, a, b, c, d, w, x, y, z)	\
  do { \
    type t02, t03, t05, t06, t07, t08, t09; \
    type t11, t12, t13, t14, t15, t17, t01; \
    t01 = b   ^ c  ; \
    t02 = a   | d  ; \
    t03 = a   ^ b  ; \
    z   = t02 ^ t01; \
    t05 = c   | z  ; \
    t06 = a   ^ d  ; \
    t07 = b   | c  ; \
    t08 = d   & t05; \
    t09 = t03 & t07; \
    y   = t09 ^ t08; \
    t11 = t09 & y  ; \
    t12 = c   ^ d  ; \
    t13 = t07 ^ t11; \
    t14 = b   & t06; \
    t15 = t06 ^ t13; \
    w   =     ~ t15; \
    t17 = w   ^ t14; \
    x   = t12 ^ t17; \
  } while (0)

/* S1: 15 12  2  7  9  0  5 10  1 11 14  8  6 13  3  4 */
#define SBOX1(type, a, b, c, d, w, x, y, z)	\
  do { \
    type t02, t03, t04, t05, t06, t07, t08; \
    type t10, t11, t12, t13, t16, t17, t01; \
    t01 = a   | d  ; \
    t02 = c   ^ d  ; \
    t03 =     ~ b  ; \
    t04 = a   ^ c  ; \
    t05 = a   | t03; \
    t06 = d   & t04; \
    t07 = t01 & t02; \
    t08 = b   | t06; \
    y   = t02 ^ t05; \
    t10 = t07 ^ t08; \
    t11 = t01 ^ t10; \
    t12 = y   ^ t11; \
    t13 = b   & d  ; \
    z   =     ~ t10; \
    x   = t13 ^ t12; \
    t16 = t10 | x  ; \
    t17 = t05 & t16; \
    w   = c   ^ t17; \
  } while (0)

/* S2:  8  6  7  9  3 12 10 15 13  1 14  4  0 11  5  2 */
#define SBOX2(type, a, b, c, d, w, x, y, z) \
  do {					   \
    type t02, t03, t05, t06, t07, t08; \
    type t09, t10, t12, t13, t14, t01; \
    t01 = a   | c  ; \
    t02 = a   ^ b  ; \
    t03 = d   ^ t01; \
    w   = t02 ^ t03; \
    t05 = c   ^ w  ; \
    t06 = b   ^ t05; \
    t07 = b   | t05; \
    t08 = t01 & t06; \
    t09 = t03 ^ t07; \
    t10 = t02 | t09; \
    x   = t10 ^ t08; \
    t12 = a   | d  ; \
    t13 = t09 ^ x  ; \
    t14 = b   ^ t13; \
    z   =     ~ t09; \
    y   = t12 ^ t14; \
  } while (0)

/* S3:  0 15 11  8 12  9  6  3 13  1  2  4 10  7  5 14 */
#define SBOX3(type, a, b, c, d, w, x, y, z) \
  do {						\
    type t02, t03, t04, t05, t06, t07, t08; \
    type t09, t10, t11, t13, t14, t15, t01; \
    t01 = a   ^ c  ; \
    t02 = a   | d  ; \
    t03 = a   & d  ; \
    t04 = t01 & t02; \
    t05 = b   | t03; \
    t06 = a   & b  ; \
    t07 = d   ^ t04; \
    t08 = c   | t06; \
    t09 = b   ^ t07; \
    t10 = d   & t05; \
    t11 = t02 ^ t10; \
    z   = t08 ^ t09; \
    t13 = d   | z  ; \
    t14 = a   | t07; \
    t15 = b   & t13; \
    y   = t08 ^ t11; \
    w   = t14 ^ t15; \
    x   = t05 ^ t04; \
  } while (0)

/* S4:  1 15  8  3 12  0 11  6  2  5  4 10  9 14  7 13 */
#define SBOX4(type, a, b, c, d, w, x, y, z) \
  do { \
    type t02, t03, t04, t05, t06, t08, t09; \
    type t10, t11, t12, t13, t14, t15, t16, t01; \
    t01 = a   | b  ; \
    t02 = b   | c  ; \
    t03 = a   ^ t02; \
    t04 = b   ^ d  ; \
    t05 = d   | t03; \
    t06 = d   & t01; \
    z   = t03 ^ t06; \
    t08 = z   & t04; \
    t09 = t04 & t05; \
    t10 = c   ^ t06; \
    t11 = b   & c  ; \
    t12 = t04 ^ t08; \
    t13 = t11 | t03; \
    t14 = t10 ^ t09; \
    t15 = a   & t05; \
    t16 = t11 | t12; \
    y   = t13 ^ t08; \
    x   = t15 ^ t16; \
    w   =     ~ t14; \
  } while (0)

/* S5: 15  5  2 11  4 10  9 12  0  3 14  8 13  6  7  1 */
#define SBOX5(type, a, b, c, d, w, x, y, z)	\
  do { \
    type t02, t03, t04, t05, t07, t08, t09; \
    type t10, t11, t12, t13, t14, t01; \
    t01 = b   ^ d  ; \
    t02 = b   | d  ; \
    t03 = a   & t01; \
    t04 = c   ^ t02; \
    t05 = t03 ^ t04; \
    w   =     ~ t05; \
    t07 = a   ^ t01; \
    t08 = d   | w  ; \
    t09 = b   | t05; \
    t10 = d   ^ t08; \
    t11 = b   | t07; \
    t12 = t03 | w  ; \
    t13 = t07 | t10; \
    t14 = t01 ^ t11; \
    y   = t09 ^ t13; \
    x   = t07 ^ t08; \
    z   = t12 ^ t14; \
  } while (0)

/* S6:  7  2 12  5  8  4  6 11 14  9  1 15 13  3 10  0 */
#define SBOX6(type, a, b, c, d, w, x, y, z) \
  do { \
    type t02, t03, t04, t05, t07, t08, t09, t10;	\
    type t11, t12, t13, t15, t17, t18, t01; \
    t01 = a   & d  ; \
    t02 = b   ^ c  ; \
    t03 = a   ^ d  ; \
    t04 = t01 ^ t02; \
    t05 = b   | c  ; \
    x   =     ~ t04; \
    t07 = t03 & t05; \
    t08 = b   & x  ; \
    t09 = a   | c  ; \
    t10 = t07 ^ t08; \
    t11 = b   | d  ; \
    t12 = c   ^ t11; \
    t13 = t09 ^ t10; \
    y   =     ~ t13; \
    t15 = x   & t03; \
    z   = t12 ^ t07; \
    t17 = a   ^ b  ; \
    t18 = y   ^ t15; \
    w   = t17 ^ t18; \
  } while (0)

/* S7:  1 13 15  0 14  8  2 11  7  4 12 10  9  3  5  6 */
#define SBOX7(type, a, b, c, d, w, x, y, z) \
  do { \
    type t02, t03, t04, t05, t06, t08, t09, t10;	\
    type t11, t13, t14, t15, t16, t17, t01; \
    t01 = a   & c  ; \
    t02 =     ~ d  ; \
    t03 = a   & t02; \
    t04 = b   | t01; \
    t05 = a   & b  ; \
    t06 = c   ^ t04; \
    z   = t03 ^ t06; \
    t08 = c   | z  ; \
    t09 = d   | t05; \
    t10 = a   ^ t08; \
    t11 = t04 & z  ; \
    x   = t09 ^ t10; \
    t13 = b   ^ x  ; \
    t14 = t01 ^ x  ; \
    t15 = c   ^ t05; \
    t16 = t11 | t13; \
    t17 = t02 | t14; \
    w   = t15 ^ t17; \
    y   = a   ^ t16; \
  } while (0)

/* In-place linear transformation.  */
#define LINEAR_TRANSFORMATION(x0,x1,x2,x3)		 \
  do {                                                   \
    x0 = ROL32 (x0, 13);                    \
    x2 = ROL32 (x2, 3);                     \
    x1 = x1 ^ x0 ^ x2;        \
    x3 = x3 ^ x2 ^ (x0 << 3); \
    x1 = ROL32 (x1, 1);                     \
    x3 = ROL32 (x3, 7);                     \
    x0 = x0 ^ x1 ^ x3;        \
    x2 = x2 ^ x3 ^ (x1 << 7); \
    x0 = ROL32 (x0, 5);                     \
    x2 = ROL32 (x2, 22);                    \
  } while (0)

/* Round inputs are x0,x1,x2,x3 (destroyed), and round outputs are
   y0,y1,y2,y3. */
#define ROUND(which, subkey, x0,x1,x2,x3, y0,y1,y2,y3) \
  do {						       \
    KEYXOR(x0,x1,x2,x3, subkey);		       \
    SBOX##which(uint32_t, x0,x1,x2,x3, y0,y1,y2,y3);	       \
    LINEAR_TRANSFORMATION(y0,y1,y2,y3);		       \
  } while (0)

#if HAVE_NATIVE_64_BIT

#define LINEAR_TRANSFORMATION64(x0,x1,x2,x3)		 \
  do {                                                   \
    x0 = ROL64 (x0, 13);                    \
    x2 = ROL64 (x2, 3);                     \
    x1 = x1 ^ x0 ^ x2;        \
    x3 = x3 ^ x2 ^ RSHIFT64(x0, 3);	    \
    x1 = ROL64 (x1, 1);                     \
    x3 = ROL64 (x3, 7);                     \
    x0 = x0 ^ x1 ^ x3;        \
    x2 = x2 ^ x3 ^ RSHIFT64(x1, 7);	    \
    x0 = ROL64 (x0, 5);                     \
    x2 = ROL64 (x2, 22);                    \
  } while (0)

#define ROUND64(which, subkey, x0,x1,x2,x3, y0,y1,y2,y3) \
  do {						       \
    KEYXOR64(x0,x1,x2,x3, subkey);		       \
    SBOX##which(uint64_t, x0,x1,x2,x3, y0,y1,y2,y3);	       \
    LINEAR_TRANSFORMATION64(y0,y1,y2,y3);		       \
  } while (0)

#endif /* HAVE_NATIVE_64_BIT */

void
serpent_encrypt (const struct serpent_ctx *ctx,
		 unsigned length, uint8_t * dst, const uint8_t * src)
{
  assert( !(length % SERPENT_BLOCK_SIZE));
  
#if HAVE_NATIVE_64_BIT
  if (length & SERPENT_BLOCK_SIZE)
#else
  while (length >= SERPENT_BLOCK_SIZE)
#endif
    {
      uint32_t x0,x1,x2,x3, y0,y1,y2,y3;
      unsigned k;

      x0 = LE_READ_UINT32 (src);
      x1 = LE_READ_UINT32 (src + 4);
      x2 = LE_READ_UINT32 (src + 8);
      x3 = LE_READ_UINT32 (src + 12);

      for (k = 0; ; k += 8)
	{
	  ROUND (0, ctx->keys[k+0], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND (1, ctx->keys[k+1], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND (2, ctx->keys[k+2], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND (3, ctx->keys[k+3], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND (4, ctx->keys[k+4], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND (5, ctx->keys[k+5], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND (6, ctx->keys[k+6], x0,x1,x2,x3, y0,y1,y2,y3);
	  if (k == 24)
	    break;
	  ROUND (7, ctx->keys[k+7], y0,y1,y2,y3, x0,x1,x2,x3);
	}

      /* Special final round, using two subkeys. */
      KEYXOR (y0,y1,y2,y3, ctx->keys[31]);
      SBOX7 (uint32_t, y0,y1,y2,y3, x0,x1,x2,x3);
      KEYXOR (x0,x1,x2,x3, ctx->keys[32]);
    
      LE_WRITE_UINT32 (dst, x0);
      LE_WRITE_UINT32 (dst + 4, x1);
      LE_WRITE_UINT32 (dst + 8, x2);
      LE_WRITE_UINT32 (dst + 12, x3);

      src += SERPENT_BLOCK_SIZE;
      dst += SERPENT_BLOCK_SIZE;
      length -= SERPENT_BLOCK_SIZE;
    }
#if HAVE_NATIVE_64_BIT
  FOR_BLOCKS(length, dst, src, 2*SERPENT_BLOCK_SIZE)
    {
      uint64_t x0,x1,x2,x3, y0,y1,y2,y3;
      unsigned k;

      x0 = LE_READ_UINT32 (src);
      x1 = LE_READ_UINT32 (src + 4);
      x2 = LE_READ_UINT32 (src + 8);
      x3 = LE_READ_UINT32 (src + 12);

      x0 <<= 32; x0 |= LE_READ_UINT32 (src + 16);
      x1 <<= 32; x1 |= LE_READ_UINT32 (src + 20);
      x2 <<= 32; x2 |= LE_READ_UINT32 (src + 24);
      x3 <<= 32; x3 |= LE_READ_UINT32 (src + 28);

      for (k = 0; ; k += 8)
	{
	  ROUND64 (0, ctx->keys[k+0], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND64 (1, ctx->keys[k+1], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND64 (2, ctx->keys[k+2], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND64 (3, ctx->keys[k+3], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND64 (4, ctx->keys[k+4], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND64 (5, ctx->keys[k+5], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND64 (6, ctx->keys[k+6], x0,x1,x2,x3, y0,y1,y2,y3);
	  if (k == 24)
	    break;
	  ROUND64 (7, ctx->keys[k+7], y0,y1,y2,y3, x0,x1,x2,x3);
	}

      /* Special final round, using two subkeys. */
      KEYXOR64 (y0,y1,y2,y3, ctx->keys[31]);
      SBOX7 (uint64_t, y0,y1,y2,y3, x0,x1,x2,x3);
      KEYXOR64 (x0,x1,x2,x3, ctx->keys[32]);
    
      LE_WRITE_UINT32 (dst + 16, x0);
      LE_WRITE_UINT32 (dst + 20, x1);
      LE_WRITE_UINT32 (dst + 24, x2);
      LE_WRITE_UINT32 (dst + 28, x3);
      x0 >>= 32; LE_WRITE_UINT32 (dst, x0);
      x1 >>= 32; LE_WRITE_UINT32 (dst + 4, x1);
      x2 >>= 32; LE_WRITE_UINT32 (dst + 8, x2);
      x3 >>= 32; LE_WRITE_UINT32 (dst + 12, x3);
    }
#endif /* HAVE_NATIVE_64_BIT */
}
