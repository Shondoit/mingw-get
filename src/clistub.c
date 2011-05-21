/*
 * clistub.c
 *
 * $Id: clistub.c,v 1.10 2011/05/21 18:38:11 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, MinGW Project
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

#include "pkgopts.h"

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

static const char *help_text =
"Manage MinGW and MSYS installations (command line user interface).\n\n"
"Usage:\n"
"  mingw-get [OPTIONS] ACTION [package-spec ...]\n\n"
"  mingw-get update\n"
"  mingw-get [OPTIONS] {install | upgrade | remove} package-spec ...\n"
"  mingw-get [OPTIONS] {show | list} [package-spec ...]\n\n"
"Options:\n"
"  --help, -h      Show this help text\n"
"  --version, -V   Show version and licence information\n"
"  --verbose, -v   Increase verbosity of diagnostic or\n"
"                  progress reporting output; repeat up\n"
"                  to three times for maximum verbosity\n"
"  --verbose=N     Set verbosity level to N; (0 <= N <= 3)\n"
#if DEBUGLEVEL & DEBUG_TRACE_DYNAMIC
"  --trace=N       Enable tracing feature N; (debugging aid)\n"
#endif
"\n"
"Actions:\n"
"  update          Update local copy of repository catalogues\n"
"  list, show      List and show details of available packages\n"
"  install         Install new packages\n"
"  upgrade         Upgrade previously installed packages\n"
"  remove          Remove previously installed packages\n\n"
"Package Specifications:\n"
"  [subsystem-]name[-component]:\n"
"  msys-bash-doc   The 'doc' component of the bash package for MSYS\n"
"  mingw32-gdb     All components of the gdb package for MinGW\n\n"
"Use 'mingw-get list' to identify possible package names\n"
"and the components associated with each.\n\n";

#define  IMPLEMENT_INITIATION_RITES	PHASE_ONE_RITES
#include "rites.c"

static __inline__ __attribute__((__always_inline__))
char **cli_setargv( HMODULE my_dll, struct pkgopts *opts, char **argv )
{
  /* A local wrapper function to facilitate passing pre-parsed
   * command line options while performing the "climain()" call
   * into mingw-get-0.dll
   *
   * Note that this requires a version of mingw-get-0.dll which
   * provides the "cli_setopts()" hook...
   */
  typedef void (*dll_hook)(struct pkgopts *);
  dll_hook cli_setopts = (dll_hook)(GetProcAddress( my_dll, "cli_setopts" ));
  if( cli_setopts != NULL )
    /*
     * ...which allows us to pass the pre-parsed options.
     */
    cli_setopts( opts );

  /* In any case, we always return the argument vector which is
   * to be passed in the "climain()" call itself.
   */
  return argv;
}

/* FIXME: We may ultimately choose to factor the following atoi() replacement
 * into a separate, free-standing module.  For now we keep it here, as a static
 * function, but we keep the required #include adjacent.
 */
#include <ctype.h>

static int xatoi( const char *input )
{
  /* A replacement for the standard atoi() function; this implementation
   * supports conversion of octal or hexadecimal representations, in addition
   * to the decimal representation required by standard atoi().
   *
   * We begin by initialising the result accumulator to zero...
   */
  int result = 0;

  /* ...then, provided we have an input string to interpret...
   */
  if( input != NULL )
  {
    /* ...we proceed with interpretation and accumulation of the result,
     * noting that we may have to handle an initial minus sign, (but we
     * don't know yet, so assume that not for now).
     */
    int negate = 0;
    while( isspace( *input ) )
      /*
       * Ignore leading white space.
       */
      ++input;

    if( *input == '-' )
      /*
       * An initial minus sign requires negation
       * of the accumulated result...
       */
      negate = *input++;

    else if( *input == '+' )
      /*
       * ...while an initial plus sign is redundant,
       * and may simply be ignored.
       */
      ++input;

    if( *input == '0' )
    {
      /* An initial zero signifies either hexadecimal
       * or octal representation...
       */
      if( (*++input | '\x20') == 'x' )
	/*
	 * ...with following 'x' or 'X' indicating
	 * the hexadecimal case.
	 */
	while( isxdigit( *++input ) )
	{
	  /* Interpret sequence of hexadecimal digits...
	   */
	  result = (result << 4) + *input;
	  if( *input > 'F' )
	    /*
	     * ...with ASCII to binary conversion for
	     * lower case digits 'a'..'f',...
	     */
	    result -= 'a' - 10;

	  else if( *input > '9' )
	    /*
	     * ...ASCII to binary conversion for
	     * upper case digits 'A'..'F',...
	     */
	    result -= 'A' - 10;

	  else
	    /* ...or ASCII to decimal conversion,
	     * as appropriate.
	     */
	    result -= '0';
	}
      else while( (*input >= '0') && (*input < '8') )
	/*
	 * Interpret sequence of octal digits...
	 */
	result = (result << 3) + *input++ - '0';
    }

    else while( isdigit( *input ) )
      /*
       * Interpret sequence of decimal digits...
       */
      result = (result << 1) + (result << 3) + *input++ - '0';

    if( negate )
      /*
       * We had an initial minus sign, so we must
       * return the negated result...
       */
      return (0 - result);
  }
  /* ...otherwise, we simply return the accumulated result
   */
  return result;
}

