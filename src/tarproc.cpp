/*
 * tarproc.cpp
 *
 * $Id: tarproc.cpp,v 1.1 2010/02/02 20:19:28 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, MinGW Project
 *
 *
 * Implementation of package archive processing methods, for reading
 * and extracting content from tar archives.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "dmh.h"
#include "mkpath.h"

#include "pkginfo.h"
#include "pkgkeys.h"
#include "pkgproc.h"

pkgTarArchiveProcessor::pkgTarArchiveProcessor( pkgXmlNode *pkg )
{
  /* Constructor to associate a package tar archive with its
   * nominated sysroot and respective installation directory path,
   * and prepare it for processing, using an appropriate streaming
   * decompression filter; (choice of filter is based on archive
   * file name extension; file names are restricted to the
   * POSIX Portable Character Set).
   *
   * First, we anticipate an invalid initialisation state...
   */
  sysroot = NULL;
  sysroot_path = NULL;
  stream = NULL;

  /* The 'pkg' XML database entry must be non-NULL, must
   * represent a package release, and must specify a canonical
   * tarname to identify the package...
   */
  if(  (pkg != NULL) && pkg->IsElementOfType( release_key )
  &&  ((tarname = pkg->GetPropVal( tarname_key, NULL )) != NULL)  )
  {
    /* When these pre-conditions are satisfied, we may proceed
     * to identify and locate the sysroot record with which this
     * package is to be associated...
     */
    pkgSpecs lookup( pkgfile = tarname );
    if( (sysroot = pkg->GetSysRoot( lookup.GetSubSystemName() )) != NULL )
    {
      /* Having located the requisite sysroot record, we may
       * retrieve its specified installation path prefix...
       */
      const char *prefix;
      if( (prefix = sysroot->GetPropVal( pathname_key, NULL )) != NULL )
      {
	/* ...and incorporate it into a formatting template
	 * for use in deriving the full path names for files
	 * which are installed from this package.
	 */
	const char *template_format = "%F%%/M/%%F";
	char template_text[mkpath( NULL, template_format, prefix, NULL )];
	mkpath( template_text, template_format, prefix, NULL );
	sysroot_path = strdup( template_text );
      }
    }
    /* Some older packages don't use the canonical tarname
     * for the archive file name; identify the real file name
     * associated with such packages...
     */
    pkgfile = pkg->ArchiveName();

    /* Finally, initialise the data stream which we will use
     * for reading the package content.
     */
    const char *archive_path_template = pkgArchivePath();
    char archive_path_name[mkpath( NULL, archive_path_template, pkgfile, NULL )];
    mkpath( archive_path_name, archive_path_template, pkgfile, NULL );
    stream = pkgOpenArchiveStream( archive_path_name );
  }
}

pkgTarArchiveProcessor::~pkgTarArchiveProcessor()
{
  /* Destructor must release the heap memory allocated in the
   * constructor, (by strdup), clean up the decompression filter
   * state, and close the archive data stream.
   */
  free( (void *)(sysroot_path) );
  delete stream;
}

int pkgTarArchiveProcessor::ProcessLinkedEntity( const char *pathname )
{
  /* FIXME: Win32 links need special handling; for hard links, we
   * may be able to create them directly, with >= Win2K and NTFS;
   * for symlinks on *all* Win32 variants, and for hard links on
   * FAT32 or Win9x, we need to make physical copies of the source
   * file, at the link target location.
   *
   * For now, we simply ignore links.
   */
  dmh_printf(
      "FIXME:ProcessLinkedEntity<stub>:Ignoring link: %s --> %s\n",
       pathname, header.field.linkname
    );
  return 0;
}

static
uint64_t compute_octval( const char *p, size_t len )
# define octval( FIELD ) compute_octval( FIELD, sizeof( FIELD ) )
{
  /* Helper to convert the ASCII representation of octal values,
   * (as recorded within tar archive header fields), to their actual
   * numeric values, ignoring leading or trailing garbage.
   */
  uint64_t value = 0LL;

  while( (len > 0) && ((*p < '0') || (*p > '7')) )
  {
    /* Step over leading garbage.
     */
    ++p; --len;
  }
  while( (len > 0) && (*p >= '0') && (*p < '8') )
  {
    /* Accumulate octal digits; (each represents exactly three
     * bits in the accumulated value), until we either exhaust
     * the width of the field, or we encounter trailing junk.
     */
    value = (value << 3) + *p++ - '0'; --len;
  }
  return value;
}

