/*
 * pkgspec.cpp
 *
 * $Id: pkgspec.cpp,v 1.3 2011/02/15 21:39:13 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, MinGW Project
 *
 *
 * Implementation for the "pkgTarName" class, as declared in header
 * file "pkginfo.h".
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
#include "pkginfo.h"
#include "pkgkeys.h"
#include "vercmp.h"

#include <string.h>

/* Constructors...
 */
pkgSpecs::pkgSpecs( const char *tarname )
{
  /* Parse the given tarball name, storing its constituent element
   * decomposition within the class' local "pkginfo" array structure.
   */
  content = get_pkginfo( tarname ? tarname : "", specs );
}

pkgSpecs::pkgSpecs( pkgXmlNode *release )
{
  /* Retrieve the "tarname" from an XML "release" specification,
   * then construct the "pkgSpecs" as if it were specified directly.
   */
  const char *tarname = release ? release->GetPropVal( tarname_key, NULL ) : NULL;
  content = get_pkginfo( tarname ? tarname : "", specs );
}

/* Copy constructor...
 */
static
void *clone_specs( char *content, pkginfo_t const src, pkginfo_t dst )
{
  /* Local helper function performs a deep copy of the "content" buffer,
   * and assigns the "specs" pointers to refer to it; this is the action
   * required to implement the copy constructor, and it is also used by
   * the assignment operator implentation.
   */
  char *rtn;
  int count = PACKAGE_TAG_COUNT;

  /* Find the last allocated pointer in the source "specs" list; this
   * tells us where to find the last string in the "content" buffer...
   */
  while( (count > 0) && (src[--count] == NULL) )
    ;

  /* ...whence we may compute the size of the buffer, and allocate
   * a new buffer, into which to copy the data.
   */
  count = src[count] + strlen( src[count] ) - content;
  if( (rtn = (char *)(malloc( count + 1))) != NULL )
  {
    /* On successful buffer allocation, copy the data,
     * then walk the list of pointers...
     */
    rtn = (char *)(memcpy( rtn, content, count ));
    for( count = 0; count < PACKAGE_TAG_COUNT; ++count )
    {
      if( src[count] == NULL )
	/*
	 * ...propagating NULL pointers "as are"...
	 */
	dst[count] = NULL;

      else
	/* ...and non-NULL adjusted, as necessary,
	 * to point into the copied data buffer...
	 */
	dst[count] = (char *)(rtn) + (src[count] - content);
    }
  }
  /* ...ultimately, returning the base address of the new buffer.
   */
  return (void *)(rtn);
}
/* Formal implementation of the copy constructor...
 */
pkgSpecs::pkgSpecs( const pkgSpecs& src )
{
  /* ...requires no more than a call to the local helper function.
   */
  content = clone_specs( (char *)(src.content), src.specs, specs );
}

/* Assignment operator...
 */
pkgSpecs& pkgSpecs::operator=( const pkgSpecs& rhs )
{
  /* Provided the lhs and rhs represent distinct objects...
   */
  if( this != &rhs )
  {
    /* ...this is much the same as the copy constructor, except that,
     * while the constructor is guaranteed to be creating a new object,
     * assignment may be replacing an existing lhs object; this will
     * own a dynamically allocated data buffer, which must be freed,
     * to avoid leaking memory.
     */
    free( content );
    content = clone_specs( (char *)(rhs.content), rhs.specs, specs );
  }
  return *this;
}

/* Destructor...
 */
pkgSpecs::~pkgSpecs()
{
  /* Need to free the dynamic memory associated with the "specs" array,
   * and in which the actual tarname decomposition is stored.
   */
  free( content );
}

/* Comparison operators...
 */
