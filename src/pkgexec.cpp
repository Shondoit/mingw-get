/*
 * pkgexec.cpp
 *
 * $Id: pkgexec.cpp,v 1.15 2011/06/13 19:00:14 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, MinGW Project
 *
 *
 * Implementation of package management task scheduler and executive.
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
#include "dmh.h"
#include "mkpath.h"

#include "pkgbase.h"
#include "pkgkeys.h"
#include "pkginfo.h"
#include "pkgtask.h"
#include "pkgopts.h"
#include "pkgproc.h"

EXTERN_C const char *action_name( unsigned long index )
{
  /* Define the keywords used on the mingw-get command line,
   * to specify the package management actions to be performed,
   * mapping each to a unique action code index.
   */
  static const char* action_id[] =
  {
    "no change",	/* unused; zero cannot test true in a bitwise test  */
    "remove",		/* remove a previously installed package	    */
    "install",		/* install a new package			    */
    "upgrade",		/* upgrade previously installed packages	    */

    "list",		/* list packages and display related information    */
    "show",		/* a synonym for "list"				    */

    "update"		/* update local copy of repository catalogues	    */
  };

  /* For specified "index", return a pointer to the associated keyword,
   * or NULL, if "index" is outside the defined action code range.
   */
  return ((index >= 0) && (index < end_of_actions))
    ? action_id[ index ]
    : NULL;
}

EXTERN_C int action_code( const char* request )
{
  /* Match an action keyword specified on the command line
   * to an entry from the above list...
   */
  int lencode = strlen( request );

  int index;
  for( index = 0; index < end_of_actions; index++ )
  {
    /* Try all defined keywords in turn, until we find a match
     * or we run out of definitions...
     */
    if( strncmp( request, action_name( index ), lencode ) == 0 )
      /*
       * for a successful match...
       * immediately return the associated action code index.
       */
      return index;
  }

  /* If we get to here, the specified keyword was not matched;
   * signal this, by returning -1.
   */
  return -1;
}

/* To circumvent MS-Windows restrictions on deletion and/or overwriting
 * executable and shared object files, while their respective code is in
 * use by a running application, and to facilitate upgrade of mingw-get
 * itself, while it is the running application performing the upgrade,
 * we introduce a "rites of passage" work around.  The first phase of
 * this is invoked immediately on process start up, but the second
 * phase is deferred...
 */
#define IMPLEMENT_INITIATION_RITES  PHASE_TWO_RITES
#include "rites.c"
/*
 * ...until we know for sure that a self-upgrade has been scheduled...
 */
RITES_INLINE bool self_upgrade_rites( const char *name )
{
  /* ...as determined by inspection of package names, and deferring
   * the rite as "pending" until a request to process "mingw-get-bin"
   * is actually received...
   */
  pkgSpecs pkg( name );
  bool pending = ((name = pkg.GetComponentClass()) == NULL)
    || (strcmp( name, "bin" ) != 0) || ((name = pkg.GetPackageName()) == NULL)
    || (strcmp( name, "mingw-get" ) != 0);

  if( ! pending )
    /*
     * We've just identified a request to process "mingw-get-bin";
     * thus the requirement to invoke the "self upgrade rites" has
     * now become immediate, so do it...
     */
    invoke_rites();

  /* Finally, return the requirement state as it now is, whether it
   * remains "pending" or not, so that the caller may avoid checking
   * the requirement for invoking the "self upgrade rites" process,
   * after it has already been requested.
   */
  return pending;
}

pkgActionItem::pkgActionItem( pkgActionItem *after, pkgActionItem *before )
{
  /* Construct an appropriately initialised non-specific pkgActionItem...
   */
  flags = 0;		/* no specific action yet assigned */

  min_wanted = NULL;	/* no minimum package version constraint... */
  max_wanted = NULL;	/* nor any maximum version */

  /* Initialise package selection to NONE, for this action... */
  selection[to_remove] = selection[to_install] = NULL;

  /* Insert this item at a specified location in the actions list.
   */
  prev = after;
  next = before;
}

pkgActionItem*
pkgActionItem::Append( pkgActionItem *item )
{
  /* Add an "item" to an ActionItems list, attaching it immediately
   * after the item referenced by the "this" pointer; nominally "this"
   * refers to the last entry in the list, resulting in a new item
   * being appended to the list, but the implementation preserves
   * integrity of any following list items, thus also fulfilling
   * an "insert after this" function.
   */
  if( this == NULL )
    /*
     * No list exists yet;
     * return "item" as first and only entry in new list.
     */
    return item;

  /* Ensure "item" physically exists, or if not, create a generic
   * placeholder in which to construct it...
   */
  if( (item == NULL) && ((item = new pkgActionItem()) == NULL) )
    /*
     * ...bailing out if no such placeholder can be created.
     */
    return NULL;

  /* Maintain list integrity...
   */
  if( (item->next = next) != NULL )
    /*
     * ...moving any existing items which already follow the insertion
     * point in the list structure, to follow the newly added "item".
     */
    next->prev = item;

  /* Set the new item's own reference pointer, to establish its list
   * attachment point...
   */
  item->prev = this;

  /* ...and attach it immediately after that point.
   */
  return next = item;
}

