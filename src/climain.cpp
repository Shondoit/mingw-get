/*
 * climain.cpp
 *
 * $Id: climain.cpp,v 1.11 2011/01/05 21:56:42 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, MinGW Project
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

#include "pkgbase.h"
#include "pkgkeys.h"
#include "pkgtask.h"

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

    /* TODO: insert code here, to interpret any OPTIONS specified
     * on the command line.
     */

    /* Interpret the `action keyword', specifying the action to be
     * performed on this invocation...
     */
    int action = action_code( *argv );
    if( action < 0 )
      /*
       * The specified action keyword was invalid;
       * force an abort through a DMH_FATAL notification...
       */
      dmh_notify( DMH_FATAL, "%s: unknown action keyword\n", *argv );

    /* If we get to here, then the specified action identifies a
     * valid operation; load the package database, according to the
     * local `profile' configuration, and invoke the operation.
     */
    const char *dfile;
    if( access( dfile = xmlfile( profile_key ), R_OK ) != 0 )
    {
      /* The user hasn't provided a custom configuration profile...
       */
      dmh_notify( DMH_WARNING, "%s: user configuration file missing\n", dfile );

      /* ...release the memory allocated by xmlfile(), to store its path name,
       * then try the mingw-get distribution default profile instead.
       */
      free( (void *)(dfile) );
      dmh_notify( DMH_INFO, "%s: trying system default configuration\n",
	  dfile = xmlfile( defaults_key ) );
    }

    pkgXmlDocument dbase( dfile );
    if( dbase.IsOk() )
    {
      /* We successfully loaded the basic settings...
       * The configuration file name was pushed on to the heap,
       * by xmlfile(); we don't need that any more, (because it
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

	/* ...and invoke the appropriate action handler.
	 */
	switch( action )
	{
	  case ACTION_LIST:
	  case ACTION_SHOW:
	    dbase.DisplayPackageInfo( argc, argv );
	    break;

	  default:
	    /* ...schedule the specified action for each additional command line
	     * argument, (each of which is assumed to represent a package name)...
	     */
	    while( --argc )
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
    return EXIT_FAILURE;
  }
}

/* $RCSfile: climain.cpp,v $: end of file */
