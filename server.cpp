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
//#include "include/ArduinoJson.h"
#include <sqlite3.h>

#include <ctime>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

//#include "db_helper.cpp"
#include "DataExchangeServer.hpp"
//#include "sensor.cpp"
//#include "BroadcastService.cpp"

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

#define PORT 9872
#define BUFSIZE 512

using namespace std;
using namespace zeroconf;


namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

using namespace logging::trivial;
namespace
{
    const size_t ERROR_IN_COMMAND_LINE = 1;
    const size_t SUCCESS = 0;
    const size_t ERROR_UNHANDLED_EXCEPTION = 2;
}


boost::mutex service_mutex;


void error(String msg) {
    perror(msg.c_str());
    //exit(1);
}


SensorData downloadData(const char *host, int port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s, j;
    size_t len;
    ssize_t nread;
    char buf[BUFSIZE];
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

    bzero(buf, BUFSIZE);
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

    bzero(buf, BUFSIZE);

    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;

    setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(struct timeval));
    nread = read(sfd, buf, BUFSIZE);
    if (nread == -1) {
        //perror("read");
        BOOST_LOG_TRIVIAL(fatal) << "Error during packet write";
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


void saveToDatabase(const char *host, int port, const string& dbFile) {
    SensorData data = downloadData(host, port);
    if (data.getSensorID() != -1) {
        DBHelper helper(dbFile);
        helper.initializeDatabase();
        helper.insertSensorData(data);
    }
}


#if 0
/*
 * Structs exported from in.h
 */

/* Internet address */
struct in_addr
{
    unsigned int s_addr;
};

/* Internet style socket address */
struct sockaddr_in
{
    unsigned short int sin_family; /* Address family */
    unsigned short int sin_port;   /* Port number */
    struct in_addr sin_addr;	 /* IP address */
    unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent
{
    char    *h_name;        /* official name of host */
    char    **h_aliases;    /* alias list */
    int     h_addrtype;     /* host address type */
    int     h_length;       /* length of address */
    char    **h_addr_list;  /* list of addresses */
}
#endif

/*
 * error - wrapper for perror
 */


void startListen(const string& dbFile) {
    DataExchangeServer se(dbFile);
    se.startListening();
}

void askDevices(Zeroconf &service, int period,const string& dbFile) {
    //cout << "Got into thread" << endl;
    vector<ZeroconfService> devices;
    while (true) {
        service.listDiscoveredServices("", devices);
        for (ZeroconfService device : devices) {
            //cout << "Get data from: " << device.ipv4_addresses.at(0).c_str() << ", " << device.port << endl;
            BOOST_LOG_TRIVIAL(info) << "Get data from: " << device.ipv4_addresses.at(0).c_str() << ", " << device.port;
            saveToDatabase(device.ipv4_addresses.at(0).c_str(), device.port, dbFile);
            //cout << "Added new data from: " << device.ipv4_addresses.at(0).c_str() << ", " << device.port << endl;
            BOOST_LOG_TRIVIAL(info) << "Added new data from: " << device.ipv4_addresses.at(0).c_str() << ", " <<
                                    device.port;
        }
        if (devices.size() > 0) {
            //cout << "Saving done. Sleep for " << period << endl;
            BOOST_LOG_TRIVIAL(info) << "Saving done. Sleep for " << period;
            this_thread::sleep_for(chrono::seconds(period));
        }
        else {
            this_thread::sleep_for(chrono::seconds(1));
        }
    }

}


void initLog(std::string logDir) {


    std::string logFile = logDir + "/ssmd_%Y-%m-%d_%N.log";
    logging::add_file_log
            (
                    keywords::file_name = logFile,                                        /*< file name pattern >*/
                    keywords::open_mode = (std::ios::out | std::ios::app),
                    keywords::rotation_size =
                            10 * 1024 * 1024,                                   /*< rotate files every 10 MiB... >*/
                    keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0,
                                                                                        0), /*< ...or at midnight >*/
                    keywords::format = (expr::stream << "[" <<
                                        expr::format_date_time<boost::posix_time::ptime>("TimeStamp",
                                                                                         "%Y-%m-%d %H:%M:%S") << "]: ["
                                        << boost::log::trivial::severity
                                        << "]\t"
                                        << expr::smessage),                                 /*< log record format >*/
                    keywords::auto_flush = true
            );

    logging::core::get()->set_filter
            (
                    logging::trivial::severity > logging::trivial::debug
            );
}

void showHelp(const std::string appName) {
    cout << "Usage: " << appName << " -i interval | -h " << std::endl;
    cout << "Avaiable options:" << std::endl;
    cout << "-h, --help - Show this info" << std::endl;
    cout << "-i, --interval time - Set interval (in seconds) between getting data from device" << std::endl;
    cout << "-d, --database database_file - Set database file" << std::endl;
    cout << "-l, --log log_directory - Set log files directory" << std::endl;
}

int main(int argc,char *argv[]) {

    /*
    if(argc < 2)
    {
        cerr << "Wrong number of parameters. Usage: " << argv[0] << " period" << endl;
        return -1;
    }
    //string::size_type st;
    int period = atoi(argv[1]);
    if (period < 1)
    {
        cerr << "Specified time is wrong. Value must be above 0" << endl;
        return -1;
    }
    */

    std::string appName = boost::filesystem::basename(argv[0]);
    std::string logDir;
    std::string dbFile;
    int interval = 60;

    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
            ("help,h", "Print help messages")
            ("interval,i", po::value<int>()->required(), "Time between next data fetch")
            ("database,d", po::value<std::string>(), "Database file")
            ("log,l", po::value<std::string>(), "Log directory");

    po::variables_map vm;

    try {
        po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
        if (vm.count("help")) {
            showHelp(appName);
            return SUCCESS;
        }
        po::notify(vm);
    }
    catch (boost::program_options::required_option &e) {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        showHelp(appName);
        return ERROR_IN_COMMAND_LINE;
    }
    catch (boost::program_options::error &e) {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        showHelp(appName);
        return ERROR_IN_COMMAND_LINE;
    }

    std::cout << "Interval = " << vm["interval"].as<int>() << std::endl;
    interval = vm["interval"].as<int>();
    if (interval <= 0) {
        std::cerr << "Interval must be greater than 0" << endl;
        return ERROR_UNHANDLED_EXCEPTION;
    }

    if (vm.count("database")) {
        std::cout << "Database = " << vm["database"].as<std::string>() << std::endl;
        dbFile = vm["database"].as<std::string>();
    }
    else {
        dbFile = "/usr/local/ssmd/var/database.db";
    }
    if (vm.count("log")) {
        std::cout << "Log = " << vm["log"].as<std::string>() << std::endl;
        logDir = vm["log"].as<std::string>();
    }
    else {
        logDir = "/var/log/ssmd";
    }
    logging::add_common_attributes();
    initLog(logDir);

    if (pid_t pid = fork()) {
        if (pid > 0) {
            exit(0);
        }
        else {
            BOOST_LOG_TRIVIAL(fatal) << "First fork failed";
            return 1;
        }
    }
    setsid();
    chdir("/");
    umask(0);
    if (pid_t pid = fork()) {
        if (pid > 0) {
            exit(0);
        }
        else {
            BOOST_LOG_TRIVIAL(fatal) << "Second fork failed";
            return 1;
        }
    }
    close(0);
    close(1);
    close(2);

    Zeroconf zeroconf_service;
    ZeroconfService collector;
    collector.name = "Collector";
    collector.type = "_ssmsd._udp";
    collector.port = 9873;
    collector.domain = "local";
    zeroconf_service.addService(collector);
    zeroconf_service.addListener("_json._udp");
    thread server(startListen, dbFile);
    sleep(2);
    thread listener(askDevices, ref(zeroconf_service), interval, dbFile);
    BOOST_LOG_TRIVIAL(info) << "Server Started";

    server.join();
    listener.join();
    return 0;
}
