/*
 * pkginst.cpp
 *
 * $Id: pkginst.cpp,v 1.4 2012/02/17 23:18:51 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2010, 2011, MinGW Project
 *
 *
 * Implementation of the primary package installation and package
 * manifest recording methods.
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dmh.h"

#include "pkginfo.h"
#include "pkgkeys.h"
#include "pkgproc.h"
#include "pkgtask.h"

EXTERN_C const char *hashed_name( int, const char *, const char * );

pkgManifest::pkgManifest( const char *tag, const char *tarname )
{
  /* Construct an in-memory image for processing a package manifest.
   *
   * We begin by initialising this pair of reference pointers,
   * assuming that this manifest may become invalid...
   */
  manifest = NULL;
  inventory = NULL;

  /* Then we check that a package tarname has been provided...
   */
  if( tarname != NULL )
  {
    /* ...in which case, we proceed to set up the reference data...
     */
    int retry = 0;
    while( retry < 16 )
    {
      /* Generate a hashed signature for the package manifest
       * record, and derive an associated database file path name
       * from it.  Note that this hash is returned in 'malloc'ed
       * memory, which we must later free.  Also note that there
       * are eight possible hashes, to mitigate hash collision,
       * each of which is denoted by retry modulo eight; we make
       * an initial pass through the possible hashes, looking for
       * an existing installation map for this sysroot, loading
       * it immediately if we find it.  Otherwise, we continue
       * with a second cycle, (retry = 8..15), looking for the
       * first generated hash with no associated file; we then
       * use this to create a new installation record file.
       */
      pkgXmlDocument *chkfile;
      const char *signame = hashed_name( retry++, manifest_key, tarname );
      const char *sigfile = xmlfile( signame, NULL );

      /* Check for an existing file associated with the hash value...
       */
      if( (chkfile = new pkgXmlDocument( sigfile ))->IsOk() )
      {
	/* ...such a file does exist, but we must still check
	 * that it relates to the specified package...
	 */
	if( retry < 9 )
	{
	  /* ...however, we only perform this check during the
	   * first pass through the possible hashes; (second time
	   * through, we are only interested in a hash which does
	   * not have an associated file; note that the first pass
	   * through is for retry = 0..7, but by the time we get
	   * to here we have already incremented 7 to become 8,
	   * hence the check for retry < 9).
	   */
	  pkgXmlNode *root, *rel;
	  const char *pkg_id, *pkg_tarname;
	  if( ((root = chkfile->GetRoot()) != NULL)
	  &&  ((root->IsElementOfType( tag )))
	  &&  ((pkg_id = root->GetPropVal( id_key, NULL )) != NULL)
	  &&  ((strcmp( pkg_id, signame ) == 0))
	  &&  ((rel = root->FindFirstAssociate( release_key )) != NULL)
	  &&  ((pkg_tarname = rel->GetPropVal( tarname_key, NULL )) != NULL)
	  &&  ((pkg_strcmp( pkg_tarname, tarname )))  )
	  {
	    /* This is the manifest file we require...
	     * assign it for return, and force an early exit
	     * from the retry loop.
	     */
	    manifest = chkfile;
	    retry = 16;
	  }
	}
      }

      /* Once more noting the prior increment of retry, such
       * that it has now become 8 for the hash generation with
       * retry = 7...
       */
      else if( retry > 8 )
      {
	/* ...we have exhausted all possible hash references,
	 * finding no existing manifest for the specified package;
	 * immediately assign the current (unused) manifest name,
	 * and initialise its root element...
	 */
	pkgXmlNode *root = new pkgXmlNode( tag );
	(manifest = chkfile)->AddDeclaration( "1.0", "UTF-8", value_yes );
	root->SetAttribute( id_key, signame );
	manifest->SetRoot( root );

	/* ...a container, to be filled later, for recording the
	 * data associated with the specific release of the package
	 * to which this manifest will refer...
	 */
	pkgXmlNode *ref = new pkgXmlNode( release_key );
	ref->SetAttribute( tarname_key, tarname );
	root->AddChild( ref );

	/* ...a further container, in which to record the sysroot
	 * associations for installed instances of this package...
	 */
	ref = new pkgXmlNode( reference_key );
	root->AddChild( ref );

	/* ...and one in which to accumulate the content manifest.
	 */
	inventory = new pkgXmlNode( manifest_key );
	root->AddChild( inventory );

	/* Finally, having constructed a skeletal manifest,
	 * force an immediate exit from the retry loop...
	 */
	retry = 16;
      }

      /* Before abandoning our references to the current hash
       * signature, and the path name for the associated XML file,
       * free the memory allocated for them.
       */
      free( (void *)(sigfile) );
      free( (void *)(signame) );

      /* If we have not yet exhausted all possible hashed file names...
       */
      if( retry < 16 )
	/*
	 * ...free the heap memory allocated for the current (unsuitable)
	 * association, so making its "chkfile" reference pointer available
	 * for the next trial, without incurring a memory leak.
	 */
	delete chkfile;
    }
  }
}

