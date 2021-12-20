#include "npclient.h"
#include "npsignal.h"
#include "nputils.h"
#include "npenv.h"
#include <sys/socket.h>

#define MAX_CHAR 60000 // max number of letters to be supported
#define MAX_ARGS 200 // max number of arguments per commands 

#define INPUT_EMPTY 1
#define INPUT_NORMAL 0
#define INPUT_EOF -1

#define CMD_EXIT -1
#define CMD_NULL 0
#define CMD_INTERNEL 1
#define CMD_EXTERNAL 2


#define MAX_PROCESS 400

typedef std::vector<char*> t_cmds;
typedef std::vector<char**> t_args;

// pid array
static std::deque<pid_t> pids;



class NPShell{
public:
    NPShell();
    NPShell(const char* env);
    ~NPShell();
public:  
    virtual int take_input();
    virtual int process_string();
    virtual void exec_args();
    virtual void spin();
    void save_env();
    void load_env();
protected:
    // command parsing & handling
    virtual void parse_newline();
    virtual void parse_redirect();
    virtual void parse_pipe();
    virtual void parse_space();
    virtual int cmd_handler();
protected:
    // input strings
    char* input_string;
    char* redirect_file;
    std::string input_string_raw;

    // parsed arguments
    t_cmds cmds;
    t_args args;
    
    // numbered pipes
    numbered_pipes* n_pipes;
    num_pipe_info pinfo;

    // io
    FILE* shell_stdin;
    FILE* shell_stdout;
    FILE* shell_stderr;

    NPEnv shell_env;
};

//********** Function Implementation **********//
NPShell::NPShell(){
    //std::cout << "Base NPShell Constructor Called\n"; 
    // signal handler
    signal(SIGCLD, signal_handler);
    // init string buffer
    this->input_string = new char [MAX_CHAR];
    this->redirect_file = new char [MAX_CHAR];
    // map I/O
    this->shell_stdin = stdin;
    this->shell_stdout = stdout;
    this->shell_stderr = stderr;
}

NPShell::NPShell(const char* env){

    //** TODO: env variable manager **//

    // setenv
    //setenv("PATH", env, 1);
    shell_env.set_env("PATH", std::string(env));
    // signal handler
    signal(SIGCLD, signal_handler);
    // init string buffer
    this->input_string = new char [MAX_CHAR];
    this->redirect_file = new char [MAX_CHAR];
    // new numbered pipe
    n_pipes = new numbered_pipes;
    // map I/O
    this->shell_stdin = stdin;
    this->shell_stdout = stdout;
    this->shell_stderr = stderr;
}

//********** Function Implementation **********//
NPShell::~NPShell(){
    delete [] this->input_string;
    delete [] this->redirect_file;
    delete [] this->n_pipes;
}

