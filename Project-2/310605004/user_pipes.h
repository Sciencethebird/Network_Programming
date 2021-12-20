#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <utility>

#define MAX_FIFO_PIPE 1000
#define FIFO_READ 0
#define FIFO_WRITE 1
typedef std::pair<int, int> user_pipe_info;

typedef struct user_pipe_data{
    int* pipefd;
    int is_read; // for closing the user pipe after finish reading
} user_pipe_data;

class user_pipes{
public:
    user_pipes(){}

    virtual int* new_user_pipe(int source_id, int target_id){
        user_pipe_info info = {source_id, target_id};

        if ( u_pipes.find(info) == u_pipes.end() ) {
            // not found
            //user_pipe_data new_pipe;
            u_pipes[info] = new int [2];
            if (pipe(u_pipes[info]) < 0) printf("\n!!! Pipe could not be initialized !!!\n");
            //u_pipes[info] = new int [2];
            //std::cout << "in user pipe\n";
            //printf("new user pipe fd: %d, %d\n", u_pipes[info][0], u_pipes[info][1]);
            return u_pipes[info];
        } else {
            // found
            return NULL;
        }
    }
    int* find_pipe(int source_id, int target_id){
        user_pipe_info info = {source_id, target_id};
        if ( u_pipes.find(info) == u_pipes.end() ) {
            // not found
            return NULL;
        } else {
            return u_pipes[info];
        }
    }
    int remove_user_pipe(int source_id, int target_id){
        user_pipe_info info = {source_id, target_id};
        if ( u_pipes.find(info) == u_pipes.end() ) {
            // not found
            return -1;
        } else {
            close(u_pipes[info][0]);
            close(u_pipes[info][1]);
            delete [] u_pipes[info];
            auto iter = u_pipes.find(info);
            u_pipes.erase(iter);
            return 1;
        }
    }
    void remove_user(int user_id){
        for (auto it = u_pipes.cbegin(); it != u_pipes.cend() /* not hoisted */; /* no increment */)
        {
            if(it->first.first == user_id || it->first.second == user_id){
                close(it->second[0]);
                close(it->second[1]);
                delete [] it->second;
                u_pipes.erase(it++);    // or "it = m.erase(it)" since C++11
            } else {
              ++it;
            }
        }
    }
    void close_all(){
        for(auto upipe: u_pipes){
            close(upipe.second[0]);
            close(upipe.second[1]);
        }
    }
    void debug(){

    }
protected:
    std::map<user_pipe_info, int*> u_pipes;
};






struct fifo_pipe_unit {
    int from_id;
    int to_id;
    char pipe_name[20];
};


void create_fifo(std::string pipe_name){
    // remove file first?????
    remove(pipe_name.c_str()); // prevent re-entry client dirty pipe
    //if( remove( pipe_name.c_str() ) != 0 )
       // perror( "Error deleting file" );
        
    int ret = mkfifo(pipe_name.c_str(), 0666/*O_CREAT|O_RDWR|O_NONBLOCK*/);
    if(ret < 0){
        //printf("mkfifo() failed.\n");
        //printf("errno = %d\n", errno);
        if(errno==EEXIST){
            //printf("That file already exists.\n");
            //printf("(or we passed in a symbolic link, which we did not.)\n");
        }
    }
}

int open_fifo_pipe(std::string name, int mode){
    int fd;
    //printf("open file name: %s",name.c_str());// << std::endl;
    if(mode == FIFO_WRITE){
        create_fifo(name);
        //fd = open(name.c_str(), O_RDWR|O_NONBLOCK);
        //fd = open(name.c_str(), O_RDWR|O_NONBLOCK);
        fd = open(name.c_str(), O_WRONLY, 0666);
        if(fd == -1){
            //printf("open failed.\n");
            //printf("errno = %s\n", strerror(errno));
        }
    }else if(mode == FIFO_READ){
        fd = open(name.c_str(), O_RDONLY, 0666);
    }else{
        fd = -2;
        std::cout << "error mode" << std::endl;
    }
    
    return fd;
}

class user_fifo_pipes: public NPShm{
public:
    user_fifo_pipes():NPShm(){}
    user_fifo_pipes(int key, int num_of_pipes): NPShm(key, sizeof(fifo_pipe_unit), num_of_pipes+2){
        shm_lock.lock();
        fifo_pipe_unit* pipes = attach();
        for(int i = 0; i< MAX_FIFO_PIPE; i++){
            pipes[i].from_id = 0;
            pipes[i].to_id = 0;
        }
        shm_lock.unlock();
        detach();
    }
    std::string find_pipe(int fromid, int toid){
        std::string pipe_name = "";
        shm_lock.lock();
        fifo_pipe_unit* pipes = attach();
        for(int i = 0; i< MAX_FIFO_PIPE; i++){
            if(pipes[i].from_id == fromid && pipes[i].to_id == toid){
                pipe_name = std::string(pipes[i].pipe_name);
                break;
            }
        }
        detach();
        shm_lock.unlock();
        return pipe_name;
    }
    std::string new_user_pipe(int fromid, int toid){
        std::string new_name = "user_pipe/"+std::to_string(fromid)+'-'+std::to_string(toid);
        shm_lock.lock();
        fifo_pipe_unit* pipes = attach();
        for(int i = 0; i< MAX_FIFO_PIPE; i++){
            if(pipes[i].from_id == 0){
                pipes[i].from_id = fromid;
                pipes[i].to_id = toid;
                strcpy(pipes[i].pipe_name, new_name.c_str());
                break;
            }
        }
        detach();
        shm_lock.unlock();
        return new_name;
    }
    void remove_user_pipe(int fromid, int toid){
        shm_lock.lock();
        fifo_pipe_unit* pipes = attach();
        for(int i = 0; i< MAX_FIFO_PIPE; i++){
            if(pipes[i].from_id == fromid && pipes[i].to_id == toid){
                pipes[i].from_id = 0;
                pipes[i].to_id = 0;
                //strcpy(pipes[i].pipe_name, fifo_name.c_str());
                break;
            }
        }
        detach();
        shm_lock.unlock();
    }
    void remove_user(int id){
        shm_lock.lock();
        fifo_pipe_unit* pipes = attach();
        for(int i = 0; i< MAX_FIFO_PIPE; i++){
            if(pipes[i].from_id == id || pipes[i].to_id == id){
                pipes[i].from_id = 0;
                pipes[i].to_id = 0;
                int fd = open_fifo_pipe(pipes[i].pipe_name, FIFO_READ);
                close(fd);
                //strcpy(pipes[i].pipe_name, fifo_name.c_str());
                //break;
            }
        }
        detach();
        shm_lock.unlock();
    }
    fifo_pipe_unit* attach(){
        curr_ptr = (fifo_pipe_unit*) get_shm_head();
        return curr_ptr;
    }
    void detach(){
        shmdt(curr_ptr);
    }
private:
    std::vector<int*> u_pipes;
    fifo_pipe_unit* curr_ptr;
};