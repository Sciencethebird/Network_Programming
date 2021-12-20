#include "socket_utils.h"
#include "npshell.h"
#include "npmessages.h"
#include "user_pipes.h"
#include <sys/socket.h>
#include <map>

#define UPIPE_ERROR -1
#define UPIPE_NORMAL 1

NPClientData client_data; // is it ok??
NPClientMsg client_msg;
user_fifo_pipes client_user_pipes;
NPLock msg_lock;
int curr_client_id;
int safe_to_exit;
int wait_server_response;

/*============== Class Declaration ==============*/
class Multi_Proc_NPShell : public NPShell{
public:
    Multi_Proc_NPShell(client_data_unit, const char*);
    //~Multi_Proc_NPShell();
public:
    //virtual int take_input();
    //virtual void exec_args();
    virtual int cmd_handler();
    virtual int process_string();
    virtual void exec_args();
    int parse_user_pipe();
    //virtual void spin();
public:
    int get_sockfd();
    int get_id();
    std::string get_name();
public: // signal handlers
    static void server_response(int); // signal handler need to be static
    void send_message(int, int, std::string);
    void broadcast(std::string);
    //void print_head();
private:
    int curr_id;
    int curr_sockfd;
    std::string nickname;
    std::string address;
private:
    // user pipes
    std::string input_user_pipe_name;
    std::string output_user_pipe_name;
    int source_user_pipe_id;
    int user_pipe_status;
};


/*============== Function Implementation ==============*/


// constructor
Multi_Proc_NPShell::Multi_Proc_NPShell(client_data_unit new_data, const char* env):NPShell(env){
    nickname = std::string(new_data.nickname);
    curr_id = new_data.id;
    curr_sockfd = new_data.sockfd;
    nickname = std::string(new_data.nickname);
    address = std::string(new_data.ip) +':'+ std::string(new_data.port);
    signal(SIGUSR2, server_response);
    send_to_sock(curr_sockfd, msg_wellcome());
    broadcast(msg_user_enter(address));
    //usleep(1000);
    //print_head();
}




//void Multi_Proc_NPShell::read_message(int signum){
//    std::string buff = client_msg.get_my_message(curr_client_id);
//    write(STDOUT_FILENO, buff.c_str(),  buff.size());
//    safe_to_exit = 1;
//}

void Multi_Proc_NPShell::broadcast(std::string msg){
    // write msg to shared memory
    // tell parent to handle the broadcast message using signal
    send_message(curr_id, EVERYONE, msg);
}

void Multi_Proc_NPShell::send_message(int source_id, int target_id, std::string msg){
    //msg_lock.lock();
    block_user2_signal();
    client_msg.add_msg(source_id, target_id, msg);
    tell_server_to_handle();
    unblock_user2_signal();
    wait_server_response = 1;
    while(1){
        block_user2_signal();
        if(wait_server_response == 0) {
            unblock_user2_signal();
            break;
        }
        unblock_user2_signal();
    }
}

