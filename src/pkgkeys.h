#ifndef PKGKEYS_H
/*
 * pkgkeys.c
 *
 * $Id: pkgkeys.h,v 1.5 2010/03/01 22:34:19 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2010, MinGW Project
 *
 *
 * Public declarations of the global definitions for the string
 * constants which are used as keys in the XML database.
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
#define PKGKEYS_H  1

#ifndef EXTERN_C_DECL
# ifdef __cplusplus
#  define EXTERN_C_DECL  extern "C"
# else
#  define EXTERN_C_DECL  extern
# endif
#endif

EXTERN_C_DECL const char *alias_key;
EXTERN_C_DECL const char *application_key;
EXTERN_C_DECL const char *catalogue_key;
EXTERN_C_DECL const char *class_key;
EXTERN_C_DECL const char *component_key;
EXTERN_C_DECL const char *dirname_key;
EXTERN_C_DECL const char *download_key;
EXTERN_C_DECL const char *download_host_key;
EXTERN_C_DECL const char *eq_key;
EXTERN_C_DECL const char *filename_key;
EXTERN_C_DECL const char *ge_key;
EXTERN_C_DECL const char *gt_key;
EXTERN_C_DECL const char *id_key;
EXTERN_C_DECL const char *installed_key;
EXTERN_C_DECL const char *issue_key;
EXTERN_C_DECL const char *le_key;
EXTERN_C_DECL const char *lt_key;
EXTERN_C_DECL const char *mirror_key;
EXTERN_C_DECL const char *modified_key;
EXTERN_C_DECL const char *name_key;
EXTERN_C_DECL const char *package_key;
EXTERN_C_DECL const char *package_collection_key;
EXTERN_C_DECL const char *package_list_key;
EXTERN_C_DECL const char *pathname_key;
EXTERN_C_DECL const char *profile_key;
EXTERN_C_DECL const char *release_key;
EXTERN_C_DECL const char *repository_key;
EXTERN_C_DECL const char *requires_key;
EXTERN_C_DECL const char *source_key;
EXTERN_C_DECL const char *subsystem_key;
EXTERN_C_DECL const char *sysmap_key;
EXTERN_C_DECL const char *sysroot_key;
EXTERN_C_DECL const char *tarname_key;
EXTERN_C_DECL const char *uri_key;

/* Some standard values, which may be associated with certain
 * of the above XML database keys.
 */
EXTERN_C_DECL const char *yes_value;
EXTERN_C_DECL const char *no_value;

#endif /* PKGKEYS_H: $RCSfile: pkgkeys.h,v $: end of file */
