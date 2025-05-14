/**
 * Kuluçka Makinesi Kontrol Sistemi - Veri Modülü
 */

 #include "DataModule.h"
 #include <LittleFS.h>
 #include <ArduinoJson.h>
 #include "TimeModule.h"
 #include "SensorModule.h"
 #include "ControlModule.h"
 
 static const char* TAG = "DATA";
 
 // Dosya yolları tanımlı "FS_*" olarak GlobalDefinitions.h içerisinde

 // Kuluçka ve motor ayarları için global değişkenler
 extern IncubationSettings settings;
 extern MotorSettings motorSettings;
 
 // Veri kaydı için kullanılan değişkenler
 static unsigned long lastLogTime = 0;
 static const unsigned long LOG_INTERVAL = 300000; // 5 dakikada bir veri kaydı (300 saniye)
 
 // Geçici veri saklama için array'ler
 static DataRecord tempHumRecords[MAX_DATA_RECORDS];
 static int recordCount = 0;
 
 // Global olarak dışa aktarılan değişkenler
 extern PowerOutage powerOutages[MAX_POWER_OUTAGES];
 extern int powerOutageCount;
 
 // LogTempHumidity için tampon
 #define LOG_BUFFER_SIZE 5  // Tampon boyutu
 static DataRecord logBuffer[LOG_BUFFER_SIZE];
 static int bufferCount = 0;
 
 bool DataInit() {
    // LittleFS'i burada başlat
    if (!LittleFS.begin(true)) {
        LOG_ERROR(TAG, "LittleFS başlatılamadı!");
        
        // Formatlamayı dene
        if (!LittleFS.format()) {
            LOG_ERROR(TAG, "LittleFS formatlanamadı!");
            return false;
        }
        
        if (!LittleFS.begin(false)) {
            LOG_ERROR(TAG, "LittleFS format sonrası başlatılamadı!");
            return false;
        }
    }
    
    LOG_INFO(TAG, "LittleFS başlatıldı");
    
    // Dosya sistemi kontrolü ve gerekirse formatlama
    if (!CheckAndFormatFileSystem()) {
        LOG_ERROR(TAG, "Dosya sistemi kontrolü başarısız!");
        return false;
    }
    
    // Ayarları yükle
    settings = LoadSettings();
    
    // Motor ayarlarını yükle
    motorSettings = LoadMotorSettings();
    
    // Elektrik kesintisi kayıtlarını yükle
    LoadPowerOutages();
    
    // Sıcaklık/nem kayıtlarını yükle
    LoadTempHumRecords();
    
    LOG_INFO(TAG, "Veri modülü başlatıldı");
    return true;
}
 
 bool CheckAndFormatFileSystem() {
    // Dosya sistemi sağlıklı mı kontrol et
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    
    if (totalBytes == 0) {
        LOG_ERROR(TAG, "Dosya sistemi bilgisi alınamadı, formatlama deneniyor...");
        LittleFS.format();
        
        totalBytes = LittleFS.totalBytes();
        if (totalBytes == 0) {
            LOG_ERROR(TAG, "Dosya sistemi formatlanmasına rağmen bilgi alınamadı!");
            return false;
        }
    }
    
    LOG_INFO(TAG, "Dosya sistemi: Toplam=%lu KB, Kullanılan=%lu KB, Boş=%lu KB", 
             totalBytes / 1024, usedBytes / 1024, 
             (totalBytes - usedBytes) / 1024);
    
    return true;
}
 
 void DataUpdate() {
     unsigned long currentTime = millis();
     
     // Periyodik veri kaydı
     if (currentTime - lastLogTime > LOG_INTERVAL) {
         lastLogTime = currentTime;
         
         // Sensör verilerini oku
         SensorData sensorData = GetSensorData();
         RelayState relayState = ControlGetRelayState();
         
         // Verileri kaydet
         if (sensorData.sensor1Valid || sensorData.sensor2Valid) {
             LogTempHumidity(sensorData.avgTemperature, sensorData.avgHumidity, 
                           relayState.heater, relayState.humidifier);
         }
     }
 }
 
 bool SaveSettings(IncubationSettings& settingsToSave) {
    // Ayarları JSON formatında kaydet
    File file = LittleFS.open(FS_SETTINGS_FILE, "w");
    if (!file) {
        LOG_ERROR(TAG, "Ayarlar dosyası açılamadı!");
        return false;
    }
    
    StaticJsonDocument<512> doc;
    
    doc["type"] = settingsToSave.type;
    doc["target_temp"] = settingsToSave.targetTemp;
    doc["target_humidity"] = settingsToSave.targetHumidity;
    doc["total_days"] = settingsToSave.totalDays;
    doc["start_time"] = settingsToSave.startTime;
    doc["motor_enabled"] = settingsToSave.motorEnabled;
    doc["pid_kp"] = settingsToSave.pidKp;
    doc["pid_ki"] = settingsToSave.pidKi;
    doc["pid_kd"] = settingsToSave.pidKd;
    doc["hum_hysteresis"] = settingsToSave.humHysteresis;
    
    size_t bytesWritten = serializeJson(doc, file);
    file.close();
    
    if (bytesWritten == 0) {
        LOG_ERROR(TAG, "Ayarlar yazılamadı!");
        return false;
    }
    
    // İç değişkeni güncelle
    settings = settingsToSave;
    
    // Global önbelleği de güncelle
    globalCachedSettings = settingsToSave;
    globalLastSettingsUpdate = millis();
    
    LOG_INFO(TAG, "Ayarlar kaydedildi (%d byte)", bytesWritten);
    return true;
}
 
 IncubationSettings LoadSettings() {
    IncubationSettings loadedSettings;
    
    // Ayarları önbellekten yükle (5 saniyeden yeni ise)
    unsigned long currentTime = millis();
    if (currentTime - globalLastSettingsUpdate < 5000) {
        LOG_DEBUG(TAG, "Ayarlar önbellekten yüklendi");
        return globalCachedSettings;
    }
    
    // Varsayılan değerleri ayarla
    loadedSettings.type = 0; // Tavuk
    loadedSettings.targetTemp = DEFAULT_TEMP;
    loadedSettings.targetHumidity = DEFAULT_HUM;
    loadedSettings.totalDays = 21;
    loadedSettings.startTime = 0;
    loadedSettings.motorEnabled = true;
    loadedSettings.pidKp = PID_KP;
    loadedSettings.pidKi = PID_KI;
    loadedSettings.pidKd = PID_KD;
    loadedSettings.humHysteresis = HUM_HYSTERESIS;
    
    // Dosya yoksa oluştur ve varsayılan değerleri kaydet
    if (!LittleFS.exists(FS_SETTINGS_FILE)) {
        LOG_INFO(TAG, "Ayarlar dosyası bulunamadı, varsayılanlar kaydediliyor");
        SaveSettings(loadedSettings);
        return loadedSettings;
    }
     
    // Dosyayı aç
    File file = LittleFS.open(FS_SETTINGS_FILE, "r");
    if (!file) {
        LOG_ERROR(TAG, "Ayarlar dosyası açılamadı!");
        return loadedSettings;
    }
    
    // Dosya boyutunu kontrol et
    size_t fileSize = file.size();
    if (fileSize == 0 || fileSize > 4096) {
        LOG_ERROR(TAG, "Ayarlar dosyası geçersiz boyutta! Size: %d", fileSize);
        file.close();
        return loadedSettings;
    }
    
    // JSON dosyasını ayrıştır
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        LOG_ERROR(TAG, "Ayarlar JSON ayrıştırma hatası: %s", error.c_str());
        return loadedSettings;
    }
    
    // Değerleri oku
    loadedSettings.type = doc["type"] | 0;
    loadedSettings.targetTemp = doc["target_temp"] | DEFAULT_TEMP;
    loadedSettings.targetHumidity = doc["target_humidity"] | DEFAULT_HUM;
    loadedSettings.totalDays = doc["total_days"] | 21;
    loadedSettings.startTime = doc["start_time"] | 0;
    loadedSettings.motorEnabled = doc["motor_enabled"] | true;
    loadedSettings.pidKp = doc["pid_kp"] | PID_KP;
    loadedSettings.pidKi = doc["pid_ki"] | PID_KI;
    loadedSettings.pidKd = doc["pid_kd"] | PID_KD;
    loadedSettings.humHysteresis = doc["hum_hysteresis"] | HUM_HYSTERESIS;
    
    // Değerlerin mantıklı aralıklarda olup olmadığını kontrol et
    loadedSettings.targetTemp = constrain(loadedSettings.targetTemp, 30.0, 42.0);
    loadedSettings.targetHumidity = constrain(loadedSettings.targetHumidity, 30.0, 90.0);
    loadedSettings.totalDays = constrain(loadedSettings.totalDays, 1, 60);
    
    // Önbelleğe al
    globalCachedSettings = loadedSettings;
    globalLastSettingsUpdate = currentTime;
    
    LOG_INFO(TAG, "Ayarlar yüklendi");
    return loadedSettings;
}
 
 bool SaveMotorSettings(MotorSettings& settingsToSave) {
     // Motor ayarlarını JSON formatında kaydet
     File file = LittleFS.open(FS_MOTOR_FILE, "w");
     if (!file) {
         LOG_ERROR(TAG, "Motor ayarları dosyası açılamadı!");
         return false;
     }
     
     StaticJsonDocument<256> doc;
     
     doc["duration"] = settingsToSave.duration;
     doc["interval"] = settingsToSave.interval;
     
     if (serializeJson(doc, file) == 0) {
         LOG_ERROR(TAG, "Motor ayarları yazılamadı!");
         file.close();
         return false;
     }
     
     file.close();
     
     // İç değişkeni güncelle
     motorSettings = settingsToSave;
     
     LOG_INFO(TAG, "Motor ayarları kaydedildi");
     return true;
 }
 
 MotorSettings LoadMotorSettings() {
     MotorSettings loadedSettings;
     
     // Varsayılan değerleri ayarla
     loadedSettings.duration = DEFAULT_MOTOR_DURATION;  // 14 saniye
     loadedSettings.interval = DEFAULT_MOTOR_INTERVAL;  // 2 saat
     
     // Dosya yoksa varsayılan değerleri döndür
     if (!LittleFS.exists(FS_MOTOR_FILE)) {
         LOG_INFO(TAG, "Motor ayarları dosyası bulunamadı, varsayılanlar kullanılıyor");
         return loadedSettings;
     }
     
     // Dosyayı aç
     File file = LittleFS.open(FS_MOTOR_FILE, "r");
     if (!file) {
         LOG_ERROR(TAG, "Motor ayarları dosyası açılamadı!");
         return loadedSettings;
     }
     
     // JSON dosyasını ayrıştır
     StaticJsonDocument<256> doc;
     DeserializationError error = deserializeJson(doc, file);
     file.close();
     
     if (error) {
         LOG_ERROR(TAG, "Motor ayarları JSON ayrıştırma hatası: %s", error.c_str());
         return loadedSettings;
     }
     
     // Değerleri oku
     loadedSettings.duration = doc["duration"] | DEFAULT_MOTOR_DURATION;
     loadedSettings.interval = doc["interval"] | DEFAULT_MOTOR_INTERVAL;
     
     LOG_INFO(TAG, "Motor ayarları yüklendi");
     return loadedSettings;
 }
 
 bool SaveSensorCalibration(SensorCalibration calibration) {
     // Sensör kalibrasyonunu JSON formatında kaydet
     File file = LittleFS.open(FS_CALIBRATION_FILE, "w");
     if (!file) {
         LOG_ERROR(TAG, "Kalibrasyon dosyası açılamadı!");
         return false;
     }
     
     StaticJsonDocument<256> doc;
     
     doc["temp_offset1"] = calibration.tempOffset1;
     doc["hum_offset1"] = calibration.humOffset1;
     doc["temp_offset2"] = calibration.tempOffset2;
     doc["hum_offset2"] = calibration.humOffset2;
     
     if (serializeJson(doc, file) == 0) {
         LOG_ERROR(TAG, "Kalibrasyon verileri yazılamadı!");
         file.close();
         return false;
     }
     
     file.close();
     LOG_INFO(TAG, "Sensör kalibrasyonu kaydedildi");
     return true;
 }
 
 SensorCalibration LoadSensorCalibration() {
     SensorCalibration calibration;
     
     // Varsayılan değerleri ayarla
     calibration.tempOffset1 = 0.0;
     calibration.humOffset1 = 0.0;
     calibration.tempOffset2 = 0.0;
     calibration.humOffset2 = 0.0;
     
     // Dosya yoksa varsayılan değerleri döndür
     if (!LittleFS.exists(FS_CALIBRATION_FILE)) {
         LOG_INFO(TAG, "Kalibrasyon dosyası bulunamadı, varsayılanlar kullanılıyor");
         return calibration;
     }
     
     // Dosyayı aç
     File file = LittleFS.open(FS_CALIBRATION_FILE, "r");
     if (!file) {
         LOG_ERROR(TAG, "Kalibrasyon dosyası açılamadı!");
         return calibration;
     }
     
     // JSON dosyasını ayrıştır
     StaticJsonDocument<256> doc;
     DeserializationError error = deserializeJson(doc, file);
     file.close();
     
     if (error) {
         LOG_ERROR(TAG, "Kalibrasyon JSON ayrıştırma hatası: %s", error.c_str());
         return calibration;
     }
     
     // Değerleri oku
     calibration.tempOffset1 = doc["temp_offset1"] | 0.0;
     calibration.humOffset1 = doc["hum_offset1"] | 0.0;
     calibration.tempOffset2 = doc["temp_offset2"] | 0.0;
     calibration.humOffset2 = doc["hum_offset2"] | 0.0;
     
     LOG_INFO(TAG, "Sensör kalibrasyonu yüklendi");
     return calibration;
 }
 
 bool LogTempHumidity(float temp, float humidity, bool heaterOn, bool humidifierOn) {
     uint32_t unixTime = GetUnixTime();
     
     // Belleğe ekleme
     if (recordCount < MAX_DATA_RECORDS) {
         tempHumRecords[recordCount].timestamp = unixTime;
         tempHumRecords[recordCount].temperature = temp;
         tempHumRecords[recordCount].humidity = humidity;
         tempHumRecords[recordCount].heaterState = heaterOn;
         tempHumRecords[recordCount].humidifierState = humidifierOn;
         recordCount++;
     } else {
         // Dizi dolmuşsa, eski kayıtları kaydır
         memmove(&tempHumRecords[0], &tempHumRecords[1], sizeof(DataRecord) * (MAX_DATA_RECORDS - 1));
         
         // Yeni kaydı en sona ekle
         tempHumRecords[MAX_DATA_RECORDS-1].timestamp = unixTime;
         tempHumRecords[MAX_DATA_RECORDS-1].temperature = temp;
         tempHumRecords[MAX_DATA_RECORDS-1].humidity = humidity;
         tempHumRecords[MAX_DATA_RECORDS-1].heaterState = heaterOn;
         tempHumRecords[MAX_DATA_RECORDS-1].humidifierState = humidifierOn;
     }
     
     // Geçerli kaydı tampona ekle
     DataRecord newRecord = {unixTime, temp, humidity, heaterOn, humidifierOn};
     
     // 10 dakika içinde aynı değerler kaydedilmişse, tekrar kaydetme (depolama optimizasyonu)
     static float lastTemp = 0;
     static float lastHum = 0;
     static bool lastHeater = false;
     static bool lastHumidifier = false;
     static uint32_t lastLogTime = 0;
     
     bool significantChange = 
         abs(temp - lastTemp) >= 0.2 ||             // Sıcaklık değişimi 0.2°C'den fazla
         abs(humidity - lastHum) >= 1.0 ||         // Nem değişimi %1'den fazla
         heaterOn != lastHeater ||                 // Isıtıcı durumu değişmiş
         humidifierOn != lastHumidifier ||         // Nemlendirici durumu değişmiş
         (unixTime - lastLogTime) >= 600;          // 10 dakika geçmiş
     
     if (significantChange) {
         logBuffer[bufferCount] = newRecord;
         bufferCount++;
         
         // Son değerleri güncelle
         lastTemp = temp;
         lastHum = humidity;
         lastHeater = heaterOn;
         lastHumidifier = humidifierOn;
         lastLogTime = unixTime;
         
         // Tampon dolduğunda veya süre aşıldığında dosyaya yaz
         static unsigned long lastFlushTime = 0;
         if (bufferCount >= LOG_BUFFER_SIZE || (millis() - lastFlushTime > 60000)) { // 1 dk geçtiğinde yaz
             lastFlushTime = millis();
             
             // Dosyaya tampondan yaz
             File file = LittleFS.open(FS_TEMP_HUM_FILE, "a");
             if (!file) {
                 // Dosya açılamadıysa yeni oluştur
                 file = LittleFS.open(FS_TEMP_HUM_FILE, "w");
                 if (!file) {
                     LOG_ERROR(TAG, "Sıcaklık/nem dosyası açılamadı!");
                     bufferCount = 0; // Tamponu temizle
                     return false;
                 }
                 
                 // CSV başlık satırı
                 file.println("timestamp,temperature,humidity,heater,humidifier");
             }
             
             // Tampondaki tüm kayıtları yaz
            char buffer[64];
            for (int i = 0; i < bufferCount; i++) {
                sprintf(buffer, "%lu,%.1f,%.1f,%d,%d", 
                        logBuffer[i].timestamp, 
                        logBuffer[i].temperature, 
                        logBuffer[i].humidity, 
                        logBuffer[i].heaterState ? 1 : 0, 
                        logBuffer[i].humidifierState ? 1 : 0);
                file.println(buffer);
            }
            
            file.close();
            bufferCount = 0; // Tamponu temizle
        }
    }
    
    return true;
}

