#ifndef PKGBASE_H
/*
 * pkgbase.h
 *
 * $Id: pkgbase.h,v 1.10 2010/08/19 19:52:35 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, MinGW Project
 *
 *
 * Public interface for the package directory management routines;
 * declares the XML data structures, and their associated class APIs,
 * which are used to describe packages and their interdependencies.
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
#define PKGBASE_H  1

#include <tinyxml.h>
#include <tinystr.h>

#ifndef EXTERN_C
# ifdef __cplusplus
#  define EXTERN_C extern "C"
# else
#  define EXTERN_C
# endif
#endif

/* Adopt sensible defaults for matching subsystem and file names...
 */
#ifdef _WIN32
  /*
   * The MS-Windows file system is intrinsically case insensitive,
   * so we prefer to match both subsystem and file names in a case
   * insensitive manner...
   */
# ifndef CASE_INSENSITIVE_SUBSYSTEMS
#  define CASE_INSENSITIVE_SUBSYSTEMS  1
# endif
# ifndef CASE_INSENSITIVE_FILESYSTEM
#  define CASE_INSENSITIVE_FILESYSTEM  1
# endif
  /*
   * The preferred name for MS-Windows' case insensitive string
   * matching function, equivalent to POSIX strcasecmp().
   */
# define strcasecmp  stricmp
#else
  /* On other systems, we prefer to adopt case sensitive matching
   * strategies for subsystem and file names.
   */
# ifndef CASE_INSENSITIVE_SUBSYSTEMS
#  define CASE_INSENSITIVE_SUBSYSTEMS  0
# endif
# ifndef CASE_INSENSITIVE_FILESYSTEM
#  define CASE_INSENSITIVE_FILESYSTEM  0
# endif
#endif

class pkgSpecs;

class pkgXmlNode : public TiXmlElement
{
  /* A minimal emulation of the wxXmlNode class, founded on
   * the tinyxml implementation of the TiXmlElement class, and
   * subsequently extended by application specific features.
   */
  public:
    /* Constructors...
     */
    inline pkgXmlNode( const char* name ):TiXmlElement( name ){}
    inline pkgXmlNode( const pkgXmlNode& src ):TiXmlElement( src ){}

    /* Accessors...
     *
     * Note that tinyxml is generally careless about checking for
     * possible dereferencing of NULL pointers; thus, many of these
     * wrappers include appropriate checks, to prevent this.
     */
    inline const char* GetName()
    {
      /* Retrieve the identifying name of the XML tag;
       * tinyxml calls this the element "value"...
       */
      return this ? Value() : NULL;
    }
    inline pkgXmlNode* GetParent()
    {
      /* wxXmlNode provides this equivalant of tinyxml's
       * Parent() method.
       */
      return this ? (pkgXmlNode*)(Parent()) : NULL;
    }
    inline pkgXmlNode* GetChildren()
    {
      /* wxXmlNode provides only this one method to access
       * the children of an element; it is equivalent to the
       * FirstChild() method in tinyxml's arsenal.
       */
      return this ? (pkgXmlNode*)(FirstChild()) : NULL;
    }
    inline pkgXmlNode* GetNext()
    {
      /* This is wxXmlNode's method for visiting other children
       * of an element, after the first found by GetChildren();
       * it is equivalent to tinyxml's NextSibling().
       */
      return this ? (pkgXmlNode*)(NextSibling()) : NULL;
    }
    inline const char* GetPropVal( const char* name, const char* subst )
    {
      /* tinyxml has no direct equivalent for this wxXmlNode method,
       * (which substitutes default "subst" text for an omitted property),
       * but it may be trivially emulated, using the Attribute() method.
       */
      const char* retval = this ? Attribute( name ) : subst;
      return retval ? retval : subst;
    }
    inline pkgXmlNode* AddChild( TiXmlNode *child )
    {
      /* This is wxXmlNode's method for adding a child node, it is
       * equivalent to tinyxml's LinkEndChild() method.
       */
      return this ? (pkgXmlNode*)(LinkEndChild( child )) : NULL;
    }
    inline bool DeleteChild( pkgXmlNode *child )
    {
      /* Both TiXmlNode and wxXmlNode have RemoveChild methods, but the
       * implementations are semantically different; for tinyxml, we may
       * simply use the RemoveChild method here, where for wxXmlNode, we
       * would use RemoveChild followed by `delete child'.
       */
      return this ? RemoveChild( child ) : false;
    }

