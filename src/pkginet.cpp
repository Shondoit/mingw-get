/*
 * pkginet.cpp
 *
 * $Id: pkginet.cpp,v 1.12 2011/05/29 20:53:37 keithmarshall Exp $
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 * Copyright (C) 2009, 2010, 2011, MinGW Project
 *
 *
 * Implementation of the package download machinery for mingw-get.
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
#define WIN32_LEAN_AND_MEAN

#define _WIN32_WINNT 0x0500	/* for GetConsoleWindow() kludge */
#include <windows.h>
/*
 * FIXME: This kludge allows us to use the standard wininet dialogue
 * to acquire proxy authentication credentials from the user; this is
 * expedient for now, (if somewhat anti-social for a CLI application).
 * We will ultimately need to provide a more robust implementation,
 * (within the scope of the diagnostic message handler), in order to
 * obtain a suitable window handle for use when called from the GUI
 * implementation of mingw-get, (when it becomes available).
 */
#define dmh_dialogue_context()	GetConsoleWindow()

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wininet.h>
#include <errno.h>

#include "dmh.h"
#include "mkpath.h"
#include "debug.h"

#include "pkgbase.h"
#include "pkgkeys.h"
#include "pkgtask.h"

class pkgDownloadMeter
{
  /* Abstract base class, from which facilities for monitoring the
   * progress of file downloads may be derived.
   */
  public:
    /* The working method to refresh the download progress display;
     * each derived class MUST furnish an implementation for this.
     */
    virtual int Update( unsigned long length ) = 0;

  protected:
    /* Storage for the expected size of the active download...
     */
    unsigned long content_length;

    /* ...and a method to format it for human readable display.
     */
    int SizeFormat( char*, unsigned long );
};

class pkgDownloadMeterTTY : public pkgDownloadMeter
{
  /* Implementation of a download meter class, displaying download
   * statistics within a CLI application context.
   */
  public:
    pkgDownloadMeterTTY( const char*, unsigned long );
    virtual ~pkgDownloadMeterTTY();

    virtual int Update( unsigned long );

  private:
    /* This buffer is used to store each compiled status report,
     * in preparation for display on the console.
     */
    const char *source_url;
    char status_report[80];
};

pkgDownloadMeterTTY::pkgDownloadMeterTTY( const char *url, unsigned long length )
{
  source_url = url;
  content_length = length;
}

pkgDownloadMeterTTY::~pkgDownloadMeterTTY()
{
  if( source_url == NULL )
    dmh_printf( "\n" );
}

static inline __attribute__((__always_inline__))
unsigned long pow10mul( register unsigned long x, unsigned power )
{
  /* Inline helper to perform a fast integer multiplication of "x"
   * by a specified power of ten.
   */
  while( power-- )
  {
    /* Each cycle multiplies by ten, first shifting to get "x * 2"...
     */
    x <<= 1;
    /*
     * ...then further shifting that intermediate result, and adding,
     * to achieve the effect of "x * 2 + x * 2 * 4".
     */
    x += (x << 2);
  }
  return x;
}

static inline __attribute__((__always_inline__))
unsigned long percentage( unsigned long x, unsigned long q )
{
  /* Inline helper to compute "x" as an integer percentage of "q".
   */
  return pow10mul( x, 2 ) / q;
}

