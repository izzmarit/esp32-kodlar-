/**
 * Kuluçka Makinesi Kontrol Sistemi - Alarm Modülü
 */

 #include "AlarmModule.h"
 #include "DisplayModule.h"  // DisplayShowNotification için
 #include "WifiModule.h"     // WifiSendData için
 #include <ArduinoJson.h>
 #include "TimeModule.h"
 #include "SensorModule.h"
 #include <LittleFS.h>
 
 // Sabit değerler
 #define ALARM_FILE "/alarm_thresholds.json"
 static const char* TAG = "ALARM";
 
 // Alarm durumu
 static AlarmStatus currentAlarm = {false, ALARM_NONE, 0, ""};
 
 // Alarm geçmişi
 static AlarmStatus alarmHistory[MAX_ALARM_HISTORY];
 static int alarmHistoryCount = 0;
 
 // Alarm eşikleri
 extern AlarmThresholds alarmThresholds;
 
 // Alarm aktif mi?
 static bool alarmEnabled = true;
 
 // Alarm güncellemesi için son kontrol zamanı
 static unsigned long lastAlarmCheck = 0;
 
 bool AlarmInit() {
     // Alarm eşiklerini yükle
     alarmThresholds = LoadAlarmThresholds();
     alarmEnabled = alarmThresholds.alarmEnabled;
     
     LOG_INFO(TAG, "Alarm modülü başlatıldı");
     return true;
 }
 
 void AlarmUpdate() {
     // Alarm devre dışı ise güncelleme yapma
     if (!alarmEnabled) {
         return;
     }
     
     unsigned long currentTime = millis();
     
     // Her 5 saniyede bir alarm kontrolü yap
     if (currentTime - lastAlarmCheck < 5000) {
         return;
     }
     
     lastAlarmCheck = currentTime;
     
     // Sensör verilerini oku
     SensorData sensorData = GetSensorData();
     
     // Sensör hatası kontrolü
     if (!sensorData.sensor1Valid && !sensorData.sensor2Valid) {
         if (!currentAlarm.active || currentAlarm.type != ALARM_SENSOR_ERROR) {
             AlarmLog(ALARM_SENSOR_ERROR, "Tüm sensörler hatalı!");
         }
         return;
     }
     
     // Sıcaklık ve nem değerleri
     float temp = sensorData.avgTemperature;
     float humidity = sensorData.avgHumidity;
     
     // Yüksek sıcaklık alarmı
     if (temp > alarmThresholds.highTempThreshold) {
         if (!currentAlarm.active || currentAlarm.type != ALARM_HIGH_TEMP) {
             AlarmLog(ALARM_HIGH_TEMP, "Yüksek sıcaklık: " + String(temp, 1) + "C");
         }
         return;
     }
     
     // Düşük sıcaklık alarmı
     if (temp < alarmThresholds.lowTempThreshold) {
         if (!currentAlarm.active || currentAlarm.type != ALARM_LOW_TEMP) {
             AlarmLog(ALARM_LOW_TEMP, "Düşük sıcaklık: " + String(temp, 1) + "C");
         }
         return;
     }
     
     // Yüksek nem alarmı
     if (humidity > alarmThresholds.highHumThreshold) {
         if (!currentAlarm.active || currentAlarm.type != ALARM_HIGH_HUMIDITY) {
             AlarmLog(ALARM_HIGH_HUMIDITY, "Yüksek nem: %" + String(humidity, 1));
         }
         return;
     }
     
     // Düşük nem alarmı
     if (humidity < alarmThresholds.lowHumThreshold) {
         if (!currentAlarm.active || currentAlarm.type != ALARM_LOW_HUMIDITY) {
             AlarmLog(ALARM_LOW_HUMIDITY, "Düşük nem: %" + String(humidity, 1));
         }
         return;
     }
     
     // Sensörler arası sıcaklık farkı kontrolü
     if (sensorData.sensor1Valid && sensorData.sensor2Valid) {
         float tempDiff = abs(sensorData.temperature1 - sensorData.temperature2);
         if (tempDiff > alarmThresholds.tempDiffThreshold) {
             if (!currentAlarm.active || currentAlarm.type != ALARM_TEMP_DIFF) {
                 AlarmLog(ALARM_TEMP_DIFF, "Sıcaklık farkı yüksek: " + String(tempDiff, 1) + "C");
             }
             return;
         }
         
         // Sensörler arası nem farkı kontrolü
         float humDiff = abs(sensorData.humidity1 - sensorData.humidity2);
         if (humDiff > alarmThresholds.humDiffThreshold) {
             if (!currentAlarm.active || currentAlarm.type != ALARM_HUM_DIFF) {
                 AlarmLog(ALARM_HUM_DIFF, "Nem farkı yüksek: %" + String(humDiff, 1));
             }
             return;
         }
     }
     
     // Eğer aktif alarm varsa ve hiçbir kontrol tarafından tetiklenmemişse, alarmı temizle
     if (currentAlarm.active) {
         AlarmClear();
     }
 }
 
 AlarmStatus AlarmGetStatus() {
     return currentAlarm;
 }
 
 void AlarmClear() {
     currentAlarm.active = false;
     currentAlarm.type = ALARM_NONE;
     currentAlarm.message = "";
     
     LOG_INFO(TAG, "Alarm temizlendi");
 }
 
 void AlarmSetEnabled(bool enabled) {
     alarmEnabled = enabled;
     alarmThresholds.alarmEnabled = enabled;
     SaveAlarmThresholds(alarmThresholds);
     
     LOG_INFO(TAG, "Alarm %s", enabled ? "etkinleştirildi" : "devre dışı bırakıldı");
 }
 
 bool AlarmIsEnabled() {
     return alarmEnabled;
 }
 
 void AlarmSetThresholds(AlarmThresholds thresholds) {
     alarmThresholds = thresholds;
     SaveAlarmThresholds(thresholds);
     
     LOG_INFO(TAG, "Alarm eşikleri güncellendi");
 }
 
 AlarmThresholds AlarmGetThresholds() {
     return alarmThresholds;
 }
 
 bool AlarmTest() {
     // Test alarmı oluştur
     AlarmLog(ALARM_HIGH_TEMP, "Test alarmı");
     return true;
 }
 
 void AlarmLog(AlarmType type, const String& message) {
    // Mevcut alarmı güncelle
    currentAlarm.active = true;
    currentAlarm.type = type;
    currentAlarm.startTime = millis();
    currentAlarm.message = message;
    
    // Alarm geçmişine ekle - döngüsel tampon kullanarak
    static int alarmHistoryIndex = 0; // Döngüsel tampon için indeks

    // En son alarmı geçmişe ekle
    alarmHistory[alarmHistoryIndex] = currentAlarm;

    // İndeksi güncelle (döngüsel olarak)
    alarmHistoryIndex = (alarmHistoryIndex + 1) % MAX_ALARM_HISTORY;

    // Geçmiş sayısını güncelle (maksimum değeri aşmadan)
    if (alarmHistoryCount < MAX_ALARM_HISTORY) {
        alarmHistoryCount++;
    }
    
    // Log mesajı
    LOG_ERROR(TAG, "ALARM: %s", message.c_str());
    
    // Ekranda alarm bildirimi göster
    if (DisplayIsInitialized()) {
        DisplayShowNotification("ALARM: " + message);
    } else {
        LOG_ERROR(TAG, "ALARM (ekran gösterilemiyor): %s", message.c_str());
    }
    
    // WiFi bağlantısı varsa, Android uygulamasına bildirim gönder
    if (WifiIsClientConnected()) {
        // JSON formatında detaylı alarm bilgisi oluştur
        StaticJsonDocument<512> alarmDoc;
        
        // Mesaj boyutunu kontrol et
        String truncatedMessage = message;
        if (truncatedMessage.length() > 200) {
            truncatedMessage = truncatedMessage.substring(0, 200) + "...";
        }
        
        alarmDoc["notification"] = "alarm";
        alarmDoc["type"] = (int)type;
        alarmDoc["message"] = truncatedMessage;
        alarmDoc["time"] = GetUnixTime();
        
        // Alarm kategorisi ve şiddetini belirle
        const char* categories[] = {
            "unknown",
            "temperature", // ALARM_HIGH_TEMP
            "temperature", // ALARM_LOW_TEMP
            "humidity",    // ALARM_HIGH_HUMIDITY
            "humidity",    // ALARM_LOW_HUMIDITY
            "system",      // ALARM_SENSOR_ERROR
            "system",      // ALARM_POWER_OUTAGE
            "sensor",      // ALARM_TEMP_DIFF
            "sensor",      // ALARM_HUM_DIFF
        };
        
        const char* severities[] = {
            "unknown",
            "high",        // ALARM_HIGH_TEMP
            "low",         // ALARM_LOW_TEMP
            "high",        // ALARM_HIGH_HUMIDITY
            "low",         // ALARM_LOW_HUMIDITY
            "critical",    // ALARM_SENSOR_ERROR
            "critical",    // ALARM_POWER_OUTAGE
            "medium",      // ALARM_TEMP_DIFF
            "medium",      // ALARM_HUM_DIFF
        };
        
        // Alarm türünün sınırlarını kontrol et
        if (type > ALARM_NONE && type <= ALARM_HUM_DIFF) {
            alarmDoc["category"] = categories[type];
            alarmDoc["severity"] = severities[type];
        } else {
            alarmDoc["category"] = "unknown";
            alarmDoc["severity"] = "medium";
        }
        
        String alarmJson;
        serializeJson(alarmDoc, alarmJson);
        
        // Tüm bağlı istemcilere gönder
        WifiSendDataToAll(alarmJson);
    }
}
 
 int AlarmGetHistoryCount() {
     return alarmHistoryCount;
 }
 
 AlarmStatus AlarmGetHistory(int index) {
     if (index >= 0 && index < alarmHistoryCount) {
         return alarmHistory[index];
     }
     
     // Geçersiz indeks
     AlarmStatus emptyAlarm = {false, ALARM_NONE, 0, ""};
     return emptyAlarm;
 }
 
 // Alarm eşiklerini yükleme fonksiyonu
 AlarmThresholds LoadAlarmThresholds() {
     AlarmThresholds thresholds;
     
     // Varsayılan değerleri ayarla
     thresholds.alarmEnabled = true;
     thresholds.highTempThreshold = 38.5;
     thresholds.lowTempThreshold = 36.5;
     thresholds.highHumThreshold = 80.0;
     thresholds.lowHumThreshold = 50.0;
     thresholds.tempDiffThreshold = 2.0;
     thresholds.humDiffThreshold = 10.0;
     
     // Dosyadan yükleme
     if (LittleFS.exists(ALARM_FILE)) {
         File file = LittleFS.open(ALARM_FILE, "r");
         if (file) {
             StaticJsonDocument<256> doc;
             DeserializationError error = deserializeJson(doc, file);
             file.close();
             
             if (!error) {
                 thresholds.alarmEnabled = doc["enabled"] | true;
                 thresholds.highTempThreshold = doc["high_temp"] | 38.5;
                 thresholds.lowTempThreshold = doc["low_temp"] | 36.5;
                 thresholds.highHumThreshold = doc["high_hum"] | 80.0;
                 thresholds.lowHumThreshold = doc["low_hum"] | 50.0;
                 thresholds.tempDiffThreshold = doc["temp_diff"] | 2.0;
                 thresholds.humDiffThreshold = doc["hum_diff"] | 10.0;
                 
                 LOG_INFO(TAG, "Alarm eşikleri dosyadan yüklendi");
             } else {
                 LOG_ERROR(TAG, "Alarm eşikleri dosyası ayrıştırma hatası: %s", error.c_str());
             }
         }
     } else {
         LOG_INFO(TAG, "Alarm eşikleri dosyası bulunamadı, varsayılanlar kullanılıyor");
         SaveAlarmThresholds(thresholds); // Varsayılanları kaydet
     }
     
     return thresholds;
 }
 
 // Alarm eşiklerini kaydetme fonksiyonu
 bool SaveAlarmThresholds(AlarmThresholds thresholds) {
     File file = LittleFS.open(ALARM_FILE, "w");
     if (!file) {
         LOG_ERROR(TAG, "Alarm eşikleri dosyası oluşturulamadı!");
         return false;
     }
     
     StaticJsonDocument<256> doc;
     doc["enabled"] = thresholds.alarmEnabled;
     doc["high_temp"] = thresholds.highTempThreshold;
     doc["low_temp"] = thresholds.lowTempThreshold;
     doc["high_hum"] = thresholds.highHumThreshold;
     doc["low_hum"] = thresholds.lowHumThreshold;
     doc["temp_diff"] = thresholds.tempDiffThreshold;
     doc["hum_diff"] = thresholds.humDiffThreshold;
     
     if (serializeJson(doc, file) == 0) {
         LOG_ERROR(TAG, "Alarm eşikleri yazılamadı!");
         file.close();
         return false;
     }
     
     file.close();
     LOG_INFO(TAG, "Alarm eşikleri kaydedildi");
     return true;
 }