/*
 * pkgname.cpp
 *
 * $Id: pkgname.cpp,v 1.6 2011/10/04 21:07:32 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, MinGW Project
 *
 *
 * Implementation for the non-inherited components of the pkgXmlNode
 * class, as declared in file "pkgdesc.h"; fundamentally, these are
 * the accessors for package "tarname" properties, as specified in
 * XML nodes identified as "release" elements.
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
#include <string.h>

#include "dmh.h"
#include "pkgbase.h"
#include "pkgkeys.h"
#include "pkginfo.h"
#include "pkgtask.h"

static
const char *pkgArchiveName( pkgXmlNode *rel, const char *tag, unsigned opt )
{
  /* Local helper to establish actual release file names...
   * applicable only to XML "release" elements.
   */
  if( ! rel->IsElementOfType( release_key ) )
  {
    /* The XML element type name is not "release"; identify it...
     */
    const char *reftype;
    if( (reftype = rel->GetName()) == NULL )
      /*
       * ...or classify as "unknown", when given a NULL element.
       */
      reftype = value_unknown;

    /* Complain that this XML element type is invalid, in this context...
     */
    dmh_control( DMH_BEGIN_DIGEST );
    dmh_notify( DMH_ERROR, "internal package specification error\n" );
    dmh_notify( DMH_ERROR, "can't get 'tarname' for non-release element %s\n", reftype );
    dmh_notify( DMH_ERROR, "please report this to the package maintainer\n" );
    dmh_control( DMH_END_DIGEST );

    /* ...and bail out, telling the caller that no archive name is available...
     */
    return NULL;
  }

  /* Given a package release specification...
   * First check that it relates to a real package, rather than to
   * a virtual "meta-package"; such meta-packages exist solely as
   * containers for requirements specifications, and have no
   * associated archive.
   */
  pkgXmlNode *pkg = rel->GetParent();
  while( (pkg != NULL) && ! pkg->IsElementOfType( package_key ) )
    pkg = pkg->GetParent();

  /* FIXME: we should probably provide some error handling here,
   * to diagnose release elements without any package association;
   * (these would be identified by pkg == NULL).
   */
  if( pkg != NULL )
  {
    /* We found the package association...
     * Check its 'class' attribute, if any, and if classified as
     * 'virtual', return the archive association as "none".
     */
    const char *package_class = pkg->GetPropVal( class_key, NULL );
    if( (package_class != NULL) && (strcmp( package_class, value_virtual ) == 0) )
      return value_none;
  }

  /* The given release specification relates to a real package...
   * Determine the archive name for the tarball to be processed; this
   * is retrieved from a child XML element with name specified by "tag";
   * by default, if "opt" is non-zero, it is the canonical "tarname"
   * assigned to the release element itself, unless an alternative
   * specification is provided; if "opt" is zero, no default is
   * assumed, and the return value is NULL if no alternative
   * specification is provided.
   */
  unsigned matched = 0;
  pkgXmlNode *dl = rel->GetChildren();
  while( dl != NULL )
  {
    /* Visit all children of the release specification element,
     * checking for the presence of an expected specification...
     */
    if( dl->IsElementOfType( tag ) )
    {
      /* Found one; ensure it is the only one...
       */
      if( matched++ )
	/*
	 * ...else emit a warning, and ignore this one...
	 */
	dmh_notify( DMH_WARNING, "%s: archive name reassignment ignored\n",
	    rel->GetPropVal( tarname_key, value_unknown )
	);
      else
	/* ...ok; this is the first "tag" specification,
	 * accept it as the non-default source of the release's
	 * "tarname" property.
	 */
	rel = dl;
    }
    /* Continue, until all children have been visited.
     */
    dl = dl->GetNext();
  }
  /* "rel" now points to the XML element having the appropriate
   * "tarname" specification; return a pointer to it's value.
   */
  return (opt || matched) ? rel->GetPropVal( tarname_key, NULL ) : NULL;
}

EXTERN_C const char *pkgAssociateName( const char *, const char * );