int pkgDownloadMeterTTY::Update( unsigned long count )
{
  /* Implementation of method to update the download progress report,
   * displaying the current byte count and anticipated final byte count,
   * each formatted in a conveniently human readable style, followed by
   * approximate percentage completion, both as a 48-segment bar graph,
   * and as a numeric tally.
   */
  char *p = status_report;
  int barlen = (content_length > count)
    ? (((count << 1) + count) << 4) / content_length
    : (content_length > 0) ? 48 : 0;

  /* We may safely use sprintf() to assemble the status report, because
   * we control the field lengths to always fit within the buffer.
   */
  p += SizeFormat( p, count );
  p += sprintf( p, " / " );
  p += (content_length > 0)
    ? SizeFormat( p, content_length )
    : sprintf( p, "????.?? ??" );
  p += sprintf( p, "%*c", status_report + 25 - p, '|' ); p[barlen] = '\0';
  p += sprintf( p, "%-48s", (char *)(memset( p, '=', barlen )) );
  p += ( (content_length > 0) && (content_length >= count) )
    ? sprintf( p, "|%4lu", percentage( count, content_length ) )
    : sprintf( p, "| ???" );

  if( source_url != NULL )
  {
    dmh_printf( "%s\n", source_url );
    source_url = NULL;
  }
  return dmh_printf( "\r%s%%", status_report );
}

int pkgDownloadMeter::SizeFormat( char *buf, unsigned long filesize )
{
  /* Helper method to format raw byte counts as B, kB, MB, GB, TB, as
   * appropriate; (this NEVER requires more than ten characters).
   */
  int retval;
  const unsigned long sizelimit = 1 << 10; /* 1k */

  if( filesize < sizelimit )
    /*
     * Raw byte count is less than 1 kB; we may simply emit it.
     */
    retval = sprintf( buf, "%lu B", filesize );

  else
  {
    /* The raw byte count is too large to be readily assimilated; we
     * scale it down to an appropriate value, and append the appropriate
     * scaling indicator.
     */
    unsigned long frac = filesize;
    const unsigned long fracmask = sizelimit - 1;
    const char *scale_indicator = "kMGT";

    /* A ten bit right shift scales by a factor of 1k; continue shifting
     * until the ultimate value is less than 1k...
     */
    while( (filesize >>= 10) >= sizelimit )
    {
      /* ...noting the residual at each shift, and adjusting the scaling
       * indicator selection to suit.
       */
      frac = filesize;
      ++scale_indicator;
    }
    /* Finally, emit the scaled result to the nearest one hundredth part
     * of the ultimate scaling unit.
     */
    retval = sprintf
      ( buf, "%lu.%02lu %cB",
	filesize, percentage( frac & fracmask, sizelimit ), *scale_indicator
      );
  }
  return retval;
}

class pkgInternetAgent
{
  /* A minimal, locally implemented class, instantiated ONCE as a
   * global object, to ensure that wininet's global initialisation is
   * completed at the proper time, without us doing it explicitly.
   */
  private:
    HINTERNET SessionHandle;

  public:
    inline pkgInternetAgent():SessionHandle( NULL )
    {
      /* Constructor...
       *
       * This is called during DLL initialisation; thus it seems to be
       * the ideal place to perform one time internet connection setup.
       * However, Microsoft caution against doing much here, (especially
       * creation of threads, either directly or indirectly); thus we
       * defer the connection setup until we ultimately need it.
       */
    }
    inline ~pkgInternetAgent()
    {
      /* Destructor...
       */
      if( SessionHandle != NULL )
	Close( SessionHandle );
    }
    HINTERNET OpenURL( const char* );

    /* Remaining methods are simple inline wrappers for the
     * wininet functions we plan to use...
     */
    inline unsigned long QueryStatus( HINTERNET id )
    {
      unsigned long ok, idx = 0, len = sizeof( ok );
      if( HttpQueryInfo( id, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE,
	    &ok, &len, &idx )
	) return ok;
      return 0;
    }
    inline unsigned long QueryContentLength( HINTERNET id )
    {
      unsigned long content_len, idx = 0, len = sizeof( content_len );
      if( HttpQueryInfo( id, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_CONTENT_LENGTH,
	    &content_len, &len, &idx )
	) return content_len;
      return 0;
    }
    inline int Read( HINTERNET dl, char *buf, size_t max, unsigned long *count )
    {
      return InternetReadFile( dl, buf, max, count );
    }
    inline int Close( HINTERNET id )
    {
      return InternetCloseHandle( id );
    }
};

