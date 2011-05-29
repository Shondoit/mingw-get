/*
 * sysroot.cpp
 *
 * $Id: sysroot.cpp,v 1.6 2011/05/29 20:53:37 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2010, 2011, MinGW Project
 *
 *
 * Implementation of the system map loader, sysroot management and
 * installation tracking functions.
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
#include <ctype.h>

#ifdef _MAX_PATH
/*
 * Work around a PATH_MAX declaration anomaly in MinGW.
 */
# undef  PATH_MAX
# define PATH_MAX _MAX_PATH
#endif

#include "dmh.h"
#include "mkpath.h"

#include "pkgbase.h"
#include "pkgkeys.h"

#include "debug.h"

EXTERN_C char *hashed_name( int, const char*, const char* );

static bool samepath( const char *tstpath, const char *refpath )
{
  /* Helper to determine equivalence of two path name references.
   *
   * We begin by checking that both of the path name references are
   * actually defined, with non-NULL values.
   */
  if( (tstpath == NULL) || (refpath == NULL) )
    /*
     * If either path is undefined, then they are equivalent only
     * if both are undefined.
     */
    return (tstpath == refpath);

  /* Convert both input path name strings to canonical forms,
   * before comparing them for equivalence.
   */
  char canonical_tstpath[PATH_MAX], canonical_refpath[PATH_MAX];

  if( (_fullpath( canonical_tstpath, tstpath, PATH_MAX) != NULL)
  &&  (_fullpath( canonical_refpath, refpath, PATH_MAX) != NULL)  )
  {
    /* We successfully obtained canonical forms for both paths
     * which are to be compared; we return the result of a case
     * insensitive comparison; (note that it is not necessary to
     * consider '/' vs. '\' directory name separator distinctions
     * here, because all such separators are normalised to '\' in
     * the canonical forms of the path names).
     */
    return stricmp( canonical_tstpath, canonical_refpath ) == 0;
  }

  /* If we get to here, then we failed to get the canonical forms
   * for both input path name strings; fall back to a less reliable
   * comparison of the non-canonical forms, ignoring case and any
   * distinction between '/' and '\' as directory separators...
   */
  while( *tstpath && *refpath )
  {
    /* Hence, comparing character by character, while we have not
     * reached the terminating '\0' on at least one path name string...
     */
    if( ((*tstpath == '/') || (*tstpath == '\\'))
    &&  ((*refpath == '/') || (*refpath == '\\'))  )
    {
      /* Here we simply step over a matched directory separator,
       * regardless of '/' vs. '\' distinction.
       */
      ++tstpath; ++refpath;
    }
    else if( tolower( *tstpath++ ) != tolower( *refpath++ ) )
      /*
       * Here we force a 'false' return, on a case-insensitive
       * MISMATCH between the two path name strings.
       */
      return false;
  }

  /* Finally, if we get to here, we found the '\0' terminator
   * for at least one of the non-canonical path name strings;
   * for equivalence, we must have reached it for both.
   */
  return (*tstpath == *refpath);
}