int Multi_Proc_NPShell::parse_user_pipe()
{
    user_pipe_status = UPIPE_NORMAL;
    input_user_pipe_name = "";
    output_user_pipe_name = "";
    std::string buffer(input_string), input_u_id, output_u_id, out1 = "", out2 = "";
    int opos, endpos;
    opos = buffer.find("<");
    if (opos >= 0)
    {

        input_u_id = buffer.substr(opos + 1);
        endpos = input_u_id.find(" ");
        if (endpos > 0)
        {
            input_u_id = input_u_id.substr(0, endpos);
            buffer.erase(opos, endpos + 1);
        }
        else if (endpos == std::string::npos)
        {
            buffer.erase(buffer.begin() + opos, buffer.end()); //?????
        }
        if (endpos != 0)
        {
            int source_id = std::stoi(input_u_id);
            client_data_unit source_client = client_data.get_client_data(source_id);
            if (source_client.sockfd > 0 && source_id <= MAX_CLIENT_NUM)
            {
                input_user_pipe_name = client_user_pipes.find_pipe(source_id, curr_id);
                if (input_user_pipe_name == "")
                {
                    std::string msg = msg_upipe_pipe_dne(source_id, curr_id);
                    write(curr_sockfd, msg.c_str(), msg.size());
                    user_pipe_status = UPIPE_ERROR;
                }
                else
                {
                    //std::string m = std::to_string(input_user_pipe[0]) + " " + std::to_string(input_user_pipe[1]) + "\n";
                    //broadcast(m);
                    int source_fd = source_client.sockfd;
                    std::string curr_name = this->nickname;
                    std::string source_name = std::string(source_client.nickname);
                    out1 = msg_upipe_recv_success(curr_name, curr_id, source_name, source_id, input_string_raw);
                    source_user_pipe_id = source_id;
                }
            }
            else
            { // reveiver DNE
                /// user does not exist yet
                std::string msg = msg_upipe_user_dne(source_id);
                write(curr_sockfd, msg.c_str(), msg.size());
                input_user_pipe_name = "";
                user_pipe_status = UPIPE_ERROR;
            }
        }
        //printf("input user pipe from %s\n", input_u_id.c_str());
        //std::cout <<"input u pipe id: ["<< input_u_id <<"]"<< std::endl;
        //printf("input user pipefd (%d, %d)\n", input_user_pipe[0], input_user_pipe[1]);
    }

    opos = buffer.find(">");
    if (opos >= 0)
    {
        output_u_id = buffer.substr(opos + 1);
        endpos = output_u_id.find(" ");
        if (endpos > 0)
        {
            output_u_id = output_u_id.substr(0, endpos);
            buffer.erase(opos, endpos + 1);
        }
        else if (endpos == std::string::npos)
        {
            buffer.erase(buffer.begin() + opos, buffer.end()); //???????
        }
        if (endpos != 0)
        { // new user pipe detected

            int target_id = std::stoi(output_u_id);
            client_data_unit target_client = client_data.get_client_data(target_id);
            if (target_client.sockfd > 0 && target_id <= MAX_CLIENT_NUM)
            { // if target user exist
                //broadcast("target valid\n");
                output_user_pipe_name = client_user_pipes.find_pipe(curr_id, target_id);
                if (output_user_pipe_name == "")
                {
                    //std::cout << "new user pipe created\n";
                    output_user_pipe_name = client_user_pipes.new_user_pipe(curr_id, target_id);
                    int targetfd = target_client.sockfd;
                    std::string curr_name = this->nickname;
                    std::string target_name = std::string(target_client.nickname);
                    out2 = msg_upipe_pipe_success(curr_name, curr_id, target_name, target_id, input_string_raw);
                    //broadcast(msg);
                }
                else
                { // old user pipe found
                    std::string msg = msg_upipe_pipe_exist(curr_id, target_id);
                    write(curr_sockfd, msg.c_str(), msg.size());
                    output_user_pipe_name = "";
                    user_pipe_status = UPIPE_ERROR;
                }
            }
            else
            {
                /// user does not exist yet
                std::string msg = msg_upipe_user_dne(target_id);
                write(curr_sockfd, msg.c_str(), msg.size());
                output_user_pipe_name = "";
                user_pipe_status = UPIPE_ERROR;
            }
        }
        //printf("output user pipe to %s\n", output_u_id.c_str());
    }
    if(out1.size() > 0 || out2.size() > 0){
        broadcast(out1+out2); // I don't know why but on np consecutive broadcast sometimes drop
    }
    //std::cout <<"output u pipe id: ["<< output_u_id <<"]"<< std::endl;
    //std::cout << "intput str: [" << buffer <<"]\n";
    //broadcast()
    strcpy(input_string, buffer.c_str());
    return user_pipe_status;
}

