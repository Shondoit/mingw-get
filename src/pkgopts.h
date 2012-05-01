#ifndef PKGOPTS_H
/*
 * pkgopts.h
 *
 * $Id: pkgopts.h,v 1.8 2012/05/01 20:01:41 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2011, 2012, MinGW Project
 *
 *
 * Public declarations of the data structures, values and functions
 * for specification and control of global program options.
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
#define PKGOPTS_H  1

#include <stdint.h>		/* required for uint64_t typedef */

#define OPTION_GENERIC  0

enum
{ /* Specification of symbolic names (keys) for each of the individual
   * entries in the options parameter array.
   */
  OPTION_FLAGS,
  OPTION_EXTRA_FLAGS,
  OPTION_ASSIGNED_FLAGS,
  OPTION_DESKTOP_ARGS,
  OPTION_START_MENU_ARGS,
  OPTION_DEBUGLEVEL,

  /* This final entry specifies the size of the parameter array which
   * comprises the data content of the options structure; it MUST be the
   * final item in the enumeration, but is NOT intended to be used as
   * a key into the data table itself.
   */
  OPTION_TABLE_SIZE
};

struct pkgopts
{
  /* The primary data structure used to accumulate the settings data for
   * user specified global program options; (fundamentally, this is a type
   * agnostic array...
   */
  union
  { /* ...which is suitable for storage of...
     */
    unsigned numeric;       /* ...integer numeric, or bit-map data...     */
    const char *string;     /* ...and reference pointers for string data) */
  }
  flags[OPTION_TABLE_SIZE];
};

/* The following macro provides a mechanism for recording when
 * any string or numeric value option is assigned, even in the
 * event that the assignment reproduces the default value.
 */
#define mark_option_as_set( options, index )  \
  (options).flags[OPTION_ASSIGNED_FLAGS].numeric |= OPTION_ASSIGNED(index)

/* Bit-mapped control tags used by the CLI options parsing code...
 */
#define OPTION_SHIFT_MASK	(0x0000000f << 24)
#define OPTION_STORAGE_CLASS	(0x00000007 << 28)
/*
 * ...to determine how option arguments are to be inserted into the
 * global options table...
 */
#define OPTION_STORE_STRING	(0x00000001 << 28)
#define OPTION_STORE_NUMBER	(0x00000002 << 28)
#define OPTION_MERGE_NUMBER	(0x00000003 << 28)
/*
 * ...with specific handling for individual options, when set (as
 * indicated by evaluation of...
 */
#define OPTION_ASSIGNED(N)	(1 << (((N) & 0x1F) - OPTION_ASSIGNED_FLAGS -1))

#define OPTION_TRACE		(OPTION_MERGE_NUMBER | OPTION_DEBUGLEVEL)

/* Options controlled by bit-mapped flags within OPTION_FLAGS...
 */
#define OPTION_VERBOSE		(0x00000003)
#define OPTION_VERBOSE_MAX	(0x00000003)

#define OPTION_REINSTALL	(0x00000010)
#define OPTION_DNLOAD_ONLY	(0x00000030)
#define OPTION_DOWNLOAD_ONLY	(0x00000030)
#define OPTION_PRINT_URIS	(0x00000070)

#define OPTION_RECURSIVE	(0x00000080)
#define OPTION_ALL_DEPS 	(0x00000090)
#define OPTION_ALL_RELATED	(0x00000100)

#define OPTION_DESKTOP		(OPTION_STORE_STRING | OPTION_DESKTOP_ARGS)
#define OPTION_START_MENU	(OPTION_STORE_STRING | OPTION_START_MENU_ARGS)

#if __cplusplus
/*
 * We provide additional features for use in C++ modules.
 */
#include <stdlib.h>

#ifndef EXTERN_C
/* This is primarily for convenience...
 */
# define EXTERN_C  extern "C"
#endif

class pkgOpts : protected pkgopts
{
  /* A derived "convenience" class, associating a collection
   * of utility methods with the pkgopts structure.
   */
  public:
    inline unsigned IsSet( int index )
    {
      return this
	? flags[OPTION_ASSIGNED_FLAGS].numeric & OPTION_ASSIGNED(index)
	: 0;
    }
    inline unsigned GetValue( int index )
    {
      /* Retrieve the value of a numeric data entry.
       */
      return this ? (flags[index & 0xFFF].numeric) : 0;
    }
    inline const char *GetString( int index )
    {
      /* Retrieve a pointer to a string data entry.
       */
      return this ? (flags[index & 0xFFF].string) : NULL;
    }
    inline unsigned Test( unsigned mask, int index = OPTION_FLAGS )
    {
      /* Test the state of specified bits within
       * a bit-mapped numeric data (flags) entry.
       */
      return this ? (flags[index].numeric & mask) : 0;
    }
    inline void SetFlags( unsigned value )
    {
      /* This is a mask and store operation, to set a specified
       * bit-field within the first pair of flags slots; it mimics
       * the options setting operation performed in the CLI start-up
       * code, where the input value represents a 12-bit flag code,
       * packaged with a 12-bit combining mask, and an alignment
       * shift count between 0 and 52, in 4-bit increments.
       */
      unsigned shift;
      if( (shift = (value & OPTION_SHIFT_MASK) >> 22) < 53 )
      {
	*(uint64_t *)(flags) &= ~((uint64_t)((value & 0xfff000) >> 12) << shift);
	*(uint64_t *)(flags) |= (uint64_t)(value) << shift;
      }
    }
};

/* Access modes for the following global options accessor function.
 */
enum { OPTION_TABLE_LOOKUP, OPTION_TABLE_ASSIGN };

/* Global options accessor function; the default access mode is
 * OPTION_TABLE_LOOKUP, (in which case the second argument is ignored),
 * which allows us to simply use
 *
 *   result = pkgOptions()->GetValue( OPTION_KEY );
 *
 * to retrieve a value from the global options table.
 *
 * The OPTION_TABLE_ASSIGN access mode is provided for initialisation
 * of the global options table reference pointer; it is used during
 * program start-up, after which it is not normally used again.
 */
EXTERN_C pkgOpts *pkgOptions( int = OPTION_TABLE_LOOKUP, struct pkgopts* = NULL );

#endif /* __cplusplus */

#endif /* PKGOPTS_H: $RCSfile: pkgopts.h,v $: end of file */
