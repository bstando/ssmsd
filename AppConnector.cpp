//
// Created by bartosz on 06.06.16.
//

#include <thread>
#include <boost/log/trivial.hpp>
#include "AppConnector.hpp"

using namespace std;
vector<std::string> AppConnector::prepareAllDataResponse() {
    vector<std::string> retval;
    DynamicJsonBuffer jsonBuffer;
    vector<SensorData> sensorData = helper->GetAllData();
    //cout << "Data size: " << sensorData.size() << endl;
    BOOST_LOG_TRIVIAL(debug)  << "Data size: " << sensorData.size() << endl;
    for (long i = 0; i < sensorData.size() ; i+=10) {
        JsonObject &response = jsonBuffer.createObject();
        response.set<int>("content_length", 0);
        JsonArray &sensorDataArray = response.createNestedArray("content");
        long j = 0;
        for(j = 0;(j<10)&&((i+j)<sensorData.size());j++)
        {
            JsonObject &sensorDataObject = sensorDataArray.createNestedObject();
            sensorDataObject["id"] = sensorData[i+j].getID();
            sensorDataObject["sensorID"] = sensorData[i+j].getSensorID();
            sensorDataObject["date"] = sensorData[i+j].getDate();
            sensorDataObject["temperature"] = sensorData[i+j].getTemperature();
            sensorDataObject["humidity"] = sensorData[i+j].getHumidity();

        }
        response.set<int>("content_length",j);
        if(j==10) {
            response.set<bool>("has_next", true);
        } else
        {
            response.set<bool>("has_next", false);
        }
	std::string line;
        response.printTo(line);
        retval.push_back(line);
        //cout << "Chunk size: " << j << ", Content length: " << line.length() << ", current pos: " << i << " of " << sensorData.size() << endl;
        BOOST_LOG_TRIVIAL(debug) << "Chunk size: " << j << ", Content length: " << line.length() << ", current pos: " << i << " of " << sensorData.size();
    }
    if(sensorData.size()%10 == 0)
    {
        JsonObject &response = jsonBuffer.createObject();
        response.set<int>("content_length", 0);
        JsonArray &sensorDataArray = response.createNestedArray("content");
        response.set<bool>("has_next", false);
	std::string line;
        response.printTo(line);
        retval.push_back(line);
    }

    return retval;
}

