/*
 * pkginfo.l
 *
 * $Id: pkginfo.l,v 1.4 2011/06/11 12:39:01 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, MinGW Project
 *
 *
 * A simple lexical analyser for decomposing package archive names into their
 * constituent components.  It implements the schema:--
 *
 *   <archive-name>	 ::= <package-id>[["-"<system-id>]][[-<status>]]"-"<type-id>
 *
 *   <package-id>	 ::= <package-name>"-"<version>[["-"<build-id>]]
 *   <system-id>	 ::= <system-name>[["-"<version>[["-"<build-id>]]]]
 *   <type-id>		 ::= <component-id>"."<format-type>[["."<compression-type>]]
 *
 *   <package-name>	 ::= !"-"{!"-"}
 *
 *   <version>		 ::= <major>[["."<minor>[["."<patchlevel>]]]]
 *
 *   <major>		 ::= "0".."9"{"0".."9"}
 *   <minor>		 ::= "0".."9"{"0".."9"}[[<suffix>]]
 *   <patchlevel>	 ::= "0".."9"{"0".."9"}[[<suffix>]]
 *   <suffix>		 ::= {!("0".."9"|"-"|".")}
 *
 *   <build-id> 	 ::= <datestamp>|<serial-number>{"-"<serial-number>}
 *
 *   <serial-number>	 ::= "0".."9"{"0".."9"}
 *   <datestamp>	 ::= <serial-number> ; nominally 8 digit date as YYYYMMDD
 *					     ; (currently unenforced, however)
 *
 *   <system-name>	 ::= !("0".."9"|"-"|"."){!("-"|".")}
 *
 *   <status>		 ::= ("alpha"|"beta"|"pre"|"rc"|"stable")[[-<build-id>]]
 *
 *   <component-id>	 ::= <component-class>[["-"<component-version>]]
 *
 *   <component-class>	 ::= !("0".."9"|"-"|"."){!("-"|".")}
 *   <component-version> ::= "0".."9"{!("-"|".")}
 *
 *   <format-type>	 ::= !("-"|"."){!("-"|".")}
 *   <compression-type>  ::= !("-"|"."){!("-"|".")}
 *
 * Notes:--
 *
 *   <format-type> is expected to take one of the nominal values from the set
 *   "exe"|"tar"|"zip"; however, this is not enforced.
 *
 *   <compression-type> is expected to take one of the nominal values from the
 *   set "bz2"|"gz"|"lzma"; however, this is similarly not enforced.
 *
 *   In addition to the list of keywords identified above, <status> may be
 *   assigned any of a nominal set of CMS identifiers, (see the definition of
 *   CMS_KEYWORDS in the code below); in such cases, a "$" token is prefixed
 *   to the effective <status> value, resulting in a ranking precedence lower
 *   than that of "alpha".
 *
 *   Additionally, "?" is used as a sentinel, and is not permitted *anywhere*
 *   in <archive-name>; (possibly something like ASCII <ETX> would be a more
 *   useful choice for this purpose).
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
%option noyywrap
%option prefix="__pkginfo_"
%option case-insensitive

%x	TRANS FINAL

STATUS_KEYWORDS (alpha|beta|pre|rc|stable)
CMS_KEYWORDS (bzr|cms|cvs|darcs|git|hg|mono|scm|svn|vcs)

    #include "pkginfo.h"
    #define YY_DECL int yylex( int start, pkginfo_t signature, char *name )

%%

    static int index, mark, phase;

    if( start == PACKAGE_NAME )
    {
      /* Initialise for new package archive name...
       * Clear all `signature' array slots, and set up
       * to capture the <package-name> element.
       */
      BEGIN INITIAL;
      phase = mark = 0;
      for( index = PACKAGE_NAME; index < PACKAGE_TAG_COUNT; index++ )
	signature[index] = NULL;
      index = PACKAGE_NAME;
      signature[PACKAGE_NAME] = name;
    }

- { /*
     * General case rule...
     * Found the standard element separator, so initiate a transition.
     */
    BEGIN TRANS;
    return yyleng;
  }

[^-]* {
    /* General case rule...
     * Matched an arbitrary sequence of non-separators, so mark them;
     * they will be appended to the current element.
     */
    return mark += yyleng;
  }

<TRANS>{STATUS_KEYWORDS}- {
    /*
     * Transitional case rule...
     * Identify a following package development <status> descriptor;
     * revert to INITIAL state, to continue the <archive-name> scan.
     */
    BEGIN INITIAL; yyless( 0 );
    if( index < PACKAGE_RELEASE_STATUS )
    {
      /* and, when the <status> descriptor is appropriately placed,
       * set up `signature' storage to capture it, adjusting phase to
       * detect a following <build-id>, (representing a release id).
       */
      name[mark++] = '\0';
      signature[index = PACKAGE_RELEASE_STATUS] = name + mark;
      phase = 1;
    }
    /* otherwise we simply ignore a misplaced <status> descriptor,
     * but in either case, we continue the scan at the start of the
     * apparent <status> descriptor, which has been detected.
     */
    else ++mark;
  }

