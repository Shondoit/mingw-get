# aclocal.m4 -*- autoconf -*- vim: filetype=config
#
# $Id: aclocal.m4,v 1.2 2010/04/18 09:09:58 keithmarshall Exp $
#
# Written by Keith Marshall <keithmarshall@users.sourceforge.net>
# Copyright (C) 2009, 2010, MinGW Project
#
#
# Configuration script for mingw-get
#
#
# This is free software.  Permission is granted to copy, modify and
# redistribute this software, under the provisions of the GNU General
# Public License, Version 3, (or, at your option, any later version),
# as published by the Free Software Foundation; see the file COPYING
# for licensing details.
#
# Note, in particular, that this software is provided "as is", in the
# hope that it may prove useful, but WITHOUT WARRANTY OF ANY KIND; not
# even an implied WARRANTY OF MERCHANTABILITY, nor of FITNESS FOR ANY
# PARTICULAR PURPOSE.  Under no circumstances will the author, or the
# MinGW Project, accept liability for any damages, however caused,
# arising from the use of this software.
#
m4_include([m4/missing.m4])

# MINGW_AC_OUTPUT
# ---------------
# A wrapper for AC_OUTPUT itself, to ensure that missing prerequisite
# checks are completed, before the final output stage.
#
AC_DEFUN([MINGW_AC_OUTPUT],
[AC_REQUIRE([_MINGW_AC_ABORT_IF_MISSING_PREREQ])dnl
 AC_OUTPUT($@)dnl
])# MINGW_AC_OUTPUT
#
# $RCSfile: aclocal.m4,v $: end of file
