/*********************************************************************
-------------------------- END-OF-HEADER -----------------------------

File    : IP_FS_Linux.c
Purpose : Implementation of file system on Linux OS
Revision: $Rev: 6176 $
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>

#include "IP_FTPServer.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/

//
// FTP Sample
//
#define MAX_CONNECTIONS  2  // Number of connections to handle at the same time

#ifndef TRUE
   #define TRUE (1)
#endif  // Boolean true

#ifndef FALSE
   #define FALSE (0)
#endif  // Boolean false

#ifndef MIN
   #define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
   #define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define _FS_LINUX_DOTDOT_HANDLE         0L
#define _FS_LINUX_INVALID_HANDLE       -1L

#define _FS_LINUX_A_NORMAL             0x00    // Normal file.
#define _FS_LINUX_A_RDONLY             0x01    // Read only file.
#define _FS_LINUX_A_HIDDEN             0x02    // Hidden file.
#define _FS_LINUX_A_SYSTEM             0x04    // System file.
#define _FS_LINUX_A_SUBDIR             0x10    // Subdirectory.
#define _FS_LINUX_A_ARCH               0x20    // Archive file.

/*********************************************************************
*
*       Types, local
*
**********************************************************************
*/

enum {
  USER_ID_ANONYMOUS = 1,
  USER_ID_ADMIN
};

typedef struct _FS_FIND_DATA {
  uint32_t  attrib;
  time_t    time_create;
  time_t    time_access;
  time_t    time_write;
  off_t     size;
  char      name[260];
} _FS_FIND_DATA;

typedef struct _FS_FHANDLE {
  DIR* dstream;
  short dironly;
  char* spec;
} _FS_FHANDLE;

/*********************************************************************
*
*       Static variables
*
**********************************************************************
*/
static int                  _ConnectCnt;
static const _FS_API *      _pFS_API;     // File system info
static char                 _acBaseDir[256] = "./";

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

// 
// Matches the regular language accepted by findfirst/findnext. More
// precisely, * matches zero or more characters and ? matches any
// characters, but only one. Every other characters match themself.
// To respect the Windows behavior, *.* matches everything.
// 
static int _FS_LINUX_MatchSpec(const char* spec, const char* text) {
  //
  // On Windows, *.* matches everything.
  //
  if (strcmp(spec, "*.*") == 0) {
    return 1;
  }
  // 
  // If the whole specification string was consumed and
  // the input text is also exhausted: it's a match.
  // 
  if (spec[0] == '\0' && text[0] == '\0') {
    return 1;
  }

  /* A star matches 0 or more characters. */
  if (spec[0] == '*') {
    // 
    // Skip the star and try to find a match after it
    // by successively incrementing the text pointer.
    // 
    do {
        if (_FS_LINUX_MatchSpec(spec + 1, text)) {
            return 1;
        }
    } while (*text++ != '\0');
  }

  // 
  // An interrogation mark matches any character. Other
  // characters match themself. Also, if the input text
  // is exhausted but the specification isn't, there is
  // no match.
  // 
  if (text[0] != '\0' && (spec[0] == '?' || spec[0] == text[0])) {
    return _FS_LINUX_MatchSpec(spec + 1, text + 1);
  }

  return 0;
}

// 
// Closes the specified search handle and releases associated
// resources. If successful, findclose returns 0. Otherwise, it
// returns -1 and sets errno to ENOENT, indicating that no more
// matching files could be found.
// 
static int _FS_LINUX_FindClose(int fhandle) {
  struct _FS_FHANDLE* handle;

  if (fhandle == _FS_LINUX_DOTDOT_HANDLE) {
    return 0;
  }

  if (fhandle == _FS_LINUX_INVALID_HANDLE) {
    return -1;
  }

  handle = (struct _FS_FHANDLE*) fhandle;

  closedir(handle->dstream);
  free(handle->spec);
  free(handle);

  return 0;
}

// 
// Find the next entry, if any, that matches the filespec argument
// of a previous call to findfirst, and then alter the fileinfo
// structure contents accordingly. If successful, returns 0. Otherwise,
// returns -1 and sets errno to EINVAL if handle or fileinfo was NULL
// or if the operating system returned an unexpected error and ENOENT
// if no more matching files could be found.
// 
static int _FS_LINUX_FindNext(int fhandle, _FS_FIND_DATA* fileinfo) {
  struct dirent *entry;
  struct _FS_FHANDLE* handle;
  struct stat st;

  if (fhandle == _FS_LINUX_DOTDOT_HANDLE) {
    return -1;
  }

  if (fhandle == _FS_LINUX_INVALID_HANDLE || !fileinfo) {
    return -1;
  }

  handle = (struct _FS_FHANDLE*) fhandle;

  while ((entry = readdir(handle->dstream)) != NULL) {
    if (!handle->dironly && !_FS_LINUX_MatchSpec(handle->spec, entry->d_name)) {
      continue;
    }

    if (fstatat(dirfd(handle->dstream), entry->d_name, &st, 0) == -1) {
      return -1;
    }

    if (handle->dironly && !S_ISDIR(st.st_mode)) {
      continue;
    }

    fileinfo->attrib = S_ISDIR(st.st_mode) ? _FS_LINUX_A_SUBDIR : _FS_LINUX_A_NORMAL;
    fileinfo->size = st.st_size;
    fileinfo->time_create = st.st_ctime;
    fileinfo->time_access = st.st_atime;
    fileinfo->time_write = st.st_mtime;
    strcpy(fileinfo->name, entry->d_name);

    return 0;
  }

  return -1;
}

