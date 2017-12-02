//
// Created by bartosz on 02.12.17.
//

#ifndef SSMSD_SERVERHELPER_HPP
#define SSMSD_SERVERHELPER_HPP


class ServerHelper {
private:
    std::string appName;
    std::string logDir;
    std::string dbFile;
    int interval;
    std::string hostname;
public:
    ServerHelper();
    void ShowHelp();
    void InitLog();
    void ParseArguments(int argc, char** argv);
    std::string GetHostname();
    std::string GetDBFileName();
    int GetInterval();
    void Daemonize();
};


#endif //SSMSD_SERVERHELPER_HPP
