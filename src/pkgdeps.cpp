/*
 * pkgdeps.cpp
 *
 * $Id: pkgdeps.cpp,v 1.10 2011/11/04 22:25:10 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, MinGW Project
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
#include "debug.h"

#include "pkginfo.h"
#include "pkgbase.h"
#include "pkgkeys.h"
#include "pkgtask.h"

/* FIXME: the following declaration belongs in a future "pkgmsgs.h"
 * header file, with the function implementation in a separate C or C++
 * messages source file, with appropriate internationalisation...
 */
EXTERN_C const char *pkgMsgUnknownPackage( void );

const char *pkgMsgUnknownPackage( void )
{
  /* FIXME: (see note above); return English language only, for now.
   */
  return "%s: unknown package\n";
}

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

const char *pkgXmlNode::GetContainerAttribute( const char *key, const char *sub )
{
  /* Walk the XML path from current element, back towards the document root,
   * until we find the innermost element which has an attribute matching "key";
   * if such an element is found, return the value of the attribute; if we have
   * traversed the entire path, all the way to the document root, and we have
   * not found any element with the "key" attribute, return "sub".
   */
  pkgXmlNode *pkg = this;
  pkgXmlNode *root = pkg->GetDocumentRoot();
  while( pkg != NULL )
  {
    /* We haven't yet tried to search beyond the document root;
     * try matching "key" to an attribute of the current element...
     */
    const char *retval = pkg->GetPropVal( key, NULL );
    if( retval != NULL )
      /*
       * ...returning its value, if such an attribute is found...
       */
      return retval;

    /* ...otherwise,
     * take a further step back towards the document root.
     */
    pkg = (pkg == root) ? NULL : pkg->GetParent();
  }

  /* If we get to here, then no element with the required "key"
   * attribute could be found; substitute the specified default.
   */
  return sub;
}

DEBUG_INVOKED static int indent = -1;

DEBUG_INVOKED static void
DEBUG_INVOKED show_required( pkgSpecs *req )
DEBUG_INVOKED {
DEBUG_INVOKED   const char *tarname = NULL;
DEBUG_INVOKED   dmh_printf( "%*s require: %s\n", indent, "", req->GetTarName( tarname ) );
DEBUG_INVOKED   free( (void *)(tarname) );
DEBUG_INVOKED }

static inline
bool is_abi_compatible( pkgSpecs *refdata, const char *version )
{
  /* Local helper, used by pkgXmlDocument::ResolveDependencies(),
   * to confirm that the ABI identification number of a selected
   * component package is an exact match to a requirement spec.
   */
  const char *ref_version;
  if( (ref_version = refdata->GetComponentVersion()) == NULL )
    /*
     * Here, confirm that both are unversioned...
     */
    return (version == NULL);

  /* ...otherwise, fall through to check that both bear IDENTICALLY
   * the same ABI version number.
   */
  return ((version != NULL) && (strcmp( version, ref_version ) == 0));
}