//
// Perfom a scan in the directory identified by dirpath.
//
static int _FS_LINUX_InDirectory(const char* dirpath, const char* spec, _FS_FIND_DATA* fileinfo) {
  DIR* dstream;
  _FS_FHANDLE* ffhandle;

  if (spec[0] == '\0') {
    return _FS_LINUX_INVALID_HANDLE;
  }

  if ((dstream = opendir(dirpath)) == NULL) {
    return _FS_LINUX_INVALID_HANDLE;
  }

  if ((ffhandle = malloc(sizeof(_FS_FHANDLE))) == NULL) {
    closedir(dstream);
    return _FS_LINUX_INVALID_HANDLE;
  }
  //
  // On Windows, *. returns only directories.
  //
  ffhandle->dironly = strcmp(spec, "*.") == 0 ? 1 : 0;
  ffhandle->dstream = dstream;
  ffhandle->spec = strdup(spec);

  if (_FS_LINUX_FindNext((int) ffhandle, fileinfo) != 0) {
    _FS_LINUX_FindClose((int) ffhandle);
    return _FS_LINUX_INVALID_HANDLE;
  }

  return (int) ffhandle;
}

//
// On Windows, . and .. return canonicalized directory names.
//
static int _FS_LINUX_DdotDot(const char* filespec, _FS_FIND_DATA* fileinfo) {
  char* dirname;
  char* canonicalized;
  struct stat st;

  if (stat(filespec, &st) != 0) {
    return _FS_LINUX_INVALID_HANDLE;
  }
  //
  // Resolve filespec to an absolute path.
  //
  if ((canonicalized = realpath(filespec, NULL)) == NULL) {
    return _FS_LINUX_INVALID_HANDLE;
  }
  //
  // Retrieve the basename from it.
  //
  dirname = basename(canonicalized);
  //
  // Make sure that we actually have a basename.
  //
  if (dirname[0] == '\0') {
    free(canonicalized);
    return _FS_LINUX_INVALID_HANDLE;
  }
  //
  // Make sure that we won't overflow finddata_t::name.
  //
  if (strlen(dirname) > 259) {
    free(canonicalized);
    return _FS_LINUX_INVALID_HANDLE;
  }

  fileinfo->attrib = S_ISDIR(st.st_mode) ? _FS_LINUX_A_SUBDIR : _FS_LINUX_A_NORMAL;
  fileinfo->size = st.st_size;
  fileinfo->time_create = st.st_ctime;
  fileinfo->time_access = st.st_atime;
  fileinfo->time_write = st.st_mtime;
  strcpy(fileinfo->name, dirname);

  free(canonicalized);

  // 
  // Return a special handle since we can't return
  // NULL. The findnext and findclose functions know
  // about this custom handle.
  // 
  return _FS_LINUX_DOTDOT_HANDLE;
}

// 
// Returns a unique search handle identifying the file or group of
// files matching the filespec specification, which can be used in
// a subsequent call to findnext or to findclose. Otherwise, findfirst
// returns NULL and sets errno to EINVAL if filespec or fileinfo
// was NULL or if the operating system returned an unexpected error
// and ENOENT if the file specification could not be matched.
// 
static int _FS_LINUX_FindFirst(const char* filespec, _FS_FIND_DATA* fileinfo) {
  char* rmslash;      // Rightmost forward slash in filespec.
  const char* spec;   // Specification string.

  if (!fileinfo || !filespec) {
    return _FS_LINUX_INVALID_HANDLE;
  }

  if (filespec[0] == '\0') {
    return _FS_LINUX_INVALID_HANDLE;
  }

  rmslash = strrchr(filespec, '/');

  if (rmslash != NULL) {
    // 
    // At least one forward slash was found in the filespec
    // string, and rmslash points to the rightmost one. The
    // specification part, if any, begins right after it.
    // 
    spec = rmslash + 1;
  } else {
    // 
    // Since no slash was found in the filespec string, its
    // entire content can be used as our spec string.
    // 
    spec = filespec;
  }

  if (strcmp(spec, ".") == 0 || strcmp(spec, "..") == 0) {
    //
    // On Windows, . and .. must return canonicalized names.
    //
    return _FS_LINUX_DdotDot(filespec, fileinfo);
  } else if (rmslash == filespec) {
    // 
    // Since the rightmost slash is the first character, we're
    // looking for something located at the file system's root.
    // 
    return _FS_LINUX_InDirectory("/", spec, fileinfo);
  } else if (rmslash != NULL) {
    // 
    // Since the rightmost slash isn't the first one, we're
    // looking for something located in a specific folder. In
    // order to open this folder, we split the folder path from
    // the specification part by overwriting the rightmost
    // forward slash.
    // 
    size_t pathlen = strlen(filespec) +1;
    char* dirpath = alloca(pathlen);
    memcpy(dirpath, filespec, pathlen);
    dirpath[rmslash - filespec] = '\0';
    return _FS_LINUX_InDirectory(dirpath, spec, fileinfo);
  } else {
    // 
    // Since the filespec doesn't contain any forward slash,
    // we're looking for something located in the current
    // directory.
    // 
    return _FS_LINUX_InDirectory(".", spec, fileinfo);
  }
}

