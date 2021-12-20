#ifndef NPSHM_H
#define NPSHM_H

// header body...


#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "npsemaphore.h"

NPLock shm_lock;
class NPShm{
public:
    NPShm(){};
    NPShm(int key, int data_size, int unit_num){
        this->key = key;
        this->unit_data_size = data_size;
        this->num_of_units = unit_num;
        this->shm_size = num_of_units * unit_data_size;
        std::string buff("shm_size:");
        buff = buff + std::to_string(shm_size) + '\n';
        write(STDOUT_FILENO, buff.c_str(), buff.size());
        shm_id =shmget(this->key, this->shm_size, 0666|IPC_CREAT);
    }
    //~NPShm();
public: 
    void* get_shm_head(){
        //int shm_id = shmget(key, 5, 0666); does shared memory id  change?
        void *p = shmat(shm_id, NULL, 0);
        //shmdt(p); p automatically detach when process exit, need to detach by myself??
        return p;
    }
    void release(){
        shmctl(shm_id, IPC_RMID, 0);
    }
protected:
    key_t key;
    int shm_id;
    int unit_data_size;
    int num_of_units;
    int shm_size;
};


#endif //NPSHM_H