pkgActionItem*
pkgActionItem::Insert( pkgActionItem *item )
{
  /* Add an "item" to an ActionItems list, inserting it immediately
   * before the item referenced by the "this" pointer.
   */
  if( this == NULL )
    /*
     * No list exists yet;
     * return "item" as first and only entry in new list.
     */
    return item;

  /* Ensure "item" physically exists, or if not, create a generic
   * placeholder in which to construct it...
   */
  if( (item == NULL) && ((item = new pkgActionItem()) == NULL) )
    /*
     * ...bailing out if no such placeholder can be created.
     */
    return NULL;

  /* Maintain list integrity...
   */
  if( (item->prev = prev) != NULL )
    /*
     * ...moving any existing items which already precede the insertion
     * point in the list structure, to precede the newly added "item".
     */
    prev->next = item;

  /* Set the new item's own reference pointer, to establish the item
   * currently at the attachment point, as its immediate successor...
   */
  item->next = this;

  /* ...and attach it, immediately preceding that point.
   */
  return prev = item;
}

pkgActionItem*
pkgActionItem::Schedule( unsigned long action, pkgActionItem& item )
{
  /* Make a copy of an action item template (which may exist in
   * a volatile scope) on the heap, assign the requested action,
   * and return it for inclusion in the task schedule.
   */
  pkgActionItem *rtn = new pkgActionItem(); *rtn = item;
  rtn->flags = action | (rtn->flags & ~ACTION_MASK);
  return rtn;
}

pkgActionItem*
pkgActionItem::GetReference( pkgActionItem& item )
{
  /* Check for a prior reference, within the task schedule,
   * for the package specified for processing by "item".
   */
  pkgXmlNode* pkg;
  if( (pkg = item.Selection()->GetParent()) != NULL )
  {
    /* We have a pointer to the XML database entry which identifies
     * the package containing the release specified as the selection
     * associated with "item"; walk the chain of prior entries in
     * the schedule...
     */
    for( pkgActionItem* item = this; item != NULL; item = item->prev )
    {
      /* ...and if we find another item holding an identical pointer,
       * (i.e. to the same package), we return it...
       */
      if( item->Selection()->GetParent() == pkg )
	return item;
    }
  }

  /* If we get to here, there is no prior action scheduled for the
   * specified package, so we return a NULL pointer...
   */
  return NULL;
}

pkgXmlNode *pkgActionItem::SelectIfMostRecentFit( pkgXmlNode *package )
{
  /* Assign "package" as the "selection" for the referring action item,
   * provided it matches the specified selection criteria and it represents
   * a more recent release than any current selection.
   */
  pkgSpecs test( package );

  /* Establish the selection criteria...
   */
  pkgSpecs min_fit( min_wanted );
  pkgSpecs max_fit( max_wanted );

  /* Choose one of the above, as a basis for identification of
   * a correct package-component match...
   */
  pkgSpecs& fit = min_wanted ? min_fit : max_fit;

  /* Verify that "package" fulfills the selection criteria...
   */
  if(  match_if_explicit( test.GetComponentClass(), fit.GetComponentClass() )
  &&   match_if_explicit( test.GetComponentVersion(), fit.GetComponentVersion() )
  && ((max_wanted == NULL) || ((flags & STRICTLY_LT) ? (test < max_fit) : (test <= max_fit)))
  && ((min_wanted == NULL) || ((flags & STRICTLY_GT) ? (test > min_fit) : (test >= min_fit)))  )
  {
    /* We have the correct package component, and it fits within
     * the allowed range of release versions...
     */
    pkgSpecs last( Selection() );
    if( test > last )
      /*
       * It is also more recent than the current selection,
       * so we now replace that...
       */
      selection[to_install] = package;
  }

  /* Whatever choice we make, we return the resultant selection...
   */
  return Selection();
}

inline void pkgActionItem::SetPrimary( pkgActionItem* ref )
{
  flags = ref->flags;
  selection[ to_install ] = ref->selection[ to_install ];
  selection[ to_remove ] = ref->selection[ to_remove ];
}

