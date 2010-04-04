/*
 * tarinst.cpp
 *
 * $Id: tarinst.cpp,v 1.1 2010/04/04 15:25:36 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, MinGW Project
 *
 *
 * Implementation of package installer methods for installation of
 * packages which are distributed in the form of tar archives.
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
#include <utime.h>

#include "dmh.h"

//#include "pkginfo.h"
#include "pkgkeys.h"
#include "pkgproc.h"

static int commit_saved_entity( const char *pathname, time_t mtime )
{
  /* Helper to set the access and modification times for a file,
   * after extraction from an archive, to match the specified "mtime";
   * (typically "mtime" is as recorded within the archive).
   */
  struct utimbuf timestamp;

  timestamp.actime = timestamp.modtime = mtime;
  return utime( pathname, &timestamp );
}

pkgTarArchiveInstaller::
pkgTarArchiveInstaller( pkgXmlNode *pkg ):pkgTarArchiveProcessor( pkg )
{
  /* Constructor: having successfully set up the pkgTarArchiveProcessor
   * base class, we attach a pkgManifest to track the installation.
   */
  if( (tarname != NULL) && (sysroot != NULL) && stream->IsReady() )
    installed = new pkgManifest( package_key, tarname );
}

int pkgTarArchiveInstaller::Process()
{
  /* Specialisation of the base class Process() method.
   */
  int status;
  /* First, process the archive as for the base class...
   */
  if( (status = pkgTarArchiveProcessor::Process()) == 0 )
  {
    /* ...then, on successful completion...
     *
     * Update the package installation manifest, to record
     * the installation in the current sysroot...
     */
    installed->BindSysRoot( sysroot, package_key );
    pkgRegister( sysroot, origin, tarname, pkgfile );
  }
  return status;
}

int pkgTarArchiveInstaller::ProcessDirectory( const char *pathname )
{
  /* Create the directory infrastructure required to support
   * a specific package installation.
   */
#if DEBUGLEVEL < 5
  int status;

  if( (status = mkdir_recursive( pathname, 0755 )) == 0 )
    /*
     * Either the specified directory already exists,
     * or we just successfully created it; attach a reference
     * in the installation manifest for the current package.
     */
    installed->AddEntry( dirname_key, pathname + sysroot_len );

  else
    /* A required subdirectory could not be created;
     * diagnose this failure.
     */
    dmh_notify( DMH_ERROR, "cannot create directory `%s'\n", pathname );

#else
  /* Debugging stub...
   *
   * FIXME:maybe adapt for 'dry-run' or 'verbose' use.
   */
  int status = 0;

  dmh_printf(
      "FIXME:ProcessDirectory<stub>:not executing: mkdir -p %s\n",
       pathname
    );
# if DEBUGLEVEL > 8
  installed->AddEntry( dirname_key, pathname + sysroot_len );
# endif
#endif
  return status;
}

int pkgTarArchiveInstaller::ProcessDataStream( const char *pathname )
{
  /* Extract file data from the archive, and copy it to the
   * associated target file stream, if any.
   */
#if DEBUGLEVEL < 5
  int fd = set_output_stream( pathname, octval( header.field.mode ) );
  int status = ProcessEntityData( fd );
  if( fd >= 0 )
  {
    /* File stream was written; close it...
     */
    close( fd );
    if( status == 0 )
    {
      /* ...and on successful completion, commit it and
       * record it in the installation database.
       */
      commit_saved_entity( pathname, octval( header.field.mtime ) );
      installed->AddEntry( filename_key, pathname + sysroot_len );
    }

    else
    {
      /* The target file was not successfully and completely
       * written; discard it, and diagnose failure.
       */
      unlink( pathname );
      dmh_notify( DMH_ERROR, "%s: extraction failed\n", pathname );
    }
  }
  return status;

#else
  /* Debugging stub...
   *
   * FIXME:maybe adapt for 'dry-run' or 'verbose' use.
   */
  dmh_printf(
      "FIXME:ProcessDataStream<stub>:not extracting: %s\n",
      pathname
    );
# if DEBUGLEVEL > 8
  installed->AddEntry( filename_key, pathname + sysroot_len );
# endif
  return ProcessEntityData( -1 );
#endif
}

/* $RCSfile: tarinst.cpp,v $: end of file */
