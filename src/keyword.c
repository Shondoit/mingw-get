/*
 * keyword.c
 *
 * $Id: keyword.c,v 1.2 2010/06/24 20:49:39 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010 MinGW Project
 *
 *
 * Implementation of "has_keyword()" function; this is used to check
 * for the presence of a specified keyword with a whitespace separated
 * list, appearing as an XML property string.
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
#include <ctype.h>
#include <stdlib.h>

#define FALSE  0
#define TRUE   !FALSE

int has_keyword( const char *lookup, const char *in_list )
{
  /* Check if the keyword specified by "lookup" is present in
   * the white-space separated list specified by "in_list".
   */
  if( (lookup != NULL) && (in_list != NULL) )
  {
    /* Provided both "lookup" and "in_list" are specified...
     */
    while( *in_list )
    {
      /* ...and while we haven't run out of list entries to match...
       *
       * Perform a character-by-character comparison between the
       * "lookup" string and the leading entry in the currently
       * unchecked section of "in_list"...
       */
      const char *inspect = lookup;
      while( *inspect && ! isspace( *in_list ) && (*inspect++ == *in_list++) )
	/*
	 * ...advancing pointers to both, with no further action,
	 * until we find a mismatch.
	 */
	;

      /* If the mismatch coincides with the terminating NUL for "lookup",
       * AND we've simultaneously encountered a keyword separator, or the
       * terminating NUL for "in_list"...
       */
      if( (*inspect == '\0') && ((*in_list == '\0') || isspace( *in_list )) )
	/*
	 * ...then we have found a keyword match...
	 */
	return TRUE;

      /* Otherwise, we have not yet found a match...
       * Step over any remaining non-white-space characters in the current
       * "in_list" entry, and also the following space character if any...
       */
      while( *in_list && ! isspace( *in_list++ ) )
	/*
	 * ...until we either exhaust "in_list", or we are ready to cycle
	 * back to evaluate the next potentially matching entry.
	 */
	;
    }
  }
  /* If we get to here, then there was no match for "lookup" in "in_list"...
   */
  return FALSE;
}

/* $RCSfile: keyword.c,v $: end of file */