/* This is the one and only instantiation of an object of this class.
 */
static pkgInternetAgent pkgDownloadAgent;

class pkgInternetStreamingAgent
{
  /* Another locally implemented class; each individual file download
   * gets its own instance of this, either as-is for basic data transfer,
   * or as a specialised derivative of this base class.
   */
  protected:
    const char *filename;
    const char *dest_template;

    char *dest_file;
    HINTERNET dl_host;
    pkgDownloadMeter *dl_meter;
    int dl_status;

  private:
    virtual int TransferData( int );

  public:
    pkgInternetStreamingAgent( const char*, const char* );
    virtual ~pkgInternetStreamingAgent();

    virtual int Get( const char* );
    inline const char *DestFile(){ return dest_file; }
};

pkgInternetStreamingAgent::pkgInternetStreamingAgent
( const char *local_name, const char *dest_specification )
{
  /* Constructor for the pkgInternetStreamingAgent class.
   */
  filename = local_name;
  dest_template = dest_specification;
  dest_file = (char *)(malloc( mkpath( NULL, dest_template, filename, NULL ) ));
  if( dest_file != NULL )
    mkpath( dest_file, dest_template, filename, NULL );
}

pkgInternetStreamingAgent::~pkgInternetStreamingAgent()
{
  /* Destructor needs to free the heap memory allocated by the
   * constructor, for storage of "dest_file" name.
   */
  free( (void *)(dest_file) );
}

