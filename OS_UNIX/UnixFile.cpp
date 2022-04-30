//
// Created by user on 22-4-27.
//
#include "OS_unix.h"

namespace tinySQL {

    static int GetErrorFromPosixError(int posixError, int ioError) {
        switch (posixError) {
            case EACCES:
            case EAGAIN:
            case ETIMEDOUT:
            case EBUSY:
            case EINTR:
            case ENOLCK:
                return Busying;
            case EPERM:
                return PermitError;
            default:
                return ioError;
        }
    }

    int UnixFile::ProcessFileLockSet(int fd, short l_type, off_t l_start, off_t l_length) {
        struct flock lock{l_type, SEEK_SET, l_start, l_length, 0};
        return OsSetAdvisoryLock(fd, &lock);
    }

    int UnixFile::SeekAndWriteFd(int fd, long offset, const void *pBuffer, long writeCount, int *piErrno) {
        long status;
        assert(fd > 2);
        assert(pBuffer);
        assert(piErrno);

        do {
            long seek = OsLseek(fd, offset, SEEK_SET);
            if (seek < 0) {
                status = -1;
                break;
            }
            status = OsWrite(fd, pBuffer, writeCount);
        } while (status < 0 && errno == EINTR);
        if (status < 0) *piErrno = errno;
        return status;
    }

    int UnixFile::FcntlSizeHint(UnixFile *pFile, long nByte) {
        if (pFile->chunkSize > 0) {
            long nSize;
            struct stat buf{};
            if (OsFstat(pFile->iFd, &buf))
                return IOError_Fstat;
            nSize = ((nByte + pFile->chunkSize - 1) / pFile->chunkSize) * pFile->chunkSize;
            if (nSize > buf.st_size) {
                int err;
                do {
                    err = OsFallocate(pFile->iFd, 0, buf.st_size, nSize - buf.st_size);
                } while (err == EINTR);
                if (err && err != EINVAL)
                    return IOError_Write;
            }
        }
        return Succeed;
    }

    void UnixFile::ModeBit(UnixFile *pFile, unsigned char mask, int *pArg) {
        if (*pArg < 0)
            *pArg = (pFile->ctrlFlags & mask) != 0;
        else if (*pArg == 0)
            pFile->ctrlFlags &= ~mask;
        else
            pFile->ctrlFlags |= mask;
    }

    const char *UnixFile::TempFileDir() {
        return "/dev/shm";
    }


    int UnixFile::xClose() {
        auto p = static_cast < UnixFile * >(this);
        xUnlock(Lock_None);
        p->pInode->Lock();
        if (p->pInode->eFileLock != Lock_None)
            p->pInode->SetPendingFd(p->iFd);
        p->pInode->Unlock();
        UnixINode::UnixInodeRelease(p->pInode);
        delete p;
        return Succeed;
    }

    int UnixFile::xRead(void *buffer, long readCount, long offset) {
        auto p = static_cast < UnixFile * >(this);

        assert(p != nullptr && p->iFd > 2);
        assert(readCount >= 0);
        assert(offset >= 0);

        off_t nowOffset = OsLseek(p->iFd, offset, SEEK_SET);
        if (nowOffset != offset)
            return IOError_Read;
        size_t Count = OsRead(p->iFd, buffer, readCount);
        p->lastErrno = errno;
        if (Count != readCount) {
            if (Count < 0) {
                switch (p->lastErrno) {
                    case ERANGE:
                    case EIO:
                    case ENXIO:
                        return IOError_CorruptFs;
                }
                return IOError_Read;
            } else {
                memset(&((char *) buffer)[Count], 0, readCount - Count);
                return IOError_ReadShort;
            }
        }
        return Succeed;
    }

    int UnixFile::xWrite(const void *buffer, long writeCount, long offset) {
        auto p = static_cast < UnixFile * >(this);
        long wrote;


        assert(p != nullptr && p->iFd > 2);
        assert(writeCount >= 0);
        assert(offset >= 0);


        while ((wrote = SeekAndWriteFd(p->iFd, offset, buffer, writeCount, &p->lastErrno)) < writeCount &&
               wrote > 0) {
            writeCount -= wrote;
            offset += wrote;
            buffer = &(static_cast <const char * >(buffer)[wrote]);
        }

        if (writeCount > wrote) {
            if (wrote < 0 && p->lastErrno != ENOSPC)
                return IOError_Write;
            else {
                p->lastErrno = 0;
                return SpaceFull;
            }
        }
        return Succeed;
    }