bool LogPowerOutage(uint32_t startTime, uint32_t endTime) {
    uint32_t duration = endTime - startTime;
    
    // Belleğe ekleme
    if (powerOutageCount < MAX_POWER_OUTAGES) {
        powerOutages[powerOutageCount].startTime = startTime;
        powerOutages[powerOutageCount].endTime = endTime;
        powerOutages[powerOutageCount].duration = duration;
        powerOutageCount++;
    } else {
        // Dizi dolmuşsa, eski kayıtları kaydır
        memmove(&powerOutages[0], &powerOutages[1], sizeof(PowerOutage) * (MAX_POWER_OUTAGES - 1));
        
        // Yeni kaydı en sona ekle
        powerOutages[MAX_POWER_OUTAGES-1].startTime = startTime;
        powerOutages[MAX_POWER_OUTAGES-1].endTime = endTime;
        powerOutages[MAX_POWER_OUTAGES-1].duration = duration;
    }
    
    // Dosyaya kaydetme - ikili formatta, daha kompakt
    File file = LittleFS.open(FS_POWER_OUTAGE_FILE, "a");
    if (!file) {
        // Dosya açılamadıysa yeni oluştur
        file = LittleFS.open(FS_POWER_OUTAGE_FILE, "w");
        if (!file) {
            LOG_ERROR(TAG, "Elektrik kesintisi dosyası açılamadı!");
            return false;
        }
        
        // Dosya başlığı (format bilgisi)
        file.write((uint8_t*)"POWEROUT", 8);
        int maxOutages = MAX_POWER_OUTAGES;
        file.write((uint8_t*)&maxOutages, sizeof(maxOutages));
    }
    
    // İkili formatta kayıt
    file.write((uint8_t*)&startTime, sizeof(startTime));
    file.write((uint8_t*)&endTime, sizeof(endTime));
    file.write((uint8_t*)&duration, sizeof(duration));
    
    file.close();
    LOG_INFO(TAG, "Elektrik kesintisi kaydedildi: %us (%u-%u)", duration, startTime, endTime);
    return true;
}