void pkgManifest::BindSysRoot( pkgXmlNode *sysroot, const char *reference_tag )
{
  /* Identify the package associated with the current manifest as
   * having been installed within the specified sysroot, by tagging
   * with a manifest reference to the sysroot identification key.
   */
  const char *id;
  if( ((id = sysroot->GetPropVal( id_key, NULL )) != NULL) && (*id != '\0') )
  {
    /* The specified sysroot has a non-NULL, non-blank key;
     * check for a prior reference to the same key, within the
     * "references" section of the manifest...
     */
    pkgXmlNode *map, *ref;
    if(  ((ref = manifest->GetRoot()) != NULL)
    &&   ((ref->IsElementOfType( reference_tag )))
    &&   ((map = ref->FindFirstAssociate( reference_key )) == NULL)  )
    {
      /* This manifest doesn't yet have a "references" section;
       * create and add one now...
       */
      map = new pkgXmlNode( reference_key );
      ref->AddChild( map );
    }

    /* Examine any existing sysroot references within this manifest...
     */
    ref = map->FindFirstAssociate( sysroot_key );
    while( (ref != NULL) && (strcmp( id, ref->GetPropVal( id_key, id )) != 0) )
      /*
       * ...progressing to the next reference, if any, until...
       */
      ref = ref->FindNextAssociate( sysroot_key );

    /* ...we either matched the required sysroot key, or we
     * ran out of existing references, without finding a match.
     */
    if( ref == NULL )
    {
      /* When no existing reference was found, add one.
       */
      ref = new pkgXmlNode( sysroot_key );
      ref->SetAttribute( id_key, id );
      map->AddChild( ref );
    }
  }
}

void pkgManifest::AddEntry( const char *key, const char *pathname )
{
  /* Method invoked by package installers, to add file or directory
   * entries to the tracked inventory of package content.
   *
   * Tracking is enabled only if the manifest structure is valid,
   * AND an inventory table has been allocated...
   */
  if( (this != NULL) && (inventory != NULL) )
  {
    /* ...in which case we allocate a new tracking record, with
     * "dir" or "file" reference key as appropriate, fill it out
     * with the associated path name attribute, and insert it in
     * the inventory table.
     */
    pkgXmlNode *entry = new pkgXmlNode( key );
    entry->SetAttribute( pathname_key, pathname );
    inventory->AddChild( entry );
  }
}

pkgManifest::~pkgManifest()
{
  /* Destructor for package manifest images; it releases
   * the memory used while processing a manifest, after first
   * committing the image to disk storage, or deleting such
   * a disk image as appropriate.
   */
  pkgXmlNode *ref;
  const char *sigfile;

  /* First confirm that an identification signature has been
   * assigned for this manifest...
   */
  if(  ((manifest != NULL)) && ((ref = manifest->GetRoot()) != NULL)
  &&   ((sigfile = ref->GetPropVal( id_key, NULL )) != NULL)          )
  {
    /* ...and map this to a file system reference path name.
     */
    sigfile = xmlfile( sigfile );

    /* Check if any current installation, as identified by
     * its sysroot key, refers to this manifest...
     */
    if(  ((ref = ref->FindFirstAssociate( reference_key )) != NULL)
    &&   ((ref = ref->FindFirstAssociate( sysroot_key )) != NULL)    )
      /*
       * ...and if so, commit this manifest to disk...
       */
      manifest->Save( sigfile );

    else
      /* ...otherwise, this manifest is defunct, so
       * delete any current disk copy.
       */
      unlink( sigfile );

    /* Release the memory used to identify the path name for
     * the on-disk copy of this manifest...
     */
    free( (void *)(sigfile) );
  }

  /* ...and finally, expunge its in-memory image.
   */
  delete manifest;
}

static
void record_dependencies( pkgXmlNode *origin, pkgXmlNode *list )
{
  /* Helper function to record dependency call-outs for the package
   * which is specified by the XML descriptor reference at "origin",
   * (which should nominally represent a "release" specification for
   * the package); the call-out references are collected in the XML
   * container referenced by "list".
   */
  if( (origin != NULL) && (list != NULL) )
  {
    /* Assume "origin" and "list" represent appropriate XML objects...
     */
    if( ! origin->IsElementOfType( package_key ) )
      /*
       * ...and walk back through the "origin" tree, until we locate
       * the top level node in the "package" specification.
       */
      record_dependencies( origin->GetParent(), list );

    /* As we unwind this recursive walk back search, we copy all
     * "requires" elements, at each intervening level, from the top
     * "package" node until we return to the original "release"...
     */
    pkgXmlNode *dep = origin->FindFirstAssociate( requires_key );
    while( dep != NULL )
    {
      /* ...each as a simple clone within the "list" container,
       * (and all at a single level within the "list")...
       */
      list->AddChild( dep->Clone() );

      /* ...repeating for all specified "requires" elements
       * at each level.
       */
      dep = dep->FindNextAssociate( requires_key );
    }
  }
}

