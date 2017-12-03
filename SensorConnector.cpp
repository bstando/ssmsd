//
// Created by bartosz on 02.12.17.
//

#include "SensorConnector.hpp"
#include <netdb.h>
#include <cstring>
#include <boost/log/support/date_time.hpp>
#include <boost/thread.hpp>
#include <ArduinoJson.h>


SensorConnector::SensorConnector(Zeroconf &zeroconf, int interval ,std::string dbFile) {
    this->zeroconf = &zeroconf;
    this->interval = interval;
    this->dbFile = dbFile;
    dbHelper = new DBHelper(dbFile);
}

SensorData SensorConnector::DownloadData(const char *host, int port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s, j;
    size_t len;
    ssize_t nread;
    char buf[BUFFSIZE];
    char chport[6];
    SensorData sensorResult;
    sensorResult.setSensorID(-1);
    time_t rawtime;
    struct tm *timeinfo;
    char timebuffer[80];


    sprintf(chport, "%d", port);
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    s = getaddrinfo(host, chport, &hints, &result);
    if (s != 0) {
        //fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        BOOST_LOG_TRIVIAL(fatal) << "getaddrinfo: " << gai_strerror(s);
        return sensorResult;
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

        close(sfd);
    }

    if (rp == NULL) {               /* No address succeeded */
        //fprintf(stderr, "Could not connect\n");
        BOOST_LOG_TRIVIAL(fatal) << "Could not connect";
        return sensorResult;
    }

    freeaddrinfo(result);           /* No longer needed */

    bzero(buf, BUFFSIZE);
    StaticJsonBuffer<512> jsonBuffer;
    JsonObject &command = jsonBuffer.createObject();
    command.set<int>("id", 2);
    command.printTo(buf, sizeof(buf));
    buf[command.measureLength()] = '\0';

    if (write(sfd, buf, strlen(buf)) != strlen(buf)) {
        //fprintf(stderr, "partial/failed write\n");
        BOOST_LOG_TRIVIAL(fatal) << "partial/failed write";
        return sensorResult;
    }

    bzero(buf, BUFFSIZE);

    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;

    setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *) &tv, sizeof(struct timeval));
    nread = read(sfd, buf, BUFFSIZE);
    if (nread == -1) {
        //perror("read");
        BOOST_LOG_TRIVIAL(fatal) << "Error during packet read";
        BOOST_LOG_TRIVIAL(fatal) << strerror(errno) << ": " << errno;
        //exit(-1);
        return sensorResult;
    }

    JsonObject &root = jsonBuffer.parseObject(buf);
    //cout << "Echo from server:" << buf << endl;
    BOOST_LOG_TRIVIAL(debug) << "Echo from server:" << buf;
    if (root.success()) {
        int id = root["id"];
        float temperature = root["temperature"];
        float humidity = root["humidity"];

        sensorResult.setSensorID(id);
        sensorResult.setTemperature(temperature);
        sensorResult.setHumidity(humidity);
        time(&rawtime);
        timeinfo = localtime(&rawtime);

        strftime(timebuffer, 80, "%Y-%m-%d %T", timeinfo);
        sensorResult.setDate(timebuffer);
    }

    return sensorResult;
}

void SensorConnector::SaveToDatabase(SensorData data) {
    dbHelper->InsertSensorData(data);
}

void SensorConnector::StartCollecting() {
    zeroconf->AddListener("_json._udp");
    shouldWork = true;
    vector<ZeroconfService> devices;
    while (shouldWork) {
        BOOST_LOG_TRIVIAL(info) << "Looking for devices to collect data";
        zeroconf->ListDiscoveredServices("", devices);
        for (ZeroconfService device : devices) {
            //cout << "Get data from: " << device.ipv4_addresses.at(0).c_str() << ", " << device.port << endl;
            BOOST_LOG_TRIVIAL(info) << "Get data from: " << device.ipv4_addresses.at(0).c_str() << ", " << device.port;
            SensorData data = DownloadData(device.ipv4_addresses.at(0).c_str(), device.port);
            SaveToDatabase(data);
            //cout << "Added new data from: " << device.ipv4_addresses.at(0).c_str() << ", " << device.port << endl;
            BOOST_LOG_TRIVIAL(info) << "Added new data from: " << device.ipv4_addresses.at(0).c_str() << ", " <<
                                    device.port;
        }
        if (!devices.empty()) {
            //cout << "Saving done. Sleep for " << period << endl;
            BOOST_LOG_TRIVIAL(info) << "Saving done. Sleep for " << interval << " seconds";
            boost::this_thread::sleep_for(boost::chrono::seconds(interval));
        }
        else {
            BOOST_LOG_TRIVIAL(info) << "Nothing to do. Sleep for 10 seconds";
            boost::this_thread::sleep_for(boost::chrono::seconds(10));
        }
    }
}

void SensorConnector::StopCollecting() {
    shouldWork = false;
}