/*********************************************************************
*
*       _ConvertFileName
*/
static void _ConvertFileName(char* sDest, const char* sSrc, unsigned BufferSize) {
  char c;
  const char* s;
  uint32_t i;

  //
  // If sSrc starts with a '/' skip it.
  //
  if (sSrc[0] == '/') {
    s = sSrc + 1;
  } else {
    s = sSrc;
  }
  //
  // Convert given filename into Windows filename.
  // e.g.:  "/pub/temp.gif"  -> "C:\ftp\pub\temp.gif"
  //
  strncpy(sDest, _acBaseDir, BufferSize);
  *(sDest + BufferSize - 1) = '\0';
  i = strlen(sDest);
  while(1) {
    c = *s++;
    if (c == '/') {
      c = '/';
    }
    *(sDest + i++) = c;
    if (c == 0) {
      break;
    }
  }
}

/*********************************************************************
*
*       _FS_LINUX_Open
*/
static void* _FS_LINUX_Open(const char* sFilename) {
  char acFilename[256];
  _FILE_HANDLE fileHandle;

  _ConvertFileName(acFilename, sFilename, sizeof(acFilename));
  fileHandle = fopen(acFilename, "r+");
  if (fileHandle == (NULL)) {
    fileHandle = 0;
  }
  return (fileHandle);
}

/*********************************************************************
*
*       _FS_LINUX_Close
*/
static int _FS_LINUX_Close(void* hFile) {
  int32_t result;

  result = fclose((FILE *)hFile);
  if (result == (EOF))
    return (-1);
  return (0);
}

/*********************************************************************
*
*       _FS_LINUX_ReadAt
*/
static int _FS_LINUX_ReadAt(void* hFile, void* pDest, uint32_t Pos, uint32_t NumBytes) {
  uint32_t result;

  fseek ((FILE *) hFile, Pos, SEEK_SET);
  result = fread ((void *) pDest, sizeof(uint8_t), (size_t) NumBytes, (FILE *) hFile);
  if (result != NumBytes)
    return (-1);
  return (0);
}

/*********************************************************************
*
*       _FS_LINUX_GetLen
*/
static long _FS_LINUX_GetLen(void* hFile) {
  long fileSize;

  fseek ((FILE *) hFile, 0, SEEK_END);
  fileSize = ftell(hFile);
  fseek ((FILE *) hFile, 0, SEEK_SET);
  return (fileSize);
}

/*********************************************************************
*
*       _FS_LINUX_ForEachDirEntry
*/
static void _FS_LINUX_ForEachDirEntry(void* pContext, const char* sDir, void (*pf)(void* pContext, void* pFileEntry)) {
  int h;
  _FS_FIND_DATA fd;
  char acFilter[256];

  strncpy(acFilter, _acBaseDir, sizeof(acFilter));
  acFilter[sizeof(acFilter) - 1] = '\0';
  if (sDir[0] == '/') {
    strcat(acFilter, sDir + 1);
  } else {
    strcat(acFilter, sDir);
  }
  strcat(acFilter, "*.*");

  h = _FS_LINUX_FindFirst(acFilter, &fd);
  if (h != -1) {
    do {
      pf(pContext, &fd);
    } while (_FS_LINUX_FindNext(h, &fd) == 0);
    _FS_LINUX_FindClose(h);
  }
}

/*********************************************************************
*
*       _FS_LINUX_GetDirEntryFileName
*/
static void _FS_LINUX_GetDirEntryFileName(void* pFileEntry, char* sFileName, uint32_t SizeOfBuffer) {
  _FS_FIND_DATA* pFD;

  pFD = (_FS_FIND_DATA*)pFileEntry;

  strncpy(sFileName, pFD->name, SizeOfBuffer);
  *(sFileName + SizeOfBuffer - 1) = 0;
}

/*********************************************************************
*
*       _FS_LINUX_GetDirEntryFileSize
*/
static uint32_t _FS_LINUX_GetDirEntryFileSize (void* pFileEntry, uint32_t* pFileSizeHigh) {
  _FS_FIND_DATA* pFD;

  pFD = (struct _FS_FIND_DATA*)pFileEntry;
  return (pFD->size);
}

/*********************************************************************
*
*       _FS_LINUX_GetDirEntryFileTime
*/
static uint32_t _FS_LINUX_GetDirEntryFileTime (void* pFileEntry) {
  _FS_FIND_DATA* pFD;
  struct            tm* tm;
  uint32_t Date, Time;

  pFD = (_FS_FIND_DATA*)pFileEntry;

  tm = gmtime( &pFD->time_write );
  Time = ((tm->tm_hour        & 0x1F) << 11) + ((tm->tm_min       & 0x3F) << 5) + ((tm->tm_sec / 2) & 0x1F);
  Date = (((tm->tm_year - 80) & 0x7F) << 9)  + (((tm->tm_mon + 1) & 0xF) << 5)  + (tm->tm_mday      & 0x1F);

  return ((Date << 16) | Time);
}