void pkgXmlDocument::LoadSystemMap()
{
  /* Load an initial, or a replacement, system map into the
   * internal XML database image space.
   */
  pkgXmlNode *dbase = GetRoot();
  pkgXmlNode *sysmap = dbase->FindFirstAssociate( sysmap_key );
  pkgXmlNode *sysroot = dbase->FindFirstAssociate( sysroot_key );

  /* First, we clear out any pre-existing sysroot mappings,
   * which may have been inherited from a previous system map...
   */
  while( sysroot != NULL )
  {
    /* This has the side effect of leaving the sysroot pointer
     * initialised, as required, to NULL, while also locating and
     * deleting the pre-existing database entries.
     */
    pkgXmlNode *to_clear = sysroot;
    sysroot = sysroot->FindNextAssociate( sysroot_key );
    dbase->DeleteChild( to_clear );
  }

  /* Next, we identify the system map to be loaded, by inspection
   * of the profile entries already loaded into the XML database.
   */
  while( sysmap != NULL )
  {
    /* Consider all specified system maps...
     * Any which are not selected for loading are to be purged
     * from the internal XML database image.
     */
    pkgXmlNode *to_clear = sysmap;

    /* Only the first system map which matches the specified selection
     * `id' criterion, and which registers at least one sysroot for which
     * the installation is to be managed, can be loaded...
     */
    if( sysroot == NULL )
    {
      /* We have not yet registered any sysroot for a managed installation;
       * this implies that no system map has yet been selected for loading,
       * so check if the current one matches the selection criterion...
       *
       * FIXME: match_if_explicit( id, NULL ) must always return true;
       * this is a place holder for a match on a system map selector,
       * which will be specified by a future command line option.
       */
      const char *id = sysmap->GetPropVal( id_key, "<default>" );
      if( match_if_explicit( id, NULL ) )
      {
	/* This system map is a candidate for loading;
	 * process all of its subsystem specific sysroot declarations...
	 */
 	DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_INIT ),
	    dmh_printf( "Load system map: id = %s\n", id )
	  );
	pkgXmlNode *subsystem = sysmap->FindFirstAssociate( sysroot_key );
	while( subsystem != NULL )
	{
	  /* ...to identify all unique sysroot path specifications,
	   * (ignoring any for which no path has been specified).
	   */
	  const char *path;
	  if( (path = subsystem->GetPropVal( pathname_key, NULL )) != NULL )
	  {
	    /* This subsystem has a valid sysroot path specification;
	     * check for a prior registration, (i.e. of a common sysroot
	     * which is shared by a preceding subsystem declaration).
	     */
	    sysroot = dbase->FindFirstAssociate( sysroot_key );
	    while( (sysroot != NULL)
	    && ! samepath( path, sysroot->GetPropVal( pathname_key, NULL )) )
	      sysroot = sysroot->FindNextAssociate( sysroot_key );

 	    DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_INIT ),
		dmh_printf( "Bind subsystem %s: sysroot = %s\n",
		  subsystem->GetPropVal( subsystem_key, "<unknown>" ), path
	      ));

	    if( sysroot == NULL )
	    {
	      /* This sysroot has not yet been registered...
	       */
	      int retry = 0;

	      while( retry < 16 )
	      {
		/* Generate a hashed signature for the sysroot installation
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
		const char *sig = hashed_name( retry++, sysroot_key, path );
		const char *sigfile = xmlfile( sig, NULL );

		/* Check for an existing sysroot file associated with the
		 * current hash value...
		 */
		pkgXmlDocument check( sigfile );
		if( check.IsOk() )
		{
		  /* ...such a file does exist, but we must still check
		   * that it relates to the path for the desired sysroot...
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
		    pkgXmlNode *root;
		    if( ((root = check.GetRoot()) != NULL)
		    &&  samepath( root->GetPropVal( pathname_key, NULL ), path )  )
		    {
		      /* This is the sysroot record we require...
		       * Copy its root element into the internal database,
		       * and force an early exit from the retry loop.
		       */
		      dbase->AddChild( root->Clone() );
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
		   * finding no existing mapping database for this sysroot...
		   * The current hashed file name has not yet been assigned,
		   * so create a new entry in the internal XML database,
		   * marking it as "modified", so that it will be written
		   * to disk, when the system map is updated.
		   *
		   * FIXME: perhaps we should not do this arbitrarily for
		   * any non-default system root.
		   */
		  pkgXmlNode *record = new pkgXmlNode( sysroot_key );
		  record->SetAttribute( modified_key, yes_value );
		  record->SetAttribute( id_key, sig );
		  record->SetAttribute( pathname_key, path );
		  dbase->AddChild( record );

		  /* Finally, force termination of the sysroot search.
		   */
		  retry = 16;
		}

		/* Before abandoning our references to the current hash
		 * signature, and the path name for the associated XML file,
		 * free the memory allocated for them.
		 */
		free( (void *)(sigfile) );
		free( (void *)(sig) );
	      }
	    }
	  }

	  /* Repeat, to map the sysroots for any additional subsystems
	   * which may be specified.
	   */
	  subsystem = subsystem->FindNextAssociate( sysroot_key );
	}

	/* Cancel the 'clear' request for the system map we just loaded.
	 */
	to_clear = NULL;
      }
    }

    /* Select the next system map declaration, if any, to be processed;
     * note that we must do this BEFORE we clear the current map, (if it
     * was unused), since the clear action would destroy the contained
     * reference for the next system map element.
     */
    sysmap = sysmap->FindNextAssociate( sysmap_key );

    /* Finally, if the current system map was not loaded...
     */
    if( to_clear != NULL )
      /*
       * ...then we delete its declaration from the active data space.
       */
      dbase->DeleteChild( to_clear );
  }
}

