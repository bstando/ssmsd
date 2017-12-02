#ifndef DB_HELPER_HPP
#define DB_HELPER_HPP
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <string>
#include "sqlite_modern_cpp.h"
#include <boost/thread/mutex.hpp>
#include "SensorData.hpp"

using namespace sqlite;
using namespace std;
class DBHelper {
	private:
	database *db;
    boost::mutex database_mutex;
	
	public:	
	//DBHelper();
	DBHelper(const string& filename);
	~DBHelper();
	bool InitializeDatabase();
	void InsertData(int sensorID, string date, float temperature, float humidity);
	void InsertSensorData(SensorData data);
	vector<SensorData> GetAllData();
	vector<SensorData> GetLastData(int limit);
	vector<SensorData> GetByDate(string date);
	vector<SensorData> GetBySensorID(int sensorID);
	vector<int> GetSensorIDs();
};

#endif