/*********************************************************************
*
*       _FS_LINUX_GetDirEntryAttributes
*
*  Return value
*    bit 0   - 0: File, 1:Directory
*/
static int _FS_LINUX_GetDirEntryAttributes(void* pFileEntry) {
  _FS_FIND_DATA* pFD;

  pFD = (_FS_FIND_DATA*)pFileEntry;
  return ((pFD->attrib & _FS_LINUX_A_SUBDIR) ? 1 : 0);
}

/*********************************************************************
*
*       _FS_LINUX_Create
*/
static void* _FS_LINUX_Create(const char* sFileName) {
  char acFilename[256];
  _FILE_HANDLE fileHandle;

  _ConvertFileName(acFilename, sFileName, sizeof(acFilename));
  fileHandle = fopen (acFilename, "w+");
  if (fileHandle == (NULL)) {
    return (NULL);
  }
  return (fileHandle);
}

/*********************************************************************
*
*       _FS_LINUX_WriteAt
*/
static int _FS_LINUX_WriteAt(void* hFile, void* pBuffer, uint32_t Pos, uint32_t NumBytes) {
  uint32_t result;

  fseek ((FILE *) hFile, Pos, SEEK_SET);
  result = fwrite ((void *) pBuffer, sizeof(uint8_t), (size_t) NumBytes, (FILE *)hFile);
  if (result != NumBytes)
    return (-1);
  return (0);
}

/*********************************************************************
*
*       _FS_LINUX_DeleteFile
*/
static int _FS_LINUX_DeleteFile(const char* sFilename) {

  char acFilename[256];
  int32_t result;

  _ConvertFileName(acFilename, sFilename, sizeof(acFilename));
  result = remove ((const char *)acFilename);
  if (result == -1)
    return (-1);
  return (0);
}

/*********************************************************************
*
*       _FS_LINUX_MakeDir
*/
static int _FS_LINUX_MakeDir(const char* sDirname) {

  char acDirname[256];
  int32_t result;

  _ConvertFileName(acDirname, sDirname, sizeof(acDirname));
  /* Owner of file is granted all permissions. */
  result = mkdir ((const char *)acDirname, 0700);
  if (result == -1)
    return (-1);
  return (0);
}

/*********************************************************************
*
*       _FS_LINUX_RemoveDir
*/
static int _FS_LINUX_RemoveDir(const char* sDirname) {

  char acDirname[256];
  int32_t result;

  _ConvertFileName(acDirname, sDirname, sizeof(acDirname));
  result = rmdir ((const char *)acDirname);
  if (result == -1)
    return (-1);
  return (0);
}

/*********************************************************************
*
*       _FS_LINUX_ConfigBaseDir
*/
static void _FS_LINUX_ConfigBaseDir(const char* sDir) {

  if (strlen(sDir) < sizeof(_acBaseDir)) {
    strncpy(_acBaseDir, sDir, sizeof(_acBaseDir));
  }
}

/*********************************************************************
*
*       SYS Net.
*/

/*********************************************************************
*
*       _SYS_NET_ListenSocket
*/
static int _SYS_NET_ListenSocket(int *listenSocket, unsigned short portNumber) {
  struct sockaddr_in  saServer;
  int                 newSocket;
  int                 nRet;
  int                 one     = 1;
  int                 status  = 0;

  newSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (0 > newSocket) {
    status = -1;
    goto exit;
  }

  if (0 > setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int))) {
    status = -1;
    goto error_cleanup;
  }

  memset((unsigned char *)&saServer, 0x00, sizeof(struct sockaddr_in));

  (saServer).sin_family         = AF_INET;
  (saServer).sin_port           = htons((portNumber));
  (saServer).sin_addr.s_addr    = INADDR_ANY;

  nRet = bind(newSocket, (struct sockaddr*)&saServer, sizeof(struct sockaddr_in));
  if (0 > nRet) {
    status = -1;
    goto error_cleanup;
  }

  nRet = listen(newSocket, SOMAXCONN);
  if (nRet != 0) {
    status = -1;
    goto error_cleanup;
  }

  *listenSocket = newSocket;
  goto exit;

error_cleanup:
  close(newSocket);

exit:
  return status;
}


/*********************************************************************
*
*       _SYS_Sleep
*/
static int _SYS_NET_AcceptSocket(int *clientSocket, int listenSocket, signed int *isBreakSignalRequest) {
  fd_set*             pSocketList     = NULL;
  struct timeval      timeout;
  struct sockaddr_in  sockAddr;
  int                 nLen            = sizeof(struct sockaddr_in);
  int          newClientSocket;
  int             status          = 0;

  if (NULL == (pSocketList = (fd_set*) malloc(sizeof(fd_set)))) {
    status = -1;
    goto exit;
  }

  while (1) {
    /* add the socket of interest to the list */
    FD_ZERO(pSocketList);
    FD_SET(listenSocket, pSocketList);

    /* poll every second to check break signal */
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    if (0 == select(FD_SETSIZE, pSocketList, NULL, NULL, &timeout))
    {
      /* time out occurred, check for break signal */
      if (TRUE == *isBreakSignalRequest)
          goto exit;

      continue;
    }

    newClientSocket = accept(listenSocket, (struct sockaddr*)&sockAddr, (socklen_t *)&nLen);
    break;
  }

  if (0 > newClientSocket) {
    status = -1;
    goto exit;
  }

  *clientSocket = newClientSocket;

exit:
  if (NULL != pSocketList)
    free(pSocketList);

  return status;

}

