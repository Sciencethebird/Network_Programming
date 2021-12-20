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

// signal handler
void signal_handler(int signum)
{
    // collect child with signal
    //pid_t pid =  waitpid(-1, NULL, WNOHANG);
    while(waitpid(-1, NULL, WNOHANG)>0){}
}

void block_child_signal(){
    sigset_t mask;  
    int cc;  
    sigemptyset(&mask);  
    sigaddset(&mask, SIGCHLD);  
    cc = sigprocmask(SIG_BLOCK, &mask, NULL);  
}

void unblock_child_signal(){
    sigset_t mask;  
    int cc;  
    sigemptyset(&mask);  
    sigaddset(&mask, SIGCHLD);  
    cc = sigprocmask(SIG_UNBLOCK, &mask, NULL);  
}

void block_user1_signal(){
    sigset_t mask;  
    int cc;  
    sigemptyset(&mask);  
    sigaddset(&mask, SIGUSR1);  
    cc = sigprocmask(SIG_BLOCK, &mask, NULL);  
}

void unblock_user1_signal(){
    sigset_t mask;  
    int cc;  
    sigemptyset(&mask);  
    sigaddset(&mask, SIGUSR1);  
    cc = sigprocmask(SIG_UNBLOCK, &mask, NULL);  
}


void block_user2_signal(){
    sigset_t mask;  
    int cc;  
    sigemptyset(&mask);  
    sigaddset(&mask, SIGUSR2);  
    cc = sigprocmask(SIG_BLOCK, &mask, NULL);  
}

void unblock_user2_signal(){
    sigset_t mask;  
    int cc;  
    sigemptyset(&mask);  
    sigaddset(&mask, SIGUSR2);  
    cc = sigprocmask(SIG_UNBLOCK, &mask, NULL);  
}

/**************** For Server 3 *****************/

void tell_server_to_handle(){
    // child tell parent to handle msg buffer
    kill(getppid(), SIGUSR1);
}

void tell_client_to_read(int pid){
    // child tell parent to handle msg buffer
    kill(pid, SIGUSR1);
}

void tell_client_read_done(int pid){
    kill(pid, SIGUSR2);
}
//void read