HINTERNET pkgInternetAgent::OpenURL( const char *URL )
{
  /* Open an internet data stream.
   */
  HINTERNET ResourceHandle;

  /* This requires an internet
   * connection to have been established...
   */
  if(  (SessionHandle == NULL)
  &&   (InternetAttemptConnect( 0 ) == ERROR_SUCCESS)  )
    /*
     * ...so, on first call, we perform the connection setup
     * which we deferred from the class constructor; (MSDN
     * cautions that this MUST NOT be done in the constructor
     * for any global class object such as ours).
     */
    SessionHandle = InternetOpen
      ( "MinGW Installer", INTERNET_OPEN_TYPE_PRECONFIG,
	 NULL, NULL, 0
      );
  
  /* Aggressively attempt to acquire a resource handle, which we may use
   * to access the specified URL; (schedule a maximum of five attempts).
   */
  int retries = 5;
  do { ResourceHandle = InternetOpenUrl
	 (
	   /* Here, we attempt to assign a URL specific resource handle,
	    * within the scope of the SessionHandle obtained above, to
	    * manage the connection for the requested URL.
	    *
	    * Note: Scott Michel suggests INTERNET_FLAG_EXISTING_CONNECT
	    * here; MSDN tells us it is useful only for FTP connections.
	    * Since we are primarily interested in HTTP connections, it
	    * may not help us.  However, it does no harm, and MSDN isn't
	    * always the reliable source of information we might like.
	    * Persistent HTTP connections aren't entirely unknown, (and
	    * indeed, MSDN itself tells us we need to use one, when we
	    * negotiate proxy authentication); thus, we may just as well
	    * specify it anyway, on the off-chance that it may introduce
	    * an undocumented benefit beyond wishful thinking.
	    */
	   SessionHandle, URL, NULL, 0, INTERNET_FLAG_EXISTING_CONNECT, 0
	 );
       if( ResourceHandle == NULL )
       {
	 /* We failed to acquire a handle for the URL resource; we may retry
	  * unless we have exhausted the specified retry limit...
	  */
	 if( --retries < 1 )
	   /*
	    * ...in which case, we diagnose failure to open the URL.
	    */
	   dmh_notify( DMH_ERROR, "%s:cannot open URL\n", URL );

	 else DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_INTERNET_REQUESTS ),
	   dmh_printf( "%s\nConnecting ... failed(status=%u); retrying...\n",
	       URL, GetLastError() )
	   );
       }
       else
       { /* We got a handle for the URL resource, but we cannot yet be sure
	  * that it is ready for use; we may still need to address a need for
	  * proxy or server authentication, or other temporary fault.
	  */
	 int retry = 5;
	 unsigned long ResourceStatus, ResourceErrno;
	 do { /* We must capture any error code which may have been returned,
	       * BEFORE we move on to evaluate the resource status, (since the
	       * procedure for checking status may change the error code).
	       */
	      ResourceErrno = GetLastError();
	      ResourceStatus = QueryStatus( ResourceHandle );
	      if( ResourceStatus == HTTP_STATUS_PROXY_AUTH_REQ )
	      {
		/* We've identified a requirement for proxy authentication;
		 * here we simply hand the task off to the Microsoft handler,
		 * to solicit the appropriate response from the user.
		 *
		 * FIXME: this may be a reasonable approach when running in
		 * a GUI context, but is rather inelegant in the CLI context.
		 * Furthermore, this particular implementation provides only
		 * for proxy authentication, ignoring the possibility that
		 * server authentication may be required.  We may wish to
		 * revisit this later.
		 */
		unsigned long user_response;
		do { user_response = InternetErrorDlg
		       ( dmh_dialogue_context(), ResourceHandle, ResourceErrno,
			 FLAGS_ERROR_UI_FILTER_FOR_ERRORS |
			 FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS |
			 FLAGS_ERROR_UI_FLAGS_GENERATE_DATA,
			 NULL
		       );
		     /* Having obtained authentication credentials from
		      * the user, we may retry the open URL request...
		      */
		     if(  (user_response == ERROR_INTERNET_FORCE_RETRY)
		     &&  HttpSendRequest( ResourceHandle, NULL, 0, 0, 0 )  )
		     {
		       /* ...and, if successful...
			*/
		       ResourceErrno = GetLastError();
		       ResourceStatus = QueryStatus( ResourceHandle );
		       if( ResourceStatus == HTTP_STATUS_OK )
			 /*
			  * ...ensure that the response is anything but 'retry',
			  * so that we will break out of the retry loop...
			  */
			 user_response ^= -1L;
		     }
		     /* ...otherwise, we keep retrying when appropriate.
		      */
		   } while( user_response == ERROR_INTERNET_FORCE_RETRY );
	      }
	      else if( ResourceStatus != HTTP_STATUS_OK )
	      {
		/* Other failure modes may not be so readily recoverable;
		 * with little hope of success, retry anyway.
		 */
		DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_INTERNET_REQUESTS ),
		    dmh_printf( "%s: abnormal request status = %u; retrying...\n",
			URL, ResourceStatus
		  ));
		if( HttpSendRequest( ResourceHandle, NULL, 0, 0, 0 ) )
		  ResourceStatus = QueryStatus( ResourceHandle );
	      }
	    } while( (ResourceStatus != HTTP_STATUS_OK) && (retry-- > 0) );

	 /* Confirm that the URL was (eventually) opened successfully...
	  */
	 if( ResourceStatus == HTTP_STATUS_OK )
	   /*
	    * ...in which case, we have no need to schedule any further
	    * retries.
	    */
	   retries = 0;

	 else
	 { /* The resource handle we've acquired isn't useable; discard it,
	    * so we can reclaim any resources associated with it.
	    */
	   Close( ResourceHandle );

	   /* When we have exhausted our retry limit...
	    */
	   if( --retries < 1 )
	   {
	     /* Give up; nullify our resource handle to indicate failure.
	      */
	     ResourceHandle = NULL;

	     /* Issue a diagnostic advising the user to refer the problem
	      * to the mingw-get maintainer for possible follow-up.
	      */
	     dmh_control( DMH_BEGIN_DIGEST );
	     dmh_notify( DMH_WARNING,
		 "%s: opened with unexpected status: code = %u\n",
		 URL, ResourceStatus
	       );
	     dmh_notify( DMH_WARNING,
		 "please report this to the mingw-get maintainer\n"
	       );
	     dmh_control( DMH_END_DIGEST );
	   }
	 }
       }
       /* If we haven't yet acquired a valid resource handle, and we haven't
	* yet exhausted our retry limit, go back and try again.
	*/
     } while( retries > 0 );

  /* Ultimately, we return the resource handle for the opened URL,
   * or NULL if the open request failed.
   */
  return ResourceHandle;
}

