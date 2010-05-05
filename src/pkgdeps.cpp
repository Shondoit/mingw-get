/*
 * pkgdeps.cpp
 *
 * $Id: pkgdeps.cpp,v 1.3 2010/05/05 20:34:17 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, MinGW Project
 *
 *
 * Implementation of the package dependency resolver method, of the
 * "pkgXmlDocument" class; includes the interface to the action item
 * task scheduler, which is called to ensure that processing for any
 * identified prerequisite packages is appropriately scheduled.
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

#include "dmh.h"

#include "pkginfo.h"
#include "pkgbase.h"
#include "pkgkeys.h"
#include "pkgtask.h"

static bool is_installed( pkgXmlNode *release )
{
  /* Helper to check installation status of a specified package release.
   */
  const char *status;
  /*
   * First, check for any 'installed' attribute which may have been
   * explicitly specified for the 'release' record...
   */
  if( (status = release->GetPropVal( installed_key, NULL )) != NULL )
    /*
     * ...immediately returning the status deduced from it,
     * when present.
     */
    return (strcmp( status, yes_value ) == 0);

  /* When the package definition itself doesn't bear an explicit
   * 'installed' attribute, then we must check the system map for
   * an associated installation record...
   */
  const char *pkgname;
  if( ((pkgname = release->GetPropVal( tarname_key, NULL )) != NULL)
  &&   (release->GetInstallationRecord( pkgname ) != NULL)  )
  {
    /* ...and, when one is found, we can mark this package with
     * an explicit status attribute for future reference, before
     * we return confirmation of the installed status...
     */
    release->SetAttribute( installed_key, yes_value );
    return true;
  }

  /* If we get to here, we may deduce that the package is not
   * installed; once again, we set the explicit attribute value
   * to convey this for future reference, before returning the
   * appropriate status flag.
   */
  release->SetAttribute( installed_key, no_value );
  return false;
}

pkgXmlNode *pkgXmlNode::GetInstallationRecord( const char *pkgname )
{
  /* Retrieve the installation record, if any, for the package
   * specified by fully qualified canonical 'pkgname'.
   *
   * First, break down the specified package name, and retrieve
   * the sysroot database entry for its associated subsystem.
   */
  pkgXmlNode *sysroot;
  pkgSpecs lookup( pkgname );
  if( (sysroot = GetSysRoot( lookup.GetSubSystemName() )) != NULL )
  {
    /* We successfully retrieved a sysroot entry; now we must
     * search the associated list of installed packages, for one
     * with the appropriate canonical package name.
     */
    pkgXmlNode *pkg = sysroot->FindFirstAssociate( installed_key );
    while( pkg != NULL )
    {
      /* We found an installed package entry; check if it has
       * the correct canonical name...
       */
      const char *installed = pkg->GetPropVal( tarname_key, NULL );
      if( (installed != NULL) && (strcmp( installed, pkgname ) == 0) )
	/*
	 * ...returning this entry if so...
	 */
	return pkg;

      /* ...otherwise, move on to the next entry, if any.
       */
      pkg = pkg->FindNextAssociate( installed_key );
    }
  }

  /* If we get to here, we didn't find an entry for the required
   * package; return NULL, indicating that it is not installed.
   */
  return NULL;
}

void
pkgXmlDocument::ResolveDependencies( pkgXmlNode* package, pkgActionItem* rank )
{
  /* For the specified "package", (nominally a "release"), identify its
   * prerequisites, (as specified by "requires" tags), and schedule actions
   * to process them; repeat recursively, to identify further dependencies
   * of such prerequisites, and finally, extend the search to capture
   * additional dependencies common to the containing package group.
   */
  pkgSpecs *refdata = NULL;
  pkgXmlNode *refpkg = package;

  while( package != NULL )
  {
    /* We have a valid XML entity, which may identify dependencies;
     * check if it includes any "requires" specification...
     */
    pkgXmlNode *dep = package->FindFirstAssociate( requires_key );
    while( dep != NULL )
    {
      /* We found a dependency specification...
       * Initially, assume this package is not installed.
       */
      pkgXmlNode *installed = NULL;

      /* To facilitate resolution of "%" version matching wildcards
       * in the requirements specification, we need to parse the version
       * specification for the current dependent package...
       */
      if( refdata == NULL )
      {
	/* ...but we deferred that until we knew for sure that it would
	 * be needed; it is, so parse it now.
	 */
	const char *refname;
	if( (refname = refpkg->GetPropVal( tarname_key, NULL )) != NULL )
	  refdata = new pkgSpecs( refname );
      }

      /* Identify the prerequisite package, from its canonical name...
       */
      pkgActionItem wanted; pkgXmlNode *selected;
      pkgSpecs req( wanted.SetRequirements( dep, refdata ) );
      /*
       * (Both the package name, and subsystem if specified, must match)...
       */
      selected = FindPackageByName( req.GetPackageName(), req.GetSubSystemName() );

      /* When we've identified the appropriate package...
       */
      if( selected != NULL )
      {
	/* ...and, more significantly, the appropriate component package,
	 * where applicable...
	 */
	pkgXmlNode *component;
	const char *reqclass = req.GetComponentClass();
	if( (component = selected->FindFirstAssociate( component_key )) == NULL )
	  /*
	   * ...but if no separate component package exists,
	   * consider the parent package itself, as sole component.
	   */
	  component = selected;

	while( component != NULL )
	{
       	  /* Step through the "releases" of this component package...
	  */
	  pkgXmlNode *required = component->FindFirstAssociate( release_key );
	  while( required != NULL )
	  {
	    /* ...noting if we find one already marked as "installed"...
	    */
	    pkgSpecs tst( required->GetPropVal( tarname_key, NULL ) );
	    if(  is_installed( required )
	    &&  (strcmp( tst.GetComponentClass(), reqclass ) == 0)  )
	      installed = required;

    	    /* ...and identify the most suitable candidate "release"
	     * to satisfy the current dependency...
	     */
	    if( wanted.SelectIfMostRecentFit( required ) == required )
	      selected = component = required;

	    /* ...continuing, until all available "releases"
	     * have been evaluated accordingly.
	     */
	    required = required->FindNextAssociate( release_key );
	  }

	  /* Where multiple component packages do exist,
	   * continue until all have been inspected.
	   */
	  component = component->FindNextAssociate( component_key );
	}
      }

      /* We have now identified the most suitable candidate package,
       * to resolve the current dependency...
       */
      if( installed )
      {
	/* ...this package is already installed, so we may schedule
	 * a resolved dependency match, with no pending action...
	 */
	unsigned long fallback = request & ~ACTION_MASK;
	if( selected != installed )
	{
	  /* ...but, if there is a better candidate than the installed
	   * version, we prefer to schedule an upgrade.
	   */
	  fallback |= ACTION_UPGRADE;
	  wanted.SelectPackage( installed, to_remove );
	}
	rank = Schedule( fallback, wanted, rank );
      }

      else if( (request & ACTION_MASK) == ACTION_INSTALL )
	/*
	 * The required package is not installed...
	 * When performing an installation, we must schedule it
	 * for installation now; (we may simply ignore it, if
	 * we are performing a removal).
	 */
	rank = Schedule( request, wanted, rank );

      /* Regardless of the action scheduled, we must recursively
       * consider further dependencies of the resolved prerequisite;
       * FIXME: do we need to do this, when performing a removal?
       */
      ResolveDependencies( selected, rank );

      /* Continue, until all prerequisites of the current package
       * have been evaluated.
       */
      dep = dep->FindNextAssociate( requires_key );
    }
    /* Also consider any dependencies which may be common to
     * all releases, or all components, of the current package;
     * we do this by walking back through the XML hierarchy,
     * searching for "requires" elements in all containing
     * contexts, until we reach the root element.
     */
    package = package->GetParent();
  }
  delete refdata;
}