/*********************************************************************
*
*       _SYS_NET_ConnectSocket
*/
static int _SYS_NET_ConnectSocket(int *pConnectSocket, unsigned char *pIpAddress, unsigned short portNo) {
  struct sockaddr_in  server;
  int             status = 0;

  if (0 >= (*pConnectSocket = socket(AF_INET, SOCK_STREAM, 0))) {
    status = -1;
    goto exit;
  }

  memset((unsigned char *)&server, 0x00, sizeof(struct sockaddr_in));

  (server).sin_family         = AF_INET;
  (server).sin_port           = htons((portNo));

  inet_pton(AF_INET, (char *)pIpAddress, &server.sin_addr);

  if (0 != connect(*pConnectSocket, (struct sockaddr *)&server, sizeof(server))) {
    status = -1;

  }

exit:
  return status;
}


/*********************************************************************
*
*       _SYS_NET_CloseSocket
*/
static int _SYS_NET_CloseSocket(int socket) {
  close(socket);
  return 0;
}

/*********************************************************************
*
*       _SYS_NET_ReadSocketAvailable
*/
static int _SYS_NET_ReadSocketAvailable(int socket, unsigned char *pBuffer, uint32_t maxBytesToRead, uint32_t *pNumBytesRead, uint32_t msTimeout) {
  fd_set*         pSocketList = NULL;
  struct timeval  timeout;
  int             retValue;
  int         status;

  if ((NULL == pBuffer) || (NULL == pNumBytesRead)) {
    status = -1;
    goto exit;
  }

  if (0 != msTimeout) {
    /* handle timeout case */
    if (NULL == (pSocketList = (fd_set*) malloc(sizeof(fd_set)))) {
      status = -1;
      goto exit;
    }

    /* add the socket of interest to the list */
    FD_ZERO(pSocketList);
    FD_SET(socket, pSocketList);

    /* compute timeout (milliseconds) */
    timeout.tv_sec  = msTimeout / 1000;
    timeout.tv_usec = (msTimeout % 1000) * 1000;    /* convert ms to us */

    /* Note: Windows ignores the first parameter '1' */
    /* other platforms may want (highest socket + 1) */

    /*  The first argument to select is the highest file
        descriptor value plus 1. In most cases, you can
        just pass FD_SETSIZE and you'll be fine. */


    if (0 == select(FD_SETSIZE, pSocketList, NULL, NULL, &timeout)) {
      status = -1;
      goto exit;
    }
  }

  *pNumBytesRead = 0;

  retValue = recv(socket, pBuffer, maxBytesToRead, 0);

  if (retValue < 0) {
    if ((EWOULDBLOCK == errno) || (EAGAIN == errno))
      status = -1;
    else
      status = -1;
    goto exit;
  }

  if (0 == retValue) {
    //Disconnected from the network
  }

  *pNumBytesRead = retValue;

  status = 0;

exit:
  if (NULL != pSocketList)
    free(pSocketList);

  return status;

}

/*********************************************************************
*
*       _SYS_NET_WriteSocket
*/
static int _SYS_NET_WriteSocket(int socket, const unsigned char * pBuffer, uint32_t numBytesToWrite, uint32_t *pNumBytesWritten) {
  int     retValue;
  int status;

  if ((NULL == pBuffer) || (NULL == pNumBytesWritten)) {
    status = -1;
    goto exit;
  }

  retValue = send((int)socket, (const char *)pBuffer, numBytesToWrite, 0);
  if (0 > retValue) {
    if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
      status = -1;
    else
      status = -1;

    *pNumBytesWritten = 0;
    goto exit;
  }

  *pNumBytesWritten = retValue;
  status = 0;

exit:
  return status;
}

/*********************************************************************
*
*       _SYS_NET_GetPeerName
*/
static int _SYS_NET_GetPeerName(int socket, unsigned short *pRetPortNo, uint32_t *pRetAddr) {

  struct sockaddr_in      myAddress = { 0 };
  socklen_t               addrLen = sizeof(myAddress);
  int                     status = 0;

  if (0 > getpeername(socket, (struct sockaddr *)&myAddress, &addrLen)) {
    status = -1;
    goto exit;
  }

  *pRetPortNo = htons(myAddress.sin_port);
  *pRetAddr = htonl(myAddress.sin_addr.s_addr);

exit:
  return status;
}

/*********************************************************************
*
*       _SYS_NET_GetSockName
*/
static int _SYS_NET_GetSockName(int socket, unsigned short *pRetPortNo, uint32_t *pRetAddr) {

  struct sockaddr_in      myAddress = { 0 };
  socklen_t               addrLen = sizeof(myAddress);
  int                     status = 0;

  if (0 > getsockname(socket, (struct sockaddr *)&myAddress, &addrLen)) {
    status = -1;
    goto exit;
  }

  *pRetPortNo = htons(myAddress.sin_port);
  *pRetAddr = htonl(myAddress.sin_addr.s_addr);

exit:
  return status;
}

/*********************************************************************
*
*       SYS Util.
*/

/*********************************************************************
*
*       _StoreU32BE
*
*  Function description:
*    Stores a 32 bit value in big endian format into a byte array.
*/
static void _StoreU32BE(uint8_t *pBuffer, uint32_t Data) {
  *pBuffer++ = (uint8_t)(Data >> 24);
  *pBuffer++ = (uint8_t)(Data >> 16);
  *pBuffer++ = (uint8_t)(Data >> 8);
  *pBuffer   = (uint8_t) Data;
}

