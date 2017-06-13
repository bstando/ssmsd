#include "sensor.hpp"


SensorData::SensorData()
{
}
SensorData::~SensorData()
{
}
int SensorData::getID()
{
	return id;
}
void SensorData::setID(int value)
{
	id  = value;
}
int SensorData::getSensorID()
{
	return sensorID;
}
void SensorData::setSensorID(int value)
{
	sensorID = value;
}
std::string SensorData::getDate()
{
	return date;
}
void SensorData::setDate(std::string value)
{
	date = value;
}
float SensorData::getTemperature()
{
	return temperature;
}
void SensorData::setTemperature(float value)
{
	temperature = value;
}
float SensorData::getHumidity()
{
	return humidity;
}
void SensorData::setHumidity(float value)
{
	humidity = value;
}	
