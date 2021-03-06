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

#define MAX_CHAR 15000 // max number of letters to be supported
#define MAX_ARGS 10000 // max number of commands to be supported


// print current working directory
void printcwd(){
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("\ncwd: %s", cwd);
}

void execArgs(char** parsed){
    // fork a child to execute command
    pid_t pid = fork();

    if(pid == -1){
        printf("\nfork() error..");
        return;
    } else if (pid == 0) {
        if (execvp(parsed[0], parsed) < 0)
            printf("\nCommand execution failed...");
        exit(0);
    } else {
        // wait for child to terminate
        wait(NULL);
        return;
    }

}

// Function to take input
int takeInput(char* str)
{
    std::string buff;
    printf("\n%% ");
    std::getline(std::cin, buff);
    std::cout << buff << " len: " << buff.length() << std::endl;
    strcpy(str, buff.c_str());
    if ( strlen(str) != 0 ) return 0;
    else return 1;
}


// special command handler
int CmdHandler(char** parsed){
	int NoOfOwnCmds = 2, i, switchOwnArg = 0;
	char* ListOfOwnCmds[NoOfOwnCmds];
	char* username;

	ListOfOwnCmds[0] = strdup("exit");
	ListOfOwnCmds[1] = strdup("cd");

	for (i = 0; i < NoOfOwnCmds; i++) {
		if (strcmp(parsed[0], ListOfOwnCmds[i]) == 0) {
			switchOwnArg = i + 1;
			break;
		}
	}

	switch (switchOwnArg) {
	case 1:
		printf("\nGoodbye\n");
		exit(0);
	case 2:
		chdir(parsed[1]);
		return 1;
	default:
		break;
	}

	return 0;
}


void parseSpace(char* str, char** parsed){
	int i;

	for (i = 0; i < MAX_CHAR; i++) {
		parsed[i] = strsep(&str, " ");

		if (parsed[i] == NULL)
			break;
		if (strlen(parsed[i]) == 0)
			i--;
	}
}

int processString(char* str, char** parsed, char** parsedpipe){

    int piped = 0;

    parseSpace(str, parsed);

    if (CmdHandler(parsed)) return 0;
    else return 1 + piped;
}



int main(){

    char inputString[MAX_CHAR], *parsedArgs[MAX_ARGS];
	char* parsedArgsPiped[MAX_ARGS];
    int execMode = 0;

    while (1){
        printcwd();
        if(takeInput(inputString))
            continue;
        execMode = processString(inputString, parsedArgs, parsedArgsPiped);
        
        // single command
        if(execMode == 1)
            execArgs(parsedArgs);

    }
    return 0;
}