void SetPowerOnTime(uint32_t time) {
    File file = LittleFS.open(FS_POWER_TIME_FILE, "w");
    if (!file) {
        LOG_ERROR(TAG, "Güç zamanı dosyası açılamadı!");
        return;
    }
    
    file.println(time);
    file.close();
    LOG_INFO(TAG, "Güç açılış zamanı kaydedildi: %u", time);
}

uint32_t GetLastPowerOnTime() {
    if (!LittleFS.exists(FS_POWER_TIME_FILE)) {
        return 0;
    }
    
    File file = LittleFS.open(FS_POWER_TIME_FILE, "r");
    if (!file) {
        LOG_ERROR(TAG, "Güç zamanı dosyası açılamadı!");
        return 0;
    }
    
    String timeStr = file.readStringUntil('\n');
    file.close();
    
    return timeStr.toInt();
}

bool GetTempHumidityRecords(DataRecord* records, int maxCount, int* actualCount) {
    *actualCount = min(recordCount, maxCount);
    
    // Belleğe alınmış kayıtları kopyala
    for (int i = 0; i < *actualCount; i++) {
        records[i] = tempHumRecords[i];
    }
    
    return (*actualCount > 0);
}

bool GetPowerOutages(PowerOutage* outages, int maxCount, int* actualCount) {
    *actualCount = min(powerOutageCount, maxCount);
    
    // Belleğe alınmış kesinti kayıtlarını kopyala
    for (int i = 0; i < *actualCount; i++) {
        outages[i] = powerOutages[i];
    }
    
    return (*actualCount > 0);
}

