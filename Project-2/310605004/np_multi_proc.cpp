# include "np_multi_proc.h"

# define DEFAULT_PORT 6666

struct active_client_info
{
    int id;
    int pid;
    int sockfd;
};

std::vector<active_client_info> active_client;

void broadcast(std::string msg){
    for(int i = 0; i< active_client.size(); i++){
        send_to_sock(active_client[i].sockfd, msg);
    }
}

void npshell_reaper(int signum){
    // SIGCLD
    block_user1_signal();
    printf("child exit signal recieved\n");
    int pid = wait(NULL);
    for(int i = 0 ; i< active_client.size(); i++){
        if(active_client[i].pid == pid){
            printf("Closing client, pid: %d, sockfd: %d\n", active_client[i].pid, active_client[i].sockfd);
            client_data.remove_client(active_client[i].id);
            close(active_client[i].sockfd);
            active_client.erase(active_client.cbegin()+i);
            /*client client data shared memory*/
            break; // assume only one client leave at a time
        }
    }
    unblock_user1_signal();
}
void server_exit(int signum){
    // SIGINT
    client_data.release();
    client_msg.release();
    client_user_pipes.release();
    shm_lock.remove_lock();
    exit(0);
}

void message_handler(int signum){
    // SIGUSR1
    block_child_signal();// prevent child exit interruption
    std::string buff("New message incomming\n");
    write(STDOUT_FILENO, buff.c_str(), buff.size());

    client_msg.print_all();

    shm_lock.lock();

    client_msg_unit* msgs = client_msg.attach();
    for(int i = 0; i < MAX_MESSAGE_QUEUE; i++){
        int target_id = msgs[i].to_id;
        std::cout << "send to " << target_id << std::endl;
        if(target_id >0){
            std::cout << "send to " << target_id << std::endl;
            int target_sockfd = client_data.get_sockfd(msgs[i].to_id);
            send_to_sock(target_sockfd, msgs[i].msg);
        }else if(target_id == EVERYONE){
            std::cout << "send to " << target_id << std::endl;
            broadcast(std::string(msgs[i].msg));
        }
    }
    for(int i = 0; i <active_client.size(); i++){
        tell_client_read_done(active_client[i].pid);
    }
    client_msg.detach();

    shm_lock.unlock();
    //msg_lock.unlock();
    client_msg.clear_all();
    //client_msg.print_all();
    
    unblock_child_signal();
}


int main(int argc, char *argv[]){

    int server_port = DEFAULT_PORT;
    if(argc == 2){
        server_port = atoi(argv[1]);
    }

    struct sockaddr_in server_addr, client_addr; 

    int master_sock, client_sock;
    int new_id;

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

    // signal handlers
    signal(SIGCLD, npshell_reaper);
    signal(SIGINT, server_exit); // ctrl-c
    signal(SIGUSR1, message_handler); // message signal from child

    // semaphore for shared memory
    shm_lock = NPLock("shm_lock"); 
    msg_lock = NPLock("msg_lock"); 
    // shared memory
    client_data = NPClientData(777, MAX_CLIENT_NUM+1); // client data
    client_msg = NPClientMsg(888, MAX_MESSAGE_QUEUE);  // message buffer
    client_user_pipes = user_fifo_pipes(999, MAX_FIFO_PIPE);  // FIFO user pipes

    while(1){

        bzero((char *) &client_addr, sizeof(client_addr));
        unsigned int client_addr_len = sizeof(client_addr);

        client_sock = accept(master_sock, (struct sockaddr *) &client_addr, &client_addr_len );
        if(client_sock < 0){
            fprintf(stderr, "accept error\n");
            /* TODO: error handling */
            continue;
        } 

        curr_client_id = client_data.find_and_set_available_id(client_sock); // the id array is in parent

        int pid = fork();

        if(pid == 0){ // child process

            client_data.add_client(curr_client_id, client_sock, client_addr);

            close(master_sock);
            // close previous client's socket
            for(int i = 0; i<active_client.size(); i++){
                close(active_client[i].sockfd); // avoid attach to other client's socket
            }

            // map stdin and stdout to socket
            dup2(client_sock, STDIN_FILENO);
            dup2(client_sock, STDOUT_FILENO);
            dup2(client_sock, STDERR_FILENO);
            // execute npserver
            Multi_Proc_NPShell npshell(client_data.get_client_data(curr_client_id), "bin:.");

            npshell.spin();
            
            exit(0);

        } else { // parent process
            active_client.push_back({curr_client_id, pid, client_sock});
        }
    }
    return 0;
}
