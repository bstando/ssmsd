#ifndef SENSOR_HPP
#define SENSOR_HPP

#include <string>

class SensorData
{
	private:
	int id;
	int sensorID;
	std::string date;
	float temperature;
	float humidity;
	
	public:
	SensorData();
	~SensorData();
	int getID();
	void setID(int value);
	int getSensorID();
	void setSensorID(int value);
	std::string getDate();
	void setDate(std::string value);
	float getTemperature();
	void setTemperature(float value);
	float getHumidity();
	void setHumidity(float value);	
	

};


#endif
