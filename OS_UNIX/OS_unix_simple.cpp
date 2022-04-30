//
// Created by user on 22-4-20.
//
#include <unistd.h>
#include <cstring>
#include <utility>
#include <sys/stat.h>
#include <fcntl.h>
#include <cassert>
#include <stdexcept>
#include <list>
#include "../tinySQL_VFS.h"
#include "../tinySQL_def.h"

namespace pandas {
    class UnixINode {
    private:
        dev_t dev;
        ino_t ino;

    public:
        UnixINode(dev_t dev, ino_t ino) : dev(dev), ino(ino) {

        }
    };


    [[maybe_unused]] class GlobalResourceManage {
    private:
        std::list<UnixINode> inodes;
        pthread_mutex_t globalMutex;
    public:


        GlobalResourceManage() : globalMutex() {
            pthread_mutex_init(&globalMutex, nullptr);
        }

        ~GlobalResourceManage() {
            pthread_mutex_destroy(&globalMutex);
        }
    } static globalResource;


    class UnixFile : public pandas_file {
    public:
        const int iFd;
        const std::string pathName;
        UnixINode *pInode;
        unsigned char eFileLock;
        unsigned short ctrlFlags;
        int lastErrno;

        explicit UnixFile(pandas_IOMethods *ioMethods, std::string pathName, int fd, UnixINode *pInode) :
                pandas_file(ioMethods), iFd(fd), pInode(pInode), pathName(std::move(pathName)), eFileLock(0),
                ctrlFlags(0),
                lastErrno(0) {
        }
    };


    class UnixIOMethods final : public pandas_IOMethods {
    private:
        static int SeekAndWriteFd(int fd, long offset, const void *pBuffer, int writeCount, int *piErrno) {
            long status;
            assert(fd > 2);
            assert(pBuffer);
            assert(piErrno);

            do {
                long seek = lseek(fd, offset, SEEK_SET);
                if (seek < 0) {
                    status = -1;
                    break;
                }
                status = write(fd, pBuffer, writeCount);
            } while (status < 0 && errno == EINTR);
            if (status < 0) *piErrno = errno;
            return status;
        }

    public:
        int xClose(pandas_file *pFile) override {
            auto p = static_cast < UnixFile * >(pFile);
            close(p->iFd);
            delete p;
            return Succeed;
        }

        int xRead(pandas_file *pFile, void *buffer, int readCount, long offset) override {
            auto p = static_cast < UnixFile * >(pFile);

            assert(p != nullptr && p->iFd > 2);
            assert(readCount >= 0);
            assert(offset >= 0);

            off_t nowOffset = lseek(p->iFd, offset, SEEK_SET);
            if (nowOffset != offset)
                return IOError_Read;
            size_t Count = read(p->iFd, buffer, readCount);
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

        int xWrite(pandas_file *pFile, const void *buffer, int writeCount, long offset) override {
            auto p = static_cast < UnixFile * >(pFile);
            long wrote;
            int status;

            assert(p != nullptr && p->iFd > 2);
            assert(writeCount >= 0);
            assert(offset >= 0);


            while ((wrote = SeekAndWriteFd(p->iFd, offset, buffer, writeCount, &p->lastErrno)) < writeCount &&
                   wrote > 0) {
                writeCount -= wrote;
                offset += wrote;
                buffer = &(static_cast <const char * >(buffer)[wrote]);
            }

            if(writeCount >wrote){
                if(wrote < 0 && p->lastErrno != ENOSPC)
                    return IOError_Write;
                else{
                    p->lastErrno = 0;
                    return SpaceFull;
                }
            }
            return Succeed;
        }

        int xTruncate(pandas_file *pFile, long size) override {
            auto p = static_cast < UnixFile * >(pFile);
            if (ftruncate(p->iFd, size))
                return IOError_Truncate;
            return Succeed;
        }

        int xSync(pandas_file *pFile, int flags) override {
            auto p = static_cast < UnixFile * >(pFile);
            return fsync(p->iFd) ? IOError_Fsync : Succeed;
        }

        int xFileSize(pandas_file *pFile, unsigned long *pSize) override {
            auto p = static_cast < UnixFile * >(pFile);
            struct stat buf;
            if (fstat(p->iFd, &buf))
                return IOError_FileSize;
            *pSize = buf.st_size;
            return Succeed;
        }

        int xLock(pandas_file *, int) override {
            return Succeed;
        }

        int xUnlock(pandas_file *, int) override {
            return Succeed;
        }

        int xCheckReservedLock(pandas_file *, int *pResOut) override {
            *pResOut = 0;
            return Succeed;
        }

        int xFileControl(pandas_file *, int op, void *pArg) override {
            return NotFound;
        }

        int xSectorSize(pandas_file *) override {
            return 4096;
        }

        int xDeviceCharacteristics(pandas_file *) override {
            return 0;
        }

        explicit UnixIOMethods(int version) : pandas_IOMethods(version) {
        }
    };


