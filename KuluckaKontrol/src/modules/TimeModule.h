/**
 * Kuluçka Makinesi Kontrol Sistemi - Zaman Modülü
 * 
 * Bu modül, DS3231 RTC kullanarak zaman yönetimini sağlar.
 */

 #ifndef TIME_MODULE_H
 #define TIME_MODULE_H
 
 #include <Arduino.h>
 #include "../config/GlobalDefinitions.h"
 #include "../include/AppConfig.h"
 
 // Fonksiyon prototipleri
 bool TimeInit();
 void TimeUpdate();
 TimeInfo GetCurrentTime();
 uint32_t GetUnixTime();
 bool SetTime(TimeInfo timeInfo);
 bool SetUnixTime(uint32_t unixTime);
 void CheckPowerOutage();
 int GetPowerOutageCount();
 PowerOutage GetPowerOutage(int index);
 uint32_t GetIncubationDay(uint32_t startTime);
 uint32_t CalculateEndTime(uint32_t startTime, int durationDays);
 TimeInfo UnixTimeToTimeInfo(uint32_t unixTime);
 bool SyncTimeWithNTP();
 
 #endif // TIME_MODULE_H