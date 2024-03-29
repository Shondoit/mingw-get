/*
 * pkgfind.cpp
 *
 * $Id: pkgfind.cpp,v 1.5 2010/06/24 20:49:39 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, MinGW Project
 *
 *
 * Implementation of search routines for locating specified records
 * within the XML package-collection database.
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
#include <string.h>

#include "pkgbase.h"
#include "pkgkeys.h"

pkgXmlNode *
pkgXmlDocument::FindPackageByName( const char *lookup, const char *subsystem )
{
  /* Create a local copy of the package "name" which we are required to
   * "lookup"; this allows us to make temporary adjustments to facilitate
   * stripping of any "component" identifying suffix, which might occlude
   * a match for a package "alias" name.
   */
  int len; char name[ 1 + (len = strlen( lookup )) ];
  strcpy( name, lookup );

  /* Working from the root of the package directory tree...
   */
  pkgXmlNode *dir = GetRoot()->GetChildren();
  /*
   * search all "package-collection" XML nodes, to locate a package
   * by "name"; return a pointer to the XML node which contains the
   * specification for the package, or NULL if no such package.
   */
  while( dir != NULL )
  {
    /* Select only "package-collection" elements, which have been
     * assigned the desired "subsystem" property...
     */
    if( dir->IsElementOfType( package_collection_key )
    &&  subsystem_strcmp( subsystem, dir->GetPropVal( subsystem_key, NULL )) )
    {
      /* ...inspect the content of each...
       */
      pkgXmlNode *pkg = dir->GetChildren();
      while( pkg != NULL )
      {
	/* ...and, for each "package" element we find...
	 */
	if( pkg->IsElementOfType( package_key ) )
	{
	  /* ...return immediately, if it has a "name" or an "alias"
	   * property which matches the required package name...
	   */
	  const char *pkg_name, *alias;
	  if( (strcmp( name, pkg_name = pkg->GetPropVal( name_key, "" )) == 0)
	  ||  (has_keyword( name, alias = pkg->GetPropVal( alias_key, NULL ) ) != 0)  )
	    return pkg;

	  /* We did find a "package" element, but neither its "name"
	   * nor its "alias" property provided a match; look within it,
	   * for a possible match on a "component" package element...
	   */
	  pkgXmlNode *cpt = pkg->GetChildren();
	  while( cpt != NULL )
	  {
	    /* For each element contained within the "package" definition,
	     * check if it represents a "component" package definition...
	     */
	    if( cpt->IsElementOfType( component_key ) )
	    {
	      /* ...and return immediately, when it does, AND it also has a
	       * "name" property which matches the required package name...
	       */
	      if( strcmp( name, cpt->GetPropVal( name_key, "" )) == 0 )
		return cpt;

	      else
	      { /* We did find a "component" package, but its "name"
		 * property didn't match; construct an alternative name,
		 * by combining the "class" property of the "component"
		 * with the "name" property of the containing "package",
		 * and evaluate that for a possible match...
		 */
		const char *cpt_class = cpt->GetPropVal( class_key, "" );
		char *cpt_name = name + len - strlen( cpt_class );
		if( (strcmp( cpt_name, cpt_class ) == 0)
		&&  (*--cpt_name == '-')  )
		{
		  /* Again, return the "component", if this identifies
		   * a successful match...
		   */
		  *cpt_name = '\0';
		  if( (strcmp( name, pkg_name ) == 0) || has_keyword( name, alias ) )
		    return cpt;

		  /* Otherwise, restore the original content of the
		   * working copy of the "lookup name", in preperation
		   * for any subsequent attempt to find a match.
		   */
		  *cpt_name = '-';
		}
	      }
	    }

	    /* ...otherwise, continue checking any other "components"
	     * which may be defined within the current "package.
	     */
	    cpt = cpt->GetNext();
	  }
	}

	/* ...otherwise, continue searching among any further
	 * entries in the current "package-collection"...
	 */
	pkg = pkg->GetNext();
      }
    }
    /* ...and ultimately, in any further "package-collection" elements
     * which may be present.
     */
    dir = dir->GetNext();
  }

  /* If we get to here, we didn't find the required "package";
   * return NULL, to indicate failure.
   */
  return NULL;
}

static
pkgXmlNode* pkgFindNextAssociate( pkgXmlNode* pkg, const char* tagname )
{
  /* Core implementation for both pkgXmlNode::FindFirstAssociate
   * and pkgXmlNode::FindNextAssociate methods.  This helper starts
   * at the node specified by "pkg", examining it, and if necessary,
   * each of its siblings in turn, until one of an element type
   * matching "tagname" is found.
   */
  while( pkg != NULL )
  {
    /* We still have this "pkg" node, not yet examined...
     */
    if( pkg->IsElementOfType( tagname ) )
      /*
       * ...it matches our search criterion; return it...
       */
      return pkg;

    /* The current "pkg" node didn't match our criterion;
     * move on, to examine its next sibling, if any...
     */
    pkg = pkg->GetNext();
  }

  /* We ran out of siblings to examine, without finding any
   * to match our criterion; return nothing...
   */
  return NULL;
}

pkgXmlNode*
pkgXmlNode::FindFirstAssociate( const char* tagname )
{
  /* For the node on which this method is invoked,
   * return the first, if any, of its immediate children,
   * which is an element of the type specified by "tagname"...
   */
  return this ? pkgFindNextAssociate( GetChildren(), tagname ) : NULL;
}

pkgXmlNode*
pkgXmlNode::FindNextAssociate( const char* tagname )
{
  /* Invoked on any node returned by "FindFirstAssociate",
   * or on any node already returned by "FindNextAssociate",
   * return the next sibling node, if any, which is an element
   * of the type specified by "tagname"...
   */
  return this ? pkgFindNextAssociate( GetNext(), tagname ) : NULL;
}

/* $RCSfile: pkgfind.cpp,v $: end of file */