void NPShell::exec_args(){
    //n_pipes->debug();
    long long num_of_child = args.size(),
              num_of_pipes = num_of_child -1;
    int opened_process = 0;

    // update destination of numbered pipes
    n_pipes->update_dest();

    // ordinary pipes
    int input_o_pipe[2], output_o_pipe[2];

    // clear pid queue
    pids.clear();

    // fork child processes
    for(int i = 0; i < num_of_child; i++){

        // check input numbered pipe
        int* input_num_pipe = n_pipes->check_input_pipe();
        
        int* output_npipe = NULL;
        // check output numbered pipe for last argument
        if( i == (num_of_child-1) && ( pinfo.dest > 0) ) {
            // check if there's pipe with same destination
            output_npipe = n_pipes->check_output_pipe(pinfo.dest);
            // create new of there's none
            if(output_npipe == NULL){
                output_npipe = n_pipes->new_pipe(pinfo.dest);
            }
        }
           
        // create ordinary pipe 
        if (i <num_of_child){
            // ordinary input pipe
            if(pipe(input_o_pipe) < 0) {
		        fprintf(shell_stdout, "\n %dth Pipe could not be initialized", i);
		        return;
            }
            if(i>0){
                // update fd table
                // replace current child's inputfd with last child's outputfd
                dup2(output_o_pipe[0], input_o_pipe[0]);
                dup2(output_o_pipe[1], input_o_pipe[1]);
                close(output_o_pipe[0]);
                close(output_o_pipe[1]);
                //printf("[Parent] closing ordinary output pipe %d, %d\n",output_o_pipe[0], output_o_pipe[1]);
            }
            // ordinary output pipe
            if(pipe(output_o_pipe) < 0) {
		        fprintf(shell_stdout,"\n %dth Pipe could not be initialized", i);
		        return;
            }
        }
        
        // fork and execute commands
        int pid;
        if ( (pid = fork()) < 0 ) {
            perror("fork error");
            abort();
        } else if( pid > 0 ){
            //printf("%dth command\n", i);
            opened_process++;

            // close input numbered pipe in parent
            if(input_num_pipe != NULL){
                //close(input_num_pipe[0]);
                //close(input_num_pipe[1]);
                //printf("[Parent] closing input numbered pipe: (%d,%d)\n", input_num_pipe[0], input_num_pipe[1]);
            }
            // close ordinary input pipe
            close(input_o_pipe[0]);
            close(input_o_pipe[1]);
            // can't close output yet since you need it to be the next input

            // collect returned child
            if(i>0){
                if(opened_process > MAX_PROCESS) {
                    // block wait oldest command
                    int cpid = waitpid(pids.front(), NULL, 0);
                    pids.pop_front();
                    opened_process--;
                }else{
                    // non-block wait
                    int cpid = waitpid(-1, NULL, WNOHANG);
                    for(int j = 0 ; j< pids.size(); j++){
                        if(cpid == pids[j]){
                            pids.erase(pids.begin()+j);
                            opened_process--;
                            break;
                        }
                    }
                }
            }
            // store pid
            pids.push_back(pid);

        } else if ( pid == 0 ) {  
            block_child_signal();
            // access argument
            char** parsed = args[i];

            // connect input ordinary pipe
            if(i > 0) {
                dup2(input_o_pipe[0], STDIN_FILENO);
            }
            // connect output ordinary pipe
            if(i < num_of_pipes){
                dup2(output_o_pipe[1], STDOUT_FILENO);
            }
            // close all ordinary pipes
            //printf("[Child] closing all ordinary pipe: (%d, %d), (%d, %d)\n", input_o_pipe[0], input_o_pipe[1], output_o_pipe[0], output_o_pipe[1]);
            close(input_o_pipe[0]);
            close(input_o_pipe[1]);
            close(output_o_pipe[0]);
            close(output_o_pipe[1]);

            // connect input numbered pipe (if exist)
            if(i == 0 && input_num_pipe != NULL){
                //std::cout << "input num pipe: " << input_num_pipe[0] << std::endl;
                dup2(input_num_pipe[0], STDIN_FILENO);
            }
		    // connect output numbered pipe (if exist)
            //int saved_stderr = -1;
            if(i == num_of_pipes && output_npipe != NULL){
                dup2(output_npipe[1], fileno(shell_stdout));
                if(pinfo.ptype == 1) {
                    //saved_stderr = dup(STDERR_FILENO);
                    dup2(output_npipe[1], fileno(shell_stderr));
                }
            }
            // close unused numbered pipes
            n_pipes->close_all();
                   
            // redirect output for lat command
            if (i == (num_of_child -1) && strlen(redirect_file) > 0){
                int out = open(redirect_file, O_RDWR|O_CREAT|O_TRUNC, 0600); // ??? cat error create useless file problem?
                if (out == -1) perror("opening redirect file error"); 
                if (dup2(out, fileno(shell_stdout)) == -1) perror("cannot redirect stdout");
            }

            // execute the command
            if (execvp(parsed[0], parsed) < 0){
                // restore for piped command
                //if(saved_stderr > 0){
                //    dup2(saved_stderr, STDERR_FILENO);
                //    close(saved_stderr);
                //}
                fprintf(stderr, "Unknown command: [%s].\n", parsed[0]);
            }
            exit(0); // for execution fail
        }
    }
    // close last process output pipe to avoid pipe too many error
    // if you don't close this too many files will be open then pipe error
    close(output_o_pipe[0]);
    close(output_o_pipe[1]);
    //printf("[Parent] closing last output ordianry pipe: (%d, %d)\n", output_o_pipe[0], output_o_pipe[1]);
    // close unused numbered pipes
    n_pipes->close_unused();

    // wait for rest of the process (if no numbered pipe)
    if(pinfo.dest<=0){ 
        for(int i = 0; i< pids.size(); i++){
            waitpid(pids[i], NULL, 0); // blocking wait
        }
    }   
}

