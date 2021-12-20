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

typedef struct npipe{
    int dest;
    int* pipefd;
} num_pipe;

typedef struct npipeinfo{
    int dest;
    int ptype;
} num_pipe_info;

class numbered_pipes{
public:
    numbered_pipes(){
        //debug();
        num_pipes.clear();
    }
 
    // return pipe which reaches the destination
    int* check_input_pipe(){
        int *src_pipefd = NULL;
        int connected = 0;
        for(int i = 0; i<num_pipes.size(); i++){
            if(num_pipes[i].dest == 0){
                connected++;
                src_pipefd = num_pipes[i].pipefd;
            }
        }
        if(connected > 1) printf("\n!!! Error: multiple input pipe detected !!!\n");
        return src_pipefd;
    }
    // find the existing pipe with same destination
    int* check_output_pipe(int dest){
        int *pipefd = NULL;
        for(int i = 0; i<num_pipes.size(); i++){
            if(num_pipes[i].dest == dest){
                pipefd = num_pipes[i].pipefd;
                break;
            }
        }
        return pipefd;
    }
    // update pipe destination
    void update_dest(){

        for(int i = 0; i<num_pipes.size(); i++){
            // update the dest 
            num_pipes[i].dest -= 1;
            // remove unused pipe
            if(num_pipes[i].dest < 0){
                //close(num_pipes[i].pipefd[0]);
                //close(num_pipes[i].pipefd[1]);
                //delete [] num_pipes[i].pipefd;
                //// remove element from vector
                //num_pipes[i] = num_pipes.back();
                //num_pipes.pop_back();
                //i--; // re-check i-th element after swap
            }
        }
    }
    // create a new numbered pipe
    int* new_pipe(int dest){
        num_pipe new_pipe;
        new_pipe.dest = dest;
        new_pipe.pipefd = new int [2];
        if (pipe(new_pipe.pipefd) < 0) printf("\n!!! Pipe could not be initialized !!!\n");
        num_pipes.push_back(new_pipe);
        return new_pipe.pipefd;
    }
    void close_all(){
        for(int i = 0; i<num_pipes.size(); i++){
            close(num_pipes[i].pipefd[0]);
            close(num_pipes[i].pipefd[1]);
        }
    }
    void close_unused(){
        for(int i = 0; i<num_pipes.size(); i++){
            if(num_pipes[i].dest <= 0){
                close(num_pipes[i].pipefd[0]);
                close(num_pipes[i].pipefd[1]);
                delete [] num_pipes[i].pipefd;
                // remove element from vector
                num_pipes[i] = num_pipes.back();
                num_pipes.pop_back();
                i--;
            }
        }
    }
    void debug(){
        std::cout << "Num of npipes: " << num_pipes.size() << std::endl;
        for(int i = 0; i< num_pipes.size(); i++){
            printf("numbered pipe fd: (%d,%d) , destination %d\n", num_pipes[i].pipefd[0], num_pipes[i].pipefd[1],num_pipes[i].dest);
        }
        std::cout << std::endl;
    }
private:
    std::deque<num_pipe> num_pipes;
};