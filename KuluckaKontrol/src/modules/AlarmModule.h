/**
 * Kuluçka Makinesi Kontrol Sistemi - Alarm Modülü
 * 
 * Bu modül, alarm durumlarını ve uyarı eşiklerini yönetir.
 */

 #ifndef ALARM_MODULE_H
 #define ALARM_MODULE_H
 
 #include <Arduino.h>
 #include "../config/GlobalDefinitions.h"
 #include "../include/AppConfig.h"
 
 // Fonksiyon prototipleri
 bool AlarmInit();
 void AlarmUpdate();
 AlarmStatus AlarmGetStatus();
 void AlarmClear();
 void AlarmSetEnabled(bool enabled);
 bool AlarmIsEnabled();
 void AlarmSetThresholds(AlarmThresholds thresholds);
 AlarmThresholds AlarmGetThresholds();
 AlarmThresholds LoadAlarmThresholds();
 bool SaveAlarmThresholds(AlarmThresholds thresholds);
 bool AlarmTest();
 void AlarmLog(AlarmType type, const String& message);
 int AlarmGetHistoryCount();
 AlarmStatus AlarmGetHistory(int index);
 
 #endif // ALARM_MODULE_H