int pkgInternetStreamingAgent::TransferData( int fd )
{
  /* In the case of this base class implementation,
   * we simply read the file's data from the Internet source,
   * and write a verbatim copy to the destination file.
   */
  char buf[8192]; unsigned long count, tally = 0;
  do { dl_status = pkgDownloadAgent.Read( dl_host, buf, sizeof( buf ), &count );
       dl_meter->Update( tally += count );
       write( fd, buf, count );
     } while( dl_status && (count > 0) );

  DEBUG_INVOKE_IF(
      DEBUG_REQUEST( DEBUG_TRACE_INTERNET_REQUESTS ) && (dl_status == 0),
      dmh_printf( "\nInternetReadFile:download error:%d\n", GetLastError() )
    );
  return dl_status;
}

static const char *get_host_info
( pkgXmlNode *ref, const char *property, const char *fallback = NULL )
{
  /* Helper function to retrieve host information from the XML catalogue.
   *
   * Call with property = "url", to retrieve the URL template to pass as
   * "fmt" argument to mkpath(), or with property = "mirror", to retrieve
   * the substitution text for the "modifier" argument.
   */
  const char *uri = NULL;
  while( ref != NULL )
  {
    /* Starting from the "ref" package entry in the catalogue...
     */
    pkgXmlNode *host = ref->FindFirstAssociate( download_host_key );
    while( host != NULL )
    {
      /* Examine its associate tags; if we find one of type
       * "download-host", with the requisite property, then we
       * immediately return that property value...
       */
      if( (uri = host->GetPropVal( property, NULL )) != NULL )
	return uri;

      /* Otherwise, we look for any other candidate tags
       * associated with the same catalogue entry...
       */
      host = host->FindNextAssociate( download_host_key );
    }
    /* Failing an immediate match, extend the search to the
     * ancestors of the initial reference entry...
     */
    ref = ref->GetParent();
  }
  /* ...and ultimately, if no match is found, we return the
   * specified "fallback" property value.
   */
  return fallback;
}

static inline
int set_transit_path( const char *path, const char *file, char *buf = NULL )
{
  /* Helper to define the transitional path name for downloaded files,
   * used to save the file data while the download is in progress.
   */
  static const char *transit_dir = "/.in-transit";
  return mkpath( buf, path, file, transit_dir );
}

