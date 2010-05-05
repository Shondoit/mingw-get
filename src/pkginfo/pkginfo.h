#ifndef PKGINFO_H
/*
 * pkginfo.h
 *
 * $Id: pkginfo.h,v 1.2 2010/05/05 20:34:17 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, MinGW Project
 *
 *
 * Public interface for the package tarname interpreter.  Provides
 * type definitions and function prototypes for the "C" interpreter,
 * which is implemented as a "flex" scanner in file "pkginfo.l", to
 * be accessed via the "get_pkginfo()" function.
 *
 * When included by "C++" code, it also defines the interface for
 * the "pkgSpecs" class, which is used by the package manager, for
 * convenient evaluation, comparison and manipulation of package
 * attributes, based on examination of the package tarname.
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
#define PKGINFO_H  1

enum
{ /* Symbolic names for the elements of an archive's tarname...
   */
  PACKAGE_NAME = 0,		/* the name of the package */
  PACKAGE_VERSION,		/* the version number of this release */
  PACKAGE_BUILD,		/* the build serial number */

  PACKAGE_SUBSYSTEM_NAME,	/* subsystem to which it belongs */
  PACKAGE_SUBSYSTEM_VERSION,	/* version of that subsystem */
  PACKAGE_SUBSYSTEM_BUILD,	/* subsystem build serial number */

  PACKAGE_RELEASE_STATUS,	/* package development status */
  PACKAGE_RELEASE_INDEX,	/* package release serial number */

  PACKAGE_COMPONENT_CLASS,	/* component type, e.g. bin, dll, dev */
  PACKAGE_COMPONENT_VERSION,	/* component version, e.g. of dll */

  PACKAGE_FORMAT,		/* package format, e.g. tar, zip, exe */
  PACKAGE_COMPRESSION_TYPE,	/* compression format, e.g. gz, bz2 */

  PACKAGE_TAG_COUNT		/* number of above element types */
};
/*
 * The "pkginfo_t" type describes a data structure,
 * (basically an array of pointers to character string elements),
 * in which each of the preceding package attributes is stored,
 * after extraction from the tarname, by  "get_pkginfo()".
 */
typedef char *pkginfo_t[PACKAGE_TAG_COUNT];

#ifdef __cplusplus
/*
 * The "get_pkginfo()" function offers a "C" language API...
 */
extern "C"
#endif
/*
 * ...it accepts a fully qualified package tarname as its first
 * argument, copies it to dynamically allocated memory, decomposes
 * this to the set of attributes enumerated above, and stores the
 * pointers to the individual attributes into the appropriate slots
 * within the "pkginfo_t" array referenced by the second argument,
 * whence the caller may retrieve the attribute values using the
 * enumerated attribute names to index the array.
 *
 * The return value is a pointer to the dynamically allocated memory
 * used to store the decomposition of the tarname, (or "NULL", if
 * the function fails); to avoid memory leaks, the caller should
 * call "free()" on this pointer, when the returned data is no
 * longer required.
 */
void *get_pkginfo( const char *, pkginfo_t );

#ifdef __cplusplus
/*
 * "C++" applications may encapsulate the "C" language API within the
 * "pkgSpecs" class.  This manages the "C" language memory allocation
 * internally, relieving the caller of the responsibility for freeing
 * allocated memory, and provides accessor and comparator methods for
 * retrieval and comparison of package attributes.
 */
#include <stdlib.h>		/* for definition of NULL */

#include "pkgbase.h"            /* for pkgXmlNode class declaration */

class pkgSpecs
{
  private:
    /* Package attribute data retrieved from the "C" language API,
     * and automatically populated on class instantiation.
     */
    pkginfo_t  specs;		/* pkginfo data array */
    void      *content;		/* buffer to hold its content */

    /* Internal method to facilitate manipulation of individual
     * property field values.
     */
    const char *SetProperty( int, const char* );

  public:
    /* Constructors...
     */
    pkgSpecs( const char* );
    pkgSpecs( const pkgSpecs& );
    pkgSpecs( pkgXmlNode* );

