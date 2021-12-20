#include <string>
std::string msg_wellcome(){
    std::string buffer = "****************************************\n** Welcome to the information server. **\n****************************************\r\n";
    return buffer;
}

std::string msg_user_enter(std::string address){
    std::string buffer = "*** User '(no name)' entered from " + address + ". ***\r\n";
    return buffer;
}

std::string msg_upipe_pipe_success(std::string a, int aid, std::string b, int bid, std::string cmd){
    //*** student1 (#1) just received from student2 (#2) by ’cat <2 | number | number | cat’ ***
    char buffer[5000];
    sprintf(buffer, "*** %s (#%d) just piped '%s' to %s (#%d) ***\r\n"
                    , a.c_str(), aid, cmd.c_str(), b.c_str(), bid);
    return std::string(buffer);
}

std::string msg_upipe_recv_success(std::string a, int aid, std::string b, int bid, std::string cmd){
    //*** student1 (#1) just received from student2 (#2) by ’cat <2 | number | number | cat’ ***
    char buffer[5000];
    sprintf(buffer, "*** %s (#%d) just received from %s (#%d) by '%s' ***\r\n"
                    , a.c_str(), aid, b.c_str(), bid, cmd.c_str());
    return std::string(buffer);
}
std::string msg_upipe_pipe_exist(int aid, int bid){
    //*** student1 (#1) just received from student2 (#2) by ’cat <2 | number | number | cat’ ***
    char buffer[5000];
    sprintf(buffer, "*** Error: the pipe #%d->#%d already exists. ***\r\n", aid, bid);
    return std::string(buffer);
}

std::string msg_upipe_pipe_dne(int aid, int bid){
    //*** student1 (#1) just received from student2 (#2) by ’cat <2 | number | number | cat’ ***
    char buffer[5000];
    sprintf(buffer, "*** Error: the pipe #%d->#%d does not exist yet. ***\r\n", aid, bid);
    return std::string(buffer);
}


std::string msg_upipe_user_dne(int id){
    //*** student1 (#1) just received from student2 (#2) by ’cat <2 | number | number | cat’ ***
    char buffer[5000];
    sprintf(buffer, "*** Error: user #%d does not exist yet. ***\r\n", id);
    return std::string(buffer);
}

