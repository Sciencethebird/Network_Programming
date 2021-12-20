# include "npshell.h"
# include "socket_utils.h"

# define DEFAULT_PORT 6666

int main(int argc, char *argv[]){
    int server_port = DEFAULT_PORT;
    if(argc == 2){
        server_port = atoi(argv[1]);
    }

    struct sockaddr_in server_addr, client_addr; 

    int master_sock, slave_sock;

    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port   = htons(server_port);

    // create and bind master listening socket
    if ( (master_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        fprintf(stderr, "server: can't open stream socket\n");
    if (bind(master_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
        fprintf(stderr, "server: can't bind local address\n");
    
    listen(master_sock, 5);

    while(1){

        bzero((char *) &client_addr, sizeof(client_addr));
        unsigned int client_addr_len = sizeof(client_addr);

        slave_sock = accept(master_sock, (struct sockaddr *) &client_addr, &client_addr_len );
        if(slave_sock < 0){
            fprintf(stderr, "accept error\n");
            /* TODO: error handling */
            continue;
        }

        int pid = fork();

        if(pid == 0){ // child process

            close(master_sock);

            // map stdin and stdout to socket
            dup2(slave_sock, STDIN_FILENO);
            dup2(slave_sock, STDOUT_FILENO);
            dup2(slave_sock, STDERR_FILENO);

            // execute npserver
            NPShell npshell("bin:.");
            npshell.spin();
            exit(0);

        } else { // parent process
            close(slave_sock);
            wait(NULL);
        }
    }
    return 0;
}
