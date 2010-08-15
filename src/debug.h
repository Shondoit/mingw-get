#ifndef DEBUG_H
/*
 * debug.h
 *
 * $Id: debug.h,v 1.1 2010/08/15 13:37:54 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2010, MinGW Project
 *
 *
 * Hooks to facilitate conditional compilation of code to activate
 * selective debugging features.
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
#define DEBUG_H  1
# if DEBUGLEVEL
#  ifndef DEBUG_TRACE_INIT
#   define DEBUG_TRACE_INIT	0x0001
#  endif
# else /* DEBUGLEVEL == 0 */
# endif /* DEBUGLEVEL */
#endif /* DEBUG_H: $RCSfile: debug.h,v $: end of file */
