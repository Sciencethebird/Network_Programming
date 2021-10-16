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

    char buff[100], n = 12;
    int pipefd[2];

    // create pipe
    if(pipe(pipefd) < 0) 
        err_sys("pipe error");
    else
        printf("read fd = %d, write fd = %d\n", pipefd[0], pipefd[1]);

    // write data into the pipe
    if(write(pipefd[1], "hello world\n", 12) != 12)
        err_sys("write error");

    // read data into buffer from the pipe
    if(read(pipefd[0], buff, 100) != n)
       err_sys("read error");

    // close(1);
    write(1, buff, n); // write to stdout
    exit(0); 
}