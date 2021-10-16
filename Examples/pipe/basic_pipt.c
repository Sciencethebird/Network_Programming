# include <sys/types.h>
# include <unistd.h>
# include <stdio.h>
# include <stdlib.h>

void err_sys(const char* x) 
{ 
    perror(x); 
    exit(1); 
}

int main(){
    int pipefd[2], n = 12;
    char buff[100];
    if(pipe(pipefd) < 0) err_sys("pipe error");
    printf("read fd = %d, write fd = %d\n", pipefd[0], pipefd[1]);
    if(write(pipefd[1], "hello world\n", 12) != 12)
        err_sys("write error");
    
    if(read(pipefd[0], buff, 100) != n)
       err_sys("read error");

    write(1, buff, n);
    exit(0); 
}