int pkgTarArchiveProcessor::GetArchiveEntry()
{
  /* Read header for next available entry in the tar archive;
   * check for end-of-archive mark, (all zero header); verify
   * checksum for active entry.
   */
  char *buf = header.aggregate;
  size_t count = stream->Read( buf, sizeof( header ) );

  if( count < sizeof( header ) )
  {
    /* Failed to read a complete header; return error code.
     */
    return -1;
  }

  while( count-- )
    /*
     * Outer loop checks for an all zero header...
     */
    if( *buf++ != '\0' )
    {
      /* Any non-zero byte transfers control to an inner loop,
       * to rescan the entire header, accumulating its checksum...
       */
      uint64_t sum = 0;
      for( buf = header.aggregate, count = sizeof( header ); count--; ++buf )
      {
	if( (buf < header.field.chksum) || (buf >= header.field.typeflag) )
	  /*
	   * ...counting the actual binary value of each byte,
	   * in all but the checksum field itself...
	   */
	  sum += *buf;
	else
	  /* ...while treating each byte within the checksum field as
	   * having an effective value equivalent to ASCII <space>.
	   */
	  sum += 0x20;
      }
      /* After computing the checksum for a non-zero header,
       * verify it against the value recorded in the checksum field;
       * return +1 for a successful match, or -2 for failure.
       */
      return (sum == octval( header.field.chksum )) ? 1 : -2;
    }

  /* If we get to here, then the inner loop was never entered;
   * the outer loop has completed, confirming an all zero header;
   * return zero, to indicate end of archive.
   */
  return 0;
}

int pkgTarArchiveProcessor::Process()
{
  /* Generic method for reading tar archives, and extracting their
   * content; loops over each archive entry in turn...
   */
  while( GetArchiveEntry() > 0 )
  {
    /* Found an archive entry; map it to an equivalent file system
     * path name, within the designated sysroot hierarchy.
     */
    char *prefix = *header.field.prefix ? header.field.prefix : NULL;
    char pathname[mkpath( NULL, sysroot_path, header.field.name, prefix )];
    mkpath( pathname, sysroot_path, header.field.name, prefix );

    /* Direct further processing to the appropriate handler; (this
     * is specific to the archive entry classification)...
     */
    switch( *header.field.typeflag )
    {
      int status;
      
      case TAR_ENTITY_TYPE_DIRECTORY:
        /*
	 * We may need to take some action in respect of directories;
	 * e.g. we may need to create a directory, or even a sequence
	 * of directories, to establish a location within the sysroot
	 * hierarchy...
	 *
	 */
	status = ProcessDirectory( pathname );
	break;

      case TAR_ENTITY_TYPE_LINK:
      case TAR_ENTITY_TYPE_SYMLINK:
	/*
	 * Links ultimately represent file system entities in
	 * our sysroot hierarchy, but we need special processing
	 * to handle them correctly...
	 *
	 */
	status = ProcessLinkedEntity( pathname );
	break;

      case TAR_ENTITY_TYPE_FILE:
      case TAR_ENTITY_TYPE_ALTFILE:
	/*
	 * These represent regular files; the file content is
	 * embedded within the archive stream, so we need to be
	 * prepared to read or copy it, as appropriate...
	 *
	 */
	ProcessDataStream( pathname );
	break;

      default:
	/* FIXME: we make no provision for handling any other
	 * type of archive entry; we should provide some more
	 * robust error handling, but for now we simply emit
	 * a diagnostic, and return an error condition code...
	 *
	 */
	dmh_notify( DMH_ERROR,
	    "unexpected archive entry classification: type %d\n",
	    (int)(*header.field.typeflag)
	  );
	return -1;
    }
  }
  /* If we didn't bail out before getting to here, then the archive
   * was processed successfully; return the success code.
   */
  return 0;
}

