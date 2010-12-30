/*
 * pkgshow.cpp
 *
 * $Id: pkgshow.cpp,v 1.1 2010/12/30 23:23:43 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, MinGW Project
 *
 *
 * Implementation of the classes and methods required to support the
 * 'mingw-get list' and 'mingw-get show' commands.
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
#include <stdio.h>
#include <stdarg.h>

#include "pkgbase.h"
#include "pkgkeys.h"

#include "dmh.h"

/* Translatable message elements, to support internationalisation.
 * FIXME: These need to be abstracted into a global message header,
 * where catalogue numbering sequences may be implemented, and more
 * readily managed for future internationalisation.
 */
#define PKGMSG_DIRECTORY_HEADER_FORMAT	"\nPackage: %*sSubsystem: %s\n"

EXTERN_C const char *pkgMsgUnknownPackage( void );

/* Utility classes and definitions for interpreting and formatting
 * package descriptions, encoded as UTF-8.
 */
typedef unsigned long utf32_t;

#define UTF32_MAX			(utf32_t)(0x10FFFFUL)
#define UTF32_OVERFLOW			(utf32_t)(UTF32_MAX + 1UL)

#define UTF32_INVALID			(utf32_t)(0x0FFFDUL)

class pkgUTF8Parser
{
  /* A class for parsing a UTF-8 string, decomposing it into
   * non-whitespace substrings, separated by whitespace, which
   * is interpreted as potential word-wrap points.
   */
  public:
    pkgUTF8Parser( const char* );
    ~pkgUTF8Parser(){ delete next; }

  protected:
    class pkgUTF8Parser *next;
    union { char utf8; wchar_t utf16; utf32_t utf32; } codepoint;
    const char *text;
    int length;

    const char *ScanBuffer( const char* );
    wchar_t GetCodePoint( ... );
};

/* Constructor...
 */
pkgUTF8Parser::pkgUTF8Parser( const char *input ):
text( NULL ), length( 0 ), next( NULL )
{
  /* Recursively construct a parse list for the input string; the
   * first node marks the location of the first non-whitespace UTF-8
   * entity in the input, while each successive node marks locations
   * of internal non-whitespace substrings, each separated from its
   * predecessor by whitespace.
   *
   * FIXME: The scanner implemented here is naive WRT layout of text,
   * insofar as it considers all whitespace to represent valid points
   * at which word breaks may be inserted, and it assumes that all code
   * points represent glyphs which occupy equal horizontal space, (thus
   * ignoring the special spacing requirements of diacritical marks, or
   * other non-spacing entities.  In the case of conventionally typeset
   * English text, in a monospaced font, this should be acceptable, but
   * it isn't universally so.  We may ultimately wish to progress to a
   * solution based on ICU, or similar, to support other languages.
   */
  const char *mark;

  /* First, step over any initial whitespace...
   */
  while( *input && iswspace( GetCodePoint( mark = ScanBuffer( input ))) )
    input = mark;

  /* If we didn't run out of input...
   */
  if( *input )
  {
    /* ...then we've found the start of a non-whitespace substring;
     * mark it, and then...
     */
    text = input;
    while( *input && ! iswspace( GetCodePoint( mark = ScanBuffer( input ))) )
    {
      /* ...scan ahead, until we again encounter whitespace, or
       * we run out of input; as we go, accumulate the effective
       * length of the current substring.
       */
      input = mark; ++length;
    }
    /* Continue to recursively add parse nodes, to mark the
     * locations of any further non-whitespace substrings.
     */
    next = new pkgUTF8Parser( input );
  }
}

