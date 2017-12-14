//
// Created by bartosz on 02.12.17.
//

#ifndef SSMSD_SERVERHELPER_HPP
#define SSMSD_SERVERHELPER_HPP


#include "DBHelper.hpp"

class ServerHelper {
private:
    std::string appName;
    std::string logDir;
    std::string dbFile;
    int interval;
    std::string hostname;
    shared_ptr<DBHelper> helper;
public:
    ServerHelper();
    void ShowHelp();
    void InitLog();
    void ParseArguments(int argc, char** argv);
    std::string GetHostname();
    std::string GetDBFileName();
    int GetInterval();
    void Daemonize();
    shared_ptr<DBHelper> InitDatabase();
};


#endif //SSMSD_SERVERHELPER_HPP