bool GetTempHumidityStats(float* minTemp, float* maxTemp, float* avgTemp,
                           float* minHum, float* maxHum, float* avgHum) {
    if (recordCount == 0) {
        return false;
    }
    
    // İlk kayıttan değerler
    *minTemp = *maxTemp = tempHumRecords[0].temperature;
    *minHum = *maxHum = tempHumRecords[0].humidity;
    
    float sumTemp = tempHumRecords[0].temperature;
    float sumHum = tempHumRecords[0].humidity;
    
    // Diğer kayıtları işle
    for (int i = 1; i < recordCount; i++) {
        // Sıcaklık
        if (tempHumRecords[i].temperature < *minTemp) {
            *minTemp = tempHumRecords[i].temperature;
        }
        if (tempHumRecords[i].temperature > *maxTemp) {
            *maxTemp = tempHumRecords[i].temperature;
        }
        sumTemp += tempHumRecords[i].temperature;
        
        // Nem
        if (tempHumRecords[i].humidity < *minHum) {
            *minHum = tempHumRecords[i].humidity;
        }
        if (tempHumRecords[i].humidity > *maxHum) {
            *maxHum = tempHumRecords[i].humidity;
        }
        sumHum += tempHumRecords[i].humidity;
    }
    
    // Ortalamalar
    *avgTemp = sumTemp / recordCount;
    *avgHum = sumHum / recordCount;
    
    return true;
}

