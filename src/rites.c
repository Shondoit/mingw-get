#ifndef BEGIN_RITES_IMPLEMENTATION
/*
 * rites.c
 *
 * $Id: rites.c,v 1.3 2012/02/20 21:17:45 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, MinGW Project
 *
 *
 * Implementation of the main program function for the "lastrites.exe"
 * helper application; also, when included within another compilation
 * module, which pre-defines IMPLEMENT_INITIATION_RITES, it furnishes
 * the complementary "pkgInitRites()" and "pkgLastRites()" functions.
 *
 * The combination of a call to pkgInitRites() at program start-up,
 * followed by eventual termination by means of an "execl()" call to
 * invoke "lastrites.exe", equips "mingw-get" with the capability to
 * work around the MS-Windows limitation which prohibits replacement
 * of the image files for a running application, and so enables the
 * "mingw-get" application to upgrade itself.
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
#include <errno.h>

#define MINGW_GET_EXE	L"mingw-get.exe"
#define MINGW_GET_LCK	L"lock"

/* We wish to define a number of helper functions, which we will prefer
 * to always compile to inline code; this convenience macro may be used
 * to facilitate achievement of this objective.
 */
#define RITES_INLINE	static __inline__ __attribute__((__always_inline__))

#ifndef EOK
 /*
  * Typically, errno.h does not define a code for "no error", but
  * it's convenient for us to have one, so that we may clear "errno"
  * to ensure that we don't inadvertently react to a stale value.
  */
# define EOK		0
#endif

#ifdef IMPLEMENT_INITIATION_RITES
 /*
  * Normally specified by including this source file within another,
  * with an appropriate prior definition.  This indicates intent to
  * provide an inline implementation of either first or second phase
  * initiation rites, (mutually exclusively), depending on whether
  * IMPLEMENT_INITIATION_RITES is defined as PHASE_ONE_RITES, or as
  * PHASE_TWO_RITES; in either case, the associated code is invoked
  * via the locally implemented "invoke_rites()" function.
  */
# define BEGIN_RITES_IMPLEMENTATION	RITES_INLINE void invoke_rites( void )
 /*
  * In this case, since the "invoke_rites()" function has nothing
  * to return, we declare a "do nothing" wrap-up hook.
  */
# define END_RITES_IMPLEMENTATION

#else /* ! defined IMPLEMENT_INITIATION_RITES */
 /*
  * This alternative case is normally achieved by free-standing
  * compilation of rites.c; it implements the "main()" function
  * for the "lastrites.exe" helper program.
  */
# define BEGIN_RITES_IMPLEMENTATION	int main()
 /*
  * In this case, we must return an exit code; we simply assume that,
  * for us. there is no meaningful concept of process failure, so we
  * always report success.
  */
# define END_RITES_IMPLEMENTATION	return EXIT_SUCCESS;
#endif

/* Provide selectors, to discriminate the two distinct classes
 * of initiation rites implementation, which may be specified as
 * the requisite form with IMPLEMENT_INITIATION_RITES.
 */
#define PHASE_ONE_RITES  1
#define PHASE_TWO_RITES  2

#if IMPLEMENT_INITIATION_RITES == PHASE_TWO_RITES
 /*
  * For the second phase initiation rites implementation, the
  * idea is to move the running executable and its associated DLL
  * out of the way, so that we may install upgraded versions while
  * the application is still running.  Thus, the first step is to
  * destroy any previously created backup copies of the running
  * program files which may still exist...
  */
# define first_act( SOURCE, BACKUP )	mingw_get_unlink( (BACKUP) )
 /*
  * ...then, we schedule a potential pending removal of these, by
  * initially renaming them with their designated backup names...
  */
# define final_act( SOURCE, BACKUP )	mingw_get_rename( (SOURCE), (BACKUP) )

#else /* ! PHASE_TWO_RITES */
 /*
  * In this case, we assume that the originally running process
  * may have invoked phase two initiation rites, so moving its own
  * executable and its associated DLL out of the way; we aim to move
  * them back again, by attempting to change the backup names back
  * to their original file names...
  */
# define first_act( SOURCE, BACKUP )	mingw_get_rename( (BACKUP), (SOURCE) )
 /*
  * We expect the preceding "rename" to succeed; if it didn't, then
  * the most probable reason is that an upgrade has been installed,
  * in which case we may remove the obsolete backup versions.
  */
