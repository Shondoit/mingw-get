#ifndef PKGPROC_H
/*
 * pkgproc.h
 *
 * $Id: pkgproc.h,v 1.6 2011/08/30 18:04:06 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, MinGW Project
 *
 *
 * Specifications for the internal architecture of package archives,
 * and the public interface for the package archive processing routines,
 * used to implement the package installer and uninstaller.
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
#define PKGPROC_H  1

#include "pkgbase.h"
#include "pkgstrm.h"

EXTERN_C void pkgInstall( pkgActionItem* );
EXTERN_C void pkgRegister( pkgXmlNode*, pkgXmlNode*, const char*, const char* );
EXTERN_C void pkgRemove( pkgActionItem* );

class pkgManifest
{
  /* A wrapper around the XML document class, with specialised methods
   * for management of the package installation manifest.
   */
  public:
    pkgManifest( const char*, const char* );
    ~pkgManifest();

    void AddEntry( const char*, const char* );
    void BindSysRoot( pkgXmlNode*, const char* );
    void DetachSysRoot( const char* );

    inline pkgXmlNode *GetRoot(){ return manifest->GetRoot(); }
    pkgXmlNode *GetSysRootReference( const char* );

  private:
    pkgXmlDocument *manifest;
    pkgXmlNode     *inventory;
};

class pkgArchiveProcessor
{
  /* A minimal generic abstract base class, from which we derive
   * processing tools for handling arbitrary package architectures.
   */
  public:
    pkgArchiveProcessor(){}
    virtual ~pkgArchiveProcessor(){}

    virtual bool IsOk() = 0;
    virtual int Process() = 0;

  protected:
    int sysroot_len;

    /* Pointers to the sysroot management records and installation
     * path template for a managed package; note that 'tarname' does
     * not explicitly refer only to tar archives; it is simply the
     * canonical name of the archive file, as recorded in the XML
     * 'tarname' property of the package identifier record.
     */
    pkgXmlNode  *origin;
    pkgXmlNode  *sysroot;
    const char  *sysroot_path;
    pkgManifest *installed;
    const char  *tarname;
    const char  *pkgfile;

    virtual int CreateExtractionDirectory( const char* );
    virtual int ExtractFile( int, const char*, int );
};

/* Our standard package format specifies the use of tar archives;
 * the following specialisation of pkgArchiveProcessor provides the
 * tools we need for processing such archives.
 *
 */
union tar_archive_header
{
  /* Layout specification for the tar archive header records,
   * one of which is associated with each individual data entity
   * stored within a tar archive.
   */
  char aggregate[512];			/* aggregate size is always 512 bytes */
  struct tar_header_field_layout
  {
    char name[100];			/* archive entry path name */
    char mode[8];			/* unix access mode for archive entry */
    char uid[8];			/* user id of archive entry's owner */
    char gid[8];			/* group id of archive entry's owner */
    char size[12];			/* size of archive entry in bytes */
    char mtime[12];			/* last modification time of entry */
    char chksum[8];			/* checksum for this header block */
    char typeflag[1];			/* type of this archive entry */
    char linkname[100];			/* if a link, name of linked file */
    char magic[6];			/* `magic' signature for the archive */
    char version[2];			/* specification conformance code */
    char uname[32];			/* user name of archive entry's owner */
    char gname[32];			/* group name of entry's owner */
    char devmajor[8];			/* device entity major number */
    char devminor[8];			/* device entity minor number */
    char prefix[155];			/* prefix to extend "name" field */
  } field;
};

/* Type descriptors, as used in the `typeflag' field of the above
 * tar archive header records; (note: this is not a comprehensive
 * list, but covers all of, and more than, our requirements).
 */
#define TAR_ENTITY_TYPE_FILE		'0'
#define TAR_ENTITY_TYPE_LINK		'1'
#define TAR_ENTITY_TYPE_SYMLINK		'2'
#define TAR_ENTITY_TYPE_CHRDEV		'3'
#define TAR_ENTITY_TYPE_BLKDEV		'4'
#define TAR_ENTITY_TYPE_DIRECTORY	'5'
#define TAR_ENTITY_TYPE_GNU_LONGNAME	'L'

/* Some older style tar archives may use '\0' as an alternative to '0',
 * to identify an archive entry representing a regular file.
 */
#define TAR_ENTITY_TYPE_ALTFILE		'\0'

class pkgTarArchiveProcessor : public pkgArchiveProcessor
{
  /* An abstract base class, from which various tar archive
   * processing tools, (including installers and uninstallers),
   * are derived; this class implements the generic methods,
   * which are shared by all such tools.
   */
  public:
    /* Constructors and destructor...
     */
    pkgTarArchiveProcessor(){}
    pkgTarArchiveProcessor( pkgXmlNode* );
    virtual ~pkgTarArchiveProcessor();

    inline bool IsOk(){ return stream->IsReady(); }
    virtual int Process();

  protected:
    /* Class data...
     */
    pkgArchiveStream *stream;
    union tar_archive_header header;

    /* Internal archive processing methods...
     * These are divided into two categories: those for which the
     * abstract base class furnishes a generic implementation...
     */
    virtual int GetArchiveEntry();
    virtual int ProcessEntityData( int );
    virtual char *EntityDataAsString();

    /* ...those for which each specialisation is expected to
     * furnish its own task specific implementation...
     */
    virtual int ProcessDirectory( const char* ) = 0;
    virtual int ProcessDataStream( const char* ) = 0;

    /* ...and those which would normally be specialised, but
     * for which we currently provide a generic stub...
     */
    virtual int ProcessLinkedEntity( const char* );
};

class pkgTarArchiveExtractor : public pkgTarArchiveProcessor
{
  /* Worker class supporting extraction of tar archives to an
   * arbitrary directory, without performing an installation.
   */
  public:
    pkgTarArchiveExtractor( const char*, const char* );

  private:
    /* Specialised implementations of the archive processing methods...
     */
    virtual int ProcessDirectory( const char* );
    virtual int ProcessDataStream( const char* );
};

class pkgTarArchiveInstaller : public pkgTarArchiveProcessor
{
  /* Worker class for extraction of package tar archive content
   * to the sysroot directory nominated in the package manifest,
   * for the purpose of performing an installation or upgrade.
   */
  public:
    /* Constructor and destructor...
     */
    pkgTarArchiveInstaller( pkgXmlNode* );
    virtual ~pkgTarArchiveInstaller(){}

    virtual int Process();

  private:
    /* Specialised implementations of the archive processing methods...
     */
    virtual int ProcessDirectory( const char* );
    virtual int ProcessDataStream( const char* );
};

class pkgTarArchiveUninstaller : public pkgTarArchiveProcessor
{
  /* Worker class for removing package content which has been
   * previously installed by the pkgTarArchiveInstaller.
   */
  public:
    /* Constructor and destructor...
     */
    pkgTarArchiveUninstaller( pkgXmlNode* );
    virtual ~pkgTarArchiveUninstaller(){};

  private:
    /* Specialised implementations of the archive processing methods...
     */
    virtual int ProcessDirectory( const char* );
    virtual int ProcessDataStream( const char* );
};

#endif /* PKGPROC_H: $RCSfile: pkgproc.h,v $: end of file */