const char *pkgUTF8Parser::ScanBuffer( const char *input )
{
  /* Read one or more octets from a UTF-8 encoded string/stream,
   * and identify the single code point value associated with the
   * initial octet sequence.  Leave this for subsequent retrieval
   * by GetCodePoint(); return a pointer to the next available
   * octet sequence, (if any).
   *
   * We begin by loading the first available octet into the code
   * point buffer, while zero filling the higher order octet space;
   * if the resultant doesn't have the high order bit of the least
   * significant octet set, then the octet represents a seven-bit
   * ASCII code, and there is nothing more to be done...
   */
  if( (codepoint.utf32 = 0xFFUL & (utf32_t)(*input++)) > 0x7FUL )
  {
    /* ...otherwise we have what is potentially the first octet
     * in a multibyte UTF-8 encoding sequence; if this is a well
     * formed sequence, then the number of contiguous high order
     * bits which are set, (between two and six), indicates the
     * length of the sequence, so we count them...
     */
    char mask = 0x80; int contbytes = 0;
    if( codepoint.utf32 == 0xFFUL )
      /*
       * This is a degenerate form; if it were valid, (which it
       * isn't), it would represent a sequence of eight octets,
       * (the current one, plus seven continuation octets), all
       * of which (together) encode a single code point; however,
       * we cannot detect this by locating the most significant
       * bit which isn't set, because no such bit is present,
       * so we simply set the continuation count directly.
       */
      contbytes = 7;

    /* Any other octet value must have at least one bit which is
     * not set; we may determine the appropriate sequence length
     * by repeated testing of high-order bits, (using a mask with
     * a progressively incrementing number of such bits set)...
     */
    else while( (codepoint.utf8 & (mask = 0x80 | (mask >> 1))) == mask )
      /*
       * ...incrementing the continuation count on each cycle,
       * while we continue to match high order bits which are
       * set; (the first mismatch indicates the location of the
       * first high-order bit in the data, which is not set).
       */
      ++contbytes;

    /* The original UTF-8 specification forbids any more than
     * five continuation bytes...
     */
    if( contbytes > 5 )
      /*
       * ...so mark as an UTF-32 overflow...
       */
      codepoint.utf32 = UTF32_OVERFLOW;

    else
      /* At this point, our mask covers the range of bits in the
       * input octet, which serve only to indicate the length of
       * the ensuing encoding sequence; they do not represent any
       * part of the encoded data, so discard them.
       */
      codepoint.utf8 &= ~mask;

    /* Any valid UTF-8 encoding sequence MUST be the shortest
     * possible, to represent the equivalent UTF-32 code point
     * value; (arbitrarily longer sequences may decode to the
     * same value, but are deemed invalid).  To permit us to
     * detect over-long encoding sequences, we make a note of
     * the smallest UTF-32 value which cannot be encoded with
     * fewer than the currently determined sequence length.
     */
    utf32_t minval = (contbytes == 1) ? 0x80UL : 0x100UL << (5 * contbytes);

    /* We now have the significant data portion of the first
     * input octet, already loaded into the code point buffer,
     * and we know how many additional bytes are required to
     * complete the the sequence.  We also know that all valid
     * continuation octets must conform to a "10xxxxxx" binary
     * bit pattern; thus we may loop over the expected number
     * of continuation octets, validating and accumulating...
     */
    do { if( (*input & 0xC0) != 0x80 )
	 {
	   /* Invalid UTF-8 sequence continuation byte;
	    * must bail out immediately, leaving the current
	    * input byte as a possible starting point for
	    * the next encoding sequence...
	    */
	   codepoint.utf32 = UTF32_INVALID;
	   return input;
	 }
	 /* If we get past the preceding validity check,
	  * the current octet is a valid UTF-8 continuation
	  * byte...
	  */
	 else if( codepoint.utf32 < UTF32_OVERFLOW )
	 {
	   /* ...provided we haven't already overflowed the
	    * UTF-32 accumulator, shift the accumulated code
	    * point value to the left, to accommodate the next
	    * sextet of data bits to be added, and update the
	    * accumulator by merging them in...
	    */
	   codepoint.utf32 <<= 6;
	   codepoint.utf8 |= *input++ & ~0xC0;
	 }
	 /* ...continuing until all anticipated continuation
	  * bytes have been processed.
	  */
       } while( --contbytes > 0 );

    /* Reject any other abnormal encoding sequences, such as...
     *
     *   - an overlong encoding; (could have been encoded with
     *     a shorter octet sequence)...
     */
    if(  (codepoint.utf32 < minval)
      /*
       * - any value greater than the maximum specified by the
       *   Unicode Standard...
       */
    ||   (codepoint.utf32 > UTF32_MAX)
      /*
       * - any value in the range 0xD800..0xDFFF; (represents
       *   one element of a UTF-16 surrogate pair, and is thus
       *   invalid as a free-standing UTF-32 code point)...
       */
    ||  ((codepoint.utf32 >= 0xD800UL) && (codepoint.utf32 <= 0xDFFFUL))  )
      /*
       * FIXME: there may be other invalid sequences; should we
       * add tests to detect the ilk of non-characters or private
       * use code points, and also flag them as invalid?  They
       * are, after all still valid Unicode scalar values, even
       * if they don't map to any specific grapheme.
       */
      codepoint.utf32 = UTF32_INVALID;
  }
  /* We always return a pointer to the next available octet in
   * the input string/stream; this represents the point at which
   * scanning should resume, to retrieve the code point associated
   * with the next available character encoding entity.
   */
  return input;
}

