#include <stdio.h>
/*
 * vercmp.cpp
 *
 * $Id: vercmp.cpp,v 1.4 2011/02/15 21:39:13 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, MinGW Project
 *
 *
 * Implementation of package version comparator module.
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
#include "vercmp.h"
#include <string.h>

void pkgVersionInfo::Parse( const char* version, const char* build )
{
  /* Delegated constructor/reconstructor implementation...
   * Decompose given version number and build serial number strings,
   * storing components within the specified class structure.
   *
   * Note that the strings to be parsed are invariant, (and it is
   * necessary that they be so), but we need to create modifiable
   * copies to facilitate decomposition...
   */
  char *wildcard = build_string = NULL;
  char *p = version_string = strdup( version ? version : "" );

  /* Walking over all version number constituent elements...
   */
  for( int index = VERSION_MAJOR; index < VERSION_ELEMENT_COUNT; index++ )
  {
    /* ...initialise default element value to zero.
     */
    unsigned long value = 0L;

    /* When appropriate...
     */
    if( (index == SNAPSHOT_DATE) && (*p == '\0') && (build != NULL) )
      /*
       * ...select second argument for parsing.
       */
      p = build_string = strdup( build );

    /* When parsing an explicitly specified numeric argument...
     */
    while( (*p >= '0') && ((*p - '0') < 10) )
    {
      /* ...accumulate its ultimate value, and clear any prior
       * "wildcard" matching request which may have been carried
       * forward from the preceding field specification.
       */
      value = *p++ - '0' + 10 * value;
      wildcard = NULL;
    }

    /* Store it, note the presence of any suffix, and establish
     * the control state for a possible "wildcard" match.
     */
    version_elements[index].value = value;
    version_elements[index].suffix = p;
    if( (value == 0L) && (*p == '*') )
      wildcard = p++;

    /* Skip forward to next element field delimiter, clearing any
     * active "wildcard" matching request, if the "suffix" doesn't
     * represent a pure "wildcard" designator.
     */
    while( *p && (*p != '.') && (*p != '-') )
      if( *p++ != '*' ) wildcard = NULL;

    /* Evaluate the current field delimiter, to identify the type
     * of the following field (if any)...
     */
    if( (*p == '-') || ((*p == '\0') && (build != NULL)) )
      /*
       * ...and, if we hit the end of the version number,
       * before we filled out all of its possible elements,
       * then zero the remainder, (while preserving "wildcard"
       * matching state), before we progress to capture the
       * build serial number.
       */
      while( index < VERSION_PATCHLEVEL )
      {
	version_elements[++index].value = 0L;
	version_elements[index].suffix = wildcard ? wildcard : p;
      }

    /* If "wildcard" matching is in the active state, by the time
     * we get to here, then it may have been activated for this field,
     * or it may have been passed forward from a preceding field...
     */
    if( wildcard )
      /*
       * ...we don't know which applies, so we unconditionally adjust
       * the "suffix" pointer, to ensure that the state is recorded.
       */
      version_elements[index].suffix = wildcard;

    /* Step over any delimiter, which demarcates the current
     * version number or build serial number element field, while
     * ensuring that the decomposed field is properly terminated.
     */
    if( *p ) *p++ = '\0';
  }
}

long pkgVersionInfo::Compare( const pkgVersionInfo& rhs, int index )
{
  /* Compare a given element of a package version specification with the
   * corresponding element of a reference (rhs) version specification; return
   * <0L, 0L or >0L for less than, equal to or greater than rhs respectively.
   */
  long cmpval;
  if( (cmpval = rhs.version_elements[index].value) == 0L )
  {
    /* In the special case where the reference value is zero...
     */
    const char *p = rhs.version_elements[index].suffix;
    if( (p != NULL) && (p[0] == '*') && (p[1] == '\0') )
    {
      /* ...and where it has an explicit suffix which is identically
       * equal to the string "*", then it represents a "wildcard" match,
       * which unconditionally matches everything as "equal".
       */
      return 0L;
    }
  }

  /* When we didn't match a "wildcard"...
   * we fall through to here, and proceed with a normal comparison.
   */
  if( (cmpval = version_elements[index].value - cmpval) == 0L )
  {
    /* The specified element values are identically equal;
     * discriminate on suffixes, if any...
     */
    const char *p = version_elements[index].suffix;
    const char *r = rhs.version_elements[index].suffix;

    /* Check that both version specifications include a suffix;
     * ( p == r implies both are NULL, hence neither has a suffix );
     * if only one has, then that is the greater...
     */
    if( p == r ) return 0L;
    if( p == NULL ) return 1L;
    if( r == NULL ) return -1L;

    /* Both DO have suffixes...
     */
    while( *p && *r && (*p == *r) && (*p != '.') && (*p != '-') )
    {
      /* ...scan both, until we find a mismatched character,
       * or the terminal delimiter for either.
       */
      ++p; ++r;
    }

    /* Compute return value based on difference between the
     * mismatched characters, representing delimiters as NUL.
     */
    cmpval = (*p && (*p != '.') && (*p != '-')) ? (long)(*p) : 0L;
    cmpval -= (*r && (*r != '.') && (*r != '-')) ? (long)(*r) : 0L;
  }
  return cmpval;
}

