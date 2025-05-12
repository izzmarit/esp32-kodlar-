/**
 * Kuluçka Makinesi Kontrol Sistemi - Sensör Modülü
 */

 #include "SensorModule.h"
 #include <Wire.h>
 #include <Adafruit_SHT31.h>
 #include "../config/pins.h"
 
 static const char* TAG = "SENSOR";
 
 // Hareketli ortalama için son değerler
 #define FILTER_SAMPLES SENSOR_FILTER_SAMPLES // AppConfig.h'dan
 static float tempSamples1[FILTER_SAMPLES] = {0};
 static float tempSamples2[FILTER_SAMPLES] = {0};
 static float humSamples1[FILTER_SAMPLES] = {0};
 static float humSamples2[FILTER_SAMPLES] = {0};
 static int sampleIndex = 0;
 
 // Hata sayaçları ve kurtarma istatistikleri
 static int sensor1ErrorCount = 0;
 static int sensor2ErrorCount = 0;
 static int sensor1RecoveryCount = 0;
 static int sensor2RecoveryCount = 0;
 static unsigned long lastSensorReset = 0;
 
 // Sensör nesneleri
 static Adafruit_SHT31 sht31_1 = Adafruit_SHT31();
 static Adafruit_SHT31 sht31_2 = Adafruit_SHT31();
 
 // Sensör verileri
 static SensorData currentSensorData = {0};
 static SensorCalibration sensorCalibration = {0.0, 0.0, 0.0, 0.0};
 
 // Sensör durumu
 static bool sensorsInitialized = false;
 static unsigned long lastReadTime = 0;
 static const unsigned long READ_INTERVAL = SENSOR_READ_INTERVAL; // AppConfig.h'dan
 
 // Hareketli ortalama filtresi uygulama fonksiyonu
 float applyMovingAverage(float newValue, float* samples) {
     // Yeni değeri örnek dizisine ekle
     samples[sampleIndex] = newValue;
     
     // Tüm örneklerin ortalamasını hesapla
     float sum = 0;
     int validSamples = 0;
     
     for (int i = 0; i < FILTER_SAMPLES; i++) {
         if (samples[i] != 0) {
             sum += samples[i];
             validSamples++;
         }
     }
     
     // Örnek indeksini güncelle
     sampleIndex = (sampleIndex + 1) % FILTER_SAMPLES;
     
     // Ortalamayı döndür
     return (validSamples > 0) ? (sum / validSamples) : newValue;
 }

 bool initSensor(Adafruit_SHT31& sensor, uint8_t address, bool& isValid, const char* sensorName) {
    // I2C hatalarına karşı güçlü başlatma işlevi
    const int MAX_ATTEMPTS = 3;
    
    for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        if (sensor.begin(address)) {
            isValid = true;
            // Sensör ısıtıcıyı kapat (daha güvenilir okumalar için)
            sensor.heater(false);
            LOG_INFO(TAG, "%s SHT31 sensörü başlatıldı (0x%02X) - %d. denemede", 
                   sensorName, address, attempt + 1);
            return true;
        }
        
        LOG_WARNING(TAG, "%s SHT31 sensörü başlatılamadı (0x%02X) - %d. deneme", 
                  sensorName, address, attempt + 1);
        
        // I2C veri yolu sıfırla
        Wire.end();
        delay(50);
        Wire.begin(SDA_PIN, SCL_PIN);
        delay(50);
    }
    
    LOG_ERROR(TAG, "%s SHT31 sensörü bulunamadı (0x%02X) - %d deneme başarısız", 
             sensorName, address, MAX_ATTEMPTS);
    isValid = false;
    return false;
}
 
