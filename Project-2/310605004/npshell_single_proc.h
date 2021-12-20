#include "npshell.h"
#include "npmessages.h"
#include "user_pipes.h"
#include <sys/socket.h>
#include <map>

#define MAX_CLIENT_NUM 30
#define UPIPE_ERROR -1
#define UPIPE_NORMAL 1
#define NO_EXCEPTION -1


/*============== Class Declaration ==============*/

class Single_Proc_NPShell : public NPShell
{
public:
    Single_Proc_NPShell();

public:
    virtual int take_input();
    virtual void exec_args();
    virtual int cmd_handler();

    void print_head();
    void load_client(int, int);
    void add_client(NPClient);
    void remove_client(int);
    void store_client(int);

public:
    void login_message();
    void add_broadcast_message(std::string);
    void broadcast_buffer();
    void broadcast(std::string, int);

    int find_client_by_name(std::string);

    virtual int process_string();
    int parse_user_pipe();

private:
    //virtual void parse_redirect();

    void ignore_child_signal();
    int find_available_id();

private:
    // client info
    std::map<int, NPClient> client_map;
    int client_ids[1024];
    int curr_sockfd;
    int curr_id;
    int master_sockfd;
    int backup_stdin, backup_stdout, backup_stderr;
    // broadcast
    std::deque<std::string> broadcast_message;

    // user pipes
    user_pipes u_pipes;
    int *input_user_pipe;
    int *output_user_pipe;
    int source_user_pipe_id;
    int user_pipe_status;
};





/*============== Function Implementation ==============*/

Single_Proc_NPShell::Single_Proc_NPShell(){
    backup_stdout = dup(STDOUT_FILENO);
    backup_stderr = dup(STDERR_FILENO);

    for (int i = 0; i <= MAX_CLIENT_NUM; i++){
        client_ids[i] = 0;
    }
}

void Single_Proc_NPShell::print_head(){
    write(curr_sockfd, "% ", 2);
}

void Single_Proc_NPShell::add_client(NPClient client)
{

    int clientfd = client.get_sockfd();
    int new_id = find_available_id();
    if (new_id > 0) {
        client.set_id(new_id);
    } else {
        std::cout << "Error finding new client id\n";
    }
    client_ids[new_id] = clientfd;
    client_map[clientfd] = client;

    std::string msg = msg_wellcome();
    send(clientfd, msg.c_str(), msg.size(), 0);
    std::string enter_msg = "*** User '(no name)' entered from " + client.get_addr_str() + ". ***\r\n";
    broadcast(enter_msg, NO_EXCEPTION);
}

void Single_Proc_NPShell::load_client(int clientfd, int masterfd)
{

    //source_user_pipe_id = -1; // for remove user pipe;

    // get current client information
    master_sockfd = masterfd;
    curr_sockfd = clientfd;
    if (client_map.find(clientfd) == client_map.end())
    {
        printf("[ERROR] client does not exit!!!\n");
        exit(0);
    }
    curr_id = client_map[curr_sockfd].get_id();
    this->n_pipes = client_map[clientfd].get_n_pipe_ptr();
    
    client_map[clientfd].shell_env.load_env();
    //setenv("PATH", client_map[clientfd].get_env(), 1);

    // map socket to stds
    npdup2(clientfd, STDOUT_FILENO);
    //npdup2(clientfd, STDIN_FILENO);
    npdup2(clientfd, STDERR_FILENO);
}

void Single_Proc_NPShell::remove_client(int clientfd)
{
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    npdup2(backup_stdout, STDOUT_FILENO);
    npdup2(backup_stderr, STDERR_FILENO);
    u_pipes.remove_user(curr_id);
    client_ids[client_map[clientfd].get_id()] = 0;
    client_map.erase(clientfd);
}

void Single_Proc_NPShell::store_client(int clientfd)
{
    //ignore_child_signal(); // if you don't ignore child signal, selec gets interrupt by the child signal
    //char *curr_env = getenv("PATH");
    //client_map[clientfd].set_env(curr_env);
}

void Single_Proc_NPShell::add_broadcast_message(std::string msg)
{
    this->broadcast_message.push_back(msg);
}
void Single_Proc_NPShell::broadcast_buffer()
{
    while (broadcast_message.size())
    {
        broadcast(broadcast_message.front(), NO_EXCEPTION);
        broadcast_message.pop_front();
    }
}

void Single_Proc_NPShell::broadcast(std::string msgstr, int except_fd = -1)
{
    for (auto client : client_map)
    {
        int fd = client.second.get_sockfd();
        if (fd != except_fd)
            send(fd, msgstr.c_str(), msgstr.size(), 0);
    }
}

int Single_Proc_NPShell::find_client_by_name(std::string new_name)
{
    for (auto client : client_map)
    {
        std::string name = client.second.get_name();
        if (new_name == name)
            return client.second.get_sockfd();
    }
    return -1;
}

