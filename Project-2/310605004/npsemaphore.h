#include <fcntl.h>          
#include <sys/stat.h>       
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifndef NPSEM_H
#define NPSEM_H

class NPLock{
public:
    NPLock(){}
    NPLock(const char* name, int init_value = 1){
        strcpy(lock_name, name);
        std::string buff = "lock name: " + std::string(lock_name) + "\n";
        write(STDOUT_FILENO, buff.c_str(), buff.size() );
        _delete_lock(); // make sure lock is clean before use
        sem_t* lock = sem_open(name, O_CREAT, 0777, init_value);  // create with init value 1, if the lock alredy exist the value does nothing
        if(SEM_FAILED == lock) perror("sem_open");
    }
    void lock(){
        sem_t* lock = _open_lock();
        sem_wait(lock);
    }
    void unlock(){
        sem_t* lock = _open_lock();
        sem_post(lock);
    }
    void remove_lock(){
        _delete_lock();
    }
private:
    sem_t* _open_lock(){
        sem_t* lock = sem_open(lock_name, O_CREAT, 0777, 1);  // create with init value 1, if the lock alredy exist the value does nothing
        if(SEM_FAILED == lock) perror("sem_open");
        return lock;
    }
    void _delete_lock(){ 
        int ret = sem_unlink(lock_name);
        //if(ret < 0) perror("sem_unlink");
    }
private:
    //sem_t* lock;
    char lock_name[20];
};

#endif // NPSEM_H