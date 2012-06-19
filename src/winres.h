#ifndef WINRES_H

#define WINRES_H

#ifndef EXTERN_C
#  ifdef __cplusplus
#    define EXTERN_C extern "C"
#  else
#    define EXTERN_C
#  endif
#endif

#include <windef.h>

EXTERN_C int LoadResDataEx(HINSTANCE hInstance,
                           LPCTSTR lpName,
                           LPVOID lpBuffer,
                           int nBufferMax,
                           WORD wLanguage);

EXTERN_C int LoadResData(LPCTSTR lpName, LPVOID lpBuffer, int nBufferMax);

#endif