int pkgInternetStreamingAgent::Get( const char *from_url )
{
  /* Download a file from the specified internet URL.
   *
   * Before download commences, we accept that this may fail...
   */
  dl_status = 0;

  /* Set up a "transit-file" to receive the downloaded content.
   */
  char transit_file[set_transit_path( dest_template, filename )];
  int fd; set_transit_path( dest_template, filename, transit_file );

  if( (fd = set_output_stream( transit_file, 0644 )) >= 0 )
  {
    /* The "transit-file" is ready to receive incoming data...
     * Configure and invoke the download handler to copy the data
     * from the appropriate host URL, to this "transit-file".
     */
    if( (dl_host = pkgDownloadAgent.OpenURL( from_url )) != NULL )
    {
      if( pkgDownloadAgent.QueryStatus( dl_host ) == HTTP_STATUS_OK )
      {
	/* With the download transaction fully specified, we may
	 * request processing of the file transfer...
	 */
	pkgDownloadMeterTTY download_meter
	  (
	    from_url, pkgDownloadAgent.QueryContentLength( dl_host )
	  );
	dl_meter = &download_meter;
	dl_status = TransferData( fd );
      }
      else DEBUG_INVOKE_IF( DEBUG_REQUEST( DEBUG_TRACE_INTERNET_REQUESTS ),
	  dmh_printf( "OpenURL:error:%d\n", GetLastError() )
	);

      /* We are done with the URL handle; close it.
       */
      pkgDownloadAgent.Close( dl_host );
    }

    /* Always close the "transit-file", whether the download
     * was successful, or not...
     */
    close( fd );
    if( dl_status )
      /*
       * When successful, we move the "transit-file" to its
       * final downloaded location...
       */
      rename( transit_file, dest_file );
    else
      /* ...otherwise, we discard the incomplete "transit-file",
       * leaving the caller to diagnose the failure.
       */
      unlink( transit_file );
  }

  /* Report success or failure to the caller...
   */
  return dl_status;
}

void pkgActionItem::DownloadArchiveFiles( pkgActionItem *current )
{
  /* Update the local package cache, to ensure that all packages needed
   * to complete the current set of scheduled actions are present; if any
   * are missing, invoke an Internet download agent to fetch them.  This
   * requires us to walk the action list...
   */
  while( current != NULL )
  {
    /* ...while we haven't run off the end...
     */
    if( (current->flags & ACTION_INSTALL) == ACTION_INSTALL )
    {
      /* For all packages specified in the current action list,
       * for which an "install" action is scheduled, and for which
       * no associated archive file is present in the local archive
       * cache, place an Internet download agent on standby to fetch
       * the required archive from a suitable internet mirror host.
       */
      const char *package_name = current->Selection()->ArchiveName();

      /* An explicit package name of "none" is a special case, indicating
       * a "virtual" meta-package; it requires nothing to be downloaded...
       */
      if( ! match_if_explicit( package_name, value_none ) )
      {
	/* ...but we expect any other package to provide real content,
	 * for which we may need to download the package archive...
	 */
	pkgInternetStreamingAgent download( package_name, pkgArchivePath() );

	/* Check if the required archive is already available locally...
	 */
	if( (access( download.DestFile(), R_OK ) != 0) && (errno == ENOENT) )
	{
	  /* ...if not, ask the download agent to fetch it...
	   */
	  current->flags |= ACTION_DOWNLOAD;
	  const char *url_template = get_host_info( current->Selection(), uri_key );
	  if( url_template != NULL )
	  {
	    /* ...from the URL constructed from the template specified in
	     * the package repository catalogue (configuration database)...
	     */
	    const char *mirror = get_host_info( current->Selection(), mirror_key );
	    char package_url[mkpath( NULL, url_template, package_name, mirror )];
	    mkpath( package_url, url_template, package_name, mirror );
	    if( download.Get( package_url ) > 0 )
	      /*
	       * Download was successful; clear the pending flag.
	       */
	      current->flags &= ~ACTION_DOWNLOAD;
	    else
	      /* Diagnose failure; leave pending flag set.
	       */
	      dmh_notify( DMH_ERROR,
		  "Get package: %s: download failed\n", package_url
		);
	  }
	  else
	    /* Cannot download; the repository catalogue didn't specify a
	     * template, from which to construct a download URL...
	     */
	    dmh_notify( DMH_ERROR,
		"Get package: %s: no URL specified for download\n", package_name
	      );
	}
      }
    }
    /* Repeat download action, for any additional packages specified
     * in the current "actions" list.
     */
    current = current->next;
  }
}

#define DATA_CACHE_PATH		"%R" "var/cache/mingw-get/data"
#define WORKING_DATA_PATH	"%R" "var/lib/mingw-get/data"