void pkgXmlDocument::UpdateSystemMap()
{
  /* Inspect all sysroot records in the current system map;
   * save copies of any marked with the 'modified' attribute
   * to the appropriate disk files.
   */
  pkgXmlNode *entry = GetRoot()->FindFirstAssociate( sysroot_key );

  while( entry != NULL )
  {
    /* We found a sysroot record...
     * evaluate and clear its 'modified' attribute...
     */
    const char *modified = entry->GetPropVal( modified_key, no_value );
    entry->RemoveAttribute( modified_key );

    if(  (strcmp( modified, yes_value ) == 0)
    &&  ((modified = entry->GetPropVal( id_key, NULL )) != NULL)  )
    {
      /* The 'modified' attribute for this record was set,
       * and the 'id' attribute is valid; establish the path
       * name for the file in which to save the record.
       */
      const char *mapfile = xmlfile( modified, NULL );

      /* Create a copy of the sysroot record, as the content of
       * a new freestanding XML document, and write it out to the
       * nominated record file.
       */
      pkgXmlDocument map;
      map.AddDeclaration( "1.0", "UTF-8", yes_value );
      map.SetRoot( entry->Clone() );
      map.Save( mapfile );

      /* The 'xmlfile()' look-up for the 'mapfile' path name used
       * the heap to return the result; free the space allocated.
       */
      free( (void *)(mapfile) );
    }

    /* Repeat for the next sysroot record, if any...
     */
    entry = entry->FindNextAssociate( sysroot_key );
  }
}

pkgXmlNode* pkgXmlNode::GetSysRoot( const char *subsystem )
{
  /* Retrieve the installation records for the system root associated
   * with the specified software subsystem.
   *
   * Note that, by the time this is called, there should be exactly
   * one system map entry remaining in the internal XML database, so
   * we need only find the first such entry.
   */
  pkgXmlNode *dbase, *sysmap;
  if( ((dbase = GetDocumentRoot()) != NULL)
  &&  ((sysmap = dbase->FindFirstAssociate( sysmap_key )) != NULL)  )
  {
    /* We've located the system map; walk its list of sysroot entries...
     */
    pkgXmlNode *sysroot = sysmap->FindFirstAssociate( sysroot_key );
    while( sysroot != NULL )
    {
      /* ...until we find one for the subsystem of interest...
       */
      if( subsystem_strcmp( subsystem, sysroot->GetPropVal( subsystem_key, NULL )) )
      {
	/* ...from which we retrieve the sysroot path specification...
	 */
	const char *sysroot_path;
	if( (sysroot_path = sysroot->GetPropVal( pathname_key, NULL )) != NULL )
	{
	  /* ...which we then use as an identifying reference...
	   */
	  pkgXmlNode *lookup = dbase->FindFirstAssociate( sysroot_key );
	  while( lookup != NULL )
	  {
	    /* ...to select and return the associated sysroot record
	     * (if any) at the top level in the internal XML database.
	     */
	    if( samepath( sysroot_path, lookup->GetPropVal( pathname_key, NULL )) )
	      return lookup;

	    /* We haven't found the required sysroot record yet...
	     * move on to the next available.
	     */
	    lookup = lookup->FindNextAssociate( sysroot_key );
	  }
	}
      }

      /* We are still looking for a matching sysroot entry in the
       * system map; move on to the next candidate.
       */
      sysroot = sysroot->FindNextAssociate( sysroot_key );
    }
  }

  /* If we get to here, we didn't find any appropriate system root
   * record; return NULL to signal this.
   */
  return NULL;
}

/* $RCSfile: sysroot.cpp,v $: end of file */
