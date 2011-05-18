/*
 * tarproc.cpp
 *
 * $Id: tarproc.cpp,v 1.10 2011/05/18 18:34:51 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, MinGW Project
 *
 *
 * Implementation of package archive processing methods, for reading
 * and extracting content from tar archives; provides implementations
 * for each of the pkgTarArchiveProcessor and pkgTarArchiveInstaller
 * classes.
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
#include "debug.h"
#include "mkpath.h"

#include "pkginfo.h"
#include "pkgkeys.h"
#include "pkgproc.h"

/*******************
 *
 * Class Implementation: pkgTarArchiveProcessor
 *
 */

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
  sysroot_len = 0;

  sysroot = NULL;
  sysroot_path = NULL;
  installed = NULL;
  stream = NULL;

  /* The 'pkg' XML database entry must be non-NULL, must
   * represent a package release, and must specify a canonical
   * tarname to identify the package...
   */
  if( ((origin = pkg) != NULL) && pkg->IsElementOfType( release_key )
  &&  ((tarname = pkg->GetPropVal( tarname_key, NULL )) != NULL)       )
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
	sysroot_len = mkpath( NULL, template_text, "", NULL ) - 1;
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
  /* Destructor must release the heap memory allocated in
   * the constructor, (by strdup and pkgManifest), clean up
   * the decompression filter state, and close the archive
   * data stream.
   */
  free( (void *)(sysroot_path) );
  delete installed;
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
    char *prefix = *header.field.prefix ? header.field.prefix : NULL;
    char *name = header.field.name;

    /* Handle the GNU long name header format. 
     * If the pathname overflows the name field, GNU tar creates a special
     * entry type, where the data contains the full pathname for the
     * following entry.
     */
    char *longname = NULL;
    if( *header.field.typeflag == TAR_ENTITY_TYPE_GNU_LONGNAME )
    {
      /* Extract the full pathname from the data of this entry.
       */
      longname = EntityDataAsString();
      if( !longname )
        dmh_notify( DMH_ERROR, "Unable to read a long name entry\n" );

      /* Read the entry for which this long name is intended.
       */
      if( GetArchiveEntry() <= 0 )
        dmh_notify( DMH_ERROR, "Expected a new entry after a long name entry\n" );

      /* Use the previously determined long name as the pathname for this entry.
       */
      prefix = NULL;
      name = longname;
    }

    /* Found an archive entry; map it to an equivalent file system
     * path name, within the designated sysroot hierarchy.
     */
    char pathname[mkpath( NULL, sysroot_path, name, prefix )];
    mkpath( pathname, sysroot_path, name, prefix );

    free( longname );

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
	 */
	 { /* Note: Microsoft's implementation of stat() appears to choke
	    * on directory path names with trailing slashes; thus, before
	    * we invoke the directory processing routine, (which may need
	    * to call stat(), to check if the specified directory already
	    * exists), we remove any such trailing slashes.
	    */
	   char *p = pathname + sizeof( pathname ) - 1;
	   while( (p > pathname) && ((*--p == '/') || (*p == '\\')) )
	     *p = '\0';
	 }

	/* We are now ready to process the directory path name entry...
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

char *pkgTarArchiveProcessor::EntityDataAsString()
{
  /* Read the data associated with a specific header within a tar archive
   * and return it as a string.  The return value is stored in memory which
   * is allocated by malloc; it should be freed when no longer required.
   *
   * It is assumed that the return data can be accommodated within available
   * heap memory.  Since the length isn't returned, we assume that the string
   * is NUL-terminated, and that it contains no embedded NULs.
   *
   * In the event of any error, NULL is returned.
   */
  char *data;
  uint64_t bytes_to_copy = octval( header.field.size );
  
  /* Round the buffer size to the smallest multiple of the record size.
   */
  bytes_to_copy += sizeof( header ) - 1;
  bytes_to_copy -= bytes_to_copy % sizeof( header );

  /* Allocate the data buffer.
   */
  data = (char*)(malloc( bytes_to_copy ));
  if( !data )
    return NULL;
  
  /* Read the data into the buffer.
   */
  size_t count = stream->Read( data, bytes_to_copy );
  if( count < bytes_to_copy )
  {
    /* Failure to fully populate the transfer buffer, (i.e. a short
     * read), indicates a corrupt archive.
     */
    free( data );
    return NULL;
  }
  return data;
}

/*******************
 *
 * Class Implementation: pkgTarArchiveInstaller
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
#if DEBUGLEVEL & DEBUG_SUPPRESS_INSTALLATION
  /*
   * Debugging stub...
   * FIXME:maybe adapt for 'dry-run' or 'verbose' use.
   */
  int status = 0;
  dmh_printf(
      "FIXME:ProcessDirectory<stub>:not executing: mkdir -p %s\n",
       pathname
    );
# if DEBUGLEVEL & DEBUG_UPDATE_INVENTORY
  /*
   * Although no installation directory has actually been created,
   * update the inventory to simulate the effect of doing so.
   */
  installed->AddEntry( dirname_key, pathname + sysroot_len );
# endif

#else
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
#endif

  return status;
}

int pkgTarArchiveInstaller::ProcessDataStream( const char *pathname )
{
  /* Extract file data from the archive, and copy it to the
   * associated target file stream, if any.
   */
#if DEBUGLEVEL & DEBUG_SUPPRESS_INSTALLATION
  /*
   * Debugging stub...
   * FIXME:maybe adapt for 'dry-run' or 'verbose' use.
   */
  dmh_printf(
      "FIXME:ProcessDataStream<stub>:not extracting: %s\n",
      pathname
    );
# if DEBUGLEVEL & DEBUG_UPDATE_INVENTORY
  /*
   * Although no file has actually been installed, update
   * the inventory to simulate the effect of doing so.
   */
  installed->AddEntry( filename_key, pathname + sysroot_len );
# endif
  return ProcessEntityData( -1 );

#else
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
    { /* The target file was not successfully and completely
       * written; discard it, and diagnose failure.
       */
      unlink( pathname );
      dmh_notify( DMH_ERROR, "%s: extraction failed\n", pathname );
    }
  }
  return status;
#endif
}

/* $RCSfile: tarproc.cpp,v $: end of file */
