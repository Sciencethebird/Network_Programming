# include "npshell_single_proc.h"
# include "socket_utils.h" 
# include <sys/select.h>

# define DEFAULT_PORT 6666

int main(int argc, char *argv[]){

    int server_port = DEFAULT_PORT;
    if(argc == 2){
        server_port = atoi(argv[1]);
    }
    struct sockaddr_in server_addr, client_addr; 
    struct timeval timeout={0,0}; 
    int master_sock, slave_sock;

    fd_set read_fds, active_fds;

    std::vector<int> active_sockets;
    std::vector<NPClient> client_info;

    // set server connection address
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port   = htons(server_port);
    
    int nfds = getdtablesize(); // fd select table size
    
    // create and bind master listening socket
    if ( (master_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        fprintf(stderr, "server: can't open stream socket\r\n");
    if (bind(master_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
        fprintf(stderr, "server: can't bind local address\r\n");

    listen(master_sock, 5);

    FD_ZERO(&active_fds);
    FD_ZERO(&read_fds);
    FD_SET(master_sock, &active_fds);

    Single_Proc_NPShell npshell;
    
    while(1){
        
        memcpy(&read_fds, &active_fds, sizeof(read_fds));
        block_child_signal();
        if(select(nfds, &read_fds, NULL, NULL, &timeout) < 0){
            fprintf(stderr, "selec error %s\r\n", strerror(errno));
        }
        unblock_child_signal();
        if(FD_ISSET(master_sock, &read_fds)) {
            /* handle new client request */
            bzero((char *) &client_addr, sizeof(client_addr));
            unsigned int client_addr_len = sizeof(client_addr);
 
            slave_sock = accept(master_sock, (struct sockaddr *) &client_addr, &client_addr_len );
            if(slave_sock < 0){
                fprintf(stderr, "accept error\r\n");
            }
            /* new client information */
            NPClient new_client("bin:.", slave_sock, client_addr);
            client_info.push_back(new_client);

            npshell.add_client(new_client);

            /* broacast message here */
            write(slave_sock,"% ", 2);

            // store client socket
            FD_SET(slave_sock, &active_fds);
            active_sockets.push_back(slave_sock);

            //std::cout << new_client.get_addr_str() <<"\n";
        }

        for (int i = 0; i < active_sockets.size(); i++){
            
            int fd = active_sockets[i];

            if (FD_ISSET(fd, &read_fds)){
                // std::cout << "dsfd\r\n";
                npshell.load_client(fd, master_sock);

                int input_status = npshell.take_input();

                int cmd_type = npshell.process_string();
                if(cmd_type == CMD_EXTERNAL)
                    npshell.exec_args();

                if(input_status == INPUT_EOF || cmd_type == CMD_EXIT) {
                    FD_CLR(fd, &active_fds);
                    active_sockets.erase(active_sockets.begin()+i);
                    npshell.remove_client(fd);
                    //shutdown(fd, )
                    close(fd);
                    i--; /* is this correct ???????*/
                }else{
                    //std::string buf = std::to_string(input_status);
                    //send_to_sock(fd, "asdfasdf:"+buf);
                    npshell.print_head();
                    //npshell.store_client(fd);
                } 
            }
		}
        //
    }
    //sleep(6);
    return 0;
}
