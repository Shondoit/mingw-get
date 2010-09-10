/*
 * clistub.c
 *
 * $Id: clistub.c,v 1.5 2010/09/10 01:44:24 cwilso11 Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, MinGW Project
 *
 *
 * Initiation stub for command line invocation of mingw-get
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
#define _WIN32_WINNT 0x500
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <process.h>
#include <getopt.h>

#define EXIT_FATAL  EXIT_FAILURE + 1

wchar_t *AppPathNameW( const wchar_t *relpath )
{
  /* UTF-16LE implementation; NOT thread safe...
   *
   * Map "relpath" into the file system hierarchy with logical root
   * at the prefix where the application suite is installed, such that
   * it becomes "d:\prefix\relpath".
   *
   * If the application's executables are installed in a directory called
   * "bin" or "sbin", then this final directory is excluded from the mapped
   * prefix, (i.e. a program installed as "d:\prefix\bin\prog.exe" returns
   * the mapped path as "d:\prefix\relpath", rather than as the inclusive
   * path "d:\prefix\bin\relpath"); in any other case, the prefix is taken
   * as the path to the directory in which the program file is installed,
   * (i.e. a program installed as "d:\prefix\foo\prog.exe" returns the
   * mapped path as "d:\prefix\foo\relpath".
   *
   * Mapped path is returned in this static buffer...
   */
  static wchar_t retpath[MAX_PATH], *tail = NULL;

  if( tail == NULL )
  {
    /* First time initialisation...
     *
     * Fill in the static local buffer, with the appropriate "prefix"
     * string, marking the point at which "relpath" is to be appended.
     *
     * On subsequent calls, we reuse this static buffer, leaving the
     * "prefix" element unchanged, but simply overwriting the "relpath"
     * element from any prior call; this is NOT thread safe!
     */
    wchar_t prev = L'\\';
    wchar_t bindir[] = L"bin", *bindir_p = bindir;
    wchar_t *mark, *scan = mark = tail = retpath;

    /* Ascertain the installation path of the calling executable.
     */
    int chk = GetModuleFileNameW( NULL, retpath, MAX_PATH );

    /* Validate it; reject any result which doesn't fit in "retpath".
     */
    if( (chk == 0) || ((chk == MAX_PATH) && (retpath[--chk] != L'\0')) )
      return tail = NULL;

    /* Parse it, to locate the end of the effective "prefix" string...
     */
    do { if( *scan == L'/' )
	   /*
	    * This is a sanity check; it should not be necessary, but...
	    *
	    * Enforce use of "\" rather than "/" as path component separator;
	    * ( "LoadLibrary" may be broken, since it seems to care! )
	    */
	   *scan = L'\\';

	 if( *scan && (prev == L'\\') )
	 {
	   /* We found the start a new path name component directory,
	    * (or maybe the file name, at the end); mark it as a possible
	    * final element of the path name, leaving "tail" pointing to
	    * the previously marked element.
	    */
	   tail = mark;
	   mark = scan;
	 }
       } while( (prev = *scan++) != L'\0' );

    /* When we get to here, "mark" should point to the executable file name,
     * at the end of the path name string, while "tail" should point to the
     * last directory in the installation path; we now check, without regard
     * to case, if this final directory name is "bin" or "sbin"...
     */
    if( (*(scan = tail) == L's') || (*scan == L'S') )
      /*
       * Might be "sbin"; skip the initial "s", and check for "bin"...
       */
      ++scan;

    while( *bindir_p && ((*scan++ | L'\x20') == *bindir_p++) )
      /*
       * ...could still match "bin"...
       */ ;
    if( *bindir_p || (*scan != L'\\') )
      /*
       * No, it didn't match; adjust "tail", so we leave the final
       * directory name as part of "prefix".
       */
      tail = mark;
  }

  if( relpath == NULL )
    /*
     * No "relpath" argument given; simply truncate, to return only
     * the effective "prefix" string...
     */
    *tail = L'\0';

  else
  { wchar_t *append = tail;
    do { /*
	  * Append the specified path to the application's root,
	  * again, taking care to use "\" as the separator character...
	  */
	 *append++ = (*relpath == L'/') ? L'\\' : *relpath;
       } while( *relpath++ && ((append - retpath) < MAX_PATH) );

    if( *--append != L'\0' )
      /*
       * Abort, if we didn't properly terminate the return string.
       */
      return NULL;
  }
  return retpath;
}

