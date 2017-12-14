//
// Created by bartosz on 02.12.17.
//

#include <iostream>
#include <boost/log/core.hpp>
#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include <boost/program_options.hpp>
#include <boost/log/common.hpp>
#include "ServerHelper.hpp"
#include "DBHelper.hpp"

#include <cerrno>
#include <cstring>
#include <unistd.h>

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


ServerHelper::ServerHelper() = default;

void ServerHelper::ShowHelp() {
    std::cout << "Usage: " << appName << " -i interval | -h " << std::endl;
    std::cout << "Available options:" << std::endl;
    std::cout << "-h, --help - Show this info" << std::endl;
    std::cout << "-i, --interval time - Set interval (in seconds) between getting data from device" << std::endl;
    std::cout << "-d, --database database_file - Set database file" << std::endl;
    std::cout << "-l, --log log_directory - Set log files directory" << std::endl;
    std::cout << "-n, --name mdns_name - Set hostname for mDNS" << std::endl;
}

void ServerHelper::InitLog() {
    logging::add_common_attributes();
    std::string logFile = logDir + "/" + appName + "_%Y-%m-%d_%N.log";
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
                                                                                                      "%Y-%m-%d %H:%M:%S")
                                                     << "]: ["
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

void ServerHelper::ParseArguments(int argc, char **argv) {
    appName = boost::filesystem::basename(argv[0]);
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
            ("help,h", "Print help messages")
            ("interval,i", po::value<int>()->required(), "Time between next data fetch")
            ("database,d", po::value<std::string>(), "Database file")
            ("log,l", po::value<std::string>(), "Log directory")
            ("name,n", po::value<std::string>(), "mDNS hostname");

    po::variables_map vm;

    try {
        po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
        if (vm.count("help")) {
            ShowHelp();
            exit(SUCCESS);
        }
        po::notify(vm);
    }
    catch (boost::program_options::required_option &e) {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        ShowHelp();
        exit(ERROR_IN_COMMAND_LINE);
    }
    catch (boost::program_options::error &e) {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        ShowHelp();
        exit(ERROR_IN_COMMAND_LINE);
    }

    std::cout << "Interval = " << vm["interval"].as<int>() << std::endl;
    interval = vm["interval"].as<int>();
    if (interval <= 0) {
        std::cerr << "Interval must be greater than 0" << std::endl;
        exit(ERROR_UNHANDLED_EXCEPTION);
    }

    if (vm.count("database")) {
        std::cout << "Database = " << vm["database"].as<std::string>() << std::endl;
        dbFile = vm["database"].as<std::string>();
    } else {
        dbFile = "/var/ssmsd/database.db";
    }
    if (vm.count("log")) {
        std::cout << "Log = " << vm["log"].as<std::string>() << std::endl;
        logDir = vm["log"].as<std::string>();
    } else {
        logDir = "/var/log/ssmsd";
    }
    if (vm.count("name")) {
        std::cout << "mDNS Name = " << vm["name"].as<std::string>() << std::endl;
        hostname = vm["name"].as<std::string>();
    } else {
        char hname[HOST_NAME_MAX];
        gethostname(hname, HOST_NAME_MAX);
        hostname.assign(hname);
    }

}

std::string ServerHelper::GetHostname() {
    return hostname;
}

std::string ServerHelper::GetDBFileName() {
    return dbFile;
}

int ServerHelper::GetInterval() {
    return interval;
}

void ServerHelper::Daemonize() {
    /*
    BOOST_LOG_TRIVIAL(info) << "Going to first fork";
    if (pid_t pid = fork()) {
        if (pid > 0) {
            exit(0);
        } else {
            BOOST_LOG_TRIVIAL(fatal) << "First fork failed";
            exit(-1);
        }
    }
    BOOST_LOG_TRIVIAL(info) << "Passed first fork";
    setsid();
    chdir("/");
    umask(0);
    BOOST_LOG_TRIVIAL(info) << "setsid, chdir and umask done, going to second fork";
    if (pid_t pid = fork()) {
        if (pid > 0) {
            exit(0);
        } else {
            BOOST_LOG_TRIVIAL(fatal) << "Second fork failed";
            exit(-1);
        }
    }
    BOOST_LOG_TRIVIAL(info) << "Passed second fork, closing desriptors";
    close(0);
    close(1);
    close(2);
    */
    int daemonize = daemon(0, 0);
    if (daemonize < 0) {
        BOOST_LOG_TRIVIAL(fatal) << "Deamonize: Can't daemonize: " << strerror(errno);
        exit(ERROR_UNHANDLED_EXCEPTION);

    }
}

shared_ptr<DBHelper> ServerHelper::InitDatabase() {
    helper = make_shared<DBHelper>(dbFile);
    if(!helper->InitializeDatabase())
    {
        BOOST_LOG_TRIVIAL(fatal) << "Database Initialization failed: " << strerror(errno);
        exit(ERROR_UNHANDLED_EXCEPTION);
    }
    return helper;
}