<TRANS>{CMS_KEYWORDS}- {
    /*
     * Transitional case rule...
     * Designate a following CMS label as a <status> descriptor;
     * revert to INITIAL state, to continue the <archive-name> scan.
     */
    BEGIN INITIAL; yyless( 0 );
    if( (PACKAGE_NAME < index) && (index < PACKAGE_RELEASE_STATUS) )
    {
      /* and, when the <status> descriptor is appropriately placed,
       * set up `signature' storage to capture it, adjusting phase to
       * detect a following <build-id>, (representing a release id).
       */
      int tail;
      name[mark++] = '\0';
      signature[index = PACKAGE_RELEASE_STATUS] = name + mark;
      for( tail = mark + strlen( name + mark ) + 1; tail > mark; --tail )
	name[tail] = name[tail - 1];
      unput( name[mark] = '$' );
      phase = 1;
    }
    /* otherwise we simply ignore a misplaced <status> descriptor,
     * but in either case, we continue the scan at the start of the
     * apparent <status> descriptor, which has been detected.
     */
    else ++mark;
  }

<TRANS>([%&*]|[^-0-9.][^-.]+)(-[0-9][^-.]*){0,1}(\.[^-.]+){1,2}\? {
    /*
     * Transitional case rule...
     * Identify a following terminal <type-id> sequence; set up
     * `signature' storage to capture it, and initiate FINAL phase
     * of <archive-name> scan.
     */
    BEGIN FINAL;
    phase = 0;
    name[mark++] = '\0';
    signature[index = PACKAGE_COMPONENT_CLASS] = name + mark;
    yyless( 0 );
  }

<TRANS>([%&*][.-])|([0-9]+[.-]) {
    /*
     * Transitional case rule...
     * Found a purely numeric following element, such as a <major>
     * version number field, or a <serial-number>.
     */
    BEGIN INITIAL;
    if( ++phase < 3 )
    {
      /* For a version number element,
       * terminate the preceding name element, and set up the
       * `signature' table to capture the version number.
       */
      name[mark++] = '\0';
      signature[++index] = name + mark;
    }
    /* For any other numeric element class,
     * simply advance the position marker, and leave the content
     * to be retrieved by a general (INITIAL) case rule.
     */
    else ++mark;
    yyless( 0 );
  }

<TRANS>. {
    /*
     * Transitional case rule...
     * Handle any other non-specific element type,
     * found after an element delimiter.
     */
    BEGIN INITIAL;
    if( phase )
    {
      /* When processing a <version> or <build-id> element,
       * terminate it.
       */
      name[mark++] = '\0';
      if( phase < 2 )
	/*
	 * ...and if we haven't reached a <build-id>, then
	 * there isn't one here; leave its pointer unassigned.
	 */
	++index;

      /* Save pointer to next element,
       * (which should be <subsystem-name>),
       * and reset phase accordingly.
       */
      signature[++index] = name + mark;
      phase = 0;
    }
    /* Update element marker, and leave content to be scanned
     * on return to the INITIAL state.
     */
    else ++mark;
    yyless( 0 );
  }

<FINAL>\. {
    /*
     * Wrap up processing rule...
     * Found a "." element separator, so move on to capture
     * the next element of the <type-id>...
     */
    if( index < PACKAGE_COMPONENT_VERSION )
      /*
       * ...omitting the <component-version>, which has either
       * been captured already, or isn't present.
       */
      ++index;
    name[mark++] = '\0';
    signature[++index] = name + mark;
    return yyleng;
  }

<FINAL>- {
    /*
     * Wrap up processing rule...
     * Found a "-" element separator...
     */
    if( index == PACKAGE_COMPONENT_CLASS )
    {
      /* ...this should occur only to separate
       * the <component-version> from the <component-class>;
       * in this case, terminate the <component-class>, and
       * prepare to capture the <component-version>.
       */
      name[mark++] = '\0';
      signature[++index] = name + mark;
    }
    return yyleng;
  }

<FINAL>[^.?-]* {
    /*
     * Wrap up processing rule...
     * Found element content; adjust mark to its end.
     */
    return mark += yyleng;
  }

<FINAL>\? {
    /*
     * Wrap up processing rule...
     * Found the sentinel for the end of <archive-name>;
     * delete it, and we should be done.
     */
    name[mark] = '\0';
  }

%%

void *get_pkginfo( const char *name, pkginfo_t signature )
{
  if( (*signature = malloc( strlen( name ) + 3)) != NULL )
  {
    int start = PACKAGE_NAME;
    sprintf( *signature, "%s?", name );
    yy_scan_string( *signature );
    while( (start = yylex( start, signature, *signature )) > 0 )
      ;
    yy_delete_buffer( YY_CURRENT_BUFFER );
  }
  return *signature;
}

/* $RCSfile: pkginfo.l,v $: end of file */