    /* Destructor...
     */
    ~pkgSpecs();

    /* Operators...
     */
    pkgSpecs& operator=( const pkgSpecs& );

    /* Accessors...
     */
    inline const char *GetPackageName()
    {
      return specs[PACKAGE_NAME];
    }
    inline const char *GetPackageVersion()
    {
      return specs[PACKAGE_VERSION];
    }
    inline const char *GetPackageBuild()
    {
      return specs[PACKAGE_BUILD];
    }
    inline const char *GetSubSystemName()
    {
      return specs[PACKAGE_SUBSYSTEM_NAME];
    }
    inline const char *GetSubSystemVersion()
    {
      return specs[PACKAGE_SUBSYSTEM_VERSION];
    }
    inline const char *GetSubSystemBuild()
    {
      return specs[PACKAGE_SUBSYSTEM_BUILD];
    }
    inline const char *GetReleaseStatus()
    {
      return specs[PACKAGE_RELEASE_STATUS];
    }
    inline const char *GetReleaseIndex()
    {
      return specs[PACKAGE_RELEASE_INDEX];
    }
    inline const char *GetComponentClass()
    {
      return specs[PACKAGE_COMPONENT_CLASS];
    }
    inline const char *GetComponentVersion()
    {
      return specs[PACKAGE_COMPONENT_VERSION];
    }
    inline const char *GetPackageFormat()
    {
      return specs[PACKAGE_FORMAT];
    }
    inline const char *GetCompressionType()
    {
      return specs[PACKAGE_COMPRESSION_TYPE];
    }

    /* Manipulators...
     */
    inline const char *SetPackageName( const char *value )
    {
      return SetProperty( PACKAGE_NAME, value );
    }
    inline const char *SetPackageVersion( const char *value )
    {
      return SetProperty( PACKAGE_VERSION, value );
    }
    inline const char *SetPackageBuild( const char *value )
    {
      return SetProperty( PACKAGE_BUILD, value );
    }
    inline const char *SetSubSystemName( const char *value )
    {
      return SetProperty( PACKAGE_SUBSYSTEM_NAME, value );
    }
    inline const char *SetSubSystemVersion( const char *value )
    {
      return SetProperty( PACKAGE_SUBSYSTEM_VERSION, value );
    }
    inline const char *SetSubSystemBuild( const char *value )
    {
      return SetProperty( PACKAGE_SUBSYSTEM_BUILD, value );
    }
    inline const char *SetReleaseStatus( const char *value )
    {
      return SetProperty( PACKAGE_RELEASE_STATUS, value );
    }
    inline const char *SetReleaseIndex( const char *value )
    {
      return SetProperty( PACKAGE_RELEASE_INDEX, value );
    }
    inline const char *SetComponentClass( const char *value )
    {
      return SetProperty( PACKAGE_COMPONENT_CLASS, value );
    }
    inline const char *SetComponentVersion( const char *value )
    {
      return SetProperty( PACKAGE_COMPONENT_VERSION, value );
    }
    inline const char *SetPackageFormat( const char *value )
    {
      return SetProperty( PACKAGE_FORMAT, value );
    }
    inline const char *SetCompressionType( const char *value )
    {
      return SetProperty( PACKAGE_COMPRESSION_TYPE, value );
    }

    /* Comparators...
     */
    bool operator<( pkgSpecs& );
    bool operator<=( pkgSpecs& );
    bool operator==( pkgSpecs& );
    bool operator>=( pkgSpecs& );
    bool operator>( pkgSpecs& );

    /* Helper to reconstitute the associated tarname...
     * The optional argument MUST either be a NULL pointer, or
     * it MUST point to memory previously allocated by malloc();
     * (the returned value is ALWAYS such a pointer, and the
     * intent of the argument is to recycle the memory used
     * for previous results, rather than to provide any
     * pre-allocated buffer).
     */
    const char *GetTarName( const char* = NULL );
};

#endif /* __cplusplus */
#endif /* PKGINFO_H: $RCSfile: pkginfo.h,v $: end of file */
