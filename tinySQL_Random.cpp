//
// Created by user on 22-4-29.
//
#include <pthread.h>
#include <cstring>
#include <cassert>
#include "tinySQL_VFS.h"
namespace tinySQL{
    struct RandomGenerator{
        pthread_mutex_t mutex;
        unsigned char i,j;
        unsigned char s[256];

        RandomGenerator(): mutex(),s(),i(0),j(0){
            pthread_mutex_init(& mutex, nullptr);
            tinySQL_VFS * pVFS = tinySQL_VFS::VFSGet(0);
            char k[256];
            unsigned char t;
            if(pVFS)
                pVFS->xRandomness(256,k);
            else
                memset(k,0,256);
            for (int index = 0;index < 256;index++)
                s[index] = static_cast<unsigned char>(index);
            for(int index = 0; index < 256; index++){
                j += s[index] + k[index];
                t = s[j];
                s[j] = s[i];
                s[i] = t;
            }
        }
        ~RandomGenerator(){
            pthread_mutex_destroy(& mutex);
        }
    };

    void tinySQL_Randomness(int nByte,void *pBuf){
        static RandomGenerator random;
        assert(nByte > 0 && pBuf);
        auto zBuf = static_cast< unsigned char*>(pBuf);
        unsigned char t;
        pthread_mutex_lock(&random.mutex);
        for(;nByte;nByte--){
            random.i++;
            t = random.s[random.i];
            random.j += t;
            random.s[random.i] = random.s[random.j];
            random.s[random.j] = t;
            t += random.s[random.i];
            *(zBuf++) = random.s[t];
        }
        pthread_mutex_unlock(&random.mutex);
    }


}