static inline
const char *pkgResolvedName( pkgXmlNode *rel, const char *tag, const char *ext )
{
  /* Local helper function to resolve the mapping from a released
   * package name, as identified from the XML release element "rel",
   * to its corresponding source or licence package name, according
   * to the selection of "source" or "licence" specified by "tag",
   * with "ext" passed a "src" or "lic" respectively.
   */
  const char *refname;
  const char *retname = NULL;

  /* First, we retrieve the released package name...
   */
  if( (refname = pkgArchiveName( rel, release_key, 1 )) != NULL )
  {
    /* ...and if successful, look for an explicit reference to
     * the source or licence package, embedded within the release
     * specification itself.
     */
    if( (retname = pkgArchiveName( rel, tag, 0 )) == NULL )
    {
      /* When this fails to identify the required mapping,
       * then we look for a generic reference, defined for
       * the containing package.
       */
      pkgXmlNode *enc = rel->GetParent();

      /* A generic reference may be placed at any nesting
       * level, between the enclosing package element and
       * the release to which it relates; thus, starting
       * at the first enclosing level...
       */
      rel = NULL;
      while( enc != NULL )
      {
	/* ...enumerate reference specifications of the
	 * appropriate type, examining all children of
	 * the enclosing element.
	 */
	unsigned matched = 0;
	pkgXmlNode *child = enc->GetChildren();
	while( child != NULL )
	{
	  /* We have a child, which we have not examined...
	   */
	  if( child->IsElementOfType( tag ) )
	  {
	    /* ...and it is of the required "tag" type.
	     */
	    if( matched++ )
	      /*
	       * We already had a candidate match, so we
	       * diagnose but otherwise this duplicate...
	       */
	      dmh_notify( DMH_WARNING,
		  "redundant %s specification ignored\n", tag
		);

	    else
	      /* This is the first candidate match found,
	       * so we accept it.
	       */
	      rel = child;
	  }
	  /* Continue examining child elements, until no more
	   * are present at the current nesting level.
	   */
	  child = child->GetNext();
	}

	/* When we've completed the examination of all children
	 * at a given nesting level, without finding a matching
	 * specification, and that level is still within the
	 * enclosing package element...
	 */
	if( (rel == NULL) && ! enc->IsElementOfType( package_key ) )
	  /*
	   * ...then we extend the search to the next enclosing
	   * level of nesting...
	   */
	  enc = enc->GetParent();

	else
	  /* ...otherwise, we abandon the search.
	   */
	  enc = NULL;
      }

      /* If we've searched all available nesting levels,
       * and failed to locate the requisite specification...
       */
      if( rel == NULL )
      {
	/* ...then we assume that the requisite tarname
	 * is identical to the release tarname, with the
	 * appropriate "ext" substitution for the package
	 * class identification...
	 */
	pkgSpecs resolved( refname );
	resolved.SetComponentClass( ext );
	/*
	 * ...so, having made the substitution,
	 * we return the resultant tarname, noting
	 * that this automatically allocates space
	 * on the heap, for the returned string.
	 */
	return resolved.GetTarName();
      }
      else
	/* We did find a mappingspecification, so we
	 * extract a tarname template from it.
	 */
	retname = rel->GetPropVal( tarname_key, NULL );
    }
    else if( strcmp( retname, value_none ) == 0 )
      /*
       * The package is virtual, or an explicit mapping
       * specification indicates that there is no related
       * source or licence package; return NULL to advise
       * the caller of this.
       */
      return NULL;

    /* If we get to here, we found a mapping specification;
     * it may be a template, so resolve any substitutions which
     * it must inherit from the released package tarname, again
     * noting that this allocates heap memory for the result.
     */
    retname = pkgAssociateName( retname, refname );
  }

  /* Finally, how ever we resolved the mapping, we return
   * the result.
   */
  return retname;
}

const char *pkgXmlNode::SourceArchiveName( unsigned long category )
{
  /* Applicable only for XML nodes designated as "release".
   *
   * Retrieve the source tarball name, if specified, from the
   * "tarname" property of the contained "source" element, within
   * an XML node designated as a "release" element.
   *
   * Returns a pointer to the text of the "tarname" property of the
   * contained "source" element, or NULL if the containing node does
   * not represent a "release", or if it does not have a contained
   * "source" element specifying a "tarname" property.
   */
  const char *tag = "lic";
  if( category != ACTION_LICENCE )
  {
    /* We may have been asked to explicitly override the "source"
     * mapping, returning the "licence" reference instead; where
     * this special exception is NOT requested, then we enforce
     * the default case.
     */
    category = ACTION_SOURCE;
    tag = "src";
  }

  /* In either case, pkgResolvedName() determines the appropriate
   * archive name, which it automatically returns on the heap.
   */
  return pkgResolvedName( this, action_name( category ), tag );
}

const char *pkgXmlNode::ArchiveName()
{
  /* Applicable only for XML nodes designated as "release".
   *
   * Retrieve the actual tarball name, if specified, from the
   * "tarname" property of a contained "download" element, within
   * an XML node designated as a "release" element.
   *
   * Returns a pointer to the text of the "tarname" property of the
   * contained "download" element, or to the "tarname" property of
   * the containing "release" element, if it does not contain an
   * alternative specification within a "download" element; if
   * unresolved to either of these, returns NULL.
   */
  return pkgArchiveName( this, download_key, 1 );
}

/* $RCSfile: pkgname.cpp,v $: end of file */