void
pkgXmlDocument::ResolveDependencies( pkgXmlNode* package, pkgActionItem* rank )
# define promote( request, action )  (((request) & (~ACTION_MASK)) | action )
{
  /* For the specified "package", (nominally a "release"), identify its
   * prerequisites, (as specified by "requires" tags), and schedule actions
   * to process them; repeat recursively, to identify further dependencies
   * of such prerequisites, and finally, extend the search to capture
   * additional dependencies common to the containing package group.
   */
  pkgSpecs *refdata = NULL;
  pkgXmlNode *refpkg = package;

  DEBUG_INVOKED ++indent;

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
	{
	  DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_DEPENDENCIES ),
	      dmh_printf( "%*s%s: resolve dependencies\n", indent, "", refname )
	    );
	  refdata = new pkgSpecs( refname );
	}
      }

      /* Identify the prerequisite package, from its canonical name...
       */
      pkgActionItem wanted; pkgXmlNode *selected;
      pkgSpecs req( wanted.SetRequirements( dep, refdata ) );
      DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_DEPENDENCIES ),
	  show_required( &req )
	);
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
	pkgXmlNode *component; const char *reqclass;
	if( (reqclass = req.GetComponentClass()) == NULL )
	  reqclass = value_unknown;

	if( (component = selected->FindFirstAssociate( component_key )) == NULL )
	  /*
	   * ...but if no separate component package exists,
	   * consider the parent package itself, as sole component.
	   */
	  component = selected;

	/* At this point, we have no more than a tentative package selection;
	 * it may not provide a component to fit the requirements specification.
	 * Thus, kill the selection, pending reaffirmation...
	 */
	selected = NULL;
	while( component != NULL )
	{
       	  /* ...by stepping through the "releases" of this component package...
	  */
	  pkgXmlNode *required = component->FindFirstAssociate( release_key );
	  while( required != NULL )
	  {
	    /* ...noting if we find one already marked as "installed"...
	    */
	    const char *tstclass;
	    DEBUG_INVOKED const char *already, *viable;
	    pkgSpecs tst( tstclass = required->GetPropVal( tarname_key, NULL ) );
	    DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_DEPENDENCIES ),
		dmh_printf( "%*s  considering: %s", indent, "", tstclass )
	      );
	    if( (tstclass = tst.GetComponentClass()) == NULL )
	      tstclass = value_unknown;

	    DEBUG_INVOKED already = viable = "";
	    if( is_installed( required ) && (strcmp( tstclass, reqclass ) == 0)
	    /*
	     * We found an installed version of the requisite component,
	     * but we ignore it unless it is ABI version compatible with
	     * the version we need; (the intent of ABI versioning is to
	     * accommodate multiple concurrent installations of shared
	     * objects supporting the differing ABI specifications).
	     */
	    &&  is_abi_compatible( &tst, req.GetComponentVersion() )  )
	    {
	      installed = required;
	      DEBUG_INVOKED already = " (already installed)";
	    }
    	    /* ...and identify the most suitable candidate "release"
	     * to satisfy the current dependency...
	     */
	    if( wanted.SelectIfMostRecentFit( required ) == required )
	    {
	      selected = component = required;
	      DEBUG_INVOKED viable = ": viable candidate";
	    }
	    /* ...continuing, until all available "releases"
	     * have been evaluated accordingly.
	     */
	    required = required->FindNextAssociate( release_key );
	    DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_DEPENDENCIES ),
		dmh_printf( "%s%s\n", viable, already )
	      );
	  }

	  /* Where multiple component packages do exist,
	   * continue until all have been inspected.
	   */
	  component = component->FindNextAssociate( component_key );
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
	  if( (selected != NULL) && (selected != installed) )
	  {
	    /* ...but, if there is a better candidate than the installed
	     * version, we prefer to schedule an upgrade.
	     */
	    fallback |= ACTION_UPGRADE;
	    DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_DEPENDENCIES ),
		dmh_printf( "%*s%s: schedule replacement\n", indent, "",
		    installed->GetPropVal( tarname_key, value_unknown )
		  )
	      );
	    wanted.SelectPackage( installed, to_remove );
	  }
	  rank = Schedule( fallback, wanted, rank );
	}

	else if( ((request & ACTION_MASK) == ACTION_INSTALL)
	  /*
	   * The required package is not installed...
	   * When performing an installation, ...
	   */
	|| ((request & (ACTION_PRIMARY | ACTION_INSTALL)) == ACTION_INSTALL) )
	  /*
	   * or when this is a new requirement of a package
	   * which is being upgraded, then we must schedule it
	   * for installation now; (we may simply ignore it, if
	   * we are performing a removal).
	   */
	  rank = Schedule( promote( request, ACTION_INSTALL ), wanted, rank );

	/* Regardless of the action scheduled, we must recursively
	 * consider further dependencies of the resolved prerequisite;
	 * FIXME: do we need to do this, when performing a removal?
	 * Right now, I (KDM) don't think so...
	 */
	if( (request & ACTION_INSTALL) != 0 )
	  ResolveDependencies( selected, rank );
      }

      if( selected == NULL )
      {
	/* No package matching the selection criteria could be found;
	 * report a dependency resolution failure in respect of each
	 * specified criterion...
	 */
	const char *ref, *key[] = { lt_key, le_key, eq_key, ge_key, gt_key };
	const char *requestor = refpkg->GetPropVal( tarname_key, value_unknown );

	dmh_control( DMH_BEGIN_DIGEST );
	dmh_notify( DMH_ERROR, "%s: requires...\n", requestor );
	for( int i = 0; i < sizeof( key ) / sizeof( char* ); i++ )
	  if( (ref = dep->GetPropVal( key[i], NULL )) != NULL )
	  {
	    dmh_notify( DMH_ERROR, "%s: unresolved dependency (type '%s')\n", ref, key[i] );
	    dmh_notify( DMH_ERROR, "%s: cannot identify any providing package\n" );
	  }
	dmh_notify( DMH_ERROR, "please report this to the package maintainer\n" );
	dmh_control( DMH_END_DIGEST );
      }

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
    package = (package == GetRoot()) ? NULL : package->GetParent();
  }
  DEBUG_INVOKED --indent;
  delete refdata;
}