/* Internet servers host package catalogues in lzma compressed format;
 * we will decompress them "on the fly", as we download them.  To achieve
 * this, we will use a variant of the pkgInternetStreamingAgent, using a
 * specialised TransferData method; additionally, this will incorporate
 * a special derivative of a pkgLzmaArchiveStream, with its GetRawData
 * method adapted to stream data from an internet URI, instead of
 * reading from a local file.
 *
 * To derive the pkgInternetLzmaStreamingAgent, we need to include the
 * specialised declarations of a pkgArchiveStream, in order to make the
 * declaration of pkgLzmaArchiveStream available as our base class.
 */
#define  PKGSTRM_H_SPECIAL  1
#include "pkgstrm.h"

class pkgInternetLzmaStreamingAgent :
public pkgInternetStreamingAgent, public pkgLzmaArchiveStream
{
  /* Specialisation of the pkgInternetStreamingAgent base class,
   * providing decompressed copies of LZMA encoded files downloaded
   * from the Internet; (the LZMA decompression capability is derived
   * from the pkgLzmaArchiveStream base class).
   */
  public:
    /* We need a specialised constructor...
     */
    pkgInternetLzmaStreamingAgent( const char*, const char* );

  private:
    /* Specialisation requires overrides for each of this pair of
     * methods, (the first from the pkgLzmaArchiveStream base class;
     * the second from pkgInternetStreamingAgent).
     */
    virtual int GetRawData( int, uint8_t*, size_t );
    virtual int TransferData( int );
};

/* This specialisation of the pkgInternetStreamingAgent class needs its
 * own constructor, simply to invoke the constructors for the base classes,
 * (since neither is instantiated by a default constructor).
 */
pkgInternetLzmaStreamingAgent::pkgInternetLzmaStreamingAgent
( const char *local_name, const char *dest_specification ):
pkgInternetStreamingAgent( local_name, dest_specification ),
/*
 * Note that, when we come to initialise the lzma streaming component
 * of this derived class, we will be streaming directly from the internet,
 * rather than from a file stream, so we don't require a file descriptor
 * for the input stream; however, the class semantics still expect one.
 * To avoid accidental association with an existing file stream, we
 * use a negative value, (which is never a valid file descriptor);
 * however, we must not choose -1, since the class implementation
 * will decline to process the stream; hence, we choose -2.
 */
pkgLzmaArchiveStream( -2 ){}

int pkgInternetLzmaStreamingAgent::GetRawData( int fd, uint8_t *buf, size_t max )
{
  /* Fetch raw (compressed) data from the Internet host, and load it into
   * the decompression filter's input buffer, whence the TransferData routine
   * may retrieve it, via the filter, as an uncompressed stream.
   */
  unsigned long count;
  dl_status = pkgDownloadAgent.Read( dl_host, (char *)(buf), max, &count );
  return (int)(count);
}

int pkgInternetLzmaStreamingAgent::TransferData( int fd )
{
  /* In this case, we read the file's data from the Internet source,
   * stream it through the lzma decompression filter, and write a copy
   * of the resultant decompressed data to the destination file.
   */
  char buf[8192]; unsigned long count;
  do { count = pkgLzmaArchiveStream::Read( buf, sizeof( buf ) );
       write( fd, buf, count );
     } while( dl_status && (count > 0) );

  DEBUG_INVOKE_IF(
      DEBUG_REQUEST( DEBUG_TRACE_INTERNET_REQUESTS ) && (dl_status == 0),
      dmh_printf( "\nInternetReadFile:download error:%d\n", GetLastError() )
    );
  return dl_status;
}

static const char *serial_number( const char *catalogue )
{
  /* Local helper function to retrieve issue numbers from any repository
   * package catalogue; returns the result as a duplicate of the internal
   * string, allocated on the heap (courtesy of the strdup() function).
   */
  const char *issue;
  pkgXmlDocument src( catalogue );

  if(   src.IsOk()
  &&  ((issue = src.GetRoot()->GetPropVal( issue_key, NULL )) != NULL)  )
    /*
     * Found an issue number; return a copy...
     */
    return strdup( issue );

  /* If we get to here, we couldn't get a valid issue number;
   * whatever the reason, return NULL to indicate failure.
   */
  return NULL;
}