void AppConnector::StartListening() {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    ssize_t nread;
    char buf[AppConnector::BUF_SIZE];


    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    s = getaddrinfo(NULL, port, &hints, &result);
    if (s != 0) {
        //fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        BOOST_LOG_TRIVIAL(fatal) << "getaddrinfo: " << gai_strerror(s);
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully bind(2).
        If socket(2) (or bind(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */

        close(sfd);
    }

    if (rp == NULL) {               /* No address succeeded */
        //fprintf(stderr, "Could not bind\n");
        BOOST_LOG_TRIVIAL(fatal) << "Could not bind";
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);           /* No longer needed */

    /* Read datagrams and echo them back to sender */

    for (; ;) {
        peer_addr_len = sizeof(struct sockaddr_storage);
        nread = recvfrom(sfd, buf, BUF_SIZE, 0,
                         (struct sockaddr *) &peer_addr, &peer_addr_len);
        if (nread == -1)
            continue;               /* Ignore failed request */

        char host[NI_MAXHOST], service[NI_MAXSERV];

        s = getnameinfo((struct sockaddr *) &peer_addr,
                        peer_addr_len, host, NI_MAXHOST,
                        service, NI_MAXSERV, NI_NUMERICSERV);
        if (s == 0) {
            //printf("Received %ld bytes from %s:%s\n",(long) nread, host, service);
            BOOST_LOG_TRIVIAL(debug) << "Received " << (long) nread << " bytes from " << host << ":" << service;
        }
        else {
            //fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));
            BOOST_LOG_TRIVIAL(fatal) << "getnameinfo: " << gai_strerror(s);
        }
        DynamicJsonBuffer buffer;
        JsonObject &command = buffer.parseObject(buf);
        if (command.success()) {
            vector<string> response;
            int commandID = command["id"];
            int count=0;
            int sensorID=0;
            string dateFrom;
            switch (commandID) {
                case 0:
                    response = prepareAllDataResponse();
                    break;
                case 1:
                    count = command["limit"];
                    if(count!=0)
                    response = prepareDataResponse(count);
                    break;
                case 2:
                    //command["date"];
                    //if(dateFrom.length()!=0)
                    response = prepareByDateResponse( command["date"]);
                    break;
                case 3:
                    sensorID = command["sensorID"];
                    response = prepareBySensorIDResponse(sensorID);
                    break;
                case 4:
                    response = prepareSensorIDsResponse();
                    break;
                default:
                    response = prepareEmptyResponse();
                    break;
            }
            if(response.size()==0) response = prepareEmptyResponse();

            for (int i = 0; i < response.size(); i++) {
                int rplen = sendto(sfd, response[i].c_str(), response[i].length(), 0,
                                   (struct sockaddr *) &peer_addr,
                                   peer_addr_len);
                //prepareAllDataResponse();
                if (rplen != response[i].length()) {
                    //fprintf(stderr, "Error sending response\n");
                    BOOST_LOG_TRIVIAL(fatal) << "Error sending response";
                }
                //printf("Length: %d\n", rplen);
                BOOST_LOG_TRIVIAL(debug) << "Length: " << rplen;
                this_thread::sleep_for(chrono::milliseconds(100));
            }
        }
    }
}

/*AppConnector::AppConnector() {
    helper = new DBHelper();
}*/

AppConnector::~AppConnector() {
    delete helper;
}

vector<std::string> AppConnector::prepareByDateResponse(std::string date) {
    vector<std::string> retval;
    DynamicJsonBuffer jsonBuffer;
    vector<SensorData> sensorData = helper->GetByDate(date);
    //cout << "Data size: " << sensorData.size() << endl;
    BOOST_LOG_TRIVIAL(debug) << "Data size: "<<sensorData.size();
    for (long i = 0; i < sensorData.size(); i += 10) {
        JsonObject &response = jsonBuffer.createObject();
        response.set<int>("content_length", 0);
        JsonArray &sensorDataArray = response.createNestedArray("content");
        long j = 0;
        for (j = 0; (j < 10) && ((i + j) < sensorData.size()); j++) {
            JsonObject &sensorDataObject = sensorDataArray.createNestedObject();
            sensorDataObject["id"] = sensorData[i + j].getID();
            sensorDataObject["sensorID"] = sensorData[i + j].getSensorID();
            sensorDataObject["date"] = sensorData[i + j].getDate();
            sensorDataObject["temperature"] = sensorData[i + j].getTemperature();
            sensorDataObject["humidity"] = sensorData[i + j].getHumidity();

        }
        response.set<int>("content_length", j);
        if (j == 10) {
            response.set<bool>("has_next", true);
        } else {
            response.set<bool>("has_next", false);
        }
	std::string line;
        response.printTo(line);
        retval.push_back(line);
        //cout << "Chunk size: " << j << ", Content length: " << line.length() << ", current pos: " << i << " of " << sensorData.size() << endl;
        BOOST_LOG_TRIVIAL(debug) << "Chunk size: " << j << ", Content length: " << line.length() << ", current pos: " << i << " of " << sensorData.size();

    }
    if(sensorData.size()%10 == 0)
    {
        JsonObject &response = jsonBuffer.createObject();
        response.set<int>("content_length", 0);
        JsonArray &sensorDataArray = response.createNestedArray("content");
        response.set<bool>("has_next", false);
	std::string line;
        response.printTo(line);
        retval.push_back(line);
    }

    return retval;
}



vector<std::string> AppConnector::prepareDataResponse(int limit) {
    vector<std::string> retval;
    DynamicJsonBuffer jsonBuffer;
    vector<SensorData> sensorData = helper->GetLastData(limit);
    //cout << "Data size: " << sensorData.size() << endl;
    BOOST_LOG_TRIVIAL(debug) << "Data size: "<<sensorData.size();
    for (long i = 0; i < sensorData.size(); i += 10) {
        JsonObject &response = jsonBuffer.createObject();
        response.set<int>("content_length", 0);
        JsonArray &sensorDataArray = response.createNestedArray("content");
        long j = 0;
        for (j = 0; (j < 10) && ((i + j) < sensorData.size()); j++) {
            JsonObject &sensorDataObject = sensorDataArray.createNestedObject();
            sensorDataObject["id"] = sensorData[i + j].getID();
            sensorDataObject["sensorID"] = sensorData[i + j].getSensorID();
            sensorDataObject["date"] = sensorData[i + j].getDate();
            sensorDataObject["temperature"] = sensorData[i + j].getTemperature();
            sensorDataObject["humidity"] = sensorData[i + j].getHumidity();

        }
        response.set<int>("content_length", j);
        if (j == 10) {
            response.set<bool>("has_next", true);
        } else {
            response.set<bool>("has_next", false);
        }
	std::string line;
        response.printTo(line);
        retval.push_back(line);
        BOOST_LOG_TRIVIAL(debug) << "Chunk size: " << j << ", Content length: " << line.length() << ", current pos: " << i << " of " <<
        sensorData.size();
    }
    if(sensorData.size()%10 == 0)
    {
        JsonObject &response = jsonBuffer.createObject();
        response.set<int>("content_length", 0);
        JsonArray &sensorDataArray = response.createNestedArray("content");
        response.set<bool>("has_next", false);
	std::string line;
        response.printTo(line);
        retval.push_back(line);
    }

    return retval;
}

vector<std::string> AppConnector::prepareBySensorIDResponse(int sensorID) {
    vector<std::string> retval;
    DynamicJsonBuffer jsonBuffer;
    vector<SensorData> sensorData = helper->GetBySensorID(sensorID);
    BOOST_LOG_TRIVIAL(debug) << "Data size: " << sensorData.size();
    for (long i = 0; i < sensorData.size(); i += 10) {
        JsonObject &response = jsonBuffer.createObject();
        response.set<int>("content_length", 0);
        JsonArray &sensorDataArray = response.createNestedArray("content");
        long j = 0;
        for (j = 0; (j < 10) && ((i + j) < sensorData.size()); j++) {
            JsonObject &sensorDataObject = sensorDataArray.createNestedObject();
            sensorDataObject["id"] = sensorData[i + j].getID();
            sensorDataObject["sensorID"] = sensorData[i + j].getSensorID();
            sensorDataObject["date"] = sensorData[i + j].getDate();
            sensorDataObject["temperature"] = sensorData[i + j].getTemperature();
            sensorDataObject["humidity"] = sensorData[i + j].getHumidity();

        }
        response.set<int>("content_length", j);
        if (j == 10) {
            response.set<bool>("has_next", true);
        } else {
            response.set<bool>("has_next", false);
        }
	std::string line;
        response.printTo(line);
        retval.push_back(line);
        //cout << "Chunk size: " << j << ", Content length: " << line.length() << ", current pos: " << i << " of " << sensorData.size() << endl;
        BOOST_LOG_TRIVIAL(debug) << "Chunk size: " << j << ", Content length: " << line.length() << ", current pos: " << i << " of " << sensorData.size();

    }
    if(sensorData.size()%10 == 0)
    {
        JsonObject &response = jsonBuffer.createObject();
        response.set<int>("content_length", 0);
        JsonArray &sensorDataArray = response.createNestedArray("content");
        response.set<bool>("has_next", false);
	std::string line;
        response.printTo(line);
        retval.push_back(line);
    }

    return retval;
}

vector<std::string> AppConnector::prepareEmptyResponse() {
    vector<std::string> retval;
    DynamicJsonBuffer jsonBuffer;
    JsonObject &response = jsonBuffer.createObject();
    response.set<int>("content_length", 0);
    JsonArray &sensorDataArray = response.createNestedArray("content");
    response.set<bool>("has_next", false);
    std::string line;
    response.printTo(line);
    retval.push_back(line);
    return retval;
}

vector<std::string> AppConnector::prepareSensorIDsResponse() {
    vector<std::string> retval;
    DynamicJsonBuffer jsonBuffer;
    vector<int> sensorData = helper->GetSensorIDs();
    //cout << "Data size: " << sensorData.size() << endl;
    BOOST_LOG_TRIVIAL(debug) << "Data size: " << sensorData.size();
    for (long i = 0; i < sensorData.size(); i += 10) {
        JsonObject &response = jsonBuffer.createObject();
        response.set<int>("content_length", 0);
        JsonArray &sensorDataArray = response.createNestedArray("content");
        long j = 0;
        for (j = 0; (j < 10) && ((i + j) < sensorData.size()); j++) {
            JsonObject &sensorDataObject = sensorDataArray.createNestedObject();
            sensorDataObject["value"] = sensorData[i + j];
        }
        response.set<int>("content_length", j);
        if (j == 10) {
            response.set<bool>("has_next", true);
        } else {
            response.set<bool>("has_next", false);
        }
	std::string line;
        response.printTo(line);
        retval.push_back(line);
        //cout << "Chunk size: " << j << ", Content length: " << line.length() << ", current pos: " << i << " of " << sensorData.size() << endl;
        BOOST_LOG_TRIVIAL(debug) << "Chunk size: " << j << ", Content length: " << line.length() << ", current pos: " << i << " of " << sensorData.size();

    }
    if(sensorData.size()%10 == 0)
    {
        JsonObject &response = jsonBuffer.createObject();
        response.set<int>("content_length", 0);
        JsonArray &sensorDataArray = response.createNestedArray("content");
        response.set<bool>("has_next", false);
	std::string line;
        response.printTo(line);
        retval.push_back(line);
    }


    return retval;
}

AppConnector::AppConnector(const string &filename) {
    helper = new DBHelper(filename);
}