//// input function
//int NPShell::take_input()
//{
//    std::string buff;
//    write(fileno(shell_stdout),"% ", sizeof("% "));
//    std::getline(std::cin, buff); 
//    strcpy(input_string, buff.c_str());
//    if (std::cin.eof()) return INPUT_EOF;
//    else if ( strlen(input_string) != 0 ) return INPUT_NORMAL;
//    else return INPUT_EMPTY;
//}

int NPShell::take_input(){
    char client_msg[MAX_CHAR];
    send(fileno(shell_stdout),"% ", 2, 0); // don't use sizeof("% "), the value is 3
    recv(STDIN_FILENO, client_msg, sizeof(client_msg), 0);
    std::string buff(client_msg);
    strcpy(input_string, buff.c_str());
    if (std::cin.eof()) return INPUT_EOF; /// handle this later!!!!!!!!!
    else if ( strlen(input_string) != 0 ) return INPUT_NORMAL;
    else return INPUT_EMPTY;
}

// internal command handler
int NPShell::cmd_handler(){
    char** parsed = args[0];
    std::string cmd(parsed[0]);
    if (cmd == "exit") {
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
    }
    return CMD_EXTERNAL;
}

void NPShell::parse_newline(){
    /*** split the original input string by '>' ***/
    char* str = this->input_string; // avoid modfication of the original pointer
    strsep(&str, "\n"); // split original string by '\n'
    str = this->input_string;
    strsep(&str, "\r"); // split original string by '\r'
    input_string_raw = std::string(this->input_string);
}


void NPShell::parse_redirect(){
    /*** split the original input string by '>' ***/
    char* str = this->input_string; // avoid modfication of the original pointer
    char* temp_file_name;
    strcpy(redirect_file, ""); // clear redirect file
    strsep(&str, ">"); // split original string by '>'
    if(str != NULL){
        while(1){ // remove spaces in filename
		    temp_file_name = strsep(&str, " ");
            if (strlen(temp_file_name) != 0){
                strcpy(redirect_file, temp_file_name);
                break;
            }
        }
    }
}

// parse pipe and seperate input string into commands
void NPShell::parse_pipe()
{
    char* str = input_string; // make a copy of the orignal pointer first

    int num_pipe_dest = -1, pipe_type = -1;
    char* head = strsep(&str, "!");
    char* token = strsep(&str, "!");
    if(token != NULL) {
        num_pipe_dest = atoi(token);
        pipe_type = 1;
    }
    str = head;
    cmds.clear();

    while( (token = strsep(&str, "|")) != NULL)
        cmds.push_back(token);

    if( is_number_str(std::string(cmds.back())) ){
        num_pipe_dest = atoi(cmds.back());
        cmds.pop_back();
        pipe_type = 0;
    }
    pinfo.dest = num_pipe_dest;
    pinfo.ptype = pipe_type;
}

void NPShell::parse_space(){

    // free token pointer array
    for(int i = 0; i< args.size(); i++) delete [] args[i];

    args.clear();

    // parse command str into args
    for(int i = 0; i < cmds.size(); i++){
        char** tokens = new char*[MAX_ARGS];
        char*token;
        int token_size = 0;
        // split command string by space 
        for (int j = 0; j < MAX_ARGS; j++) {
		    tokens[j] = strsep(&cmds[i], " ");
		    if (tokens[j] == NULL){
                token_size = j+1;
                break;
            }
		    if (strlen(tokens[j]) == 0) j--;
	    }
        // push only when args is more than just a NULL
        if(token_size > 1) args.push_back(tokens);
    }
}

int NPShell::process_string(){
    // parse input string
    this->parse_newline();
    this->parse_redirect(); 
    this->parse_pipe(); 
    this->parse_space();

    // execute the command
    int exec_mode = CMD_NULL; // no command to execute
    if(args.size() > 0)
        exec_mode = this->cmd_handler();
    if(exec_mode == CMD_INTERNEL){
        n_pipes->update_dest();
    }
    return exec_mode;
}

void NPShell::spin(){
    while (1){
        int input_status = this->take_input();
        if(input_status == INPUT_EMPTY) continue;

        if( this->process_string() == CMD_EXTERNAL )
            this->exec_args();
        if(input_status == INPUT_EOF) break;
    }
}