extern const char *version_identification;

#define  IMPLEMENT_INITIATION_RITES
#include "rites.c"

int main( int argc, char **argv )
{
  /* Make a note of...
   */
  const char *progname = basename( *argv );	/* ...this program's name    */
  wchar_t *approot;				/* and where it is installed */

  if( argc > 1 )
  {
    /* The user specified arguments on the command line...
     * Interpret any which specify processing options for this application,
     * (these are all specified in GNU `long only' style).
     */
    struct option options[] =
    {
      /* Option Name		  Argument Category    Store To   Return Value
       * ----------------------   ------------------   --------   ------------
       */
      { "version",		  no_argument,	       NULL,	  'V'	       },

      /* This list must be terminated by a null definition...
       */
      { NULL, 0, NULL, 0 }
    };

    int opt, offset;
    while( (opt = getopt_long_only( argc, argv, "V", options, &offset )) != -1 )
      switch( opt )
      {
	case 'V':
	  /* This is a request to display the version of the application;
	   * emit the requisite informational message, and quit.
	   */
	  printf( version_identification );
	  return EXIT_SUCCESS;

	default:
	  /* User specified an invalid or unsupported option...
	   */
	  if( opt != '?' )
	    fprintf( stderr, "%s: option '-%s' not yet supported\n",
		progname, options[offset].name
	      );
	  return EXIT_FAILURE;
      }
  }

  /* Establish the installation path for the mingw-get application...
   */
  if( (approot = AppPathNameW( NULL )) != NULL )
  {
    /* ...and set up the APPROOT environment variable to refer to
     * the associated installation prefix...
     */
    char approot_setup[1 + snprintf( NULL, 0, "APPROOT=%S", approot )];
    snprintf( approot_setup, sizeof( approot_setup ), "APPROOT=%S", approot );
    putenv( approot_setup );
  }

  if( --argc )
  {
    /* The user specified arguments on the command line...
     * we load the supporting DLL into the current process context,
     * then, remaining in command line mode, we jump to its main
     * command line processing routine...
     */
    int lock;
    typedef int (*dll_entry)( int, char ** );
    HMODULE my_dll = LoadLibraryW( AppPathNameW( MINGW_GET_DLL ) );
    dll_entry climain = (dll_entry)(GetProcAddress( my_dll, "climain" ));
    if( climain == NULL )
    {
      /* ...bailing out, on failure to load the DLL.
       */
      fprintf( stderr, "%s: %S: shared library load failed\n", 
	  progname, MINGW_GET_DLL
	);
      return EXIT_FATAL;
    }

    /* We want only one mingw-get process accessing the XML database
     * at any time; attempt to acquire an exclusive access lock...
     */
    if( (lock = pkgInitRites( progname )) >= 0 )
    {
      /* ...and proceed, only if successful.
       *  A non-zero return value indicates that a fatal error occurred.
       */
      int rc = climain( argc, argv );

      /* We must release the mingw-get DLL code, BEFORE we invoke
       * last rites processing, (otherwise the last rites clean-up
       * handler exhibits abnormal behaviour when it is exec'd).
       */
      FreeLibrary( my_dll );
      if (rc == 0)
        return pkgLastRites( lock, progname );
      else
      {
        (void) pkgLastRites( lock, progname );
        return EXIT_FATAL;
      }
    }
    /* If we get to here, then we failed to acquire a lock;
     * we MUST abort!
     */
    return EXIT_FATAL;
  }

  else
  { /* No arguments were specified on the command line...
     * we interpret this as a request to start up in GUI mode...
     */ 
    wchar_t *libexec_path = AppPathNameW( MINGW_GET_GUI );
    char gui_program[1 + snprintf( NULL, 0, "%S", libexec_path )];
    snprintf( gui_program, sizeof( gui_program ), "%S", libexec_path );
    int status = execv( gui_program, (const char* const*)(argv) );

    /* If we get to here, then the GUI could not be started...
     * Issue a diagnostic message, before abnormal termination.
     */
    fprintf( stderr, "%s: %S: unable to start application; status = %d\n",
	progname, MINGW_GET_GUI, status
      );
    return EXIT_FATAL;
  }
}

/* $RCSfile: clistub.c,v $: end of file */