wchar_t pkgUTF8Parser::GetCodePoint( ... )
{
  /* Retrieve the code point value representing the last scanned
   * UTF-8 single code point sequence, encoding it as UTF-16.
   *
   * NOTE:
   * While we don't actually use any argument in this routine,
   * we allow any to be arbitrarily specified; this is to support
   * calls of the form:
   *
   *   value = GetCodePoint( input = ScanBuffer( input ) );
   *
   */
  if( codepoint.utf32 > 0xFFFFUL )
  {
    /* This code point lies beyond the BMP; we must decompose it
     * to form a surrogate pair, returning the leading surrogate
     * immediately, while leaving the trailing surrogate behind,
     * in the code point buffer, to be fetched by a subsequent
     * call; (we place the onus on the caller to complete this
     * call-back, to retrieve the trailing surrogate).
     */
    utf32_t retval = (codepoint.utf32 >> 10) + 0xD800UL - (0x10000UL >> 10);
    codepoint.utf32 = 0xDC00UL | (codepoint.utf32 & 0x3FFUL);
    return (wchar_t)(retval & 0xFFFFUL);
  }
  /* Except in the special case of returning a leading surrogate
   * above, whether the code point value lies within the BMP, or
   * it is the residual trailing surrogate from an immediately
   * preceding decomposition, it is already representable as a
   * UTF-16 value, so simply return it.
   */
  return codepoint.utf16;
}

class pkgNroffLayoutEngine : public pkgUTF8Parser
{
  /* A class for laying out a UTF-8 encoded string as a rudimentary
   * "nroff" style word-wrapped and justified paragraph.
   */
  public:
    pkgNroffLayoutEngine( const char *input ):pkgUTF8Parser( input ){}
    pkgNroffLayoutEngine *WriteLn( int = 0, int = 0, int = 65 );
};