static inline bool assert_unmatched
( const char *ref, const char *val, const char *name, const char *alias )
{
# define if_noref( name )	((name == NULL) || (*name == '\0'))
# define if_match( ref, name )	((name != NULL) && (strcmp( ref, name ) == 0))
# define if_alias( ref, list )	((list != NULL) && has_keyword( ref, list ))

  /* Helper for the following "assert_installed" function; it determines if
   * the reference name specified by "ref" matches either the corresponding
   * field value "val" from a tarname look-up, or in the case of a package
   * name reference, the containing package "name" attribute or any of its
   * specified "alias" names.  The return value is false, in the case of a
   * match, or true when unmatched.
   */
  return (ref == NULL)

    ? /* When "ref" is NULL, then a match requires all specified candidates
       * for matching to also be NULL, or to be pointers to empty strings.
       */
      !( if_noref( val ) && if_noref( name ) && if_noref( alias ))

    : /* Otherwise, when "ref" is not NULL, then a match is identified when
       * any one candidate is found to match.
       */
      !( if_match( ref, val ) || if_match( ref, name ) || if_alias( ref, alias ));
}

static
pkgXmlNode *assert_installed( pkgXmlNode *current, pkgXmlNode *installed )
{
  /* Validation hook for pkgXmlDocument::Schedule(); it checks for
   * possible prior installation of an obsolete version of a current
   * package, (i.e. the package itself is listed in the distribution
   * manifest, but the listing for the installed version has been
   * removed).
   *
   * Note that, by the time this helper is called, an installation
   * may have been identified already, by a release reference which
   * is still present in the distribution manifest; we perform this
   * check, only if no such identification was possible.
   */
  if( current && (installed == NULL) )
  {
    /* This is the specific case where we have selected a current
     * package for processing, but we HAVE NOT been able to identify
     * a prior installation through a distribution manifest reference;
     * thus, we must perform the further check for prior installation
     * of an obsolete version.
     *
     * Starting from the sysroot record for the specified release...
     */
    pkgXmlNode *sysroot; const char *tarname;
    pkgSpecs lookup( current->GetPropVal( tarname_key, NULL ) );
    if( (sysroot = current->GetSysRoot( lookup.GetSubSystemName() )) != NULL )
    {
      /* ...identify the first, if any, package installation record.
       */
      pkgXmlNode *ref = sysroot->FindFirstAssociate( installed_key );
      if( ref != NULL )
      {
	/* When at least one installation record exists,
	 * establish references for the "tarname" fields which
	 * we must match, to identify a prior installation.
	 */
	const char *refname = lookup.GetPackageName();
	const char *cptname = lookup.GetComponentClass();
	const char *version = lookup.GetComponentVersion();

	/* Also identify the formal name for the containing package,
	 * and any aliases by which it may also be known, so that we
	 * may be able to identify a prior installation which may
	 * have borne a deprecated package name.
	 */
	const char *pkgname = current->GetContainerAttribute( name_key );
	const char *alias = current->GetContainerAttribute( alias_key );

	/* For each candidate installation record found...
	 */
	while( ref != NULL )
	{
	  /* ...check if it matches the look-up criteria.
	   */
	  pkgSpecs chk( tarname = ref->GetPropVal( tarname_key, NULL ) );
	  if( assert_unmatched( chk.GetPackageName(), refname, pkgname, alias )
	  ||  assert_unmatched( chk.GetComponentClass(), cptname, NULL, NULL )
	  ||  assert_unmatched( chk.GetComponentVersion(), version, NULL, NULL )  )
	    /*
	     * This candidate isn't a match; try the next, if any...
	     */
	    ref = ref->FindNextAssociate( installed_key );

	  else
	  { /* We found a prior installation of a deprecated version;
	     * back-build a corresponding reference within the associated
	     * package or component-package inventory, in the internal
	     * copy of the distribution manifest...
	     */
	    if( (installed = new pkgXmlNode( release_key )) != NULL )
	    {
	      installed->SetAttribute( tarname_key, tarname );
	      installed->SetAttribute( installed_key, value_yes );
	      if( (ref = current->GetParent()) != NULL )
		installed = ref->AddChild( installed );
	    }
	    /* Having found a prior installation, there is no need to
	     * check any further installation records; force "ref" to
	     * NULL, to inhibit further searching.
	     */
	    ref = NULL;
	  }
	}
      }
    }
  }
  /* However we get to here, we always return the pointer to the installed
   * package entry identified by the dependency resolver, which may, or may
   * not have been modified by this function.
   */
  return installed;
}