#define minval( a, b )  ((a) < (b)) ? (a) : (b)

int main( int argc, char **argv )
{
  /* Make a note of...
   */
  const char *progname = basename( *argv );	/* ...this program's name    */
  wchar_t *approot;				/* and where it is installed */

  /* Provide storage for interpretation of any parsed command line options.
   * Note that we could also initialise them here, but then we would need to
   * give attention to the number of initialisers required; to save us that
   * concern we will defer to an initialisation loop, below.
   */
  struct pkgopts parsed_options;

  if( argc > 1 )
  {
    /* The user specified arguments on the command line...
     * Interpret any which specify processing options for this application,
     * (these are all specified in GNU `long only' style).
     */
    int optref;
    struct option options[] =
    {
      /* Option Name      Argument Category    Store To   Return Value
       * --------------   ------------------   --------   --------------
       */
      { "v",              no_argument,         NULL,      'v'            },
      { "version",        no_argument,         NULL,      'V'            },
      { "help",           no_argument,         NULL,      'h'            },
      { "trace",          required_argument,   &optref,   OPTION_TRACE   },
      { "verbose",        optional_argument,   NULL,      OPTION_VERBOSE },

      /* This list must be terminated by a null definition...
       */
      { NULL, 0, NULL, 0 }
    };

    int opt, offset;
    for( opt = OPTION_FLAGS; opt < OPTION_TABLE_SIZE; ++opt )
      /*
       * Ensure that all entries within the options table are initialised
       * to zero, (equivalent to NULL for pointer entries)...
       */
      parsed_options.flags[opt].numeric = 0;

    while( (opt = getopt_long_only( argc, argv, "Vh", options, &offset )) != -1 )
      /*
       * Parse any user specified options from the command line...
       */
      switch( opt )
      {
	case 'V':
	  /* This is a request to display the version of the application;
	   * emit the requisite informational message, and quit.
	   */
	  printf( version_identification );
	  return EXIT_SUCCESS;

	case 'h':
	  /* This is a request to display help text and quit.
	   */
	  printf( help_text );
	  return EXIT_SUCCESS;

	case OPTION_VERBOSE:
	  /* This is a request to set an explicit verbosity level,
	   * (minimum zero, maximum three), or if no explicit argument
	   * is specified, to increment verbosity as does "-v".
	   */
	  if( optarg != NULL )
	  {
	    /* This is the case where an explicit level was specified...
	     */
	    unsigned verbosity = xatoi( optarg );
	    parsed_options.flags[OPTION_FLAGS].numeric &= ~OPTION_VERBOSE;
	    parsed_options.flags[OPTION_FLAGS].numeric |= minval( verbosity, 3 );
	    break;
	  }

	case 'v':
	  /* This is a request to increment the verbosity level
	   * from its initial zero setting, to a maximum of three.
	   */
	  if( (parsed_options.flags[OPTION_FLAGS].numeric & OPTION_VERBOSE) < 3 )
	    ++parsed_options.flags[OPTION_FLAGS].numeric;
	  break;

	case 0:
	  switch( optref & OPTION_STORAGE_CLASS )
	  {
	    case OPTION_STORE_STRING:
	      parsed_options.flags[optref & 0xfff].string = optarg;
	      break;

	    case OPTION_STORE_NUMBER:
	      parsed_options.flags[optref & 0xfff].numeric = xatoi( optarg );
	      break;

	    case OPTION_MERGE_NUMBER:
	      parsed_options.flags[optref & 0xfff].numeric |= xatoi( optarg );
	      break;
	  }
	  break;

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

  if( argc > 1 )
  {
    /* The user specified arguments on the command line...
     * we load the supporting DLL into the current process context,
     * then, remaining in command line mode, we jump to its main
     * command line processing routine...
     */
    int lock;
    char *argv_base = *argv;
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

    /* Adjust argc and argv to discount parsed options...
     */
    argc -= optind;
    argv += optind - 1;
    /*
     * ...while preserving the original argv[0] reference within
     * the first remaining argument to be passed to climain().
     */
    *argv = argv_base;

    /* We want only one mingw-get process accessing the XML database
     * at any time; attempt to acquire an exclusive access lock...
     */
    if( (lock = pkgInitRites( progname )) >= 0 )
    {
      /* ...and proceed, only if successful.
       *  A non-zero return value indicates that a fatal error occurred.
       */
      int rc = climain( argc, cli_setargv( my_dll, &parsed_options, argv ) );

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
    fprintf( stderr,
	"%s: %S: unable to start GUI; helper program not installed\n",
	progname, MINGW_GET_GUI
      );
    return EXIT_FATAL;
  }
}

/* $RCSfile: clistub.c,v $: end of file */
