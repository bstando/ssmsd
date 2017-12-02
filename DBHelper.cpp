#include <boost/log/trivial.hpp>
#include "DBHelper.hpp"

using namespace std;
using namespace boost::log::trivial;
/*
DBHelper::DBHelper() {
    db = new database("/usr/local/smss/var/database.db");
}
*/

DBHelper::DBHelper(const string &filename) {
    db = new database(filename);
}

DBHelper::~DBHelper() {
	delete db;
}

bool DBHelper::InitializeDatabase() {
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
	boost::mutex::scoped_lock lock(database_mutex);
	try {
		*db << "INSERT INTO sensor (sensorID, date, temperature,humidity) values (?,?,?,?);"
			<< data.getSensorID()
			<< data.getDate()
			<< data.getTemperature()
			<< data.getHumidity();
		lock.unlock();
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
}

vector<SensorData> DBHelper::GetAllData() {
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
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
	return *retval;
}

vector<SensorData> DBHelper::GetLastData(int limit) {
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
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
	return *retval;
}

vector<SensorData> DBHelper::GetByDate(string date) {
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
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
	return *retval;
}

vector<SensorData> DBHelper::GetBySensorID(int sensorID) {
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
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
	return *retval;
}

vector<int> DBHelper::GetSensorIDs() {
	vector<int> *retval = new vector<int>();
	boost::mutex::scoped_lock lock(database_mutex);
	try {
		*db << "SELECT DISTINCT sensorID FROM sensor;"
			>> [&](int sensorID) {
				retval->push_back(sensorID);
			};
		lock.unlock();
	}
	catch (exception &e) {
		lock.unlock();
		BOOST_LOG_TRIVIAL(fatal) << e.what() << endl;
	}
	return *retval;
}








