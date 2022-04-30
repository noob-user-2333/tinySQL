//
// Created by user on 22-4-20.
//

#ifndef SQLITELIKE_TINYSQL_DEF_H
#define SQLITELIKE_TINYSQL_DEF_H
#include <unistd.h>
#include <cstring>
#include <utility>
#include <sys/stat.h>
#include <fcntl.h>
#include <cassert>
#include <stdexcept>
#include <list>
namespace tinySQL {
    static constexpr int DefaultFilePermission = 0644;

    static constexpr int Open_Exclusive = 1;
    static constexpr int Open_Create = 1 << 1;
    static constexpr int Open_ReadOnly = 1 << 2;
    static constexpr int Open_ReadWrite = 1 << 3;
    static constexpr int Open_Delete = 1 << 4;
    static constexpr int Open_SuperJournal = 1 << 8;
    static constexpr int Open_MainJournal = 1 << 9;
    static constexpr int Open_MainWAL = 1 << 10;


    static constexpr int Access_Exists = 0;
    static constexpr int Access_Read = 1;
    static constexpr int Access_ReadWrite = 2;

    static constexpr int Succeed = 0;
    static constexpr int IOError = 1;
    static constexpr int IOError_Write = 2;
    static constexpr int IOError_Read = 3;
    static constexpr int IOError_ReadShort = 4;
    static constexpr int IOError_Truncate = 5;
    static constexpr int IOError_Fsync = 6;
    static constexpr int IOError_Fstat = 7;
    static constexpr int IOError_SectorSize = 8;
    static constexpr int IOError_Delete = 9;
    static constexpr int IOError_CorruptFs = 10;
    static constexpr int IOError_GetTempPath = 11;
    static constexpr int IOError_DeleteNoEntry = 12;
    static constexpr int IOError_Lock = 13;
    static constexpr int IOError_ReadLock = 14;
    static constexpr int IOError_Unlock = 15;

    static constexpr int Busying = 100;
    static constexpr int PermitError = 101;

    static constexpr int Fcntl_LockState = 0;
    static constexpr int Fcntl_LastErrno = 1;
    static constexpr int Fcntl_ChunkSize = 2;
    static constexpr int Fcntl_SizeHint = 3;
    static constexpr int Fcntl_PersistWal = 4;
    static constexpr int Fcntl_PowerSafeOverwrite = 5;
    static constexpr int Fcntl_VFSName = 6;
    static constexpr int Fcntl_TempFileName = 7;
    static constexpr int Fcntl_HaveMoved = 8;
    static constexpr int Fcntl_ExternalReader = 9;
//    static constexpr int

    static constexpr int UnixFile_PersistWal = 0x04;
    static constexpr int UnixFile_PSOW = 0x10;
    static constexpr int NotFound = 0x10;
    static constexpr int CanNotOpen = 0x11;
    static constexpr int SpaceFull = 0x12;

    static constexpr int MinFileDescriptor = 3;

    static constexpr int Lock_None = 0;
    static constexpr int Lock_Shared = 1;
    static constexpr int Lock_Reserved = 2;
    static constexpr int Lock_Pending = 3;
    static constexpr int Lock_Exclusive = 4;


    static constexpr int LockZone_PendingByte = 0x40000000;
    static constexpr int LockZone_ReservedByte = Lock_Pending + 1;
    static constexpr int LockZone_SharedFirst = Lock_Pending + 2;
    static constexpr int LockZone_SharedSize = 510;

    static constexpr int (*OsOpen)(const char * zName,int flag,...) = open;
    static constexpr int (*OsClose)(int) = close;
    static constexpr int (*OsUnlink)(const char * zPath) = unlink;
    static constexpr int (*OsAccess)(const char *,int) = access;
    static constexpr ssize_t (*OsRead)(int,void*,size_t) = read;
    static constexpr ssize_t (*OsWrite)(int,const void*,size_t) = write;
    static constexpr off_t  (*OsLseek)(int,off_t ,int) = lseek;
    static constexpr int (*OsFstat)(int,struct stat*) = fstat;
    static constexpr int (*OsFchmod)(int,mode_t) = fchmod;
    static constexpr int (*OsFallocate)(int,int,off_t,off_t) = fallocate;
    static constexpr int (*OsFtruncate)(int,off_t) = ftruncate;
    static constexpr int (*OsFsync)(int) = fsync;

    void tinySQL_Randomness(int nByte,void *pBuf);
    int inline OsSetAdvisoryLock(int fd,struct flock *pLock){
        return fcntl(fd,F_SETLK,pLock);
    }

}
#endif //SQLITELIKE_TINYSQL_DEF_H