/*********************************************************************
*
*       _StoreU16LE
*
*  Function description:
*    Writes 16 bit little endian.
*/
static void _StoreU16LE(uint8_t *pBuffer, uint16_t Data) {
  *pBuffer++ = (uint8_t)Data;
  Data >>= 8;
  *pBuffer = (uint8_t)Data;
}

/*********************************************************************
*
*       _SYS_Sleep
*/
static void _SYS_Sleep(uint64_t milliseconds) {
  usleep((useconds_t)(milliseconds*1000));
}

/*********************************************************************
*
*       User management.
*/

/*********************************************************************
*
*       _FindUser
*
*  Function description
*    Callback function for user management.
*    Checks if user name is valid.
*
*  Return value
*    0    UserID invalid or unknown
*  > 0    UserID, no password required
*  < 0    - UserID, password required
*/
static int _FindUser (const char * sUser) {
  if (strcmp(sUser, "Admin") == 0) {
    return (0 - USER_ID_ADMIN);
  }
  if (strcmp(sUser, "anonymous") == 0) {
    return (USER_ID_ANONYMOUS);
  }
  return (0);
}

/*********************************************************************
*
*       _CheckPass
*
*  Function description
*    Callback function for user management.
*    Checks user password.
*
*  Return value
*    0    UserID know, password valid
*    1    UserID unknown or password invalid
*/
static int _CheckPass (int UserId, const char * sPass) {
  if ((UserId == USER_ID_ADMIN) && (strcmp(sPass, "Secret") == 0)) {
    return (0);
  } else {
    return (1);
  }
}

/*********************************************************************
*
*       _GetDirInfo
*
*  Function description
*    Callback function for permission management.
*    Checks directory permissions.
*
*  Return value
*    Returns a combination of the following:
*    IP_FTPS_PERM_VISIBLE    - Directory is visible as a directory entry
*    IP_FTPS_PERM_READ       - Directory can be read/entered
*    IP_FTPS_PERM_WRITE      - Directory can be written to
*
*  Parameters
*    UserId        - User ID returned by _FindUser()
*    sDirIn        - Full directory path and with trailing slash
*    sDirOut       - Reserved for future use
*    DirOutSize    - Reserved for future use
*
*  Notes
*    In this sample configuration anonymous user is allowed to do anything.
*    Samples for folder permissions show how to set permissions for different
*    folders and users. The sample configures permissions for the following
*    directories:
*      - /READONLY/: This directory is read only and can not be written to.
*      - /VISIBLE/ : This directory is visible from the folder it is located
*                    in but can not be accessed.
*      - /ADMIN/   : This directory can only be accessed by the user "Admin".
*/
static int _GetDirInfo(int UserId, const char * sDirIn, char * sDirOut, int DirOutSize) {
  int Perm;

  (void)sDirOut;
  (void)DirOutSize;

  Perm = IP_FTPS_PERM_VISIBLE | IP_FTPS_PERM_READ | IP_FTPS_PERM_WRITE;

  if (strcmp(sDirIn, "/READONLY/") == 0) {
    Perm = IP_FTPS_PERM_VISIBLE | IP_FTPS_PERM_READ;
  }
  if (strcmp(sDirIn, "/VISIBLE/") == 0) {
    Perm = IP_FTPS_PERM_VISIBLE;
  }
  if (strcmp(sDirIn, "/ADMIN/") == 0) {
    if (UserId != USER_ID_ADMIN) {
      return (0);  // Only Admin is allowed for this directory
    }
  }
  return (Perm);
}

/*********************************************************************
*
*       _GetFileInfo
*
*  Function description
*    Callback function for permission management.
*    Checks file permissions.
*
*  Return value
*    Returns a combination of the following:
*    IP_FTPS_PERM_VISIBLE    - File is visible as a file entry
*    IP_FTPS_PERM_READ       - File can be read
*    IP_FTPS_PERM_WRITE      - File can be written to
*
*  Parameters
*    UserId        - User ID returned by _FindUser()
*    sFileIn       - Full path to the file
*    sFileOut      - Reserved for future use
*    FileOutSize   - Reserved for future use
*
*  Notes
*    In this sample configuration all file accesses are allowed. File
*    permissions are checked against directory permissions. Therefore it
*    is not necessary to limit permissions on files that reside in a
*    directory that already limits access.
*    Setting permissions works the same as for _GetDirInfo() .
*/
static int _GetFileInfo(int UserId, const char * sFileIn, char * sFileOut, int FileOutSize) {
  int Perm;

  (void)UserId;
  (void)sFileIn;
  (void)sFileOut;
  (void)FileOutSize;

  Perm = IP_FTPS_PERM_VISIBLE | IP_FTPS_PERM_READ | IP_FTPS_PERM_WRITE;

  return (Perm);
}

