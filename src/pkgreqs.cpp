/*
 * pkgreqs.cpp
 *
 * $Id: pkgreqs.cpp,v 1.3 2011/05/18 18:34:51 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, MinGW Project
 *
 *
 * Implements the SetRequirements() method for the pkgActionItem class,
 * together with additional components of the pkgSpecs class which are
 * required specifically to support it.
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
#include "pkgtask.h"

#include <stdlib.h>
#include <string.h>

/*
 ******************
 *
 * Class Implementation: pkgSpecs; SetProperty and GetTarName methods
 *
 */

static inline
int buflen( pkginfo_t specs )
{
  /* Local helper function, to determine the size of the buffer used
   * to store the data content of all pkgSpecs fields; (it is also the
   * length of the C string representing the associated package tarname,
   * INCLUDING its NUL terminator).
   */
  int index = PACKAGE_TAG_COUNT;
  while( (index-- > 0) && (specs[index] == NULL) )
    /*
     * Locate the last populated data field in the content buffer...
     */ ;
    /* ...then compute the total buffer length, from the start of
     * the first field, up to the NUL terminator of the last.
     */
  return specs[index] ? specs[index] - *specs + strlen( specs[index] ) + 1 : 0;
}

static inline
int fieldlen( const char *string )
{
  /* NULL-safe local helper function to determine the length of
   * the C string representing a pkgSpecs data field, (EXCLUDING
   * its NUL terminator); returns zero in the case of an unassigned
   * field, identified by a NULL reference pointer.
   */
  return (string == NULL) ? 0 : strlen( string );
}

static inline
char *fieldcpy( char *dest, const char *src )
{
  /* Local helper function to copy the content of a single
   * pkgSpecs data field...
   */
  while( (*dest++ = *src++) != '\0' )
    /*
     * ...copying byte by byte, including the terminating NUL,
     * to the destination buffer...
     */ ;
  /* ...and ultimately, return the next available location
   * within the destination buffer.
   */
  return dest;
}

static inline
char *bufcpy( int index, int end, char *buf, pkginfo_t specs )
{
  /* Local helper function to copy a specified field range from
   * its original location within the pkgSpecs content buffer, to
   * an alternate location specified by "buf".
   */
  do { if( specs[index] != NULL )
         /*
	  * Copy each non-empty data field...
	  */
         buf = fieldcpy( buf, specs[index] );

       /* ...until all specified fields in the source range
	* have been copied.
	*/
     } while( ++index < end );

  /* Finally, return the location at which any further data should
   * be appended to the data just copied.
   */
  return buf;
}

const char *pkgSpecs::SetProperty( int index, const char *repl )
{
  /* A private method to modify the content of any single data field
   * within a pkgSpecs data buffer; it provides the core implementation
   * for each of the public inline field manipulator methods.
   */
  char *p;

  /* We begin by computing the current size of the content buffer,
   * and the size it will become after making the change.
   */
  int oldlen = buflen( specs );
  int newlen = oldlen + fieldlen( repl ) - fieldlen( specs[index] );

  if( (repl != NULL) && (specs[index] == NULL) )
    /*
     * When inserting a non-empty value into a previously empty field,
     * we need to allow an additional byte for an extra field separator.
     */
    ++newlen;

  if( newlen > oldlen )
  {
    /* The buffer size must increase; thus, there is insufficient space
     * at the location of the field to be modified, to accomodate its new
     * value, without corrupting the content of following fields.  To avoid
     * this, we use a temporary buffer in which we construct an image of
     * the modified content...
     */
    char newbuf[ newlen ];
    /*
     * ...first copying the content of all fields which precede that
     * which is to be modified...
     */
    p = bufcpy( 0, index, newbuf, specs );
    /*
     * ...appending the new value to be assigned to the modified field...
     */
    p = fieldcpy( p, repl );
    /*
     * ...and then the content of any following unmodified fields.
     */
    bufcpy( index + 1, PACKAGE_TAG_COUNT, p, specs );

    /* Having created this temporary image of the content buffer, as
     * it is to become, adjust the allocation size of the original buffer,
     * and copy its modified content into place.
     */
    if( (p = (char *)(content = realloc( content, newlen ))) != NULL )
      memcpy( p, newbuf, newlen );
  }

  else
  { /* In this case, the size of content buffer will either shrink, or
     * it will remain unchanged; we may modify it in place, beginning at
     * the location of the field which is to be modified, by writing its
     * new value, if any, into place.
     */
    p = specs[index];
    if( (repl != NULL) && (*repl != '\0') )
      p = fieldcpy( p, repl );

    /* If the new content of the modified field is exactly the same length
     * as the content it has replaced, then the overall size of the content
     * buffer does not change...
     */
    if( newlen == oldlen )
      /*
       * ...so we have nothing more to do, to complete the modification...
       */
      return (char *)(content);

    /* ...otherwise, there is residual data from the original content at
     * the location now indicated by "p", and preceding the start of the
     * content of the next populated field, (if any); clear this, moving
     * the content of all following fields up to fill the gap...
     */
    bufcpy( index + 1, PACKAGE_TAG_COUNT, p, specs );

    /* ...and adjust the allocation size to the reduced data length.
     */
    p = (char *)(content = realloc( content, newlen ));
  }

  /* Irrespective of which of the preceding paths we followed to get here,
   * the size of the content buffer was adjusted, so...
   */
  if( p != NULL )
  {
    /* ...provided it was not lost altogether...
     */
    if( (repl == NULL) || (*repl == '\0') )
      /*
       * ...we either mark the content of the modified field as "deleted",
       * when appropriate...
       */
      specs[index] = NULL;

    else
      /* ...or we ensure that it is marked as having SOME content, in case
       * it may have been empty beforehand; (note that this does NOT assign
       * the correct pointer value -- any arbitrary non-NULL pointer value
       * will suffice here, so we just adopt the physical start address of
       * the content buffer).
       */
      specs[index] = p;

    /* Now, we walk through the content buffer, to fix up the data pointers
     * for the content of any fields for which the start address may have
     * changed, as a result of buffer size adjustment...
     */
    for( index = 0; index < PACKAGE_TAG_COUNT; index++ )
    {
      /* ...thus, for each non-empty field...
       */
      if( specs[index] != NULL )
      {
	/* ...assign the start address of the next available block of
	 * populated content data...
	 */
	specs[index] = p;

	/* ...and advance the data pointer to the start of the next
	 * available block of content, (if any).
	 */
	p += strlen( p ) + 1;
      }
    }
  }

  /* Finally, we return the address of the physical starting location
   * of the (possibly relocated) pkgSpecs content buffer.
   */
  return (char*)(content);
}

