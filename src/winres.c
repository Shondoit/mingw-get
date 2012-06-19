#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <tchar.h>
#include "winres.h"

/** Load RCDATA with id <b>lpName</b> and the <b>wLanguage</b> locale
 *  from the binary file specified by <b>hInstance</b>.
 *  If the resource doesn't exist, return 0 and don't touch <b>lpBuffer</b>.
 *
 *  If <b>lpBuffer</b> is NULL then return the size of the resource,
 *
 *  If <b>lpBuffer</b> is provided then write the resource data as is to
 *  <b>lpBuffer</b> and return the length of the size written data.
 *  If <b>nBufferMax</b> is less than the actual size of the resource,
 *  then fill the buffer till the end.
 *
 *  \code
 *  LPTSTR lpName = "resource-name";
 *  WORD langID = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
 *  int bufferLen = LoadResDataEx(NULL, lpName, NULL, 0, langID);
 *  LPTSTR buffer = (LPTSTR)malloc(bufferLen);
 *  int size = LoadResDataEx(NULL, lpName, buffer, bufferLen, langID);
 *  if (size > 0) {
 *    _tprintf(_T("Resource size: %d\n"), size);
 *  } else {
 *    _tprintf(_T("Resource '%d' does not exist.\n"), lpName);
 *  }
 *  free(buffer);
 *  \endcode
 */
int
LoadResDataEx(HINSTANCE hInstance,
              LPCTSTR lpName,
              LPVOID lpBuffer,
              int nBufferMax,
              WORD wLanguage)
{
  HRSRC hRes = FindResourceEx(hInstance, RT_RCDATA,  lpName, wLanguage);
  if (hRes == NULL) return 0;

  DWORD resSize = SizeofResource(hInstance, hRes);
  if (resSize == 0) return 0;

  /* If lpBuffer is NULL, then this is a request for
   * the required buffer length */
  if (lpBuffer == NULL) return resSize;

  HGLOBAL hGlobal = LoadResource(hInstance, hRes);
  if (hGlobal == NULL) return 0;

  LPVOID lpResData = (LPVOID)LockResource(hGlobal);
  if (lpResData == NULL) return 0;

  /* Limit the buffer length to the provided one. */
  int bufferLen = min((int)resSize, nBufferMax);
  /* Copy the data to the buffer as is. We won't interpret the data format */
  memcpy(lpBuffer, lpResData, bufferLen);

  return bufferLen;
}

/** Load RCDATA with id <b>lpName</b> from the current executable file.
 *
 *  Call LoadResDataEx with hInstance set to NULL and wLanguage set to
 *  MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)
 */
int
LoadResData(LPCTSTR lpName, LPVOID lpBuffer, int nBufferMax)
{
  return LoadResDataEx(NULL,
                       lpName,
                       lpBuffer,
                       nBufferMax,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
}