/*********************************************************************
*
*       _GetTimeDate
*
*  Description:
*    Current time and date in a format suitable for the FTP server.
*
*    Bit 0-4:   2-second count (0-29)
*    Bit 5-10:  Minutes (0-59)
*    Bit 11-15: Hours (0-23)
*    Bit 16-20: Day of month (1-31)
*    Bit 21-24: Month of year (1-12)
*    Bit 25-31: Count of years from 1980 (0-127)
*
*  Note:
*    FTP server requires a real time clock for to transmit the
*    correct timestamp of files. Lists transmits either the
*    year or the HH:MM. For example:
*    -rw-r--r--   1 root 1000 Jan  1  1980 DEFAULT.TXT
*    or
*    -rw-r--r--   1 root 1000 Jul 29 11:40 PAKET01.TXT
*    The decision which of both infos the server transmits
*    depends on the system time. If the year of the system time
*    is identical to the year stored in the timestamp of the file,
*    the time will be transmitted, if not the year.
*/
static uint32_t _GetTimeDate(void) {
  uint32_t r;
  uint16_t Sec, Min, Hour;
  uint16_t Day, Month, Year;

  Sec   = 0;        // 0 based.  Valid range: 0..59
  Min   = 0;        // 0 based.  Valid range: 0..59
  Hour  = 0;        // 0 based.  Valid range: 0..23
  Day   = 1;        // 1 based.    Means that 1 is 1. Valid range is 1..31 (depending on month)
  Month = 1;        // 1 based.    Means that January is 1. Valid range is 1..12.
  Year  = 28;        // 1980 based. Means that 2008 would be 28.
  r   = Sec / 2 + (Min << 5) + (Hour  << 11);
  r  |= (uint32_t)(Day + (Month << 5) + (Year  << 9)) << 16;
  return (r);
}

/**********************************************************************
*
*       IP interface.
*
*  Mapping of the required socket functions to the actual IP stack.
*  This is required becasue the socket functions are slightly different on different systems.
*/

/*********************************************************************
*
*       _Send
*
*  Function description
*    Callback function that sends data to the client on socket level.
*
*  Parameters
*    pData
*    len
*    Socket
*
*  Return value
*    >= 0:  O.K., number of bytes sent
*     -1 :  Error
*/
static int _Send(const unsigned char * pData, int len, FTPS_SOCKET hSock) {
  uint32_t     retValue;
  int     status;
  status = _SYS_NET_WriteSocket((int)hSock, pData, len, &retValue);
  if (status < 0) {
    return (-1);
  }
  return (retValue);
}

/*********************************************************************
*
*       _Recv
*
*  Function description
*    Callback function that receives data from the client on socket level.
*
*  Parameters
*    pData
*    len
*    Socket
*
*  Return value
*    >= 0:  O.K., number of bytes received
*     -1 :  Error
*
*  Notes
*    (1) Returns as soon as something has been received (may be less than MaxNumBytes) or error happened
*/
static int _Recv(unsigned char * pData, int len, FTPS_SOCKET hSock) {
  uint32_t     retValue;
  int     status;
  status = _SYS_NET_ReadSocketAvailable((int)hSock, pData, len, &retValue, 0);
  if (status < 0) {
    return (-1);
  }
  return (retValue);
}

/*********************************************************************
*
*       _Connect
*
*  Function description
*    This function is called from the FTP server module if the client
*    uses active FTP to establish the data connection.
*    Callback function that handles the connect back to a FTP client on socket level if not using passive mode.
*
*  Parameters
*    CtrlSocket
*    Port
*    Socket
*
*  Return value
*    >= 0 :  O.K., 
*    NULL :  Error
*
*  Notes
*    
*/
static FTPS_SOCKET _Connect(FTPS_SOCKET hCtrlSock, uint16_t Port) {
  int              DataSock;
  int                status;
  uint32_t     sin_addr;
  unsigned short   sin_port;
  unsigned char    str[INET_ADDRSTRLEN];

  _SYS_NET_GetPeerName((int)hCtrlSock, &sin_port, &sin_addr);
  sin_addr = htonl(sin_addr);
  inet_ntop(AF_INET, &sin_addr, (char *)str, INET_ADDRSTRLEN);

  status = _SYS_NET_ConnectSocket(&DataSock, str, Port);
  if(status < 0){
    return (NULL);
  }
  return ((FTPS_SOCKET)DataSock);
}

/*********************************************************************
*
*       _Disconnect
*
*  Function description
*    This function is called from the FTP server module to close the
*    data connection.
*    Callback function that disconnects a connection to the FTP client on socket level if not using passive mode.
*
*  Parameters
*    DataSocket
*/
static void _Disconnect(FTPS_SOCKET hDataSock) {
  _SYS_NET_CloseSocket((int) hDataSock);
}

/*********************************************************************
*
*       _Listen
*
*  Function description
*    This function is called from the FTP server module if the client
*    uses passive FTP. It creates a socket and searches for a free port
*    which can be used for the data connection.
*    Callback function that binds the server to a port and addr.
*
*  Parameters
*    CtrlSocket  
*    pIPAddr   IPv4 address expected in little endian form, meaning 127.0.0.1 is expected as 0x7F000001
*    pPort     Port to listen at
*
*  Return value
*    > 0  : O.K.  Socket descriptor
*    NULL : Error
*/
static FTPS_SOCKET _Listen(FTPS_SOCKET hCtrlSock, uint16_t *pPort, uint8_t * pIPAddr) {
  int              DataSock;
  int                status;
  uint32_t     sin_addr;
  unsigned short   sin_port;

  status = _SYS_NET_ListenSocket(&DataSock, 0);  // Let Stack find a free port
  if (status < 0) {
    return (NULL);
  }

  //
  //  Get port number stack has assigned
  //
  _SYS_NET_GetSockName(DataSock, &sin_port, &sin_addr);
  _StoreU16LE((uint8_t *)pPort, sin_port);
  _SYS_NET_GetSockName((int)hCtrlSock, &sin_port, &sin_addr);
  _StoreU32BE(pIPAddr, sin_addr);
  return ((FTPS_SOCKET)DataSock);
}