const char *pkgSpecs::GetTarName( const char *buf )
{
  /* Reconstitute the canonical tarname for the package
   * identified by the current pkgSpecs record, returning
   * the result in a C string buffer allocated by malloc();
   * "buf" may be specified as NULL, otherwise it MUST point
   * to a similarly allocated buffer, which is recycled.
   */
  char *dest = (char*)(realloc( (void*)(buf), buflen( specs ) ));
  if( (buf = dest) != NULL )
    /*
     * We have a valid buffer suitable for returning the result...
     */
    for( int index = PACKAGE_NAME; index < PACKAGE_TAG_COUNT; index++ )
    {
      /* For each field in the package specification...
       */
      char *src = specs[index];
      if( (src != NULL) && (*src != '\0') )
      {
	/* ...for which a non-empty value is defined...
	 */
	if( dest > buf )
	  /*
	   * ...when not the first such field, insert the
	   * appropriate field separator character...
	   */
	  *dest++ = (index < PACKAGE_FORMAT) ? '-' : '.';

	/* ...then, noting that the release status may have an
	 * initial '$' token which was inserted by get_pkginfo(),...
	 */
	if( (index == PACKAGE_RELEASE_STATUS) && (*src == '$') )
	  /*
	   * ...and which we don't therefore want to include within
	   * this reverse transformation...
	   */
	  ++src;
	/*
	 * ...copy the field content to the result buffer.
	 */
	while( (*dest = *src++) != '\0' )
	  ++dest;
      }
    }

  /* Return the fully reconstituted tarname, or NULL if the
   * return buffer allocation failed.
   */
  return buf;
}


/*
 ******************
 *
 * Class Implementation: pkgActionItem; SetRequirements method
 *
 */

enum inherit_mode
{
  /* Definition of the inheritance flags identifying those elements
   * of the package and subsystem version fields, within a requirements
   * specification, which are to be matched by a "%" wildcard.
   *
   * These flags apply to any ONE version number specification, which
   * may represent EITHER the package version OR the subsystem version
   * within a requirements specification; it takes the form:
   *
   *   major.minor.patch-datestamp-index
   *
   * and the flags are interpreted to indicate...
   */
  INHERIT_NONE = 0,	/* no "%" wildcard match requested            */
  INHERIT_VERSION,	/* "major.minor.patch" matches a "%" wildcard */
  INHERIT_BUILD,	/* "datestamp-index" matches a "%" wildcard   */
  INHERIT_ALL		/* "%" matches the entire version string      */
};

static
enum inherit_mode inherited( const char *ver, const char *bld )
{
  /* Local helper function to assign "%" wildcard inheritance flags
   * to a specified version number, where:
   *
   *   "ver" represents the "major.minor.patch" part of the version
   *   "bld" represents the optional "datestamp-index" extension
   */
  enum inherit_mode retval = INHERIT_NONE;

  /* Noting that the "%" wildcard must represent the respective part
   * of the version specification in its entirety...
   */
  if( (ver != NULL) && (ver[0] == '%') && (ver[1] == '\0') )
    /*
     * ...flag a match on the "major.minor.patch" specification.
     */
    retval = INHERIT_VERSION;

