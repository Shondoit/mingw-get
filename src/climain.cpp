/*
 * climain.cpp
 *
 * $Id: climain.cpp,v 1.18 2012/05/01 20:01:41 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, 2012, MinGW Project
 *
 *
 * Implementation of the main program function, which is invoked by
 * the command line start-up stub when arguments are supplied; this
 * causes the application to continue running as a CLI process.
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
#include <libgen.h>
#include <string.h>
#include <fcntl.h>

#include "dmh.h"
#include "mkpath.h"

#include "winres.h"

#include "pkgbase.h"
#include "pkgkeys.h"
#include "pkgopts.h"
#include "pkgtask.h"

EXTERN_C void cli_setopts( struct pkgopts *opts )
{
  /* Start-up hook used to make the table of command line options,
   * as parsed by the CLI start-up module, available within the DLL.
   */
  (void) pkgOptions( OPTION_TABLE_ASSIGN, opts );
}

EXTERN_C pkgOpts *pkgOptions( int action, struct pkgopts *ref )
{
  /* Global accessor for the program options data table.
   */
  static pkgOpts *table = NULL;
  if( action == OPTION_TABLE_ASSIGN )
    /*
     * This is a request to initialise the data table reference;
     * it is typically called at program start-up, to record the
     * location of the data table into which the CLI start-up
     * module stores the result of its CLI options parse.
     */
    table = (pkgOpts *)(ref);

  /* In all cases, we return the assigned data table location.
   */
  return table;
}

class pkgArchiveNameList
{
  /* A locally implemented class, managing a LIFO stack of
   * package names; this is used when processing the source
   * and licence requests, to track the packages processed,
   * so that we may avoid inadvertent duplicate processing.
   */
  private:
    const char		*name;	// name of package tracked
    pkgArchiveNameList	*next;	// pointer to next stack entry

  public:
    inline bool NotRecorded( const char *candidate )
    {
      /* Walk the stack of tracked package names, to determine
       * if an entry matching "candidate" is already present;
       * returns false if such an entry exists, otherwise true
       * to indicate that it may be added as a unique entry.
       */
      pkgArchiveNameList *check = this;
      while( check != NULL )
      {
	/* We haven't walked off the bottom of the stack yet...
	 */
	if( strcmp( check->name, candidate ) == 0 )
	{
	  /* ...and the current entry matches the candidate;
	   * thus the candidate will not be stacked, so we
	   * may discard it from the heap.
	   */
	  free( (void *)(candidate) );

	  /* We've found a match, so there is no point in
	   * continuing the search; simply return false to
	   * signal rejection of the candidate.
	   */
	  return false;
	}
	/* No matching entry found yet; walk down to the next
	 * stack entry, if any, to continue the search.
	 */
	check = check->next;
      }
      /* We walked off the bottom of the stack, without finding
       * any match; return true to accept the candidate.
       */
      return true;
    }
    inline pkgArchiveNameList *Record( const char *candidate )
    {
      /* Add a new entry at the top of the stack, to record
       * the processing of an archive named by "candidate";
       * on entry "this" is the current stack pointer, and
       * we return the new stack pointer, referring to the
       * added entry which becomes the new top of stack.
       */
      pkgArchiveNameList *retptr = new pkgArchiveNameList();
      retptr->name = candidate; retptr->next = this;
      return retptr;
    }
    inline ~pkgArchiveNameList()
    {
      /* Completely clear the stack, releasing the heap memory
       * allocated to record the stacked package names.
       */
      free( (void *)(name) );
      delete next;
    }
};

static pkgArchiveNameList *pkgProcessedArchives; // stack pointer

