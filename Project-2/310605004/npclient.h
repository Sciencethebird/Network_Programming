# include "numbered_pipes.h"
# include "npshm.h"
# include "npsemaphore.h"
# include "npenv.h"
# include <netinet/in.h>
# include <netdb.h> 
# include <sys/socket.h>
# include <arpa/inet.h>

# define MAX_CLIENT_NUM 30
# define MAX_MESSAGE_QUEUE 10 
# define EVERYONE -1

/*================== NPClient ==================*/
/*  This class is for np_single_proc (Server 2) */ 
class NPClient{
public:
    NPClient(){
        client_name = "(no name)";
        shell_env.set_env("PATH", "bin:.");
    }
    NPClient(const char* envc, int fd, struct sockaddr_in addr){
        //n_pipes.debug();
        env = std::string(envc);
        client_sockfd = fd;
        client_name = "(no name)";
        client_addr = addr;
        shell_env.set_env("PATH", "bin:.");
    }
    int get_sockfd(){
        return client_sockfd;
    }
    const char* get_env(){
        return env.c_str();
    }
    void set_env(char* new_env){
        env = new_env;
    }
    numbered_pipes get_n_pipe(){
        return n_pipes;
    }
    numbered_pipes* get_n_pipe_ptr(){
        return &n_pipes;
    }
    void set_n_pipe(numbered_pipes new_pipes){
        n_pipes = new_pipes;
    }
    std::string get_name(){
        return client_name;
    }
    void set_name(std::string name){
        client_name = name;
    }
    std::string get_addr_str(){
        return std::string(inet_ntoa(client_addr.sin_addr))+":"+std::to_string(htons (client_addr.sin_port));
    }
    int get_id(){
        return client_id;
    }
    void set_id(int id){
        client_id = id;
    }
public:
    NPEnv shell_env;
private:
    numbered_pipes n_pipes;
    std::string env;
    std::string client_name;
    int client_sockfd;
    int client_id;
    struct sockaddr_in client_addr; 
};


/*============= NPClientData =============*/
/*      This class is for server 3        */ 

struct client_data_unit
{
    int id;
    int sockfd;
    char nickname[20];
    char ip[20];
    char port[20];
    
    //struct sockaddr_in client_addr; 
};
class NPClientData: public NPShm{
public:
    NPClientData():NPShm(){}
    NPClientData(int key, int num_of_data):NPShm(key, sizeof(client_data_unit), num_of_data){
        //std::cout << "unit data size: " << sizeof(client_data_unit) << std::endl;
        for(int i = 0; i<= MAX_CLIENT_NUM; i++){
            client_sockfds[i] = 0;
        }
        shm_lock.lock();
        client_data_unit* shm_head = (client_data_unit*) get_shm_head();
        for(int i = 0; i<= MAX_CLIENT_NUM; i++){
            shm_head[i].sockfd = 0;
            shm_head[i].id = 0;
        }
        shmdt(shm_head); /// ??
        shm_lock.unlock();
    }
    void add_client(int id, int sockfd, struct sockaddr_in client_addr){
        shm_lock.lock();
        fprintf(stdout,"[SHM] shared mem size: %d\n", shm_size);
        client_data_unit new_client;
        new_client.id = id;
        if(new_client.id <= 0){
            fprintf(stdout,"[Error] Can't assign new client id\n");
        }
        new_client.sockfd = sockfd;
        
        strcpy( new_client.nickname, "(no name)");
        strcpy( new_client.ip, std::string(inet_ntoa(client_addr.sin_addr)).c_str() );
        strcpy( new_client.port, std::to_string(htons(client_addr.sin_port)).c_str() );

        client_data_unit* shm_head = (client_data_unit*) get_shm_head();
        
        shm_head[new_client.id] = new_client;

        client_sockfds[new_client.id] = sockfd;
        //detach
        shmdt(shm_head); // ????
        shm_lock.unlock();
    }
    void remove_client(int id){
        // this function should only be called in parent
        shm_lock.lock();
        client_sockfds[id] = 0;
        client_data_unit* shm_head = (client_data_unit*) get_shm_head();
        shm_head[id].sockfd = 0;
        shmdt(shm_head);
        shm_lock.unlock();
    }
    int check_client(int id){
        shm_lock.lock();
        client_data_unit* shm_head = (client_data_unit*) get_shm_head();
        if(shm_head[id].sockfd == 0){
            shmdt(shm_head);
            shm_lock.unlock();
            return 0;
        }else{
            shmdt(shm_head);
            shm_lock.unlock();
            return 1;
        }
    }
    int find_and_set_available_id(int sockfd){
        for(int i = 1; i<= MAX_CLIENT_NUM; i++){
            if(client_sockfds[i] == 0){
                client_sockfds[i] = sockfd;
                return i;
            } 
        }
        return -1;
    }
    client_data_unit get_client_data(int id){
        if(id > MAX_CLIENT_NUM || id < 1) id = 0;
        shm_lock.lock();
        client_data_unit* shm_head = (client_data_unit*) get_shm_head();
        client_data_unit temp = shm_head[id];
        // TODO: fix get 0
        shmdt(shm_head); /// ??
        shm_lock.unlock();
        return temp;
    }
    std::string get_name(int id){
        // TODO: client DNE error?
        return std::string( get_client_data(id).nickname );
    }
    void set_name(int id, std::string new_name){
        shm_lock.lock();
        client_data_unit* shm_head = (client_data_unit*) get_shm_head();
        strcpy(shm_head[id].nickname, new_name.c_str());
        shmdt(shm_head); /// ??
        shm_lock.unlock();
    }
    int find_client_by_name(std::string name){
        shm_lock.lock();
        int id_found = -1;
        client_data_unit* shm_head = (client_data_unit*) get_shm_head();
        for(int i = 1; i<= MAX_CLIENT_NUM; i++){
            if(name == std::string(shm_head[i].nickname)){
                id_found = i;
                break;
            }
        }
        shmdt(shm_head); /// ??
        shm_lock.unlock();
        return id_found;
    }
    std::string get_addr_str(int id){
        shm_lock.lock();
        client_data_unit* shm_head = (client_data_unit*) get_shm_head();
        std::string buffer = std::string(shm_head[id].ip)+':'+std::string(shm_head[id].port);
        shmdt(shm_head); /// ??
        shm_lock.unlock();
        return buffer;
    }
    std::string who(int me/*, int fd*/){
        std::string buff = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\r\n";
        //write(fd, buff.c_str(), buff.size());
        for(int id = 1; id <= MAX_CLIENT_NUM; id++){
            client_data_unit curr = get_client_data(id);
            if(curr.sockfd > 0){
                int fd = curr.sockfd;
                buff += std::to_string(id)+'\t'+
                       std::string(curr.nickname) +'\t'+
                       std::string(curr.ip)+':'+std::string(curr.port);
                if(id == me){
                    buff += "\t<-me\r\n";
                }else{
                    buff += "\r\n";
                }
            }
        }
        //write(fd, buff.c_str(), buff.size());
        return buff;
    }
    int get_sockfd(int id){ // this should not be uses in child
        return client_sockfds[id];
    }
private:
    // this array only works/valid in parent
    int client_sockfds[MAX_CLIENT_NUM+1]; // maybe change to avaliable_id (since sockfd is useless here)
};


