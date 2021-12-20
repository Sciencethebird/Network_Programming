#ifndef NPENV_H
#define NPENV_H

#include <string>
#include <vector>
#include <map>


class NPEnv{
public:
    NPEnv(){}
    void set_env(std::string var_name, std::string value){
        setenv(var_name.c_str() , value.c_str(), 1);
        env_vars[var_name] = value;

    }
    std::string get_env(std::string var_name){
        char* env = getenv(var_name.c_str());
        if(env != NULL) {
            return std::string(env);
        } else {
            return "";
        }
    }
    void load_env(){
        clearenv(); // set env to default before load
        for(auto var: env_vars){
            set_env(var.first, var.second);
        }
    }
private:
    std::map<std::string, std::string> env_vars;
};

#endif // NPENV_H