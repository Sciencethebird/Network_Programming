#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>
#include <deque>

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

void npdup2(int oldfd, int newfd){
    if(dup2(oldfd, newfd)<0){
        fprintf(stderr, "dup2 error, old:%d, new:%d, %s", oldfd, newfd, strerror(errno));
    }
}