EXTERN_C void pkgRegister
( pkgXmlNode *sysroot, pkgXmlNode *origin, const char *tarname, const char *pkgfile )
{
  /* Search the installation records for the current sysroot...
   */
  const char *pkg_tarname = NULL;
  pkgXmlNode *ref = sysroot->FindFirstAssociate( installed_key );
  while(  (ref != NULL)
    &&   ((pkg_tarname = ref->GetPropVal( tarname_key, NULL )) != NULL)
    && !  (pkg_strcmp( tarname, pkg_tarname ))  )
      /*
       * ...continuing until we either run out of installation records,
       * or we find an already existing reference to this package.
       */
      ref = ref->FindNextAssociate( installed_key );

  /* When we didn't find an appropriate existing installation record,
   * we instantiate a new one...
   */
  if( (ref == NULL) && ((ref = new pkgXmlNode( installed_key )) != NULL) )
  {
    /* Fill out its "tarname" attribution...
     */
    ref->SetAttribute( tarname_key, tarname );
    if( pkgfile != tarname )
    {
      /* When the real package tarball name isn't identically
       * the same as the canonical name, then we record the real
       * file name too.
       */
      pkgXmlNode *dl = new pkgXmlNode( download_key );
      dl->SetAttribute( tarname_key, pkgfile );
      ref->AddChild( dl );
    }

    /* Record dependency call-outs for the installed package.
     */
    record_dependencies( origin, ref );

    /* Set the 'modified' flag for, and attach the installation
     * record to, the relevant sysroot record.
     */
    sysroot->SetAttribute( modified_key, value_yes );
    sysroot->AddChild( ref );
  }
}

EXTERN_C void pkgInstall( pkgActionItem *current )
{
  /* Common handler for all package installation tasks...
   */
  pkgXmlNode *pkg;
  if( (pkg = current->Selection()) != NULL )
  {
    /* The current action item has a valid package association...
     */
    if( current->HasAttribute( ACTION_DOWNLOAD ) == 0 )
    {
      /* ...and the required package has been successfully downloaded.
       *
       * FIXME: the notification here is somewhat redundant, but it
       * does maintain symmetry with the "remove" operation, and will
       * make "upgrade" notifications more logical; in any event, it
       * should ultimately be made conditional on a "verbose" mode
       * option selection.
       */
      dmh_printf( " installing %s\n", pkg->GetPropVal( tarname_key, value_unknown ));
      if( current->Selection( to_remove ) == NULL )
      {
	/* The selected package has either not yet been installed,
	 * or any prior installation has been removed in preparation
	 * for re-installation or upgrade.
	 */
	const char *pkgfile, *tarname;

	/* Before proceeding with the installation, we should invoke
	 * any associated pre-install script.
	 */
	pkg->InvokeScript( "pre-install" );

	/* Now, we may proceed with package installation...
	 */
	if(  match_if_explicit( pkgfile = pkg->ArchiveName(), value_none )
	&& ((tarname = pkg->GetPropVal( tarname_key, NULL )) != NULL)       )
	{
	  /* In this case, the selected package has no associated archive,
	   * (i.e. it is a "virtual" package); provided we can identify an
	   * associated "sysroot"...
	   */
	  pkgXmlNode *sysroot;
	  pkgSpecs lookup( tarname );
	  if( (sysroot = pkg->GetSysRoot( lookup.GetSubSystemName() )) != NULL )
	    /*
	     * ...the installation process becomes a simple matter of
	     * recording the state of this virtual package as "installed",
	     * in the sysroot manifest, and itemising its prerequisites.
	     */
	    pkgRegister( sysroot, pkg, tarname, pkgfile );
	}
	else
	{
	  /* Here we have a "real" (physical) package to install;
	   * for the time being, we assume it is packaged in our
	   * standard "tar" archive format.
	   */
	  pkgTarArchiveInstaller install( pkg );
	  if( install.IsOk() )
	    install.Process();
	}
	/* Whether we just installed a virtual package or a real package,
	 * we may now run its post-install script, (if any).
	 */
	pkg->InvokeScript( "post-install" );
      }
      else
	/* There is a prior installation of the selected package, which
	 * prevents us from proceeding; diagnose and otherwise ignore...
	 */
	dmh_notify( DMH_ERROR,
	    "package %s is already installed\n",
	    current->Selection()->GetPropVal( tarname_key, value_unknown )
	  );
    }
    else
    { /* We have a valid package selection, but the required package is
       * not present in the local cache; this indicates that the package
       * has never been successfully downloaded.
       */
      int action = current->HasAttribute( ACTION_MASK );
      dmh_notify( DMH_ERROR, "required package file is not available\n" );
      dmh_notify( DMH_ERROR, "cannot %s%s%s\n", action_name( action ),
	  (action == ACTION_UPGRADE) ? " to " : " ",
	  pkg->GetPropVal( tarname_key, value_unknown )
	);
      dmh_notify( DMH_ERROR, "due to previous download failure\n" );
    }
  }
}

/* $RCSfile: pkginst.cpp,v $: end of file */