    int UnixFile::xTruncate(long size) {
        auto p = static_cast < UnixFile * >(this);
        assert(p);
        int status = OsFtruncate(p->iFd, size);
        while (status < 0 && errno == EINTR) {
            status = OsFtruncate(p->iFd, size);
        }
        if (status < 0) {
            p->lastErrno = errno;
            return IOError_Truncate;
        }
        return Succeed;
    }

    int UnixFile::xSync(int flags) {
        auto p = static_cast < UnixFile * >(this);
        return OsFsync(p->iFd) ? IOError_Fsync : Succeed;
    }

    int UnixFile::xFileSize(unsigned long *pSize) {
        auto p = static_cast < UnixFile * >(this);
        struct stat buf{};
        if (OsFstat(p->iFd, &buf)) {
            p->lastErrno = errno;
            return IOError_Fstat;
        }
        *pSize = buf.st_size;
        return Succeed;
    }


    int UnixFile::xLock(int eFileLock) {
        auto p = static_cast < UnixFile * >(this);
        int status = 0;
        int tError = 0;
        assert(p);


        if (p->eFileLock >= eFileLock)
            return Succeed;

        assert(p->eFileLock != Lock_None || eFileLock == Lock_Shared);
        assert(eFileLock != Lock_Pending);
        assert(eFileLock != Lock_Reserved || p->eFileLock == Lock_Shared);

        p->pInode->Lock();
        if (p->eFileLock != pInode->eFileLock &&
            (pInode->eFileLock >= Lock_Pending || eFileLock > Lock_Shared)) {
            status = Busying;
            goto end_lock;
        }

        if (eFileLock == Lock_Shared &&
            (pInode->eFileLock == Lock_Shared || pInode->eFileLock == Lock_Reserved)) {
            assert(eFileLock == Lock_Shared);
            assert(p->eFileLock == Lock_None);
            assert(pInode->nShared > 0);
            p->eFileLock = Lock_Shared;
            pInode->nShared++;
            pInode->nLock++;
            goto end_lock;
        }

        if (eFileLock == Lock_Shared ||
            (eFileLock == Lock_Exclusive && p->eFileLock < Lock_Pending))
            if (ProcessFileLockSet(p->iFd, eFileLock == Lock_Shared ? F_RDLCK : F_WRLCK, 1, LockZone_PendingByte)) {
                tError = errno;
                status = GetErrorFromPosixError(tError, IOError_Lock);
                if (status == Busying)
                    p->lastErrno = tError;
                goto end_lock;
            }

        if (eFileLock == Lock_Shared) {
            assert(pInode->nShared == 0);
            assert(pInode->eFileLock == 0);
            assert(status == Succeed);

            if (ProcessFileLockSet(p->iFd, F_RDLCK, LockZone_SharedFirst, LockZone_SharedSize)) {
                tError = errno;
                status = GetErrorFromPosixError(tError, IOError_Lock);
            }

            if (ProcessFileLockSet(p->iFd, F_UNLCK, LockZone_PendingByte, 1) && status == Succeed) {
                tError = errno;
                status = IOError_Unlock;
            }
            if (status) {
                if (status != Busying) {
                    p->lastErrno = errno;
                    goto end_lock;
                } else {
                    p->eFileLock = Lock_Shared;
                    pInode->nLock++;
                    pInode->nShared = 1;
                }
            }

        } else if (eFileLock == Lock_Exclusive && pInode->nShared > 1)
            status = Busying;
        else {
            assert(p->eFileLock != 0);
            assert(eFileLock == Lock_Reserved || eFileLock == Lock_Exclusive);
            int ret;
            if (eFileLock == Lock_Reserved)
                ret = ProcessFileLockSet(p->iFd, F_WRLCK, LockZone_ReservedByte, 1);
            else
                ret = ProcessFileLockSet(p->iFd, F_WRLCK, LockZone_SharedFirst, LockZone_SharedSize);
            if (ret) {
                tError = errno;
                status = GetErrorFromPosixError(tError, IOError_Lock);
                if (status != Busying)
                    p->lastErrno = errno;
            }
        }
        if (status == Succeed) {
            p->eFileLock = eFileLock;
            pInode->eFileLock = eFileLock;
        } else if (eFileLock == Lock_Exclusive) {
            p->eFileLock = Lock_Pending;
            pInode->eFileLock = Lock_Pending;
        }

        end_lock:
        p->pInode->Unlock();
        return status;
    }