EXTERN_C int climain( int argc, char **argv )
{
  try
  { /* Set up the diagnostic message handler, using the console's
     * `stderr' stream for notifications, and tagging messages with
     * the program basename derived from argv[0]...
     */
    char *dmh_progname;
    char progname[ 1 + strlen( dmh_progname = strdup( basename( *argv++ ))) ];
    dmh_init( DMH_SUBSYSTEM_TTY, strcpy( progname, dmh_progname ) );
    free( dmh_progname );

    /* Interpret the `action keyword', specifying the action to be
     * performed on this invocation...
     */
    int action = action_code( *argv );
    if( action < 0 )
    {
      /* No valid action keyword was found; force an abort
       * through an appropriate DMH_FATAL notification...
       */
      if( *argv == NULL )
	/*
	 * ...viz. the user didn't specify any argument, which
	 * could have been interpreted as an action keyword.
	 */
	dmh_notify( DMH_FATAL, "no action specified\n" );

      else
	/* ...or, the specified action keyword was invalid.
	 */
	dmh_notify( DMH_FATAL, "%s: unknown action keyword\n", *argv );
    }

	/* Create the necessary folders so that we can write files if needed.
	 */
	mkdir("var");
	mkdir("var/lib");
	mkdir("var/lib/mingw-get");
	mkdir("var/lib/mingw-get/data");

    /* If we get to here, then the specified action identifies a
     * valid operation; load the package database, according to the
     * local `profile' configuration, and invoke the operation.
     */
    const char *dfile;
    if( access( dfile = xmlfile_root( profile_key ), R_OK ) != 0 )
    {
      /* The user hasn't provided a custom configuration profile...
       */
      dmh_notify( DMH_WARNING, "%s: user configuration file missing\n", dfile );

      /* ...release the memory allocated by xmlfile_root(), to store its path name,
       * then try the mingw-get distribution default profile instead.
       */
      free( (void *)(dfile) );
      dmh_notify( DMH_INFO, "%s: trying system default configuration\n",
	  dfile = xmlfile_root( defaults_key ) );
      if( access( dfile, R_OK ) != 0 )
      {
        const char *resfile = "profile.xml";
        int buffersize = LoadResData( resfile, NULL, 0 );
        if ( buffersize )
        {
          void *buffer = (void *)(malloc(buffersize));
          if( LoadResData( resfile, buffer, buffersize ) )
          {
            FILE *fpprofile;
            fpprofile = fopen( dfile, "wb" );
            fwrite( buffer, buffersize, 1, fpprofile );
            fclose( fpprofile );
          }
          free( buffer );
        }
      }
    }

    pkgXmlDocument dbase( dfile );
    if( dbase.IsOk() )
    {
      /* We successfully loaded the basic settings...
       * The configuration file name was pushed on to the heap,
       * by xmlfile_root(); we don't need that any more, (because it
       * is reproduced within the database image itself), so
       * free the heap copy, to avoid memory leaks.
       */
      free( (void *)(dfile) );

      /* Merge all package lists, as specified in the "repository"
       * section of the "profile", into the XML database tree...
       */
      if( dbase.BindRepositories( action == ACTION_UPDATE ) == NULL )
	/*
	 * ...bailing out, on an invalid profile specification...
	 */
	dmh_notify( DMH_FATAL, "%s: invalid application profile\n", dbase.Value() );

      /* If the requested action was "update", then we've already done it,
       * as a side effect of binding the cached repository catalogues...
       */
      if( action != ACTION_UPDATE )
      {
	/* ...otherwise, we need to load the system map...
	 */
	dbase.LoadSystemMap();

	/* ...initialise any preferences which the user may
	 * have specified within profile.xml...
	 */
	dbase.EstablishPreferences();

	/* ...and invoke the appropriate action handler.
	 */
	switch( action )
	{
	  case ACTION_LIST:
	  case ACTION_SHOW:
	    /*
	     * "list" and "show" actions are synonymous;
	     * invoke the info-display handler.
	     */
	    dbase.DisplayPackageInfo( argc, argv );
	    break;

	  case ACTION_SOURCE:
	  case ACTION_LICENCE:
	    /*
	     * Process the "source" or "licence" request for one
	     * or more packages; begin with an empty stack of names,
	     * for tracking packages as processed.
	     */
	    pkgProcessedArchives = NULL;
	    if( pkgOptions()->Test( OPTION_ALL_RELATED ) )
	    {
	      /* The "--all-related" option is in effect; ensure
	       * that all dependencies will be evaluated, as if to
	       * perform a recursive reinstall...
	       */
	      pkgOptions()->SetFlags( OPTION_ALL_DEPS );
	      /*
	       * ...then, for each package which is identified on
	       * the command line...
	       */
	      while( --argc )
		/*
		 * ...schedule a request to install the package,
		 * together with all of its runtime dependencies...
		 */
		dbase.Schedule( ACTION_INSTALL, *++argv );

	      /* ...but DON'T proceed with installation; rather
	       * process the "source" or "licence" request for
	       * each scheduled package.
	       */
	      dbase.GetScheduledSourceArchives( (unsigned long)(action) );
	    }
	    else while( --argc )
	      /*
	       * The "--all-related" option is NOT in effect; simply
	       * process the "source" or "licence" request exclusively
	       * in respect of each package named on the command line.
	       */
	      dbase.GetSourceArchive( *++argv, (unsigned long)(action) );

	    /* Finally, clear the stack of processed package names.
	     */
	    delete pkgProcessedArchives;
	    break;

	  case ACTION_UPGRADE:
	    if( argc < 2 )
	      /*
	       * This is a special case of the upgrade request, for which
	       * no explicit package names have been specified; in this case
	       * we retrieve the list of all installed packages, scheduling
	       * each of them for upgrade...
	       */
	      dbase.RescheduleInstalledPackages( ACTION_UPGRADE );

	    /* ...subsequently falling through to complete the action,
	     * using the default processing mechanism; (note that in this
	     * case no further scheduling will be performed, because there
	     * are no additional package names specified in the argv list).
	     */
	  default:
	    /* ...schedule the specified action for each additional command line
	     * argument, (each of which is assumed to represent a package name)...
	     */
	    while( --argc )
	      /*
	       * (Skipped if argv < 2 on entry).
	       */
	      dbase.Schedule( (unsigned long)(action), *++argv );

	    /* ...finally, execute all scheduled actions, and update the
	     * system map accordingly.
	     */
	    dbase.ExecuteActions();
	    dbase.UpdateSystemMap();
	}
      }
      /* If we get this far, then all actions completed successfully;
       * we are done.
       */
      return EXIT_SUCCESS;
    }

    /* If we get to here, then the package database load failed;
     * once more, we force an abort through a DMH_FATAL notification...
     *
     * Note: although dmh_notify does not return, in the DMH_FATAL case,
     * GCC cannot know this, so we pretend that it gives us a return value,
     * to avoid a possible warning about reaching the end of a non-void
     * function without a return value assignment...
     */
    return dmh_notify( DMH_FATAL, "%s: cannot load configuration\n", dfile );
  }

  catch( dmh_exception &e )
  {
    /* An error occurred; it should already have been diagnosed,
     * so simply bail out.
     */
    return EXIT_FAILURE;
  }
}

