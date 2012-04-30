/*
 * pkgunst.cpp
 *
 * $Id: pkgunst.cpp,v 1.9 2012/04/30 20:10:23 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2011, 2012, MinGW Project
 *
 *
 * Implementation of the primary package removal methods.
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
#include <errno.h>

#include "dmh.h"
#include "debug.h"

#include "pkginfo.h"
#include "pkgkeys.h"
#include "pkgproc.h"
#include "pkgtask.h"

#include "mkpath.h"

#define PKGERR_INVALID_MANIFEST( REASON )  \
  PKGMSG_INVALID_MANIFEST, tarname, REASON

#define PKGMSG_INVALID_MANIFEST  	"%s: invalid manifest; %s\n"
#define PKGMSG_NO_RELEASE_KEY 		"no release key assigned"
#define PKGMSG_RELEASE_KEY_MISMATCH 	"release key mismatch"

static const char *request_key = "request";

static __inline__ __attribute__((__always_inline__))
pkgXmlNode *sysroot_lookup( pkgXmlNode *pkg, const char *tarname )
{
  /* A local helper function, to identify the sysroot association
   * for any package which is to be uninstalled.
   */
  pkgSpecs lookup( tarname );
  return pkg->GetSysRoot( lookup.GetSubSystemName() );
}

static __inline__ __attribute__((__always_inline__))
const char *id_lookup( pkgXmlNode *reftag, const char *fallback )
{
  /* A local convenience function, to retrieve the value of
   * the "id" attribute associated with any pkgXmlNode element.
   */
  return reftag->GetPropVal( id_key, fallback );
}

static __inline__ __attribute__((__always_inline__))
const char *pathname_lookup( pkgXmlNode *reftag, const char *fallback )
{
  /* A local convenience function, to retrieve the value of
   * the "id" attribute associated with any pkgXmlNode element.
   */
  return reftag->GetPropVal( pathname_key, fallback );
}

unsigned long pkgActionItem::SetAuthorities( pkgActionItem *current )
{
  /* Helper method to either grant or revoke authority for removal of
   * any package, either permanently when the user requests that it be
   * removed, or temporarily in preparation for replacement by a newer
   * version, when scheduled for upgrade.
   *
   * This is a multiple pass method, iterating over the entire list
   * of scheduled actions within each pass, until the entire schedule
   * of authorities has been established.
   */
  if( (flags & ACTION_PREFLIGHT) == 0 )
  {
    /* This applies exclusively in the first pass, which is effectively
     * a "preflight" checking pass only.
     */
    while( current != NULL )
    {
      /* Each scheduled action is inspected in turn...
       */
      pkgXmlNode *ref;
      if( ((current->flags & ACTION_REMOVE) != 0)
      &&  ((ref = current->Selection( to_remove )) != NULL)  )
      {
	/* ...and, when it specifies a "remove" action relating to a
	 * package which has been identified as "installed"...
	 */
	const char *tarname = ref->GetPropVal( tarname_key, value_unknown );
	DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_INIT ),
	    dmh_printf( "%s: selected for %s\n",
  	      (tarname = ref->GetPropVal( tarname_key, value_unknown )),
	      (current->flags & ACTION_INSTALL) ? "upgrade" : "removal"
	  ));

	/* ...we identify its associated entry in the installed
	 * package manifest...
	 */
	if( (ref = ref->GetInstallationRecord( tarname )) != NULL )
	{
	  /* ...and then, having confirmed its validity...
	   */
	  DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_INIT ),
    	      dmh_printf( "%s: marked for %s\n",
		ref->GetPropVal( tarname_key, value_unknown ),
  		(current->flags & ACTION_INSTALL) ? "upgrade" : "removal"
	    ));

	  /* ...we mark it as a candidate for removal...
	   */
	  ref->SetAttribute( request_key, action_name( ACTION_REMOVE ) );

	  /* ...and assign a provisional grant of authority
	   * to proceed with the removal...
	   */
	  current->flags |= ACTION_PREFLIGHT;
	}
      }
      /* ...then, we move on to perform the same "preflight" check
       * for the next scheduled action, if any.
       */
      current = current->next;
    }
    /* When we get to here, we have completed the "preflight" checks
     * for all scheduled actions.  The "current" pointer has advanced,
     * but the "this" pointer still refers to the head of the actions
     * list; terminate this "preflight" pass, marking the associated
     * entry to confirm that it has been completed.
     */
    return flags |= ACTION_PREFLIGHT;
  }

  /* We will get to here only in second and subsequent passes, after
   * the initial "preflight" checks have been completed.  For actions
   * which schedule a package removal, (whether explicitly, or as an
   * implicit prerequisite in preparation for upgrade), the initial
   * pass will have provisionally authorised removal; in subsequent
   * passes, we either ratify that authority, or we revoke it in the
   * event that it would break a dependency of another package.
   *
   * FIXME: we need to develop the code to perform the dependency
   * analysis; for the time being, we simply return zero, ratifying
   * authority for all removal requests.
   */
  return 0;
}