pkgNroffLayoutEngine*
pkgNroffLayoutEngine::WriteLn( int style, int offset, int maxlen )
{
  /* Method to lay out a single line of text, with a specified
   * justification style, left margin offset and line length.
   *
   * Justification is achieved by insertion of extra padding
   * space at natural word breaks, commencing at left and right
   * ends of successive lines alternately; this alternation is
   * controlled by the static "padmode" flag.
   */
  static int padmode = 1;
  pkgNroffLayoutEngine *curr = this;

  int count = 0; ++maxlen;
  while( (curr != NULL) && (maxlen > curr->length) )
  {
    /* Determine the maximum number of "words" from the output
     * stream which will fit on the current line, and discount
     * "maxlen" by the space they will occupy, so establishing
     * the number of padding spaces which will be required for
     * justification in the specified style.
     */
    ++count;
    maxlen -= 1 + curr->length;
    curr = (pkgNroffLayoutEngine *)(curr->next);
  }

  while( offset-- > 0 )
    /*
     * Emit white space to fill the left margin.
     */
    putwchar( '\x20' );

  if( style < 0 )
    /*
     * Justification style is "flush right"; insert all
     * necessary padding space at the left margin.
     */
    while( maxlen-- > 0 )
      putwchar( '\x20' );

  else if( curr == NULL )
  {
    /* The output text stream has been exhausted; the
     * current line will be the last in the "paragraph";
     * we do not justify it.
     */
    style = 0;
    /*
     * Additionally, we reset "padmode" to ensure that
     * each subsequent paragraph starts in the same phase
     * of alternation of padding character insertion.
     */
    padmode = 1;
  }

  /* For justification, padding spaces will be distributed
   * as evenly as possible over all word breaks within the
   * line; where an exactly equal distribution cannot be
   * achieved...
   */
  int lpad = 0;
  if( (style > 0) && ((padmode = 1 - padmode) == 1) )
  {
    /* ...and for alternating lines on which wider spacing
     * is inserted from the left, we calculate the number of
     * leftmost word breaks in which one extra padding space
     * is to be inserted.
     */
    lpad = maxlen % (count - 1);
    maxlen -= lpad;
  }

  /* Back up to the first word on the current output line...
   */
  curr = this;
  /*
   * ...then for all words comprising this line...
   */
  while( count > 0 )
  {
    /* ...emit the glyphs comprising one word.
     */
    while( curr->length-- > 0 )
      putwchar( curr->GetCodePoint( curr->text = curr->ScanBuffer( curr->text )) );

    if( --count > 0 )
    {
      /* There are more words to be placed on this line...
       */
      if( style > 0 )
      {
	/* ...and justification is required...
	 */
	if( lpad-- > 0 )
	  /*
	   * ...and this line has pending left-weighted
	   * extra padding, so this word break gets one
	   * extra space...
	   */
	  putwchar( '\x20' );

	for( int pad = maxlen / count; pad > 0; --pad )
	{
	  /* ...plus any additional equally distributed
	   * padding spaces; (note that this will stretch
	   * automatically towards the right end of the
	   * line, for right-weighted padding)...
	   */
	  --maxlen; putwchar( '\x20' );
	}
      }
      /* Ensure there is always at least one space separating
       * adjacent words; this has already been included in the
       * layout computation, in addition to the extra padding
       * spaces just inserted.
       */
      putwchar( '\x20' );
    }
    /* Move on to the next word, if any, in the accumulated
     * output stream; (note that this may represent the first
     * word to be placed on the next line).
     */
    curr = (pkgNroffLayoutEngine *)(curr->next);
  }
  /* Emit a line break at end of line.
   */
  putwchar( '\n' );

  /* Return a pointer to the first word to be placed on the next
   * line, or NULL at end of paragraph.
   */
  return curr;
}

class pkgDirectoryViewerEngine
{
  /* A minimal abstract base class, through which directory
   * traversal methods of the following "pkgDirectory" class
   * gain access to an appropriate handler in a specialised
   * directory viewer class, (or any other class which may
   * provide directory traversal hooks).  All such helper
   * classes should derive from this base class, providing
   * a "Dispatch" method as the directory traversal hook.
   */
  public:
    virtual void Dispatch( pkgXmlNode* ) = 0;
#if 0
    pkgDirectoryViewerEngine()
    {
      for( int i = 1; i < 9; i++ ) printf( "%10d", i ); putchar( '\n' );
      for( int i = 1; i < 9; i++ ) printf( "1234567890" ); putchar( '\n' );
    }
#endif
};

class pkgDirectory
{
  /* A locally defined class, used to manage a list of package
   * or component package references in the form of an unbalanced
   * binary tree, such that an in-order traversal will produce
   * an alpha-numerically sorted package list.
   */
  public:
    pkgDirectory( pkgXmlNode* );
    pkgDirectory *Insert( const char*, pkgDirectory* );
    void InOrder( pkgDirectoryViewerEngine* );
    ~pkgDirectory();

  protected:
    pkgXmlNode *entry;
    pkgDirectory *prev;
    pkgDirectory *next;
};

/* Constructor...
 */
pkgDirectory::pkgDirectory( pkgXmlNode *item ):
entry( item ), prev( NULL ), next( NULL ){}

