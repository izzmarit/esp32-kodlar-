/**
 * Kuluçka Makinesi Kontrol Sistemi - Veri Modülü
 * 
 * Bu modül, LittleFS kullanarak veri saklama ve okuma işlemlerini yönetir.
 * Sıcaklık, nem ve diğer verilerin kayıt, görüntüleme ve analizi sağlanır.
 */

 #ifndef DATA_MODULE_H
 #define DATA_MODULE_H
 
 #include <Arduino.h>
 #include "../config/GlobalDefinitions.h"
 #include "../include/AppConfig.h"

 extern IncubationSettings settings;
 extern MotorSettings motorSettings;
 extern bool InitializeFilesystem();
 
 // Fonksiyon prototipleri
 bool DataInit();
 void DataUpdate();
 bool CheckAndFormatFileSystem();
 void LoadPowerOutages();
 void LoadTempHumRecords();
 
 // Ayar yönetimi
 bool SaveSettings(IncubationSettings& settings);
 IncubationSettings LoadSettings();
 bool SaveMotorSettings(MotorSettings& settings);
 MotorSettings LoadMotorSettings();
 bool SaveSensorCalibration(SensorCalibration calibration);
 SensorCalibration LoadSensorCalibration();
 
 // Veri kayıt ve erişim
 bool LogTempHumidity(float temp, float humidity, bool heaterOn, bool humidifierOn);
 bool LogPowerOutage(uint32_t startTime, uint32_t endTime);
 void SetPowerOnTime(uint32_t time);
 uint32_t GetLastPowerOnTime();
 
 // Veri analizi
 bool GetTempHumidityRecords(DataRecord* records, int maxCount, int* actualCount);
 bool GetPowerOutages(PowerOutage* outages, int maxCount, int* actualCount);
 bool GetTempHumidityStats(float* minTemp, float* maxTemp, float* avgTemp,
                           float* minHum, float* maxHum, float* avgHum);
 bool GetLatestTempHumidity(float* temp, float* humidity);
 bool GetTimeRangeStats(uint32_t startTime, uint32_t endTime, 
                       float* avgTemp, float* avgHum, 
                       float* minTemp, float* maxTemp, 
                       float* minHum, float* maxHum, 
                       int* heaterOnPercentage);
 
 // Grafik veri hazırlama
 bool GetTempGraphData(float* data, uint32_t* times, int maxPoints, int* actualPoints);
 bool GetHumGraphData(float* data, uint32_t* times, int maxPoints, int* actualPoints);
 
 // Veri yönetimi
 bool ClearAllData();
 bool BackupData();
 bool RestoreData();
 
 #endif // DATA_MODULE_H