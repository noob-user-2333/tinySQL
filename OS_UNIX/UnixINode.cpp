//
// Created by user on 22-4-27.
//
#include "OS_unix.h"

namespace tinySQL{
    struct UnixInodeList{
        std::list<UnixINode> list;
        pthread_mutex_t mutex;

        UnixInodeList(): mutex(),list(){
            pthread_mutex_init(&mutex, nullptr);
        }
        ~UnixInodeList() {
            pthread_mutex_destroy(& mutex);
        }
    };
    static UnixInodeList inodeList;


    UnixINode::UnixINode(dev_t dev, ino_t ino) : dev(dev), ino(ino),nLock(0),unusedFd(),
    nShared(0),nRef(0), bProcessLock(0), lockMutex(),eFileLock(Lock_None) {
        pthread_mutex_init(&lockMutex, nullptr);
    }

    UnixINode::~UnixINode() {
        ClosePendingFds();
        pthread_mutex_unlock(&lockMutex);
        pthread_mutex_destroy(&lockMutex);
    }

    void UnixINode::Lock() {
        pthread_mutex_lock(& lockMutex);
    }

    void UnixINode::Unlock() {
        pthread_mutex_unlock(& lockMutex);
    }

    UnixINode *UnixINode::UnixINodeFind(dev_t dev, ino_t ino) {
        UnixINode * pInode = nullptr;
        pthread_mutex_lock(&inodeList.mutex);
        for(auto & it : inodeList.list)
            if(it.ino == ino && it.dev == dev) {
                pInode = &it;
                break;
            }
        if(pInode == nullptr){
            inodeList.list.emplace_front(dev,ino);
            pInode = &inodeList.list.front();
        }
        pthread_mutex_unlock(& inodeList.mutex);
        return pInode;
    }

    void UnixINode::UnixInodeRelease(UnixINode *pInode) {
        assert(pInode != nullptr);

        pthread_mutex_lock(&inodeList.mutex);
        pInode->Lock();
        pInode->nRef--;
        if(pInode->nRef == 0){
            for(auto it = inodeList.list.begin(); it != inodeList.list.end();it++)
                if(&(*it) == pInode){
                    inodeList.list.erase(it);
                    pthread_mutex_unlock(&inodeList.mutex);
                    return;
                }
            throw std::runtime_error("can not find the inode in list");
        }
        pInode->Unlock();
    }

    void UnixINode::ClosePendingFds() {
        for(auto value : unusedFd)
            close(value);
        unusedFd.clear();
    }

    void UnixINode::SetPendingFd(int fd) {
        unusedFd.emplace_front(fd);
    }


}