pkgDirectory *pkgDirectory::Insert( const char *keytype, pkgDirectory *newentry )
{
  /* Add a new package or component reference to a directory compilation,
   * using an unbalanced binary tree representation to achieve a sorted
   * listing, in ascending alpha-numeric collating order.
   */
  if( this && newentry )
  {
    /* We have an existing directory to augment, and a valid reference
     * pointer for a new directory entry; we must locate the appropriate
     * insertion point, to achieve correct sort order.
     */
    pkgDirectory *refpt, *pt = this;
    const char *mt = "", *ref = newentry->entry->GetPropVal( keytype, mt );
    do { refpt = pt;
	 if( pt->prev && strcmp( ref, pt->prev->entry->GetPropVal( keytype, mt )) < 0 )
	   /*
	    * Follow predecessor reference pointers, until we locate
	    * the last with an entry which must sort AFTER the new
	    * reference to be inserted...
	    */
	   pt = pt->prev;

	 if( pt->next && strcmp( ref, pt->next->entry->GetPropVal( keytype, mt )) > 0 )
	   /*
	    * ...and similarly for successor references, until we
	    * locate the last to sort BEFORE the new entry...
	    */
	   pt = pt->next;

	 /* ...repeating each of the above, until the insertion point
	  * has been located.
	  */
       } while( refpt != pt );

    /* We've found the insertion point...
     */
    if( strcmp( ref, pt->entry->GetPropVal( keytype, mt )) < 0 )
    {
      /* ...and the new entry must become its immediate predecessor
       * in dictionary sort order; displace the current predecessor,
       * if any, to make way for this insertion...
       */
      if( pt->prev && strcmp( ref, pt->prev->entry->GetPropVal( keytype, mt )) > 0 )
	/*
	 * In this case, the original predecessor must also appear
	 * before the new entry, in dictionary sort order...
	 */
	newentry->prev = pt->prev;
      else
	/* ...or in this alternative case, it must appear as a
	 * successor, in the sort order.
	 */
	newentry->next = pt->prev;

      /* Finally, attach the new entry at the appropriate insertion point.
       */
      pt->prev = newentry;
    }
    else
    { /* Similarly, for the case where the new entry must appear as
       * a successor to the insertion point entry, we displace the
       * original successor entry, if any...
       */
      if( pt->next && strcmp( ref, pt->next->entry->GetPropVal( keytype, mt )) > 0 )
	/*
	 * ...re-attaching it as predecessor...
	 */
	newentry->prev = pt->next;
      else
	/* ...or as successor to the new entry, as appropriate...
	 */
	newentry->next = pt->next;

      /* ...again finishing by attaching the new entry at the insertion point.
       */
      pt->next = newentry;
    }
  }
  /* Finally, we return the pointer to either the original root node in the
   * directory tree, or if that had never been previously assigned, then the
   * new entry, which will become the root of a new directory tree.
   */
  return this ? this : newentry;
}

void pkgDirectory::InOrder( pkgDirectoryViewerEngine *action )
{
  /* Perform an in-order traversal of a package directory tree,
   * invoking a specified processing action on each node in turn;
   * note that this requires a pointer to a concrete instance of
   * a "Viewer" class, derived from the abstract "ViewerEngine".
   */
  if( this )
  {
    /* Proceeding only when we have a valid directory object,
     * recursively traverse the "left hand" (prev) sub-tree...
     */
    prev->InOrder( action );
    /*
     * ...processing the current node when no unprocessed
     * "left hand" sub-tree nodes remain...
     */
    action->Dispatch( entry );
    /*
     * ...then finish off with a recursive traversal of the
     * "right-hand" (next) sub-tree...
     */
    next->InOrder( action );
  }
}

/* Destructor...
 */
pkgDirectory::~pkgDirectory()
{
  /* We need to recursively delete any "prev" and "next" references
   * for the current directory node, as a prequisite to deleting the
   * current node itself.
   */
  delete prev;
  delete next;
}

class pkgDirectoryViewer : public pkgDirectoryViewerEngine
{
  /* A concrete class, providing the directory traversal hooks
   * for display of package directory content in a CLI console.
   */
  public:
    pkgDirectoryViewer();
    virtual void Dispatch( pkgXmlNode* );

  protected:
    int page_width;
    virtual void EmitHeader( pkgXmlNode* );
    virtual void EmitDescription( pkgXmlNode*, const char* = NULL );
    int ct;
};

/* Constructor...
 */