bool SensorInit() {
    // I2C veri yolunu yeniden başlat
    Wire.end();
    delay(50);
    Wire.begin(SDA_PIN, SCL_PIN);
    delay(100); // Yolun stabil olması için bekle
    
    // Sensörleri başlat
    bool sensor1Success = initSensor(sht31_1, SHT31_1_ADDR, currentSensorData.sensor1Valid, "İlk");
    bool sensor2Success = initSensor(sht31_2, SHT31_2_ADDR, currentSensorData.sensor2Valid, "İkinci");
    
    // En az bir sensör çalışıyorsa başlat
    sensorsInitialized = (currentSensorData.sensor1Valid || currentSensorData.sensor2Valid);
    
    // Hata sayaçlarını sıfırla
    sensor1ErrorCount = 0;
    sensor2ErrorCount = 0;
    sensor1RecoveryCount = 0;
    sensor2RecoveryCount = 0;
    lastSensorReset = millis();
    
    // Hareketli ortalama indeksini sıfırla
    sampleIndex = 0;
    
    if (sensorsInitialized) {
        // İlk okumayı yap
        SensorUpdate();
        LOG_INFO(TAG, "Sensör modülü başlatıldı (Aktif sensör sayısı: %d)", 
                currentSensorData.sensor1Valid + currentSensorData.sensor2Valid);
        return true;
    } else {
        LOG_ERROR(TAG, "Sensör modülü başlatılamadı - sensörler bulunamadı!");
        return false;
    }
}
 
 bool SensorUpdate() {
     unsigned long currentTime = millis();
     
     // Okumalar arasında minimum süre kontrolü
     if (currentTime - lastReadTime < READ_INTERVAL) {
         return true; // Henüz okuma zamanı gelmedi
     }
     
     lastReadTime = currentTime;
     currentSensorData.timestamp = currentTime;
     
     // Periyodik sensör resetleme (her 6 saatte bir)
     if (currentTime - lastSensorReset > 6 * 3600000) {
         lastSensorReset = currentTime;
         
         if (!currentSensorData.sensor1Valid && sensor1ErrorCount >= 3) {
             // Sensör 1'i yeniden başlatmayı dene
             sht31_1.begin(SHT31_1_ADDR);
             sht31_1.heater(false);
             sensor1RecoveryCount++;
             LOG_WARNING(TAG, "SHT31 #1 yeniden başlatma denemesi %d", sensor1RecoveryCount);
         }
         
         if (!currentSensorData.sensor2Valid && sensor2ErrorCount >= 3) {
             // Sensör 2'yi yeniden başlatmayı dene
             sht31_2.begin(SHT31_2_ADDR);
             sht31_2.heater(false);
             sensor2RecoveryCount++;
             LOG_WARNING(TAG, "SHT31 #2 yeniden başlatma denemesi %d", sensor2RecoveryCount);
         }
     }
     
     // İlk sensörden oku
     if (currentSensorData.sensor1Valid || sensor1ErrorCount < 3) {
         float rawTemp1 = sht31_1.readTemperature();
         float rawHum1 = sht31_1.readHumidity();
         
         if (!isnan(rawTemp1) && !isnan(rawHum1) && 
             rawTemp1 > -40.0 && rawTemp1 < 125.0 && // Sensör çalışma aralıkları
             rawHum1 >= 0.0 && rawHum1 <= 100.0) {
             
             // Offsetleri uygula
             float offsetTemp1 = rawTemp1 + sensorCalibration.tempOffset1;
             float offsetHum1 = rawHum1 + sensorCalibration.humOffset1;
             
             // Makul aralık kontrolü
             if (abs(offsetTemp1) > 100.0 || abs(offsetHum1) > 150.0) {
                 // Okunan değerler fiziksel olarak imkansız, sensör hatası
                 sensor1ErrorCount++;
                 if (sensor1ErrorCount >= 3) {
                     currentSensorData.sensor1Valid = false;
                     LOG_ERROR(TAG, "SHT31 #1 fiziksel olarak imkansız değerler okudu: T=%.1f H=%.1f", 
                             offsetTemp1, offsetHum1);
                 }
             } else {
                 // Hareketli ortalama filtresi uygula
                 currentSensorData.temperature1 = applyMovingAverage(offsetTemp1, tempSamples1);
                 currentSensorData.humidity1 = applyMovingAverage(offsetHum1, humSamples1);
                 
                 // Değerleri mantıklı aralıklara sınırla
                 currentSensorData.humidity1 = constrain(currentSensorData.humidity1, 0.0, 100.0);
                 
                 // Başarılı okumada hata sayacını sıfırla
                 sensor1ErrorCount = 0;
                 currentSensorData.sensor1Valid = true;
                 
                 // Sensör geçersizse ve başarıyla okuduysa, geçerli yap
                 if (!currentSensorData.sensor1Valid) {
                     LOG_INFO(TAG, "SHT31 #1 yeniden aktif");
                     currentSensorData.sensor1Valid = true;
                 }
                 
                 LOG_VERBOSE(TAG, "SHT31 #1 - T:%.1f°C H:%.1f%%", 
                            currentSensorData.temperature1, currentSensorData.humidity1);
             }
         } else {
             // Sensör okunamadı
             sensor1ErrorCount++;
                 
             if (sensor1ErrorCount >= 3) {
                 currentSensorData.sensor1Valid = false;
                 LOG_ERROR(TAG, "SHT31 #1 okuma hatası! (3 kez üst üste başarısız)");
             } else {
                 LOG_WARNING(TAG, "SHT31 #1 okuma hatası! Son geçerli değer korunuyor.");
             }
         }
     }
     
     // İkinci sensörden oku
     if (currentSensorData.sensor2Valid || sensor2ErrorCount < 3) {
         float rawTemp2 = sht31_2.readTemperature();
         float rawHum2 = sht31_2.readHumidity();
         
         if (!isnan(rawTemp2) && !isnan(rawHum2) && 
             rawTemp2 > -40.0 && rawTemp2 < 125.0 && // Sensör çalışma aralıkları
             rawHum2 >= 0.0 && rawHum2 <= 100.0) {
             
             // Offsetleri uygula
             float offsetTemp2 = rawTemp2 + sensorCalibration.tempOffset2;
             float offsetHum2 = rawHum2 + sensorCalibration.humOffset2;
             
             // Makul aralık kontrolü
             if (abs(offsetTemp2) > 100.0 || abs(offsetHum2) > 150.0) {
                 // Okunan değerler fiziksel olarak imkansız, sensör hatası
                 sensor2ErrorCount++;
                 if (sensor2ErrorCount >= 3) {
                     currentSensorData.sensor2Valid = false;
                     LOG_ERROR(TAG, "SHT31 #2 fiziksel olarak imkansız değerler okudu: T=%.1f H=%.1f", 
                             offsetTemp2, offsetHum2);
                 }
             } else {
                 // Hareketli ortalama filtresi uygula
                 currentSensorData.temperature2 = applyMovingAverage(offsetTemp2, tempSamples2);
                 currentSensorData.humidity2 = applyMovingAverage(offsetHum2, humSamples2);
                 
                 // Değerleri mantıklı aralıklara sınırla
                 currentSensorData.humidity2 = constrain(currentSensorData.humidity2, 0.0, 100.0);
                 
                 // Başarılı okumada hata sayacını sıfırla
                 sensor2ErrorCount = 0;
                 currentSensorData.sensor2Valid = true;
                 
                 // Sensör geçersizse ve başarıyla okuduysa, geçerli yap
                 if (!currentSensorData.sensor2Valid) {
                     LOG_INFO(TAG, "SHT31 #2 yeniden aktif");
                     currentSensorData.sensor2Valid = true;
                 }
                 
                 LOG_VERBOSE(TAG, "SHT31 #2 - T:%.1f°C H:%.1f%%", 
                            currentSensorData.temperature2, currentSensorData.humidity2);
             }
         } else {
             // Sensör okunamadı
             sensor2ErrorCount++;
             
             if (sensor2ErrorCount >= 3) {
                 currentSensorData.sensor2Valid = false;
                 LOG_ERROR(TAG, "SHT31 #2 okuma hatası! (3 kez üst üste başarısız)");
             } else {
                 LOG_WARNING(TAG, "SHT31 #2 okuma hatası! Son geçerli değer korunuyor.");
             }
         }
     }
     
     // Karşılaştırmalı sensör geçerliliği kontrolü
     if (currentSensorData.sensor1Valid && currentSensorData.sensor2Valid) {
         // İki sensör değeri arasında büyük fark varsa
         float tempDiff = abs(currentSensorData.temperature1 - currentSensorData.temperature2);
         float humDiff = abs(currentSensorData.humidity1 - currentSensorData.humidity2);
         
         if (tempDiff > 5.0 || humDiff > 15.0) {
             // Büyük fark varsa hangi sensörün daha güvenilir olduğunu belirle
             if (tempDiff > 5.0) {
                 // Sıcaklık farkı çok büyük, durumu log'a yaz
                 LOG_WARNING(TAG, "Sensörler arası sıcaklık farkı çok büyük: %.1f°C", tempDiff);
                 
                 // Isıtıcı durumuna göre kontrol
                 // Eğer ısıtıcı açıksa, daha yüksek okuyan sensör muhtemelen ısıtıcıya yakındır
                 // Bu durumda düşük sıcaklık okuyan sensör muhtemelen daha doğrudur
                 // Eğer ısıtıcı kapalıysa, her iki sensörün de benzer değerler vermesi beklenirdi
                 
                 // Burada ısıtıcı durumunu bilmediğimiz için sadece uyarı loglarız
                 if (currentSensorData.temperature1 > currentSensorData.temperature2 + 5.0) {
                     // Sensör 1 muhtemelen hatalı veya ısıtıcıya çok yakın
                     LOG_WARNING(TAG, "Sensör 1 çok yüksek sıcaklık okuyor: %.1f°C (Sensör 2: %.1f°C)",
                               currentSensorData.temperature1, currentSensorData.temperature2);
                 } else {
                     // Sensör 2 muhtemelen hatalı veya ısıtıcıya çok yakın
                     LOG_WARNING(TAG, "Sensör 2 çok yüksek sıcaklık okuyor: %.1f°C (Sensör 1: %.1f°C)",
                               currentSensorData.temperature2, currentSensorData.temperature1);
                 }
             }
             
             if (humDiff > 15.0) {
                 LOG_WARNING(TAG, "Sensörler arası nem farkı çok büyük: %.1f%%", humDiff);
             }
         }
     }
     
     // Ortalama değerleri hesapla
     if (currentSensorData.sensor1Valid && currentSensorData.sensor2Valid) {
         // İki sensör de çalışıyorsa, ağırlıklı ortalama kullan
         // Daha güvenilir sensöre daha yüksek ağırlık ver
         float weight1 = 0.5;
         float weight2 = 0.5;
         
         if (sensor1ErrorCount > 0 && sensor2ErrorCount == 0) {
             // Sensör 2 daha güvenilir
             weight1 = 0.3;
             weight2 = 0.7;
         } else if (sensor2ErrorCount > 0 && sensor1ErrorCount == 0) {
             // Sensör 1 daha güvenilir
             weight1 = 0.7;
             weight2 = 0.3;
         }
         
         currentSensorData.avgTemperature = 
             (currentSensorData.temperature1 * weight1 + currentSensorData.temperature2 * weight2);
         currentSensorData.avgHumidity = 
             (currentSensorData.humidity1 * weight1 + currentSensorData.humidity2 * weight2);
     } else if (currentSensorData.sensor1Valid) {
         // Sadece ilk sensör çalışıyorsa
         currentSensorData.avgTemperature = currentSensorData.temperature1;
         currentSensorData.avgHumidity = currentSensorData.humidity1;
     } else if (currentSensorData.sensor2Valid) {
         // Sadece ikinci sensör çalışıyorsa
         currentSensorData.avgTemperature = currentSensorData.temperature2;
         currentSensorData.avgHumidity = currentSensorData.humidity2;
     } else {
         // Hiçbir sensör çalışmıyorsa
         LOG_ERROR(TAG, "Hiçbir sensör çalışmıyor!");
         return false;
     }
     
     LOG_DEBUG(TAG, "Ortalama: T:%.1f°C H:%.1f%%", 
              currentSensorData.avgTemperature, currentSensorData.avgHumidity);
     
     return true;
 }
 
 SensorData GetSensorData() {
     return currentSensorData;
 }
 
 bool SetCalibration(SensorCalibration calibration) {
     sensorCalibration = calibration;
     LOG_INFO(TAG, "Sensör kalibrasyonu güncellendi");
     return true;
 }
 
 SensorCalibration GetCalibration() {
     return sensorCalibration;
 }
 
 bool PerformSemiAutoCalibration(float refTemp, float refHum) {
     // Yarı otomatik kalibrasyon işlemi
     // Referans değerler ile sensör değerlerini karşılaştırıp offset hesapla
     
     if (!SensorUpdate()) {
         LOG_ERROR(TAG, "Kalibrasyon başarısız - sensör okunamadı");
         return false; // Sensör okunamadı
     }
     
     // İlk sensör için kalibrasyon
     if (currentSensorData.sensor1Valid) {
         float rawTemp1 = sht31_1.readTemperature(); // Ofset uygulanmamış okuma
         float rawHum1 = sht31_1.readHumidity();     // Ofset uygulanmamış okuma
         
         sensorCalibration.tempOffset1 = refTemp - rawTemp1;
         sensorCalibration.humOffset1 = refHum - rawHum1;
         
         LOG_INFO(TAG, "Sensör 1 kalibrasyon: T-offset:%.2f H-offset:%.2f", 
                 sensorCalibration.tempOffset1, sensorCalibration.humOffset1);
     }
     
     // İkinci sensör için kalibrasyon
     if (currentSensorData.sensor2Valid) {
         float rawTemp2 = sht31_2.readTemperature(); // Ofset uygulanmamış okuma
         float rawHum2 = sht31_2.readHumidity();     // Ofset uygulanmamış okuma
         
         sensorCalibration.tempOffset2 = refTemp - rawTemp2;
         sensorCalibration.humOffset2 = refHum - rawHum2;
         
         LOG_INFO(TAG, "Sensör 2 kalibrasyon: T-offset:%.2f H-offset:%.2f", 
                 sensorCalibration.tempOffset2, sensorCalibration.humOffset2);
     }
     
     // Sensörleri yeni kalibrasyon ile güncelle
     SensorUpdate();
     
     return true;
 }
 
 bool IsSensorDataValid() {
     return (currentSensorData.sensor1Valid || currentSensorData.sensor2Valid);
 }
 
 float GetTemperature() {
     return currentSensorData.avgTemperature;
 }
 
 float GetHumidity() {
     return currentSensorData.avgHumidity;
 }
 
 // Sensörlerin doğru çalışıp çalışmadığını kontrol et
 bool CheckSensorsIntegrity() {
     // Sensör donanım testleri
     bool sensor1Ok = true;
     bool sensor2Ok = true;
     
     // Sensör 1 testi
     if (currentSensorData.sensor1Valid) {
         float testTemp1 = sht31_1.readTemperature();
         float testHum1 = sht31_1.readHumidity();
         
         // Sensörden alınan değerler mantıklı değilse hata var demektir
         if (isnan(testTemp1) || isnan(testHum1) || 
             testTemp1 < -10.0 || testTemp1 > 60.0 || 
             testHum1 < 0.0 || testHum1 > 100.0) {
             sensor1Ok = false;
             LOG_ERROR(TAG, "Sensör 1 bütünlük testi başarısız! T:%.1f H:%.1f", testTemp1, testHum1);
         }
     } else {
         // Sensör zaten geçersiz durumda
         sensor1Ok = false;
     }
     
     // Sensör 2 testi
     if (currentSensorData.sensor2Valid) {
         float testTemp2 = sht31_2.readTemperature();
         float testHum2 = sht31_2.readHumidity();
         
         // Sensörden alınan değerler mantıklı değilse hata var demektir
         if (isnan(testTemp2) || isnan(testHum2) || 
             testTemp2 < -10.0 || testTemp2 > 60.0 || 
             testHum2 < 0.0 || testHum2 > 100.0) {
             sensor2Ok = false;
             LOG_ERROR(TAG, "Sensör 2 bütünlük testi başarısız! T:%.1f H:%.1f", testTemp2, testHum2);
         }
     } else {
         // Sensör zaten geçersiz durumda
         sensor2Ok = false;
     }
     
     // Sensör durumlarını güncelle
     currentSensorData.sensor1Valid = sensor1Ok;
     currentSensorData.sensor2Valid = sensor2Ok;
     
     LOG_INFO(TAG, "Sensör bütünlük testi: Sensör1:%s Sensör2:%s", 
             sensor1Ok ? "Başarılı" : "Başarısız", 
             sensor2Ok ? "Başarılı" : "Başarısız");
     
     // En az bir sensör çalışıyorsa başarılı kabul et
     return (sensor1Ok || sensor2Ok);
 }
 
 // Sensör hata sayaçlarını döndür
 int GetSensor1ErrorCount() {
     return sensor1ErrorCount;
 }
 
 int GetSensor2ErrorCount() {
     return sensor2ErrorCount;
 }
 
 int GetSensor1RecoveryAttempts() {
     return sensor1RecoveryCount;
 }
 
 int GetSensor2RecoveryAttempts() {
     return sensor2RecoveryCount;
 }
 
 void ResetSensorErrorCounters() {
     sensor1ErrorCount = 0;
     sensor2ErrorCount = 0;
     sensor1RecoveryCount = 0;
     sensor2RecoveryCount = 0;
     lastSensorReset = millis();
     LOG_INFO(TAG, "Sensör hata sayaçları sıfırlandı");
 }