void pkgActionItem::ConfirmInstallationStatus()
{
  /* Set the "to_remove" selection in an action item to match the installed
   * package entry, even when the release in question is no longer enumerated
   * in the package catalogue; (used to identify any installed version when
   * compiling a package reference listing).
   */
  selection[to_remove]
    = assert_installed( selection[to_install], selection[to_remove] );
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

	/* Any action request processed here is, by definition,
	 * a request for a primary action; mark it as such.
	 */
	action |= ACTION_PRIMARY;

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

	if( (installed = assert_installed( upgrade, installed )) == NULL )
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

	  else
	  { /* attempting ACTION_UPGRADE or ACTION_REMOVE
	     * is an error; diagnose it.
	     */
	    if( component == NULL )
	      /*
	       * In this case, the user explicitly specified a single
	       * package component, so it's a simple error...
	       */
	      dmh_notify( DMH_ERROR, "%s %s: package is not installed\n",
		  action_name( action & ACTION_MASK ), name
		);
	    else
	    {
	      /* ...but here, the user specified only the package name,
	       * which implicitly applies to all associated components;
	       * since some may be installed, prefer to issue a warning
	       * in respect of any which aren't.
	       */
	      const char *extname = component->GetPropVal( class_key, "" );
	      char full_package_name[2 + strlen( name ) + strlen( extname )];
	      sprintf( full_package_name, *extname ? "%s-%s" : "%s", name, extname );

	      dmh_control( DMH_BEGIN_DIGEST );
	      dmh_notify( DMH_WARNING, "%s %s: request ignored...\n",
		  extname = action_name( action & ACTION_MASK ), full_package_name
		);
	      dmh_notify( DMH_WARNING, "%s: package was not previously installed\n",
		  full_package_name
		);
	      dmh_notify( DMH_WARNING, "%s: it will remain this way until you...\n",
		  full_package_name
		);
	      dmh_notify( DMH_WARNING, "use 'mingw-get install %s' to install it\n",
		  full_package_name
		);
	      dmh_control( DMH_END_DIGEST );
	    }
	  }
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
    dmh_notify( DMH_ERROR, pkgMsgUnknownPackage(), name );
}

void pkgXmlDocument::RescheduleInstalledPackages( unsigned long action )
{
  /* Wrapper function to retrieve the list of all installed packages,
   * passing each entry in turn to the standard task scheduler.  We
   * begin by locating the first sysroot entry in the XML database...
   */
  pkgXmlNode *sysroot = GetRoot()->FindFirstAssociate( sysroot_key );

  /* ...then, while we have sysroots to examine...
   */
  while( sysroot != NULL )
  {
    /* ...we retrieve the first package installation record within
     * the current sysroot data set.
     */
    pkgXmlNode *package = sysroot->FindFirstAssociate( installed_key );

    /* Within each sysroot, until we've retrieved all embedded
     * installation records...
     */
    while( package != NULL )
    {
      /* ...we read the canonical tarname for the package,
       * and when it is appropriately specified...
       */
      const char *tarname = package->GetPropVal( tarname_key, NULL );
      if( tarname != NULL )
      {
	/* ...we decode it, to determine the package name,
	 * subsystem name and component class.
	 */
	pkgSpecs decode( tarname );
	const char *pkgname = decode.GetPackageName();
	const char *sysname = decode.GetSubSystemName();
	const char *cptname = decode.GetComponentClass();

	/* From these three, we need to reconstruct an effective
	 * package name for the scheduler look-up; this reconstruction
	 * is performed using the following formatted buffer.
	 */
	const char *fmt = "%s-%s";
	char refname[3 + strlen( sysname ) + strlen( pkgname ) + strlen( cptname )];
	if( FindPackageByName( pkgname, sysname ) == NULL )
	{
	  /* The package name alone is insufficient for a successful
	   * look-up; assume that the effective package name has been
	   * defined by prefixing the sysroot name.
	   */
	  sprintf( refname, fmt, sysname, pkgname );
	  pkgname = refname;
	}
	if( cptname != NULL )
	{
	  /* A fully qualified logical package name should include
	   * the component class name, abstracted from the canonical
	   * tarname, and appended to the package name.
	   */
	  sprintf( refname, fmt, pkgname, cptname );
	  pkgname = refname;
	}

	/* Having constructed the effective logical package name,
	 * we schedule the requested action on the package...
	 */
	Schedule( action, pkgname );
      }
      /* ...then move on to the next installed package, if any,
       * within the current sysroot data set...
       */
      package = package->FindNextAssociate( installed_key );
    }
    /* ...and ultimately, to the next sysroot, if any, in the
     * XML database.
     */
    sysroot = sysroot->FindNextAssociate( sysroot_key );
  }
}

/* $RCSfile: pkgdeps.cpp,v $: end of file */