pkgDirectoryViewer::pkgDirectoryViewer()
{
  /* Set up the display width, for word-wrapped output...
   */
  const char *cols;
  if( (cols = getenv( "COLS" )) != NULL )
  {
    /* In cases where the COLS environment variable defines the
     * terminal width, then use 90% of that...
     *
     * FIXME: consider querying the terminfo database, where one
     * is available, (not likely on MS-Windows), to get a default
     * value when COLS is not set...
     */
    page_width = atoi( cols );
    page_width -= page_width / 10;
  }
  else
    /* When no COLS, (or terminfo), setting is available, then
     * assume a default 80-column console, and set 90% of that.
     */
    page_width = 72;

  /* Set single entry output mode, for component package listings.
   */
  ct = -1;
}

void pkgDirectoryViewer::EmitHeader( pkgXmlNode *entry )
{
  /* Method to display package identification information
   * in a standard console (CLI) based package directory listing;
   * package name is presented flush left, while the associated
   * subsystem identification is presented flush right, both
   * on a single output line, at nominated console width.
   *
   * NOTE:
   * There is no provision to accommodate package or subsystem
   * names containing other than ASCII characters; we assume that
   * such names are restricted to the ASCII subset of UTF-8.
   */
  const char *fmt = PKGMSG_DIRECTORY_HEADER_FORMAT;
  const char *pkg = entry->GetPropVal( name_key, value_unknown );
  const char *sys = entry->GetContainerAttribute( subsystem_key, value_unknown );

  /* Justification is achieved by adjustment of the field width
   * used to format the package name, (in "%*s" format).  We use
   * snprintf to determine the required width, initially relative
   * to a default 80-column console, then adjust it to the actual
   * nominated width; the additional two in the adjustment is to
   * discount the leading and trailing '\n' codes, which
   * surround the formatted output string.
   */
  int padding = snprintf( NULL, 0, fmt, 80, pkg, sys ) - (80 + page_width + 2);
  printf( fmt, padding, pkg, sys );
}

static __inline__ __attribute__((__always_inline__))
void pkgNroffLayout( int style, int offset, int width, const char *text )
{
  /* Wrapper to facilitate layout of a single paragraph of text,
   * with specified justification style, left margin offset and
   * page width, using an nroff style layout engine.
   */
  pkgNroffLayoutEngine layout( text ), *output = &layout;

  /* Precede each paragraph by one line height vertical space...
   */
  putchar( '\n' );
  while( output != NULL )
    /*
     * ...then write out the text, line by line, until done.
     */
    output = output->WriteLn( style, offset, width );
}

static int offset_printf( int offset, const char *fmt, ... )
{
  /* Helper to emit regular printf() formatted output, with
   * a preset first line indent.
   */
  va_list argv;
  va_start( argv, fmt );

  /* We need a temporary storage location, to capture the
   * printf() return value.
   */
  int retval;

  /* Fill the left margin with whitespace, to the first line
   * indent width.
   */
  while( offset-- > 0 )
    putchar( '\x20' );

  /* Emit the printf() output...
   */
  retval = vprintf( fmt, argv );
  va_end( argv );

  /* ...and return its result.
   */
  return retval;
}

static void underline( int underline_glyph, int offset, int len )
{
  /* Helper to underline a preceding line of text, of specified
   * length, with a specified repeating glyph, beginning at a
   * specified left margin offset.
   */
  while( offset-- > 0 )
    /*
     * Fill the left margin with whitespace...
     */
    putchar( '\x20' );
  while( len-- > 0 )
    /*
     * ...followed by the specified number of repetitions
     * of the specified underlining glyph.
     */
    putchar( underline_glyph );
}