void pkgXmlDocument::Schedule( unsigned long action, const char* name )
{
  /* Task scheduler interface; schedules actions to process dependencies
   * for the package specified by "name".
   */
  pkgXmlNode *release;
  if( (release = FindPackageByName( name )) != NULL )
  {
    /* We found the specification for the named package...
     */
    pkgXmlNode *component = release->FindFirstAssociate( component_key );
    if( component != NULL )
      /*
       * When it is subdivided into component-packages,
       * we need to consider each as a possible candidate
       * for task scheduling.
       */
      release = component;

    while( release != NULL )
    {
      /* Within each candidate package or component-package...
       */
      if( (release = release->FindFirstAssociate( release_key )) != NULL )
      {
	/* ...initially assume it is not installed, and that
	 * no installable upgrade is available.
	 */
	pkgActionItem latest;
	pkgXmlNode *installed = NULL, *upgrade = NULL;

	/* Establish the action for which dependency resolution is
	 * to be performed; note that this may be promoted to a more
	 * inclusive class, during resolution, so we need to reset
	 * it for each new dependency which may be encountered.
	 */
	request = action;

	/* For each candidate release in turn...
	 */
	while( release != NULL )
	{
	  /* ...inspect it to identify any which is already installed,
	   * and also the latest available...
	   */
	  if( is_installed( release ) )
	    /*
	     * ...i.e. here we have identified a release
	     * which is currently installed...
	     */
	    latest.SelectPackage( installed = release, to_remove );

	  if( latest.SelectIfMostRecentFit( release ) == release )
	    /*
	     * ...while this is the most recent we have
	     * encountered so far.
	     */
	    upgrade = release;

	  /* Continue with the next specified release, if any.
	   */
	  release = release->FindNextAssociate( release_key );
	}

	if( installed == NULL )
	{
	  /* There is no installed version...
	   * therefore, there is nothing to do for any action
	   * other than ACTION_INSTALL...
	   */
	  if( (action & ACTION_MASK) == ACTION_INSTALL )
	    /*
	     * ...in which case, we must recursively resolve
	     * any dependencies for the scheduled "upgrade".
	     */
	    ResolveDependencies( upgrade, Schedule( action, latest ));
	}

	else if( upgrade && (upgrade != installed) )
	{
	  /* There is an installed version...
	   * but an upgrade to a newer version is available;
	   * ACTION_INSTALL implies ACTION_UPGRADE.
	   */
	  unsigned long fallback = action;
	  if( (action & ACTION_MASK) == ACTION_INSTALL )
	    fallback = ACTION_UPGRADE + (action & ~ACTION_MASK);

	  /* Again, we recursively resolve any dependencies
	   * for the scheduled upgrade.
	   */
	  ResolveDependencies( upgrade, Schedule( fallback, latest ));
	}

	else
	  /* In this case, the package is already installed,
	   * and no more recent release is available; we still
	   * recursively resolve its dependencies, to capture
	   * any potential upgrades for them.
	   */
	  ResolveDependencies( upgrade, Schedule( action, latest ));
      }

      if( (component = component->FindNextAssociate( component_key )) != NULL )
	/*
	 * When evaluating a component-package, we extend our
	 * evaluation, to consider for any further components of
	 * the current package.
	 */
	release = component;
    }
  }

  else
    /* We found no information on the requested package;
     * diagnose as a non-fatal error.
     */
    dmh_notify( DMH_ERROR, "%s: unknown package\n", name );
}

/* $RCSfile: pkgdeps.cpp,v $: end of file */