# define final_act( SOURCE, BACKUP )	mingw_get_remove( (BACKUP) )
#endif

RITES_INLINE const char *approot_path( void )
{
  /* Inline helper to identify the root directory path for the running
   * application, (which "mingw-get" passes through the APPROOT variable
   * in the process environment)...
   *
   * Caution: although this is called more than once, DO NOT attempt to
   * optimise getenv() lookup by saving the returned pointer across calls;
   * the environment block may have been moved between calls, which makes
   * the pointer returned from a previous call potentially invalid!
   */
  char *approot;
  return ((approot = getenv( "APPROOT" )) == NULL)

    ? "c:\\mingw\\"	/* default, for failed environment look-up */
    : approot;		/* normal return value */
}

#include "debug.h"

#if DEBUGLEVEL & DEBUG_INHIBIT_RITES_OF_PASSAGE
 /*
  * When debugging, (with rites of passage selected for debugging),
  * then we substitute debug message handlers for the real "rename()"
  * and "unlink()" functions used to implement the rites.
  */
RITES_INLINE int mingw_get_rename( const char *from, const char *to )
{
  /* Inline debugging message handler to report requests to perform
   * the file rename rite, with optional success/failure result.
   */
  fprintf( stderr, "rename: %s to %s\n", from, to );
  errno = (DEBUGLEVEL & DEBUG_FAIL_FILE_RENAME_RITE) ? EEXIST : EOK;
  return (DEBUGLEVEL & DEBUG_FAIL_FILE_RENAME_RITE) ? -1 : 0;
}

RITES_INLINE int mingw_get_unlink( const char *name )
{
  /* Inline debugging message handler to report requests to perform
   * the file "unlink" rite.
   */
  fprintf( stderr, "unlink: %s\n", name );
  return (DEBUGLEVEL & DEBUG_FAIL_FILE_UNLINK_RITE) ? -1 : 0;
}

#else
 /* We are doing it for real...
  * Simply map the "rename" and "unlink" requests through to the
  * appropriate system calls.
  */
# define mingw_get_rename( N1, N2 )	rename( (N1), (N2) )
# define mingw_get_unlink( N1 )    	unlink( (N1) )
#endif

RITES_INLINE void mingw_get_remove( const char *name )
{
  /* Inline helper to perform the "unlink" rite, provided a preceding
   * request, (presumably "rename"), has not failed with EEXIST status.
   */
  if( errno == EEXIST ) mingw_get_unlink( name );
}

RITES_INLINE void perform_rites_of_passage( const wchar_t *name )
{
  /* Local helper function, to perform the required rite of passage
   * for a single specified process image file, as specified by its
   * relative path name within the application directory tree.
   */
  const char *approot = approot_path();

  /* We begin by allocating stack space for the absolute path names
   * for both the original file name and its backup name.
   */
  size_t buflen = snprintf( NULL, 0, "%s%S~", approot, name );
  char normal_name[ buflen ], backup_name[ 1 + buflen ];

  /* Fill out this buffer pair with the requisite path names,
   * noting that the backup name is the same as the original
   * name, with a single tilde appended.
   */
  snprintf( normal_name, buflen, "%s%S", approot, name );
  snprintf( backup_name, ++buflen, "%s~", normal_name );

  /* Clear any pre-existing error condition, then perform the
   * requisite "rename" and "unlink" rites, in the pre-scheduled
   * order appropriate to the build context.
   */
  errno = EOK;
  first_act( normal_name, backup_name );
  final_act( normal_name, backup_name );
}

/* Here, we specify the variant portion of the implementation...
 */
BEGIN_RITES_IMPLEMENTATION
{
  /* ...where the requisite "rites of passage" are initiated
   * for each process image file affected, specifying each by
   * its path name relative to the root of the application's
   * installation directory tree.
   */
  perform_rites_of_passage( MINGW_GET_EXE );
  END_RITES_IMPLEMENTATION
}

#if IMPLEMENT_INITIATION_RITES == PHASE_ONE_RITES
/*
 * The following inline functions are required, specifically
 * and exclusively, for the first phase of initiation rites...
 */
# include <process.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>

