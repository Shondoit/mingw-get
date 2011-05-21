#ifndef PKGOPTS_H
/*
 * pkgopts.h
 *
 * $Id: pkgopts.h,v 1.1 2011/05/21 18:38:11 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2011, MinGW Project
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

enum
{ /* Specification of symbolic names (keys) for each of the individual
   * entries in the options parameter array.
   */
  OPTION_FLAGS,
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

/* Bit-mapped control tags used by the CLI options parsing code...
 */
#define OPTION_STORAGE_CLASS	(0x00000007 << 28)
/*
 * ...to determine how option arguments are to be inserted into the
 * global options table...
 */
#define OPTION_STORE_STRING	(0x00000001 << 28)
#define OPTION_STORE_NUMBER	(0x00000002 << 28)
#define OPTION_MERGE_NUMBER	(0x00000003 << 28)
/*
 * ...with specific handling for individual options.
 */
#define OPTION_TRACE  (OPTION_MERGE_NUMBER | OPTION_DEBUGLEVEL)

/* Options controlled by bit-mapped flags within OPTION_FLAGS...
 */
#define OPTION_VERBOSE		(0x00000003)

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
    inline unsigned GetValue( int index )
    {
      /* Retrieve the value of a numeric data entry.
       */
      return this ? (flags[index].numeric) : 0;
    }
    inline const char *GetString( int index )
    {
      /* Retrieve a pointer to a string data entry.
       */
      return this ? (flags[index].string) : NULL;
    }
    inline unsigned Test( unsigned mask, int index = OPTION_FLAGS )
    {
      /* Test the state of specified bits within
       * a bit-mapped numeric data (flags) entry.
       */
      return this ? (flags[index].numeric & mask) : 0;
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
