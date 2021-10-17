#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<iostream>
#include<string>
#include<vector>
#include<sstream>

typedef std::vector<std::string> t_args;

// print current working directory
void printcwd(){
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("\ncwd: %s", cwd);
}
char** create_args(t_args* parsed){
    char** c_arg = new (char*) [par]
}
void execArgs(t_args* parsed){
    // fork a child to execute command
    pid_t pid = fork();

    if(pid == -1){
        printf("\nfork() error..");
        return;
    } else if (pid == 0) {
        for(auto arg:(*parsed)){

        }
        if (execvp())
    } else {
        // wait for child to terminate
        wait(NULL);
        return;
    }

}
// take input
int takeInput(std::string* str){
    printf("\n%% ");
    std::getline(std::cin, *str);
    std::cout << *str << " len: " << str->length() << std::endl;
    if ( str->length() != 0 ) return 0;
    else return 1;
}

// special command handler
int CmdHandler(t_args* parsed){
    t_args MyOwnCmds = {"exit", "cd"};
    int matched_idx = -1;
    for (int i = 0; i < MyOwnCmds.size(); i++){
        if( parsed->front() == MyOwnCmds[i] ) matched_idx = i;
    }
    switch (matched_idx){
    case 0:
        printf("keep up gogo~~\n");
        exit(0);
    case 1:
        chdir( (*parsed)[1].c_str() );
		return 1;
    default:
        break;
    }
    return 0;
}
void ttesparseSpace(std::string str, t_args* parsed){
    // https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c?answertab=votes#tab-top
    parsed->clear();
    size_t pos = 0;
    std::string token;
    while( (pos = str.find(" ")) != std::string::npos ){
        token = str.substr(0, pos);
        std::cout << token << std::endl;
        parsed->push_back(token);
        str.erase(0, pos+1); //1 is length of delimiter
    }

}

void parseSpace(std::string str, t_args* parsed){
    // https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c?answertab=votes#tab-top
    parsed->clear();
    std::istringstream ss(str);
    std::string token;
    while( ss >> token )
        parsed->push_back(token);
}

int processString(std::string* str, t_args* parsed, t_args* parsed_pipe){

    int piped = 0;

    parseSpace(*str, parsed);

    if (CmdHandler(parsed)) return 0;
    else return 1 + piped;
}



int main(){

    std::string inputString;
    t_args parsedArgs, parsedArgsPiped;
    int execMode = 0;

    while (1){
        printcwd();
        if(takeInput(&inputString))
            continue;
        execMode = processString(&inputString, &parsedArgs, &parsedArgsPiped);
        
        // single command
        if(execMode == 1)
            execArgs(parsedArgs);

    }
    return 0;
}