pkgXmlNode *pkgManifest::GetSysRootReference( const char *key )
{
  /* Method to verify that a package manifest includes
   * a reference to any sysroot which claims it, returning
   * a pointer to the first such reference found.
   */
  if( (this != NULL) && (manifest != NULL) && (key != NULL) )
  {
    /* We appear to have a valid manifest, and a valid sysroot
     * key to match; locate this manifest's first, (and nominally
     * its only), references section.
     */
    pkgXmlNode *grp = manifest->GetRoot()->FindFirstAssociate( reference_key );
    while( grp != NULL )
    {
      /* Having identified a references section, locate the
       * first sysroot reference contained therein...
       */
      pkgXmlNode *ref = grp->FindFirstAssociate( sysroot_key );
      while( ref != NULL )
      {
	/* ...then retrieve the sysroot ID key value,
	 * for comparison with the requested key...
	 */
	const char *chk;
	if( ((chk = ref->GetPropVal( id_key, NULL )) != NULL)
	&&  (strcmp( chk, key ) == 0)			     )
	  /*
	   * ...returning immediately, if a match is found...
	   */
	  return ref;

	/* ...otherwise, repeat check for any further sysroot
	 * references which may be present...
	 */
	ref = ref->FindNextAssociate( sysroot_key );
      }
      /* ...ultimately extending the search into any further
       * (unlikely) references sections which might be present.
       */
      grp = grp->FindNextAssociate( reference_key );
    }
  }
  /* If we fell through the preceding loop, then the expected reference
   * was not present; return NULL, to report this unexpected result.
   */
  return NULL;
}

void pkgManifest::DetachSysRoot( const char *sysroot )
{
  /* Method to remove all references to a specified sysroot
   * from a package manifest; (note that it would be unusual
   * for a manifest to refer to a given sysroot more than once,
   * but we repeat the request until we are sure no more such
   * references exist...
   */
  pkgXmlNode *ref;
  while( (ref = GetSysRootReference( sysroot )) != NULL )
    /*
     * ...deleting each one we do find, as we go.
     */
    ref->GetParent()->DeleteChild( ref );
}

/* Format string used to construct absolute path names from the
 * "sysroot" relative "pathname" maintained in package manifests;
 * (shared by "pkg_rmdir()" and "pkg_unlink()" functions).
 */
static const char *pkg_path_format = "%s/%s";