    /* Additional methods specific to the application.
     */
    inline pkgXmlNode *GetDocumentRoot()
    {
      /* Convenience method to retrieve a pointer to the document root.
       */
      return this ? (pkgXmlNode*)(GetDocument()->RootElement()) : NULL;
    }
    inline bool IsElementOfType( const char* tagname )
    {
      /* Confirm if the owner XML node represents a data element
       * with the specified "tagname".
       */
      return this ? strcmp( GetName(), tagname ) == 0 : false;
    }

    /* Methods for retrieving the system root management records
     * for a specified installed subsystem.
     */
    pkgXmlNode *GetSysRoot( const char *subsystem );
    pkgXmlNode *GetInstallationRecord( const char* );

    /* The following pair of methods provide an iterator
     * for enumerating the contained nodes, within the owner,
     * which themselves exhibit a specified tagname.
     */
    pkgXmlNode* FindFirstAssociate( const char* tagname );
    pkgXmlNode* FindNextAssociate( const char* tagname );

    /* Specific to XML node elements of type "release",
     * the following pair of methods retrieve the actual name of
     * the release tarball, and its associated source code tarball,
     * as they are named on the project download servers.
     */
    const char* ArchiveName();
    const char* SourceArchiveName();

    /* The following retrieves an attribute which may have been
     * specified on an ancestor (container) node; typically used to
     * retrieve the package name or alias attributes which are to
     * be associated with a release.
     */
    const char *GetContainerAttribute( const char*, const char* = NULL );
};

enum { to_remove = 0, to_install, selection_types };

class pkgActionItem
{
  /* A class implementing a bi-directionally linked list of
   * "action" descriptors, which is to be associated with the
   * pkgXmlDocument class, specifying actions to be performed
   * on the managed software installation.
   */
  private:
    /* Pointers to predecessor and successor in the linked list
     * comprising the schedule of action items.
     */
    pkgActionItem* prev;
    pkgActionItem* next;

    /* Flags define the specific action associated with this item.
     */
    unsigned long flags;

    /* Criteria for selection of package versions associated with
     * this action item.
     */
    const char* min_wanted;
    const char* max_wanted;

    /* Pointers to the XML database entries for the package selected
     * for processing by this action.
     */
    pkgXmlNode* selection[ selection_types ];

    /* Method for retrieving packages from a distribution server.
     */
    void DownloadArchiveFiles( pkgActionItem* );

  public:
    /* Constructor...
     */
    pkgActionItem( pkgActionItem* = NULL, pkgActionItem* = NULL );

    /* Methods for assembling action items into a linked list.
     */
    pkgActionItem* Append( pkgActionItem* = NULL );
    pkgActionItem* Insert( pkgActionItem* = NULL );

    /* Methods for compiling the schedule of actions.
     */
    pkgActionItem* GetReference( pkgActionItem& );
    pkgActionItem* Schedule( unsigned long, pkgActionItem& );

    /* Methods for defining the selection criteria for
     * packages to be processed.
     */
    pkgXmlNode* SelectIfMostRecentFit( pkgXmlNode* );
    const char* SetRequirements( pkgXmlNode*, pkgSpecs* );
    inline void SelectPackage( pkgXmlNode *pkg, int opt = to_install )
    {
      /* Mark a package as the selection for a specified action.
       */
      selection[ opt ] = pkg;
    }
    inline pkgXmlNode* Selection( int mode = to_install )
    {
      /* Retrieve the package selection for a specified action.
       */
      return selection[ mode ];
    }

    /* Method for processing all scheduled actions.
     */
    void Execute();

    /* Destructor...
     */
    ~pkgActionItem();
};

class pkgXmlDocument : public TiXmlDocument
{
  /* Minimal emulation of the wxXmlDocument class, founded on
   * the tinyxml implementation of the TiXmlDocument class.
   */
  public:
    /* Constructors...
     */
    inline pkgXmlDocument(){}
    inline pkgXmlDocument( const char* name )
    {
      /* tinyxml has a similar constructor, but unlike wxXmlDocument,
       * it DOES NOT automatically load the document; force it.
       */
      LoadFile( name );

      /* Always begin with an empty actions list.
       */
      actions = NULL;
    }

