#ifndef PKGSTRM_H
/*
 * pkgstrm.h
 *
 * $Id: pkgstrm.h,v 1.2 2010/01/23 15:20:06 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, MinGW Project
 *
 *
 * Declaration of the streaming API, for reading package archives.
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
#define PKGSTRM_H  1

#include <stdint.h>

class pkgArchiveStream
{
  /* Abstract base class...
   * All archive streaming classes are be derived from this.
   */
  public:
    pkgArchiveStream(){}
    virtual int Read( char*, size_t ) = 0;
    virtual ~pkgArchiveStream(){}

  protected:
    virtual int GetRawData( int, uint8_t*, size_t );
};

#ifdef PKGSTRM_H_SPECIAL
/*
 * Specialisations of the generic base class...
 * Most clients don't need to be specifically aware of these;
 * those that do must #define PKGSTRM_H_SPECIAL, before they
 * #include pkgstrm.h
 *
 */
class pkgRawArchiveStream : public pkgArchiveStream
{
  /* A regular (uncompressed) data stream...
   */
  protected:
    int fd;

  public:
    pkgRawArchiveStream( int );
    pkgRawArchiveStream( const char* );
    virtual ~pkgRawArchiveStream();

    virtual int Read( char*, size_t );
};

/* Compressed data stream classes...
 */
#include <zlib.h>
#include <bzlib.h>
#include <lzma.h>

class pkgGzipArchiveStream : public pkgArchiveStream
{
  /* A stream compressed using the "gzip" algorithm...
   */
  protected:
    gzFile stream;

  public:
    pkgGzipArchiveStream( int );
    pkgGzipArchiveStream( const char* );
    virtual ~pkgGzipArchiveStream();

    virtual int Read( char*, size_t );
};

class pkgBzipArchiveStream : public pkgArchiveStream
{
  /* A stream compressed using the "bzip2" algorithm...
   */
  protected:
    BZFILE *stream;
    int bzerror;

  public:
    pkgBzipArchiveStream( int );
    pkgBzipArchiveStream( const char* );
    virtual ~pkgBzipArchiveStream();

    virtual int Read( char*, size_t );
};

class pkgLzmaArchiveStream : public pkgArchiveStream
{
  /* A stream compressed using the "lzma" algorithm...
   */
  protected:
    int fd;
    lzma_stream stream;
    uint8_t streambuf[BUFSIZ];
    int status;

  public:
    pkgLzmaArchiveStream( int );
    pkgLzmaArchiveStream( const char* );
    virtual ~pkgLzmaArchiveStream();

    virtual int Read( char*, size_t );
};

class pkgXzArchiveStream : public pkgArchiveStream
{
  /* A stream compressed using the "xz" algorithm...
   */
  protected:
    int fd;
    lzma_stream stream;
    uint8_t streambuf[BUFSIZ];
    lzma_action opmode;
    int status;

  public:
    pkgXzArchiveStream( int );
    pkgXzArchiveStream( const char* );
    virtual ~pkgXzArchiveStream();

    virtual int Read( char*, size_t );
};

#endif /* PKGSTRM_H_SPECIAL */

/* A generic helper function, to open an archive stream using
 * the appropriate specialised stream class...
 */
extern "C" pkgArchiveStream *pkgOpenArchiveStream( const char* );

#endif /* PKGSTRM_H: $RCSfile: pkgstrm.h,v $: end of file */