RITES_INLINE const char *lockfile_name( void )
{
  /* Helper to identify the absolute path for the lock file...
   */
  static char *lockfile = NULL;

  if( lockfile == NULL )
  {
    /* We resolve this only once; this is the first reference,
     * (or all prior references were unsuccessfully resolved), so
     * we must resolve it now.
     */
    const char *lockpath = approot_path();
    const wchar_t *lockname = MINGW_GET_LCK;
    size_t wanted = 1 + snprintf( NULL, 0, "%s%S", lockpath, lockname );
    if( (lockfile = malloc( wanted )) != NULL )
      snprintf( lockfile, wanted, "%s%S", lockpath, lockname );
  }
  /* In any case, we return the pointer as resolved on first call.
   */
  return lockfile;
}

/* Provide a facility for clearing a stale lock; for Win32, we may
 * simply refer this to the "unlink()" function, because the system
 * will not permit us to unlink a lock file which is owned by any
 * active process; (i.e. it is NOT a stale lock).
 *
 * FIXME: This will NOT work on Linux (or other Unixes), where it
 *        IS permitted to unlink an active lock file; to support
 *        such systems, we will need to provide a more robust
 *        implementation for "unlink_if_stale()".
 */
#define unlink_if_stale  mingw_get_unlink

RITES_INLINE int pkgInitRites( const char *progname )
{
  /* Helper to acquire an exclusive execution lock, and if sucessful,
   * to establish pre-conditions to permit self-upgrade.
   */
  int lock;
  const char *lockfile;

  /* First, attempt to clear any prior (stale) lock, then create
   * a new one.  (Note that we DON'T use O_TEMPORARY here; on Win2K,
   * it leads to strange behaviour when another process attempts to
   * unlink a stale lock file).
   */
  unlink_if_stale( lockfile = lockfile_name() );
  if( (lock = open( lockfile, O_RDWR | O_CREAT | O_EXCL, S_IWRITE )) < 0 )
  {
    /* We failed to acquire the lock; diagnose failure...
     */
    fprintf( stderr, "%s: cannot acquire lock for exclusive execution\n",
	progname
      );
    fprintf( stderr, "%s: ", progname ); perror( lockfile );
    if( errno == EEXIST )
      fprintf( stderr, "%s: another mingw-get process appears to be running\n",
	  progname
	);
  }

  /* Return the lock, indicating success or failure as appropriate.
   */
  return lock;
}

RITES_INLINE int pkgLastRites( int lock, const char *progname )
{
  /* Inline helper to clear the lock acquired by "pkgInitRites()",
   * and to initiate clean-up of the changes made by "invoke_rites()"
   * when it is invoked in second phase of initiation rites.
   */
  const char *approot = approot_path();
  const char *lastrites = "lastrites.exe";
  char rites[1 + snprintf( NULL, 0, "%s%s", approot, lastrites )];

  /* Clear the lock; note that we must both close AND unlink the
   * lock file, because we didn't open it as O_TEMPORARY.
   */
  close( lock );
  unlink( lockfile_name() );

  /* Initiate clean-up; we hand this off to a free-standing process,
   * so that it may delete the old EXE and DLL image files belonging to
   * this process, if they were upgraded since acquiring the lock.
   *
   * Note that we use the execl() function to invoke the clean-up
   * process.  However, we recognise that Microsoft's implementation
   * of this function does NOT behave in a POSIXly correct manner;
   * specifically, it returns control immediately to the calling
   * process, causing it to resume execution concurrently with the
   * exec()ed process, whereas POSIXly correct behaviour would cause
   * the calling process to wait for the exec()ed process.  This
   * lack of POSIX-like behaviour is unfortunate, since it results
   * in a potential race condition between the exec()ed process and
   * the calling process, should the latter immediately attempt to
   * invoke a new instance of mingw-get.  To mitigate this potential
   * race condition, we call the "invoke_rites()" function to pre-empt
   * as much as possible of the processing to be performed by the
   * clean-up program, recognising that we can be only partially
   * successful, (but silently ignoring the partial failure), before
   * calling execl() to complete those clean-up aspects which cannot
   * be successfully performed in this pre-emptive fashion.
   */
  snprintf( rites, sizeof( rites ), "%s%s", approot, lastrites );
  invoke_rites();
}

#endif

#endif /* BEGIN_RITES_IMPLEMENTATION: $RCSfile: rites.c,v $: end of file */