static __inline__ __attribute__((__always_inline__))
int pkg_rmdir( const char *sysroot, const char *pathname )
{
  /* Local helper, used to remove directories which become empty
   * during scheduled package removal; "pathname" is specified from
   * the package manifest, and is relative to "sysroot", (thus both
   * must be specified and non-NULL).  Return value is non-zero
   * when the specified directory is successfully removed, or
   * zero otherwise.
   */
  int retval = 0;
  if( (sysroot != NULL) && (pathname != NULL) )
  {
    /* "sysroot" and "pathname" are both specified.  Construct
     * the absolute path name, and attempt to "rmdir" it, setting
     * return value as appropriate; silently ignore failure.
     */
    char fullpath[ mkpath( NULL, sysroot, pathname, NULL ) ];
    mkpath( fullpath, sysroot, pathname, NULL );

    DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_TRANSACTIONS ),
	dmh_printf( "  %s: rmdir\n", fullpath )
      );

    retval = rmdir( fullpath ) == 0;
  }
  return retval;
}

/* We want the following "unlink" function to emulate "rm -f"
 * semantics; thus we need to ensure that each file we attempt
 * to unlink is writeable.  To do this, we call "chmod()" prior
 * to "unlink()"; we need sys/stat.h for the S_IWRITE mode we
 * are required to set.
 */
#include <sys/stat.h>

static __inline__ __attribute__((__always_inline__))
int pkg_unlink( const char *sysroot, const char *pathname )
{
  /* Local helper, used to delete files during scheduled package
   * removal; "pathname" is specified within the package manifest,
   * and is relative to "sysroot", (thus both must be specified and
   * non-NULL).  Return value is non-zero when the specified file
   * is successfully deleted, or zero otherwise.
   */
  int retval = 0;
  if( (sysroot != NULL) && (pathname != NULL) )
  {
    char filepath[ mkpath( NULL, sysroot, pathname, NULL ) ];
    mkpath( filepath, sysroot, pathname, NULL );

    DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_TRANSACTIONS ),
	dmh_printf( "  %s: unlink file\n", filepath )
      );

    chmod( filepath, S_IWRITE );
    if( ((retval = unlink( filepath )) != 0) && (errno != ENOENT) )
      dmh_notify( DMH_WARNING, "%s:unlink failed; %s\n", filepath, strerror( errno ) );
  }
  return retval;
}

