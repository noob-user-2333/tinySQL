//
// Created by user on 22-4-27.
//

#include <utility>

#include "OS_unix.h"

namespace tinySQL {
    std::list<tinySQL_VFS*> tinySQL_VFS::list;

    static int RobustOpen(const char * zName,int flags,int mode){
        int mode2 = mode ? mode : DefaultFilePermission;
        int fd = 0;
        while(1){
            fd = OsOpen(zName,flags | O_CLOEXEC,mode2);
            //when error happened because of interrupt,retry it
            if(fd < 0){
                if(errno == EINTR)
                    continue;
                break;
            }
            if(fd >= MinFileDescriptor)
                break;
            OsClose(fd);
            fd = -1;
            //if it can not open /dev/null,break
            //why open /dev/null?
            //it is reserved for fd of stdin,stdout and stderr
            //so only go into the branch when fd < 3
            if(OsOpen("/dev/null",O_RDONLY,0) < 0)
                break;
        }

        if(fd > 0 && mode){
            struct stat buf{};
            //chang mode only when created(st.size == 0)
            if(OsFstat(fd,&buf) == 0 &&
            buf.st_size == 0 &&
            buf.st_mode != mode)
                OsFchmod(fd,mode);
        }
        return fd;
    }

    tinySQL_VFS *tinySQL_VFS::VFSGet(int index) {
        static UnixVFS unixVFS(1, 256, "unixVFS");
        auto it = list.begin();
        for(;index > 0 && it != list.end();index--)
            it++;
        if(it != list.end())
            return *it;
        return nullptr;
    }


    int UnixVFS::xOpen(const char *zName, tinySQL_file **ppFile, int flags, int *pOutFlags) {
        assert(ppFile && zName);
        int fd = -1;
        int status = 0;
        int eType = flags & 0xFFF00;
        int ctrlFlags = 0;
        int openFlag = 0;
        char zTempName[mxPathName + 2];

        bool isExclusive = flags & Open_Exclusive;
        bool isDelete = flags & Open_Delete;
        bool isCreate = flags & Open_Create;
        bool isReadOnly = flags & Open_ReadOnly;
        bool isReadWrite = flags & Open_ReadWrite;
        bool isNewJournal = isCreate && ( eType == Open_MainJournal
                || eType == Open_SuperJournal || eType == Open_MainWAL);

//        if(zName == nullptr){
//            assert(isDelete && !isNewJournal);
//            status = x
//        }

        if(isReadOnly) openFlag |= O_RDONLY;
        if(isReadWrite) openFlag |= O_RDWR;
        if(isCreate) openFlag |= O_CREAT;
        if(isExclusive) openFlag |= O_EXCL;

        fd = RobustOpen(zName,openFlag,0);
        if(fd < 0)
            return CanNotOpen;
        if(pOutFlags)
            *pOutFlags = flags;
        *ppFile = new UnixFile(zName,fd,this);
        return status;
    }

    int UnixVFS::xDelete(const char *zName) {
        return OsUnlink(zName)  ? IOError_Delete : Succeed;
    }

    int UnixVFS::xAccess(const char *zName, int flags, int *pResOut) {
        assert(pResOut);
        int eAccess = F_OK;
        assert(flags == Access_Exists ||
               flags == Access_Read ||
               flags == Access_ReadWrite);

        if (flags == Access_Read) eAccess = R_OK;
        if (flags == Access_ReadWrite) eAccess = R_OK | W_OK;

        *pResOut = (OsAccess(zName, eAccess) == 0);
        return Succeed;
    }

    int UnixVFS::xFullPathname(const char *zName, int nOut, char *zOut) {
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

    void *UnixVFS::xDlOpen(const char *zFilename) {
        throw std::runtime_error("no support");
    }

    void UnixVFS::xDlError(int nByte, char *zErrMsg) {
        throw std::runtime_error("no support");
    }

    void UnixVFS::xDlClose(void *) {
        throw std::runtime_error("no support");
    }

    int UnixVFS::xRandomness(int nByte, char *pBuf) {
        memset(pBuf,0,nByte);
        int fd = RobustOpen("/dev/urandom",O_RDONLY,0);
        if(fd < 0){
            //just copy time and pid to pBuf
            time_t t = time(nullptr);
            pid_t pid = getpid();
            assert(nByte >= sizeof(pid) + sizeof(t));
            memcpy(pBuf,&t, sizeof(t));
            memcpy(&pBuf[sizeof(t)],&pid, sizeof(pid));
            nByte = sizeof(pid) + sizeof(t);
        }else{
            //get random num by read /dev/urandom
            long got;
            do{
                got = OsRead(fd,pBuf,nByte);
            }while(got < 0 && errno == EINTR);
            OsClose(fd);
        }
        return nByte;
    }

    int UnixVFS::xSleep(int microseconds) {
        sleep(microseconds / 1000000);
        usleep(microseconds % 1000000);
        return microseconds;
    }

    int UnixVFS::xCurrentTime(double *pTime) {
        unsigned long time;
        int status = xCurrentTimeInt64(&time);
        *pTime = time;
        return status;
    }

    int UnixVFS::xGetLastError(int, char *) {
        return errno;
    }

    int UnixVFS::xCurrentTimeInt64(unsigned long *pOutTime) {
        *pOutTime = time(nullptr);
        return Succeed;
    }

    UnixVFS::UnixVFS(int version, int maxPathNameLength, std::string name, void *pAppData) : tinySQL_VFS(version,
                                                                                                         maxPathNameLength,
                                                                                                         std::move(name),
                                                                                                         pAppData) {

    }
}