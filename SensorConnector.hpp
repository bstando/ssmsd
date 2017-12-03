//
// Created by bartosz on 02.12.17.
//

#ifndef SSMSD_SENSORCONNECTOR_HPP
#define SSMSD_SENSORCONNECTOR_HPP


#include <boost/log/trivial.hpp>
#include "SensorData.hpp"
#include "Zeroconf.hpp"
#include "DBHelper.hpp"

#define BUFFSIZE 500

using namespace boost::log::trivial;
using namespace zeroconf;

class SensorConnector {
private:
    bool shouldWork;
    std::string dbFile;
    void SaveToDatabase(SensorData data);
    SensorData DownloadData(const char *host, int port);
    Zeroconf *zeroconf;
    int interval;
    DBHelper* dbHelper;
public:
    SensorConnector(Zeroconf &zeroconf, int interval, std::string dbFile);
    void StartCollecting();
    void StopCollecting();
};


#endif //SSMSD_SENSORCONNECTOR_HPP