void pkgXmlDocument::SyncRepository( const char *name, pkgXmlNode *repository )
{
  /* Fetch a named package catalogue from a specified Internet repository.
   *
   * Package catalogues are XML files; the master copy on the Internet host
   * must be stored in lzma compressed format, and named to comply with the
   * convention "%F.xml.lzma", in which "%F" represents the value of the
   * "name" argument passed to this pkgXmlDocument class method.
   */ 
  const char *url_template;
  if( (url_template = repository->GetPropVal( uri_key, NULL )) != NULL )
  {
    /* Initialise a streaming agent, to manage the catalogue download;
     * (note that we must include the "%/M" placeholder in the template
     * for the local name, to accommodate the name of the intermediate
     * "in-transit" directory used by the streaming agent).
     */
    pkgInternetLzmaStreamingAgent download( name, DATA_CACHE_PATH "%/M/%F.xml" );
    {
      /* Construct the full URI for the master catalogue, and stream it to
       * a locally cached, decompressed copy of the XML file.
       */
      const char *mirror = repository->GetPropVal( mirror_key, NULL );
      char catalogue_url[mkpath( NULL, url_template, name, mirror )];
      mkpath( catalogue_url, url_template, name, mirror );
      if( download.Get( catalogue_url ) <= 0 )
	dmh_notify( DMH_ERROR,
	    "Sync Repository: %s: download failed\n", catalogue_url
	  );
    }

    /* We will only replace our current working copy of this catalogue,
     * (if one already exists), with the copy we just downloaded, if this
     * downloaded copy bears an issue number indicating that it is more
     * recent than the working copy.
     */
    const char *repository_version, *working_version;
    if( (repository_version = serial_number( download.DestFile() )) != NULL )
    {
      /* Identify the location for the working copy, (if it exists).
       */
      const char *working_copy_path_name = WORKING_DATA_PATH "/%F.xml";
      char working_copy[mkpath( NULL, working_copy_path_name, name, NULL )];
      mkpath( working_copy, working_copy_path_name, name, NULL );

      /* Compare issue serial numbers...
       */
      if( ((working_version = serial_number( working_copy )) == NULL)
      ||  ((strcmp( repository_version, working_version )) > 0)        )
      {
	/* In these circumstances, we couldn't identify an issue number
	 * for the working copy of the catalogue; (maybe there is no such
	 * catalogue, or maybe it doesn't specify a valid issue number);
	 * in either case, we promote the downloaded copy in its place.
	 *
	 * FIXME: we assume that the working file and the downloaded copy
	 * are stored on the same physical file system device, so we may
	 * replace the former by simply deleting it, and renaming the
	 * latter with its original path name; we make no provision for
	 * replacing the working version by physical data copying.
	 */
	unlink( working_copy );
	rename( download.DestFile(), working_copy );
      }

      /* The issue numbers, returned by the serial_number() function, were
       * allocated on the heap; free them to avoid leaking memory!
       */
      free( (void *)(repository_version) );
      /*
       * The working copy issue number may be represented by a NULL pointer;
       * while it may be safe to call free on this, it just *seems* wrong, so
       * we check it first, to be certain.
       */
      if( working_version != NULL )
	free( (void *)(working_version) );
    }

    /* If the downloaded copy of the catalogue is still in the download cache,
     * we have chosen to keep a previous working copy, so we have no further
     * use for the downloaded copy; discard it, noting that we don't need to
     * confirm its existence because this will fail silently, if it is no
     * longer present.
     */
    unlink( download.DestFile() );
  }
}

/* $RCSfile: pkginet.cpp,v $: end of file */
