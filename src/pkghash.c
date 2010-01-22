/*
 * pkghash.c
 *
 * $Id: pkghash.c,v 1.1 2010/01/22 17:11:48 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, MinGW Project
 *
 *
 * Hashing functions, used to generate CRC hashes from path names,
 * and to derive signature file names from hashed path names.
 *
 *
 * This is free software.  Permission is granted to copy, modify and
 * redistribute this software, under the provisions of the GNU General
 * Public License, Version 3, (or, at your option, any later version),
 * as published by the Free Software Foundation; see the file COPYING
 * for licensing details.
 *
 * Note, in particular, that this software is provided "as is", in the
 * hope that it may prove useful, but WITHOUT WARRANTY OF ANY KIND; not
 * even an implied WARRANTY OF MERCHANTABILITY, nor of FITNESS FOR ANY
 * PARTICULAR PURPOSE.  Under no circumstances will the author, or the
 * MinGW Project, accept liability for any damages, however caused,
 * arising from the use of this software.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( __MS_DOS__ ) || defined( _WIN32 ) || defined( __CYGWIN__ )
# include <ctype.h>
/*
 * When generating path name hashes for Microsoft file systems, we want
 * the same result irrespective of case distinctions in the input name,
 * or of choice of `/' or `\' as the directory name separator; thus we
 * use this input mapping function which passes each input character to
 * the hashing algorithm as its lower case equivalent, and `\' as if it
 * had been specified as `/'.
 */
static __inline__ __attribute__((__always_inline__))
unsigned long input_as_ulong( const char value )
{
  if( value == '\\' )
    return (unsigned long)('/');

  return (unsigned long)(tolower(value));
}
#else
/*
 * Alternatively, for non-Microsoft file systems, we simply cast each
 * input character to its literal unsigned long equivalent value.
 */
# define input_as_ulong( value )  (unsigned long)(value)
#endif

static __inline__ unsigned long generic_crc_update
( unsigned bits, unsigned long poly, unsigned long input, unsigned long hash )
{
  /* Helper function; it incorporates the effect of the next input byte
   * into the hash, as already computed from all preceding input bytes,
   * using the specified generator polynomial and bit length.
   */
  int bitcount = 8;				/* bits in input byte */
  unsigned long mask = 1UL << (bits - 1);	/* most significant bit */

  /* The input byte is passed as the least significant 8-bits of the
   * associated unsigned long argument; we begin by aligning its most
   * significant bit with the most significant bit of the hash...
   */
  input <<= bits - bitcount;
  while( bitcount-- > 0 )
  {
    /* ...then for all eight input bits, from most significant to
     * least significant...
     */
    if( (hash ^ input) & mask )
      /*
       * ...a 'one' bit here indicates that appending the current
       * input bit to the current interim CRC residual makes that
       * residual modulo-2 divisible by the generator polynomial,
       * so align and modulo-2 subtract (i.e. XOR) it.
       */
      hash = (hash << 1) ^ poly;

    else
      /* ...otherwise, we simply shift out the most significant bit
       * of the original hash value, (effectively appending a 'zero'
       * bit to the CRC quotient, and adjusting the residual hash),
       * in preparation for processing the next input bit...
       */
      hash <<= 1;

    /* ...and, in either case, progress to the next input bit.
     */
    input <<= 1;
  }

  /* Ultimately, we return the new value of the hash...
   * Note that we don't discard superfluous carry bits here;
   * the caller *must* mask the return value, to extract the
   * appropriate number of least significant bits from the
   * returned value, to obtain the specified CRC hash.
   */
  return hash;
}

unsigned long generic_crc
( unsigned bits, unsigned long poly, const char *input, size_t length )
{
  /* Compute a CRC hash of specified bit length, using the specified
   * generator polynomial, for the given input byte stream buffer of
   * specified length.
   */
  unsigned long hash = 0UL;	/* Initial value */
  while( length-- > 0 )
    /*
     * Update the hash value, processing each byte of the input buffer
     * in turn, until all specified bytes have been processed.
     */
    hash = generic_crc_update( bits, poly, input_as_ulong( *input++ ), hash );

  /* Note that, for CRCs of fewer than the number of bits in an unsigned
   * long, the accumulated hash may include unwanted noise, (carried out),
   * in the more significant bits.  The required hash value is to be found
   * in the specified number of less significant bits; mask out the noise,
   * and return the required hash value.
   */
  return hash & ((1UL << bits) - 1L);
}

char *hashed_name( int retry, const char *tag, const char *refname )
{
  /* Generate a hashed name, comprising the specified 'tag' prefix,
   * followed by the collision retry index, the length and a pair of 
   * distinct CRC hashes, which is representative of the specified
   * 'refname' string.  The result is returned in a buffer allocated
   * dynamically on the heap; this should be freed by the caller,
   * when no longer required.
   */
  char *retname;
  size_t len = strlen( tag );

  /* While hash collision may be improbable, it is not impossible;
   * to mitigate, we provide a collection of generator polynomials,
   * selected in pairs indexed by the 'retry' parameter, offering
   * eight hash possibilities for each input 'refname'.
   */
  static unsigned long p16[] =	/* 16-bit CRC generator polynomials */
  {
    0x1021,				/* CCITT standard */
    0x8408,				/* CCITT reversed */
    0x8005,				/* CRC-16 standard */
    0xA001				/* CRC-16 reversed */
  };

  static unsigned long p24[] =	/* 24-bit CRC generator polynomials */
  {
    0x5d6dcb,				/* CRC-24 (FlexRay) */
    0x864cfb				/* CRC-24 (OpenPGP) */
  };

  if( (retname = malloc( 19 + len )) != NULL )
  {
    /* Return buffer successfully allocated; populate it...
     */
    retry &= 7;
    len = strlen( refname );
    sprintf( retname, "%s-%u-%03u-%04x-%06x", tag, retry, len,
	(unsigned)(generic_crc( 16, p16[retry>>1], refname, len )),  /* 16-bit CRC */
	(unsigned)(generic_crc( 24, p24[retry&1], refname, len ))    /* 24-bit CRC */
      );
  }

  /* Return value is either a pointer to the populated buffer,
   * or NULL on allocation failure.
   */
  return retname;
}

/* $RCSfile: pkghash.c,v $: end of file */