struct client_msg_unit{
    char msg [1100];
    int read_done[MAX_CLIENT_NUM+1];
    int from_id;
    int to_id;
};

class NPClientMsg: public NPShm{
public:
    NPClientMsg():NPShm(){}
    NPClientMsg(int key, int num_of_data):NPShm(key, sizeof(client_msg_unit), num_of_data+2){
        //  use first message to store msg counter

        //std::cout << "unit data size: " << sizeof(client_data_unit) << std::endl;
        client_msg_unit* shm_head = (client_msg_unit*) get_shm_head();
        for(int i = 0; i<num_of_data; i++){
            shm_head[i].from_id = 0; // 0 mean this space is available
        }
        shmdt(shm_head);
    }
    client_msg_unit* attach(){
        curr_ptr = (client_msg_unit*) get_shm_head();
        return curr_ptr;
    }
    void detach(){
        shmdt(curr_ptr);
    }
    void add_msg(int from_id, int to_id, std::string msg){
        
        //std::cout << num_of_units << std::endl;
        client_msg_unit new_msg;

        new_msg.from_id = from_id;
        new_msg.to_id = to_id;
        strcpy(new_msg.msg, msg.c_str());
        
        for(int i = 0; i<MAX_CLIENT_NUM; i++){
            new_msg.read_done[i] = 0;
        }
        shm_lock.lock();
        client_msg_unit* shm_head = (client_msg_unit*) get_shm_head();
        // semophore lock
        // find empty space from begining
        for(int i = 0; i<num_of_units; i++){
            if(shm_head[i].from_id == 0){ // means th
                shm_head[i] = new_msg;
                break;
            }  
        }
        //detach
        //std::cout << shm_head[0].msg << std::endl;
        shmdt(shm_head); // ????
        shm_lock.unlock();
        //std::cout << num_of_units << std::endl;
        // semophore unlock
    }
    std::string get_my_message(int my_id){
        shm_lock.lock();
        client_msg_unit* shm_head = (client_msg_unit*) get_shm_head();
        std::string buffer;
        // sem_lock
        for(int i = 0; i<num_of_units; i++){
            if(shm_head[i].to_id == my_id || shm_head[i].to_id == EVERYONE){ // means th
                buffer+=std::string(shm_head[i].msg); //add new line?
            }
            shm_head[i].read_done[my_id] = 1;
        }
        // sem_unlock
        shmdt(shm_head);
        shm_lock.unlock();
        return buffer;
    } 
    void clear_all(){
        shm_lock.lock();
        client_msg_unit* shm_head = (client_msg_unit*) get_shm_head();
        for(int i = 0; i< MAX_MESSAGE_QUEUE; i++){
            shm_head[i].to_id = 0;
            shm_head[i].from_id = 0;
            for(int id = 1; id <=  MAX_CLIENT_NUM; id++){
                shm_head[i].read_done[id] = 0;
            }
            strcpy(shm_head[i].msg, "XXXXXXXXX");
        }
        shmdt(shm_head);
        shm_lock.unlock();
    }
    client_data_unit get_client_data(int idx){
        // TODO: fix get 0
        shm_lock.lock();
        client_data_unit* shm_head = (client_data_unit*) get_shm_head();
        client_data_unit temp = shm_head[idx];
        shmdt(shm_head); /// ??
        shm_lock.unlock();
        return temp;
    }
    std::string get_name(int idx){
        return std::string( get_client_data(idx).nickname );
    }

    void print_all(int fd = STDOUT_FILENO){
        shm_lock.lock();
        client_msg_unit* shm_head = (client_msg_unit*) get_shm_head();
        for(int i = 0; i< 5; i++){
            //shm_head[i].from_id;
            char buff[1024];
            sprintf(buff, "[MSG] %dth, from: %d, to: %d, msg: ", i, shm_head[i].from_id, shm_head[i].to_id);
            std::string str_buff(buff);// = "fromid: " + std::to_string(shm_head[i].from_id);
            str_buff += std::string(shm_head[i].msg);
            write(fd, str_buff.c_str(), str_buff.size());
        }
        shmdt(shm_head);
        shm_lock.unlock();
    }
private:
    client_msg_unit* curr_ptr;
};