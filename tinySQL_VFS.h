//
// Created by user on 22-4-20.
//

#ifndef SQLITELIKE_TINYSQL_VFS_H
#define SQLITELIKE_TINYSQL_VFS_H

#include <string>
#include <list>
#include <utility>
#include "tinySQL_file.h"

namespace tinySQL {

    class tinySQL_VFS {
        static std::list<tinySQL_VFS*> list;
    protected:
        tinySQL_VFS(int version, int maxPathNameLength, std::string name,
                   void *pAppData) : iVersion(version), mxPathName(maxPathNameLength), zName(std::move(name)),
                                               pAppData(pAppData) {
            list.push_back(this);
        }

        ~tinySQL_VFS() {
            if (pAppData)
                free(pAppData);
            for(auto it = list.begin();it != list.end();it++)
                if(*it == this) {
                    list.erase(it);
                    break;
                }
        }

    public:
        const int iVersion;            /* Structure version number (currently 3) */
        const int mxPathName;          /* Maximum file pathname length */
        const std::string zName;       /* Name of this virtual file system */
        void *pAppData;          /* Pointer to application-specific data */


        virtual int xOpen(const char *zName, tinySQL_file **ppFile,
                          int flags, int *pOutFlags) = 0;

        virtual int xDelete(const char *zName) = 0;

        virtual int xAccess(const char *zName, int flags, int *pResOut) = 0;

        virtual int xFullPathname(const char *zName, int nOut, char *zOut) = 0;

        virtual void *xDlOpen(const char *zFilename) = 0;

        virtual void xDlError(int nByte, char *zErrMsg) = 0;


        virtual void xDlClose(void *) = 0;

        virtual int xRandomness(int nByte, char *pBuf) = 0;

        virtual int xSleep(int microseconds) = 0;

        virtual int xCurrentTime(double *) = 0;

        virtual int xGetLastError(int, char *) = 0;


        virtual int xCurrentTimeInt64(unsigned long *) = 0;


        static tinySQL_VFS * VFSGet(int index);
    };
}
#endif //SQLITELIKE_TINYSQL_VFS_H