  /* Considering the optional "datestamp-index" specification...
   */
  if( bld == NULL )
  {
    /* ...when it isn't specified at all, then it is implicitly
     * governed by a match on the "major.minor.patch" element...
     */
    if( retval == INHERIT_VERSION )
      /*
       * ...thus promoting this to become a match on the entire
       * "major.minor.patch-datestamp-index" specification.
       */
      retval = INHERIT_ALL;
  }
  else if( (bld[0] == '%') && (bld[1] == '\0') )
    /*
     * ...otherwise we may, (less usefully, perhaps and therefore
     * not to be encouraged), require a "%" wildcard match on the
     * "datestamp-index" element alone, (with some other criterion
     * applied to the "major.minor.patch" element -- perhaps even
     * redundantly "%-%", which is equivalent to the simpler form
     * of "%" alone, to match the entire version specification).
     */
    retval = (enum inherit_mode)(retval | INHERIT_BUILD);

  /* The inheritance state is now fully specified; pass it back
   * to the caller.
   */
  return retval;
}

static
const char *requirement( const char *wanted, pkgSpecs *dep )
{
  /* Local helper function to generate "min_wanted" and "max_wanted"
   * dependency specifications, for assignment within a pkgActionItem;
   * propagation of version number fields inherited from the dependant,
   * as specified by the "%" wildcard, is appropriately enforced.  The
   * resultant specification is ALWAYS returned on the heap, in memory
   * dynamically allocated by malloc().
   */
  pkgSpecs id( wanted );

  /* Evaluate inheritance of the PACKAGE version number specification...
   */
  enum inherit_mode clone;
  clone = inherited( id.GetPackageVersion(), id.GetPackageBuild() );
  if( (clone & INHERIT_VERSION) > INHERIT_NONE )
    /*
     * ...propagating its "major.minor.patch" element as required...
     */
    id.SetPackageVersion( dep->GetPackageVersion() );

  if( (clone & INHERIT_BUILD) > INHERIT_NONE )
    /*
     * ...and similarly, its "datestamp-index" element.
     */
    id.SetPackageBuild( dep->GetPackageBuild() );

  /* Similarly, for the SUBSYSTEM version number specification...
   */
  clone = inherited( id.GetSubSystemVersion(), id.GetSubSystemBuild() );
  if( (clone & INHERIT_VERSION) > INHERIT_NONE )
    /*
     * ...propagate its "major.minor.patch" element as required...
     */
    id.SetSubSystemVersion( dep->GetSubSystemVersion() );

  if( (clone & INHERIT_BUILD) > INHERIT_NONE )
    /*
     * ...and similarly, its "datestamp-index" element.
     */
    id.SetSubSystemBuild( dep->GetSubSystemBuild() );

  /* Finally, reconstitute the canonical tarname representation of the
   * specification, from its decomposed representation; (as a side effect,
   * this automatically places the return string on the heap).
   */
  return id.GetTarName();
}

const char * pkgActionItem::SetRequirements( pkgXmlNode *req, pkgSpecs *dep )
{
  /* Establish the selection criteria, for association of any
   * particular package release with an action item.
   */
  flags &= ACTION_MASK;

  /* First check for a strict equality requirement...
   */
  if( (min_wanted = req->GetPropVal( eq_key, NULL )) != NULL )
    /*
     * ...and if specified, set the selection range such that only one
     * specific release can be matched; evaluate any specified dependency
     * relationship, to ensure that inherited version numbers are correctly
     * propagated, and assign the resultant dynamically allocated tarname
     * specifications to the respective constraints.
     */
    return max_wanted = min_wanted = requirement( min_wanted, dep );

  else
  { /* Check for either an inclusive, or a strictly exclusive,
     * minimum requirement (release "greater" than) specification,
     * setting the minimum release selector...
     */
    if( ((min_wanted = req->GetPropVal( ge_key, NULL )) == NULL)
    &&  ((min_wanted = req->GetPropVal( gt_key, NULL )) != NULL)  )
      /*
       * ...and its selection mode flag accordingly.
       */
      flags |= STRICTLY_GT;

    /* Similarly, check for an inclusive, or a strictly exclusive,
     * maximum requirement (release "less" than) specification,
     * setting the maximum release selector...
     */
    if( ((max_wanted = req->GetPropVal( le_key, NULL )) == NULL)
    &&  ((max_wanted = req->GetPropVal( lt_key, NULL )) != NULL)  )
      /*
       * ...and its selection mode flag accordingly.
       */
      flags |= STRICTLY_LT;
  }

  /* Check the minimum required version specification...
   */
  if( min_wanted != NULL )
    /*
     * ...ensuring that inherited version numbers are propagated, as
     * required from the dependant, and assign the canonical tarname
     * representation of the final specification on the heap.
     */
    min_wanted = requirement( min_wanted, dep );

  /* And...
   */
  if( max_wanted != NULL )
    /*
     * ...likewise for the maximum required version specification.
     */
    max_wanted = requirement( max_wanted, dep );

  /* Return a canonical representation of the requirements spec.
   */
  return (min_wanted == NULL) ? max_wanted : min_wanted;
}

/* $RCSfile: pkgreqs.cpp,v $: end of file */
