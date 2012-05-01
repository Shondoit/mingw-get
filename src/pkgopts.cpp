/*
 * pkgopts.cpp
 *
 * $Id: pkgopts.cpp,v 1.1 2012/05/01 20:01:41 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2012, MinGW Project
 *
 *
 * Implementation of XML interpreter for configuation of preferences.
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
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "dmh.h"

#include "pkgbase.h"
#include "pkgkeys.h"
#include "pkgopts.h"

#define STATIC_INLINE  static __inline__ __attribute__((__always_inline__))

#define declare_hook(NAME)      initialise_hook(MINGW_GET_##NAME##_HOOK)
#define initialise_hook(NAME)   static const char *NAME = stringify_hook(NAME)
#define stringify_hook(NAME)    #NAME

declare_hook(DESKTOP);
declare_hook(START_MENU);

#define PKG_DESKTOP_HOOK	MINGW_GET_DESKTOP_HOOK, desktop_options
#define PKG_START_MENU_HOOK	MINGW_GET_START_MENU_HOOK, start_menu_options

#define desktop_options 	desktop_option, all_users_option
#define start_menu_options	start_menu_option, all_users_option

static const char *desktop_option = "--desktop";
static const char *start_menu_option = "--start-menu";
static const char *all_users_option = "--all-users";

#define opt_strcmp(OPT,KEY)	strcmp( OPT, KEY + 2 )

class pkgPreferenceEvaluator
{
  /* A locally implemented class, providing the interpreter for
   * the content of any <preferences>...</preferences> sections
   * found within the XML profile database.
   */
  public:
    pkgPreferenceEvaluator( pkgXmlNode *opt ):ref( opt ){}
    void GetNext( const char *key ){ ref = ref->FindNextAssociate( key ); }
    const char *SetName(){ return optname = ref->GetPropVal( name_key, NULL ); }
    const char *SetName( const char *name ){ return optname = name; }
    void PresetScriptHook( int, const char *, ... );
    void SetScriptHook( const char *, ... );
    pkgXmlNode *Current(){ return ref; }

  private:
    pkgXmlNode *ref;
    const char *optname;

    inline const char *SetOptions( const char *, va_list, const char * );
    inline const char *ValidatedOption( const char *, va_list );
    inline const char *AttributeError( unsigned, const char * );
};

/* XML tag/attribute key names, used exclusively within this module.
 */
static const char *prefs_key = "preferences";
static const char *option_key = "option";
static const char *value_key = "value";

STATIC_INLINE int pkg_setenv( const char *varname, const char *value )
{
  /* A helper function, approximating the effect of POSIX' setenv(),
   * (which Windows lacks), when invoked as setenv( varname, value, 1 )
   */
  char assignment[2 + strlen( varname ) + strlen( value )];
  sprintf( assignment, "%s=%s", varname, value );
  return putenv( assignment );
}

const char *pkgPreferenceEvaluator::ValidatedOption
( const char *keyword, va_list valid_opts )
{
  /* A private helper method, used by the SetOptions() method
   * to filter out any value assignment, which may not be valid
   * for the option being processed.
   */
  unsigned matched = 0;
  const char *retval = NULL, *chkval;
  while( (chkval = va_arg( valid_opts, const char * )) != NULL )
  {
    /* For each permitted matching keyword in turn...
     */
    const char *chk = chkval;
    while( *chk == '-' )
      /*
       * ...and NOT considering leading hyphens...
       */
      ++chk;

    /* ...capture the first permitted keyword with initial substring
     * matching the option under test; continue to check for, and count
     * any further possible matches.
     */
    if( (strncmp( chk, keyword, strlen( keyword ) ) == 0) && (++matched == 1) )
      retval = chkval;
  }
  /* Exactly one match is a successful outcome; otherwise punt.
   */
  return (matched == 1) ? retval : AttributeError( matched, keyword );
}

