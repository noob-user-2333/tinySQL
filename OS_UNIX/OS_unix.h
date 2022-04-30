//
// Created by user on 22-4-27.
//

#ifndef SQLITELIKE_OS_UNIX_H
#define SQLITELIKE_OS_UNIX_H


#include "../tinySQL_VFS.h"
#include "../tinySQL_def.h"

namespace tinySQL {
    class UnixVFS final : public tinySQL_VFS {
    public:

        int xOpen(const char *zName, tinySQL_file **ppFile,
                  int flags, int *pOutFlags) override;

        int xDelete(const char *zName) override;

        int xAccess(const char *zName, int flags, int *pResOut) override;

        int xFullPathname(const char *zName, int nOut, char *zOut) override;

        void *xDlOpen(const char *zFilename) override;

        void xDlError(int nByte, char *zErrMsg) override;


        void xDlClose(void *) override;

        int xRandomness(int nByte, char *zOut) override;


        int xSleep(int microseconds) override;

        int xCurrentTime(double *pTime) override;

        int xGetLastError(int, char *) override;


        int xCurrentTimeInt64(unsigned long *pOutTime) override;


        UnixVFS(int version, int maxPathNameLength, std::string name,
                void *pAppData = nullptr);
    };

    struct UnixINode {
    private:
        std::list<int> unusedFd;
    public:
        dev_t dev;
        ino_t ino;
        pthread_mutex_t lockMutex;
        int nShared;
        int nLock;
        unsigned char eFileLock;
        unsigned char bProcessLock;
        int nRef;


        void Lock();
        void Unlock();
        void ClosePendingFds();
        void SetPendingFd(int fd);
        UnixINode(dev_t dev, ino_t ino);
        ~UnixINode();

        static UnixINode* UnixINodeFind(dev_t dev,ino_t ino);
        static void UnixInodeRelease(UnixINode *pInode);

    };


    class UnixFile : public tinySQL_file {
    private:
        static int ProcessFileLockSet(int fd, short l_type, off_t l_start, off_t l_length);
        static int SeekAndWriteFd(int fd, long offset, const void *pBuffer,long writeCount, int *piErrno);
        static int FcntlSizeHint(UnixFile *pFile, long nByte);
        static void ModeBit(UnixFile *pFile, unsigned char mask, int *pArg);
        static const char *TempFileDir();
    public:
        const int iFd;
        const std::string pathName;
        const UnixVFS *pVFS;
        UnixINode *pInode;
        unsigned char eFileLock;
        unsigned short ctrlFlags;
        int lastErrno;
        int sectorSize;
        int chunkSize;

        int xClose() override;

        int xRead(void *pBuff, long readCount, long offset) override;

        int xWrite(const void *pBuff, long writeCount, long offset) override;

        int xTruncate(long size) override;

        int xSync(int flags) override;

        int xFileSize(unsigned long *pSize) override;

        int xLock(int eFileLock) override;

        int xUnlock(int eFileLock) override;

        int xCheckReservedLock(int *pResOut) override;

        int xFileControl(int op, void *pArg) override;

        int xSectorSize() override;

        int xDeviceCharacteristics() override;

        UnixFile(std::string pathName, int fd,UnixVFS *pVFS);


    };


}
#endif //SQLITELIKE_OS_UNIX_H