bool GetLatestTempHumidity(float* temp, float* humidity) {
    if (recordCount == 0) {
        return false;
    }
    
    // En son kaydı al
    *temp = tempHumRecords[recordCount - 1].temperature;
    *humidity = tempHumRecords[recordCount - 1].humidity;
    
    return true;
}

// Grafik verileri için ortak fonksiyon 
bool GetGraphData(float* data, uint32_t* times, int maxPoints, int* actualPoints, bool isTemperature) {
    if (recordCount == 0) {
        *actualPoints = 0;
        return false;
    }
    
    // Kaç nokta döndürüleceğini belirle
    *actualPoints = min(recordCount, maxPoints);
    
    // Örnekleme adımını belirle
    int step = 1;
    if (recordCount > maxPoints) {
        step = recordCount / maxPoints;
    }
    
    // Verileri doldur
    for (int i = 0; i < *actualPoints; i++) {
        int idx = i * step;
        if (idx >= recordCount) idx = recordCount - 1;
        
        // isTemperature true ise sıcaklık, false ise nem verisini al
        data[i] = isTemperature ? tempHumRecords[idx].temperature : tempHumRecords[idx].humidity;
        times[i] = tempHumRecords[idx].timestamp;
    }
    
    return true;
}

// Sıcaklık grafiği verileri
bool GetTempGraphData(float* data, uint32_t* times, int maxPoints, int* actualPoints) {
    return GetGraphData(data, times, maxPoints, actualPoints, true);
}

