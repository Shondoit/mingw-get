#ifndef PKGTASK_H
/*
 * pkgtask.h
 *
 * $Id: pkgtask.h,v 1.8 2012/03/26 21:20:18 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, 2012, MinGW Project
 *
 *
 * This header provides manifest definitions for the action codes,
 * which are used by the installer engine's task scheduler.
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
#define PKGTASK_H  1

enum
{
  action_none = 0,

  action_remove,
  action_install,
  action_upgrade,

  action_list,
  action_show,

  action_update,
  action_licence,
  action_source,

  end_of_actions
};

#define ACTION_MASK		0x0F

#define ACTION_NONE     	(unsigned long)(action_none)
#define ACTION_REMOVE   	(unsigned long)(action_remove)
#define ACTION_INSTALL  	(unsigned long)(action_install)
#define ACTION_UPGRADE  	(unsigned long)(action_upgrade)
#define ACTION_LIST     	(unsigned long)(action_list)
#define ACTION_SHOW     	(unsigned long)(action_show)
#define ACTION_UPDATE   	(unsigned long)(action_update)
#define ACTION_LICENCE  	(unsigned long)(action_licence)
#define ACTION_SOURCE   	(unsigned long)(action_source)

#define STRICTLY_GT		(ACTION_MASK + 1)
#define STRICTLY_LT		(STRICTLY_GT << 1)

#define ACTION_PRIMARY  	(STRICTLY_LT << 1)

/* Attributes used to identify when a removal action
 * may break dependencies for other installed packages.
 */
#define ACTION_REMOVE_OK	(ACTION_PRIMARY << 1)
#define ACTION_PREFLIGHT	(ACTION_PRIMARY << 2 | ACTION_REMOVE_OK)

/* Attributes used to identify when a package installation
 * or upgrade cannot be successfully installed or upgraded,
 * due to a previously failing download.
 */
#define ACTION_DOWNLOAD 	(ACTION_PRIMARY << 3)
#define ACTION_DOWNLOAD_OK	(ACTION_DOWNLOAD | ACTION_REMOVE_OK)

/* Flag set by pkgActionItem::SelectIfMostRecentFit(),
 * to indicate viability of the last package evaluated,
 * irrespective of whether it is selected, or not.
 */
#define ACTION_MAY_SELECT	(ACTION_PRIMARY << 4)

#ifndef EXTERN_C
/* A convenience macro, to facilitate declaration of functions
 * which must exhibit extern "C" bindings, in a manner which is
 * compatible with inclusion in either C or C++ source.
 */
# ifdef __cplusplus
#  define EXTERN_C extern "C"
# else
#  define EXTERN_C
# endif
#endif

EXTERN_C const char *action_name( unsigned long );
EXTERN_C int action_code( const char* );

#endif /* PKGTASK_H: $RCSfile: pkgtask.h,v $: end of file */
