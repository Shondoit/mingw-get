/*
 * pkgkeys.c
 *
 * $Id: pkgkeys.c,v 1.2 2010/01/26 21:07:18 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2010, MinGW Project
 *
 *
 * Implementation of the global definitions for the string constants
 * which are used as keys in the XML database.
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
const char *alias_key		    =	"alias";
const char *application_key	    =	"application";
const char *catalogue_key	    =	"catalogue";
const char *component_key	    =	"component";
const char *download_host_key	    =	"download-host";
const char *eq_key		    =	"eq";
const char *ge_key		    =	"ge";
const char *gt_key		    =	"gt";
const char *id_key		    =	"id";
const char *installed_key	    =	"installed";
const char *issue_key		    =	"issue";
const char *le_key		    =	"le";
const char *lt_key		    =	"lt";
const char *mirror_key		    =	"mirror";
const char *name_key		    =	"name";
const char *package_key		    =	"package";
const char *package_collection_key  =	"package-collection";
const char *package_list_key	    =	"package-list";
const char *pathname_key	    =	"path";
const char *profile_key		    =	"profile";
const char *release_key		    =	"release";
const char *repository_key	    =	"repository";
const char *requires_key	    =	"requires";
const char *subsystem_key	    =	"subsystem";
const char *sysmap_key		    =	"system-map";
const char *sysroot_key		    =	"sysroot";
const char *tarname_key		    =	"tarname";
const char *uri_key		    =	"uri";

/* Some standard values, which may be associated with certain
 * of the above keys.
 */
const char *yes_value		    =	"yes";
const char *no_value		    =	"no";

/* $RCSfile: pkgkeys.c,v $: end of file */