// Nem grafiği verileri
bool GetHumGraphData(float* data, uint32_t* times, int maxPoints, int* actualPoints) {
    return GetGraphData(data, times, maxPoints, actualPoints, false);
}

// Yardımcı fonksiyonlar
void LoadTempHumRecords() {
    recordCount = 0;
    
    if (!LittleFS.exists(FS_TEMP_HUM_FILE)) {
        LOG_INFO(TAG, "Sıcaklık/nem dosyası bulunamadı");
        return;
    }
    
    File file = LittleFS.open(FS_TEMP_HUM_FILE, "r");
    if (!file) {
        LOG_ERROR(TAG, "Sıcaklık/nem dosyası açılamadı!");
        return;
    }
    
    // İlk satırı atla (başlık)
    String header = file.readStringUntil('\n');
    
    // En son MAX_DATA_RECORDS satır oku
    String lines[MAX_DATA_RECORDS];
    int lineCount = 0;
    
    while (file.available() && lineCount < MAX_DATA_RECORDS) {
        lines[lineCount] = file.readStringUntil('\n');
        lineCount++;
    }
    
    file.close();
    
    // En son satırdan başlayarak işle (en yeni veriler)
    for (int i = lineCount - 1; i >= 0 && recordCount < MAX_DATA_RECORDS; i--) {
        // CSV satırını ayrıştır
        int pos1 = lines[i].indexOf(',');
        int pos2 = lines[i].indexOf(',', pos1 + 1);
        int pos3 = lines[i].indexOf(',', pos2 + 1);
        int pos4 = lines[i].indexOf(',', pos3 + 1);
        
        if (pos1 > 0 && pos2 > 0 && pos3 > 0 && pos4 > 0) {
            tempHumRecords[recordCount].timestamp = lines[i].substring(0, pos1).toInt();
            tempHumRecords[recordCount].temperature = lines[i].substring(pos1 + 1, pos2).toFloat();
            tempHumRecords[recordCount].humidity = lines[i].substring(pos2 + 1, pos3).toFloat();
            tempHumRecords[recordCount].heaterState = lines[i].substring(pos3 + 1, pos4).toInt() != 0;
            tempHumRecords[recordCount].humidifierState = lines[i].substring(pos4 + 1).toInt() != 0;
            recordCount++;
        }
    }
    
    // Zamansal sırayla sırala (eskiden yeniye)
    if (recordCount > 1) {
        for (int i = 0; i < recordCount - 1; i++) {
            for (int j = 0; j < recordCount - i - 1; j++) {
                if (tempHumRecords[j].timestamp > tempHumRecords[j + 1].timestamp) {
                    // Swap
                    DataRecord temp = tempHumRecords[j];
                    tempHumRecords[j] = tempHumRecords[j + 1];
                    tempHumRecords[j + 1] = temp;
                }
            }
        }
    }
    
    LOG_INFO(TAG, "Sıcaklık/nem kayıtları yüklendi: %d kayıt", recordCount);
}

void LoadPowerOutages() {
    powerOutageCount = 0;
    
    if (!LittleFS.exists(FS_POWER_OUTAGE_FILE)) {
        LOG_INFO(TAG, "Elektrik kesintisi dosyası bulunamadı");
        return;
    }
    
    File file = LittleFS.open(FS_POWER_OUTAGE_FILE, "r");
    if (!file) {
        LOG_ERROR(TAG, "Elektrik kesintisi dosyası açılamadı!");
        return;
    }
    
    // Dosya format kontrolü
    char header[8];
    file.read((uint8_t*)header, 8);
    
    if (strncmp(header, "POWEROUT", 8) != 0) {
        // Eski format veya geçersiz dosya
        file.close();
        LOG_ERROR(TAG, "Elektrik kesintisi dosyası geçersiz format!");
        return;
    }
    
    // Maksimum kayıt sayısını oku
    int maxRecords;
    file.read((uint8_t*)&maxRecords, sizeof(maxRecords));
    
    // Kayıtları oku
    while (file.available() >= (int)(sizeof(uint32_t) * 3) && powerOutageCount < MAX_POWER_OUTAGES) {
        uint32_t startTime, endTime, duration;
        
        file.read((uint8_t*)&startTime, sizeof(startTime));
        file.read((uint8_t*)&endTime, sizeof(endTime));
        file.read((uint8_t*)&duration, sizeof(duration));
        
        powerOutages[powerOutageCount].startTime = startTime;
        powerOutages[powerOutageCount].endTime = endTime;
        powerOutages[powerOutageCount].duration = duration;
        powerOutageCount++;
    }
    
    file.close();
    
    LOG_INFO(TAG, "Elektrik kesintisi kayıtları yüklendi: %d kayıt", powerOutageCount);
}

