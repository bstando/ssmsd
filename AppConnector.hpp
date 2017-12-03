//
// Created by bartosz on 06.06.16.
//

#ifndef SERVER_REQUESTSLISTENER_HPP
#define SERVER_REQUESTSLISTENER_HPP

#include <string>

#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>


#include <cstring>
#include <vector>
#include <string>
#include "DBHelper.hpp"
#include <ArduinoJson.h>


class AppConnector {
private:

    static const int BUF_SIZE = 500;
    DBHelper *helper;

    vector<std::string> PrepareAllDataResponse();
    vector<std::string> PrepareDataResponse(int limit);
    vector<std::string> PrepareByDateResponse(std::string date);
    vector<std::string> PrepareBySensorIDResponse(int sensorID);
    vector<std::string> PrepareSensorIDsResponse();
    vector<std::string> PrepareEmptyResponse();

public:
    const char *port = "9873";
    void StartListening();
    //AppConnector();
    explicit AppConnector(const string &filename);
    ~AppConnector();
};


#endif //SERVER_REQUESTSLISTENER_HPP