void pkgDirectoryViewer::EmitDescription( pkgXmlNode *pkg, const char *title )
{
  /* Method to print formatted package descriptions to the console
   * stdout stream, using the nroff style formatting engine, with
   * fixed (for the time being) zero left margin offset.
   */
  int offset = 0;

  /* These XML element and attribute identification keys should
   * perhaps be defined in the pkgkeys.h header; once more, for
   * the time being we simply define them locally.
   */
  const char *title_key = "title";
  const char *description_key = "description";
  const char *paragraph_key = "paragraph";

  /* The procedure is recursive, selecting description elements
   * from inner levels of the XML document, progressing outward to
   * the document root, then printing them in outer to inner order;
   * proceed only if we have a valid starting package element,
   * within which to search for description elements...
   */
  if( pkg != NULL )
  {
    /* ...in which case, we locate the first of any such
     * elements at the current nesting level.
     */
    pkgXmlNode *desc = pkg->FindFirstAssociate( description_key );
    pkgXmlNode *content = desc;
    while( (title == NULL) && (desc != NULL) )
    {
      /* On first entry, the "title" argument should have been
       * passed as NULL; we subsequently select the first title
       * attribute we encounter, at the innermost level where
       * one is available, to become the title for this
       * package description.
       */
      if( (title = desc->GetPropVal( title_key, NULL )) != NULL )
      {
	/* This is the innermost definition we've encountered for a
	 * title line; print and underline it.  (The -1 discounts the
	 * the page offset padding spaces and trailing newline, which
	 * surround the title text itself in the initial printf(),
	 * when computing the required underline length).
	 */
	putchar( '\n' );
	underline( '-', offset, offset_printf( offset, "%s\n", title ) - 1 );
	putchar( '\n' );
      }
      else
	/* We haven't identified a title yet; check in any further
	 * description elements which appear at this nesting level.
	 */
	desc = desc->FindNextAssociate( description_key );
    }

    /* Regardless of whether we've found a title yet, or not,
     * if we haven't already reached the document root...
     */
    if( pkg != pkg->GetDocumentRoot() )
      /*
       * ...recurse, to inspect the next outward containing element.
       */
      EmitDescription( pkg->GetParent(), title );

    /* Finally, unwind the recursion stack...
     */
    while( content != NULL )
    {
      /* ...selecting paragraph elements from within each
       * description element...
       */
      pkgXmlNode *para = content->FindFirstAssociate( paragraph_key );
      while( para != NULL )
      {
	/* ...and printing each in turn...
	 */
	pkgNroffLayout( 1, offset, page_width - offset, para->GetText() );
	/*
	 * ...repeating for the next paragraph, if any...
	 */
	para = para->FindNextAssociate( paragraph_key );
      }
      /* ...and then proceeding to the next description element, if any,
       * ultimately falling through to unwind the next level of recursion,
       * if any, within which paragraphs from any description elements from
       * inner levels of the document will be printed.
       */
      content = content->FindNextAssociate( description_key );
    }
  }
}

