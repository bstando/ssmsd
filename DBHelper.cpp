#include <boost/log/trivial.hpp>
#include "DBHelper.hpp"

using namespace std;
using namespace boost::log::trivial;
/*
DBHelper::DBHelper() {
    db = new database("/usr/local/smss/var/database.db");
}
*/

DBHelper::DBHelper(string filename) {
    boost::mutex::scoped_lock lock(database_mutex);
    try {
        BOOST_LOG_TRIVIAL(info) << "Initializing DBHelper object";
        BOOST_LOG_TRIVIAL(info) << filename;
        sqlite_config config;
        config.flags = OpenFlags::READWRITE | OpenFlags::CREATE;
        db = new database(filename,config);
        lock.unlock();
        InitializeDatabase();
        BOOST_LOG_TRIVIAL(info) << "DBHelper object initialized";
    }
    catch (exception &e) {
        BOOST_LOG_TRIVIAL(fatal) << e.what();
        lock.unlock();
    }
}

DBHelper::~DBHelper() {
	delete db;
}

bool DBHelper::InitializeDatabase() {
	BOOST_LOG_TRIVIAL(info) << "Trying to initialize database";
	boost::mutex::scoped_lock lock(database_mutex);
	try {
		*db << "CREATE TABLE IF NOT EXISTS sensor "
				"( _id integer primary key autoincrement not null,"
				"sensorID int,"
				"date string,"
				"temperature float,"
				"humidity float"
				");";
		lock.unlock();
		BOOST_LOG_TRIVIAL(info) << "Database initialized";
		return true;
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
		return false;
	}
	lock.unlock();
	return false;
}

void DBHelper::InsertData(int sensorID, string date, float temperature, float humidity) {
	boost::mutex::scoped_lock lock(database_mutex);
	try {
		*db << "INSERT INTO sensor (sensorID, date, temperature,humidity) values (?,?,?,?);"
			<< sensorID
			<< date
			<< temperature
			<< humidity;
		lock.unlock();
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
}

void DBHelper::InsertSensorData(SensorData data) {
    BOOST_LOG_TRIVIAL(info) << "Trying to insert data to database";
    BOOST_LOG_TRIVIAL(info) << "SensorID: " << data.getSensorID() << ", date: " << data.getDate() << ", temperature: " << data.getTemperature() << ", humidity: " << data.getHumidity();
	boost::mutex::scoped_lock lock(database_mutex);
	try {
		*db << "INSERT INTO sensor (sensorID, date, temperature,humidity) values (?,?,?,?);"
			<< data.getSensorID()
			<< data.getDate()
			<< data.getTemperature()
			<< data.getHumidity();
		lock.unlock();
        BOOST_LOG_TRIVIAL(info) << "Row added";
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
}

vector<SensorData> DBHelper::GetAllData() {
    BOOST_LOG_TRIVIAL(info) << "Trying to get all data from database";
	vector<SensorData> *retval = new vector<SensorData>();
	boost::mutex::scoped_lock lock(database_mutex);
	try {
		*db << "SELECT * FROM sensor;"
			>> [&](int id, int sensorID, string date, float temperature, float humidity) {
				SensorData *sensor = new SensorData();
				sensor->setID(id);
				sensor->setSensorID(sensorID);
				sensor->setDate(date);
				sensor->setTemperature(temperature);
				sensor->setHumidity(humidity);
				retval->push_back(*sensor);
			};
		lock.unlock();
        BOOST_LOG_TRIVIAL(info) << "Got " << retval->size() << " rows from database";
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
	return *retval;
}

vector<SensorData> DBHelper::GetLastData(int limit) {
    BOOST_LOG_TRIVIAL(info) << "Trying to get " << limit << " rows from database";
	vector<SensorData> *retval = new vector<SensorData>();
	boost::mutex::scoped_lock lock(database_mutex);
	try {
		string statement = "SELECT * FROM sensor ORDER BY date DESC LIMIT " + to_string(limit) + ";";
		*db << statement
			>> [&](int id, int sensorID, string date, float temperature, float humidity) {
				SensorData *sensor = new SensorData();
				sensor->setID(id);
				sensor->setSensorID(sensorID);
				sensor->setDate(date);
				sensor->setTemperature(temperature);
				sensor->setHumidity(humidity);
				retval->push_back(*sensor);
			};
		lock.unlock();
        BOOST_LOG_TRIVIAL(info) << "Got " << retval->size() << " rows from database";
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
	return *retval;
}

vector<SensorData> DBHelper::GetByDate(string date) {
    BOOST_LOG_TRIVIAL(info) << "Trying to get all rows since " << date <<  " from database";
	vector<SensorData> *retval = new vector<SensorData>();
	boost::mutex::scoped_lock lock(database_mutex);
	try {
		*db << "SELECT * FROM sensor WHERE date >=?;" << date
			>> [&](int id, int sensorID, string date, float temperature, float humidity) {
				SensorData *sensor = new SensorData();
				sensor->setID(id);
				sensor->setSensorID(sensorID);
				sensor->setDate(date);
				sensor->setTemperature(temperature);
				sensor->setHumidity(humidity);
				retval->push_back(*sensor);
			};
		lock.unlock();
        BOOST_LOG_TRIVIAL(info) << "Got " << retval->size() << " rows from database";
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
	return *retval;
}

vector<SensorData> DBHelper::GetBySensorID(int sensorID) {
    BOOST_LOG_TRIVIAL(info) << "Trying to get all rows for sensorID " << sensorID << " from database";
	vector<SensorData> *retval = new vector<SensorData>();
	boost::mutex::scoped_lock lock(database_mutex);
	try {
		*db << "SELECT * FROM sensor WHERE sensorID >=?;" << sensorID
			>> [&](int id, int sensorID, string date, float temperature, float humidity) {
				SensorData *sensor = new SensorData();
				sensor->setID(id);
				sensor->setSensorID(sensorID);
				sensor->setDate(date);
				sensor->setTemperature(temperature);
				sensor->setHumidity(humidity);
				retval->push_back(*sensor);
			};
		lock.unlock();
        BOOST_LOG_TRIVIAL(info) << "Got " << retval->size() << " rows from database";
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
	return *retval;
}

vector<int> DBHelper::GetSensorIDs() {
    BOOST_LOG_TRIVIAL(info) << "Trying to get sensorsID's from database";
	vector<int> *retval = new vector<int>();
	boost::mutex::scoped_lock lock(database_mutex);
	try {
		*db << "SELECT DISTINCT sensorID FROM sensor;"
			>> [&](int sensorID) {
				retval->push_back(sensorID);
			};
		lock.unlock();
        BOOST_LOG_TRIVIAL(info) << "Got " << retval->size() << " rows from database";
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
	return *retval;
}