    int UnixFile::xUnlock(int eFileLock) {
        if (eFileLock >= this->eFileLock)
            return Succeed;
        assert(eFileLock <= Lock_Shared);
        auto pInode = this->pInode;
        auto p = this;
        int status = Succeed;
        pInode->Lock();
        if (p->eFileLock > Lock_Shared) {
            assert(p->eFileLock == pInode->eFileLock);
            if (eFileLock == Lock_Shared) {
                if (ProcessFileLockSet(p->iFd, F_RDLCK, LockZone_SharedFirst, LockZone_SharedSize)) {
                    status = IOError_ReadLock;
                    p->lastErrno = errno;
                    goto end_lock;
                }
            }
            if (ProcessFileLockSet(p->iFd, F_UNLCK, Lock_Pending, 2)) {
                status = IOError_Unlock;
                p->lastErrno = errno;
                goto end_lock;
            } else
                pInode->eFileLock = Lock_Shared;
        }
        if (eFileLock == Lock_None) {
            pInode->nShared--;
            if (pInode->nShared == 0) {
                if (ProcessFileLockSet(p->iFd, F_UNLCK, 0, 0)) {
                    status = IOError_Unlock;
                    p->lastErrno = errno;
                    p->eFileLock = Lock_None;
                    pInode->eFileLock = Lock_None;
                } else
                    pInode->eFileLock = Lock_None;
            }
            pInode->nLock--;
            assert(pInode->nLock >= 0);
            if (pInode->nLock == 0)
                pInode->ClosePendingFds();
        }

        end_lock:
        pInode->Unlock();
        if (status == Succeed)
            p->eFileLock = eFileLock;
        return status;
    }

    int UnixFile::xCheckReservedLock(int *pResOut) {
        *pResOut = 0;
        return Succeed;
    }

    int UnixFile::xFileControl(int op, void *pArg) {
        auto p = static_cast<UnixFile *>(this);
        assert(p != nullptr);
        switch (op) {
            case Fcntl_LockState :
                *(int *) pArg = p->eFileLock;
                return Succeed;
            case Fcntl_LastErrno :
                *(int *) pArg = p->lastErrno;
                return Succeed;
            case Fcntl_ChunkSize :
                *(int *) pArg = p->chunkSize;
                return Succeed;
            case Fcntl_SizeHint :
                return FcntlSizeHint(p, *(long *) pArg);
            case Fcntl_PersistWal :
                ModeBit(p, UnixFile_PersistWal, (int *) pArg);
                return Succeed;
            case Fcntl_PowerSafeOverwrite:
                ModeBit(p, UnixFile_PSOW, (int *) pArg);
                return Succeed;
            case Fcntl_VFSName: {
                std::string name(p->pVFS->zName);
                char *pName = new char[name.size() + 1];
                memcpy(pName, name.c_str(), name.size());
                pName[name.size()] = 0;
                *(char **) pArg = pName;
                return Succeed;
            }
            case Fcntl_TempFileName : {
//                const char *zDir = TempFileDir();
//                if(zDir == nullptr)
//                    return IOError_GetTempPath;

                throw std::runtime_error("no support");
            }

            case Fcntl_HaveMoved: {
                struct stat buf{};
                *(int *) pArg = (p->pInode && (OsFstat(p->iFd, &buf) || buf.st_ino != p->pInode->ino));
                return Succeed;
            }
            case Fcntl_ExternalReader :
                throw std::runtime_error("not support");
            default :
                return NotFound;
        }
    }

    int UnixFile::xSectorSize() {
        auto p = static_cast < UnixFile * >(this);
        return p->sectorSize;
    }

    int UnixFile::xDeviceCharacteristics() {
        return 0;
    }

    UnixFile::UnixFile(std::string pathName, int fd,UnixVFS *pVFS) :
            iFd(fd), pInode(nullptr), pVFS(pVFS), pathName(std::move(pathName)), eFileLock(Lock_None), lastErrno(0),
            sectorSize(0), chunkSize(0), ctrlFlags(0) {

        struct stat buf;
        if(fstat(fd,&buf))
            throw std::runtime_error("can not get info about the file");
        sectorSize = buf.st_blksize;
        pInode = UnixINode::UnixINodeFind(buf.st_dev,buf.st_ino);
        pInode->Lock();
        pInode ->nRef++;
        pInode ->Unlock();

    }


}