// internal command handler
int Multi_Proc_NPShell::cmd_handler(){
    char** parsed = args[0];
    std::string cmd(parsed[0]);
    if (cmd == "exit") {

        std::string msg = "*** User '"+client_data.get_name(curr_id)+"' left. ***\r\n";

        close(curr_sockfd);
        close(STDOUT_FILENO);
        close(STDIN_FILENO);
        close(STDERR_FILENO);

        send_message(curr_id, EVERYONE, msg);
        client_user_pipes.remove_user(curr_id);
        exit(0);
    }
    else if(cmd == "cd"){
        chdir(parsed[1]);
		return CMD_INTERNEL;
    } 
    else if(cmd == "setenv"){
        //setenv(parsed[1],parsed[2],1);
        shell_env.set_env(std::string(parsed[1]), std::string(parsed[2]));
        return CMD_INTERNEL;
    } 
    else if(cmd == "printenv") {
        std::string env_value = shell_env.get_env(std::string(parsed[1]));
        if(env_value.size() > 0){
            std::string msg = env_value + "\r\n";
            send(STDOUT_FILENO, msg.c_str(), strlen(msg.c_str()), 0);
        }
        return CMD_INTERNEL;
    } else if (cmd == "tell") {
        std::string msg;
        int target_id = atoi(parsed[1]);
        std::string client_name = client_data.get_name(curr_id);
        if(client_data.check_client(target_id)==1){
            int msg_pos = input_string_raw.find(std::string(parsed[2]));
            msg = "*** " + client_name + " told you ***: "+input_string_raw.substr(msg_pos) + "\r\n";
            send_message(curr_id, target_id, msg);
        }else{
            msg = "*** Error: user #"+std::to_string(target_id)+" does not exist yet. ***\r\n";
            send_to_sock(curr_sockfd, msg);
        }
        return CMD_INTERNEL;

    } else if(cmd == "yell") {
        std::size_t pos = input_string_raw.find("yell"); 
        std::string msg = input_string_raw.substr(pos+5);
        ////if() // check empty yell
        msg = "*** "+ client_data.get_name(curr_id) +" yelled ***: " + msg + "\r\n";
        //std::cout << yell_msg << std::endl;
        send_message(curr_id, EVERYONE, msg);
        return CMD_INTERNEL;
    } else if(cmd =="name"){
        std::string new_name = std::string(parsed[1]);
        if(client_data.find_client_by_name(new_name)>0){
            std::string msg = "*** User '"+new_name+"' already exists. ***\r\n";
            send_to_sock(curr_sockfd, msg);
        }else{
            client_data.set_name(curr_id, new_name);
            this->nickname = new_name;
            std::string msg = "*** User from "+ client_data.get_addr_str(curr_id)
                            +" is named '"+ new_name +"'. ***\r\n";
            broadcast(msg);
        }
        return CMD_INTERNEL;
    } else if(cmd == "who"){
        std::string msg = client_data.who(curr_id);
        write(curr_sockfd, msg.c_str(), msg.size());
        return CMD_INTERNEL;
    }
    return CMD_EXTERNAL;
}

//int Multi_Proc_NPShell::take_input(){
//    char client_msg[MAX_CHAR];
//    recv(curr_sockfd, client_msg, sizeof(client_msg), 0);
//    std::string buff(client_msg);
//    strcpy(input_string, buff.c_str());
//    if (std::cin.eof())
//        return INPUT_EOF; /// handle this later!!!!!!!!!
//    else if (strlen(input_string) != 0)
//        return INPUT_NORMAL;
//    else
//        return INPUT_EMPTY; // not used???
//}
//
//void Multi_Proc_NPShell::print_head(){
//    write(curr_sockfd, "% ", 2);
//}
//void Multi_Proc_NPShell::spin(){
//    while (1){
//        int input_status = this->take_input();
//
//        if( this->process_string() == CMD_EXTERNAL )
//            this->exec_args();
//        if(input_status == INPUT_EOF) break;
//        print_head();
//    }
//}

void Multi_Proc_NPShell::server_response(int signum){
    wait_server_response = 0;
}
int Multi_Proc_NPShell::get_id(){
    return curr_id;
}
int Multi_Proc_NPShell::get_sockfd(){
    return curr_sockfd;
}
std::string Multi_Proc_NPShell::get_name(){
    return nickname;
}