EXTERN_C void pkgRemove( pkgActionItem *current )
{
  /* Common handler for all package removal tasks...
   */
  pkgXmlNode *pkg;
  if( ((pkg = current->Selection( to_remove )) != NULL)
  &&  (current->HasAttribute( ACTION_DOWNLOAD_OK ) == ACTION_REMOVE_OK)  )
  {
    /* We've identified a candidate package for removal;
     * first, identify the canonical tarname for the package,
     * and the sysroot with which it is associated.
     */
    const char *tarname = pkg->GetPropVal( tarname_key, value_unknown );
    pkgXmlNode *sysroot = sysroot_lookup( pkg, tarname );

    /* If the package we are about to remove has an associated
     * pre-remove script, now is the time to invoke it...
     */
    pkg->InvokeScript( "pre-remove" );

    /* ...before we proceed to removal of actual package content.
     */
    dmh_printf( " removing %s %s\n", pkg->GetName(), tarname );

    /* Removal of virtual (meta) packages is comparitively simple;
     * identified by having an associated archive name of "none", they
     * have no associated archive file, no installed footprint on disk,
     * and no associated content manifest to process; thus...
     */
    if( ! match_if_explicit( pkg->ArchiveName(), value_none ) )
    {
      /* ...only in the case of packages identified as "real", (which
       * we expect to be in a substantial majority), do we need to refer
       * to any installation manifest, to identify actual disk files to
       * be removed.
       */
      const char *refname;
      pkgManifest inventory( package_key, tarname );
      pkgXmlNode *ref, *manifest = inventory.GetRoot();

      /* Perform some sanity checks on the retrieved manifest...
       */
      if( ((ref = manifest) == NULL)
      ||  ((ref = ref->FindFirstAssociate( release_key )) == NULL)  )
	/*
	 * Mostly "belt-and-braces": this should never happen, because
	 * the manifest constructor should never return a manifest which
	 * lacks a "release" element.  If we see this, there is something
	 * seriously wrong; hopefully we will get a bug report.
	 */
	dmh_notify( DMH_ERROR, PKGERR_INVALID_MANIFEST( PKGMSG_NO_RELEASE_KEY ) );

      else if( ((refname = ref->GetPropVal( tarname_key, NULL )) == NULL)
      ||        (strcmp( tarname, refname ) != 0)			   )
      {
	/* Another "belt-and-braces" case: once again, it should never
	 * happen, because the manifest constructor should never return
	 * a manifest with a "tarname" attribute in the "release" element
	 * which doesn't match the "tarname" requested.
	 */
	dmh_control( DMH_BEGIN_DIGEST );
	dmh_notify( DMH_ERROR, PKGERR_INVALID_MANIFEST( PKGMSG_RELEASE_KEY_MISMATCH ) );
	if( refname != NULL )
	  dmh_notify( DMH_ERROR, "%s: found %s instead\n", tarname, refname );
	dmh_control( DMH_END_DIGEST );
      }

      else if( ref->FindNextAssociate( release_key ) != NULL )
	/*
	 * Yet another "belt-and-braces" case: the constructor should
	 * never create a manifest which does not have exactly one, and
	 * no more than one, "release" element.
	 */
	dmh_notify( DMH_ERROR, PKGERR_INVALID_MANIFEST( "too many release keys" ) );

      else if( ((ref = manifest->FindFirstAssociate( reference_key )) == NULL)
      ||        (ref->FindFirstAssociate( sysroot_key ) == NULL) 		)
	dmh_notify( DMH_ERROR, PKGERR_INVALID_MANIFEST( "no references" ) );

      else
      { /* We have a manifest which we may likely be able to process;
	 * before proceeding, perform a few sanity checks, and report
	 * any anomalies which may be recoverable.
	 */
	const char *sysname = id_lookup( sysroot, NULL );
	if( inventory.GetSysRootReference( sysname ) == NULL )
	{
	  /* This indicates that the manifest file itself is lacking
	   * a reference to the sysroot.  The sysroot record indicates
	   * that such a reference should be present; diagnose the
	   * anomaly, and proceed anyway.
	   *
	   * FIXME: we should probably make this an error condition,
	   * which will suppress processing unless it is overridden by
	   * a user specified option.
	   */
	  dmh_notify( DMH_WARNING,
	      "%s: unreferenced in %s\n", sysname, id_lookup( manifest, value_unknown )
	    );
	}
	/* Now, we've validated the manifest, and confirmed that it
	 * correctly records its association with the current sysroot,
	 * (or we've reported the inconsistency; we may proceed with
	 * removal of the associated files.
	 */
	if( (manifest = manifest->FindFirstAssociate( manifest_key )) != NULL )
	{
	  /* The manifest records file pathnames relative to sysroot;
	   * thus, first identify the pathname prefix which identifies
	   * the absolute locations of the files and directories which
	   * are to be purged; (note that we specify this as a template
	   * for use with mkpath(), rather than as a simple path name,
	   * so that macros--esp. "%R"--may be correctly resolved at
	   * point of use).
	   */
	  const char *refpath = pathname_lookup( sysroot, value_unknown );
	  char syspath[4 + strlen( refpath )]; sprintf( syspath, "%s%%/F", refpath );

	  /* Read the package manifest...
	   */
	  ref = manifest;
	  while( ref != NULL )
	  {
	    /* ...selecting records identifying installed files...
	     */
	    pkgXmlNode *files = ref->FindFirstAssociate( filename_key );
	    while( files != NULL )
	    {
	      /* ...and delete each in turn...
	       */
	      pkg_unlink( syspath, pathname_lookup( files, NULL ) );
	      /*
	       * ...before moving on to the next in the list.
	       */
	      files = files->FindNextAssociate( filename_key );
	    }
	    /* It should not be, but allow for the possibility that
	     * the manifest is subdivided into multiple sections.
	     */
	    ref = ref->FindNextAssociate( manifest_key );
	  }

	  /* Having deleted all files associated with the package,
	   * we attempt to prune any directories, which may have been
	   * created during the installation of this package, from the
	   * file system tree.  We note that we may remove only those
	   * directories which no longer contain any files or other
	   * subdirectories, (i.e. those which are leaf directories
	   * within the file system).  We also note that many of the
	   * directories associated with the package being removed
	   * may also contain files belonging to other packages; thus
	   * we do not consider it to be an error if we are unable to
	   * remove any directory specified in the package manifest.
	   *
	   * Removal of any leaf directory may expose its own parent
	   * as a new leaf, which may then itself become a candidate
	   * for removal; thus we adopt an iterative removal procedure,
	   * restarting with a further iteration after any pass through
	   * the manifest in which any directory is removed.
	   */
	  bool restart;
	  do {
	       /* Process the entire manifest on each iteration;
		* initially assume that no restart will be required.
		*/
	       ref = manifest; restart = false;
	       while( ref != NULL )
	       {
		 /* Select manifest records which specify directories...
		  */
		 pkgXmlNode *dir = ref->FindFirstAssociate( dirname_key );
		 while( dir != NULL )
		 {
		   /* ...attempting to remove each in turn; request a
		    * restart when at least one such attempt succeeds...
		    */
		   restart |= pkg_rmdir( syspath, pathname_lookup( dir, NULL ) );
		   /*
		    * ...then move on to the next record, if any.
		    */
		   dir = dir->FindNextAssociate( dirname_key );
		 }
		 /* As in the case of file removal, allow for the
		  * possibility of a multisectional manifest.
		  */
		 ref = ref->FindNextAssociate( manifest_key );
	       }
	       /* Restart the directory removal process, with a new
		* iteration through the entire manifest, until no more
		* listed directories can be removed.
		*/
	     } while( restart );

	  /* Finally, disassociate the package manifest from the active sysroot;
	   * this will automatically delete the manifest itself, unless it has a
	   * further association with any other sysroot, (e.g. in an alternative
	   * system map).
	   */
	  inventory.DetachSysRoot( sysname );
	}
      }
    }
    /* In the case of both real and virtual packages, the final phase of removal
     * is to expunge the installation record from the associated sysroot element
     * within the system map; (that is, any record of type "installed" contained
     * within the sysroot element referenced by the "sysroot" pointer identified
     * above, with a tarname attribute which matches "tarname").
     */
    pkgXmlNode *expunge, *instrec = sysroot->FindFirstAssociate( installed_key );
    while( (expunge = instrec) != NULL )
    {
      /* Consider each installation record in turn, as a possible candidate for
       * deletion; in any case, always locate the NEXT candidate, BEFORE deleting
       * a matched record, so we don't destroy our point of reference, whence we
       * must continue the search.
       */
      instrec = instrec->FindNextAssociate( installed_key );
      if( strcmp( tarname, expunge->GetPropVal( tarname_key, value_unknown )) == 0 )
      {
	/* The CURRENT candidate matches the "tarname" criterion for deletion;
	 * we may delete it, also marking the sysroot record as "modified", so
	 * that the change will be committed to disk.
	 */
	sysroot->DeleteChild( expunge );
	sysroot->SetAttribute( modified_key, value_yes );
      }
    }
    /* After package removal has been completed, we invoke any
     * post-remove script which may be associated with the package.
     */
    pkg->InvokeScript( "post-remove" );
  }
  else if( (pkg != NULL) && current->HasAttribute( ACTION_DOWNLOAD ) )
  {
    /* This condition arises only when an upgrade has been requested,
     * but the package archive for the new version is not available in
     * the local package cache, and all attempts to download it have
     * been unsuccessful; diagnose, and otherwise ignore it.
     */
    dmh_notify( DMH_WARNING, "not removing installed %s\n", pkg->GetName() );
    dmh_notify( DMH_WARNING, "%s is still installed\n",
	pkg->GetPropVal( tarname_key, value_unknown )
      );
  }
}

/* $RCSfile: pkgunst.cpp,v $: end of file */