void pkgDirectoryViewer::Dispatch( pkgXmlNode *entry )
{
  /* Method to dispatch requests to display information
   * about a single package entity.
   */
  if( entry->IsElementOfType( package_key ) )
  {
    /* The selected entity is a full package;
     * create an auxiliary directory...
     */
    pkgDirectory *dir = NULL;
    pkgXmlNode *cpt = entry->FindFirstAssociate( component_key );
    while( cpt != NULL )
    {
      /* ...in which we enumerate its components.
       */
      dir = dir->Insert( class_key, new pkgDirectory( cpt ) );
      cpt = cpt->FindNextAssociate( component_key );
    }

    /* Signalling that a component list is to be included...
     */
    ct = 0;
    /* ...print the standard form package name header...
     */
    EmitHeader( entry );
    if( dir != NULL )
    {
      /* ...with included enumeration of the component names...
       */
      dir->InOrder( this ); putchar( '\n' );
      /*
       * ...before discarding the auxiliary directory.
       */
      delete dir;
    }
    /* For this category of information display, we don't include
     * package version information; proceed immediately, to print
     * the description, as it applies to all components...
     */
    EmitDescription( entry );
    /*
     * ...followed by one additional line space; (note that we don't
     * include this within the EmitDescription method itself, because
     * its recursive processing would result in a surfeit of embedded
     * line spaces, within the description).
     */
    putchar( '\n' );

    /* Finally, reset the component tracking enumeration flag to its
     * normal (initial) state.
     */
    ct = -1;
  }

  else if( entry->IsElementOfType( component_key ) )
  {
    /* In this case, the selected entity is a component package...
     */
    if( ct < 0 )
    {
      /* ...and the component tracking state indicates that we are NOT
       * simply enumerating components for the more inclusive style of
       * package information printed in the preceding case.  Thus, we
       * deduce that the user has explicitly requested information on
       * just this specific component package; retrieve and print the
       * standard header information for the containing package...
       */
      EmitHeader( entry->GetParent() );
      /*
       * ...followed by this specific component identification.
       */
      printf( "Component: %s\n", entry->GetPropVal( class_key, value_unknown ) );

      /* In this case, we DO want to print package version information
       * for the specific component package, indicating the installed
       * version, if any, and the latest available in the repository.
       * Thus, we initiate a local action request...
       */
      pkgActionItem avail;
      pkgXmlNode *rel = entry->FindFirstAssociate( release_key );
      while( rel != NULL )
      {
	/* ...to scan all associated release keys, and select the
	 * most recent recorded in the database as available...
	 */
	avail.SelectIfMostRecentFit( rel );
	if( rel->GetInstallationRecord( rel->GetPropVal( tarname_key, NULL )) != NULL )
	  /*
	   * ...also noting if any is marked as installed...
	   */
	  avail.SelectPackage( rel, to_remove );

	/* ...until all release keys have been inspected...
	 */
	rel = rel->FindNextAssociate( release_key );
      }
      /* ...and performing a final check on installation status.
       */
      avail.ConfirmInstallationStatus();

      /* Now print the applicable version information, noting that
       * "none" may be appropriate for the installed version.
       */
      pkgXmlNode *current = avail.Selection( to_remove );
      const char *tarname = avail.Selection()->GetPropVal( tarname_key, NULL );
      printf( "\nInstalled Version:  %s\nRepository Version: %s\n",
	  (current != NULL)
	  ? current->GetPropVal( tarname_key, value_unknown )
	  : value_none,
	  tarname
	);

      /* Finally, collate and print the package description for
       * this component package, with following line space, just
       * as we did in the preceding case.
       */
      EmitDescription( entry );
      putchar( '\n' );
    }

    else
      /* The component package is simply being enumerated, as part
       * of the full package information data; we simply emit its
       * component class name into the package information header.
       */
      printf( ct++ ? ", %s" : "Components: %s",
	  entry->GetPropVal( class_key, value_unknown )
	);
  }
}

void pkgXmlDocument::DisplayPackageInfo( int argc, char **argv )
{
  /* Primary method for retrieval and display of package information
   * on demand from the mingw-get command line interface.
   */
  pkgDirectory *dir = NULL;
  pkgDirectoryViewer output;

  if( argc > 1 )
  {
    /* One or more possible package name arguments
     * were specified on the command line.
     */
    while( --argc > 0 )
    {
      /* Look up each one in the package catalogue...
       */
      pkgXmlNode *pkg;
      if( (pkg = FindPackageByName( *++argv )) != NULL )
      {
	/* ...and, when found, create a corresponding
	 * print-out directory entry, inserting it in
	 * its alphanumerically sorted position.
	 */
	dir = dir->Insert( name_key, new pkgDirectory( pkg ) );
      }
      else
	/* We found no information on the requested package;
	 * diagnose as a non-fatal error.
	 */
	dmh_notify( DMH_ERROR, pkgMsgUnknownPackage(), *argv );
    }
  }

  else
  {
    /* No arguments were specified; interpret this as a request
     * to display information pertaining to all known packages in
     * the current mingw-get repository's universe, thus...
     */
    pkgXmlNode *grp = GetRoot()->FindFirstAssociate( package_collection_key );
    while( grp != NULL )
    {
      /* ...for each known package collection...
       */
      pkgXmlNode *pkg = grp->FindFirstAssociate( package_key );
      while( pkg != NULL )
      {
	/* ...add an entry for each included package
	 * into the print-out directory...
	 */
	dir = dir->Insert( name_key, new pkgDirectory( pkg ) );
	/*
	 * ...enumerating each and every package...
	 */
	pkg = pkg->FindNextAssociate( package_key );
      }
      /* ...then repeat for the next package collection,
       * if any.
       */
      grp = grp->FindNextAssociate( package_collection_key );
    }
  }

  /* Regardless of how the print-out directory was compiled,
   * we hand it off to the common viewer, for output.
   */
  dir->InOrder( &output );

  /* Finally, we are done with the print-out directory,
   * so we may delete it.
   */
  delete dir;
}

/* $RCSfile: pkgshow.cpp,v $: end of file */