/*********************************************************************
*
*       _Accept
*
*  Function description
*    This function is called from the FTP server module if the client
*    uses passive FTP. It sets the command socket to non-blocking before
*    accept() will be called. This guarantees that the FTP server always
*    returns even if the connection to the client gets lost while
*    accept() waits for a connection. The timeout is set to 10 seconds.
*    Callback function that accepts incoming connections.
*
*  Parameters
*    CtrlSocket
*    pSocket
*
*  Return value
*     = 0 :  O.K. Handle to socket of new connection that has been established
*     -1  :  Error
*/
static int _Accept(FTPS_SOCKET hCtrlSock, FTPS_SOCKET * phDataSocket) {
  int         Socket;
  int       DataSock;
  int         status;
  int  isBreakRequest = FALSE;

  (void)hCtrlSock;

  Socket   = *(int*)phDataSocket;
  status = _SYS_NET_AcceptSocket(&DataSock, Socket, &isBreakRequest);
  if (status < 0) {
    return (-1);
  }
  *phDataSocket = (FTPS_SOCKET)DataSock;
  _SYS_NET_CloseSocket(Socket);
  //
  // Successfully connected
  //
  return (0);
}

/*********************************************************************
*
*       Private data
*
**********************************************************************
*/

/*********************************************************************
*
*       FTPS_ACCESS_CONTROL
*
*  Description
*   Access control function table
*/
static FTPS_ACCESS_CONTROL _Access_Control = {
  _FindUser,
  _CheckPass,
  _GetDirInfo,
  _GetFileInfo  // Optional, only required if permissions for individual files shall be used
};

static const FTPS_APPLICATION _Application = {
  &_Access_Control,
  _GetTimeDate
};

static const IP_FTPS_API _IP_API = {
  _Send,
  _Recv,
  _Connect,
  _Disconnect,
  _Listen,
  _Accept
};

const _FS_API IP_FS_Linux = {
  //
  // Read only file operations.
  //
  _FS_LINUX_Open,
  _FS_LINUX_Close,
  _FS_LINUX_ReadAt,
  _FS_LINUX_GetLen,
  //
  // Simple directory operations.
  //
  _FS_LINUX_ForEachDirEntry,
  _FS_LINUX_GetDirEntryFileName,
  _FS_LINUX_GetDirEntryFileSize,
  _FS_LINUX_GetDirEntryFileTime,
  _FS_LINUX_GetDirEntryAttributes,
  //
  // Simple write type file operations.
  //
  _FS_LINUX_Create,
  _FS_LINUX_DeleteFile,
  _FS_LINUX_WriteAt,
  //
  // Additional directory operations
  //
  _FS_LINUX_MakeDir,
  _FS_LINUX_RemoveDir,
};

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/

/*********************************************************************
*
*       _AddToConnectCnt
*
*/
static void _AddToConnectCnt(int Delta) {
  _ConnectCnt += Delta;
}

/*********************************************************************
*
*       _FTPServerChildTask
*
*/
static void* _FTPServerChildTask(void * Context) {
  int                 hSock;

  _pFS_API   = &IP_FS_Linux;
  hSock      = (int)Context;

  IP_FTPS_Process(&_IP_API, Context, _pFS_API, &_Application);

  _SYS_Sleep(2);          // Give connection some time to complete
  _SYS_NET_CloseSocket(hSock);
  _AddToConnectCnt(-1);
  return (0);
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       _FTPServerParentTask
*
*/
void _FTPServerParentTask(void) {
  int            hSockListen;
  int                  hSock;
  int                      i;
  pthread_t         ThreadId;
  int                 status;
  int          isBreakRequest = FALSE;

  //
  // Config Base Dir
  //
  _FS_LINUX_ConfigBaseDir("./");
  //
  // Get a socket into listening state
  //
  status = _SYS_NET_ListenSocket(&hSockListen, 2121);
  if (status < 0) {
    perror("listen tcp error");
    exit(-1);
  }
  //
  // Loop once per client and create a thread for the actual server
  //
  do {
    //
    // Wait for an incoming connection
    //
    status = _SYS_NET_AcceptSocket(&hSock, hSockListen, &isBreakRequest);
    if (status < 0) {
      continue;               // Error, try again.
    }
    if (_ConnectCnt < MAX_CONNECTIONS) {
      for (i = 0; i < MAX_CONNECTIONS; i++) {
        pthread_create(&ThreadId, NULL, _FTPServerChildTask, (void*)hSock);
        _AddToConnectCnt(1);
        break;
      }
    } else {
      IP_FTPS_OnConnectionLimit(&_IP_API, (FTPS_SOCKET)hSock);
      _SYS_Sleep(2);          // Give connection some time to complete
      _SYS_NET_CloseSocket(hSock);
    }
  } while (1);
}

/*************************** End of file ****************************/