bool GetTimeRangeStats(uint32_t startTime, uint32_t endTime, 
                      float* avgTemp, float* avgHum, 
                      float* minTemp, float* maxTemp,
                      float* minHum, float* maxHum,
                      int* heaterOnPercentage) {
    // Veri yoksa false döndür
    if (recordCount == 0) {
        return false;
    }
    
    // İstatistik değişkenleri
    float sumTemp = 0;
    float sumHum = 0;
    *minTemp = 100.0;  // Makul bir üst sınır
    *maxTemp = 0.0;    // Makul bir alt sınır
    *minHum = 100.0;   // Makul bir üst sınır
    *maxHum = 0.0;     // Makul bir alt sınır
    int totalRecords = 0;
    int heaterOnCount = 0;
    
    // Tüm kayıtları kontrol et
    for (int i = 0; i < recordCount; i++) {
        // Belirtilen zaman aralığında mı?
        if (tempHumRecords[i].timestamp >= startTime && 
            tempHumRecords[i].timestamp <= endTime) {
            
            // Toplam değerleri güncelle
            sumTemp += tempHumRecords[i].temperature;
            sumHum += tempHumRecords[i].humidity;
            
            // Min/Max değerleri güncelle
            if (tempHumRecords[i].temperature < *minTemp) *minTemp = tempHumRecords[i].temperature;
            if (tempHumRecords[i].temperature > *maxTemp) *maxTemp = tempHumRecords[i].temperature;
            if (tempHumRecords[i].humidity < *minHum) *minHum = tempHumRecords[i].humidity;
            if (tempHumRecords[i].humidity > *maxHum) *maxHum = tempHumRecords[i].humidity;
            
            // Isıtıcı durumu
            if (tempHumRecords[i].heaterState) {
                heaterOnCount++;
            }
            
            totalRecords++;
        }
    }
    
    // Kayıt bulunamadıysa false döndür
    if (totalRecords == 0) {
        return false;
    }
    
    // Ortalama değerleri hesapla
    *avgTemp = sumTemp / totalRecords;
    *avgHum = sumHum / totalRecords;
    
    // Isıtıcı açık kalma yüzdesi
    *heaterOnPercentage = (heaterOnCount * 100) / totalRecords;
    
    return true;
}

bool ClearAllData() {
    bool success = true;
    
    // Veri dosyalarını sil
    if (LittleFS.exists(FS_TEMP_HUM_FILE)) {
        success &= LittleFS.remove(FS_TEMP_HUM_FILE);
    }
    
    if (LittleFS.exists(FS_POWER_OUTAGE_FILE)) {
        success &= LittleFS.remove(FS_POWER_OUTAGE_FILE);
    }
    
    // Bellek dizilerini temizle
    recordCount = 0;
    powerOutageCount = 0;
    
    // Tampon temizle
    bufferCount = 0;
    
    LOG_INFO(TAG, "Tüm veriler temizlendi");
    return success;
}