bool pkgVersionInfo::operator<( const pkgVersionInfo& rhs )
{
  /* Comparison operator...
   * Does the version number under consideration represent a less
   * recent release than the specified reference version number.
   */
  long cmp;
  for( int index = VERSION_MAJOR; index < VERSION_ELEMENT_COUNT; index++ )
    if( (cmp = Compare( rhs, index )) != 0L ) return (cmp < 0L);

  /* If we get to here, lhs and rhs versions are identically equal,
   * and hence fail the lhs < rhs comparison.
   */
  return false;
}

bool pkgVersionInfo::operator<=( const pkgVersionInfo& rhs )
{
  /* Comparison operator...
   * Does the version number under consideration represent the same, or
   * a less recent release than the specified reference version number.
   */
  long cmp;
  for( int index = VERSION_MAJOR; index < VERSION_ELEMENT_COUNT; index++ )
    if( (cmp = Compare( rhs, index )) != 0L ) return (cmp < 0L);

  /* If we get to here, lhs and rhs versions are identically equal,
   * and hence satisfy the lhs <= rhs comparison.
   */
  return true;
}

bool pkgVersionInfo::operator==( const pkgVersionInfo& rhs )
{
  /* Comparison operator...
   * Does the version number under consideration exactly match
   * the specified reference version number.
   */
  for( int index = VERSION_MAJOR; index < VERSION_ELEMENT_COUNT; index++ )
    if( Compare( rhs, index ) != 0L ) return false;

  /* If we get to here, lhs and rhs versions are identically equal,
   * which is what we require to satisfy the lhs == rhs comparison.
   */
  return true;
}

bool pkgVersionInfo::operator!=( const pkgVersionInfo& rhs )
{
  /* Comparison operator...
   * Does the version number under consideration differ from
   * the specified reference version number.
   */
  for( int index = VERSION_MAJOR; index < VERSION_ELEMENT_COUNT; index++ )
    if( Compare( rhs, index ) != 0L ) return true;

  /* If we get to here, lhs and rhs versions are identically equal,
   * which is the sole condition to fail the lhs != rhs comparison.
   */
  return false;
}

bool pkgVersionInfo::operator>=( const pkgVersionInfo& rhs )
{
  /* Comparison operator...
   * Does the version number under consideration represent the same, or
   * a more recent release than the specified reference version number.
   */
  long cmp;
  for( int index = VERSION_MAJOR; index < VERSION_ELEMENT_COUNT; index++ )
    if( (cmp = Compare( rhs, index )) != 0L ) return (cmp > 0L);

  /* If we get to here, lhs and rhs versions are identically equal,
   * and hence satisfy the lhs >= rhs comparison.
   */
  return true;
}

bool pkgVersionInfo::operator>( const pkgVersionInfo& rhs )
{
  /* Comparison operator...
   * Does the version number under consideration represent a more
   * recent release than the specified reference version number.
   */
  long cmp;
  for( int index = VERSION_MAJOR; index < VERSION_ELEMENT_COUNT; index++ )
    if( (cmp = Compare( rhs, index )) != 0L ) return (cmp > 0L);

  /* If we get to here, lhs and rhs versions are identically equal,
   * and hence fail the lhs > rhs comparison.
   */
  return false;
}

/* $RCSfile: vercmp.cpp,v $: end of file */