const char *pkgPreferenceEvaluator::AttributeError
( unsigned why_failed, const char *keyword_not_validated )
{
  /* Private helper method, used by ValidatedOption() to diagnose
   * ambiguous, or simply failed, keyword matches.
   */
  dmh_notify( DMH_WARNING,
      "option '%s': %s attribute '%s' ignored\n", optname,
      why_failed ? "ambiguous" : "invalid", keyword_not_validated
    );

  /* ValidatedOption() uses this NULL return value, as its own
   * failed status return value.
   */
return NULL;
}

const char *pkgPreferenceEvaluator::SetOptions
( const char *value, va_list valid_opts, const char *extra )
{
  /* A private helper method to generate the list of attribute
   * values to be appended to the environment variable assignment,
   * when creating an IPC hook for use by the lua interpreter.
   *
   * Note that the resultant attribute list is specified on the
   * heap, and the caller must free the associated memory.
   */
  if( (extra != NULL) && (strncmp( extra, value_none, strlen( extra )) == 0) )
    /*
     * Passing an extra attribute of "none" is a special case;
     * it overrides all automatic attribute settings.
     */
    value = strdup( value_none );

  else if( ((value = strdup( value )) != NULL) && (extra != NULL) )
  {
    /* This is the normal case.  The extra attribute string represents
     * a comma or space separated list of attributes to be appended to
     * the automatic attribute settings; we must decompose it, so that
     * we may validate each specified attribute.
     */
    char extlist[1 + strlen( extra )];
    char *next = strcpy( extlist, extra );
    while( *next )
    {
      /* We may still have unparsed extra attributes...
       */
      while( (*next == ',') || isspace( *next ) )
	/*
	 * ...so, first strip away any leading separators...
	 */
	++next;
      if( *next )
      {
	/* ...and, when any further potential attribute remains...
	 */
	const char *attribute = next;
	while( *next && (*next != ',') && (! isspace( *next )) )
	  /*
	   * ...mark and collect the unvalidated attribute name...
	   */
	  ++next;
	if( *next )
	  /*
	   * ...NUL terminate it, and prepare to process the following
	   * attribute, if any.
	   */
	  *next++ = '\0';

	/* Validate the collected attribute, expanding any abbreviation
	 * to the fully qualified, unabridged attribute name; when valid...
	 */
	if( (attribute = ValidatedOption( attribute, valid_opts )) != NULL )
	{
	  /* ...create additional space within the dynamically allocated
	   * heap block, in which the validated attribute list is to be
	   * returned...
	   */
	  void *tmp_value = (void *)(value);
	  size_t realloc_len = 2 + strlen( attribute ) + strlen( value );
	  if( (tmp_value = realloc( tmp_value, realloc_len )) != NULL )
	  {
	    /* ...and append the last validated attribute.
	     */
	    sprintf( (char *)(tmp_value), "%s %s", tmp_value, attribute );
	    value = (const char *)(tmp_value);
	  }
	}
      }
    }
  }
  /* Return the resultant validated attribute list.
   */
  return value;
}

void pkgPreferenceEvaluator::PresetScriptHook( int index, const char *key, ... )
{
  /* Method to interpret options specified on the command line,
   * and initialise associated environment variable hooks, such
   * that they override XML preference settings.
   */
  if( pkgOptions()->IsSet( index ) && (key != NULL) && (*key != '\0') )
  {
    /* The indexed command line option has been specified, and
     * it is associated with a viable environment variable name;
     * validate any option arguments against the variadic list
     * of valid attribute values...
     */
    va_list argv;
    va_start( argv, key );
    const char *value = va_arg( argv, const char * );
    const char *preset = pkgOptions()->GetString( index );
    if( (value = SetOptions( SetName( value ), argv, preset )) != NULL )
    {
      /* ...initialise the environment variable accordingly,
       * and release the temporary heap memory block returned
       * by the validation routine.
       */
      pkg_setenv( key, value );
      free( (void *)(value) );
    }
    va_end( argv );
  }
}

