# include <netinet/in.h>
# include <netdb.h> 
# include <strings.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <string>

int TCPsocket(int port){
    return 0;
}

void send_to_sock(int sockfd, std::string msg){
    send(sockfd, msg.c_str(), msg.size(), 0);
}