bool BackupData() {
    // Tüm ayarları ve verileri tek bir JSON dosyasına yedekle
    StaticJsonDocument<4096> doc;
    
    // Ayarlar
    JsonObject settingsObj = doc["settings"].to<JsonObject>();
    settingsObj["type"] = settings.type;
    settingsObj["target_temp"] = settings.targetTemp;
    settingsObj["target_humidity"] = settings.targetHumidity;
    settingsObj["total_days"] = settings.totalDays;
    settingsObj["start_time"] = settings.startTime;
    settingsObj["motor_enabled"] = settings.motorEnabled;
    settingsObj["pid_kp"] = settings.pidKp;
    settingsObj["pid_ki"] = settings.pidKi;
    settingsObj["pid_kd"] = settings.pidKd;
    settingsObj["hum_hysteresis"] = settings.humHysteresis;
    
    // Motor ayarları
    JsonObject motorObj = doc.createNestedObject("motor_settings");
    motorObj["duration"] = motorSettings.duration;
    motorObj["interval"] = motorSettings.interval;
    
    // Sensör kalibrasyonu
    SensorCalibration calibration = LoadSensorCalibration();
    JsonObject calObj = doc.createNestedObject("calibration");
    calObj["temp_offset1"] = calibration.tempOffset1;
    calObj["hum_offset1"] = calibration.humOffset1;
    calObj["temp_offset2"] = calibration.tempOffset2;
    calObj["hum_offset2"] = calibration.humOffset2;
    
    // Son güç kesintisi kaydı
    JsonObject powerObj = doc.createNestedObject("power");
    powerObj["last_on_time"] = GetLastPowerOnTime();
    
    // Sıcaklık/nem kayıtları (son 10 kayıt)
    JsonArray tempHumArray = doc.createNestedArray("temp_hum_records");
    int lastRecords = min(10, recordCount);
    for (int i = recordCount - lastRecords; i < recordCount; i++) {
        JsonObject record = tempHumArray.createNestedObject();
        record["timestamp"] = tempHumRecords[i].timestamp;
        record["temperature"] = tempHumRecords[i].temperature;
        record["humidity"] = tempHumRecords[i].humidity;
        record["heater"] = tempHumRecords[i].heaterState;
        record["humidifier"] = tempHumRecords[i].humidifierState;
    }
    
    // Elektrik kesintisi kayıtları
    JsonArray outageArray = doc.createNestedArray("power_outages");
    for (int i = 0; i < powerOutageCount; i++) {
        JsonObject outage = outageArray.createNestedObject();
        outage["start_time"] = powerOutages[i].startTime;
        outage["end_time"] = powerOutages[i].endTime;
        outage["duration"] = powerOutages[i].duration;
    }
    
    // Yedek dosyaya yaz
    File file = LittleFS.open(FS_BACKUP_FILE, "w");
    if (!file) {
        LOG_ERROR(TAG, "Yedek dosyası açılamadı!");
        return false;
    }
    
    size_t bytesWritten = serializeJson(doc, file);
    file.close();
    
    if (bytesWritten == 0) {
        LOG_ERROR(TAG, "Yedek yazılamadı!");
        return false;
    }
    
    LOG_INFO(TAG, "Tüm veriler yedeklendi (%d byte)", bytesWritten);
    return true;
}

bool RestoreData() {
    if (!LittleFS.exists(FS_BACKUP_FILE)) {
        LOG_ERROR(TAG, "Yedek dosyası bulunamadı!");
        return false;
    }
    
    File file = LittleFS.open(FS_BACKUP_FILE, "r");
    if (!file) {
        LOG_ERROR(TAG, "Yedek dosyası açılamadı!");
        return false;
    }
    
    // Yedek verisini oku
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        LOG_ERROR(TAG, "Yedek JSON ayrıştırma hatası: %s", error.c_str());
        return false;
    }
    
    // Ayarları geri yükle
    if (!doc["settings"].isNull() && doc["settings"].is<JsonObject>()) {
        JsonObject settingsObj = doc["settings"].as<JsonObject>();
        
        settings.type = settingsObj["type"] | 0;
        settings.targetTemp = settingsObj["target_temp"] | DEFAULT_TEMP;
        settings.targetHumidity = settingsObj["target_humidity"] | DEFAULT_HUM;
        settings.totalDays = settingsObj["total_days"] | 21;
        settings.startTime = settingsObj["start_time"] | 0;
        settings.motorEnabled = settingsObj["motor_enabled"] | true;
        settings.pidKp = settingsObj["pid_kp"] | PID_KP;
        settings.pidKi = settingsObj["pid_ki"] | PID_KI;
        settings.pidKd = settingsObj["pid_kd"] | PID_KD;
        settings.humHysteresis = settingsObj["hum_hysteresis"] | HUM_HYSTERESIS;
        
        SaveSettings(settings);
    }
    
    // Motor ayarlarını geri yükle
    if (doc.containsKey("motor_settings")) {
        JsonObject motorObj = doc["motor_settings"].as<JsonObject>();
        
        motorSettings.duration = motorObj["duration"] | DEFAULT_MOTOR_DURATION;
        motorSettings.interval = motorObj["interval"] | DEFAULT_MOTOR_INTERVAL;
        
        SaveMotorSettings(motorSettings);
    }
    
    // Sensör kalibrasyonunu geri yükle
    if (doc.containsKey("calibration")) {
        JsonObject calObj = doc["calibration"].as<JsonObject>();
        
        SensorCalibration calibration;
        calibration.tempOffset1 = calObj["temp_offset1"] | 0.0;
        calibration.humOffset1 = calObj["hum_offset1"] | 0.0;
        calibration.tempOffset2 = calObj["temp_offset2"] | 0.0;
        calibration.humOffset2 = calObj["hum_offset2"] | 0.0;
        
        SaveSensorCalibration(calibration);
    }
    
    // Güç kesintisi verisini geri yükle
    if (doc.containsKey("power")) {
        JsonObject powerObj = doc["power"].as<JsonObject>();
        
        uint32_t lastOnTime = powerObj["last_on_time"] | 0;
        SetPowerOnTime(lastOnTime);
    }
    
    LOG_INFO(TAG, "Yedek geri yüklendi");
    return true;
}