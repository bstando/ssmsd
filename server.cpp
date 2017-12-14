#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <ctime>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include "AppConnector.hpp"
#include "ServerHelper.hpp"

#include <thread>
#include <chrono>
#include "Zeroconf.hpp"

#include <boost/log/core.hpp>
#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <sys/stat.h>
#include "boost/program_options.hpp"
#include "boost/filesystem.hpp"
#include "SensorConnector.hpp"

using namespace std;
using namespace zeroconf;

using namespace boost::log::trivial;


void StartSensorConnector(Zeroconf &zeroconf, int interval, shared_ptr<DBHelper> dbFile)
{
    BOOST_LOG_TRIVIAL(info) << "Sensor Connector starting ...";
    SensorConnector sc(zeroconf,interval,dbFile);
    sc.StartCollecting();
}

void StartAppConnector(shared_ptr<DBHelper> dbFile)
{
    BOOST_LOG_TRIVIAL(info) << "App Connector starting ...";
    AppConnector ap(dbFile);
    ap.StartListening();
}

int main(int argc,char *argv[]) {

    ServerHelper serverHelper;
    serverHelper.ParseArguments(argc,argv);
    serverHelper.InitLog();
    shared_ptr<DBHelper> dbHelper = serverHelper.InitDatabase();
    serverHelper.Daemonize();
    BOOST_LOG_TRIVIAL(info) << "Daemon started";

    ZeroconfService collector;
    collector.name = serverHelper.GetHostname();
    collector.type = "_ssmsd._udp";
    collector.port = 9873;
    collector.domain = "local";

    Zeroconf zeroconf;
    zeroconf.AddService(collector);

    BOOST_LOG_TRIVIAL(info) << "Service added to avahi";
     
    thread scThread(StartSensorConnector,ref(zeroconf),serverHelper.GetInterval(),dbHelper);
    BOOST_LOG_TRIVIAL(info) << "Sensor Connector thread started";
    sleep(2);
    thread apThread(StartAppConnector,dbHelper);
    BOOST_LOG_TRIVIAL(info) << "App Connector thread started";

    BOOST_LOG_TRIVIAL(info) << "Server Started";

    apThread.join();
    scThread.join();
    return 0;
}