int Multi_Proc_NPShell::process_string()
{
    // parse input string
    this->parse_newline();
    if (this->parse_user_pipe() == UPIPE_ERROR)
    {
        //return CMD_NULL;
    }
    this->parse_redirect();
    this->parse_pipe();
    this->parse_space();

    // execute the command
    int exec_mode = CMD_NULL; // no command to execute
    if (args.size() > 0)
        exec_mode = this->cmd_handler();
    if (exec_mode == CMD_INTERNEL)
    {
        n_pipes->update_dest();
    }
    return exec_mode;
}

void Multi_Proc_NPShell::exec_args()
{
    // n_pipes->debug();
    long long num_of_child = args.size(),
              num_of_pipes = num_of_child - 1;
    int opened_process = 0;

    // update destination of numbered pipes
    n_pipes->update_dest();

    // ordinary pipes
    int input_o_pipe[2], output_o_pipe[2];

    // clear pid queue
    pids.clear();

    // fork child processes
    for (int i = 0; i < num_of_child; i++)
    {

        // check input numbered pipe
        int *input_num_pipe = n_pipes->check_input_pipe();

        int *output_npipe = NULL;
        // check output numbered pipe for last argument
        if (i == (num_of_child - 1) && (pinfo.dest > 0))
        {
            // check if there's pipe with same destination
            output_npipe = n_pipes->check_output_pipe(pinfo.dest);
            // create new of there's none
            if (output_npipe == NULL)
            {
                output_npipe = n_pipes->new_pipe(pinfo.dest);
            }
        }
        // create ordinary pipe
        if (i < num_of_child)
        {
            // ordinary input pipe
            if (pipe(input_o_pipe) < 0)
            {
                fprintf(shell_stdout, "\n %dth Pipe could not be initialized\r\n", i);
                return;
            }
            if (i > 0)
            {
                // update fd table
                // replace current child's inputfd with last child's outputfd
                npdup2(output_o_pipe[0], input_o_pipe[0]);
                npdup2(output_o_pipe[1], input_o_pipe[1]);
                close(output_o_pipe[0]);
                close(output_o_pipe[1]);
                //printf("[Parent] closing ordinary output pipe %d, %d\r\n",output_o_pipe[0], output_o_pipe[1]);
            }
            // ordinary output pipe
            if (pipe(output_o_pipe) < 0)
            {
                fprintf(shell_stdout, "\n %dth Pipe could not be initialized\r\n", i);
                return;
            }
        }

        // fork and execute commands
        int pid;
        if ((pid = fork()) < 0)
        {
            perror("fork error");
            abort();
        }
        else if (pid > 0)
        {
            //printf("%dth command\n", i);
            opened_process++;

            // close input numbered pipe in parent
            if (input_num_pipe != NULL)
            {
                //sleep(1);
                //close(input_num_pipe[0]); // if you close this, bad file descripter occurs.
                //close(input_num_pipe[1]);
                //printf("[Parent] closing input numbered pipe: (%d,%d)\r\n", input_num_pipe[0], input_num_pipe[1]);
            }
            // close ordinary input pipe
            close(input_o_pipe[0]);
            close(input_o_pipe[1]);
            // can't close output yet since you need it to be the next input

            // collect returned child
            if (i > 0)
            {
                if (opened_process > MAX_PROCESS)
                {
                    // block wait oldest command
                    int cpid = waitpid(pids.front(), NULL, 0);
                    pids.pop_front();
                    opened_process--;
                }
                else
                {
                    // non-block wait
                    int cpid = waitpid(-1, NULL, WNOHANG);
                    for (int j = 0; j < pids.size(); j++)
                    {
                        if (cpid == pids[j])
                        {
                            pids.erase(pids.begin() + j);
                            opened_process--;
                            break;
                        }
                    }
                }
            }
            // store pid
            pids.push_back(pid);
        }
        else if (pid == 0)
        {
            close(curr_sockfd);
            // access argument
            char **parsed = args[i];

            // connect input ordinary pipe
            if (i > 0)
            {
                npdup2(input_o_pipe[0], STDIN_FILENO);
            }
            // connect output ordinary pipe
            if (i < num_of_pipes)
            {
                npdup2(output_o_pipe[1], STDOUT_FILENO);
            }
            // close all ordinary pipes
            //printf("[Child] closing all ordinary pipe: (%d, %d), (%d, %d)\r\n", input_o_pipe[0], input_o_pipe[1], output_o_pipe[0], output_o_pipe[1]);
            close(input_o_pipe[0]);
            close(input_o_pipe[1]);
            close(output_o_pipe[0]);
            close(output_o_pipe[1]);

            // connect input pipe to first command (if exist)
            if (i == 0)
            {
                if (input_num_pipe != NULL)
                {
                    npdup2(input_num_pipe[0], STDIN_FILENO);
                }
                else if (input_user_pipe_name.size() > 0)
                {
                    //send_to_sock(curr_sockfd, "input_user_pipe_name:"+input_user_pipe_name);
                    int fd = open_fifo_pipe(input_user_pipe_name, FIFO_READ);
                    npdup2(fd, STDIN_FILENO);
                    close(fd);
                    client_user_pipes.remove_user_pipe(source_user_pipe_id, curr_id);
                }
                else if (user_pipe_status == UPIPE_ERROR)
                {
                    int devNull = open("/dev/null", O_RDONLY);
                    npdup2(devNull, STDIN_FILENO);
                    close(devNull);
                }
            }
            // connect output pipe to last command (if exist)
            if (i == num_of_pipes)
            {
                if (output_npipe != NULL)
                {
                    npdup2(output_npipe[1], STDOUT_FILENO);
                    if (pinfo.ptype == 1)
                    {
                        //saved_stderr = dup(STDERR_FILENO);
                        npdup2(output_npipe[1], STDERR_FILENO);
                    }
                }
                else if (output_user_pipe_name.size()>0)
                {
                    int fd = open_fifo_pipe(output_user_pipe_name, FIFO_WRITE);
                    //std::cout << "output user pipefd: " << fd << std::endl;
                    npdup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                else if (user_pipe_status == UPIPE_ERROR)
                {
                    int devNull = open("/dev/null", O_WRONLY);
                    npdup2(devNull, STDOUT_FILENO);
                    close(devNull);
                }
            }

            // close all numbered pipes, including other user's
            n_pipes->close_all();
            close(curr_sockfd);
            //for (auto client : client_map)
            //{
            //    client.second.get_n_pipe().close_all();
            //    close(client.second.get_sockfd()); // remember to close all socket
            //}

            // close  all user pipe in child
            // user_pipes.close_all();

            // redirect output for last command
            if (i == (num_of_child - 1) && strlen(redirect_file) > 0)
            {
                int out = open(redirect_file, O_RDWR | O_CREAT | O_TRUNC, 0600); // ??? cat error create useless file problem?
                if (out == -1)
                    perror("opening redirect file error");
                npdup2(out, STDOUT_FILENO);
            }

            // execute the command
            if (execvp(parsed[0], parsed) < 0)
            {
                std::string buffer = "Unknown command: [" + std::string(parsed[0]) + "].\r\n";
                write(STDERR_FILENO, buffer.c_str(), buffer.size());
                //fprintf(stderr, "Unknown command: [%s].\r\n", parsed[0]);
            }
            exit(0); // in child
        }
    }
    // close last process output pipe to avoid pipe too many error
    // if you don't close this too many files will be open then pipe error
    close(output_o_pipe[0]);
    close(output_o_pipe[1]);

    //printf("[Parent] closing last output ordianry pipe: (%d, %d)\r\n", output_o_pipe[0], output_o_pipe[1]);
    // close unused numbered pipes
    n_pipes->close_unused();

    //if (input_user_pipe != NULL)
    //{
    //    user_pipes.remove_user_pipe(source_user_pipe_id, curr_id);
    //}
    //broadcast("wait\n");
    // wait for rest of the process (if no numbered pipe)
    if ((pinfo.dest <= 0) && (output_user_pipe_name == ""))
    {
        for (int i = 0; i < pids.size(); i++)
        {
            waitpid(pids[i], NULL, 0); // blocking wait
        }
    }
    //broadcast("exec finished\n");
}