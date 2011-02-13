# configure.ac -*- autoconf -*- vim: filetype=config
#
# $Id: configure.ac,v 1.12 2011/02/13 21:23:58 keithmarshall Exp $
#
# Written by Keith Marshall <keithmarshall@users.sourceforge.net>
# Copyright (C) 2009, 2010, 2011, MinGW Project
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
  AC_INIT([mingw-get],[0.1-alpha-6cvs],[http://mingw.org/reporting_bugs])

  AC_SUBST([COPYRIGHT_HOLDER],["MinGW Project"])
  AC_SUBST([YEARS_OF_ISSUE],["2009, 2010, 2011"])

# Override the normal UNIX-centric autoconf default prefix;
# substitute the MinGW standard in its place.
#
  AC_PREFIX_DEFAULT([C:/MinGW])

# We need the canonical representation of the intended host platform
# identification triplet, from which to deduce the subsystem designator
# in distributed tarball names.  (Note that we explicitly override any
# user specified "build_alias", because autoconf gets upset if "host"
# is specified without "build", and we need "host" if cross-compiling;
# all we need in this case is "host" != "build", and we don't really
# care what "build" is, but AC_CANONICAL_HOST gets very upset, when it
# calls ${srcdir}/build-aux/config.sub, if the user specifies a short
# form which it cannot resolve -- e.g. "--build=linux" will fail).
#
  AC_CONFIG_AUX_DIR([build-aux])
  build_alias=`${srcdir}/build-aux/config.guess`
  AC_CANONICAL_HOST

# We need both C and C++ compilers; check how to invoke them
#
  AC_PROG_CC
  AC_PROG_CXX

# We also need a lexical analyser generator
#
  AC_PROG_LEX

# Ensure that (at least the headers for) prerequisite libraries,
# zlib, libbz2 and liblzma are available
#
  AC_CHECK_HEADER([zlib.h],,MINGW_AC_ASSERT_MISSING([zlib-dev],
    [libz-1.2.3-1-mingw32-dev.tar.gz]))
  AC_CHECK_HEADER([bzlib.h],,MINGW_AC_ASSERT_MISSING([libbz2-dev],
    [bzip2-1.0.5-2-mingw32-dev.tar.gz]))
  AC_CHECK_HEADER([lzma.h],,MINGW_AC_ASSERT_MISSING([liblzma-dev],
    [liblzma-4.999.9beta_20091209-3-mingw32-dev.tar.bz2]))

# Set up the archive librarian, to match our compiler settings
#
  AC_CHECK_TOOL([AR],[ar],[ar])
  AC_SUBST([ARFLAGS],[${ARFLAGS-rcs}])

# Identify tools required for product installation
#
  AC_PROG_MKDIR_P
  AC_PROG_INSTALL
  AC_PROG_LN_S

# As for the archive librarian, we need to ensure that the tool
# for stripping binaries is compatible with the compiler
#
  AC_CHECK_TOOL([STRIP],[strip],[strip])

# Establish the product version banner, and create a makefile
#
  AC_CONFIG_FILES([version.c Makefile])
  MINGW_AC_OUTPUT
#
# $RCSfile: configure.ac,v $: end of file
