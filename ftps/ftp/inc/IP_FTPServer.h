/*********************************************************************
----------------------------------------------------------------------
File        : IP_FTPServer.h
Purpose     : Publics for the FTP server
Revision    : $Rev: 6176 $
---------------------------END-OF-HEADER------------------------------

Attention : Do not modify this file !
*/

#ifndef  IP_FTPS_H
#define  IP_FTPS_H


#if defined(__cplusplus)
extern "C" {     /* Make sure we have C-declarations in C++ programs */
#endif

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/

#define FTPS_AUTH_BUFFER_SIZE   32
#define FTPS_BUFFER_SIZE       512
#define FTPS_DATA_BUFFER_SIZE  512
#define FTPS_MAX_PATH          128
#define FTPS_MAX_PATH_DIR       64

/*********************************************************************
*
*       Function-like macros
*
**********************************************************************
*/

#ifndef _COUNTOF
    #define _COUNTOF(a)          (sizeof(a) / sizeof(a[0]))
#endif
#ifndef _MIN
    #define _MIN(a,b)            (((a) < (b)) ? (a) : (b))
#endif
#ifndef _MAX
    #define _MAX(a,b)            (((a) > (b)) ? (a) : (b))
#endif

/*********************************************************************
*
*       defines
*
**********************************************************************
*/
#define IP_FS_ATTRIB_DIR      (1 << 0)

#define IP_FTPS_PERM_VISIBLE  (1 << 0)
#define IP_FTPS_PERM_READ     (1 << 1)
#define IP_FTPS_PERM_WRITE    (1 << 2)

/*********************************************************************
*
*       Types
*
**********************************************************************
*/

typedef void * FTPS_SOCKET;

typedef struct {
  int         (*pfSend)       (const unsigned char * pData, int Len, FTPS_SOCKET hSock);
  int         (*pfReceive)    (unsigned char * pData, int Len, FTPS_SOCKET hSock);
  FTPS_SOCKET (*pfConnect)    (FTPS_SOCKET hCtrlSock, uint16_t Port);
  void        (*pfDisconnect) (FTPS_SOCKET hDataSock);
  FTPS_SOCKET (*pfListen)     (FTPS_SOCKET hCtrlSock, uint16_t * pPort, uint8_t * pIPAddr);
  int         (*pfAccept)     (FTPS_SOCKET hCtrlSock, FTPS_SOCKET * phDataSocket);
} IP_FTPS_API;

typedef void* FTPS_OUTPUT;

typedef struct {
  int (*pfFindUser)   (const char * sUser);
  int (*pfCheckPass)  (int UserId, const char * sPass);
  int (*pfGetDirInfo) (int UserId, const char * sDirIn, char * sDirOut, int SizeOfDirOut);
  int (*pfGetFileInfo)(int UserId, const char * sFileIn, char * sFileOut, int FileOutSize);
} FTPS_ACCESS_CONTROL;

typedef struct {
  FTPS_ACCESS_CONTROL * pAccess;
  uint32_t (*pfGetTimeDate) (void);
} FTPS_APPLICATION;

typedef void* _FILE_HANDLE;

typedef struct {
  //
  // Read only file operations. These have to be present on ANY file system, even the simplest one.
  //
  void*      (*pfOpenFile)             (const char* sFilename);
  int        (*pfCloseFile)            (void* hFile);
  int        (*pfReadAt)               (void* hFile, void* pBuffer, uint32_t Pos, uint32_t NumBytes);
  long       (*pfGetLen)               (void* hFile);
  //
  // Directory query operations.
  //
  void       (*pfForEachDirEntry)      (void* pContext, const char* sDir, void (*pf)(void* pContext, void* pFileEntry));
  void       (*pfGetDirEntryFileName)  (void* pFileEntry, char* sFileName, uint32_t SizeOfBuffer);
  uint32_t   (*pfGetDirEntryFileSize)  (void* pFileEntry, uint32_t* pFileSizeHigh);
  uint32_t   (*pfGetDirEntryFileTime)  (void* pFileEntry);
  int        (*pfGetDirEntryAttributes)(void* pFileEntry);
  //
  // Write file operations.
  //
  void*      (*pfCreate)               (const char* sFileName);
  int        (*pfDeleteFile)           (const char* sFilename);
  int        (*pfWriteAt)              (void* hFile, void* pBuffer, uint32_t Pos, uint32_t NumBytes);
  //
  // Additional directory operations
  //
  int        (*pfMKDir)                (const char* sDirName);
  int        (*pfRMDir)                (const char* sDirName);
} _FS_API;

/*********************************************************************
*
*       Functions
*
**********************************************************************
*/

int  IP_FTPS_Process           (const IP_FTPS_API * pIP_API, FTPS_SOCKET hCtrlSock, const _FS_API * pFS_API, const FTPS_APPLICATION * pApplication);
void IP_FTPS_OnConnectionLimit (const IP_FTPS_API * pIP_API, FTPS_SOCKET hCtrlSock);

#if defined(__cplusplus)
  }
#endif


#endif   /* Avoid multiple inclusion */

/*************************** End of file ****************************/