pkgActionItem* pkgXmlDocument::Schedule
( unsigned long action, pkgActionItem& item, pkgActionItem* rank )
{
  /* Schedule an action item with a specified ranking order in
   * the action list, (or at the end of the list if no ranking
   * position is specified)...
   */
  pkgActionItem *ref = rank ? rank : actions;

  /* If we already have a prior matching item...
   */
  pkgActionItem *prior;
  if( (prior = actions->GetReference( item )) != NULL )
  {
    /* ...then, when the current request refers to a primary action,
     * we update the already scheduled request to reflect this...
     */
    if( (action & ACTION_PRIMARY) == ACTION_PRIMARY )
      prior->SetPrimary( rank = ref->Schedule( action & ACTION_MASK, item ) );
    return prior;
  }
  /* ...otherwise, when this request produces a valid package reference,
   * we raise a new scheduling request...
   */
  else if( ((ref = ref->Schedule( action, item )) != NULL)
  &&   ((ref->Selection() != NULL) || (ref->Selection( to_remove ) != NULL)) )
  {
    /* ...and, when successfully raised, add it to the task list...
     */
    if( rank )
      /*
       * ...at the specified ranking position, if any...
       */
      return rank->Insert( ref );

    else
      /* ...otherwise, at the end of the list.
       */
      return actions = actions->Append( ref );
  }

  /* If we get to here, then no new action was scheduled; we simply
   * return the current insertion point in the task list.
   */
  return rank;
}

void pkgActionItem::Execute()
{
  if( this != NULL )
  {
    pkgActionItem *current = this;
    bool init_rites_pending = true;
    while( current->prev != NULL ) current = current->prev;
    do {
	 DownloadArchiveFiles( current );
       } while( SetAuthorities( current ) > 0 );
    while( current != NULL )
    {
      /* Processing only those packages with assigned actions...
       */
      if( (current->flags & ACTION_MASK) != 0 )
      {
	/* Print a notification of the installation process to be
	 * performed, identifying the package to be processed.
	 */
	const char *tarname;
	if( (tarname = current->Selection()->GetPropVal( tarname_key, NULL )) == NULL )
	  tarname = current->Selection( to_remove )->GetPropVal( tarname_key, value_unknown );
	dmh_printf( "%s: %s\n", action_name(current->flags & ACTION_MASK), tarname );

	/* Check for any outstanding requirement to invoke the
	 * "self upgrade rites" process, so that we may install an
	 * upgrade for mingw-get itself...
	 */
	if( init_rites_pending )
	  /*
	   * ...discontinuing the check once this has been completed,
	   * since it need not be performed more than once.
	   */
	  init_rites_pending = self_upgrade_rites( tarname );

	/* If we are performing an upgrade...
	 */
	if( ((current->flags & ACTION_MASK) == ACTION_UPGRADE)
	/*
	 * ...and the latest version of the package is already installed...
	 */
	&&  (current->Selection() == current->Selection( to_remove ))
	/*
	 * ...and the `--reinstall' option hasn't been specified...
	 */
	&&  (pkgOptions()->Test( OPTION_REINSTALL ) == 0)  )
	  /*
	   * ...then simply report the up-to-date status...
	   */
	  dmh_notify( DMH_INFO, "package %s is up to date\n", tarname );

	else
	{ /* ...otherwise, proceed to perform remove and install
	   * operations, as appropriate.
	   */
	  if( (current->flags & ACTION_REMOVE) == ACTION_REMOVE )
	  {
	    /* The selected package has been marked for removal,
	     * either explicitly, or as an implicit prerequisite for upgrade.
	     */
	    pkgRemove( current );
	  }

	  if( (current->flags & ACTION_INSTALL) == ACTION_INSTALL )
	  {
	    /* The selected package has been marked for installation,
	     * either explicitly, or implicitly to complete a package upgrade.
	     */
	    pkgXmlNode *tmp = current->Selection( to_remove );
	    if(   pkgOptions()->Test( OPTION_REINSTALL )
	    ||  ((current->flags & ACTION_MASK) == ACTION_UPGRADE)  )
	      current->selection[ to_remove ] = NULL;
	    pkgInstall( current );
	    current->selection[ to_remove ] = tmp;
	  }
	}
      }
      /* Proceed to the next package, if any, with scheduled actions.
       */
      current = current->next;
    }
  }
}

pkgActionItem::~pkgActionItem()
{
  /* Destructor...
   * The package version range selectors, "min_wanted" and "max_wanted",
   * are always allocated storage space on the heap; we need to free that,
   * before we destroy the reference pointers.
   */
  if( (max_wanted != NULL) && (max_wanted != min_wanted) )
    /*
     * "max_wanted" is non-NULL, and is distinct, (i.e. it doesn't
     * represent an equality constraint which shares a reference with
     * "min_wanted"); we need to free it independently.
     */
    free( (void *)(max_wanted) );

  if( min_wanted != NULL )
    /*
     * "min_wanted" is non-NULL; we don't care if it is distinct,
     * because if not, freeing it is required anyway, to also free
     * the same memory referenced by "max_wanted".
     */
    free( (void *)(min_wanted) );
}

/* $RCSfile: pkgexec.cpp,v $: end of file */
