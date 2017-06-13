#ifndef DB_HELPER_HPP
#define DB_HELPER_HPP
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <string>
#include "sqlite_modern_cpp.h"
#include <boost/thread/mutex.hpp>
#include "sensor.hpp"

using namespace  sqlite;
using namespace std;
class DBHelper {
	private:
	database *db;
    boost::mutex database_mutex;
	
	public:	
	//DBHelper();
	DBHelper(const string& filename);
	~DBHelper();
	bool initializeDatabase();
	void insertData(int sensorID, string date, float temperature, float humidity);
	void insertSensorData(SensorData data);
	vector<SensorData> getAllData();
	vector<SensorData> getLastData(int limit);
	vector<SensorData> getByDate(string date);
	vector<SensorData> getBySensorID(int sensorID);
	vector<int> getSensorIDs();
};

#endif