int Single_Proc_NPShell::find_available_id()
{
    for (int i = 1; i <= MAX_CLIENT_NUM; i++)
    {
        if (client_ids[i] == 0)
            return i;
    }
    return -1;
}
int Single_Proc_NPShell::take_input()
{
    char client_msg[MAX_CHAR];
    recv(curr_sockfd, client_msg, sizeof(client_msg), 0);
    std::string buff(client_msg);
    strcpy(input_string, buff.c_str());
    if (std::cin.eof())
        return INPUT_EOF; /// handle this later!!!!!!!!!
    else if (strlen(input_string) != 0)
        return INPUT_NORMAL;
    else
        return INPUT_EMPTY;
}

int Single_Proc_NPShell::parse_user_pipe()
{
    user_pipe_status = UPIPE_NORMAL;
    input_user_pipe = NULL;
    output_user_pipe = NULL;
    std::string buffer(input_string), input_u_id, output_u_id;
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
            if (client_ids[source_id] > 0 && source_id <= MAX_CLIENT_NUM)
            {
                input_user_pipe = u_pipes.find_pipe(source_id, curr_id);
                if (input_user_pipe == NULL)
                {
                    std::string msg = msg_upipe_pipe_dne(source_id, curr_id);
                    write(curr_sockfd, msg.c_str(), msg.size());
                    user_pipe_status = UPIPE_ERROR;
                }
                else
                {
                    //std::string m = std::to_string(input_user_pipe[0]) + " " + std::to_string(input_user_pipe[1]) + "\n";
                    //broadcast(m);
                    int source_fd = client_ids[source_id];
                    std::string curr_name = client_map[curr_sockfd].get_name();
                    std::string source_name = client_map[source_fd].get_name();
                    std::string msg = msg_upipe_recv_success(curr_name, curr_id, source_name, source_id, input_string_raw);
                    source_user_pipe_id = source_id;
                    broadcast(msg, NO_EXCEPTION);
                }
            }
            else
            { // reveiver DNE
                /// user does not exist yet
                std::string msg = msg_upipe_user_dne(source_id);
                write(curr_sockfd, msg.c_str(), msg.size());
                input_user_pipe = NULL;
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

            if (client_ids[target_id] > 0 && target_id <= MAX_CLIENT_NUM)
            { // if target user exist
                //broadcast("target valid\n");
                output_user_pipe = u_pipes.find_pipe(curr_id, target_id);
                if (output_user_pipe == NULL)
                {
                    //broadcast("new user pipe created");
                    output_user_pipe = u_pipes.new_user_pipe(curr_id, target_id);
                    int targetfd = client_ids[target_id];
                    std::string curr_name = client_map[curr_sockfd].get_name();
                    std::string target_name = client_map[targetfd].get_name();
                    std::string msg = msg_upipe_pipe_success(curr_name, curr_id, target_name, target_id, input_string_raw);
                    broadcast(msg, NO_EXCEPTION);
                }
                else
                { // old user pipe found
                    std::string msg = msg_upipe_pipe_exist(curr_id, target_id);
                    write(curr_sockfd, msg.c_str(), msg.size());
                    output_user_pipe = NULL;
                    user_pipe_status = UPIPE_ERROR;
                }
            }
            else
            {
                /// user does not exist yet
                std::string msg = msg_upipe_user_dne(target_id);
                write(curr_sockfd, msg.c_str(), msg.size());
                output_user_pipe = NULL;
                user_pipe_status = UPIPE_ERROR;
            }
        }
        //printf("output user pipe to %s\n", output_u_id.c_str());
    }

    //std::cout <<"output u pipe id: ["<< output_u_id <<"]"<< std::endl;
    //std::cout << "intput str: [" << buffer <<"]\n";
    //broadcast()
    strcpy(input_string, buffer.c_str());
    return user_pipe_status;
}

void Single_Proc_NPShell::ignore_child_signal()
{
    //signal(SIGCHLD, SIG_DFL); // you need signal handler to reap the child
    block_child_signal();
}

