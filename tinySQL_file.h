//
// Created by user on 22-4-20.
//

#ifndef SQLITELIKE_TINYSQL_FILE_H
#define SQLITELIKE_TINYSQL_FILE_H
namespace tinySQL {



    class tinySQL_file {
    public:
        virtual int xClose() = 0;

        virtual int xRead(void * pBuff, long readCount, long offset) = 0;

        virtual int xWrite(const void * pBuff,long writeCount, long offset) = 0;

        virtual int xTruncate(long size) = 0;

        virtual int xSync(int flags) = 0;

        virtual int xFileSize(unsigned long *pSize) = 0;

        virtual int xLock(int eFileLock) = 0;

        virtual int xUnlock(int eFileLock) = 0;

        virtual int xCheckReservedLock(int *pResOut) = 0;

        virtual int xFileControl(int op, void *pArg) = 0;

        virtual int xSectorSize() = 0;

        virtual int xDeviceCharacteristics() = 0;


        virtual ~tinySQL_file()= default;
    };
}
#endif //SQLITELIKE_TINYSQL_FILE_H