int pkgTarArchiveProcessor::ProcessEntityData( int fd )
{
  /* Generic method for reading past the data associated with
   * a specific header within a tar archive; if given a negative
   * value for `fd', it will simply skip over the data, otherwise
   * `fd' is assumed to represent a descriptor for an opened file
   * stream, to which the data will be copied (extracted).
   */
   int status = 0;

  /* Initialise a counter for the length of the data content, and
   * specify the default size for the transfer buffer in which to
   * process it; make the initial size of the transfer buffer 16
   * times the header size.
   */
  uint64_t bytes_to_copy = octval( header.field.size );
  size_t block_size = sizeof( header ) << 4;

  /* While we still have unread data, and no processing error...
   */
  while( (bytes_to_copy > 0) && (status == 0) )
  {
    /* Adjust the requested size for the transfer buffer, shrinking
     * it by 50% at each step, until it is smaller than the remaining
     * data length, but never smaller than the header record length.
     */
    while( (bytes_to_copy < block_size) && (block_size > sizeof( header )) )
      block_size >>= 1;

    /* Allocate a transfer buffer of the requested size, and populate
     * it, by reading data from the archive; (since the transfer buffer
     * is never smaller than the header length, this will also capture
     * any additional padding bytes, which may be required to keep the
     * data length equal to an exact multiple of the header length).
     */
    char buffer[block_size];
    if( stream->Read( buffer, block_size ) < (int)(block_size) )
      /*
       * Failure to fully populate the transfer buffer, (i.e. a short
       * read), indicates a corrupt archive; bail out immediately.
       */
      return -1;

    /* When the number of actual data bytes expected is fewer than the
     * total number of bytes in the transfer buffer...
     */
    if( bytes_to_copy < block_size )
      /*
       * ...then we have reached the end of the data for the current
       * archived entity; adjust the block size to reflect the number
       * of actual data bytes present in the transfer buffer...
       */
      block_size = bytes_to_copy;

    /* With the number of actual data bytes present now accurately
     * reflected by the block size, we save that data to the stream
     * specified for archive extraction, (if any).
     */
    if( (fd >= 0) && (write( fd, buffer, block_size ) != (int)(block_size)) )
      /*
       * An extraction error occurred; set the status code to
       * indicate failure.
       */
      status = -2;

    /* Adjust the count of remaining unprocessed data bytes, and begin
     * a new processing cycle, to capture any which may be present.
     */
    bytes_to_copy -= block_size;
  }

  /* Finally, when all data for the current archive entry has been
   * processed, we return to the caller with an appropriate completion
   * status code.
   */
  return status;
}


/* Here, we implement the methods for installing software from
 * packages which are distributed in the form of tar archives.
 *
 */
#include <utime.h>

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
  /* Constructor: having set up the pkgTarArchiveProcessor base class,
   * we add a package installation record to the sysroot entry in the
   * XML database, and mark that sysroot entry as 'modified'.
   */
  if( (tarname != NULL) && (sysroot != NULL) )
  {
    sysroot->SetAttribute( modified_key, yes_value );
    pkgXmlNode *installed = new pkgXmlNode( installed_key );
    installed->SetAttribute( tarname_key, tarname );
    if( pkgfile != tarname )
    {
      pkgXmlNode *download = new pkgXmlNode( download_key );
      download->SetAttribute( tarname_key, pkgfile );
      installed->AddChild( download );
    }
    sysroot->AddChild( installed );
  }
}

int pkgTarArchiveInstaller::ProcessDirectory( const char *pathname )
{
#if DEBUGLEVEL < 5
  int status;

  if( (status = mkdir_recursive( pathname, 0755 )) != 0 )
    dmh_notify( DMH_ERROR, "cannot create directory `%s'\n", pathname );
#else
  int status = 0;

  dmh_printf(
      "FIXME:ProcessDirectory<stub>:not executing: mkdir -p %s\n",
       pathname
    );
#endif
  return status;
}

int pkgTarArchiveInstaller::ProcessDataStream( const char *pathname )
{
#if DEBUGLEVEL < 5
  int fd = set_output_stream( pathname, octval( header.field.mode ) );
  int status = ProcessEntityData( fd );
  if( fd >= 0 )
  {
    close( fd );
    if( status == 0 )
      commit_saved_entity( pathname, octval( header.field.mtime ) );

    else
    {
      unlink( pathname );
      dmh_notify( DMH_ERROR, "%s: extraction failed\n", pathname );
    }
  }
  return status;
#else
  dmh_printf(
      "FIXME:ProcessDataStream<stub>:not extracting: %s\n",
      pathname
    );
  return ProcessEntityData( -1 );
#endif
}

/* $RCSfile: tarproc.cpp,v $: end of file */