// internal command handler
int Single_Proc_NPShell::cmd_handler()
{
    char **parsed = args[0];
    std::string cmd(parsed[0]);
    if (cmd == "exit")
    {
        std::string msg = "*** User '" + client_map[curr_sockfd].get_name() + "' left. ***\r\n";
        broadcast(msg, curr_sockfd);
        return CMD_EXIT;
    }
    else if (cmd == "cd")
    {
        chdir(parsed[1]);
        return CMD_INTERNEL;
    }
    else if (cmd == "setenv")
    {
        client_map[curr_sockfd].shell_env.set_env(std::string(parsed[1]), std::string(parsed[2]));
        return CMD_INTERNEL;
    }
    else if (cmd == "printenv")
    {
        std::string env_value = client_map[curr_sockfd].shell_env.get_env(std::string(parsed[1]));
        if(env_value.size() > 0){
            std::string msg = env_value + "\r\n";
            send(STDOUT_FILENO, msg.c_str(), strlen(msg.c_str()), 0);
        }
        return CMD_INTERNEL;
    }
    else if (cmd == "tell")
    {
        std::string msg;
        int target_id = atoi(parsed[1]);
        int target_sockfd = client_ids[target_id];
        std::string client_name = client_map[curr_sockfd].get_name();
        if (target_sockfd > 0)
        {
            int msg_pos = input_string_raw.find(std::string(parsed[2]));
            msg = "*** " + client_name + " told you ***: " + input_string_raw.substr(msg_pos) + "\r\n";
            send(target_sockfd, msg.c_str(), msg.size(), 0);
        }
        else
        {
            msg = "*** Error: user #" + std::to_string(target_id) + " does not exist yet. ***\r\n";
            send(curr_sockfd, msg.c_str(), msg.size(), 0);
        }
        return CMD_INTERNEL;
    }
    else if (cmd == "yell")
    {
        std::size_t pos = input_string_raw.find("yell");
        std::string yell_msg = input_string_raw.substr(pos + 5);
        //if() // check empty yell
        yell_msg = "*** " + client_map[curr_sockfd].get_name() + " yelled ***: " + yell_msg + "\r\n";
        broadcast(yell_msg, NO_EXCEPTION);
        return CMD_INTERNEL;
    }
    else if (cmd == "name")
    {
        std::string new_name = std::string(parsed[1]);
        if (find_client_by_name(new_name) > 0)
        {
            std::string msg = "*** User '" + new_name + "' already exists. ***\r\n";
            send(curr_sockfd, msg.c_str(), msg.size(), 0);
        }
        else
        {
            client_map[curr_sockfd].set_name(new_name);
            std::string msg = "*** User from " + client_map[curr_sockfd].get_addr_str() + " is named '" + new_name + "'. ***\r\n";
            broadcast(msg, NO_EXCEPTION);
        }
        return CMD_INTERNEL;
    }
    else if (cmd == "who")
    {
        std::string buff = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\r\n";
        send(curr_sockfd, buff.c_str(), buff.size(), 0);
        for (int id = 1; id <= MAX_CLIENT_NUM; id++)
        {
            if (client_ids[id] > 0)
            {
                int fd = client_ids[id];
                buff = std::to_string(id) + '\t' +
                       client_map[fd].get_name() + '\t' +
                       client_map[fd].get_addr_str();
                if (fd == curr_sockfd)
                {
                    buff += "\t<-me\r\n";
                }
                else
                {
                    buff += "\r\n";
                }
                send(curr_sockfd, buff.c_str(), buff.size(), 0);
            }
        }
        return CMD_INTERNEL;
    }
    return CMD_EXTERNAL;
}

int Single_Proc_NPShell::process_string()
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

void Single_Proc_NPShell::exec_args()
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
            //block_child_signal();
            close(master_sockfd);
            //npdup2(backup_stderr, STDERR_FILENO);
            //npdup2(backup_stdout, STDOUT_FILENO);
            close(backup_stdout);
            close(backup_stderr);
            //close(curr_sockfd);
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
                else if (input_user_pipe != NULL)
                {
                    npdup2(input_user_pipe[0], STDIN_FILENO);
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
                    npdup2(output_npipe[1], fileno(shell_stdout));
                    if (pinfo.ptype == 1)
                    {
                        //saved_stderr = dup(STDERR_FILENO);
                        npdup2(output_npipe[1], fileno(shell_stderr));
                    }
                }
                else if (output_user_pipe != NULL)
                {
                    npdup2(output_user_pipe[1], fileno(shell_stdout));
                }
                else if (user_pipe_status == UPIPE_ERROR)
                {
                    int devNull = open("/dev/null", O_WRONLY);
                    npdup2(devNull, STDOUT_FILENO);
                    close(devNull);
                }
            }

            // close all numbered pipes, including other user's
            for (auto client : client_map)
            {
                client.second.get_n_pipe().close_all();
                close(client.second.get_sockfd()); // remember to close all socket
            }

            // close  all user pipe in child
            u_pipes.close_all();

            // redirect output for lat command
            if (i == (num_of_child - 1) && strlen(redirect_file) > 0)
            {
                int out = open(redirect_file, O_RDWR | O_CREAT | O_TRUNC, 0600); // ??? cat error create useless file problem?
                if (out == -1)
                    perror("opening redirect file error");
                npdup2(out, fileno(shell_stdout));
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

    if (input_user_pipe != NULL)
    {
        close(input_user_pipe[0]);
        close(input_user_pipe[1]);
        u_pipes.remove_user_pipe(source_user_pipe_id, curr_id);
    }
    //broadcast("wait\n");
    // wait for rest of the process (if no numbered pipe)
    if ((pinfo.dest <= 0) && (output_user_pipe == NULL))
    {
        for (int i = 0; i < pids.size(); i++)
        {
            waitpid(pids[i], NULL, 0); // blocking wait
        }
    }
    //broadcast("exec finished\n");
}