int pkgSpecs::VersionComparator( pkgSpecs& rhs )
{
  /* Private helper method, used to facilitate implementation
   * of the comparison operator methods.  It considers the "this"
   * pointer as a reference to the entity on the left hand side of
   * the comparison operator, and the single argument as a reference
   * to the entity on the right hand side.  The integer return value
   * is zero if the two entities compare as equal, (i.e. representing
   * identically the same package version), less than zero if the LHS
   * entity represents a "lesser" (i.e. an earlier) version than the
   * RHS, or greater than zero if the LHS represents a "greater"
   * (i.e. a more recent) version than the RHS.
   *
   * Initially, we compare just the package version itself...
   */
  pkgVersionInfo lhs_version( GetPackageVersion(), GetPackageBuild() );
  pkgVersionInfo rhs_version( rhs.GetPackageVersion(), rhs.GetPackageBuild() );
  /*
   * ...returning immediately, with an appropriate return value,
   * if LHS and RHS versions are distinct.
   */
  if( lhs_version < rhs_version ) return -1;
  if( lhs_version > rhs_version ) return +1;

  /* If we get to here, then the package versions of LHS and RHS
   * are identically the same; however, we may still be able to
   * differentiate between them, on the basis of progression in
   * their respective development (release) status qualifiers.
   */
  const char *lhs_quality, *rhs_quality;
  if( (lhs_quality = GetReleaseStatus()) != NULL )
  {
    /* The LHS entity is qualified as "alpha", "beta", ...
     */
    if( (rhs_quality = rhs.GetReleaseStatus()) == NULL )
      /*
       * ...but the RHS entity is not; we always consider an
       * unqualified version to implicitly represent "stable",
       * which is always compared as "more recent" than any
       * "alpha", "beta" or "rc" qualified release at the
       * same package version point, so we may immediately
       * confirm the LHS as the "lesser" release.
       */
      return -1;

    /* If we still haven't differentiated them, then both LHS
     * and RHS must be qualified.  Check if we can resolve the
     * deadlock on the basis of progression of development from
     * "alpha" through "beta" and "rc" to "stable" phases; (note
     * that simply checking the initial character of the phase
     * qualifier indicates the appropriate progression).
     */
    int chkval = *lhs_quality - *rhs_quality;
    if( chkval != 0 ) return chkval;

    /* If we still can't resolve the deadlock, then both LHS
     * and LHS must be qualified as being in identically the
     * same development phase, so we must now differentiate
     * on the basis of progression of the release index...
     */
    lhs_version.Reset( GetReleaseIndex() );
    rhs_version.Reset( rhs.GetReleaseIndex() );
    /*
     * ...noting that these progress in the same manner as
     * the package version number itself.
     */
    if( lhs_version < rhs_version ) return -1;
    if( lhs_version > rhs_version ) return +1;
  }

  else if( rhs.GetReleaseStatus() != NULL )
    /*
     * In this case, the RHS entity is qualified as "alpha",
     * "beta", ..., but the LHS is not.  Since we've already
     * determined that both represent the same version of the
     * package, we may infer that the LHS represents a stable
     * derivative of the qualified RHS, and thus corresponds
     * to a more recent release, so return the appropriate
     * value to indicate LHS > RHS.
     */
    return +1;

  /* If we get to here, then LHS and RHS represent the same
   * version of the package, at the same phase of development;
   * the only remaining determinant, which may differentiate
   * them, is that one has been released for a more recent
   * version of the host subsystem...
   */
  lhs_version.Reset( GetSubSystemVersion(), GetSubSystemBuild() );
  rhs_version.Reset( rhs.GetSubSystemVersion(), rhs.GetSubSystemBuild() );
  /*
   * ...so we may compare these, just as we did initially
   * for the package version identification.
   */
  if( lhs_version < rhs_version ) return -1;
  if( lhs_version > rhs_version ) return +1;

  /* Finally, if we get past all of the preceding comparisons,
   * then the LHS and RHS cannot be differentiated on the basis
   * of any package version comparison, so we may return zero
   * to assert their equality.
   */
  return 0;
}

bool pkgSpecs::operator<( pkgSpecs& rhs )
{
  /* Check if the given package release is less recent, as indicated
   * by its version and build date/serial number, than another.
   */
  return VersionComparator( rhs ) < 0;
}

bool pkgSpecs::operator<=( pkgSpecs& rhs )
{
  /* Check if the given package release is no more recent, as indicated
   * by its version and build date/serial number, than another.
   */
  return VersionComparator( rhs ) <= 0;
}

bool pkgSpecs::operator>=( pkgSpecs& rhs )
{
  /* Check if the given package release is no less recent, as indicated
   * by its version and build date/serial number, than another.
   */
  return VersionComparator( rhs ) >= 0;
}

bool pkgSpecs::operator>( pkgSpecs& rhs )
{
  /* Check if the given package release is more recent, as indicated
   * by its version and build date/serial number, than another.
   */
  return VersionComparator( rhs ) > 0;
}

/* $RCSfile: pkgspec.cpp,v $: end of file */
