/**
 * Kuluçka Makinesi Kontrol Sistemi - Sensör Modülü
 * 
 * Bu modül, SHT31 sensörlerini kullanarak sıcaklık ve nem ölçümlerini yönetir.
 */

 #ifndef SENSOR_MODULE_H
 #define SENSOR_MODULE_H
 
 #include <Arduino.h>
 #include "../config/GlobalDefinitions.h"
 #include "../include/AppConfig.h"
 
 // Fonksiyon prototipleri
 bool SensorInit();
 bool SensorUpdate();
 SensorData GetSensorData();
 bool SetCalibration(SensorCalibration calibration);
 SensorCalibration GetCalibration();
 bool PerformSemiAutoCalibration(float refTemp, float refHum);
 bool IsSensorDataValid();
 float GetTemperature();
 float GetHumidity();
 bool CheckSensorsIntegrity();
 
 // Sensör güvenilirliği bilgileri
 int GetSensor1ErrorCount();
 int GetSensor2ErrorCount();
 int GetSensor1RecoveryAttempts();
 int GetSensor2RecoveryAttempts();
 void ResetSensorErrorCounters();
 
 #endif // SENSOR_MODULE_H