    /* Accessors...
     */
    inline bool IsOk()
    {
      /* tinyxml doesn't have this, but instead provides a complementary
       * `Error()' indicator, so to simulate `IsOk()'...
       */
      return ! Error();
    }
    inline pkgXmlNode* GetRoot()
    {
      /* This is wxXmlDocument's method for locating the document root;
       * it is equivalent to tinyxml's RootElement() method.
       */
      return (pkgXmlNode *)(RootElement());
    }
    inline void AddDeclaration
    ( const char *version, const char *encoding, const char *standalone )
    {
      /* Not a standard method of either wxXmlDocumemnt or TiXmlDocument;
       * this is a convenience method for setting up a new XML database.
       */
      LinkEndChild( new TiXmlDeclaration( version, encoding, standalone ) );
    }
    inline void SetRoot( TiXmlNode* root )
    {
      /* tinyxml has no direct equivalent for this wxXmlDocument method;
       * to emulate it, we must first explicitly delete an existing root
       * node, if any, then link the new root node as a document child.
       */
      pkgXmlNode *oldroot;
      if( (oldroot = GetRoot()) != NULL )
	delete oldroot;
      LinkEndChild( root );
    }
    inline bool Save( const char *filename )
    {
      /* This wxXmlDocument method for saving the database is equivalent
       * to the corresponding tinyxml SaveFile( const char* ) method.
       */
      return SaveFile( filename );
    }

  private:
    /* Properties specifying the schedule of actions.
     */
    unsigned long request;
    pkgActionItem* actions;

  public:
    /* Method to synchronise the state of the local package manifest
     * with the master copy held on the distribution server.
     */
    void SyncRepository( const char*, pkgXmlNode* );

    /* Method to merge content from repository-specific package lists
     * into the central XML package database.
     */
    pkgXmlNode* BindRepositories( bool );

    /* Method to load the system map, and the lists of installed
     * packages associated with each specified sysroot.
     */
    void LoadSystemMap();

    /* Complementary method, to update the saved sysroot data associated
     * with the active system map.
     */
    void UpdateSystemMap();

    /* Method to locate the XML database entry for a named package.
     */
    pkgXmlNode* FindPackageByName( const char*, const char* = NULL );

    /* Method to resolve the dependencies of a specified package,
     * by walking the chain of references specified by "requires"
     * elements in the respective package database entries.
     */
    void ResolveDependencies( pkgXmlNode*, pkgActionItem* = NULL );

    /* Methods for compiling a schedule of actions.
     */
    void Schedule( unsigned long, const char* );
    pkgActionItem* Schedule( unsigned long, pkgActionItem&, pkgActionItem* = NULL );

    /* Method to execute a sequence of scheduled actions.
     */
    inline void ExecuteActions(){ actions->Execute(); }
};

EXTERN_C const char *xmlfile( const char*, const char* = NULL );
EXTERN_C int has_keyword( const char*, const char* );

typedef int (*strcmp_function)( const char *, const char * );

static inline
bool safe_strcmp( strcmp_function strcmp, const char *value, const char *proto )
{
  /* Helper to compare a pair of "C" strings for equality,
   * accepting NULL as a match for anything; for non-NULL matches,
   * case sensitivity is determined by choice of strcmp function.
   *
   * N.B. Unlike the 'strcmp' function which this calls, this is
   * a boolean function, returning TRUE when the 'strcmp' result
   * is zero, (i.e. the sense of the result is inverted).
   */
  return (value == NULL) || (proto == NULL) || (strcmp( value, proto ) == 0);
}

/* Define a safe_strcmp() alias for an explicitly case sensitive match.
 */
#define match_if_explicit( A, B )  safe_strcmp( strcmp, (A), (B) )

/* Further safe_strcmp() aliases provide for matching subsystem names,
 * with implementation dependent case sensitivity...
 */
#if CASE_INSENSITIVE_SUBSYSTEMS
# define subsystem_strcmp( A, B )  safe_strcmp( strcasecmp, (A), (B) )
#else
# define subsystem_strcmp( A, B )  safe_strcmp( strcmp, (A), (B) )
#endif

/* ...and similarly, for matching of file names.
 */
#if CASE_INSENSITIVE_FILESYSTEM
# define pkg_strcmp( A, B )  safe_strcmp( strcasecmp, (A), (B) )
#else
# define pkg_strcmp( A, B )  safe_strcmp( strcmp, (A), (B) )
#endif

#endif /* PKGBASE_H: $RCSfile: pkgbase.h,v $: end of file */