void pkgPreferenceEvaluator::SetScriptHook( const char *key, ... )
{
  /* Method to interpret options specified as XML preferences,
   * and assign values to their associated environment variable
   * hooks, provided no prior assignment based on command line
   * settings is in place.
   */
  if( (key != NULL) && (*key != '\0') )
  {
    /* The environment variable name is viable; provided there
     * is no prior assignment to it, validate the content of any
     * specified preferences attribute against the variadic list
     * of valid attribute values...
     */
    va_list argv;
    va_start( argv, key );
    const char *value = va_arg( argv, const char * );
    const char *old_value = getenv( key );
    if( old_value == NULL )
    {
      /* ...i.e. there is no prior assignment...
       */
      if( value != NULL )
      {
	/* ...and there is at least one qualifying attribute...
	 */
	value = SetOptions( value, argv, ref->GetPropVal( value_key, NULL ) );
	if( value != NULL )
	{
	  /* ...populate the environment variable accordingly,
	   * and release the temporary heap memory block returned
	   * by the validation routine.
	   */
	  pkg_setenv( key, value );
	  free( (void *)(value) );
	}
      }
    }
    else if( (value == NULL) && (strcmp( old_value, value_none ) == 0) )
      /*
       * This is a special case, in which the environment variable
       * hook has been explicitly initialised to "none", and there
       * is an explicitly NULL qualifying attribute; this is used
       * to explicitly request that the environment variable hook
       * should be deleted.
       */
      pkg_setenv( key, "" );

    va_end( argv );
  }
}

void pkgXmlDocument::EstablishPreferences()
{
  /* Method to interpret the content of any "preferences" sections
   * appearing within the XML database.
   */
  pkgXmlNode *dbase_root;
  if( (dbase_root = GetRoot()) != NULL )
  {
    /* Initialise preferences set by command line options.
     */
    pkgPreferenceEvaluator opt( dbase_root );
    opt.PresetScriptHook( OPTION_DESKTOP, PKG_DESKTOP_HOOK, NULL );
    opt.PresetScriptHook( OPTION_START_MENU, PKG_START_MENU_HOOK, NULL );

    /* Locate the first of any XML "preferences" elements...
     */
    pkgXmlNode *prefs = dbase_root->FindFirstAssociate( prefs_key );
    while( prefs != NULL )
    {
      /* ...and interpret any contained "enable" specifications.
       */
      pkgPreferenceEvaluator opt( prefs->FindFirstAssociate( option_key ));
      while( opt.Current() != NULL )
      {
	const char *optname;
	if( (optname = opt.SetName()) != NULL )
	{
	  if( opt_strcmp( optname, desktop_option ) == 0 )
	    /*
	     * Enable post-install hook for creation of desktop shortcuts;
	     * by default, these are defined for current user only, but may
	     * optionally be provided for all users.
	     */
	    opt.SetScriptHook( PKG_DESKTOP_HOOK, NULL );

	  else if( opt_strcmp( optname, start_menu_option ) == 0 )
	    /*
	     * Similarly, enable hook for creation of start menu shortcuts.
	     */
	    opt.SetScriptHook( PKG_START_MENU_HOOK, NULL );

	  else
	    /* Any unrecognised option specification is simply ignored,
	     * after posting an appropriate diagnostic message.
	     */
	    dmh_notify( DMH_WARNING, "unknown option '%s' ignored\n", optname );
	}
	/* Repeat for any further "enable" specifations...
	 */
	opt.GetNext( option_key );
      }
      /* ...and for any additional "preferences" sections.
       */
      prefs = prefs->FindNextAssociate( prefs_key );
    }

    /* Finally, remove any environment variable hooks which have been
     * created for options which are to be explicitly disabled.
     */
    opt.SetScriptHook( MINGW_GET_DESKTOP_HOOK, NULL );
    opt.SetScriptHook( MINGW_GET_START_MENU_HOOK, NULL );
  }
}

/* $RCSfile: pkgopts.cpp,v $: end of file */