    pandas_IOMethods *pandas_IOMethods::IOMethodsGet() {
        static UnixIOMethods unixIoMethods(1);
        return &unixIoMethods;
    }

    class UnixVFS final : public pandas_VFS {
    public:
        int xOpen(const char *zName, pandas_file **ppFile,
                  int flags, int *pOutFlags) override {
            int oflag = 0;
            int fd = 0;
            if (flags & Open_Create) oflag |= O_CREAT;
            if (flags & Open_Exclusive) oflag |= O_EXCL;
            if (flags & Open_ReadOnly) oflag |= O_RDONLY;
            if (flags & Open_ReadWrite) oflag |= O_RDWR;

            fd = open(zName, oflag, DefaultFilePermission);
            if (fd < 0)
                return CanNotOpen;
            if (pOutFlags)
                *pOutFlags = flags;
            *ppFile = new UnixFile(pandas_IOMethods::IOMethodsGet(), zName, fd, nullptr);
            return Succeed;
        }

        int xDelete(const char *zName) override {
            return unlink(zName) ? IOError_Delete : Succeed;
        }

        int xAccess(const char *zName, int flags, int *pResOut) override {

            int eAccess = F_OK;
            assert(flags == Access_Exists ||
                   flags == Access_Read ||
                   flags == Access_ReadWrite);

            if (flags == Access_Read) eAccess = R_OK;
            if (flags == Access_ReadWrite) eAccess = R_OK | W_OK;

            *pResOut = (access(zName, eAccess) == 0);
            return Succeed;
        }

        int xFullPathname(const char *zName, int nOut, char *zOut) override {
            char Dir[mxPathName + 1];
            if (zName[0] == '/') {
                Dir[0] = 0;
            } else {
                if (getcwd(Dir, sizeof(Dir)) == nullptr)
                    return IOError;
            }
            Dir[mxPathName] = 0;
            snprintf(zOut, nOut, "%s/%s", Dir, zName);
            zOut[nOut - 1] = 0;
            return Succeed;
        }

        void *xDlOpen(const char *zFilename) override {
            throw std::runtime_error("not support");
        }

        void xDlError(int nByte, char *zErrMsg) override {
            throw std::runtime_error("not support");
        }


        void xDlClose(void *) override {
            throw std::runtime_error("not support");
        }

        int xRandomness(int nByte, char *zOut) override {
            return Succeed;
        }


        int xSleep(int microseconds) override {
            sleep(microseconds / 1000000);
            usleep(microseconds % 1000000);
            return microseconds;
        }

        int xCurrentTime(double *pTime) override {
            time_t t = time(nullptr);
            *pTime = static_cast<double>(t) / 86400.0 + 2440587.5;
            return Succeed;
        }

        int xGetLastError(int, char *) override {
            throw std::runtime_error("not support");
        }


        int xCurrentTimeInt64(unsigned long *pOutTime) override {
            *pOutTime = time(nullptr);
            return Succeed;
        }


        UnixVFS(int version, int maxPathNameLength, std::string name,
                void *pAppData = nullptr) : pandas_VFS(version, maxPathNameLength, std::move(name), pAppData) {

        }
    };

    pandas_VFS *pandas_VFS::VFSGet() {
        static UnixVFS unixVFS(1, 256, "unixVFS");
        return &unixVFS;
    }
}