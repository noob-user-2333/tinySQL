#include "tinySQL_VFS.h"
#include "tinySQL_def.h"

char buff[1024];
constexpr char defaultPath[]="/dev/shm/test.file";







int main() {
    auto pVFS = tinySQL::tinySQL_VFS::VFSGet(0);
    tinySQL::tinySQL_file* pFile;
    pVFS->xOpen(defaultPath,&pFile,tinySQL::Open_Create | tinySQL::Open_ReadWrite,nullptr);
    pFile->xWrite("test\n",5,0);



    return 0;
}