#include "pkgproc.h"

void pkgActionItem::GetSourceArchive( pkgXmlNode *package, unsigned long category )
{
  /* Handle a 'mingw-get source ...' or a 'mingw-get licence ...' request
   * in respect of the source code or licence archive for a single package.
   */
  const char *src = package->SourceArchiveName( category );
  if( (src != NULL) && pkgProcessedArchives->NotRecorded( src ) )
  {
    if( pkgOptions()->Test( OPTION_PRINT_URIS ) == OPTION_PRINT_URIS )
    {
      /* The --print-uris option is in effect; this is all
       * that we are expected to do.
       */
      PrintURI( src );
    }

    else
    { /* The --print-uris option is not in effect; we must at
       * least check that the source package is available in the
       * source archive cache, and if not, download it...
       */
      const char *path_template; flags |= ACTION_DOWNLOAD;
      DownloadSingleArchive( src, path_template = (category == ACTION_SOURCE)
	  ? pkgSourceArchivePath() : pkgArchivePath()
	);

      /* ...then, unless the --download-only option is in effect...
       */
      if( pkgOptions()->Test( OPTION_DOWNLOAD_ONLY ) != OPTION_DOWNLOAD_ONLY )
      {
	/* ...we establish the current working directory as the
	 * destination where it should be unpacked...
	 */
	char source_archive[mkpath( NULL, path_template, src, NULL )];
	mkpath( source_archive, path_template, src, NULL );

	/* ...and extract the content from the source archive.
	 */
	pkgTarArchiveExtractor unpack( source_archive, "." );
      }
      /* The path_template was allocated on the heap; we are
       * done with it, so release the memory allocation...
       */
      free( (void *)(path_template) );
    }

    /* Record the current archive name as "processed", so we may
     * avoid any inadvertent duplicate processing.
     */
    pkgProcessedArchives = pkgProcessedArchives->Record( src );
  }
}

void pkgActionItem::GetScheduledSourceArchives( unsigned long category )
{
  /* Process "source" or "licence" requests in respect of a list of
   * packages, (scheduled as if for installation); this is the handler
   * for the case when the "--all-related" option is in effect for a
   * "source" or "licence" request.
   */
  if( this != NULL )
  {
    /* The package list is NOT empty; ensure that we begin with
     * a reference to its first entry.
     */
    pkgActionItem *scheduled = this;
    while( scheduled->prev != NULL ) scheduled = scheduled->prev;

    /* For each scheduled list entry...
     */
    while( scheduled != NULL )
    {
      /* ...process the "source" or "licence" request, as appropriate,
       * in respect of the associated package...
       */
      scheduled->GetSourceArchive( scheduled->Selection(), category );
      /*
       * ...then move on to the next entry, if any.
       */
      scheduled = scheduled->next;
    }
  }
}

void pkgXmlDocument::GetSourceArchive( const char *name, unsigned long category )
{
  /* Look up a named package reference in the XML catalogue,
   * then forward it as a pkgActionItem, for processing of an
   * associated "source" or "licence" request.
   */ 
  pkgXmlNode *pkg = FindPackageByName( name );
  if( pkg->IsElementOfType( package_key ) )
  {
    /* We found a top-level specification for the required package...
     */
    pkgXmlNode *component = pkg->FindFirstAssociate( component_key );
    if( component != NULL )
      /*
       * When this package is subdivided into components,
       * then we derive the source reference from the first
       * component defined.
       */
      pkg = component;
  }

  /* Now inspect the "release" specifications within the
   * selected package/component definition...
   */
  if( (pkg = pkg->FindFirstAssociate( release_key )) != NULL )
  {
    /* ...creating a pkgActionItem...
     */
    pkgActionItem latest;
    pkgXmlNode *selected = pkg;

    /* ...and examining each release in turn...
     */
    while( pkg != NULL )
    {
      /* ...select the most recent release, and assign it
       * to the pkgActionItem reference...
       */
      if( latest.SelectIfMostRecentFit( pkg ) == pkg )
	latest.SelectPackage( selected = pkg );

      /* ...continuing until we have examined all available
       * release specifications.
       */
      pkg = pkg->FindNextAssociate( release_key );
    }

    /* Finally, hand off the "source" or "licence" processing
     * request, based on the most recent release selection, to
     * the pkgActionItem we've just instantiated.
     */
    latest.GetSourceArchive( selected, category );
  }
}

/* $RCSfile: climain.cpp,v $: end of file */
