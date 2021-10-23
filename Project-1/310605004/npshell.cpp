#include "numbered_pipes.h"

#define MAX_CHAR 60000 // max number of letters to be supported
#define MAX_ARGS 200 // max number of arguments per commands 

#define INPUT_EMPTY 1
#define INPUT_NORMAL 0
#define INPUT_EOF -1

#define CMD_NULL -1
#define CMD_INTERNEL 0
#define CMD_EXTERNAL 1

#define MAX_PROCESS 400

typedef std::vector<char*> t_cmds;
typedef std::vector<char**> t_args;

// pid array
static std::deque<pid_t> pids;

// signal handler
void signal_handler(int signum)
{
    // collect child with signal
    pid_t pid =  waitpid(-1, NULL, WNOHANG);
}

// check if argument is number
bool is_number_str(const std::string& s)
{
    int digit_count = 0;
    for(int i = 0; i< s.size(); i++){
        if(std::isdigit(s[i])) digit_count++;
        else if(s[i] != ' ') return false;
    }
    if(digit_count>0) return true;
    else return false;
}

// init shell
void init_shell()
{   
    setenv("PATH","bin:.",1);
}

void exec_args(t_args* args, char* redirect_file, numbered_pipes* n_pipes, num_pipe_info* pinfo){

    long long num_of_child = args->size(),
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
        if( i == (num_of_child-1) && ( pinfo->dest > 0) ) {
            // check if there's pipe with same destination
            output_npipe = n_pipes->check_output_pipe(pinfo->dest);
            // create new of there's none
            if(output_npipe == NULL){
                output_npipe = n_pipes->new_pipe(pinfo->dest);
            }
        }
           
        // create ordinary pipe 
        if (i <num_of_child){
            // ordinary input pipe
            if(pipe(input_o_pipe) < 0) {
		        printf("\n %dth Pipe could not be initialized", i);
		        return;
            }
            if(i>0){
                // update fd table
                // replace current child's inputfd with last child's outputfd
                dup2(output_o_pipe[0], input_o_pipe[0]);
                dup2(output_o_pipe[1], input_o_pipe[1]);
                close(output_o_pipe[0]);
                close(output_o_pipe[1]);
            }
            // ordinary output pipe
            if(pipe(output_o_pipe) < 0) {
		        printf("\n %dth Pipe could not be initialized", i);
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
                close(input_num_pipe[1]);
                close(input_num_pipe[1]);
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

            // access argument
            char** parsed = (*args)[i];

            // connect input ordinary pipe
            if(i > 0) {
                dup2(input_o_pipe[0], STDIN_FILENO);
            }
            // connect output ordinary pipe
            if(i < num_of_pipes){
                dup2(output_o_pipe[1], STDOUT_FILENO);
            }
            // close all ordinary pipes
            close(input_o_pipe[0]);
            close(input_o_pipe[1]);
            close(output_o_pipe[0]);
            close(output_o_pipe[1]);

            // connect input numbered pipe (if exist)
            if(i == 0 && input_num_pipe != NULL){
                dup2(input_num_pipe[0], STDIN_FILENO);
            }
		    // connect output numbered pipe (if exist)
            //int saved_stderr = -1;
            if(i == num_of_pipes && output_npipe != NULL){
                dup2(output_npipe[1], STDOUT_FILENO);
                if(pinfo->ptype == 1) {
                    //saved_stderr = dup(STDERR_FILENO);
                    dup2(output_npipe[1], STDERR_FILENO);
                }
            }
            // close unused numbered pipes
            n_pipes->close_all();
                   
            // redirect output for lat command
            if (i == (num_of_child -1) && strlen(redirect_file) > 0){
                int out = open(redirect_file, O_RDWR|O_CREAT|O_TRUNC, 0600); // ??? cat error create useless file problem?
                if (out == -1) perror("opening redirect file error"); 
                if (dup2(out, STDOUT_FILENO) == -1) perror("cannot redirect stdout");
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
    
    // close unused numbered pipes
    n_pipes->close_unused();

    // wait for rest of the process (if no numbered pipe)
    if(pinfo->dest<=0){ 
        for(int i = 0; i< pids.size(); i++){
            waitpid(pids[i], NULL, 0); // blocking wait
        }
    }   
}

// input function
int take_input(char* str)
{
    std::string buff;
    printf("%% ");
    std::getline(std::cin, buff);
    strcpy(str, buff.c_str());
    if (std::cin.eof()) return INPUT_EOF;
    else if ( strlen(str) != 0 ) return INPUT_NORMAL;
    else return INPUT_EMPTY;
}

// internal command handler
int CmdHandler(t_args* args){
    char** parsed = (*args)[0];
    std::string cmd(parsed[0]);
    if (cmd == "exit") {
		exit(0);
    }
    else if(cmd == "cd"){
        chdir(parsed[1]);
		return CMD_INTERNEL;
    } 
    else if(cmd == "setenv"){
        setenv(parsed[1],parsed[2],1);
        return CMD_INTERNEL;
    } 
    else if(cmd == "printenv") {
        char* env = getenv(parsed[1]);
        if(env != NULL) printf("%s\n", env);
        return CMD_INTERNEL;
    }
    return CMD_EXTERNAL;
}


void parse_redirect(char* str, char* redirect_file){
    /*** split the original input string by '>' ***/
    char* head = str;  // keep original str head location
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
		    if (redirect_file == NULL){
		    	break;
            }
        }
    }
    str = head; // str point back to original for next parse 
}

// parse pipe and seperate input string into commands
void parse_pipe(char* str, t_cmds* cmds, num_pipe_info* pinfo)
{
    int num_pipe_dest = -1, pipe_type = -1;
    char* head = strsep(&str, "!");
    char* token = strsep(&str, "!");
    if(token != NULL) {
        num_pipe_dest = atoi(token);
        pipe_type = 1;
    }
    str = head;
    cmds->clear();

    while( (token = strsep(&str, "|")) != NULL)
        cmds->push_back(token);

    if( is_number_str(std::string(cmds->back())) ){
        num_pipe_dest = atoi(cmds->back());
        cmds->pop_back();
        pipe_type = 0;
    }
    pinfo->dest = num_pipe_dest;
    pinfo->ptype = pipe_type;
}

void parse_space(t_cmds* cmds, t_args* args){

    // free token pointer array
    for(int i = 0; i< args->size(); i++) delete [] (*args)[i];

    args->clear();

    // parse command str into args
    for(int i = 0; i < cmds->size(); i++){
        char** tokens = new char*[MAX_ARGS];
        char*token;
        int token_size = 0;
        // split command string by space 
        for (int j = 0; j < MAX_ARGS; j++) {
		    tokens[j] = strsep(&(*cmds)[i], " ");
		    if (tokens[j] == NULL){
                token_size = j+1;
                break;
            }
		    if (strlen(tokens[j]) == 0) j--;
	    }
        // push only when args is more than just a NULL
        if(token_size > 1) args->push_back(tokens);
    }
}


int process_string(char* str, t_cmds* cmds, t_args* args, num_pipe_info* pinfo, char* redirect_file){
    // parse input string
    parse_redirect(str, redirect_file); 
    parse_pipe(str, cmds, pinfo); 
    parse_space(cmds, args);

    int exec_mode = CMD_NULL; // no command to execute
    if(args->size() > 0)
        exec_mode = CmdHandler(args);
    return exec_mode;
}

int main(){

    char input_string[MAX_CHAR], redirect_file[MAX_CHAR];
    
    t_cmds cmds;
    t_args args;

    numbered_pipes n_pipes;
    num_pipe_info pinfo;  // numbered pipe dest and type
    
    // signal handler
    signal(SIGCLD, signal_handler);

    init_shell();

    while (1){
        int input_status = take_input(input_string);
        if(input_status == INPUT_EMPTY) continue;
        
        if( process_string(input_string, &cmds, &args, &pinfo, redirect_file) == CMD_EXTERNAL )
            exec_args(&args, redirect_file, &n_pipes, &pinfo);

        if(input_status == INPUT_EOF) return 0;
    }
    return 0;
}
